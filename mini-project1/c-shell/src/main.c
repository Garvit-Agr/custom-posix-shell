#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>
#include <stdbool.h>

#include "prompt.h"
#include "parser.h"
#include "cmds_hop.h"

#define MAX_USER_INPUT_ALLOWED 50 //used because the max input length in not mentioned


int main() {

    char homewd[MAXPATHLEN+5]; //found this in description of "man getcwd"
    getcwd(homewd,MAXPATHLEN+5);
    
    char prevwd[MAXPATHLEN+5]={0}; //found this in description of "man getcwd"
    
    while(1) {
        display_prompt(homewd);
        char *input = NULL;
        size_t len = 0;

        if (getline(&input, &len, stdin) == -1) {
            printf("\n");
            free(input);
            break;
        }
        input[strcspn(input,"\n")] = '\0';

        tknll *head=lexer(input);

        if(head==NULL) continue;

        if(valid_grmr(head)==false) {
            printf("cshell: invalid syntax\n");
            free_tkn_ll(head);
            continue;
        }


        
        if(strcmp(head->tkn,"hop")==0) hop(head, homewd, prevwd);
        
        





        free_tkn_ll(head);
        free(input);
    }


    return 0;
}