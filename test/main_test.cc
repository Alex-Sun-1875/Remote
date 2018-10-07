#include <iostream>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "remote/child/child_process.h"
#include "remote/child/child_thread_impl.h"
#include "ipc/ipc_channel.h"

#include "base/posix/global_descriptors.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

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

void SaySomeThing(std::string str) {
  LOG(INFO) << "Say: " << str << ", pid = " << getpid() << ", ppid = " << getppid() <<", tid = " << pthread_self();
}

void Print(std::string str) {
  content::ChildProcess::current()->main_thread()->main_thread_runner()->PostTask(FROM_HERE,
                                                                         base::Bind(&SaySomeThing, str));
}

int main(int argc, char* argv[]) {
  std::cout << "Start A Remote!!!" << std::endl;
  LOG(INFO) << ", pid = " << getpid() << ", ppid = " << getppid() <<", tid = " << pthread_self();
  base::CommandLine::Init(argc, argv);
  // Chromium 中创建进程的时候需要一个 AtExitManager
  base::AtExitManager at_exit;
  // base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  // Init mojo
  // mojo::core::Configuration mojo_config;
  // mojo::core::Init();
  // base::GlobalDescriptors::GetInstance()->Set(service_manager::kMojoIPCChannel,
  //                                            service_manager::kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);

  // cmd_line->AppendSwitchASCII("service-request-channel-token", base::NumberToString(base::RandUint64()));

  // std::unique_ptr<base::MessageLoop>  main_message_loop;
  // main_message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
  // base::PlatformThread::SetName("RemoteMain");

  // base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;

  // content::ChildProcess child_process(io_thread_priority);

  base::RunLoop run_loop;
  LOG(INFO) << "###sunlh### func: " << __func__ << ", create child thread start!";
  // content::ChildThreadImpl* child_thread = new content::ChildThreadImpl(run_loop.QuitClosure());
  LOG(INFO) << "###sunlh### func: " << __func__ << ", create child thread end!";
  LOG(INFO) << "###sunlh### func: " << __func__ << ", set main thread begin!";
  // child_process.set_main_thread(child_thread);
  LOG(INFO) << "###sunlh### func: " << __func__ << ", set main thread end!";

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&SaySomeThing, "Hello World!!!"));
  // base::RepeatingTimer timer;
  // timer.Start(FROM_HERE, base::TimeDelta::FromSeconds(2),
  //             base::Bind(&Print, "Hello World!!!"));

  run_loop.Run();

  return 0;
}
