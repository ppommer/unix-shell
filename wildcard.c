#include <fnmatch.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "list.h"

#include "wildcard.h"

char *wildcardStr;

// the function which acts as a filter for scandir
int scanFilter(const struct dirent *dirEnt) {

    // ignore "." and ".."
    if (strcmp(dirEnt->d_name, ".") == 0 || strcmp(dirEnt->d_name, "..") == 0) {
        return 0;
    }

    // wildcard matching through the neat fnmatch function
    if (fnmatch(wildcardStr, dirEnt->d_name, 0) == 0) {
        return 1;
    }
    return 0;

}

// takes every parameter and checks for wildcards
// then it adds every possible match as an own param
// e.g.
// ["ls"] -> ["/home/stud*/hw"]
// =>
// ["ls"] -> ["/home/student/hw"] -> ["/home/student2/hw"]
void expandWildcards(list_t *paras) {
    struct list_elem *curr = paras->first;
    for (int i = 0; curr != NULL; i++) {
        char *string = (char *) curr->data;

        // contains wildcard ?
        if (strchr(string, '*') != NULL) {

            // search for the last slash before the wildcard
            // the wildcard itself
            // and the next slash after the wildcard
            // they're the index of the corresponding chars within the string (0 based)
            //          "/home/stud*/hw" (len: 14)
            // lastSlash(5)---|    ||---nextSlash(11)
            //      wildcard(10)---|
            int lastSlash = -1;
            int wildcard = -1;
            int nextSlash = -1;

            for (int j = 0; j < strlen(string); ++j) {
                if (string[j] == '/') {
                    // if wildcard not found maybe its the lastSlash
                    if (wildcard < 0) {
                        lastSlash = j;
                    } // if wildcard already found it's the nextSlash
                    else {
                        if (nextSlash < 0) {
                            nextSlash = j;
                            break;
                        }
                    }
                } else if (string[j] == '*') {
                    wildcard = j;
                }
            }

            // split the string into 3 parts:
            // "/home/stud*/hw"

            // prefix: the path before the part with the wildcard: "/home" (empty if lastSlash < 0)
            char *prefix;
            if (lastSlash < 0) {
                prefix = calloc(1, sizeof(char));
            } else {
                prefix = calloc(lastSlash + 1, sizeof(char));
                strncpy(prefix, string, lastSlash);
            }

            // infix: the part with the wildcard in it: "stud*"
            char *infix;
            int inL;
            if (nextSlash < 0) {
                inL = strlen(string) - (lastSlash + 1);
                infix = calloc(inL + 1, sizeof(char));
            } else {
                inL = nextSlash - (lastSlash + 1);
                infix = calloc(inL + 1, sizeof(char));
            }
            strncpy(infix, string + lastSlash + 1, inL);

            // suffix: the path after the part with the wildcard: "hw" (empty if nextSlash < 0)
            char *suffix;
            if (nextSlash < 0) {
                suffix = calloc(1, sizeof(char));
            } else {
                int sufL = strlen(string) - (nextSlash + 1);
                suffix = calloc(sufL + 1, sizeof(char));
                strncpy(suffix, string + nextSlash + 1, sufL);
            }

            // scan all dirs in prefix or if prefix is empty in the current dir (".")
            wildcardStr = infix;

            struct dirent **namelist;
            int n;

            n = scandir(strlen(prefix) == 0 ? "." : prefix, &namelist, scanFilter, alphasort);
            if (n < 0) {
                // ERROR
            } else {
                while (n--) {

                    // concat the prefix with the found dir and the suffix (optional slashes in between)
                    char *tempStr = calloc(strlen(prefix) + (prefix[0] == '\0' ? 0 : 1) +
                                           strlen(namelist[n]->d_name) +
                                           (suffix[0] == '\0' ? 0 : 1) + strlen(suffix) +
                                           1,
                                           sizeof(char));

                    sprintf(tempStr, "%s%s%s%s%s",
                            prefix, prefix[0] == '\0' ? "" : "/",
                            namelist[n]->d_name,
                            suffix[0] == '\0' ? "" : "/", suffix);

                    /*
                    char *realPath = calloc(4096, sizeof(char));
                    realpath(tempStr, realPath);
                    free(tempStr);
                    char *finalStr = calloc(strlen(realPath) + 1, sizeof(char));
                    strcpy(finalStr, realPath);
                    free(realPath);
                    */


                    //check if this path actually exists
                    struct stat s;
                    int err = stat(tempStr, &s);
                    if (err != -1) {
                        // yes it does -> good to go (be added to the params list)
                        list_insert_after(paras, curr, tempStr);
                    } else {
                        // meehh -> discard
                        free(tempStr);
                    }

                    free(namelist[n]);
                }
                free(namelist);
            }
            struct list_elem *next = curr->next;
            // remove wildcard parameter
            free(string);
            list_remove(paras, curr);

            curr = next;
        } else {
            curr = curr->next;
        }
    }
}