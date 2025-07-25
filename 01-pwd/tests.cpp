#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

extern "C" int pwd_main();

namespace {

class PwdTest : public ::testing::Test {
protected:
    char original_cwd[PATH_MAX];

    void SetUp() override {
        ASSERT_NE(getcwd(original_cwd, sizeof(original_cwd)), nullptr);
    }

    void TearDown() override {
        chdir(original_cwd);
    }

    std::pair<std::string, int> run_pwd_command() {
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

            // Run the student's pwd_main function
            exit(pwd_main());
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

        return {output, WEXITSTATUS(status)};
    }
};

TEST_F(PwdTest, CurrentDirectory) {
    char expected[PATH_MAX];
    ASSERT_NE(getcwd(expected, sizeof(expected)), nullptr);

    auto result_status = run_pwd_command();
    std::string result = result_status.first;
    int status = result_status.second;

    // Remove the newline character from the result
    result.erase(result.find_last_not_of("\n") + 1);

    ASSERT_EQ(status, 0) << "pwd should return 0 on success.";
    ASSERT_STREQ(expected, result.c_str()) << "Current Directory is not correct. Expected: " << expected << ", got: " << result.c_str();
}

TEST_F(PwdTest, ChangeDirectory) {
    const char *new_dir = "/tmp";
    ASSERT_EQ(chdir(new_dir), 0);

    char expected[PATH_MAX];
    ASSERT_NE(getcwd(expected, sizeof(expected)), nullptr);

    auto result_status = run_pwd_command();
    std::string result = result_status.first;
    int status = result_status.second;

    // Remove the newline character from the result
    result.erase(result.find_last_not_of("\n") + 1);

    ASSERT_EQ(status, 0) << "pwd should return 0 on success.";
    ASSERT_STREQ(expected, result.c_str()) << "Current Directory is not correct. Expected: " << expected << ", got: " << result.c_str();
}

TEST_F(PwdTest, ChangeToParentDirectory) {
    ASSERT_EQ(chdir(".."), 0);

    char expected[PATH_MAX];
    ASSERT_NE(getcwd(expected, sizeof(expected)), nullptr);

    auto result_status = run_pwd_command();
    std::string result = result_status.first;
    int status = result_status.second;

    // Remove the newline character from the result
    result.erase(result.find_last_not_of("\n") + 1);

    ASSERT_EQ(status, 0) << "pwd should return 0 on success.";
    ASSERT_STREQ(expected, result.c_str()) << "Current Directory is not correct. Expected: " << expected << ", got: " << result.c_str();
}

TEST_F(PwdTest, ChangeToNewDirectory) {
    const char *new_dir = "test_dir";
    ASSERT_EQ(mkdir(new_dir, 0755), 0);
    ASSERT_EQ(chdir(new_dir), 0);

    char expected[PATH_MAX];
    ASSERT_NE(getcwd(expected, sizeof(expected)), nullptr);

    auto result_status = run_pwd_command();
    std::string result = result_status.first;
    int status = result_status.second;

    // Remove the newline character from the result
    result.erase(result.find_last_not_of("\n") + 1);

    ASSERT_EQ(status, 0) << "pwd should return 0 on success.";
    ASSERT_STREQ(expected, result.c_str()) << "Current Directory is not correct. Expected: " << expected << ", got: " << result.c_str();

    // Clean up
    chdir("..");
    rmdir(new_dir);
}

TEST_F(PwdTest, LongPath) {
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

    char expected[PATH_MAX];
    ASSERT_NE(getcwd(expected, sizeof(expected)), nullptr);

    auto result_status = run_pwd_command();
    std::string result = result_status.first;
    int status = result_status.second;

    // Remove the newline character from the result
    result.erase(result.find_last_not_of("\n") + 1);

    ASSERT_EQ(status, 0) << "pwd should return 0 on success.";
    ASSERT_STREQ(expected, result.c_str()) << "Current Directory is not correct. Expected: " << expected << ", got: " << result.c_str();

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
}
