#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "parser.h"
#include "cmds_ping.h"
#include "jobs.h"

void ping_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->next==NULL || head->next->next->next!=NULL) {
        printf("ping: invalid syntax\n");
        return;
    }

    char *tgt_str=head->next->tkn;
    char *sig_str=head->next->next->tkn;
    
    for(int i=0;sig_str[i]!='\0';i++) {
        if(sig_str[i]<'0' || sig_str[i]>'9') {
            printf("ping: invalid syntax\n");
            return;
        }
    }

    int sig_num=atoi(sig_str)%64; 
    pid_t target_pid=-1;

    if(tgt_str[0]=='%') {
        int j_num=atoi(tgt_str+1);
        int found_idx=-1;
        for(int i=0;i<job_cnt;i++) {
            if(bg_jobs[i].job_num==j_num && bg_jobs[i].is_done==0) {
                found_idx=i;
                break;
            }
        }
        if(found_idx==-1) {
            printf("ping: no such process found\n");
            return;
        }
        target_pid=-bg_jobs[found_idx].pid; 
    } else {
        int raw_pid=atoi(tgt_str);
        int found=0;
        for(int i=0;i<job_cnt;i++) {
            if(bg_jobs[i].is_done==0) {
                for(int j=0;j<bg_jobs[i].num_pids;j++) {
                    if(bg_jobs[i].pids[j]==raw_pid) {
                        found=1;
                        break;
                    }
                }
            }
        }
        if(found==0) {
            printf("ping: no such process found\n");
            return;
        }
        target_pid=raw_pid;
    }

    if(kill(target_pid, sig_num)<0) {
        perror("cshell: ping");
    } else {
        printf("Sent signal %s to %s\n", sig_str, tgt_str);
    }
}