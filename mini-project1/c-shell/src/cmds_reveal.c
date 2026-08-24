#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h> // for dirent struct and DIR commands like opendir and closedir
#include <sys/param.h>

#include "parser.h"
#include "cmds_reveal.h"

int cmp(const void *a, const void *b) {
    //fixing sort by bare name without trailing /, because of doubt doc q19
    const char *sa=*(const char**)a;
    const char *sb=*(const char**)b;
    int la=strlen(sa);
    int lb=strlen(sb);
    if(la>0 && sa[la-1]=='/') la--;
    if(lb>0 && sb[lb-1]=='/') lb--;
    int min_len=la<lb?la:lb;
    int r=strncmp(sa, sb, min_len);
    if(r!=0) return r;
    return la-lb;
}

void fetch_paths(char *base, char *rel, char ***lst, int *cnt, int *cap, int a, int t) {
    char full_path[(2*MAXPATHLEN)+10];
    if(rel[0]=='\0') strcpy(full_path, base);
    else snprintf(full_path, sizeof(full_path), "%s/%s", base, rel);

    DIR *d=opendir(full_path);
    if(d==NULL) return;

    struct dirent *dir;
    while((dir=readdir(d))!=NULL) {
        if(a==0 && dir->d_name[0]=='.') continue;
        
        if(t!=0 && (strcmp(dir->d_name, ".")==0 || strcmp(dir->d_name, "..")==0)) continue;

        char rel2[MAXPATHLEN+5];
        if(rel[0]=='\0') strcpy(rel2, dir->d_name);
        else snprintf(rel2, sizeof(rel2), "%s%s", rel, dir->d_name);

        char item_full[(2*MAXPATHLEN)+10];
        snprintf(item_full, sizeof(item_full), "%s/%s", base, rel2);
        DIR *chk=opendir(item_full);
        
        if(chk!=NULL) { 
            closedir(chk);
            
            if(t!=0) {
                char rel_slash[MAXPATHLEN+10];
                snprintf(rel_slash, sizeof(rel_slash), "%s/", rel2);
                
                if(*cnt>=*cap) {
                    *cap*=2;
                    *lst=realloc(*lst, (*cap)*sizeof(char*));
                }
                (*lst)[*cnt]=strdup(rel_slash);
                (*cnt)++;
                
                fetch_paths(base, rel_slash, lst, cnt, cap, a, t);
            } else {
                if(*cnt>=*cap) {
                    *cap*=2;
                    *lst=realloc(*lst, (*cap)*sizeof(char*));
                }
                (*lst)[*cnt]=strdup(rel2);
                (*cnt)++;
            }
        }
        else {
            if(*cnt>=*cap) {
                *cap*=2;
                *lst=realloc(*lst, (*cap)*sizeof(char*));
            }
            (*lst)[*cnt]=strdup(rel2);
            (*cnt)++;
        }
    }
    closedir(d);
}

void reveal(tknll *head, char *homwd, char *prevwd) {
    tknll *ptr=head->next;
    
    int a=0;
    int t=0;
    char target[MAXPATHLEN+5]="";
    int tgt_set=0;
    int inv_syn=0;

    //fixing builtins ignoring operators, because of problem statement part A3
    while(ptr!=NULL && ptr->type!=OP_PIPE && ptr->type!=OP_SEMI && ptr->type!=OP_AMP && ptr->type!=OP_LT && ptr->type!=OP_GT && ptr->type!=OP_GTGT) {
        if(ptr->tkn[0]=='-' && strlen(ptr->tkn)>1 && strcmp(ptr->tkn, "-")!=0) {
            //fixing reveal flags after path, because of doubt doc q18
            if(tgt_set!=0) {
                inv_syn=1;
            }
            else {
                for(int i=1;ptr->tkn[i]!='\0';i++) {
                    if(ptr->tkn[i]=='a') a=1;
                    else if(ptr->tkn[i]=='t') t=1;
                    else inv_syn=1;
                }
            }
        }
        else {
            if(tgt_set!=0) inv_syn=1;
            else {
                strcpy(target, ptr->tkn);
                tgt_set=1;
            }
        }
        ptr=ptr->next;
    }

    if(inv_syn!=0) {
        printf("reveal: invalid syntax\n");
        return;
    }

    char res_path[MAXPATHLEN+5];
    if(tgt_set==0) {
        strcpy(res_path, ".");
    }
    else if(strcmp(target, "~")==0) {
        strcpy(res_path, homwd);
    }
    else if(strcmp(target, "-")==0) {
        if(prevwd[0]=='\0') {
            printf("reveal: no such directory\n");
            return;
        }
        strcpy(res_path, prevwd);
    }
    else {
        strcpy(res_path, target);
    }

    DIR *target_dir=opendir(res_path);
    if(target_dir==NULL) {
        printf("reveal: no such directory\n");
        return;
    }
    closedir(target_dir);

    int cap=100;
    int cnt=0;
    char **lst=malloc(cap*sizeof(char*));

    fetch_paths(res_path, "", &lst, &cnt, &cap, a, t);

    qsort(lst, cnt, sizeof(char*), cmp);

    for(int i=0;i<cnt;i++) {
        //fixing quoting filenames with spaces, because of doubt doc q30
        if(strchr(lst[i], ' ')!=NULL) printf("'%s'\n", lst[i]);
        else printf("%s\n", lst[i]);
        free(lst[i]);
    }

    
    free(lst);
}