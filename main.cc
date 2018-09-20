#include <stdint.h>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "build/build_config.h"

#include "remote/delegate/remote_main.h"
#include "remote/delegate/remote_main_delegate.h"
#include "remote/delegate/remote_service_manager_main_delegate.h"

// TODO: Need transplant to remote
#include "content/public/common/content_switches.h"

#include "services/service_manager/embedder/main.h"
#include "base/allocator/buildflags.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "services/service_manager/embedder/main_delegate.h"
#include "services/service_manager/embedder/process_type.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/init.h"

#if defined(OS_POSIX) && !defined(OS_ANDROID)
#include <locale.h>
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/service_manager/embedder/mac_init.h"

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/allocator_shim.h"
#endif
#endif // defined(OS_MACOSX)

using namespace content;
using namespace service_manager;

namespace service_manager {

namespace {

constexpr size_t kMaximumMojoMessageSize = 128 * 1024 * 1024;

class ServiceProcessLauncherDelegateImpl
    : public service_manager::ServiceProcessLauncherDelegate {
  public:
    explicit ServiceProcessLauncherDelegateImpl(MainDelegate* main_delegate)
        : main_delegate_(main_delegate) {}

    ~ServiceProcessLauncherDelegateImpl() override {}

  private:
    void AdjustCommandLineArgumentsForTarget(const service_manager::Identity& target,
                                             base::CommandLine* command_line) override {
      if (main_delegate_->ShouldLaunchAsServiceProcess(target)) {
        command_line->AppendSwitchASCII(switches::kProcessType,
                                        switches::kProcessTypeService);
      }

      main_delegate_->AdjustServiceProcessCommandLine(target, command_line);
    }

    MainDelegate* const main_delegate_;

    DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncherDelegateImpl);
};

#if defined(OS_POSIX) && !defined(OS_ANDROID)

void SetupSignalHandlers() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableInProcessStackTraces)) {
    return;
  }

  sigset_t empty_signal_set;
  CHECK_EQ(0, sigemptyset(&empty_signal_set));
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] = {
    SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE,
    SIGSEGV, SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP
  };

  for (unsigned i = 0; i < arraysize(signals_to_reset); ++i) {
    CHECK_EQ(0, sigaction(signals_to_reset[i], &sigact, NULL));
  }

  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));
}

void PopulateFDsFromCommandLine() {
  const std::string& shared_file_param =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedFiles);

  if (shared_file_param.empty())
    return;

  base::Optional<std::map<int, std::string>> shared_file_descriptors =
      service_manager::ParseSharedFileSwitchValue(shared_file_param);

  if (!shared_file_descriptors)
    return;

  for (const auto& descriptor : *shared_file_descriptors) {
    base::MemoryMappedFile::Region region;
    const std::string& key = descriptor.second;
    base::ScopedFD fd = base::GlobalDescriptors::GetInstance()->TakeFD(descriptor.first, &region);
    base::FileDescriptorStore::GetInstance().Set(key, std::move(fd), region);
  }
}

#endif // defined(OS_POSIX) && !defined(OS_ANDROID)

void CommonSubprocessInit() {
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  setlocale(LC_NUMERIC, "C");
#endif
}

void NonEmbedderProcessInit() {
  service_manager::InitializeLogging();

  base::TaskScheduler::CreateAndStartWithDefaultParams("ServiceManagerProcess");
}

int RunServiceManager(MainDelegate* delegate) {
  NonEmbedderProcessInit();

  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);

  base::Thread ipc_thread("IPC Thread");
  ipc_thread.StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(ipc_thread.task_runner(),
                                           mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
  ServiceProcessLauncherDelegateImpl service_process_launcher_delegate(delegate);
  service_manager::BackgroundServiceManager background_service_manager(&service_process_launcher_delegate,
                                                                       delegate->CreateServiceCatalog());

  base::RunLoop run_loop;
  delegate->OnServiceManagerInitialized(run_loop.QuitClosure(),
                                        &background_service_manager);

  run_loop.Run();

  ipc_thread.Stop();
  base::TaskScheduler::GetInstance()->Shutdown();

  return 0;
}

int RunService(MainDelegate* delegate) {
  NonEmbedderProcessInit();

  int exit_code = 0;

  RunStandaloneService(base::Bind([](MainDelegate* delegate, int* exit_code, mojom::ServiceRequest request){
                                    base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
                                    base::RunLoop run_loop;

                                    std::string service_name =
                                        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kServiceName);

                                    if (service_name.empty()) {
                                      LOG(ERROR) << "Service process requires --service-name";
                                      *exit_code = 1;
                                      return;
                                    }

                                    std::unique_ptr<Service> service = delegate->CreateEmbeddedService(service_name);

                                    if (!service) {
                                      LOG(ERROR) << "Failed to start embedded service: " << service_name;
                                      *exit_code = 1;
                                      return;
                                    }

                                    ServiceContext context(std::move(service), std::move(request));
                                    context.SetQuitClosure(run_loop.QuitClosure());
                                    run_loop.Run();
                                  }, delegate, &exit_code));

  return exit_code;
}

} // namespace

} // namespace service_manager

int main(int argc, const char** argv) {
  RemoteMainDelegate remote_main_delegate(base::TimeTicks::FromInternalValue(0));
  RemoteMainParams params(&remote_main_delegate);
  RemoteServiceManagerMainDelegate delegate(params);
  MainParams main_params(&delegate);
#if !defined(OS_WIN) && !defined(OS_ANDROID)
  main_params.argc = argc;
  main_params.argv = argv;
#endif

  MainDelegate* main_delegate = main_params.delegate;
  DCHECK(main_delegate);

  int exit_code = -1;

  ProcessType process_type = main_delegate->OverrideProcessType();

#if defined(OS_MACOSX)
  std::unique_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool;
#endif

  static bool is_initialized = false;

#if !defined(OS_ANDROID)
  DCHECK(!is_initialized);
#endif

#if defined(OS_MACOSX)
  base::allocator::InitializeAllocatorShim();
#endif

  base::EnableTerminationOnOutOfMemory();

#if defined(OS_LINUX)
  const int kNoOverrideIfAlreadySet = 0;
  setenv("DBUS_SESSION_BUS_ADDRESS", "disable:", kNoOverrideIfAlreadySet);
#endif

#if !defined(OS_ANDROID)
  base::CommandLine::Init(argc, argv);

#if defined(OS_POSIX)
  PopulateFDsFromCommandLine();
#endif

  base::EnableTerminationOnHeapCorruption();
  SetProcessTitleFromCommandLine(argv);
#endif // !defined(OS_ANDROID)

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  setlocale(LC_ALL, "");
  SetupSignalHandlers();
#endif

  const auto& command_line = *base::CommandLine::ForCurrentProcess();

  MainDelegate::InitializeParams init_params;

#if defined(OS_MACOSX)
  autorelease_pool = std::make_unique<base::mac::ScopedNSAutoreleasePool>();
  init_params.autorelease_pool = autorelease_pool.get();
  // InitializeMac();
#endif

  mojo::core::Configuration mojo_config;
  if (process_type == ProcessType::kDefault &&
      command_line.GetSwitchValueASCII(service_manager::switches::kProcessType) ==
          service_manager::switches::kProcessTypeServiceManager) {
    mojo_config.is_broker_process = true;
  }
  mojo_config.max_message_num_bytes = kMaximumMojoMessageSize;
  main_delegate->OverrideMojoConfiguration(&mojo_config);
  mojo::core::Init(mojo_config);

  exit_code = main_delegate->Initialize(init_params);
  if (exit_code >= 0) {
    LOG(INFO) << "###sunlh### func: " << __func__ << " exit_code = " << exit_code;
    return exit_code;
  }

  const auto& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (process_type == ProcessType::kDefault) {
    std::string type_switch = cmd_line.GetSwitchValueASCII(service_manager::switches::kProcessType);
    if (service_manager::switches::kProcessTypeServiceManager == type_switch) {
      process_type = ProcessType::kServiceManager;
    } else if (service_manager::switches::kProcessTypeService == type_switch) {
      process_type = ProcessType::kService;
    } else {
      process_type = ProcessType::kEmbedder;
    }
  }

  switch(process_type) {
    case ProcessType::kDefault:
      NOTREACHED();
      break;

    case ProcessType::kServiceManager:
      exit_code = RunServiceManager(main_delegate);
      break;

    case ProcessType::kService:
      CommonSubprocessInit();
      exit_code = RunService(main_delegate);
      break;

    case ProcessType::kEmbedder:
      if (main_delegate->IsEmbedderSubprocess())
        CommonSubprocessInit();
      exit_code = main_delegate->RunEmbedderProcess();
      break;
  }

#if defined(OS_MACOSX)
  autorelease_pool.reset();
#endif

  if (process_type == ProcessType::kEmbedder)
    main_delegate->ShutDownEmbedderProcess();

  LOG(INFO) << "###sunlh### func: " << __func__ << ", exit_code = " << exit_code;
  return exit_code;
}
