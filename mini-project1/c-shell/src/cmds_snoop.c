#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#include "parser.h"
#include "cmds_snoop.h"

void snoop_cmd(tknll *head) {
    if(head == NULL || head->next == NULL) {
        printf("cshell: snoop: missing command to trace\n");
        return;
    }

    int argc = 0;
    tknll *tmp = head->next;
    while(tmp != NULL && tmp->type == WORD) {
        argc++;
        tmp = tmp->next;
    }

    char **argv = malloc((argc + 1) * sizeof(char*));
    tmp = head->next;
    for(int i = 0; i < argc; i++) {
        argv[i] = tmp->tkn;
        tmp = tmp->next;
    }
    argv[argc] = NULL;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        free(argv);
        return;
    }

    if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0); 
        
        ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

        while (1) {
            ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
            waitpid(pid, &status, 0);
            
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                break;
            }

            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, NULL, &regs);
            
            printf("Syscall ID: %llu\n", (unsigned long long)regs.orig_rax);
            
            ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
            waitpid(pid, &status, 0);
        }
        free(argv);
    }
}