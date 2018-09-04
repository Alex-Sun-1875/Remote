#include "child_thread_impl.h"

#include <signal.h>
#include <string>
#include <utility>

#include "base/base_switchs.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/profiler.h"
#include "base/lazy_instance.h"
#include "base/locations.h"
#include "base/logging.h"
#include "base/macros.h"
// #include "base/memory/memory_pressure_listener.h"
#include "base/message_loop/timer_slack.h"
// #include "base/metrics/field_trial.h"
// #include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
// #include "base/trace_event/memory_dump_manager.h"
// #include "base/trace_event/trace_event.h"
#include "build/build_config.h"
// #include "components/tracing/child/child_trace_message_filter.h"
// #include "content/child/child_histogram_fetcher_impl.h"

#include "child_process.h"
#include "thread_safe_sender.h"

// #include "content/common/field_trial_recorder.mojom.h"
#include "content/common/in_process_child_thread_params.h"
// #include "content/public/common/connection_filter.h"
// #include "content/public/common/content_client.h"
// #include "content/public/common/content_features.h"
// #include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/simple_connection_filter.h"

#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"

#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

// #include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"
// #include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
// #include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
// #include "services/service_manager/sandbox/sandbox_type.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#endif
#endif

namespace content {
namespace {
const int kConnectionTimeoutS = 15;

base::LazyInstance<base::ThreadLocalPointer<ChildThread>>::DestructorAtExit
    g_lazy_tls = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_POSIX)

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDERFINED_SANITIZER)

class WaitAndExitDelegate : public base::PlatformThread::Delegate {
  public:
    explicit WaitAndExitDelegate(base::TimeDelta duration)
      : duration_(duration) {}

    void ThreadMain() override {
      base::PlatformThread::Sleep(duration_);
      base::Process::TerminateCurrentProcessImmediately(0);
    }

  private:
    const base::TimeDelta duration_;
    // const成员变量只可以初始化列表中初始化,const用于类中成员变量时，将类成员变
    // 为只读属性（只读：不能出现在“=”的左边，但在类中仍可以用一个指针来修改其
    // 值。） 所以不可以直接在类的构造函数中初始化const 的成员

    DISALLOW_COPY_AND_ASSIGN(WaitAndExitDelegate);
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  std::unique_ptr<WaitAndExitDelegate> delegate(new WaitAndExitDelegate(duration));

  const bool thread_created = base::PlatformThread::CreateNonJoinable(0, delegate.get());

  if (!thread_created) {
    return false;
  }

  WaitAndExitDelegate* leaking_delegate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delegate);
  ignore_result(leaking_delegate);
  return true
}
#endif

class SuicideOnChannelErrorFilter : public IPC::MessageFilter {
  public:
    void OnChannelError() override {

      base::debug::StopProfiling();
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
      defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
      defined(UNDERFINED_SANITIZER)
      CHECK(CreateWaitAndExitThread(base::TimeDelta::FromSeconds(60)));
#if defined(LEAK_SANITIZER)
      __lsan_do_leak_check();
#endif
#else
      base::Process::TerminateCurrentProcessImmediately(0);
#endif
    }

  protected:
    ~SuicideOnChannelErrorFilter() override {}
    // 如果你不想让外面的用户直接构造一个类（假设这个类的名字为A）的对象,
    // 而希望用户只能构造这个类A的子类，那你就可以将类A的构造函数/析构函
    // 数声明为protected，而将类A的子类的构造函数/析构函数声明为public
};

#endif // OS(POSIX)

#if 0
#if defined(OS_ANDROID)
class QuitClosure {
// TODO:
}
#endif
#endif

base::Optional<mojo::IncomingInvitation> InitializeMojoIPCChannel() {
  // TRACE_EVENT0("startup", "InitializeMojoIPCChannel");
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_POSIX)
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_mannage::kMojoIPCChannel))));
#endif

  if (!endpoint.is_valid())
    return base::nullopt;

  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

class ChannelBootstrapFilter : public ConnectionFilter {
  public:
    explicit ChannelBootstrapFilter(IPC::mojom::ChannelBootstrapPtrInfo bootstrap)
      : bootstrap_(std::move(bootstrap)) {}

  private:
    void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                         const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle* interface_pipe,
                         service_manager::Connector* connector) override {
#if 0
      if (source_info.identity.name() != mojom::kBrowserServiceName)
        return;
#endif

      if (interface_name == IPC::mojom::ChannelBootstrap::Name_) {
        DCHECK(bootstrap_.is_valid());
        mojo::FuseInterface(
          IPC::mojom::ChannelBootstrapRequest(std::move(*interface_pipe)),
          std::move(bootstrap_));
      }
    }

    IPC::mojom::ChannelBootstrapPtrInfo bootstrap_;

    DISALLOW_COPY_AND_ASSIGN(ChannelBootstrapFilter);
};

} // namespace

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

// 假如用户定义了其他构造函数（比如有参数的，或者参数不同的），那么编译器无论如何就不会再合成默认的构造函数了
// 因此如果要使用无参数版本，用户必须显示的定义一个无参版本的构造函数
ChildThreadImpl::Options::Options()
    : auto_start_services_mannager_connection(true),
      connect_to_browser(false) {}

// 在函数声明后加上”=default;”，就可将该函数声明为defaulted函数，编译器将为显式声明的defaulted函数自动生成函数体
// 我们只对具有合成版本的成员函数使用 "=default"
ChildThreadImpl::Options::Options(const Options& other) = default;

ChildThreadImpl::Options::~Options() {}

ChildThreadImpl::Options::Builder::Builder() {}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InBrowserProcess(
    const InProcessChildThreadParams& params) {
  options_.browser_process_io_runner = params.io_runner();
  options_.in_process_services_request_tooken = params.services_request_token();
  options_.mojo_invatation = params.mojo_invatation();

  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AutoStartServiceManagerConnection(bool auto_start) {
  options_.auto_start_services_mannager_connection = auto_start;

  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ConnectToBrowser(
    bool connect_to_browser_parms) {
  options_.connect_to_browser = connect_to_browser_parms;

  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AddStartupFilter(
    IPC::MessageFilter* filter) {
  options_.startup_filters.push_back(filter);

  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::IPCTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_parms) {
  options_.ipc_task_runner = ipc_task_runner_parms;

  return *this;
}

ChildThreadImpl::Options ChildThreadImpl::Options::Builder::Build() {
  return options_;
}

ChildThreadImpl::ChildThreadMessageRouter::ChildThreadMessageRouter(
    IPC::Sender* sender)
  : sender_(sender) {}

bool ChildThreadImpl::ChildThreadMessageRouter::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool ChildThreadImpl::ChildThreadMessageRouter::RouteMessage(const IPC::Message& msg) {
  bool handled = IPC::MessageRouter::RouteMessage(msg);

  return handled;
}

ChildThreadImpl::ChildThreadImpl()
    : route_provider_binding_(this),
      route_(this),
      channel_connected_factory_(new base::WeakPtrFactory<ChildThreadImpl>(this)),
      weak_factory_(this) {
  Init(Options::Builder::Build());
}

ChildThreadImpl::ChildThreadImpl(const Options& options)
    : // route_provider_binding_(this),
      route_(this),
      browser_process_io_runner_(options.browser_process_io_runner),
      channel_connected_factory_(new base::WeakPtrFactory<ChildThreadImpl>(this)),
      ipc_task_runner(options.ipc_task_runner),
      weak_factory_(this) {
  Init(options);
}

scoped_refptr<base::SingleThreadTaskRunner> ChildThreadImpl::GetIOTaskRunner() {
  if (IsInBrowserProcess()) {
    return browser_process_io_runner_;
  }

  return ChildProcess::current()->io_task_runner();
}

void ChildThreadImpl::ConnectChannel() {
  DCHECK(service_manager_connection_);
  IPC::mojom::ChannelBootstrapPtr bootstrap;
  mojo::ScopedMessagePipeHandle handle = mojo::MakeRequest(&bootstrap).PassMessagePipe();
  service_manager_connection_->AddConnectionFilter(
      std::make_unique<ChannelBootstrapFilter>(bootstrap.PassInterface()));

  channel_->Init(
      IPC::ChannelMojo::CreateClientFactory(
          std::move(handle), ChildProcess::current()->io_task_runner(),
          ipc_task_runner_ ? ipc_task_runner_ : base::ThreadTaskRunnerHandle::Get()),
      true /* create_pipe_now */);
}

void ChildThreadImpl::Init(const Options& options) {
  g_lazy_tls.Pointer()->Set(this);
  on_channel_error_called = false;
  main_thread_runner = base::ThreadTaskRunnerHandle::Get();

  channel_ = IPC::SyncChannel::Create(this, ChildProcess::current()->io_task_runner(),
                                      ipc_task_runner_ ? ipc_task_runner_ : base::ThreadTaskRunnerHandle::Get(),
                                      ChildProcess::current()->GetShutDownEvent());

  mojo::ScopedMessagePipeHandle service_request_pipe;

  if (!IsInBrowserProcess()) {
    mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
        GetIOTaskRunner(), mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));
    // c++14中将包含一个std::optional类，它的功能和用法和boost的optional类似。optional<T>
    // 内部存储空间可能存储了T类型的值也可能没有存储T类型的值，只有当optional被T初始化之后,
    // 这个optional才是有效的，否则是无效的，它实现了未初始化的概念。
    base::Optional<mojo::IncomingInvitation> invitation = InitializeMojoIPCChannel();

    std::string services_request_token =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            service_manager::switches::kServiceRequestChannelToken);

    if (!services_request_token.empty() && invitation) {
      service_request_pipe = invitation->ExtractMessagePipe(services_request_token);
    }
  } else {
    service_request_pipe = options.mojo_invatation->ExtractMessagePipe(
        options.in_process_service_request_tooken);
  }

  if (service_request_pipe.is_valid()) {
    service_manager_connection_ = ServiceManagerConnection::Create(
        service_manager::mojom::ServiceRequest(std::move(service_request_pipe)),
        GetIOTaskRunner());
  }

  sync_message_filter_ = channel_->CreateSyncMessageFilter();
  thread_safe_sender_ = new ThreadSafeSender(main_thread_runner_, sync_message_filter_.get());
  auto registry = std::make_unique<service_manager::BinderRegistry>();
#if 0
  registry->AddInterface(base::Bind(&ChildHistogramFetcherFactoryImpl::Create),
                         GetIOTaskRunner());
#endif
  registry->AddInterface(base::Bind(&ChildThreadImpl::OnChildControlRequest, base::Unretained(this)),
                         base::ThreadTaskRunnerHandle::Get());
  GetServiceManagerConnection()->AddConnectionFilter(
      std::make_unique<SimpleConectionFilter>(std::move(registry)));

  // omit something

#if defined(OS_POSIX)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType))
    channel_->AddFilter(new SuicideOnChannelErrorFilter());
#endif

  for (auto* startup_filter : options.startup_filters) {
    channel_->AddFilter(startup_filter);
  }

  ConnectChannel();

  if (options.auto_start_services_mannager_connection &&
      service_manager_connection_) {
    StartServiceManagerConnection();
  }

  int connection_timeout = kConnectionTimeoutS;
  std::string connection_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIPCConnectionTimeout);

  if (!connection_override.empty()) {
    int temp;
    if (base::StringToInt(connection_override, &temp))
      connection_timeout = temp;
  }

  main_thread_runner_->PostDelayedTask(FROM_HERE,
                                       base::BindOnce(&ChildThreadImpl::EnsureConnected,
                                                      channel_connected_factory_.GetWeakPtr()),
                                       base::TimeDelta::FromSeconds(connection_timeout));
}

ChildThreadImpl::~ChildThreadImpl() {
  channel_->RemoveFilter(sync_message_filter_.get());
  channel_->ClearIPCTaskRunner();
  g_lazy_tls.Pointer()->Set(nullptr);
}

void ChildThreadImpl::Shutdown() {

}

void ChildThreadImpl::ShouldBeDestroyed() {
  return true;
}

void ChildThreadImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_factory_.reset();
}

void ChildThreadImpl::OnChannelError() {
  on_channel_error_called_ = true;

  if (!IsInBrowserProcess())
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

bool ChildThreadImpl::Send(IPC::Message* msg) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (!channel_) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

ServiceManagerConnection* ChildThreadImpl::GetServiceManagerConnection() {
  return service_manager_connection_.get();
}

service_manager::Connector* ChildThreadImpl::GetConnector() {
  return service_manager_connection_->GetConnector();
}

IPC::MessageRouter* ChildThreadImpl::GetRouter() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  return &router_;
}

mojom::RouteProvider* ChildThreadImpl::GetRemoteRouteProvider() {
  if (!remote_route_provider_) {
    DCHECK(channel_);
    channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
  }

  return remote_route_provider_.get();
}

bool ChildThreadImpl::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.OnMessageReceived(msg);
}

void ChildThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == mojom::RouteProvider::Name_) {
    DCHECK(!route_provider_binding_.is_bound());
    route_provider_binding_.Bind(mojom::RouteProviderAssociatedRequest(std::move(handle)),
                                 ipc_task_runner_ ? ipc_task_runner_ : base::ThreadTaskRunnerHandle::Get());
  } else {
    LOG(ERROR) << "Request for unknown Channel-associated interface:" << interface_name;
  }
}

void ChildThreadImpl::StartServiceManagerConnection() {
  DCHECK(service_manager_connection_);
  service_manager_connection_->Start();
  // GetContentClient()->OnServiceManagerConnected(service_manager_connection_.get());
}

bool ChildThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return false;
}

void ChildThreadImpl::ProcessShutdown() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void ChildThreadImpl::OnChildControlRequest(mojo::ChildControlRequest request) {
  child_control_bindings_.AddBinding(this, std::move(request));
}

ChildThreadImpl* ChildThreadImpl::current() {
  return g_lazy_tls.Pointer()->Get();
}

void ChildThreadImpl::OnProcessFinalRelease() {
  if (on_channel_error_called_)
    return;

  ProcessShutdown();
}

void ChildThreadImpl::EnsureConnected() {
  VLOG(0) << "ChildThreadImpl::EnsureConnected()";
  base::Porcess::TerminateCurrentProcessImmediately(0);
}

void ChildThreadImpl::GetRouter(int32_t routing_id,
                                blink::mojom::AssociatedInterfaceProviderAssociatedRequest request) {
  associated_interface_provider_bindings_.AddBinding(this, std::move(request), routing_id);
}

void ChildThreadImpl::GetAssociatedInterface(const std::string& name,
                                             blink::mojom::AssociatedInterfaceAssociatedRequest request) {
  int32_t routing_id = associated_interface_provider_bindings_.dispatch_context();
  Listener* route = route_.GetRouter(routing_id);
  if (route)
    route->OnAssociatedInterfaceRequest(name, request.PassHandle());
}

bool ChildThreadImpl::IsInBrowserProcess() const {
  // return static_cast<bool>(browser_process_io_runner_);
  return false;
}

} // namespace content
