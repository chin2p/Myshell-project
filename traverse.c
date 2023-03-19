// enable telldir(), seekdir() and DT_DIR when using glibc
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>



void traverse(char *dname)
{
    struct dirent *de;
    long offset;
    int flen;
    int dlen = strlen(dname);
    char *pname;
    DIR *dp = opendir(dname);
    if (!dp) {
        perror(dname);
        return;
    }

    printf("Traversing %s\n", dname);
    while ((de = readdir(dp))) {
        printf("%s/%s %d\n", dname, de->d_name, de->d_type);
        if (de->d_type == DT_DIR && de->d_name[0] != '.') {
            // construct new path
            flen = strlen(de->d_name);
            pname = malloc(dlen + flen + 2);
            memcpy(pname, dname, dlen);
            pname[dlen] = '/';
            memcpy(pname + dlen + 1, de->d_name, flen);
            pname[dlen + 1 + flen] = '\0';

            // save location in directory
            offset = telldir(dp);
            closedir(dp);

            // recursively traverse subdirectory
            traverse(pname);
            free(pname);

            // restore position in directory
            dp = opendir(dname);   // FIXME: check for success
            seekdir(dp, offset);
        }
    }

    closedir(dp);
}

int main(int argc, char **argv)
{
    char *dname = (argc < 2)? "." : argv[1];

    traverse(dname);

    return EXIT_SUCCESS;
}
