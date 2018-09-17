#ifndef REMOTE_SERVICE_MANAGER_MAIN_DELEGATE_H_
#define REMOTE_SERVICE_MANAGER_MAIN_DELEGATE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "services/service_manager/embedder/main_delegate.h"

#include "remote/delegate/remote_main.h"

namespace content {

class RemoteMainRunnerImpl;

class RemoteServiceManagerMainDelegate : public service_manager::MainDelegate {
  public:
    explicit RemoteServiceManagerMainDelegate(const RemoteMainParams& params);

    ~RemoteServiceManagerMainDelegate() override;

    // service_manager::MainDelegate:
    int Initialize(const InitializeParams& params) override;
    bool IsEmbedderSubprocess() override;
    int RunEmbedderProcess() override;
    void ShutDownEmbedderProcess() override;
    service_manager::ProcessType OverrideProcessType() override;
    void OverrideMojoConfiguration(mojo::core::Configuration* config) override;
    std::unique_ptr<base::Value> CreateServiceCatalog() override;
    bool ShouldLaunchAsServiceProcess(const service_manager::Identity& identity) override;
    void AdjustServiceProcessCommandLine(const service_manager::Identity& identity,
                                         base::CommandLine* command_line) override;
    void OnServiceManagerInitialized(const base::Closure& quit_closure,
                                     service_manager::BackgroundServiceManager* service_manager) override;
    std::unique_ptr<service_manager::Service> CreateEmbeddedService(const std::string& service_name) override;
    void SetStartServiceManagerOnly(bool start_service_manager_only);

  private:
    RemoteMainParams remote_main_params_;
    std::unique_ptr<RemoteMainRunnerImpl> remote_main_runner_;
    bool start_service_manager_only_ = false;

    DISALLOW_COPY_AND_ASSIGN(RemoteServiceManagerMainDelegate);
};

} // namespace content

#endif
