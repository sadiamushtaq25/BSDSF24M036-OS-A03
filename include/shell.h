#ifndef SHELL_H
#define SHELL_H
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_TOKENS 256
#define MAX_JOBS 128

typedef struct { pid_t pid; char command[256]; } Job;
extern Job jobs[MAX_JOBS];
extern int job_count;
char *read_cmd(const char *prompt, FILE *fp);
int process_line(char *line);
int execute_tokens(char **tokens, int background);
int run_builtin(char **argv, int in_child);
void reap_jobs(void);
void print_jobs(void);
void expand_variables(char **argv);
#endif
