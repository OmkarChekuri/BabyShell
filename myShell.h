#ifndef __MY_SHELL_H__
#define __MY_SHELL_H__
#include <map>
#include <vector>
#include <string>

class MyShell {
private:
  bool exitting;
  std::string input;
  std::vector<std::string> piped_commands;
  std::vector<std::string> commands;
  std::map<std::string, std::string> vars;
  int * pipefd;
  std::string redirect_input_file;
  std::string redirect_output_file;
  std::string redirect_error_file;
  void setVar(std::string key, std::string value);
  void parsePipedInput();
  void parseCommand(int i);
  void evaluateVars();
  bool searchCommand();
  void runExitCommands(std::size_t command_index);
  void runCdCommand(std::size_t command_index);
  void runSetCommand(std::size_t command_index);
  void runExportCommand(std::size_t command_index);
  void runCommand(char ** c_commands, std::size_t command_index);
  void runPipedCommands();
  void refresh();
  void configCommandPipe(std::size_t command_index);
public:
  MyShell();
  void execute();
  bool isExitting();
};


#endif
