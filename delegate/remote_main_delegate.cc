#include "remote/delegate/remote_main_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/process/process_info.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"

namespace {
struct MainFunction {
  const char* name;
  int (*function)(const content::MainFunctionParams&);
};
} // namespace

namespace content {

RemoteMainDelegate::RemoteMainDelegate()
    : RemoteMainDelegate(base::TimeTicks()) {}

RemoteMainDelegate::RemoteMainDelegate(base::TimeTicks exe_entry_point_ticks) {}

RemoteMainDelegate::~RemoteMainDelegate() {}

bool RemoteMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

int RemoteMain(const content::MainFunctionParams& params) {
  LOG(INFO) << "###sunlh### func: " << __func__ << ", Start Run!!!";

  return 0;
}

int RemoteMainDelegate::RunProcess(const std::string& process_type,
                                   const content::MainFunctionParams& main_function_params) {
#if !defined(OS_ANDROID)
  static const MainFunction kMainFunctions[] = {
    {"RemoteMain", RemoteMain},
    {"<invalid>", NULL},
  };

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_function_params);
  }
#endif
  return -1;
}

void RemoteMainDelegate::ProcessExiting(const std::string& process_type) {}

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

void RemoteMainDelegate::PreCreateMainMessageLoop() {}

} // namespace content
