#ifndef CHILD_THREAD_IMPL_H_
#define CHILD_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
//#include "base/metrics/field_trial.h"
//#include "base/power_monitor/power_monitor.h"

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"

// #include "components/variations/child_process_field_trial_syncer.h"
// #include "content/child/memory/child_memory_coordinator_impl.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/child_control.mojom.h"
#include "content/common/content_export.h"

#include "child_thread.h"

#include "ipc/ipc.mojom.h"
#include "ipc/ipc_buildflags.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/message_router.h"

#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
// #include "services/tracing/public/cpp/trace_event_agent.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"

#if 0
#if defined(OS_WIN)
#include "content/public/common/font_cache_win.mojom.h"
#elif defined(OS_MACOSX)
#include "content/common/font_loader_mac.mojom.h"
#endif
#endif

namespace IPC {
class MessageFilter;
class SyncChannel;
class SyncMessageFilter;
} // namespace IPC

namespace mojo {
class OutGoingInvitation;
namespace core {
class ScopedIPCSupport;
} // core
} // namespace mojo

namespace content {
class InProcessChildThreadParams;
class ThreadSafeSender;

class CONTENT_EXPORT ChildThreadImpl
    : public IPC::Listener,
      virtual public ChildThread,
      // private base::FiledTrialList::Observer,
      // public ChildMemoryCoordinatorDelegate,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider,
      public mojom::ChildControl {
public:
  struct CONTENT_EXPORT Options;

  ChildThreadImpl();
  explicit ChildThreadImpl(const Options& options);
  ~ChildThreadImpl() override; // 类的析构函数只有一个,不能够被继承,只能被重写

  virtual void ShutDown();
  virtual bool ShouldBeDestroyed();

  bool Send(IPC::Message* message) override;

  ServiceManagerConnection* GetServiceManagerConnection() override;
  service_manager::Connector* GetConnector() override;

  scoped_redptr<base::SingleThreadTaskRunner> GetIOTaskRunner();

  IPC::SyncChannel* channel() { return channel_.get(); }
  IPC::MessageRouter* GetRouter();

  mojom::RouteProvider* GetRemoteRouteProvider();

  IPC::SyncMessageFilter* sync_message_filter() const {
    return sync_message_filter_.get();
  }

  ThreadSafeSender* thread_safe_sender() cpnst {
    return thread_safe_sender_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner() const {
    return main_thread_runner_;
  }

  static ChildThreadImpl* current();

#if 0
#if defined(OS_ANDROID)
  static void ShutDownThread();
#endif
#endif

protected:
  friend class ChildProcess; // ChildProcess class 可以访问 ChildThreadImpl class 的私有成员

  virtual void OnPorcessFinalRelease();

  // mojom::ChildControl
  void ProcessShutDown() override;
  void OnChildControlRequest(mojom::ChildControlRequest);
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(const std::string& interface_name, mojo::ScopedInterfaceEndPointHandle handle) override;
  void OnChannelConnected(int32_t peer_id) override;
  bool on_channel_error_called() const { return on_channel_error_called_; }
  bool IsInBrowserProcess() const;

private:
  class ChildThreadMessageRouter : public IPC::MessageRouter {
    public:
      explicit ChildThreadMessageRouter(IPC::Sender* sender);
      bool Send(IPC::Message* msg) override;

      bool RouteMessage(const IPC::Message& msg) override;

    private:
      IPC::Sender* const sender_;
  }

  void Init(const Options& options);

  // void InitTracing();

  void ConnectChannel();

  void EnsureConnected();

  void GetRouter(int32_t routing_id, blink::mojom::AssociatedInterfaceProviderAssociatedRequest request) override;

  std::unique_ptr<IPC::SyncChannel> channel_;

  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  ChildThreadMessageRouter router_;

  bool on_channel_error_called_;

  scoped_refptr<base::SingleThreadTaskRunner> browser_process_io_runner;

  std::unique_ptr<base::WeakPtrFactory<ChildThreadImpl>> channel_connected_factory_;

  scoped_redptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  base::WeakPtrFactory<ChildThreadImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildThreadImpl);
};

struct ChildThreadImpl::Options {
  Options(const Options& other);
  ~Options();

  class Builder;

  bool auto_start_services_mannager_connection;
  bool connect_to_browser;
  scoped_refptr<base::SingleThreadTaskRunner> browser_process_io_runner;
  std::vector<IPC::MessageFilter*> startup_filters;
  mojo::OutGoingInvitation* mojo_invatation;
  std::string in_process_services_request_tooken;
  scoped_redptr<base::SingleThreadTaskRunner> ipc_task_runner;

private:
  Options();
};

class ChildThreadImpl::Options::Builder {
  public:
    Builder();

    Builder& InBrowserProcess(const InProcessChildThreadParams& params);
    Builder& AutoStartServiceManagerConnection(bool auto_start);
    Builder& ConnectToBrowser(bool connect_to_browser);
    Builder& AddStartupFilter(IPC::MessageFilter* filter);
    Builder& IPCTaskRunner(scoped_redptr<base::SingleThreadTaskRunner> ipc_task_runner);
    Options Build();

  private:
    struct Options options_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
};

} // namespace content

#endif // CHILD_THREAD_IMPL_H_
