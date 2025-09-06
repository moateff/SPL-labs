#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define PATH_MAX 4096

int pwd_main()
{
    char cwd[PATH_MAX];

    // Get current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        size_t len = strlen(cwd);
        ssize_t ret = write(STDOUT_FILENO, cwd, len);
        if (ret != (ssize_t) len) {
            perror("write");
            return 1;
        }

        ret = write(STDOUT_FILENO, "\n", 1);
        if (ret != 1) {
            perror("write");
            return 1;
        }

        return 0;
    } else {
        perror("getcwd");
        return 1;
    }
}
