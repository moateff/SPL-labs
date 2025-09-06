#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>

#define MAXINPUT 10240
#define MAXTOKENS 1024
#define MAXPATH 4096

typedef enum {
    CMD_EXIT,
    CMD_CD,
    CMD_PWD,
    CMD_ECHO,
    CMD_UNKNOWN
} CommandType;

CommandType get_command_type(const char *cmd) {
    if (strcmp(cmd, "exit") == 0) return CMD_EXIT;
    if (strcmp(cmd, "cd") == 0)   return CMD_CD;
    if (strcmp(cmd, "pwd") == 0)  return CMD_PWD;
    if (strcmp(cmd, "echo") == 0) return CMD_ECHO;
    return CMD_UNKNOWN;
}

int read_input(char *buffer, size_t size) {
    printf("picoshell$ "); 
    fflush(stdout);

    if (fgets(buffer, size, stdin) == NULL) {
        return 1;  // EOF or error
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    return 0;
}

int tokenize(char *input, char *tokens[]) {
    int count = 0;
    char *token = strtok(input, " \t");

    while (token != NULL && count < MAXTOKENS - 1) {
        tokens[count++] = token;
        token = strtok(NULL, " \t");
    }

    tokens[count] = NULL; 
    return count;
}

int execute_exit(int argc, char *argv[]) {
    int status = 0;

    if (argc > 1) {
        status = atoi(argv[1]);  
    }

    printf("Good Bye\n"); 
    exit(status);
}

int execute_cd(int argc, char *argv[]) {
    if (argc < 2) {
        printf("cd: missing argument\n");
        return 1;
    }

    if (chdir(argv[1]) != 0) {
        fprintf(stderr, "cd: /invalid_directory: No such file or directory\n");
        return 1;
    }

    return 0;
}

int execute_pwd(int argc, char *argv[])
{
    char cwd[MAXPATH];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;
    }

    return 1;
}

int execute_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

int execute_program(int argc, char *argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;   
    }

    if (pid == 0) {
        // Child process
        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        exit(127);  
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return 1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);  // child exit code
        } else if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);  // killed by signal
        } else {
            return 1;  // unknown failure
        }
    }
}

int execute_command(int argc, char *argv[]) {
    if (argc == 0) return 1;

    int status = 0;

    switch (get_command_type(argv[0])) {
        case CMD_EXIT:
            status = execute_exit(argc, argv);
            break;
        case CMD_CD:
            status = execute_cd(argc, argv);
            break;
        case CMD_PWD:
            status = execute_pwd(argc, argv);
            break;
        case CMD_ECHO:
            status = execute_echo(argc, argv);
            break;
        default:
            status = execute_program(argc, argv);
            break;
    }

    return status;
}

int picoshell_main(int argc, char *argv[]) {
    char input[MAXINPUT];
    char *tokens[MAXTOKENS];
    int ntokens;
    int status = 0;

    while (1) { 
        if (read_input(input, MAXINPUT)) break;
        if (strlen(input) == 0) continue;
        ntokens = tokenize(input, tokens);
        status = execute_command(ntokens, tokens);
    }

    return status;
}


// int main(int argc, char *argv[]) {
//     return picoshell_main(argc, argv);
// }
