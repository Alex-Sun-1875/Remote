#include "remote/delegate/remote_main_runner_impl.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/thread_task_runner_handle.h"
// #include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

#include "remote/delegate/mojo/mojo_init.h"
#include "remote/delegate/remote_main_delegate.h"
#include "remote/common/content_constants_internal.h"

// TODO: Need transplant to remote
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

#include "services/service_manager/embedder/switches.h"

#if defined(OS_MACOSX)
#include "base/mac/mach_port_broker.h"
#endif

#if defined(OS_POSIX)
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"

// TODO: Need transplant to remote
#include "content/public/common/content_descriptors.h"
#endif

namespace content {

struct MainFunction {
  const char* name;
  int (*function)(const MainFunctionParams&);
};

void RunRemoteProcessMain(const MainFunctionParams& main_function_params,
                         RemoteMainDelegate* delegate_) {
  int exit_code = delegate_->RunProcess("", main_function_params);

  if (exit_code >= 0) {
    return;
  }

  LOG(INFO) << "###sunlh### func: " << __func__ << ", Main Start!!!";

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  return;
}

int RunOtherNameProcessTypeMain(const std::string& process_type,
                                const MainFunctionParams& main_func_params,
                                RemoteMainDelegate* delegate) {
  return delegate->RunProcess(process_type, main_func_params);
}

// static
RemoteMainRunnerImpl* RemoteMainRunnerImpl::Create() {
  return new RemoteMainRunnerImpl();
}

RemoteMainRunnerImpl::RemoteMainRunnerImpl() {}

RemoteMainRunnerImpl::~RemoteMainRunnerImpl() {
  if (is_initialized_ && !is_shutdown_)
    Shutdown();
}

int RemoteMainRunnerImpl::TerminateForFatalInitializationError() {
  return delegate_->TerminateForFatalInitializationError();
}

int RemoteMainRunnerImpl::Initialize(const RemoteMainParams& params) {
#if defined(OS_MACOSX)
  autorelease_pool_ = params.autorelease_pool;
#endif

  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
#if !defined(OS_ANDROID)
  g_fds->Set(service_manager::kMojoIPCChannel,
             service_manager::kMojoIPCChannel +
              base::GlobalDescriptors::kBaseDescriptor);
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  g_fds->Set(service_manager::kCrashDumpSignal,
             service_manager::kCrashDumpSignal +
              base::GlobalDescriptors::kBaseDescriptor);
#endif

  is_initialized_ = true;
  delegate_ = params.delegate;

  exit_manager_.reset(new base::AtExitManager);

  int exit_code = 0;
  if (delegate_->BasicStartupComplete(&exit_code))
    return exit_code;
  completed_basic_startup_ = true;

  const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  std::string process_type = command_line.GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_MACOSX)
  if (!process_type.empty() || delegate_->ProcessRegistersWithSystemProcess(process_type)) {
    base::MachPortBroker::ChildSendTaskPortToParent(kMachBootstrapName);
  }
#endif

  return -1;
}

int RemoteMainRunnerImpl::Run(bool start_service_manager_only) {
  DCHECK(is_initialized_);
  DCHECK(!is_shutdown_);

  const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  std::string process_type = command_line.GetSwitchValueASCII(switches::kProcessType);
  MainFunctionParams main_params(command_line);

#if defined(OS_MACOSX)
  main_params.autorelease_pool = autorelease_pool_;
#endif

  // TODO: Need to implement
  // RegisterMainThreadFactories();

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  if (process_type.empty()) {
    main_message_loop_.reset(base::MessageLoop(base::MessageLoop::TYPE_UI));
    base::PlatformThread::SetName("RemoteMain");
    base::TaskScheduler::Create("RemoteMain");
    base::TaskScheduler::GetInstance()->StartWithDefaultParams();

    main_thread_.reset(new base::Thread("RemoteMain"));
    main_thread_->Start();

    base::RunLoop run_loop;
    main_thread_->task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    return 0;
    // return RunRemoteProcessMain(main_params, delegate_);
  }
#endif

  return RunOtherNameProcessTypeMain(process_type, main_params, delegate_);
}

void RemoteMainRunnerImpl::Shutdown() {
  DCHECK(is_initialized_);
  DCHECK(!is_shutdown_);

  if (completed_basic_startup_) {
    const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    std::string process_type = command_line.GetSwitchValueASCII(switches::kProcessType);
    delegate_->ProcessExiting(process_type);
  }

  main_message_loop_.reset();

  exit_manager_.reset(nullptr);

  delegate_ = nullptr;
  is_shutdown_ = true;
}

// static

RemoteMainRunner* RemoteMainRunner::Create() {
  return RemoteMainRunnerImpl::Create();
}

} // namespace content
