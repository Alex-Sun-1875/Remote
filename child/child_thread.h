#ifndef CHILD_THREAD_H_
#define CHILD_THREAD_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "remote/common/content_export.h"
#include "ipc/ipc_sender.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class Connector;
}

namespace content {

class ServiceManagerConnection;

class CONTENT_EXPORT ChildThread : public IPC::Sender {
  public:
    static ChildThread* Get();

    ~ChildThread() override {}

    virtual ServiceManagerConnection* GetServiceManagerConnection() = 0;
    virtual service_manager::Connector* GetConnector() = 0;
    virtual scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() = 0;
};

} // namespace content

#endif // CHILD_THREAD_H_
