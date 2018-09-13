#include <iostream>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "remote/child/child_process.h"
#include "remote/child/child_thread_impl.h"
#include "ipc/ipc_channel.h"

#include "base/posix/global_descriptors.h"

// mojo include file
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

// service manager include file
#include "services/service_manager/embedder/descriptors.h"
#include "services/service_manager/embedder/main_delegate.h"
#include "services/service_manager/embedder/process_type.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/init.h"

#include "base/logging.h"

int main(int argc, char* argv[]) {
  std::cout << "Start A Remote!!!" << std::endl;
  base::CommandLine::Init(argc, argv);
  // Chromium 中创建进程的时候需要一个 AtExitManager
  base::AtExitManager at_exit;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  // Init mojo
  // mojo::core::Configuration mojo_config;
  mojo::core::Init();
  base::GlobalDescriptors::GetInstance()->Set(service_manager::kMojoIPCChannel,
                                              service_manager::kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);

  cmd_line->AppendSwitchASCII("Hello", "World");

  std::unique_ptr<base::MessageLoop>  main_message_loop;
  main_message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
  base::PlatformThread::SetName("RemoteMain");

  base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;

  content::ChildProcess child_process(io_thread_priority);

  base::RunLoop run_loop;
  LOG(INFO) << "###sunlh### func: " << __func__ << ", create child thread start!";
  content::ChildThreadImpl* child_thread = new content::ChildThreadImpl(run_loop.QuitClosure());
  LOG(INFO) << "###sunlh### func: " << __func__ << ", create child thread end!";
  child_process.set_main_thread(child_thread);

  run_loop.Run();

  return 0;
}
