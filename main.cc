#include <iostream>
#include "base/command_line.h"

int main(int argc, char* argv[]) {
  std::cout << "Start A Remote!!!" << std::endl;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  return 0;
}
