#include <iostream>
#include "base/command_line.h"
#include "remote/child/child_process.h"
#include "remote/child/child_thread_impl.h"
#include "ipc/ipc_channel.h"

int main(int argc, char* argv[]) {
  std::cout << "Start A Remote!!!" << std::endl;
#if 0
  base::PlatformThread::SetName("RemoteMain");

  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  cmd_line->AppendSwitchASCII("", "");

  content::ChildProcess child_process;
  content::ChildThreadImpl* child_thread = new content::ChildThreadImpl();
  child_process.set_main_thread(child_thread);
#endif

  return 0;
}
