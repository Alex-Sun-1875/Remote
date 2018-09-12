#include "remote/delegate/remote_main_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace content {

bool RemoteMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

int RemoteMainDelegate::RunProcess(const std::string& process_type,
                                   const content::MainFunctionParams& main_function_params) {
  return -1;
}

#if defined(OS_MACOSX)

bool RemoteMainDelegate::ProcessRegistersWithSystemProcess(const std::string& process_type) {
  return false;
}

bool RemoteMainDelegate::ShouldSendMachPort(const std::string& process_type) {
  return false;
}

#elif defined(OS_LINUX)

// TODO:

#endif

int RemoteMainDelegate::TerminateForFatalInitializationError() {
  CHECK(false);

  return 0;
}

service_manager::ProcessType RemoteMainDelegate::OverrideProcessType() {
  return service_manager::ProcessType::kDefault;
}

void RemoteMainDelegate::AdjustServiceProcessCommandLine(const service_manager::Identity& identity,
                                                         base::CommandLine* command_line) {}

void RemoteMainDelegate::OnServiceManagerInitialized(const base::Closure& quit_closure,
                                                     service_manager::BackgroundServiceManager* service_manager) {}

} // namespace content
