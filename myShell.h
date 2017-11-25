#ifndef __MY_SHELL_H__
#define __MY_SHELL_H__
#include <map>
#include <vector>
#include <string>

class MyShell {
private:
  bool exitting;
  std::string command;
  std::vector<std::string> commands;
  std::map<std::string, std::string> vars;
  std::string redirect_input_file;
  std::string redirect_output_file;
  std::string redirect_error_file;
  void setVar(std::string key, std::string value);
  void parseCommand();
  void evaluateVars();
  bool searchCommand();
  void runExitCommands();
  void runCdCommand();
  void runSetCommand();
  void runExportCommand();
  void runCommand(char ** c_commands);
  void refresh();
public:
  MyShell();
  void execute();
  bool isExitting();
};


#endif
