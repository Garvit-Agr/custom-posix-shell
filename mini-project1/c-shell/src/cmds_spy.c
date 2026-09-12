#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <limits.h>

#include "parser.h"
#include "cmds_spy.h"

int spy_parse_pid(char *str, pid_t *value) {
    if(str==NULL || *str=='\0') return 0;

    long num=0;
    for(int i=0;str[i]!='\0';i++) {
        if(str[i]<'0' || str[i]>'9') return 0;
        if(num>(LONG_MAX-(str[i]-'0'))/10) return 0;
        num=num*10+(str[i]-'0');
    }

    if(num<=0 || num>INT_MAX) return 0;
    *value=(pid_t)num;
    return 1;
}

void spy_cmd(tknll *head) {
    if(head==NULL) return;

    if(head->next!=NULL && head->next->type!=WORD &&
       head->next->type!=OP_LT && head->next->type!=OP_GT && head->next->type!=OP_GTGT) return;
    if(head->next!=NULL && head->next->type==WORD &&
       head->next->next!=NULL && head->next->next->type==WORD) {
        printf("spy: invalid syntax\n");
        return;
    }

    pid_t target_pid=getpid();
    if(head->next!=NULL) {
        if(!spy_parse_pid(head->next->tkn, &target_pid)) {
            printf("spy: no such process\n");
            return;
        }
    }

    char proc_dir[256];
    snprintf(proc_dir, sizeof(proc_dir), "/proc/%d", target_pid);
    DIR *check_dir=opendir(proc_dir);
    if(check_dir==NULL) {
        printf("spy: no such process\n");
        return;
    }
    closedir(check_dir);

    printf("%-6s %-6s %-6s %s\n", "PID", "FD", "TYPE", "PATH");

    char link_path[4096];
    char target_path[PATH_MAX+5];
    ssize_t len;

    snprintf(link_path, sizeof(link_path), "/proc/%d/cwd", target_pid);
    len=readlink(link_path, target_path, sizeof(target_path)-1);
    if(len!=-1) {
        target_path[len]='\0';
        printf("%-6d %-6s %-6s %s\n", target_pid, "cwd", "DIR", target_path);
    }

    snprintf(link_path, sizeof(link_path), "/proc/%d/exe", target_pid);
    len=readlink(link_path, target_path, sizeof(target_path)-1);
    if(len!=-1) {
        target_path[len]='\0';
        printf("%-6d %-6s %-6s %s\n", target_pid, "txt", "REG", target_path);
    }

    snprintf(link_path, sizeof(link_path), "/proc/%d/maps", target_pid);
    FILE *maps_f=fopen(link_path, "r");
    if(maps_f!=NULL) {
        char line[2048];
        char **seen_maps=NULL;
        int seen_cnt=0;
        int seen_cap=0;

        while(fgets(line, sizeof(line), maps_f)!=NULL) {
            char *path=strchr(line, '/');
            if(path==NULL) continue;

            path[strcspn(path, "\n")]='\0';
            int dup=0;
            for(int i=0;i<seen_cnt;i++) {
                if(strcmp(seen_maps[i], path)==0) {
                    dup=1;
                    break;
                }
            }

            if(dup==0) {
                if(seen_cnt>=seen_cap) {
                    int new_cap=(seen_cap==0) ? 16 : seen_cap*2;
                    char **new_maps=realloc(seen_maps, (size_t)new_cap*sizeof(char *));
                    if(new_maps==NULL) break;
                    seen_maps=new_maps;
                    seen_cap=new_cap;
                }
                seen_maps[seen_cnt]=strdup(path);
                if(seen_maps[seen_cnt]==NULL) break;
                seen_cnt++;
                printf("%-6d %-6s %-6s %s\n", target_pid, "mem", "REG", path);
            }
        }

        for(int i=0;i<seen_cnt;i++) free(seen_maps[i]);
        free(seen_maps);
        fclose(maps_f);
    }

    snprintf(link_path, sizeof(link_path), "/proc/%d/fd", target_pid);
    DIR *fd_dir=opendir(link_path);
    if(fd_dir!=NULL) {
        struct dirent *dir;
        while((dir=readdir(fd_dir))!=NULL) {
            if(strcmp(dir->d_name, ".")==0 || strcmp(dir->d_name, "..")==0) continue;

            char fd_link[5000];
            snprintf(fd_link, sizeof(fd_link), "%s/%s", link_path, dir->d_name);
            len=readlink(fd_link, target_path, sizeof(target_path)-1);
            if(len==-1) continue;
            target_path[len]='\0';

            struct stat st;
            char *type_str="REG";
            if(stat(target_path, &st)==0) {
                if(S_ISDIR(st.st_mode)) type_str="DIR";
                else if(S_ISCHR(st.st_mode)) type_str="CHR";
                else if(S_ISBLK(st.st_mode)) type_str="BLK";
                else if(S_ISFIFO(st.st_mode)) type_str="FIFO";
                else if(S_ISLNK(st.st_mode)) type_str="LNK";
                else if(S_ISSOCK(st.st_mode)) type_str="SOCK";
            }
            else {
                if(strncmp(target_path, "pipe:", 5)==0) type_str="FIFO";
                else if(strncmp(target_path, "socket:", 7)==0) type_str="SOCK";
                else if(strncmp(target_path, "anon_inode:", 11)==0) type_str="CHR";
            }

            printf("%-6d %-6s %-6s %s\n", target_pid, dir->d_name, type_str, target_path);
        }
        closedir(fd_dir);
    }
}
