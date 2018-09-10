#include <iostream>
#include "base/command_line.h"
#include "base/run_loop.h"
#include "remote/child/child_process.h"
#include "remote/child/child_thread_impl.h"
#include "ipc/ipc_channel.h"

int main(int argc, char* argv[]) {
  std::cout << "Start A Remote!!!" << std::endl;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  cmd_line->AppendSwitchASCII("Hello", "World");

  std::unique_ptr<base::MessageLoop>  main_message_loop;
  main_message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
  base::PlatformThread::SetName("RemoteMain");

  base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;

  content::ChildProcess child_process(io_thread_priority);

  base::RunLoop run_loop;
  content::ChildThreadImpl* child_thread = new content::ChildThreadImpl(run_loop.QuitClosure());
  child_process.set_main_thread(child_thread);

  run_loop.Run();

  return 0;
}
