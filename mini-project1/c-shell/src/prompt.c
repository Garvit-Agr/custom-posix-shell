#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>

#include "prompt.h"

void display_prompt(char *homewd) {
    char *usr=getlogin();
    if(usr==NULL || usr[0]=='\0') usr=getenv("USER");
    if(usr==NULL || usr[0]=='\0') {
        struct passwd *pw=getpwuid(getuid());
        if(pw!=NULL) usr=pw->pw_name;
    }
    if(usr==NULL || usr[0]=='\0') usr="unknown";

    char hst[PATH_MAX+5];
    char cwd[PATH_MAX+5];

    if(gethostname(hst,sizeof(hst))==-1) strcpy(hst,"unknown");
    else hst[sizeof(hst)-1]='\0';

    if(getcwd(cwd,sizeof(cwd))==NULL) strcpy(cwd,"?");


    if(strcmp(cwd,homewd)==0) strcpy(cwd,"~\0");
    else if(strcmp(homewd,"/")==0 && cwd[0]=='/') {
        char temp[PATH_MAX+5];
        snprintf(temp,sizeof(temp),"~%s",cwd+1);
        strcpy(cwd,temp);
    }
    else if(strlen(cwd)>strlen(homewd)) {

        if(strncmp(cwd,homewd,strlen(homewd))==0 && cwd[strlen(homewd)]=='/') {
            char temp[PATH_MAX+1];
            snprintf(temp,sizeof(temp),"~%s",cwd+strlen(homewd));
            strcpy(cwd,temp);
        }

    }

    printf("<%s@%s:%s> ",usr,hst,cwd);
    fflush(stdout);
}