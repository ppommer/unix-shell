#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>


struct termios orig_termios;
int non_canon = 0;

void processString(char *str){

    int i, j, strLen = strlen(str);
    for(i = 0, j = 0;j < strLen;j++){
        if(str[j] == '\b'){
            i--;
        } else {
            str[i++] = str[j];
        }
    }
    str[i] = '\0';
}

int io_close() {
    if (non_canon) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        non_canon = 0;
        return 1;
    }
    return 0;
}

int io_open() {
    if (non_canon) return 0;
    if (!isatty(STDIN_FILENO)) return -1;

    tcgetattr(STDIN_FILENO, &orig_termios);
    //atexit(io_close);

    struct termios new_termios;
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &new_termios);
    non_canon = 1;
    return 1;
}

char wildcardStr[4096];

// the function which acts as a filter for scandir
int io_scanFilter(const struct dirent *dirEnt) {

    // wildcard matching through the neat fnmatch function
    //printf("dirEnt->d_name: %s\n", dirEnt->d_name);
    if (fnmatch(wildcardStr, dirEnt->d_name, 0) == 0) {
        return 1;
    }
    return 0;

}

void commonPrefix(char *str1, char *str2) {
    int n1 = strlen(str1), n2 = strlen(str2);

    // Compare str1 and str2
    int i, j;
    for (i = 0, j = 0; i < n1 && j < n2; i++, j++) {
        if (str1[i] != str2[j])
            break;
    }
    str1[i] = '\0';
}

int io_expand(char *str, int n) {

    //printf("\nstr: %s\n", str);

    char realstr[4096];
    strcpy(realstr, str);
    processString(realstr);

    //printf("realstr: %s\n", realstr);

    char preprefix[1024];
    char *chptr = strrchr(realstr, ' ');
    int lastSpace = (chptr == NULL) ? -1 : (chptr - realstr);
    if (lastSpace >= 0) {
        strncpy(preprefix, realstr, lastSpace);
    } else {
        preprefix[0] = '\0';
    }

    char sufstr[4096];
    sprintf(sufstr, "%s", realstr + (lastSpace+ 1));

    char searchDir[4096];
    char prefix[4096];

    chptr = strrchr(sufstr, '/');
    int lastSlash = (chptr == NULL) ? -1 : (chptr - sufstr);
    int absolute = lastSlash == 0 ? 1 : 0;

    if (lastSlash >= 0) {
        strncpy(prefix, sufstr, lastSlash);
    } else {
        prefix[0] = '\0';
    }

    if (!absolute) {
        sprintf(searchDir, "./%s/", prefix);
    }


    if (lastSlash == strlen(sufstr) - 1) {
        sprintf(wildcardStr, "*");
    } else {
        sprintf(wildcardStr, "%s*", sufstr + (lastSlash + 1));
    }

    //printf("prefix: %s\n", prefix);
    //printf("searchDir: %s\n", searchDir);
    //printf("wildcardStr: %s\n", wildcardStr);

    struct dirent **namelist;
    int nn;

    nn = scandir(strlen(searchDir) == 0 ? "." : searchDir, &namelist, io_scanFilter, alphasort);

    char completionStr[4096];
    completionStr[0] = '\0';

    if (nn < 0) {
        perror("Error.");
    } else if (nn == 1) {
        char tempStr[4096];
        sprintf(tempStr, "%s%s%s", searchDir, searchDir[0] == '\0' ? "" : "/", namelist[0]->d_name);
        //printf("tempstr: %s\n", tempStr);

        struct stat s;
        int err = stat(tempStr, &s);
        if (err != -1 && S_ISDIR(s.st_mode)) {
            sprintf(completionStr, "%s/", namelist[0]->d_name);
        } else if(err != -1){
            sprintf(completionStr, "%s", namelist[0]->d_name);
        }

        free(namelist[0]);
        free(namelist);

    } else if (nn > 1){

        char prefix[1024];
        //printf("d_name: %s\n", namelist[0]->d_name);
        strcpy(prefix, namelist[0]->d_name);

        for (int i = 1; i < nn ; i++) {
            commonPrefix(prefix, namelist[i]->d_name);
        }

        //printf("prefix: %s\n", prefix);

        strcpy(completionStr, prefix);

        for (int i = 0; i < nn; ++i) {
            free(namelist[i]);
        }
        free(namelist);
    } else if(nn == 0){
        sprintf(completionStr, "%s", sufstr + (lastSlash + 1));
    }


    //printf("completionStr: %s\n", completionStr);

    sprintf(searchDir, "%s%s%s%s%s", preprefix, preprefix[0] == '\0'?"":" ", prefix, prefix[0] == '\0'?"":"/", completionStr);
    int orig_length = strlen(realstr);
    strncpy(realstr, searchDir, n);
    //printf("\nstr: %s\n", sufstr);
    int realstrlen = strlen(realstr);
    int dif = realstrlen - orig_length;
    strcpy(str+strlen(str), realstr + realstrlen - dif);
    return dif;
}

char *io_fgets(char *str, int n, FILE *stream) {
    if (non_canon) {
        char buf;
        int i;
        int isBreak = 0;
        for (i = 0; (--n > 0) && (read(STDIN_FILENO, &buf, 1) == 1);) {
            if (buf == '\r' || buf == '\n') {
                buf = '\n';
                isBreak = 1;
            } else if (buf == '\x7f' || buf == '\b') {
                str[i++] = '\b';
                str[i++] = ' ';
                str[i++] = '\b';
                printf("\b \b");
                fflush(stdout);
                continue;
            } else if (buf == '\x03') {
                return NULL;
            } else if (buf == '\t') {
                //processString(str);
                //printf("\nstr: %s (%d)\n", str, strlen(str));

                int ret = io_expand(str, n);
                i += ret;
                n -= ret;
                //printf("ret: %d\n", ret);
                int strLen = strlen(str);
                for (int j = strLen-ret; j < strLen; ++j) {
                    printf("%c", str[j]);
                }
                fflush(stdout);
                continue;
            }
            str[i] = buf;
            printf("%c", buf);
            fflush(stdout);
            i++;
            if (isBreak) break;
        }
        str[i] = '\0';
        processString(str);
        return str;
    } else {
        return fgets(str, n, stream);
    }
}
