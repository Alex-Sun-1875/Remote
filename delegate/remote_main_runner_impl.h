#ifndef REMOTE_MAIN_RUNNER_IMPL_H_
#define REMOTE_MAIN_RUNNER_IMPL_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "remote/delegate/remote_main.h"
#include "remote/delegate/remote_main_runner.h"

namespace base {
class AtExitManager;
}

namespace content {
class RemoteMainDelegate;
struct RemoteMainParams;

class RemoteMainRunnerImpl : public RemoteMainRunner {
  public:
    static RemoteMainRunnerImpl* Create();

    RemoteMainRunnerImpl();
    ~RemoteMainRunnerImpl() override;

    int TerminateForFatalInitializationError();

    // RemoteMainRunner
    int Initialize(const RemoteMainParams& params) override;
    int Run(bool start_service_manager_only) override;
    void Shutdown() override;

  private:
    bool is_initialized_ = false;
    bool is_shutdown_ = false;
    bool completed_basic_startup_ = false;
    RemoteMainDelegate* delegate_ = nullptr;
    std::unique_ptr<base::AtExitManager> exit_manager_;

#if defined(OS_MACOSX)
    base::mac::ScopedNSAutoreleasePool* autorelease_pool_ = nullptr;
#endif

    std::unique_ptr<base::MessageLoop> main_message_loop_;
    std::unique_ptr<StartupDataImpl> startup_data_;

    DISALLOW_COPY_AND_ASSIGN(RemoteMainRunnerImpl);
};

} // namespace content

#endif
