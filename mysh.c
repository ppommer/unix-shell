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

// fake directory name to identify internal commands
char internalPrefixFormat[] = "/>%06d#internal</";

// resulting string whith length:
// snprintf(NULL, 0, internalPrefixFormat, 0) + 1
char internalPrefix[20];

// current working directory as string
char *pwd;

// thread list
list_t *tL;
// pipe descriptor list
list_t *cL;
// list of parameters given to the shell
list_t *paras;

// parser.c function declarations
extern list_t *myParse(list_t *res, char *str, char *envp[]);

extern int myParseStg2(list_t *args, char **outFileP, char **inFileP);

extern int myParsePipe(list_t *args, list_t *args2, int pipeA[]);
// END

// compare two pointers like ints
int intPcmp(const void *intP1, const void *intP2) {
    return (*(int *) intP1) - (*(int *) intP2);
}

// free allocated memory and exit
void cleanExit(int status) {
    list_finit(tL);
    list_finit(cL);
    list_finit(paras);
    free(pwd);
    exit(status);
}

// change the current working directory by the given list of parameters
// e.g. list ["cd"] -> ["a/dir/to/go"]
// the first element of the list should always be the string "cd"
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

        // the new working dir is: pwd + "/" + dir
	// the length must be strlen(pwd) + 1 + strlen(dir) + 1 (null term)
        char newPwd[strlen(pwd) + strlen(dir) + 2];
        sprintf(newPwd, "%s/%s", pwd, dir);

        // get the real path of the new wd (/path/.././path/.. -> /) in other words deuglify it
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
        // nothing has to be freed as only arrays were declared
    } // too many arguments or list is empty
    else {
        // print usage
        printf("Usage: cd <dir>\n");
    }
    return 0;
}

int execCmd(const char *filename, char *const argv[], char *const envp[]) {
    return execve(filename, argv, envp);
}


int processCmd(list_t *tL, list_t *cmdList, char *envp[], int inPipe, list_t *cL) {

    // check if cd is called
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

        // pipe output if possible
        if (pipeA2[1] > 0) {
            dup2(pipeA2[1], STDOUT_FILENO);
        }
        // get input from pipe if possible
        if (inPipe > 0) {
            dup2(inPipe, STDIN_FILENO);
        }

        // close all pipe fds
        while (cL->first != NULL) {
            struct list_elem *cC = cL->first;
            close(*((int *) cC->data));
            free(cC->data);
            list_remove(cL, cC);
        }

        int fd = -1;

        // output to file if possible
        if (*outFileP != NULL && (fd = open(*outFileP, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) >= 0) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        // get input from file if possible
        if (*inFileP != NULL && (fd = open(*inFileP, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) >= 0) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // exec the command as it is if it contains a slash
        if (strchr(argv[0], '/') != NULL) {
            execCmd(argv[0], argv, envp);
        } // if not, search for executable in PATH
        else {

            //try every PATH entry
            ptr = strtok(string, delimiter);

            while (ptr != NULL) {
                char cmd[strlen(ptr) + strlen(argv[0]) + 2];
                sprintf(cmd, "%s/%s", ptr, argv[0]);

                execCmd(cmd, argv, envp);

                ptr = strtok(NULL, delimiter);
            }

            // if a valid executable was found execCmd (execve) doesn't return
            fprintf(stderr, "Command '%s' not found\n", argv[0]);
            cleanExit(-1);
        }
    } else {
        // add the child command process to the task list
        int *cpidP = malloc(sizeof(int));
        *cpidP = cpid;
        list_append(tL, cpidP);
    }

    return 0;
}

int main(int argc, char *argv[], char *envp[]) {

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
        // if not found, assume root dir
        sprintf(pwd, "/");
    }

    // enable non-canonical input mode
    io_open();

    printf("%s $ ", pwd);
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
