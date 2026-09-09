#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "cmds_spy.h"

void spy_cmd(tknll *head) {
    pid_t target_pid = getpid();
    
    if(head != NULL && head->next != NULL && head->next->type == WORD) {
        target_pid = atoi(head->next->tkn);
    }

    char stat_path[256];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", target_pid);

    FILE *f = fopen(stat_path, "r");
    
    char buf[1024];
    fgets(buf, sizeof(buf), f);
    fclose(f);

    int pid;
    char comm[256];
    char state;
    int ppid, pgrp;
    
    sscanf(buf, "%d (%[^)]) %c %d %d", &pid, comm, &state, &ppid, &pgrp);

    printf("pid: %d\n", pid);
    printf("Process Status: %c\n", state);
    printf("Process Group: %d\n", pgrp);
    printf("Executable Name: %s\n", comm);
}