#ifndef REMOTE_MAIN_RUNNER_H_
#define REMOTE_MAIN_RUNNER_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

class RemoteMainParams;

class CONTENT_EXPORT RemoteMainRunner {
  public:
    virtual ~RemoteMainRunner() {}

    static RemoteMainRunner* Create();

    virtual int Initialize(const RemoteMainParams& params) = 0;

    virtual int Run(bool start_service_manager_only) = 0;

    virtual void Shutdown() = 0;
};

} // namespace content

#endif // REMOTE_MAIN_RUNNER_H_
