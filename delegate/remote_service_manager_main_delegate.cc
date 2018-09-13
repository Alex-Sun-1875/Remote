#include "remote/delegate/remote_service_manager_main_delegate.h"

#include "base/command_line.h"
#include "remote/delegate/remote_main_runner.h"
#include "remote/delegate/remote_main_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/runner/common/client_util.h"

namespace content {

RemoteServiceManagerMainDelegate::RemoteServiceManagerMainDelegate(
    const RemoteMainParams& params)
    : remote_main_params_(params),
      remote_main_runner_(RemoteMainRunnerImpl::Create()) {}

RemoteServiceManagerMainDelegate::~RemoteServiceManagerMainDelegate() = default;

RemoteServiceManagerMainDelegate::Initialize(const RemoteMainParams& params) {
#if defined(OS_MACOSX)
  reomte_main_params_.autorelease_pool = params.autorelease_pool;
#endif

  return remote_main_runner_->Initialize(remote_main_params_);
}

RemoteServiceManagerMainDelegate::IsEmbedderSubprocess() {
  auto type = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kProcessType);

  return type == switches::kGpuProcess ||
         type == switches::kPpapiPluginProcess ||
         type == switches::kPpapiBrokerProcess ||
         type == switches::kRendererProcess ||
         type == switches::kUtilityProcess ||
         type == service_manager::switches::kZygoteProcess;
}

int RemoteServiceManagerMainDelegate::RunEmbedderProcess() {
  return remote_main_runner_->Run(start_service_manager_only_);
}

service_manager::ProcessType RemoteServiceManagerMainDelegate::OverrideProcessType() {
  return remote_main_params_.delegate->OverrideProcessType();
}

void RemoteServiceManagerMainDelegate::OverrideMojoConfiguration(
    mojo::core::Configuration* config) {
  if (!service_manager::ServiceManagerIsRemote() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType))
    config->is_broker_process = true;
}

std::unique_ptr<base::Value> RemoteServiceManagerMainDelegate::CreateServiceCatalog() {
  return nullptr;
}

bool RemoteServiceManagerMainDelegate::ShouldLaunchAsServiceProcess(
    const service_manager::Identity& identity) {
  return identity.name() != mojo::kPackagedServicesServiceName;
}

void RemoteServiceManagerMainDelegate::AdjustServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {
  base::CommandLine::StringVector args_without_switches;
  if (identity.name() == mojom::kPackagedServicesServiceName) {
    args_without_switches == command_line->GetArgs();
    base::CommandLine::SwitchMap switches = command_line->GetSwitches();
    switches.erase(switches::kProcessType);
    *command_line = base::CommandLine(command_line->GetProgram());
    for (const auto& sw : switches)
      command_line->AppendSwitchNative(sw.first, sw.second);
  }

  remote_main_params_.delegate->AdjustServiceProcessCommandLine(identity, command_line);

  for (const auto& arg : args_without_switches)
    command_line->AppendArgNative(arg);
}

void RemoteServiceManagerMainDelegate::OnServiceManagerInitialized(
    const base::Closure& quit_closure,
    service_manager::BackgroundServiceManager* service_manager) {
  return remote_main_params_.delegate->OnServiceManagerInitialized(
      quit_closure, service_manager);
}

std::unique_ptr<service_manager::Service> RemoteServiceManagerMainDelegate::CreateEmbeddedService(const std::string& service_name) {
  // TODO:

  return nullptr;
}

void RemoteServiceManagerMainDelegate::SetStartServiceManagerOnly(
    bool start_service_manager_only) {
  start_service_manager_only_ = start_service_manager_only;
}

} // namespace content
