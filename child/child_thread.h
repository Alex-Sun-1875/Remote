#ifndef CHILD_THREAD_H_
#define CHILD_THREAD_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class CONTENT_EXPORT ChildThread : public IPC::Sender {
  public:
    static ChildThread* Get();

    ~ChildThread() override { }
};

} // namespace content

#endif // CHILD_THREAD_H_
