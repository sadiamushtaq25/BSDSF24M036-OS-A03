#include "shell.h"
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
static int completion_command_mode;
static char *completion_generator(const char *text, int state) {
    static int index, len; static char **items; static int count;
    if (!state) { index=0; len=(int)strlen(text); if (items) { for (int z=0; z<count; z++) free(items[z]); free(items); } count=0; items=NULL;
        if (completion_command_mode) {
            const char *path=getenv("PATH"); char *copy=path?strdup(path):strdup(""); char *save=NULL;
            for(char *d=strtok_r(copy,":",&save); d; d=strtok_r(NULL,":",&save)){ DIR *dir=opendir(*d?d:"."); if(!dir)continue; struct dirent *e; while((e=readdir(dir))) if(!strncmp(e->d_name,text,(size_t)len)){ char full[PATH_MAX]; snprintf(full,sizeof(full),"%s/%s",*d?d:".",e->d_name); if(access(full,X_OK)==0){items=realloc(items,(size_t)(count+1)*sizeof(char*));items[count++]=strdup(e->d_name);} } closedir(dir);} free(copy);
        } else { DIR *dir=opendir("."); if(dir){struct dirent *e;while((e=readdir(dir)))if(!strncmp(e->d_name,text,(size_t)len)){items=realloc(items,(size_t)(count+1)*sizeof(char*));items[count++]=strdup(e->d_name);}closedir(dir);} }
    }
    while(index<count){char *v=items[index++]; if(!strncmp(v,text,(size_t)len))return strdup(v);} return NULL;
}
static char **shell_completion(const char *text,int start,int end){(void)end;completion_command_mode=(start==0);rl_attempted_completion_over=1;return rl_completion_matches(text,completion_generator);}
#endif

char *history_items[HISTORY_SIZE];
unsigned long history_total = 0;
int history_count = 0;
char *variables[128][2];
int variable_count = 0;
Job jobs[MAX_JOBS];
int job_count = 0;

char *read_cmd(const char *prompt, FILE *fp) {
#ifdef HAVE_READLINE
    (void)fp;
    rl_attempted_completion_function = shell_completion;
    return readline(prompt);
#else
    printf("%s", prompt); fflush(stdout);
    size_t cap = 512, len = 0; char *line = malloc(cap); if (!line) return NULL;
    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n') { if (len + 2 > cap) { cap *= 2; char *p = realloc(line, cap); if (!p) { free(line); return NULL; } line = p; } line[len++] = (char)c; }
    if (c == EOF && len == 0) { free(line); return NULL; }
    line[len] = '\0'; return line;
#endif
}

static void remember_command(const char *s) {
    if (!s || !*s) return;
    free(history_items[history_total % HISTORY_SIZE]);
    history_items[history_total % HISTORY_SIZE] = strdup(s);
    history_total++; if (history_count < HISTORY_SIZE) history_count++;
#ifdef HAVE_READLINE
    add_history(s);
#endif
}
static const char *history_number(unsigned long n) {
    if (n == 0 || n > history_total) return NULL;
    unsigned long oldest = history_total - (unsigned long)history_count + 1;
    if (n < oldest) return NULL;
    return history_items[(n - 1) % HISTORY_SIZE];
}
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    size_t n = strlen(s); while (n && (s[n-1]==' ' || s[n-1]=='\t' || s[n-1]=='\n')) s[--n]='\0';
    return s;
}
static int execute_script_line(char *s) { return process_line(s); }
static int run_if_block(char *first) {
    char *condition = trim(first + 2);
    char *then_lines[128], *else_lines[128]; int nt=0, ne=0, in_else=0;
    char *line;
    while ((line = read_cmd("... ", stdin)) != NULL) {
        char *t = trim(line);
        if (!strcmp(t,"fi")) { free(line); break; }
        if (!strcmp(t,"then")) { free(line); continue; }
        if (!strcmp(t,"else")) { in_else=1; free(line); continue; }
        if (in_else) { if (ne<128) else_lines[ne++]=strdup(t); }
        else { if (nt<128) then_lines[nt++]=strdup(t); }
        free(line);
    }
    int status = execute_script_line(condition);
    char **chosen = status == 0 ? then_lines : else_lines;
    int count = status == 0 ? nt : ne;
    for (int i=0;i<count;i++) { execute_script_line(chosen[i]); free(chosen[i]); }
    if (status == 0) for (int i=0;i<ne;i++) free(else_lines[i]);
    else for (int i=0;i<nt;i++) free(then_lines[i]);
    return status;
}

int main(void) {
    while (1) {
        reap_jobs();
        char *line = read_cmd(PROMPT, stdin);
        if (!line) break;
        char *t = trim(line);
        if (*t == '!') {
            char *end=NULL; unsigned long n=strtoul(t+1,&end,10);
            const char *old = (end && *end=='\0') ? history_number(n) : NULL;
            if (!old) { fprintf(stderr,"myshell: history number out of range\n"); free(line); continue; }
            char *replacement=strdup(old); printf("%s\n",replacement); free(line); line=replacement; t=line;
        }
        if (!*t) { free(line); continue; }
        remember_command(t);
        if (!strncmp(t,"if ",3)) run_if_block(t);
        else execute_script_line(t);
        free(line);
    }
    for (int i=0;i<HISTORY_SIZE;i++) free(history_items[i]);
    for (int i=0;i<variable_count;i++) { free(variables[i][0]); free(variables[i][1]); }
    printf("\nShell exited.\n"); return 0;
}
