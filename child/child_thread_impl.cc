#include "child_thread_impl.h"

#include <signal.h>
#include <string>
#include <utility>

#include "base/base_switchs.h"
#include "base/command_line.h"
// #include "base/debug/alias.h"
// #include "base/debug/leak_annotations.h"
// #include "base/debug/profiler.h"
#include "base/lazy_instance.h"
#include "base/locations.h"
#include "base/logging.h"
#include "base/macros.h"
// #include "base/memory/memory_pressure_listener.h"
#include "base/message_loop/timer_slack.h"
// #include "base/metrics/field_trial.h"
// #include "base/metrics/histogram_macros.h"
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
#include "build/build_config.h"
// #include "components/tracing/child/child_trace_message_filter.h"
#include "content/child/child_histogram_fetcher_impl.h"

#include "child_process.h"
#include "thread_safe_sender.h"

#if 0
#include "content/common/field_trial_recorder.mojom.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/simple_connection_filter.h"
#endif

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

#if 0
#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/sandbox/sandbox_type.h"

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

class WaitAndExitDelefate : public base::PlatformThread::Delegate {
  public:
    explicit WaitAndExitDelefate(base::TimeDelta duration)
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

    DISALLOW_COPY_AND_ASSIGN(WaitAndExitDelefate);
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  std::unique_ptr<WaitAndExitDelefate> delegate(new WaitAndExitDelefate(duration));

  const bool thread_created = base::PlatformThread::CreateNonJoinable(0, delegate.get());

  if (!thread_created) {
    return false;
  }

  WaitAndExitDelefate* leaking_delefate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delefate);
  ignore_result(leaking_delefate);
  return true
}
#endif

class SuicideOnChannelErrorFilter : IPC::MessageFilter {
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
      if (source_info.identity.name() != mojom::kBrowserServiceName)
        return;

      if (interface_name == IPC::mojom::ChannelBootstrap::Name_) {
        DCHECK(bootstrap_.is_valid());
        mojo::FuseInterface(
          IPC::mojom::ChannelBootstrapRequest(std::move(*interface_pipe)),
          std::move(bootstrap_));
      }
    }

    IPC::mojom::ChannelBootstrapPtrInfo bootstrap_;

    DISALLOW_COPY_AND_ASSIGN(ChannelBootstrapFilter);
}


}

} // namespace content
