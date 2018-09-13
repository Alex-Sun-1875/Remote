#include "remote/delegate/remote_main.h"

#include "remote/delegate/remote_service_manager_main_delegate.h"
#include "services/service_manager/embedder/main.h"

namespace content {

int RemoteMain(const RemoteMainParams& params) {
  RemoteServiceManagerMainDelegate delegate(params);
  service_manager::MainParams main_params(&delegate);
#if !defined(OS_WIN) || !defined(OS_ANDROID)
  main_params.argc = params.argc;
  main_params.argv = params.argv;
#endif

  return service_manager::Main(main_params);
}

} // namespace content
