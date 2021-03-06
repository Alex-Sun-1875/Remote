#ifndef REMOTE_MAIN_H_
#define REMOTE_MAIN_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "remote/common/content_export.h"

namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}

namespace content {

class RemoteMainDelegate;

struct RemoteMainParams {
  explicit RemoteMainParams(RemoteMainDelegate* delegate) : delegate(delegate) {}

  RemoteMainDelegate* delegate;

#if !defined(OS_ANDROID)
  int argc = 0;
  const char** argv = nullptr;
#endif

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#endif
};

CONTENT_EXPORT int RemoteMain(const RemoteMainParams& params);

} // namespace content

#endif  // REMOTE_MAIN_H_
