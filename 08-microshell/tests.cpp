#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>

extern "C" int microshell_main(int argc, char *argv[]);

class MicroShellTest : public ::testing::Test {
protected:
    std::string prompt;
    char original_cwd[PATH_MAX];

    void SetUp() override {
        ASSERT_NE(getcwd(original_cwd, sizeof(original_cwd)), nullptr);
        // Determine the shell prompt by pressing Enter twice
        prompt = determine_shell_prompt();
    }

    void TearDown() override {
        chdir(original_cwd);
    }

    std::vector<pid_t> get_child_processes() {
        std::vector<pid_t> child_pids;
        DIR *dir;
        struct dirent *ent;
        char path[1024];
        FILE *fp;
        pid_t pid;

        if ((dir = opendir("/proc")) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                if (isdigit(ent->d_name[0])) {
                    pid = atoi(ent->d_name);
                    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
                    fp = fopen(path, "r");
                    if (fp) {
                        char comm[256];
                        char state;
                        pid_t ppid;
                        fscanf(fp, "%d %s %c %d", &pid, comm, &state, &ppid);
                        fclose(fp);
                        if (ppid == getpid()) {
                            child_pids.push_back(pid);
                        }
                    }
                }
            }
            closedir(dir);
        }
        return child_pids;
    }

    void assert_no_remaining_processes() {
        auto child_pids = get_child_processes();
        ASSERT_TRUE(child_pids.empty()) << "There are remaining child processes: " << child_pids.size();
    }

    std::pair<std::string, int> run_shell_command(const std::string &input) {
        int pipefd[2];
        pid_t pid;
        char buffer[1024];
        std::string output;
        int status;

        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Child process
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
            dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe
            close(pipefd[1]);

            // Simulate user input
            FILE *input_file = fopen("/tmp/input.txt", "w");
            fprintf(input_file, "%s\n", input.c_str());
            fclose(input_file);
            freopen("/tmp/input.txt", "r", stdin);

            char *argv[] = {(char*)"./micro_shell", NULL};
            int status = microshell_main(1, argv);

            assert_no_remaining_processes();
            exit(status);
        } else { // Parent process
            close(pipefd[1]); // Close unused write end
            wait(&status); // Wait for child process to finish

            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                output += buffer;
            }
            close(pipefd[0]);
        }

        std::cout << output;

        return std::make_pair(output, WEXITSTATUS(status));
    }

    std::string determine_shell_prompt() {
        int pipefd[2];
        pid_t pid;
        char buffer[1024];
        std::string output;

        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Child process
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
            dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe
            close(pipefd[1]);

            // Run the shell without any input
            char *argv[] = {(char*)"./micro_shell", NULL};
            exit(microshell_main(1, argv));
        } else { // Parent process
            close(pipefd[1]); // Close unused write end

            // Sleep for a short time to allow the shell to print the prompt
            usleep(100000);

            // Kill the child process
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);

            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                output += buffer;
            }
            close(pipefd[0]);
        }

        std::cout << "Detected prompt: \"" << output << "\"" << std::endl;

        return output;
    }
};

class MicroShellEchoTest : public MicroShellTest {
};

class MicroShellPwdTest : public MicroShellTest {
};

class MicroShellCdTest : public MicroShellTest {
};

class MicroShellExtCmdTest : public MicroShellTest {
};

class MicroShellMixTest : public MicroShellTest {
};

class MicroShellVariables : public MicroShellTest {
};

class MicroShellIORedirection : public MicroShellTest {
};

TEST_F(MicroShellTest, PressEnterWithoutCommand) {
    std::string input = "\n\n";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    std::string expected_output = prompt + prompt + prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellTest, HandleMultipleSpaces) {
    std::string input = "echo    Hello     World!";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "Hello World!\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellTest, ExitCommand) {
    std::string input = "exit";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "Good Bye\n";
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellTest, InvalidCommand) {
    std::string input = "IamNotACommand_abcdefxyz";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "IamNotACommand_abcdefxyz: command not found\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "When the shell exits, it should return the status of the last command. In this test, the last command failed.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellEchoTest, EchoCommand) {
    std::string input = "echo Hello, World!";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "Hello, World!\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellEchoTest, LargeNumberOfConsecutiveCommands) {
    std::string input;
    std::string expected_output;
    for (int i = 0; i < 100; ++i) {
        input += "echo Command " + std::to_string(i) + "\n";
        expected_output += prompt + "Command " + std::to_string(i) + "\n";
    }
    expected_output += prompt;
    expected_output += prompt;

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellEchoTest, EchoWithLargeTextWithoutSpaces) {
    std::string large_text(10000, 'a'); // Create a string with 10,000 'a' characters
    std::string input = "echo " + large_text;
    std::string expected_output = prompt + large_text + "\n" + prompt;

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellEchoTest, EchoWithLargeNumberOfArguments) {
    std::string input = "echo";
    std::string expected_output = prompt;
    for (int i = 0; i < 1000; ++i) {
        input += " arg" + std::to_string(i);
        expected_output += "arg" + std::to_string(i);
        if (i < 999) {
            expected_output += " ";
        }
    }
    expected_output += "\n" + prompt;

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellPwdTest, PwdCurrentDirectory) {
    std::string input = "pwd";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    std::string expected_output = prompt + std::string(expected_cwd) + "\n" + prompt;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellPwdTest, PwdChangeToParentDirectory) {
    ASSERT_EQ(chdir(".."), 0);

    std::string input = "pwd";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    std::string expected_output = prompt + std::string(expected_cwd) + "\n" + prompt;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellPwdTest, PwdChangeDirectory) {
    const char *new_dir = "/tmp";
    ASSERT_EQ(chdir(new_dir), 0);
    std::string input = "pwd";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    std::string expected_output = prompt + std::string(expected_cwd) + "\n" + prompt;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellPwdTest, PwdChangeToNewDirectory) {
    const char *new_dir = "test_dir";
    ASSERT_EQ(mkdir(new_dir, 0755), 0);
    ASSERT_EQ(chdir(new_dir), 0);

    std::string input = "pwd";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    std::string expected_output = prompt + std::string(expected_cwd) + "\n" + prompt;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Clean up
    chdir("..");
    rmdir(new_dir);
}

TEST_F(MicroShellPwdTest, PwdLongPath) {
    const char *base_dir = "long_path_test";
    ASSERT_EQ(mkdir(base_dir, 0755), 0);
    ASSERT_EQ(chdir(base_dir), 0);

    // Create a long path by nesting directories
    char long_path[PATH_MAX] = "";
    for (int i = 0; i < 100; ++i) {
        char dir_name[16];
        snprintf(dir_name, sizeof(dir_name), "dir_%d", i);
        ASSERT_EQ(mkdir(dir_name, 0755), 0);
        ASSERT_EQ(chdir(dir_name), 0);
        strcat(long_path, "/");
        strcat(long_path, dir_name);
    }

    std::string input = "pwd";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    std::string expected_output = prompt + std::string(expected_cwd) + "\n" + prompt;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Clean up
    for (int i = 99; i >= 0; --i) {
        ASSERT_EQ(chdir(".."), 0);
        char dir_name[16];
        snprintf(dir_name, sizeof(dir_name), "dir_%d", i);
        ASSERT_EQ(rmdir(dir_name), 0);
    }
    chdir("..");
    rmdir(base_dir);
}

TEST_F(MicroShellCdTest, CdCommand) {
    std::string input = "cd /tmp\npwd";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + "/tmp\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellCdTest, InvalidCdCommand) {
    std::string input = "cd /invalid_directory";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "cd: /invalid_directory: No such file or directory\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "When the shell exits, it should return the status of the last command. In this test, the last command failed.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellExtCmdTest, ExternalCommand) {
    std::string input = "cat /tmp/input.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "cat /tmp/input.txt" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_NE(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellExtCmdTest, ExternalCommandWithLargeNumberOfArguments) {
    std::string input = "cat /tmp/input.txt";
    for (int i = 0; i < 50; ++i) {
        input += "  /tmp/input.txt";
    }

    std::string expected_output = prompt;
    for (int i = 0; i < 51; ++i) {
        expected_output += input;
        if (i < 999) {
            expected_output += "\n";
        }
    }
    expected_output += prompt;

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellMixTest, CreateRemoveDir) {
    const char *new_dir = "test_dir";
    std::string input;
    std::string expected_output;

    // Prepare the sequence of commands to be executed in the shell
    input = "mkdir " + std::string(new_dir) + "\n";
    input += "cd " + std::string(new_dir) + "\n";
    input += "pwd\n";
    input += "cd ..\n";
    input += "rmdir " + std::string(new_dir);

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    // Get the expected current working directory after changing to the new directory
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    strcat(expected_cwd, "/");
    strcat(expected_cwd, new_dir);

    // Prepare the expected output
    expected_output = prompt + prompt + prompt + std::string(expected_cwd) + "\n" + prompt + prompt + prompt;

    // Run the sequence of commands in the shell
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    // Verify the output and status
    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify that the directory was removed
    struct stat info;
    ASSERT_NE(stat(new_dir, &info), 0) << "Directory " << new_dir << " was not removed.";
}

TEST_F(MicroShellVariables, VariableAssignment) {
    std::string input = "you_are=a_programmer";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellVariables, VariableSubstitution) {
    std::string input = "you_are=a_good_programmer\necho $you_are";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + "a_good_programmer\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellVariables, VariableSubstitutionInPath) {
    std::string input = "filename=input.txt\ncat /tmp/$filename";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + "filename=input.txt\ncat /tmp/$filename\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellVariables, NonExistingVariableSubstitution) {
    std::string input = "echo $non_existing_var";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellVariables, NonExistingVariableSubstitutionInString) {
    std::string input = "echo Hello$non_existing_var World!";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "Hello World!\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellVariables, ExportVariable) {
    std::string input = "Linux_system_programming=FunForEver\nexport Linux_system_programming\nprintenv";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_NE(output.find("Linux_system_programming=FunForEver"), std::string::npos) << "Expected environment variable x=5 not found.";
}

TEST_F(MicroShellVariables, CreateRemoveDirWithVars) {
    const char *new_dir = "test_vars_dir";
    std::string input;
    std::string expected_output;

    // Prepare the sequence of commands to be executed in the shell
    input = "MKDIR=mkdir\n";
    input += "DIR_NAME=" + std::string(new_dir) + "\n";
    input += "$MKDIR $DIR_NAME\n";
    input += "cd $DIR_NAME\n";
    input += "pwd\n";
    input += "cd ..\n";
    input += "rmdir $DIR_NAME";

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    // Get the expected current working directory after changing to the new directory
    char expected_cwd[PATH_MAX];
    ASSERT_NE(getcwd(expected_cwd, sizeof(expected_cwd)), nullptr);
    strcat(expected_cwd, "/");
    strcat(expected_cwd, new_dir);

    // Prepare the expected output
    expected_output = prompt+ prompt + prompt + prompt + prompt + std::string(expected_cwd) + "\n" + prompt + prompt + prompt;

    // Run the sequence of commands in the shell
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    // Verify the output and status
    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify that the directory was removed
    struct stat info;
    ASSERT_NE(stat(new_dir, &info), 0) << "Directory " << new_dir << " was not removed.";
}

TEST_F(MicroShellVariables, EchoWithLargeNumberOfArgumentsWithVars) {
    std::string echo_cmd = "echo";
    std::string input = "";
    std::string expected_output = prompt;
    std::string echo_expected_output = "";
    for (int i = 0; i < 1000; ++i) {
        input += "var" + std::to_string(i) + "=value" + std::to_string(i) + "\n";
        echo_cmd += " $var" + std::to_string(i);
        
        expected_output += prompt;
        echo_expected_output += "value" + std::to_string(i);
        if (i < 999) {
            echo_expected_output += " ";
        }
    }
    input += echo_cmd;
    expected_output += echo_expected_output + "\n" + prompt;

    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;

    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellIORedirection, RedirectOutputToFile) {
    remove("/tmp/output.txt");

    std::string input = "echo Hello, World! > /tmp/output.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify the content of the output file
    std::ifstream file("/tmp/output.txt");
    std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    ASSERT_EQ(file_content, "Hello, World!\n") << "File content is not correct. Expected: Hello, World!\n, got: " << file_content;
}

TEST_F(MicroShellIORedirection, RedirectInputFromFile) {
    // Prepare the input file
    remove("/tmp/input_iored.txt");
    std::ofstream file("/tmp/input_iored.txt");
    file << "Hello, World!";
    file.close();

    std::string input = "echo Hello\ncat < /tmp/input_iored.txt\necho World!";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "Hello\n" + prompt + "Hello, World!" + prompt + "World!\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellIORedirection, RedirectErrorToFile) {
    remove("/tmp/error.txt");

    std::string input = "ls non_existing_file_no_redirect\nls non_existing_file 2> /tmp/error.txt\nls non_existing_file_no_redirect_after";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "ls: cannot access 'non_existing_file_no_redirect': No such file or directory\n" + prompt + prompt + "ls: cannot access 'non_existing_file_no_redirect_after': No such file or directory\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "When the shell exits, it should return the status of the last command. In this test, the last command failed.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify the content of the error file
    std::ifstream file("/tmp/error.txt");
    std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    ASSERT_EQ(file_content, "ls: cannot access 'non_existing_file': No such file or directory\n") << "Error message not found in file.";
}

TEST_F(MicroShellIORedirection, RedirectOutputAndErrorToFile) {
    remove("/tmp/output.txt");
    remove("/tmp/error.txt");

    std::string input = "ls non_existing_file_o_e > /tmp/output.txt 2> /tmp/error.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "When the shell exits, it should return the status of the last command. In this test, the last command failed.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify the content of the output file
    std::ifstream output_file("/tmp/output.txt");
    std::string output_file_content((std::istreambuf_iterator<char>(output_file)), std::istreambuf_iterator<char>());
    output_file.close();
    ASSERT_EQ(output_file_content, "") << "Output file should be empty.";

    // Verify the content of the error file
    std::ifstream error_file("/tmp/error.txt");
    std::string error_file_content((std::istreambuf_iterator<char>(error_file)), std::istreambuf_iterator<char>());
    error_file.close();
    ASSERT_EQ(error_file_content, "ls: cannot access 'non_existing_file_o_e': No such file or directory\n") << "Error message not found in file.";
}

TEST_F(MicroShellIORedirection, RedirectOutputWithVariableSubstitution) {
    remove("/tmp/output_var.txt");

    std::string input = "filename=output_var.txt\necho Hello, World! > /tmp/$filename";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify the content of the output file
    std::ifstream file("/tmp/output_var.txt");
    std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    ASSERT_EQ(file_content, "Hello, World!\n") << "File content is not correct. Expected: Hello, World!\n, got: " << file_content;
}

TEST_F(MicroShellIORedirection, RedirectInputWithVariableSubstitution) {
    // Prepare the input file
    remove("/tmp/input_var.txt");
    std::ofstream file("/tmp/input_var.txt");
    file << "Hello, World!";
    file.close();

    std::string input = "filename=input_var.txt\ncat < /tmp/$filename";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + "Hello, World!" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "The shell main function should return 0 on success.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellIORedirection, RedirectErrorWithVariableSubstitution) {
    remove("/tmp/error_var.txt");

    std::string input = "filename=error_var.txt\nls non_existing_file_var 2> /tmp/$filename";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "When the shell exits, it should return the status of the last command. In this test, the last command failed.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;

    // Verify the content of the error file
    std::ifstream file("/tmp/error_var.txt");
    std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    ASSERT_EQ(file_content, "ls: cannot access 'non_existing_file_var': No such file or directory\n") << "Error message not found in file.";
}

TEST_F(MicroShellIORedirection, RedirectOutputToNonWritableLocation) {
    std::string input = "echo Hello, World! > /root/output.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "/root/output.txt: Permission denied\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "The shell main function should return non-zero on failure.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellIORedirection, RedirectInputFromNonExistentFile) {
    std::string input = "cat < /tmp/non_existent_file.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "cannot access /tmp/non_existent_file.txt: No such file or directory\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "The shell main function should return non-zero on failure.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(MicroShellIORedirection, RedirectionOrder1) {
    remove("/tmp/output.txt");
    remove("/tmp/err.txt");

    std::string input = "cat < /tmp/non_existent_file.txt > /tmp/output.txt 2> /tmp/err.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "cannot access /tmp/non_existent_file.txt: No such file or directory\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "The shell main function should return non-zero on failure.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
    
    // Verify the content of the output file
    std::ifstream output_file("/tmp/output.txt");
    std::string output_file_content((std::istreambuf_iterator<char>(output_file)), std::istreambuf_iterator<char>());
    output_file.close();
    ASSERT_EQ(output_file_content, "") << "Output file should be empty.";
    
    // Verify the content of the error file
    std::ifstream err_file("/tmp/err.txt");
    std::string err_file_content((std::istreambuf_iterator<char>(err_file)), std::istreambuf_iterator<char>());
    err_file.close();
    ASSERT_EQ(err_file_content, "") << "Error file should be empty as the error happened before error redirection is done.";
}

TEST_F(MicroShellIORedirection, RedirectionOrder2) {
    remove("/tmp/output.txt");
    remove("/tmp/err.txt");

    std::string input = "cat > /tmp/output.txt 2> /tmp/err.txt < /tmp/non_existent_file.txt";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "The shell main function should return non-zero on failure.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
    
    // Verify the content of the output file
    std::ifstream output_file("/tmp/output.txt");
    std::string output_file_content((std::istreambuf_iterator<char>(output_file)), std::istreambuf_iterator<char>());
    output_file.close();
    ASSERT_EQ(output_file_content, "") << "Output file should be empty.";
    
    // Verify the content of the error file
    std::ifstream err_file("/tmp/err.txt");
    std::string err_file_content((std::istreambuf_iterator<char>(err_file)), std::istreambuf_iterator<char>());
    err_file.close();
    ASSERT_EQ(err_file_content, "cannot access /tmp/non_existent_file.txt: No such file or directory\n") << "Error file should contain the input redirection error message as error redirection happened first.";
}
