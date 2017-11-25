#include "myShell.h"
#include <cstdlib>
#include <string>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
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

void MyShell::setVar(std::string key, std::string value) {
  vars[key] = value;
}

/**
 * parse the one-line user input
 * input is delimitered by whitespace, unless the whitespace is escaped, whitespace will be discarded
 * the first delimited "word" in the input is the command
 * all other "words" are arguments to the command
 * the \ mark will be removed from the user input command string for the ease of future use
 * the return vector could be empty, if 1) the input is empty, 2) the input is invalid (\ terminated)
 *   input: std::string &
 *   return value: std::vector<std::string>
 **/
void MyShell::parseCommand() {
  std::string word;
  std::string modifiedCommand;
  for (std::size_t end = 0; end < command.length(); end++) {
    if (command[end] == ' ') {
      if ((end > 0 && command[end-1] != ' ') || (end > 1 && command[end-2] == '\\')) {
	        commands.push_back(std::string(word));
	        word.erase();
      }
      modifiedCommand.push_back(command[end]);
    } else if (command[end] == '\\') {
      if (end == command.length() - 1) {
	        std::cerr << "cannot use escape mark at the end of a line." << std::endl;
	        commands.clear();
	        return;
      }
      word.push_back(command[++end]); // put the escaped char into the word stream
      modifiedCommand.push_back(command[end]); // remove the \ from the input command
      if (end == command.length() - 1) {
	       commands.push_back(std::string(word));
      }
    } else { // normal non-space and non-escape chars
      word.push_back(command[end]);
      modifiedCommand.push_back(command[end]);
      if (end == command.length() - 1) {
	       commands.push_back(std::string(word));
      }
    }
  }
  command = modifiedCommand;
  evaluateVars();
}

/**
 * for each parsed command component (command itself or argument)
 * if it contains one or more variable (has $),
 * replace the variable name with the actual variable value
 * if the variable doesn't exist, print an error, and set the commands vector empty
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
            updated_curr.append(var);
          } else { // cannot find
            std::cerr << "cannot find variable " << var << std::endl;
            commands.clear();
            return;
          }
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
        updated_curr.append(var);
      } else { // cannot find
        std::cerr << "cannot find variable " << var << std::endl;
        commands.clear();
        return;
      }
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
 * in the two cases, if the file is found, return true. Else print the required text, and return false
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
  if (!exist) {
    std::cout << "Command " << command << " not found" << std::endl;
  }
  return exist;
}

void MyShell::runExitCommands() {
  exitting = true;
}

/**
 * cd command can only take 0 or 1 argument
 * 0 argument: cd to home directory
 * 1 argument: cd to the given argument if it exists
 **/
void MyShell::runCdCommand() {
  if (commands.size() > 2) {
    std::cerr << "too many arguments for cd" << std::endl;
    return;
  }
  std::string dest = getenv("HOME");
  if (commands.size() == 2) {
    dest = commands[1];
  }
  if(chdir(dest.c_str()) != 0) { // chdir fails, the current directory doesn't change
    std::cerr << "cannot change directory: " << std::strerror(errno) << std::endl;
  } else {
    char cwd[1024];
    setenv("OLDPWD", vars["PWD"].c_str(), 1);
    setVar("PWD", getcwd(cwd, sizeof(cwd))); // get absolute working path in the case
    setenv("PWD", vars["PWD"].c_str(), 1);
  }
}

void MyShell::runSetCommand() {
  if (commands.size() < 3) {
    std::cerr << "too few arguments for set: " << commands.size() << std::endl;
    for (std::vector<std::string>::iterator it = commands.begin(); it != commands.end(); it++) {
      std::cout << *it << std::endl;
    }
    return;
  }
  if (validateVarName(commands[1]) == false) {
    std::cerr << "invalid var name: var names can only contain letters (case sensitive), numbers and underscores" << std::endl;
    return;
  }
  std::size_t setPos = command.find(commands[0]); // the starting index of substr "set"
  std::size_t varPos = command.find(commands[1], setPos + commands[0].length()); // the start index of var name, with a valid var name, find won't have strange behavior
  std::size_t valPos = varPos + commands[1].length() + 1; // in this case, the starting index of value is one space after var name
  std::string value = command.substr(valPos);
  setVar(commands[1], value);
  std::cout << "set variable " << commands[1] << " with value " << vars[commands[1]] << std::endl;
}

void MyShell::runExportCommand() {
  for (std::vector<std::string>::iterator it = commands.begin() + 1; it != commands.end(); it++) {
    std::map<std::string, std::string>::iterator varsit = vars.find(*it);
    std::string value = "";
    if (varsit != vars.end()) {
      value = varsit->second;
    }
    if (setenv(it->c_str(), value.c_str(), 1) != 0) { // 1: always replace
      std::cerr << "failed to export variable " << *it << ": " << std::strerror(errno) << std::endl;
    } else {
      std::cout << "export variable " << *it << " with value " << value << std::endl;
    }
  }
}

void MyShell::runCommand(char ** c_commands) {
  pid_t forkResult = fork(); // fork a second process same as the current one
  if (forkResult == -1) {
    // fork error, exit the shell
    std::cerr << "failed to create a child process: " << std::strerror(errno) << std::endl;
  } else if (forkResult > 0) {
    // in the parent process
    int childStatus = 0;
    waitpid(forkResult, &childStatus, 0); // the parent process suspends its execution until the child process termitates, and get the terminating status back
    if (WIFEXITED(childStatus)) { // the child process is terminated normally
      std::cout << "Program exited with status " << WEXITSTATUS(childStatus) << std::endl;
    } else if (WIFSIGNALED(childStatus)) { // the child process is terminated due to receipt of a signal
      std::cout << "Program was killed by signal " << WTERMSIG(childStatus) << std::endl;
    }
  } else if (forkResult == 0) {
    // in the child process
    close(1); // close the file descriptor for stdout, and the effect is on both parent and child process
    open("temp", O_WRONLY);
    // if the child calls execve right after forking, the second copy of the parent's memory is destroyed and replaced with a memory image loaded from the requested binary
    execve(c_commands[0], c_commands, environ);
    std::cerr << "execve failed: " << std::strerror(errno) << std::endl;
    _exit(EXIT_FAILURE); // if execve returns, the child process fails, and should use _exit to exit the forked process
  }
}

/**
 * remove contents of command and commands
 * update environment variables in case other programs change them
 **/
void MyShell::refresh() {
  command.clear();
  commands.clear();
  char ** envp = environ;
  while (*envp) {
    std::string curr_env(*envp);
    envp++;
    std::size_t equal_index = curr_env.find('=');
    setVar(curr_env.substr(0, equal_index), curr_env.substr(equal_index + 1));
  }
}

/****************************************/
/**********CLASS PUBLIC FUNCTIONS********/
/****************************************/
MyShell::MyShell(): exitting(false) {
}

void MyShell::execute() {
  refresh();
  std::cout << "myShell:" << vars["PWD"] << "$ ";
  std::getline(std::cin, command); // default delim is '\n' and will be discarded, great!
  if (std::cin.eof()) {
    runExitCommands();
    std::cin.clear();
    std::cout << std::endl;
  } else if (std::cin.fail() || std::cin.bad()) {
    std::cin.clear();
  } else { // good
    parseCommand();
    // for (std::vector<std::string>::iterator it = commands.begin(); it != commands.end(); it++) {
      //std::cout << *it << std::endl;
      //}
    // no meaningful commands, do nothing
    if (!commands.empty()) {
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
      	// command doesn't exist, do nothing
      	if (searchCommand() == true) {
      	  char ** c_commands = vector2array(commands);
      	  runCommand(c_commands);
      	  for (std::size_t i = 0; i < commands.size(); i++) {
      	    delete[] c_commands[i];
      	  }
      	  delete[] c_commands;
      	}
      }
    }
  }
}

bool MyShell::isExitting() {
  return exitting;
}
