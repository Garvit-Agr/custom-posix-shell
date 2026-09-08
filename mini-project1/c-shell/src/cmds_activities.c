#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "cmds_activities.h"
#include "jobs.h"

void activities(tknll *head) {
    if(head==NULL) return;

    for(int i=0;i<job_cnt;i++) {
        printf("[%d] pgid %d\n", bg_jobs[i].job_num, bg_jobs[i].pid);
        printf("  %d  %s  Running\n", bg_jobs[i].pid, bg_jobs[i].cmd);
    }
}