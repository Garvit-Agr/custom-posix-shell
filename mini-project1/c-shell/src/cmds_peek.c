#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/param.h>

#include "parser.h"
#include "cmds_peek.h"

void print_line_fwd(char *line, int *line_num, int n) {
    int empty=1;
    for(int i=0;line[i]!='\0';i++) {
        if(line[i]!='\r' && line[i]!='\n') { empty=0; break; }
    }
    
    if(empty==0 && n!=0) {
        printf("%d %s\n", (*line_num)++, line);
    }
    else printf("%s\n", line);

}

void print_line_bwd(char *line, int *tot_non_empty, int n) {
    int empty=1;
    for(int i=0;line[i]!='\0';i++) {
        if(line[i]!='\r' && line[i]!='\n') { empty=0; break; }
    }
    
    if(empty==0 && n!=0) {
        printf("%d %s\n", *tot_non_empty, line);
        (*tot_non_empty)--;
    }
    else {
        printf("%s\n", line);
    }
}

void process_file(int fd, int n, int r) {
    if(r==0) {
        int line_num=1;
        int line_cap=128;
        char *line_buf=malloc(line_cap);
        line_buf[0]='\0';
        int line_len=0;
        
        char buf[4096];
        int bytes;
        
        while((bytes=read(fd, buf, sizeof(buf)))>0) {
            for(int i=0;i<bytes;i++) {
                if(buf[i]=='\n') {
                    print_line_fwd(line_buf, &line_num, n);
                    line_len=0;
                    line_buf[0]='\0';
                }
                else {
                    if(line_len+1>=line_cap) {
                        line_cap*=2;
                        line_buf=realloc(line_buf, line_cap);
                    }
                    line_buf[line_len++]=buf[i];
                    line_buf[line_len]='\0';
                }
            }
        }
        if(line_len>0) print_line_fwd(line_buf, &line_num, n);
        free(line_buf);
        
    } else {
        off_t size=lseek(fd, 0, SEEK_END);
        
        if(size!=-1) {
            int tot_non_empty=0;
            lseek(fd, 0, SEEK_SET);
            char fbuf[4096];
            int bytes;
            int is_empty=1;
            
            while((bytes=read(fd, fbuf, sizeof(fbuf)))>0) {
                for(int i=0;i<bytes;i++) {
                    if(fbuf[i]=='\n') {
                        if(is_empty==0) tot_non_empty++;
                        is_empty=1;
                    }
                    else if(fbuf[i]!='\r') {
                        is_empty=0;
                    }
                }
            }
            if(is_empty==0) tot_non_empty++;

            off_t pos=size;
            if(pos>0) {
                lseek(fd, pos-1, SEEK_SET);
                char last_c;
                read(fd, &last_c, 1);
                if(last_c=='\n') pos--;
            }

            int line_cap=128;
            char *line_buf=malloc(line_cap);
            line_buf[0]='\0';
            int line_len=0;

            while(pos>0) {
                off_t to_read=4096;
                if(pos<4096) to_read=pos;
                pos-=to_read;

                lseek(fd, pos, SEEK_SET);
                char buf[4096];
                read(fd, buf, to_read);

                for(int i=to_read-1;i>=0;i--) {
                    if(buf[i]=='\n') {
                        for(int k=0;k<line_len/2;k++) {
                            char t=line_buf[k];
                            line_buf[k]=line_buf[line_len-1-k];
                            line_buf[line_len-1-k]=t;
                        }
                        print_line_bwd(line_buf, &tot_non_empty, n);
                        line_len=0;
                        line_buf[0]='\0';
                    }
                    else {
                        if(line_len+1>=line_cap) {
                            line_cap*=2;
                            line_buf=realloc(line_buf, line_cap);
                        }
                        line_buf[line_len++]=buf[i];
                        line_buf[line_len]='\0';
                    }
                }
            }
            if(line_len>0) {
                for(int k=0;k<line_len/2;k++) {
                    char t=line_buf[k];
                    line_buf[k]=line_buf[line_len-1-k];
                    line_buf[line_len-1-k]=t;
                }
                print_line_bwd(line_buf, &tot_non_empty, n);
            }
            free(line_buf);
            
        } else {
            int full_cap=4096;
            char *full_buf=malloc(full_cap);
            int full_len=0;
            char buf[4096];
            int bytes_read;
            
            while((bytes_read=read(fd, buf, sizeof(buf)))>0) {
                if(full_len+bytes_read>full_cap) {
                    while(full_len+bytes_read>full_cap) full_cap*=2;
                    full_buf=realloc(full_buf, full_cap);
                }
                memcpy(full_buf+full_len, buf, bytes_read);
                full_len+=bytes_read;
            }

            int tot_non_empty=0;
            int is_empty=1;
            for(int i=0;i<full_len;i++) {
                if(full_buf[i]=='\n') {
                    if(is_empty==0) tot_non_empty++;
                    is_empty=1;
                }
                else if(full_buf[i]!='\r') {
                    is_empty=0;
                }
            }
            if(is_empty==0) tot_non_empty++;

            int pos=full_len;
            if(pos>0 && full_buf[pos-1]=='\n') pos--;

            int line_cap=128;
            char *line_buf=malloc(line_cap);
            line_buf[0]='\0';
            int line_len=0;

            for(int i=pos-1;i>=0;i--) {
                if(full_buf[i]=='\n') {
                    for(int k=0;k<line_len/2;k++) {
                        char t=line_buf[k];
                        line_buf[k]=line_buf[line_len-1-k];
                        line_buf[line_len-1-k]=t;
                    }
                    print_line_bwd(line_buf, &tot_non_empty, n);
                    line_len=0;
                    line_buf[0]='\0';
                }
                else {
                    if(line_len+1>=line_cap) {
                        line_cap*=2;
                        line_buf=realloc(line_buf, line_cap);
                    }
                    line_buf[line_len++]=full_buf[i];
                    line_buf[line_len]='\0';
                }
            }
            if(line_len>0) {
                for(int k=0;k<line_len/2;k++) {
                    char t=line_buf[k];
                    line_buf[k]=line_buf[line_len-1-k];
                    line_buf[line_len-1-k]=t;
                }
                print_line_bwd(line_buf, &tot_non_empty, n);
            }
            free(line_buf);
            free(full_buf);
        }
    }
}

void peek(tknll *head, char *homwd, char *prevwd) {
    (void)homwd;
    (void)prevwd;
    
    tknll *ptr=head->next;
    
    int n=0;
    int r=0;
    char *files[500];
    int file_cnt=0;

    while(ptr!=NULL) {
        if(ptr->tkn[0]=='-' && strlen(ptr->tkn)>1) {
            int valid=1;
            for(int i=1;ptr->tkn[i]!='\0';i++) {
                if(ptr->tkn[i]=='n') n=1;
                else if(ptr->tkn[i]=='r') r=1;
                else valid=0;
            }
            if(valid==0) {
                printf("peek: invalid syntax\n");
                return;
            }
        }
        else files[file_cnt++]=ptr->tkn;

        ptr=ptr->next;
    }

    if(file_cnt==0) {
        files[0]="-";
        file_cnt=1;
    }

    for(int i=0;i<file_cnt;i++) {
        char *file=files[i];
        int fd;
        
        if(strcmp(file, "-")==0) {
            fd=STDIN_FILENO;
            process_file(fd, n, r);
        }
        else {
            DIR *dir_check = opendir(file);
            if(dir_check != NULL) {
                printf("peek: is a directory\n");
                closedir(dir_check);
                continue;
            }
            
            fd=open(file, O_RDONLY);
            if(fd!=-1) {
                process_file(fd, n, r);
                close(fd);
            }
            else {
                printf("peek: no such file or directory\n");
            }
        }
    }
}