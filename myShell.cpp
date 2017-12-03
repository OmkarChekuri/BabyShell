#include "myShell.h"
#include <cstdlib>
#include <string>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iterator>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>

extern char ** environ;
/********************************/
/**********HELPER FUNCTIONS******/
/********************************/

/**
 * a valid variable name can only contain upper and lower letters, numbers, and underscores
 **/
bool validateVarName(std::string & var) {
  for (std::string::iterator it = var.begin(); it != var.end(); it++) {
    if (!isalnum(*it) && *it != '_') return false;
  }
  return true;
}

/**
 * transform the parsed command from a vector of strings into an array of char pointers,
 * which is required by the execve function
 * the 2d array is dynamically allocated, needs to be freed manually after use
 **/
char ** vector2array(std::vector<std::string> v) {
  char ** array = new char*[v.size() + 1];
  std::size_t count = 0;
  for (std::vector<std::string>::iterator it = v.begin(); it < v.end(); it++) {
    array[count] = new char[it->length() + 1];
    strncpy(array[count], it->c_str(), it->length() + 1);
    count++;
  }
  array[count] = NULL;
  return array;
}

/**
 * split the colon delimeted PATH string into a vector of strings on colon
 **/
std::vector<std::string> splitPath(const std::string & paths) {
  std::vector<std::string> paths_vector;
  std::istringstream iss(paths);
  std::string path;
  while(std::getline(iss, path, ':')) {
    paths_vector.push_back(path);
  }
  return paths_vector;
}

/****************************************/
/**********CLASS PRIVATE FUNCTIONS*******/
/****************************************/

/**
 * set the key-value pair into MyShell::vars
 * this function doesn't check if the variable name is valid
 * it's the caller's job to guarantee the variable name is valid
 **/
void MyShell::setVar(std::string key, std::string value) {
  vars[key] = value;
}

/**
 * parse the one-line user input on '|' to get each piped command
 * store all piped commands into std::vector<std::string> MyShell::piped_commands
 * if the last character in MyShell::input is '|', this is regarged as an error
 **/
void MyShell::parsePipedInput() {
  std::size_t start = 0;
  std::size_t pipe_pos = 0;
  while ((pipe_pos = input.find('|', start)) != std::string::npos) {
    piped_commands.push_back(input.substr(start, pipe_pos - start));
    start = pipe_pos + 1;
  }
  if (start == input.length()) {
    std::cerr << "cannot have | at the end of input" << std::endl;
    error = true;
    return;
  }
  piped_commands.push_back(input.substr(start));
}

/**
 * parse piped_commands[curr_command_index]
 * command is delimitered by whitespace, unless the whitespace is escaped, whitespace will be discarded
 * the first delimited "word" in the command is the command name
 * all other "words" are arguments to the command
 * the \ mark will be removed from the user input command string for the ease of future use
 * the MyShell::commands vector could be empty if the input is empty
 **/
void MyShell::parseCommand() {
  std::string curr_command = piped_commands[curr_command_index];
  std::string word;
  std::string modified_curr_command;
  for (std::size_t end = 0; end < curr_command.length(); end++) {
    if (curr_command[end] == ' ') {
      if ((end > 0 && curr_command[end-1] != ' ') || (end > 1 && curr_command[end-2] == '\\')) {
	        commands.push_back(std::string(word));
	        word.erase();
      }
      modified_curr_command.push_back(curr_command[end]);
    } else if (curr_command[end] == '\\') {
      if (end == curr_command.length() - 1) {
	        std::cerr << "cannot use escape mark at the end of a command" << std::endl;
          error = true;
	        return;
      }
      word.push_back(curr_command[++end]); // put the escaped char into the word stream
      modified_curr_command.push_back(curr_command[end]); // remove the \ from the input command
      if (end == curr_command.length() - 1) {
	       commands.push_back(std::string(word));
      }
    } else { // normal non-space and non-escape chars
      word.push_back(curr_command[end]);
      modified_curr_command.push_back(curr_command[end]);
      if (end == curr_command.length() - 1) {
	       commands.push_back(std::string(word));
      }
    }
  }
  piped_commands[curr_command_index] = modified_curr_command;
  evaluateVars();
}

/**
 * for each parsed command component (command itself or argument) in MyShell::commands
 * if it contains one or more variable (has $),
 * replace the variable name with the actual variable value
 * if the variable doesn't exist, replace the variable name with " "
 **/
void MyShell::evaluateVars() {
  for (std::vector<std::string>::iterator it = commands.begin(); it != commands.end(); it++) {
    if (it->find('$') == std::string::npos) continue;
    std::string curr = *it;
    std::string updated_curr = "";
    int begin = -1;
    for (std::size_t i = 0; i < curr.length(); i++) {
      if (curr[i] == '$') {
        if (begin == -1) {
          begin = i;
        } else {
          std::string var = curr.substr(begin + 1, i);
          begin = i;
          std::map<std::string, std::string>::iterator varsit = vars.find(var);
          if (varsit != vars.end()) { // find the variable, replace value
            var = varsit -> second;
          } else { // cannot find
            var = " ";
          }
          updated_curr.append(var);
        }
      } else {
        if (begin == -1) {
          updated_curr.push_back(curr[i]);
        }
      }
    }
    if (begin != -1) {
      std::string var = curr.substr(begin + 1);
      std::map<std::string, std::string>::iterator varsit = vars.find(var);
      if (varsit != vars.end()) { // find the variable, replace value
        var = varsit -> second;
      } else { // cannot find
        var = " ";
      }
      updated_curr.append(var);
    }
    *it = updated_curr;
  }
}

/**
 * search if the given path for the command really exists
 * commands guaranteed to be non-empty
 * commands[0] is the real command that we care about
 * 1. commands[0] contain '/': find if the file name exist
 * 2. commands[0] doesn't contain "/": search all paths in PATH, to see if the file name is in any one of them
 * in the two cases, if the file is found, return true, else return false
 **/
bool MyShell::searchCommand() {
  std::string command = commands[0];
  std::vector<std::string> paths = splitPath(vars["PATH"]);
  bool exist = false;
  if (command.find('/') != std::string::npos) {
    std::ifstream ifs(command.c_str());
    if (ifs.good()) {
      exist = true;
    }
  } else {
    for (std::vector<std::string>::iterator it = paths.begin(); it != paths.end(); it++) {
      std::string complete_path = *it + "/" + command;
      std::ifstream ifs(complete_path.c_str());
      if (ifs.good()) {
  	    exist = true;
  	    commands[0] = complete_path; // replace the shortened path with the complete one
  	    break;
      }
    }
  }
  return exist;
}

/**
 * run exit commands (EOF and "exit")
 * set the boolean variable exitting to true
 **/
void MyShell::runExitCommands() {
  // configCommandPipe();
  exitting = true;
}

/**
 * run "cd" command
 * cd command can only take 0 or 1 argument
 * 0 argument: cd to home directory
 * 1 argument: cd to the given argument if it exists
 **/
void MyShell::runCdCommand() {
  // configCommandPipe();
  if (commands.size() > 2) {
    std::cerr << "too many arguments for cd" << std::endl;
    error = true;
    return;
  }
  std::string dest = getenv("HOME");
  if (commands.size() == 2) {
    dest = commands[1];
  }
  if(chdir(dest.c_str()) != 0) { // chdir fails, the current directory doesn't change
    std::cerr << "cannot change directory: " << std::strerror(errno) << std::endl;
    error = true;
  } else {
    char cwd[1024];
    setVar("OLDPWD", vars["PWD"]);
    setenv("OLDPWD", vars["PWD"].c_str(), 1);
    setVar("PWD", getcwd(cwd, sizeof(cwd))); // get absolute working path in the case
    setenv("PWD", vars["PWD"].c_str(), 1);
  }
}

/**
 * run "set" command
 * the syntax has to be: set "variableName" "variableValue"
 * and there are a set of rules for valid variable name, refer to document
 **/
void MyShell::runSetCommand() {
  // configCommandPipe();
  if (commands.size() < 3) {
    std::cerr << "too few arguments for set: " << commands.size() << std::endl;
    error = true;
    return;
  }
  if (!validateVarName(commands[1])) {
    std::cerr << "invalid var name: var names can only contain letters (case sensitive), numbers and underscores" << std::endl;
    error = true;
    return;
  }
  std::size_t setPos = input.find(commands[0]); // the starting index of substr "set"
  std::size_t varPos = input.find(commands[1], setPos + commands[0].length()); // the start index of var name, with a valid var name, find won't have strange behavior
  std::size_t valPos = varPos + commands[1].length() + 1; // in this case, the starting index of value is one space after var name
  std::string value = input.substr(valPos);
  setVar(commands[1], value);
  std::cout << "set variable " << commands[1] << " with value " << vars[commands[1]] << std::endl;
}

/**
 * run "export" command
 * the "export" command can take any number of variables, and export each of them into environ
 * but if any of the variable names is invalid, the export process stops right at it, vars before it are exported, but all later vars (including itself) are not exported
 * if the exported var is in MyShell::vars, just export it into environ, do nothing else
 * else add the var and "" as its value into MyShell::vars, and then export it into environ
 **/
void MyShell::runExportCommand() {
  // configCommandPipe();
  for (std::vector<std::string>::iterator it = commands.begin() + 1; it != commands.end(); it++) {
    if (!validateVarName(*it)) {
      std::cerr << "invalid var name: var names can only contain letters (case sensitive), numbers and underscores" << std::endl;
      error = true;
      return;
    }
    std::map<std::string, std::string>::iterator varsit = vars.find(*it);
    std::string value = "";
    if (varsit != vars.end()) {
      value = varsit->second;
    } else {
      setVar(*it, "");
    }
    if (setenv(it->c_str(), value.c_str(), 1) != 0) { // 1: always replace
      std::cerr << "failed to export variable " << *it << ": " << std::strerror(errno) << std::endl;
      error = true;
    } else {
      std::cout << "export variable " << *it << " with value " << value << std::endl;
    }
  }
}

/**
 * in a child process
 * config redirect input and output
 * <: redirect stdin to the given input file
 * >: redirect stdout to the given outout file, if the file doesn't exist, create with permissions
 * 2>: redirect stderr to the given output file, if the file doesn't exist, create with permissions
 *    2>&1: redirect stderr to the same file as the the output file
 * TODO refactor, extract methods
 **/
void MyShell::configCommandRedirect() {
  std::string input_filename;
  std::string output_filename;
  std::string error_filename;
  for (std::vector<std::string>::iterator it = commands.begin() + 1; it != commands.end(); ) {
    std::string curr = *it;
    if (curr.compare(0, 1, "<") == 0) {
      if (curr.length() == 1) { // curr == "<"
        if (it + 1 == commands.end()) {
          std::cerr << "incorrect input format: < requires an input file" << std::endl;
          error = true;
          return;
        } else {
          input_filename = *(it + 1);
          commands.erase(it);
          commands.erase(it); // no need for it++ again
        }
      } else { // curr starts with "<"
        input_filename = curr.substr(1);
        commands.erase(it); // no need for it++ again
      }
    } else if (curr.compare(0, 1, ">") == 0) {
      if (curr.length() == 1) { // curr == ">"
        if (it + 1 == commands.end()) {
          std::cerr << "incorrect input format: > requires an output file" << std::endl;
          error = true;
          return;
        } else {
          output_filename = *(it + 1);
          commands.erase(it);
          commands.erase(it); // no need for it++ again
        }
      } else { // curr starts with ">"
        output_filename = curr.substr(1);
        commands.erase(it); // no need for it++ again
      }
    } else if (curr.compare(0, 2, "2>") == 0) {
      if (curr.length() == 2) { // curr == "2>"
        if (it + 1 == commands.end()) {
          std::cerr << "incorrect input format: 2< requires an output file" << std::endl;
          error = true;
          return;
        } else {
          error_filename = *(it + 1);
          commands.erase(it);
          commands.erase(it); // no need for it++ again
        }
      } else { // curr starts with "2>"
        if (curr.compare("2>&1") == 0) {
          error_filename = output_filename;
        } else {
          error_filename = curr.substr(2);
        }
        commands.erase(it); // no need for it++ again
      }
    } else {
      it++;
    }
  }
  if (!input_filename.empty()) {
    if (curr_command_index != 0) { // only the first piped command can redirect stdin
      std::cerr << "cannot redirect stdin for a non-head command in pipe" << std::endl;
      error = true;
      return;
    }
    close(0);
    open(input_filename.c_str(), O_RDONLY);
  }
  if (!output_filename.empty()) {
    if (curr_command_index != piped_commands.size() - 1) { // only the last piped command can redirect stdout
      std::cerr << "cannot redirect stdout for a non-end command in pipe" << std::endl;
      error = true;
      return;
    }
    close(1);
    open(output_filename.c_str(), O_WRONLY | O_CREAT, 0666); // set file permission
  }
  if (!error_filename.empty()) {
    close(2);
    open(error_filename.c_str(), O_WRONLY | O_CREAT, 0666);
  }
}

/**
 * in a child process
 * config its pipe input and output
 **/
void MyShell::configCommandPipe() {
  std::size_t num_commands = piped_commands.size();
  std::size_t num_pipes = piped_commands.size() - 1;
  if (curr_command_index != 0) { // not the first command
    // close fd 0 for stdin, and redirect pipe R end to fd 0
    if (dup2(pipefd[2 * (curr_command_index - 1)], 0) < 0) {
      std::cerr << "failed to redirect stdin: " << std::strerror(errno) << std::endl;
      error = true;
      return;
      // exit(EXIT_FAILURE);
    }
  }
  if (curr_command_index != num_commands - 1) { // not the last command
    // close fd 1 for stdout, and redirect pipe W end to fd 1
    if (dup2(pipefd[2 * curr_command_index + 1], 1) < 0) {
      std::cerr << "failed to redirect stdout: " << std::strerror(errno) << std::endl;
      error = true;
      return;
      // exit(EXIT_FAILURE);
    }
  }
  // close all pipe fds, only use the new 0 and 1 for read and write
  for (std::size_t i = 0; i < 2 * num_pipes; i++) {
    if (close(pipefd[i]) < 0) {
      std::cerr << "failed to close pipes: " << std::strerror(errno) << std::endl;
      error = true;
      return;
    }
  }
}

/**
 * run a normal command with the need to fork a child process
 * (commands except for exit, cd, set, and export)
 **/
void MyShell::runCommand() {
  pid_t forkResult = fork(); // fork a child process same as the parent one
  num_child_processes++;
  if (forkResult == -1) { // fork error, skip the command
    std::cerr << "failed to create a child process: " << std::strerror(errno) << std::endl;
    error = true;
  } else if (forkResult == 0) { // in the child process
    configCommandRedirect();
    configCommandPipe();
    char ** c_commands = vector2array(commands);
    // if the child calls execve right after forking, the second copy of the parent's memory is destroyed and replaced with a memory image loaded from the requested binary
    execve(c_commands[0], c_commands, environ);
    std::cerr << "execve failed: " << std::strerror(errno) << std::endl;
    _exit(EXIT_FAILURE); // if execve returns, the child process fails, and should use _exit to exit the forked child process
  }
}

/**
 * in the parent process
 * allocate space for MyShell::pipefd
 * create pipes for all piped commands
 **/
void MyShell::createPipes() {
  int num_pipes = piped_commands.size() - 1;
  pipefd = new int[2 * num_pipes]; // 2 * number of pipe marks
  for (int i = 0; i < num_pipes; i++) {
    if (pipe(pipefd + 2 * i) < 0) { // R end: 2 * i, W end: 2 * i + 1
      std::cerr << "failed to create pipes: " << std::strerror(errno) << std::endl;
      error = true;
      return;
    }
  }
}

/**
 * in the parent process
 * close all pipe fds before waiting for child processes
 **/
void MyShell::closePipes() {
  int num_pipes = piped_commands.size() - 1;
  for (int i = 0; i < 2 * num_pipes; i++) {
    if (close(pipefd[i]) < 0) {
      std::cerr << "failed to close pipes: " << std::strerror(errno) << std::endl;
      error = true;
      return;
    }
  }
}

/**
 * in the parent process
 * the parent needs to wait for all its forked child processes to finish (exit), and then exit itself
 * report the exit status of the last piped command, if that command has a exit status (needs fork)
 **/
void MyShell::waitForChildProcesses() {
  int childStatus;
  for (std::size_t i = 0; i < num_child_processes; i++) {
    wait(&childStatus);
    if (i == num_child_processes - 1) { // only print the exit status of the last "reportable" piped command
      if (WIFEXITED(childStatus)) { // the child process is terminated normally
        std::cout << "Program exited with status " << WEXITSTATUS(childStatus) << std::endl;
      } else if (WIFSIGNALED(childStatus)) { // the child process is terminated due to receipt of a signal
        std::cout << "Program was killed by signal " << WTERMSIG(childStatus) << std::endl;
      }
    }
  }
}

/**
 * run the piped commands in MyShell::piped_commands in order
 **/
void MyShell::runPipedCommands() {
  createPipes();
  for (curr_command_index = 0; curr_command_index < piped_commands.size(); curr_command_index++) {
    if (error) break; // if any error occur previously during the execution of this input, stop
    parseCommand();
    if (error) break; // if error occur during parsing comamnd, stop
    if (!commands.empty()) { // if the command is not empty
      std::string command_name = commands[0];
      if (command_name.compare("exit") == 0) {
  	    runExitCommands();
      } else if (command_name.compare("cd") == 0) {
        runCdCommand();
      } else if (command_name.compare("set") == 0) {
       runSetCommand();
      } else if (command_name.compare("export") == 0) {
        runExportCommand();
      } else {
      	if (searchCommand()) { // command exists
          runCommand();
        } else {
          std::cerr << "command " << commands[0] << " not found" << std::endl;
          error = true;
        }
      }
      commands.clear();
    }
  }
  closePipes();
  waitForChildProcesses();
  delete[] pipefd;
}

/**
 * set the error indicator to false
 * remove contents of input, piped_commands and commands
 * reset curr_command_index and num_child_processes to 0
 * update environment variables in case other programs change them
 **/
void MyShell::refresh() {
  error = false;
  input.clear();
  piped_commands.clear();
  commands.clear();
  curr_command_index = 0;
  num_child_processes = 0;
}

/****************************************/
/**********CLASS PUBLIC FUNCTIONS********/
/****************************************/
/**
 * default constructor of MyShell CLASS
 * initialize some class variables
 * set up env vars once
 **/
MyShell::MyShell(): error(false), exitting(false), curr_command_index(0), num_child_processes(0) {
  char ** envp = environ;
  while (*envp) {
    std::string curr_env(*envp);
    envp++;
    std::size_t equal_index = curr_env.find('=');
    setVar(curr_env.substr(0, equal_index), curr_env.substr(equal_index + 1));
  }
}

/**
 * execute one round of input command
 **/
void MyShell::execute() {
  refresh();
  std::cout << "myShell:" << vars["PWD"] << "$ ";
  std::getline(std::cin, input); // default delim is '\n' and will be discarded, great!
  if (std::cin.eof()) {
    runExitCommands();
    std::cin.clear();
    std::cout << std::endl;
  } else if (std::cin.fail() || std::cin.bad()) {
    std::cin.clear();
  } else { // std::cin is good, valid input
    parsePipedInput();
    if (error) return;
    runPipedCommands();
  }
}

/**
 * return to the caller if the shell is going to exit
 * i.e., in the previous step the user gives "exit" commands
 **/
bool MyShell::isExitting() {
  return exitting;
}
