#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "parser.h"
#include "cmds_fg.h"
#include "jobs.h"

void fg_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->type!=WORD) {
        printf("cshell: fg: invalid arguments\n");
        return;
    }

    int j_num=atoi(head->next->tkn);
    int found_idx=-1;

    for(int i=0; i<job_cnt; i++) {
        if(bg_jobs[i].job_num==j_num && bg_jobs[i].is_done==0) {
            found_idx=i;
            break;
        }
    }

    if(found_idx==-1) {
        printf("cshell: fg: job not found\n");
        return;
    }

    job fg_job=bg_jobs[found_idx];
    remove_job(fg_job.pid);

    printf("%s\n", fg_job.cmd);

    tcsetpgrp(STDIN_FILENO, fg_job.pid);

    if(kill(-fg_job.pid, SIGCONT)<0) {
        perror("kill");
    }

    int stopped=0;
    int status;
    
    waitpid(fg_job.pid, &status, WUNTRACED);
    if(WIFSTOPPED(status)) stopped=1;

    tcsetpgrp(STDIN_FILENO, getpid());

    if(stopped) {
        add_job(fg_job.pid, fg_job.pids, fg_job.num_pids, fg_job.cmd);
        printf("\n[%d] %d\n", next_job_num-1, fg_job.pid);
    }
}