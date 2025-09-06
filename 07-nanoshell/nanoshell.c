#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <linux/limits.h>

#define MAXBUFF 10240
#define MAXTOKENS 1024
#define MAXPATH 4096

extern char **environ;

typedef struct Variable {
    char *name;
    char *value;
    int exported;  
    struct Variable *next;
} Variable;

Variable *var_list = NULL;  // global linked list head

void set_variable(const char *name, const char *value, int exported) {
    Variable *curr = var_list;

    // check if variable already exists
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = strdup(value);
            if (exported) curr->exported = 1; 
            if (curr->exported) setenv(name, value, 1);
            return;
        }
        curr = curr->next;
    }

    // create new node
    Variable *new_var = (Variable *)malloc(sizeof(Variable));
    new_var->name = strdup(name);
    new_var->value = strdup(value);
    new_var->exported = exported;
    new_var->next = var_list;
    var_list = new_var;

    if (exported) {
        setenv(name, value, 1);
    }
}

const char *get_variable(const char *name) {
    Variable *curr = var_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr->value;
        }
        curr = curr->next;
    }
    return NULL; 
}

int export_variable(const char *name) {
    Variable *curr = var_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->exported = 1;
            setenv(curr->name, curr->value, 1);
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

char *substitute_variable(const char *arg) {
    size_t len = strlen(arg);
    char *result = (char *)malloc(1);
    result[0] = '\0';
    size_t res_len = 0;

    for (size_t i = 0; i < len; ) {
        if (arg[i] == '$') {
            i++; // skip '$'

            // collect variable name
            size_t start = i;
            while (i < len && (isalnum((unsigned char)arg[i]) || arg[i] == '_')) {
                i++;
            }
            size_t var_len = i - start;

            if (var_len > 0) {
                char varname[256];
                strncpy(varname, arg + start, var_len);
                varname[var_len] = '\0';

                const char *val = get_variable(varname);
                if (!val) val = ""; // missing → empty

                size_t val_len = strlen(val);
                result = (char *)realloc(result, res_len + val_len + 1);
                strcpy(result + res_len, val);
                res_len += val_len;
            } else {
                // just a standalone '$'
                result = (char *)realloc(result, res_len + 2);
                result[res_len++] = '$';
                result[res_len] = '\0';
            }
        } else {
            // copy normal character
            result = (char *)realloc(result, res_len + 2);
            result[res_len++] = arg[i++];
            result[res_len] = '\0';
        }
    }

    return result;
}

void substitute_args(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        char *new_arg = substitute_variable(argv[i]);
        // free(argv[i]);       
        argv[i] = new_arg;   // replace with substituted one
    }
}

void free_variables() {
    Variable *curr = var_list;
    while (curr) {
        Variable *tmp = curr;
        curr = curr->next;
        free(tmp->name);
        free(tmp->value);
        free(tmp);
    }
    var_list = NULL;
}

typedef enum {
    BUILTIN_CMD,
    SETVAR_CMD,
    PROGRAM_CMD
} CommandType;

typedef enum {
    CMD_EXIT,
    CMD_CD,
    CMD_PWD,
    CMD_ECHO,
    CMD_EXPORT,
    CMD_PRINTENV
} BuiltInType;

CommandType get_command_type(int argc, char *argv[]) 
{
    if (strcmp(argv[0], "exit") == 0 ||
        strcmp(argv[0], "cd") == 0 ||
        strcmp(argv[0], "pwd") == 0 ||
        strcmp(argv[0], "echo") == 0 ||
        strcmp(argv[0], "export") == 0 ||
        strcmp(argv[0], "printenv") == 0) {
        return BUILTIN_CMD;
    }

    for (int i = 0; i < argc; i++){
        if (strchr(argv[i], '=')) 
            return SETVAR_CMD;
    }

    return PROGRAM_CMD;
}

BuiltInType get_builtin_command_type(const char *cmd) 
{
    if (strcmp(cmd, "exit") == 0)     return CMD_EXIT;
    if (strcmp(cmd, "cd") == 0)       return CMD_CD;
    if (strcmp(cmd, "pwd") == 0)      return CMD_PWD;
    if (strcmp(cmd, "echo") == 0)     return CMD_ECHO;
    if (strcmp(cmd, "export") == 0)   return CMD_EXPORT;
    if (strcmp(cmd, "printenv") == 0) return CMD_PRINTENV;
}

int read_input(char *buffer, size_t size) 
{
    printf("nanoshell$ "); 
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

int tokenize(char *input, char *tokens[]) 
{
    int count = 0;
    char *token = strtok(input, " \t");

    while (token != NULL && count < MAXTOKENS - 1) {
        tokens[count++] = token;
        token = strtok(NULL, " \t");
    }

    tokens[count] = NULL; 
    return count;
}

int execute_exit(int argc, char *argv[]) 
{
    int status = 0;

    if (argc > 1) {
        status = atoi(argv[1]);  
    }

    printf("Good Bye\n"); 
    exit(status);
}

int execute_cd(int argc, char *argv[]) 
{
    if (argc > 2) {
        printf("cd: too many argument\n");
        return 1;
    }

    if ((argc == 2) & (chdir(argv[1]) != 0)) {
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

int execute_export(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "export: missing argument\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (export_variable(argv[i]) == 0) {
            fprintf(stderr, "export: %s not found\n", argv[i]);
            continue;
        }
    }

    return 0;
}

int execute_printenv(int argc, char **argv) {
    if (argc > 1) {
        fprintf(stderr, "printenv: this command takes no arguments\n");
        return 1;
    }

    for (char **env = environ; *env != NULL; env++) {
        printf("%s\n", *env);
    }
    return 0;
}

int parse_assignment(const char *arg, char **name, char **value) {
    char *eq = (char *)strchr(arg, '=');
    if (!eq || eq == arg) return 0; // missing '=' or empty name

    size_t name_len = eq - arg;
    *name = strndup(arg, name_len);
    *value = strdup(eq + 1); // value can be empty
    return 1;
}

int execute_setvar_command(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(stderr, "Invalid command\n");
        return 1;
    }

    char *name = NULL;
    char *value = NULL;

    if (!parse_assignment(argv[0], &name, &value)) {
        fprintf(stderr, "Invalid command\n");
        return 1;
    }

    // set as local variable (exported = 0)
    set_variable(name, value, 0);

    free(name);
    free(value);

    return 0; // success
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

int execute_builtin_command(int argc, char *argv[]) {
    int status = 0;

    switch (get_builtin_command_type(argv[0])) {
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
        case CMD_EXPORT:
            status = execute_export(argc, argv);
            break;
        case CMD_PRINTENV:
            status = execute_printenv(argc, argv);
            break;
    }

    return status;
}

int execute_command(int argc, char *argv[]) {
    int status = 0;

    if (argc == 0) return status;

    substitute_args(argc, argv);
    /*
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++){
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    */
    switch (get_command_type(argc, argv)) {
        case BUILTIN_CMD:
            status = execute_builtin_command(argc, argv);
            break;
        case SETVAR_CMD:
            status = execute_setvar_command(argc, argv);
            break;
        case PROGRAM_CMD:
            status = execute_program(argc, argv);
            break;
    }

    return status;
}

int nanoshell_main(int argc, char *argv[]) {
    char buffer[MAXBUFF];
    char *tokens[MAXTOKENS];
    int ntokens;
    int status = 0;
    while (read_input(buffer, MAXBUFF) == 0) { 
        if (strlen(buffer) == 0) continue;
        ntokens = tokenize(buffer, tokens);
        /*
        printf("ntokens = %d\n", ntokens);
        for (int i = 0; i < ntokens; i++){
            printf("token[%d] = %s\n", i, tokens[i]);
        }
        */
        status = execute_command(ntokens, tokens);
    }

    return status;
}

/*
int main(int argc, char *argv[]) {
    return nanoshell_main(argc, argv);
}
*/