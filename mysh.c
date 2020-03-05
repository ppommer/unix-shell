#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include "list.h"
#include "wildcard.h"
#include "io.h"

// fake directory name to idetify internal commands
char internalPrefixFormat[] = "/>%06d#internal</";

// The resulting string whith length:
// snprintf(NULL, 0, internalPrefixFormat, 0) + 1
char internalPrefix[20];

// Current working directory as string
char *pwd;

// Thread list (they aren't actual threads so I don't remember why i called it that)
list_t *tL;
// Pipe descriptor list (again, I don't remember why it's cL)
list_t *cL;
// List of parameters given to the shell
list_t *paras;

// parser.c function declarations
extern list_t *myParse(list_t *res, char *str, char *envp[]);

extern int myParseStg2(list_t *args, char **outFileP, char **inFileP);

extern int myParsePipe(list_t *args, list_t *args2, int pipeA[]);
// END

// Compares two pointers like ints.
int intPcmp(const void *intP1, const void *intP2) {
    return (*(int *) intP1) - (*(int *) intP2);
}

// Frees 'all' allocated memory and exits.
void cleanExit(int status) {
    list_finit(tL);
    list_finit(cL);
    list_finit(paras);
    free(pwd);
    exit(status);
}

// Change the current working directory by the given list of parameters
// e.g. list ["cd"] -> ["a/dir/to/go"]
// the first element of the list should always be the string "cd".
// It can be something else but it does get ignored none the less.
// Always returns 0
int changeDir(list_t *cmdList) {
    int argc = list_length(cmdList);
    // no arguments given
    if (argc == 1) {
        // print current working dir
        printf("%s\n", pwd);
    } // exactly one argument given
    else if (argc == 2) {
        // the new dir string is the second argument
        char *dir = (char *) cmdList->first->next->data;

        // the new working dir is: pwd + "/" + dir. So the length must be strlen(pwd) + 1 + strlen(dir) + 1 (null term)
        char newPwd[strlen(pwd) + strlen(dir) + 2];
        sprintf(newPwd, "%s/%s", pwd, dir);

        // get the real path of the new wd (/path/.././path/.. -> /) in other words deuglify it
        // but because
        char realNewPwd[4096];
        realpath(newPwd, realNewPwd);
        //printf("RealNewPwd: %s\n", realNewPwd);

        // check if the given path exists and is a directory
        struct stat s;
        int err = stat(newPwd, &s);
        if (err != -1 && S_ISDIR(s.st_mode)) {

            // copy the realNewPwd to our current working dir
            strcpy(pwd, realNewPwd);
            //printf("pwd: %s\n", pwd);
            // set the environmental var PWD to the new wd
            setenv("PWD", pwd, 1);
            // also directly set the the wd for the current process
            chdir(pwd);

        } else {
            // Error: file or directory not found
            printf("cd: %s: %s\n", dir, strerror(ENOENT));
        }
        // we don't have to free anything because we only declared arrays :)
    } // too many arguments or list is empty
    else {
        // Print usage
        printf("Usage: cd <dir>\n");
    }
    return 0;
}

int execCmd(const char *filename, char *const argv[], char *const envp[]) {
    // I actually wanted to delegate the internal process execution to this method but this method gets called after
    // fork() so we can't change anything in the actual shell process
    // TODO: Cleanup
    /*
    if (strncmp(filename, internalPrefix, strlen(internalPrefix)) == 0) {
        if (strncmp(filename + strlen(internalPrefix), "/cd", 3) == 0) {
            return changeDir(argv, envp);
        }
    }
    */
    return execve(filename, argv, envp);
}


int processCmd(list_t *tL, list_t *cmdList, char *envp[], int inPipe, list_t *cL) {

    // check if cd is called
    // maybe delegate this check to another function like 'processInternalCmd'
    if (list_length(cmdList) > 0 && strncmp((char *) cmdList->first->data, "cd", 2) == 0) {
        return changeDir(cmdList);
    }

    char **outFileP = malloc(sizeof(char *));
    char **inFileP = malloc(sizeof(char *));
    int cpid;


    list_t *args2 = list_init();


    int pipeA2[2];
    // look for next pipe
    if (myParsePipe(cmdList, args2, pipeA2) >= 0) {

        // add the output fd of the pipe to the pipe list
        int *cP = malloc(sizeof(int));
        *cP = pipeA2[0];
        list_append(cL, cP);

        // add the input fd of the pipe to the pipe list
        cP = malloc(sizeof(int));
        *cP = pipeA2[1];
        list_append(cL, cP);

        // process the command after the pipe first
        processCmd(tL, args2, envp, pipeA2[0], cL);
    } // no pipes no worries :)
    else {
        pipeA2[0] = -1;
        pipeA2[1] = -1;
    }

    // look for in-/out-redirs
    myParseStg2(cmdList, outFileP, inFileP);

    if ((cpid = fork()) == 0) {

        // convert the param list to the argv array for execve
        char *argv[list_length(cmdList) + 1];
        list_to_array(cmdList, (void **) argv);
        argv[list_length(cmdList)] = NULL;
        list_finit(cmdList);

        char delimiter[] = ":";
        char *ptr;
        char *string = getenv("PATH");
        if (string == NULL) {
            fprintf(stderr, "No PATH variable");
            cleanExit(-1);
        }

        /*
        char string[strlen(internalPrefix) + strlen(path) + 2];
        sprintf(string, "%s:%s", internalPrefix, path);
        */

        // if we can pipe our output
        if (pipeA2[1] > 0) {
            dup2(pipeA2[1], STDOUT_FILENO);    // do it
        }
        // if we can get our input from a pipe
        if (inPipe > 0) {
            dup2(inPipe, STDIN_FILENO);    // do it
        }

        // we don't need any pipe fd anymore, because we already dup'd them. So close them all
        while (cL->first != NULL) {
            struct list_elem *cC = cL->first;
            close(*((int *) cC->data));
            free(cC->data);
            list_remove(cL, cC);
        }

        int fd = -1;

        // if we can output to a file
        if (*outFileP != NULL && (fd = open(*outFileP, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) >= 0) {
            dup2(fd, STDOUT_FILENO);    // do it
            close(fd);
        }
        // if we can get input from a file
        if (*inFileP != NULL && (fd = open(*inFileP, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) >= 0) {
            dup2(fd, STDIN_FILENO);    // do it
            close(fd);
        }

        // if the command contains a slash -> execute as is
        if (strchr(argv[0], '/') != NULL) {
            execCmd(argv[0], argv, envp);
        } // if not -> search for executable in PATH
        else {

            //try every PATH entry
            ptr = strtok(string, delimiter);

            while (ptr != NULL) {
                char cmd[strlen(ptr) + strlen(argv[0]) + 2];
                sprintf(cmd, "%s/%s", ptr, argv[0]);

                execCmd(cmd, argv, envp);

                // naechsten Abschnitt erstellen
                ptr = strtok(NULL, delimiter);
            }

            // if a valid executable was found execCmd (execve) doesn't return
            fprintf(stderr, "Command '%s' not found\n", argv[0]);
            cleanExit(-1);
        }
    } else {
        // add the child command process to the 'tread' list (maybe it's task list)
        int *cpidP = malloc(sizeof(int));
        *cpidP = cpid;
        list_append(tL, cpidP);
    }

    return 0;
}

int main(int argc, char *argv[], char *envp[]) {

    // generate the internalPrefix
    // obsolete
    // TODO: remove, cleanup
    srand(time(NULL));
    sprintf(internalPrefix, internalPrefixFormat, rand() % 900000 + 100000);

    // input buffer
    char str[4096];

    tL = list_init();
    cL = list_init();
    paras = list_init();

    // set the current working dir of our shell (pwd) to the current working dir of the process
    pwd = calloc(4096, sizeof(char));
    if (getcwd(pwd, 4096) == NULL) {
        // if not found, just asume we're in th root dir
        sprintf(pwd, "/");
    }

    // enable non-canonical input mode
    io_open();

    printf("%s $ ", pwd);
    // because we didn't print a new line the our printf might not have been actually printed
    fflush(stdout);
    while (io_fgets(str, 4096, stdin) != NULL) {
        // if the command is "exit" -> exit the shell
        if (strcmp(str, "exit\n") == 0) {
            io_close();
            cleanExit(0);
        }

        // parse the input string to a list of params
        if (paras != NULL) {
            myParse(paras, str, envp);
        }

        // expand all wildcards
        expandWildcards(paras);

        // process the command
        processCmd(tL, paras, envp, -1, cL);

        // close all pipe fds (because we only closed them in the child processes)
        while (cL->first != NULL) {
            struct list_elem *cC = cL->first;
            close(*((int *) cC->data));
            free(cC->data);
            list_remove(cL, cC);
        }

        // wait for all processes to finish
        int cpid;
        while (tL->first != NULL) {
            cpid = wait(NULL);
            list_remove(tL, list_find(tL, &cpid, intPcmp));
        }

        fprintf(stdout, "%s $ ", pwd);
        fflush(stdout);

    }
    // disable non-canonical input mode
    io_close();
}
