#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "parser.h"
#include "cmds_bg.h"
#include "jobs.h"

void bg_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->type!=WORD) {
        printf("cshell: bg: invalid arguments\n");
        return;
    }

    int j_num=atoi(head->next->tkn);
    int found=0;
    pid_t target_pid=-1;

    for(int i=0; i<job_cnt; i++) {
        if(bg_jobs[i].job_num==j_num && bg_jobs[i].is_done==0) {
            target_pid=bg_jobs[i].pid;
            found=1;
            break;
        }
    }

    if(found==0) {
        printf("cshell: bg: job not found\n");
        return;
    }

    if(kill(-target_pid, SIGCONT)<0) {
        perror("kill");
    }
}