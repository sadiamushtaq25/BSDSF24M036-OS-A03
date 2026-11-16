# FCIT Custom UNIX Shell (PA-03)

A modular C command-line interpreter developed for Operating Systems Programming Assignment 03.

## Build

```bash
sudo apt-get update
sudo apt-get install build-essential libreadline-dev
make
./bin/myshell
```

If GNU Readline development files are not installed, the Makefile builds a basic-input fallback; install `libreadline-dev` to enable Readline history, line editing and completion.

## Features

- External commands using `fork()`, `execvp()` and `waitpid()`
- Built-ins: `exit`, `cd`, `help`, `jobs`, `history`, `set`
- Last 20 command history entries and `!n` recall
- GNU Readline integration, arrow-key history and command/file completion when available
- Input/output redirection (`<`, `>`), pipelines (`|`)
- Sequential command chaining (`;`) and background jobs (`&`)
- Multiline `if` / `then` / optional `else` / `fi` based on command exit status
- Shell variables (`NAME=value`, `$NAME` expansion)

## Examples

```sh
pwd
cd /tmp
history
!2
MESSAGE="hello world"
echo $MESSAGE
ls -l > files.txt
sort < files.txt
cat /etc/passwd | grep root
echo first ; echo second
sleep 5 &
jobs
if true
then
echo success
else
echo failure
fi
```

## Layout

- `src/main.c`: input loop, history and conditional block collection
- `src/shell.c`: parsing, built-ins, variables, process execution, redirection and jobs
- `include/shell.h`: shared declarations
- `Makefile`: build configuration
