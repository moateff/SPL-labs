#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int echo_main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        ssize_t ret = write(STDOUT_FILENO, argv[i], len);
        if (ret != (ssize_t) len) {
            perror("write");
            return 1;       
        }

        if (i < argc - 1) {
            ret = write(STDOUT_FILENO, " ", 1);
            if (ret != 1) {
                perror("write");
                return 1;
            }
        }
    }

    ssize_t ret = write(STDOUT_FILENO, "\n", 1);
    if (ret != 1) {
        perror("write");
        return 1;
    }

    return 0;
}
