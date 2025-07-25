#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

extern "C" int femtoshell_main(int argc, char *argv[]);

class FemtoShellTest : public ::testing::Test {
protected:
    std::string prompt;

    void SetUp() override {
        // Determine the shell prompt by pressing Enter twice
        prompt = determine_shell_prompt();
    }

    void TearDown() override {
        // Any cleanup can be done here if needed
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
            FILE *input_file = fopen("input.txt", "w");
            fprintf(input_file, "%s\n", input.c_str());
            fclose(input_file);
            freopen("input.txt", "r", stdin);

            char *argv[] = {(char*)"./femtoshell", NULL};
            exit(femtoshell_main(1, argv));
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
            char *argv[] = {(char*)"./femtoshell", NULL};
            exit(femtoshell_main(1, argv));
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

TEST_F(FemtoShellTest, EchoCommand) {
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

TEST_F(FemtoShellTest, ExitCommand) {
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

TEST_F(FemtoShellTest, InvalidCommand) {
    std::string input = "ls -l";
    std::cout << "Test input: \"" << input << "\"" << std::endl;
    std::cout << "Your shell output:" << std::endl;
    std::string expected_output = prompt + "Invalid command\n" + prompt;
    auto result_status = run_shell_command(input);
    std::string output = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "When the shell exits, it should return the status of the last command. In this test, the last command failed.";
    ASSERT_EQ(output, expected_output) << "Expected output: " << expected_output << " but got: " << output;
}

TEST_F(FemtoShellTest, PressEnterWithoutCommand) {
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

TEST_F(FemtoShellTest, LargeNumberOfConsecutiveCommands) {
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

TEST_F(FemtoShellTest, EchoWithLargeTextWithoutSpaces) {
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

TEST_F(FemtoShellTest, EchoWithLargeNumberOfArguments) {
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
