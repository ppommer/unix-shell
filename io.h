#ifndef CLIONPROJ_IO_H
#define CLIONPROJ_IO_H

#include <termios.h>
#include <unistd.h>
#include <stdio.h>

// enables non-canonical (raw) input mode
int io_open();

// disables non-canonical (raw) input mode
int io_close();

// if non-canonical input mode is disabled, just passes to fgets
// else it processes ech character by itself and tab-completes
char *io_fgets(char *str, int n, FILE *stream);


#endif //CLIONPROJ_IO_H
