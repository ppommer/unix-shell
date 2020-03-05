#include "list.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef BIT_MAN
#define BIT_MAN
// set nth bit of x
#define _BS(x, n) (x | (1 << n));
// clear nth bit of x
#define _BC(x, n) (x & (~(1UL << n)));
// toggle nth bit of x
#define _BT(x, n) (x ^ (1UL << n));
// check nth bit of x
#define _CB(x, n) ((x >> n) & 1U);
#endif


// This function takes the list of chars at buffer and appends them as a string to the list at params
void addParam(list_t *params, list_t *buffer) {
    if (list_length(buffer) > 0) {
        list_append(params, list_toString(buffer));
        list_removeAll(buffer);
    }
}

/*
 * This function takes the name of the env var at varBuffer (stored as list of chars)
 * and add the value of this env var value char by char to the list buffer
 */
void addVar(list_t *buffer, list_t *varBuffer) {
    if (list_length(varBuffer) <= 0) {
        return;
    }
    char *envVar = getenv(list_toString(varBuffer));
    list_removeAll(varBuffer);

    // env var not found, variable ignored
    if (envVar == NULL) {
        return;
    }

    char *cP;
    for (int i = 0; i < strlen(envVar); ++i) {
        cP = malloc(sizeof(char));
        *cP = envVar[i];
        list_append(buffer, cP);
    }

}

// parse the command provided by str and stores every parameter as a own string in the list at res
list_t *myParse(list_t *res, char *str, char *envp[]) {

    //list_t *res = list_init();
    // Maybe don't do this so that we can have strings already in res.
    // the paras only get appended sooo... it's not really necessary (I think)
    list_removeAll(res);
    //printf("myParse res: %p", res);

    // the list of chars that form a param
    list_t *buf = list_init();
    // list of chars that form a var name
    list_t *varBuf = list_init();

    // stores the last enclosing char so we know if the current chars are in enclosed
    char encChar = 0;
    // if the current chars are part of a var name
    bool isVar = false;
    // if the current char(s) is/are escaped
    bool isEscaped = false;

    int i;
    char c;
    char *cP;
    for (i = 0, c = str[i]; c != 0; i++, c = str[i]) {
        // user input end
        if (c == '\n') {
            break;
        }

        // New pointer to the current char so we can store it in lists
        cP = malloc(sizeof(char));
        *cP = c;
        //printf("c: %c\n", *cP);

        // if currently in var name
        if (isVar) {
            // if valid var name char (only 0-9, A-Z, _)
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_') {
                // valid -> add to varBuf list
                list_append(varBuf, cP);
                continue;
            } else {
                // no var name anymore -> add var value to buf
                addVar(buf, varBuf);
                isVar = false;
            }
        }

        // if currently escaped
        if (isEscaped) {
            // append the current char as is without processing it
            list_append(buf, cP);
            isEscaped = false;
        } // if currently enclosed
        else if (encChar != 0) {
            // if current char is the enclosing char
            if (c == encChar) {
                // not enclosed anymore
                encChar = 0;
            } else {
                // var name ?
                if (c == '$') {
                    isVar = true;
                } // escaped ?
                else if (c == '\\') {
                    isEscaped = true;
                } // add as is
                else {
                    list_append(buf, cP);
                }
            }
        } else {
            switch (c) {
                case '"':
                case '\'':
                    encChar = c;
                    break;
                case '$':
                    isVar = true;
                    break;
                case '\\':
                    isEscaped = true;
                    break;
                case ' ':
                    // current param is ready to be added to the list of params
                    addParam(res, buf);
                    break;
                default:
                    list_append(buf, cP);
            }

        }
    }

    // end of input

    // var name left ?
    if (isVar) {
        // process the var
        addVar(buf, varBuf);
    }

    // ignore if enclosing chars aren't closed... for now
    /*
    if (encChar != 0) {

    } else if (isEscaped) {

    } else {

    }
    */

    // add to param list
    addParam(res, buf);

    // clean up
    list_finit(buf);
    list_finit(varBuf);

    return res;
}

/*
 * Gets Input-/Output- redirects and stores the filenames in the Pointers
 * Pointers are NULL if no redirect was found or if an error occured
 * Returns -1 if error occured, 0b01 if Out-red was found, 0b10 if In-red was found,
 * and 0b11 if both redirects were found
 * removes '>', '<' and there argumments from the args list
 */
int myParseStg2(list_t *args, char **outFileP, char **inFileP) {
    // init
    int retVal = 0;
    // number of elements to be deleted starting from the current element
    int delCurrCount = 0;
    *outFileP = NULL;
    *inFileP = NULL;
    // search for redirects
    for (struct list_elem *curr = args->first; curr; curr = curr->next) {
        // if current arg is > (out-redir)
        if (strcmp((char *) curr->data, ">") == 0) {
            // if not last arg
            if (curr->next != NULL) {

                // next arg is out-redir filename
                char *outFileT = (char *) curr->next->data;
                // allocate new string space for output
                *outFileP = calloc(strlen(outFileT) + 1, sizeof(char));
                strcpy(*outFileP, outFileT);

                // if string copying didn't fail
                if (strcmp(*outFileP, outFileT) == 0) {
                    // set the 0th bit in the return value -> out-redir found
                    retVal = _BS(retVal, 0);
                    delCurrCount = 2;
                } else {
                    *outFileP = NULL;
                    retVal = -1;
                    break;
                }
            } // is last arg -> error
            else {
                retVal = -1;
                break;
            }
        } // if current arg is < (in-redir)
        else if (strcmp((char *) curr->data, "<") == 0) {
            // if not last arg
            if (curr->next != NULL) {

                // next arg is in-redir filename
                char *inFileT = (char *) curr->next->data;
                // allocate new string space for output
                *inFileP = calloc(strlen(inFileT) + 1, sizeof(char));
                strcpy(*inFileP, inFileT);

                // if string copying didn't fail
                if (strcmp(*inFileP, inFileT) == 0) {
                    // set the 1st bit in the return value -> out-redir found
                    retVal = _BS(retVal, 1);
                    delCurrCount = 2;
                } else {
                    *inFileP = NULL;
                    retVal = -1;
                    break;
                }
            } // is last arg -> error
            else {
                retVal = -1;
                break;
            }
        }

        // TODO: Maybe rewrite deleting, not very nice style, just use a while loop
        // while we have to delete something
        while (delCurrCount > 0) {
            // backup curr
            struct list_elem *currT = curr;
            curr = curr->next;

            // remove last curr
            list_remove(args, currT);
            delCurrCount--;

            // if deleting is done
            if (delCurrCount == 0) {
                // we have to make a pseudo curr-element where the next elem is the actual curr,
                // because the for loop would skip the curr
                curr = &((struct list_elem) {.next=curr});
            }
        }
    }
    return retVal;
}

/*
 * takes the list of arguments in args and splits it by the first pipe-char it finds.
 * The elements after the pipe get moved to args2 and return a new pipe
 * returns -1 on error, -2 if no pipe was found, pipe() return val on success
 */
int myParsePipe(list_t *args, list_t *args2, int pipeA[]) {
    if (pipeA == NULL || args2 == NULL || args == NULL || args->count <= 0) {
        return -1;
    }

    // search for pipe char ("|")
    struct list_elem *prev = NULL, *curr = NULL;
    for (curr = args->first; curr != NULL; prev = curr, curr = curr->next) {
        if (strcmp((char *) curr->data, "|") == 0) {
            break;
        }
    }

    // no pipe found
    if (curr == NULL) {
        return -2;
    }

    // clear args2
    list_removeAll(args2);

    /*
     * -> access is shorted by . (yes it's technically wrong but looks better ^^)
     *    ["ls"]       -> ["."] -> ["|"] -> ["rev"]
     *      /\             /\       /\        /\
     *   args.first       prev     curr    args.last
     *
     *                       ;;;;;
     *                       ;;;;;
     *                     ..;;;;;..
     *                      ':::::'
     *                        ':`
     *  --------------------------    ----------------------------
     *  |          args          |    |              args2       |
     *
     *    ["ls"]       -> ["."]                  ["rev"]
     *      /\             /\                      /\
     * [args.first]       prev          curr.next-/  \-args.last
     * [   ||     ]        ||               ||             ||
     * [args.first]     args.last      args2.first    args2.last
     */

    args2->last = args->last;
    args2->first = curr->next;
    args->last = prev;

    // delete the element containing the pipe char
    prev->next = NULL;


    // we manually changed the list so we the count values are all wrong
    list_updateCount(args);
    list_updateCount(args2);

    return pipe(pipeA);
}