#ifndef CHILD_THREAD_IMPL_H_
#define CHILD_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
//#include "base/metrics/field_trial.h"
//#include "base/power_monitor/power_monitor.h"

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"

// #include "components/variations/child_process_field_trial_syncer.h"
// #include "content/child/memory/child_memory_coordinator_impl.h"
// #include "content/common/associated_interfaces.mojom.h"
// #include "content/common/child_control.mojom.h"
// #include "content/common/content_export.h"

#include "child_thread.h"

#include "ipc/ipc.mojom.h"
// #include "ipc/ipc_buildflags.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/message_router.h"

#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"

#if 0
#if defined(OS_WIN)
#include "content/public/common/font_cache_win.mojom.h"
#elif defined(OS_MACOSX)
#include "content/common/font_loader_mac.mojom.h"
#endif
#endif

namespace IPC {
class MessageFilter;
class SyncChannel;
class SyncMessageFilter;
} // namespace IPC

namespace mojo {
class OutGoingInvitation;
namespace {
class ScopedIPCSupport;
} // core
} // namespace mojo

namespace content {

} // namespace content

#endif
