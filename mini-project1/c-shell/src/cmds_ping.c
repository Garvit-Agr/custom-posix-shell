#include <stdio.h>
#include <signal.h>
#include <limits.h>

#include "parser.h"
#include "cmds_ping.h"
#include "jobs.h"

int ping_parse_pid(const char *str, pid_t *value) {
    if(str==NULL || *str=='\0') return 0;

    unsigned long long num=0;
    for(int i=0;str[i]!='\0';i++) {
        if(str[i]<'0' || str[i]>'9') return 0;
        if(num>(unsigned long long)INT_MAX) return 0;
        num=num*10u+(unsigned long long)(str[i]-'0');
        if(num>(unsigned long long)INT_MAX) return 0;
    }

    if(num==0) return 0;
    *value=(pid_t)num;
    return 1;
}

int parse_signal(const char *str) {
    if(str==NULL || *str=='\0') return -1;

    int mod=0;
    for(int i=0;str[i]!='\0';i++) {
        if(str[i]<'0' || str[i]>'9') return -1;
        mod=(mod*10+(str[i]-'0'))%64;
    }
    return mod;
}

void ping_cmd(tknll *head) {
    if(head==NULL || head->next==NULL || head->next->type!=WORD ||
       head->next->next==NULL || head->next->next->type!=WORD ||
       head->next->next->next!=NULL) {
        printf("ping: invalid syntax\n");
        return;
    }

    char *tgt_str=head->next->tkn;
    char *sig_str=head->next->next->tkn;
    int sig_num=parse_signal(sig_str);
    if(sig_num<0) {
        printf("ping: invalid syntax\n");
        return;
    }

    pid_t target_pid=-1;

    if(tgt_str[0]=='%') {
        pid_t job_num;
        if(!ping_parse_pid(tgt_str+1, &job_num)) {
            printf("ping: no such process found\n");
            return;
        }

        int found_idx=-1;
        for(int i=0;i<job_cnt;i++) {
            if(bg_jobs[i].job_num==(int)job_num && bg_jobs[i].is_done==0) {
                found_idx=i;
                break;
            }
        }
        if(found_idx==-1) {
            printf("ping: no such process found\n");
            return;
        }
        target_pid=-bg_jobs[found_idx].pid;
    }
    else {
        pid_t raw_pid;
        if(!ping_parse_pid(tgt_str, &raw_pid)) {
            printf("ping: no such process found\n");
            return;
        }

        int found=0;
        for(int i=0;i<job_cnt && !found;i++) {
            if(bg_jobs[i].is_done!=0) continue;
            for(int j=0;j<bg_jobs[i].num_pids;j++) {
                if(bg_jobs[i].pids[j]==raw_pid) {
                    found=1;
                    break;
                }
            }
        }
        if(!found) {
            printf("ping: no such process found\n");
            return;
        }
        target_pid=raw_pid;
    }

    if(kill(target_pid, sig_num)<0) {
        printf("ping: no such process found\n");
    }
    else {
        printf("Sent signal %s to %s\n", sig_str, tgt_str);
    }
}
