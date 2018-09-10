#ifndef CHILD_PROCESS_H_
#define CHILD_PROCESS_H_

#include <memory>
#include <vector>

#include <base/macros.h>
#include <base/synchronization/waitable_event.h>
#include <base/task/task_scheduler/task_scheduler.h>
#include <base/threading/platform_thread.h>
#include <base/threading/thread.h>
#include <remote/common/content_export.h>

namespace content {
class ChildThreadImpl;

// Note: IO thread outlives the ChildThradImpl object
class CONTENT_EXPORT ChildProcess {
  public:
    // Normally you would immediately call set_main_thread after construction
    ChildProcess(
        base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL,
        const std::string& task_scheduler_name = "ContentChild",
        std::unique_ptr<base::TaskScheduler::InitParams>
            task_scheduler_init_params = nullptr);

    virtual ~ChildProcess();

    ChildThreadImpl* main_thread();

    void set_main_thread(ChildThreadImpl* thread);

    base::MessageLoop* io_message_loop() { return io_thread_.message_loop(); }
    base::SingleThreadTaskRunner* io_task_runner() {
      return io_thread_.task_runner().get();
    }

    base::PlatformThreadId io_thread_id() { return io_thread_.GetThreadId(); }

    base::WaitableEvent* GetShutDownEvent();

    virtual void AddRefProcess();
    virtual void ReleaseProcess();

    static ChildProcess* current();

  private:
    int ref_count_;

    base::WaitableEvent shutdown_event_;

    base::Thread io_thread_;

    std::unique_ptr<ChildThreadImpl> main_thread_;

    bool initialized_task_scheduler_ = false;

    DISALLOW_COPY_AND_ASSIGN(ChildProcess);
};

} // namespace content

#endif
