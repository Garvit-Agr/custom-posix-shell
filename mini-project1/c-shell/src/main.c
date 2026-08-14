#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> //included for "MAXPATHLEN"
#include <string.h>

#include "prompt.h"
#include "parser.h"

#define MAX_USER_INPUT_ALLOWED 50 //used because the max input length in not mentioned


int main() {

    char homewd[MAXPATHLEN+5]; //found this in description of "man getcwd"
    getcwd(homewd,MAXPATHLEN+5);

    
    while(1) {
        display_prompt(homewd);
        char input[MAX_USER_INPUT_ALLOWED+5];
        
        if (fgets(input, sizeof(input), stdin)==NULL) { // used this instead if scanf, so that multi-word sentences can be taken easily as an input
                                                        // put "if" to bypass ctrl+d issue.
            printf("\n");
            break;
        }
        input[strcspn(input,"\n")] = '\0';  // replaced trailing "\n" in the "input" string with "\0"

        tknll *head=lexer(input);




        free_tkn_ll(head);

    }


    return 0;
}