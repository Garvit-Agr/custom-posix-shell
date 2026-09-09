#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

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
    if (f == NULL) {
        perror("cshell: spy: could not read process stat (is /proc mounted?)");
        return;
    }
    
    char buf[1024];
    if (fgets(buf, sizeof(buf), f)==NULL) {
        fclose(f);
        return;
    }
    fclose(f);

    int pid;
    char comm[256];
    char state;
    int ppid, pgrp;
    
    sscanf(buf, "%d (%[^)]) %c %d %d", &pid, comm, &state, &ppid, &pgrp);

    char statm_path[256];
    snprintf(statm_path, sizeof(statm_path), "/proc/%d/statm", target_pid);
    FILE *fm = fopen(statm_path, "r");
    long vmem_pages = 0;

    if (fm != NULL) {
        fscanf(fm, "%ld", &vmem_pages);
        fclose(fm);
    }

    char fd_path[256];
    snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd", target_pid);
    DIR *d = opendir(fd_path);
    int fd_cnt = 0;
    
    if (d != NULL) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                fd_cnt++;
            }
        }
        closedir(d);
    }

    printf("pid: %d\n", pid);
    printf("Process Status: %c\n", state);
    printf("Process Group: %d\n", pgrp);
    printf("Virtual Memory: %ld bytes\n", vmem_pages*4096); 
    printf("Executable Name: %s\n", comm);
    printf("File Descriptors: %d\n", fd_cnt);
}