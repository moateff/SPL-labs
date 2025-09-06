#include <fcntl.h>
#include <unistd.h>

#define BUFSIZE 1024

int mv_main(int argc, char *argv[])
{

    if (argc != 3) {
        write(STDERR_FILENO, "Usage: mv <source> <destination>\n", 34);
        return 1;
    }

    int fd_src, fd_dest;
    char buffer[BUFSIZE];
    ssize_t bytes_read, bytes_written;

    fd_src = open(argv[1], O_RDONLY);
    if (fd_src < 0) {
        return 1;
    }

    fd_dest = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_dest < 0) {
        close(fd_src);
        return 1;
    }

    while ((bytes_read = read(fd_src, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(fd_dest, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            close(fd_src);
            close(fd_dest);
            return 1;
        }
    }

    if (bytes_read < 0) {
        close(fd_src);
        close(fd_dest);
        return 1;
    }

    close(fd_src);
    close(fd_dest);

    // Remove source file
    unlink(argv[1]);

    return 0;
}
