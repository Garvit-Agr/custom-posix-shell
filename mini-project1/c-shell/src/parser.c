#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"


tknll* lexer(char *input) {

    tknll *head=NULL;
    tknll *ptr=NULL;

    for(int i=0;input[i]!='\0';i++) {
        if(input[i]==' ' || input[i]=='\t' || input[i]=='\n' || input[i]=='\r') continue;

        tknll *new=malloc(sizeof(tknll));
        new->next=NULL;
        new->tkn=NULL;

        if(input[i]=='|') new->type=OP_PIPE;
        else if(input[i]=='&') new->type=OP_AMP;
        else if(input[i]==';') new->type=OP_SEMI;
        else if(input[i]=='<') new->type=OP_LT;
        else if(input[i]=='>') {
            if(input[i+1]=='>') {
                new->type=OP_GTGT;
                i++;
            }
            else new->type=OP_GT;
        }


        else {
            new->type=WORD;
            new->tkn=malloc(strlen(input)+1);
            int t=0;
            int sngl_quot=0;
            int dbl_quot=0;

            while(input[i]!='\0') {

                if(sngl_quot==0 && dbl_quot==0) {
                    if(input[i]==' ' || input[i]=='\t' || input[i]=='\n' || input[i]=='\r' ||
                       input[i]=='|' || input[i]=='&' || input[i]==';' || input[i]=='<' || input[i]=='>') {
                        break;
                    }
                }


                if(input[i]=='\\') {
                    if(sngl_quot!=0) {
                        new->tkn[t++]=input[i++];
                    } else {
                        i++;
                        if(input[i]=='\0') {
                            printf("cshell: invalid syntax\n");
                            free(new->tkn); free(new); free_tkn_ll(head);
                            return NULL;
                        }

                        if(dbl_quot!=0 && input[i]!='"' && input[i]!='\\') {
                            new->tkn[t++]='\\';
                        }
                        new->tkn[t++]=input[i++];
                    }
                }
                
                else if(input[i]=='\'') {
                    if(dbl_quot==0) {sngl_quot=!sngl_quot; i++;}
                    else new->tkn[t++]=input[i++];
                }
                
                else if(input[i]=='"') {
                    if(sngl_quot==0) {dbl_quot=!dbl_quot; i++;}
                    else new->tkn[t++]=input[i++];
                }
                
                else new->tkn[t++]=input[i++];


            }

            if(sngl_quot!=0 || dbl_quot!=0) {
                printf("cshell: invalid syntax\n");
                free(new->tkn); free(new); free_tkn_ll(head);
                return NULL;
            }
            
            new->tkn[t]='\0';
            i--;
        }

        if(head==NULL) {
            head=new;
            ptr=head;
        } else {
            ptr->next=new;
            ptr=new;
        }
    }
    
    return head;
}



void free_tkn_ll(tknll *head) {
    while(head!=NULL) {
        tknll *next=head->next;

        if(head->tkn != NULL) free(head->tkn);

        free(head);
        head=next;
    }
}



bool valid_grmr(tknll *head) {

    int state=0; 
    // state==0 -> LINE / BG
    // state==1 -> ARG
    // state==2 -> TGT / CMD

    tknll *ptr=head;

    while(ptr!=NULL) {
        if(state==0 || state==2) {
            if(ptr->type==WORD) state=1;
            else return false;
        }
        else if(state==1) {
            if(ptr->type==WORD) state=1;
            else if(ptr->type==OP_AMP) state=0;
            else state=2;
        }

        ptr=ptr->next;
    }

    if(state==2) return false;

    return true;
}






