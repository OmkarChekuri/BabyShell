## Duke ECE 551 2017 Fall Mini-Project: Command Shell

<img src="shell_icon.png" alt="icon" style="width: 50px;"/>

### For Users

```
$ git clone git@github.com:YilinGao/BabyShell.git
$ cd BabyShell
$ ./myShell
```

Then the shell program is running in your shell, and you can type its supported commands and play with it.

### For Developers

If you want to make changes to the program:

```
$ git clone git@github.com:YilinGao/BabyShell.git
$ cd BabyShell
$ emacs myShell.h
$ emacs myShell.cpp
$ emacs main.cpp
$ make
$ ./myShell
```
The program requires to be compiled with C++ 11.

### Shell Function Requirement

For this mini-project you will write a "baby" command shell.

1. **Read a command name, run it**

You should write a program called "myShell".

Whenever your command shell expects input, it should print a prompt (`myShell $`). It should read one line of input (from stdin) which is the name of a command. For this step, you may assume that the user will type the full path to the command they want to run.

Your shell should then run the specified command---see `fork`, `execve`, and `waitpid`. Note that there is a library function `system` which combines all three of these, which may be easier for now, but will NOT work for later steps.

After the specified program exits, your shell should print:

  `Program exited with status 0 [replace 0 with the actual exit status]`

  OR

  `Program was killed by signal 11 [replace 11 with the actual signal]`

See `man waitpid`, and the `WIFSIGNALED(status)` macro.

Then your shell should print the `myShell$` prompt again, and repeat the process.

If the user types the command `exit`, or `EOF` is encountered reading from stdin, then the shell should exit.

2. **Improving commands**

For this steps, you will improve your handling of commands in two ways:
  * Search the PATH environment variable for commands

  You will want to use the PATH environment variable that exists when your shell starts (see `man getenv`), which will be a colon delimited list of paths.  When the user types a command name which does nothave a "/" in it, you should search each directory specified in the `PATH` (in order) for the specified program. If the program is found, your shell should execute it.  If not, your shell should print:

  `Command commandName not found [replace commandName with the actual command name]`.

  If the path name does contain a "/" in it, you should only look in the specified directory (which may not be on the `PATH` at all).  Note that a path with a "/" in it could be relative (.e.g, ./myProgram) or absolute (e.g. /bin/ls).

  * Commands can take arguments (separated by white spaces)

  You should also make commands such that they can take arguments separated by white space. For example, `./myProgram a b 23` runs myProgram with arguments `a`, `b` and `23`. here may be an arbitrary amount of whitespace in between arguments, so `./myProgram         a               b     23` has the same behavior as the previous example. However, any white space which is escaped with a "\" should be literally included in the argument, and not used as a separator: `./myProgram  a\ b c\ \ d` should run myProgram with two arguments "a b" and "c  d". Note that `./myProgram a\  b c \ d`  would have arguments "a " "b" "c" and " d", as the non-escaped spaces separate arguments.


3. **Directories and Variables**
  * Add the `cd` command to change the current directory. See the `chdir` function.
  * The prompt should show the current directory before the $. That is, if your current directory is `/home/drew`, the prompt should be `myShell:/home/drew $`.
  * Your shell should provide access to variables. A variable name must be a combination of letters (case sensitive), underscores, and numbers, (e.g., PATH, xyz, abc42, my_var).
  * If the user writes $varname on the command line, your shell should replace it with the current value of that variable.
  * You should provide two built in commands:
    * `set var value`

    This should set the variable var to the string on the rest of the command line [even if it contains spaces, etc]. Your shell should remember this value, and make use of it in future $ evaluations, however, it should not be placed in the environment for other programs.
    * `export var`

    This should put the current value of var into the environment for other programs.

  Note that you may find the `env` command useful for testing this: if you `set` a variable (but don't `export` it) the new value should not show up in `env`, while if you `export` it, the new value should show up in `env`.

  Note also that if the user changes `PATH`, it should affect where your shell searches for programs.

4. **Pipes and redirection**
  * Implement input redirection (<) and output redirection (>)

     `< filename`   redirects standard input for the command

     `> filename`   redirects standard output

     `2> filename`  redirects standard error

   Note that you will need to implement these between the `fork()` and `execve()` calls.  You will need to make use of `close()` on the relevant file descriptors (0 = stdin, 1 = stdout, 2 = stderr) use `open()` to open the appropriate file. You may also need to make use of `dup2()`.

  * Implement pipes (|)

  You should be able to run one command and pipe its output to another's input: `./myProgram | ./anotherProgram`. See the `pipe()` system call.

  Note that you need to be able to mix and match these in ways that make sense, along with having command line arguments: `./myProgram a b c < anInputFile | ./anotherProgram 23 45 > someOutputFile`.

  NOTE: It is very easy to write a broken implementation of pipes which LOOKS like it works on short input (< 65536 bytes), but deadlocks on longer input. Your implementation must work correctly when sending large amounts of data through the pipe for full credit.

  Using `fcntl` with `F_SETPIPE_SZ` to adjust the kernel buffer size to "larger than you think we will test with" is NOT a valid approach. If I see you trying to do this, you will get no credit for part 4.
