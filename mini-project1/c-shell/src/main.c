#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>
#include <stdbool.h>

#include "prompt.h"
#include "parser.h"
#include "cmds_hop.h"
#include "cmds_reveal.h"
#include "cmds_peek.h"
#include "cmds_locate.h"
#include "exec.h"

#define MAX_USER_INPUT_ALLOWED 1024


int main() {

    char homewd[MAXPATHLEN+5]; //found this in description of "man getcwd"
    getcwd(homewd,MAXPATHLEN+5);
    
    char prevwd[MAXPATHLEN+5]={0}; //found this in description of "man getcwd"
    
    while(1) {
        display_prompt(homewd);
        char input[MAX_USER_INPUT_ALLOWED+5];
        
        if (fgets(input, sizeof(input), stdin)==NULL) { // used this instead if scanf, so that multi-word sentences can be taken easily as an input
                                                        // put "if" to bypass ctrl+d issue.
            printf("\n");
            break;
        }
        input[strcspn(input,"\n")]='\0';

        tknll *head=lexer(input);

        if(head==NULL) continue;

        if(valid_grmr(head)==false) {
            printf("cshell: invalid syntax\n");
            free_tkn_ll(head);
            continue;
        }


        
        if(strcmp(head->tkn,"hop")==0) hop(head, homewd, prevwd);
        else if(strcmp(head->tkn,"reveal")==0) reveal(head, homewd, prevwd);
        else if(strcmp(head->tkn,"peek")==0) peek(head, homewd, prevwd);
        else if(strcmp(head->tkn,"locate")==0) locate(head, homewd, prevwd);


        else {
            printf("You are in else block\n");
        }





        free_tkn_ll(head);
    }


    return 0;
}