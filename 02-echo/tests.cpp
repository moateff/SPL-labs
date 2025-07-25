#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

extern "C" int echo_main(int argc, char *argv[]);

class EchoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Any setup can be done here if needed
    }

    void TearDown() override {
        // Any cleanup can be done here if needed
    }

    std::pair<std::string, int> run_echo_command(int argc, char *argv[]) {
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
            close(pipefd[1]);

            // Run the student's echo_main function
            exit(echo_main(argc, argv));
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
};

TEST_F(EchoTest, PrintString) {
    const char *argv[] = {"echo", "Hello", "World", NULL};
    int argc = 3;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("Hello World\n", result.c_str());
}

TEST_F(EchoTest, NoArguments) {
    const char *argv[] = {"echo", NULL};
    int argc = 1;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("\n", result.c_str()) << "When echo is called with no arguments, it should print newline only.";
}

TEST_F(EchoTest, SpecialCharacters) {
    const char *argv[] = {"echo", "Hello", "World!", "@#$%^&*()", NULL};
    int argc = 4;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("Hello World! @#$%^&*()\n", result.c_str()) << "Test your echo and the linux version with 'Hello World! @#$%^&*()' and compare!";
}

TEST_F(EchoTest, MultipleSpaces) {
    const char *argv[] = {"echo", "Hello", "   ", "World", NULL};
    int argc = 4;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("Hello     World\n", result.c_str()) << "Test your echo with multiple spaces passed as an argument (inside double or single qoutes. For example: echo hello '   ' linux)";
}

TEST_F(EchoTest, EmptyString) {
    const char *argv[] = {"echo", "", NULL};
    int argc = 2;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("\n", result.c_str());
}

TEST_F(EchoTest, NewlineCharacter) {
    const char *argv[] = {"echo", "Hello\nWorld", NULL};
    int argc = 2;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("Hello\nWorld\n", result.c_str());
}

TEST_F(EchoTest, TabCharacter) {
    const char *argv[] = {"echo", "Hello\tWorld", NULL};
    int argc = 2;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ("Hello\tWorld\n", result.c_str());
}

TEST_F(EchoTest, LongString) {
    std::string long_string(10000, 'a'); // Create a string with 10,000 'a' characters
    const char *argv[] = {"echo", long_string.c_str(), NULL};
    int argc = 2;

    auto result_status = run_echo_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    long_string += "\n"; // Append newline character to the expected result
    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ(long_string.c_str(), result.c_str()) << "The echo program should work properly even if the passed string is too long!";
}

TEST_F(EchoTest, LongListOfArgs) {
    const int num_args = 1000;
    char *argv[num_args + 2];
    argv[0] = const_cast<char*>("echo");

    std::string expected_output;
    for (int i = 1; i <= num_args; ++i) {
        std::string arg = "arg" + std::to_string(i);
        argv[i] = new char[arg.size() + 1];
        std::strcpy(argv[i], arg.c_str());
        expected_output += arg;
        if (i < num_args) {
            expected_output += " ";
        }
    }
    argv[num_args + 1] = NULL;
    expected_output += "\n";

    auto result_status = run_echo_command(num_args + 1, argv);
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "echo program should return 0 on success.";
    ASSERT_STREQ(expected_output.c_str(), result.c_str()) << "The echo program should work properly even if the number of the passed arguments is too long!";

    // Clean up
    for (int i = 1; i <= num_args; ++i) {
        delete[] argv[i];
    }
}
