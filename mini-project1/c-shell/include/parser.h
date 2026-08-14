#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

typedef enum tkntype {
    OP_PIPE,
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT,
    WORD
} tkntype;

typedef struct tknll {
    char *tkn;
    tkntype type;
    struct tknll *next;
} tknll;


tknll* lexer(char *input);
void free_tkn_ll(tknll *head);




#endif