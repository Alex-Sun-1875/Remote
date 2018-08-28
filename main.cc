#include <iostream>
#include "base/command_line.h"
#include "base/threading/platform_thread.h"

#include "content/child/child_process.h"
#include "content/child/child_thread_impl.h"

int main(int argc, char* argv[]) {
  std::cout << "Start A Remote!!!" << std::endl;
  base::PlatformThread::SetName("RemoteMain");

  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  cmd_line->AppendSwitchASCII("", "");

  base::MessageLoop main_loop;

  content::ChildProcess child_process;
  content::ChildThread::Options::Builder builder;
  content::ChildThreadImpl* child_thread = new ChildThreadImpl(builder.WithChannelName("0.0.0.0:12345")
                                                         .WithChannelMode(ipc::Channel::MODE_NAMED_SERVER)
                                                         .WithChannelType(ipc::Channel::TYPE_DGRAM)
                                                         .Build());
  child_process.set_main_thread(child_thread);
  // main_loop.Run();

  return 0;
}
