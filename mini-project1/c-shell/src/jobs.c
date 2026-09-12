#include <string.h>

#include "jobs.h"

job bg_jobs[100];
int job_cnt=0;
int next_job_num=1;

void add_job(pid_t lead_pid, pid_t *all_pids, int num_pids, char names[][64], char *cmd) {
    if(job_cnt>=100) return;

    bg_jobs[job_cnt].job_num=next_job_num++;
    bg_jobs[job_cnt].pid=lead_pid;

    if(num_pids>MAX_JOB_PIDS) num_pids=MAX_JOB_PIDS;

    for(int i=0;i<num_pids;i++) bg_jobs[job_cnt].pids[i]=all_pids[i];
    bg_jobs[job_cnt].num_pids=num_pids;

    for(int i=0;i<num_pids;i++) {
        strncpy(bg_jobs[job_cnt].names[i], names[i], 63);
        bg_jobs[job_cnt].names[i][63]='\0';
    }

    strncpy(bg_jobs[job_cnt].cmd, cmd, 1023);
    bg_jobs[job_cnt].cmd[1023]='\0';
    bg_jobs[job_cnt].state=1;
    bg_jobs[job_cnt].is_done=0;
    bg_jobs[job_cnt].exit_status=0;
    job_cnt++;
}

void remove_job(pid_t pid) {
    for(int i=0;i<job_cnt;i++) {
        if(bg_jobs[i].pid==pid) {
            for(int j=i;j<job_cnt-1;j++) bg_jobs[j]=bg_jobs[j+1];
            job_cnt--;
            break;
        }
    }
}
