#ifndef REMOTE_MAIN_DELEGATE_H_
#define REMOTE_MAIN_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "services/service_manager/embedder/process_type.h"

namespace base {
class CommandLine;
}

namespace service_manager {
class BackgroundServiceManager;
class Identity;
}

namespace content {

struct MainFunctionParams;

class RemoteMainDelegate {
  public:
    virtual ~RemoteMainDelegate() {}

    virtual bool BasicStartupComplete(int* exit_code);

    virtual int RunProcess(const std::string& process_type,
                           const MainFunctionParams& main_function_params);

    virtual void ProcessExiting(const std::string& process_type) {}

#if defined(OS_MACOSX)
    virtual bool ProcessRegistersWithSystemProcess(const std::string& process_type);

    virtual bool ShouldSendMachPort(const std::string& process_type);
#elif defined(OS_LINUX)
    // TODO:
#endif

    virtual int TerminateForFatalInitializationError();

    virtual service_manager::ProcessType OverrideProcessType();

    virtual void AdjustServiceProcessCommandLine(const service_manager::Identity& identify,
                                                 base::CommandLine* command_line);

    virtual void OnServiceManagerInitialized(const base::Closure& quit_closure,
                                             service_manager::BackgroundServiceManager* service_manager);

    virtual void PreCreateMainMessageLoop() {}

    virtual void PostEarlyInitialization() {}
};

} // namespace content

#endif // REMOTE_MAIN_DELEGATE_H_
