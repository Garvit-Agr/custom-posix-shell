#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "parser.h"
#include "cmds_snoop.h"

#define MAX_SYSCALLS 512

typedef struct {
    unsigned long long id;
    int count;
    double total_time;
    int first_occurrence;
} syscall_stat;

const char* get_syscall_name(unsigned long long id) {
    switch(id) {
        case 0: return "read";
        case 1: return "write";
        case 2: return "open";
        case 3: return "close";
        case 4: return "stat";
        case 5: return "fstat";
        case 8: return "lseek";
        case 9: return "mmap";
        case 10: return "mprotect";
        case 11: return "munmap";
        case 12: return "brk";
        case 13: return "rt_sigaction";
        case 14: return "rt_sigprocmask";
        case 16: return "ioctl";
        case 35: return "nanosleep";
        case 39: return "getpid";
        case 59: return "execve";
        case 60: return "exit";
        case 231: return "exit_group";
        default: return NULL; 
    }
}

int compare_syscalls(const void *a, const void *b) {
    syscall_stat *statA=(syscall_stat *)a;
    syscall_stat *statB=(syscall_stat *)b;

    if(statB->count!=statA->count) return statB->count-statA->count;
    return statA->first_occurrence-statB->first_occurrence;
}

void snoop_cmd(tknll *head) {
    if(head==NULL || head->next==NULL) {
        printf("snoop: invalid syntax\n");
        return;
    }

    int is_attach=0;
    pid_t attach_pid=-1;

    if(head->next->type!=WORD) {
        printf("snoop: invalid syntax\n");
        return;
    }

    if(strcmp(head->next->tkn, "-p")==0) {
        if(head->next->next==NULL || head->next->next->type!=WORD || head->next->next->next!=NULL) {
            printf("snoop: invalid syntax\n");
            return;
        }

        char *pid_str=head->next->next->tkn;
        if(*pid_str=='\0') {
            printf("snoop: invalid syntax\n");
            return;
        }
        long value=0;
        for(int i=0;pid_str[i]!='\0';i++) {
            if(pid_str[i]<'0' || pid_str[i]>'9' || value>(2147483647-(pid_str[i]-'0'))/10) {
                printf("snoop: invalid syntax\n");
                return;
            }
            value=value*10+(pid_str[i]-'0');
        }
        if(value<=0) {
            printf("snoop: no such process\n");
            return;
        }

        is_attach=1;
        attach_pid=(pid_t)value;
    }

    pid_t pid;
    char **argv=NULL;
    int exec_failed=0;

    if(is_attach) {
        pid=attach_pid;
        if(ptrace(PTRACE_ATTACH, pid, NULL, NULL)<0) {
            printf("snoop: no such process\n");
            return;
        }

        int status=0;
        if(waitpid(pid, &status, 0)<0) {
            printf("snoop: no such process\n");
            return;
        }
    }
    else {
        int argc=0;
        tknll *tmp=head->next;
        while(tmp!=NULL && tmp->type==WORD) {
            argc++;
            tmp=tmp->next;
        }
        if(argc==0 || tmp!=NULL) {
            printf("snoop: invalid syntax\n");
            return;
        }

        argv=malloc((argc+1)*sizeof(char *));
        if(argv==NULL) return;
        tmp=head->next;
        for(int i=0;i<argc;i++) {
            argv[i]=tmp->tkn;
            tmp=tmp->next;
        }
        argv[argc]=NULL;

        int err_pipe[2];
        if(pipe(err_pipe)==-1) {
            free(argv);
            return;
        }
        int flags=fcntl(err_pipe[1], F_GETFD);
        if(flags!=-1) fcntl(err_pipe[1], F_SETFD, flags|FD_CLOEXEC);

        pid=fork();
        if(pid==-1) {
            close(err_pipe[0]);
            close(err_pipe[1]);
            free(argv);
            return;
        }

        if(pid==0) {
            close(err_pipe[0]);
            if(ptrace(PTRACE_TRACEME, 0, NULL, NULL)<0) _exit(1);
            execvp(argv[0], argv);
            char failed=1;
            write(err_pipe[1], &failed, 1);
            close(err_pipe[1]);
            _exit(1);
        }

        close(err_pipe[1]);

        int status=0;
        if(waitpid(pid, &status, 0)<0) {
            close(err_pipe[0]);
            free(argv);
            return;
        }

        if(WIFEXITED(status) || WIFSIGNALED(status)) {
            char failed=0;
            ssize_t n=read(err_pipe[0], &failed, 1);
            if(n>0 && failed!=0) exec_failed=1;
            close(err_pipe[0]);
            if(exec_failed!=0) {
                printf("snoop: command not found\n");
                free(argv);
                return;
            }
            free(argv);
            return;
        }

        /* The initial stop is the exec/trace stop. */
        close(err_pipe[0]);
    }

    if(ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD)<0) {
        if(!is_attach && argv!=NULL) free(argv);
        return;
    }

    syscall_stat stats[MAX_SYSCALLS];
    memset(stats, 0, sizeof(stats));
    for(int i=0;i<MAX_SYSCALLS;i++) {
        stats[i].id=i;
        stats[i].first_occurrence=-1;
    }

    int in_syscall=0;
    unsigned long long curr_syscall=0;
    struct timespec start_time, end_time;
    int occurrence_counter=0;
    int status=0;

    while(1) {
        if(ptrace(PTRACE_SYSCALL, pid, NULL, NULL)<0) break;
        if(waitpid(pid, &status, 0)<0) break;

        if(WIFEXITED(status) || WIFSIGNALED(status)) break;
        if(!WIFSTOPPED(status)) continue;

        struct user_regs_struct regs;
        if(ptrace(PTRACE_GETREGS, pid, NULL, &regs)<0) break;

        if(!in_syscall) {
            curr_syscall=regs.orig_rax;
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            in_syscall=1;
        }
        else {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double elapsed=(end_time.tv_sec-start_time.tv_sec)+((end_time.tv_nsec-start_time.tv_nsec)/1e9);

            if(curr_syscall<MAX_SYSCALLS) {
                if(stats[curr_syscall].count==0) stats[curr_syscall].first_occurrence=occurrence_counter++;
                stats[curr_syscall].count++;
                stats[curr_syscall].total_time+=elapsed;
            }
            in_syscall=0;
        }
    }

    if(!is_attach && argv!=NULL) free(argv);

    qsort(stats, MAX_SYSCALLS, sizeof(syscall_stat), compare_syscalls);

    printf("%-14s%-8s%s\n", "syscall", "calls", "time");
    for(int i=0;i<MAX_SYSCALLS;i++) {
        if(stats[i].count>0) {
            const char* name=get_syscall_name(stats[i].id);
            if(name!=NULL) printf("%-14s%-8d%.3fs\n", name, stats[i].count, stats[i].total_time);
            else {
                char unknown_name[32];
                snprintf(unknown_name, sizeof(unknown_name), "syscall_%llu", stats[i].id);
                printf("%-14s%-8d%.3fs\n", unknown_name, stats[i].count, stats[i].total_time);
            }
        }
    }
}
