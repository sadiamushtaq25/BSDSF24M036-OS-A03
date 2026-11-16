#include "shell.h"
#include <ctype.h>

extern char *variables[128][2];
extern int variable_count;
extern unsigned long history_total;
extern char *history_items[HISTORY_SIZE];

static char *next_token(const char **p) {
    while (**p==' ' || **p=='\t') (*p)++;
    if (!**p) return NULL;
    if (**p=='>' || **p=='<' || **p=='|' || **p==';' || **p=='&') { char *s=strndup(*p,1); (*p)++; return s; }
    char buf[4096]; size_t n=0; char quote=0;
    while (**p) {
        char c=**p;
        if (quote) { if (c==quote) { quote=0; (*p)++; } else if (c=='\\' && (*p)[1]) { (*p)++; buf[n++]=*(*p)++; } else { buf[n++]=c; (*p)++; } }
        else { if (c=='\'' || c=='"') { quote=c; (*p)++; } else if (c==' ' || c=='\t' || c=='>' || c=='<' || c=='|' || c==';' || c=='&') break; else { buf[n++]=c; (*p)++; } }
        if (n>=sizeof(buf)-1) break;
    }
    buf[n]='\0'; return strdup(buf);
}
static char **tokenize_line(const char *line) {
    char **v=calloc(MAX_TOKENS,sizeof(char*)); int n=0; char *t;
    while ((t=next_token(&line))!=NULL && n<MAX_TOKENS-1) v[n++]=t;
    v[n]=NULL; return v;
}
void expand_variables(char **argv) {
    for (int i=0;argv && argv[i];i++) if (argv[i][0]=='$') {
        const char *key=argv[i]+1, *value="";
        for (int j=0;j<variable_count;j++) if (!strcmp(variables[j][0],key)) { value=variables[j][1]; break; }
        free(argv[i]); argv[i]=strdup(value);
    }
}
static int set_variable(const char *s) {
    const char *eq=strchr(s,'='); if (!eq || eq==s) return 0;
    char *key=strndup(s,(size_t)(eq-s)); const char *val=eq+1; int i;
    for (i=0;i<variable_count;i++) if (!strcmp(variables[i][0],key)) break;
    if (i==128) { fprintf(stderr,"myshell: variable table full\n"); free(key); return 1; }
    if (i==variable_count) variables[variable_count++][0]=key; else { free(key); }
    free(variables[i][1]); variables[i][1]=strdup(val); return 1;
}
void reap_jobs(void) {
    int status; pid_t p;
    while ((p=waitpid(-1,&status,WNOHANG))>0) {
        for (int i=0;i<job_count;i++) if (jobs[i].pid==p) { printf("[done] %d %s\n",p,jobs[i].command); jobs[i]=jobs[--job_count]; break; }
    }
}
void print_jobs(void) { for (int i=0;i<job_count;i++) printf("[%d] %d Running %s\n",i+1,jobs[i].pid,jobs[i].command); }
int run_builtin(char **a, int in_child) {
    if (!a || !a[0]) return 0;
    if (!strcmp(a[0],"exit")) { if (in_child) _exit(0); exit(0); }
    if (!strcmp(a[0],"cd")) { const char *dir=a[1]?a[1]:getenv("HOME"); if (chdir(dir)<0) perror("cd"); return 1; }
    if (!strcmp(a[0],"help")) { puts("Built-ins: exit cd help jobs history set"); return 1; }
    if (!strcmp(a[0],"jobs")) { print_jobs(); return 1; }
    if (!strcmp(a[0],"history")) { extern unsigned long history_total; extern char *history_items[HISTORY_SIZE]; int count=(history_total<HISTORY_SIZE)?(int)history_total:HISTORY_SIZE; unsigned long start=history_total-(unsigned long)count; for(int i=0;i<count;i++) printf("%lu  %s\n",start+(unsigned long)i+1,history_items[(start+(unsigned long)i)%HISTORY_SIZE]); return 1; }
    if (!strcmp(a[0],"set")) { for(int i=0;i<variable_count;i++) printf("%s=%s\n",variables[i][0],variables[i][1]); return 1; }
    if (strchr(a[0],'=') && a[0][0]!='$') { set_variable(a[0]); return 1; }
    return 0;
}
static int run_one(char **args, int background, const char *display) {
    if (!args[0]) return 0;
    if (!background && run_builtin(args,0)) return 0;
    pid_t pid=fork();
    if (pid<0) { perror("fork"); return 1; }
    if (pid==0) { if (run_builtin(args,1)) _exit(0); execvp(args[0],args); perror(args[0]); _exit(127); }
    if (background) { if (job_count<MAX_JOBS) { jobs[job_count].pid=pid; snprintf(jobs[job_count].command,sizeof(jobs[job_count].command),"%s",display?display:args[0]); job_count++; } printf("[background] %d\n",pid); return 0; }
    int status; while (waitpid(pid,&status,0)<0) if(errno!=EINTR) { perror("waitpid"); return 1; }
    return WIFEXITED(status)?WEXITSTATUS(status):1;
}
static int execute_pipeline(char **tok, int start, int end, int background, const char *display) {
    int pipes=0; for(int i=start;i<end;i++) if(!strcmp(tok[i],"|")) pipes++;
    int segments=pipes+1; int prev=-1, status=0; pid_t pids[64]; int pc=0; int pos=start;
    for(int seg=0;seg<segments;seg++) {
        int stop=pos; while(stop<end && strcmp(tok[stop],"|")) stop++;
        char *args[MAX_TOKENS]; int ac=0; char *infile=NULL,*outfile=NULL;
        for(int i=pos;i<stop;i++) { if(!strcmp(tok[i],"<") && i+1<stop) infile=tok[++i]; else if(!strcmp(tok[i],">") && i+1<stop) outfile=tok[++i]; else args[ac++]=tok[i]; }
        args[ac]=NULL;
        int fd[2]={-1,-1}; if(seg<segments-1 && pipe(fd)<0) { perror("pipe"); return 1; }
        pid_t pid=fork(); if(pid<0){perror("fork");return 1;}
        if(pid==0) {
            if(prev!=-1){dup2(prev,STDIN_FILENO);close(prev);} if(fd[1]!=-1){dup2(fd[1],STDOUT_FILENO);close(fd[1]);close(fd[0]);}
            if(infile){int f=open(infile,O_RDONLY);if(f<0){perror(infile);_exit(1);}dup2(f,STDIN_FILENO);close(f);}
            if(outfile){int f=open(outfile,O_WRONLY|O_CREAT|O_TRUNC,0644);if(f<0){perror(outfile);_exit(1);}dup2(f,STDOUT_FILENO);close(f);}
            if (run_builtin(args, 1)) _exit(0);
            execvp(args[0], args);
            perror(args[0]);
            _exit(127);
        }
        pids[pc++]=pid; if(prev!=-1)close(prev); if(fd[1]!=-1)close(fd[1]); prev=fd[0]; pos=stop+1;
    }
    if(prev!=-1)close(prev);
    if(background){ for(int i=0;i<pc;i++) if(job_count<MAX_JOBS){jobs[job_count].pid=pids[i];snprintf(jobs[job_count].command,sizeof(jobs[job_count].command),"%s",display);job_count++;} printf("[background] %d\n",pids[0]); return 0; }
    for(int i=0;i<pc;i++){int st;while(waitpid(pids[i],&st,0)<0&&errno==EINTR){} if(i==pc-1)status=WIFEXITED(st)?WEXITSTATUS(st):1;} return status;
}
int process_line(char *line) {
    char **tok=tokenize_line(line); int n=0; while(tok[n]) n++; if(!n){free(tok);return 0;}
    int start=0,status=0;
    for(int i=0;i<=n;i++) if(i==n || !strcmp(tok[i],";")) {
        if(i>start) { int bg=0; if(!strcmp(tok[i-1],"&")){bg=1;free(tok[i-1]);tok[i-1]=NULL;i--;}
            char *part[MAX_TOKENS];int k=0;for(int j=start;j<i;j++)part[k++]=tok[j];part[k]=NULL; expand_variables(part);
            int haspipe=0;for(int j=0;j<k;j++)if(!strcmp(part[j],"|"))haspipe=1;
            if(haspipe || memchr(line,'<',strlen(line)) || memchr(line,'>',strlen(line))) status=execute_pipeline(part,0,k,bg,line);
            else status=run_one(part,bg,line);
        }
        start=i+1;
    }
    for (int i = 0; i < n; i++) free(tok[i]);
    free(tok);
    return status;
}
