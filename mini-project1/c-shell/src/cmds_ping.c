#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "parser.h"
#include "cmds_ping.h"
#include "jobs.h"

void ping_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->next==NULL) {
        printf("cshell: ping: invalid arguments\n");
        return;
    }

    int j_num=atoi(head->next->tkn);
    int sig_num=atoi(head->next->next->tkn);

    int found_idx=-1;

    for(int i=0; i<job_cnt; i++) {
        if(bg_jobs[i].job_num==j_num && bg_jobs[i].is_done==0) {
            found_idx=i;
            break;
        }
    }

    if(found_idx==-1) {
        printf("cshell: ping: no such job found\n");
        return;
    }

    pid_t target_pid = bg_jobs[found_idx].pid;

    if(kill(target_pid, sig_num) < 0) {
        perror("cshell: ping");
    } else {
        printf("Sent signal %d to process %d\n", sig_num, target_pid);
    }
}