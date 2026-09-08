#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "cmds_activities.h"
#include "jobs.h"

void activities(tknll *head) {
    if(head==NULL) return;

    for(int i=0;i<job_cnt;i++) {
        if(bg_jobs[i].is_done!=0) continue;
        
        int grp_prnt=0;

        for(int j=0;j<bg_jobs[i].num_pids;j++) {
            pid_t cur_p=bg_jobs[i].pids[j];
            if(cur_p<=0) continue;

            char st_pth[256];
            snprintf(st_pth, sizeof(st_pth), "/proc/%d/stat", cur_p);
            
            FILE *f=fopen(st_pth, "r");
            if(f==NULL) continue;

            char buf[1024];
            if(fgets(buf, sizeof(buf), f)==NULL) {
                fclose(f);
                continue;
            }
            fclose(f);

            char st_chr='R';
            char *opn_par=strchr(buf, '(');
            char *cls_par=strrchr(buf, ')');
            
            if(opn_par!=NULL && cls_par!=NULL) {
                st_chr=*(cls_par+2);
            }

            if(st_chr=='Z' || st_chr=='X') continue;

            if(grp_prnt==0) {
                printf("[%d] pgid %d\n", bg_jobs[i].job_num, bg_jobs[i].pid);
                grp_prnt=1;
            }
            
            char *st_str="Running";
            if(st_chr=='T') st_str="Stopped";

            printf("  %d  %s  %s\n", cur_p, bg_jobs[i].cmd, st_str);
        }
    }
}