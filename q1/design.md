# Q1. Shell
## Run
```
make run
```

## Design
### Commands separated by single pipe |
Sample command: `ls -l | wc | cat >> out.txt`
![Commands separated by single pipe design](./q1_design_1.png?raw=true)

### Commands separated by double or triple pipes (|| or |||)
Sample command: `ls -l || wc, cat >> out.txt`

In this case, both the `wc` and `cat >> out.txt` commands should get an input as the output of `ls -l` command. Since reads in pipe are destructive, when `wc` command is run, we first buffer the data in the pipe (output of `ls`) into a char array. Then, we write that data back to two pipes, one for `wc` and one for `cat >> out.txt`. The figure below depicts the implementation.
Sample command: `ls -l | wc | cat >> out.txt`
![Commands separated by double/triple pipe design](./q1_design_2.png?raw=true)

## Features
- For each command, shell creates a new process group and gives foreground control to that group. Upon completion of the command, the foreground control is given back to the main process.
- Shell also supports background commands. In that case, the command is not given foreground control and continues running in the background.
- Shell supports input and output redirection operations
- Shell supports pipelining (|, ||, and |||)
- Shell supports adding, deleting and running a short-cut command. While insertion, if a short-cut command with the index already exists, the shell confirms whether to replace it. The index can be any integer.

## Example Commands
- Simple shell commands
  ```
    shell> ls
    
    shell> wc
  ```

- Piped commands
  ```
    shell> ls -l | wc
    
    shell> ls -l ||  grep ^-, grep ^d
    
    shell> ls -l ||| grep ^-, grep ^d, cat >> out.txt
  ```

- Background commands
  ```
    shell> ls -l | wc&
  ```

- Short-cut commands
  ```
    shell> sc -i 1 ls -l | wc
    
    shell> Ctrl+C
    Enter command index: 1
    
    shell> sc -d 1 ls -l | wc
  ```

- Quitting the shell
  ```
    shell> exit
  ```

## Assumptions
For simplicity, several assumptions were made -
- The maximum number of arguments in a command allowed is 20
- The size of each argument should not exceed 30
- Each identifier in a command should be separated by a space. eg: `ls | cat >> out.txt` is valid but `ls | cat>>out.txt` is not valid.
- It is assumed that the index of each shortcut command would be unique. Hence, while deleting a short-cut command, only the index is required. eg: both `sc -d 1` and `sc -d 1 ls | cat` are valid commands.

## [TO DO] Screenshots
