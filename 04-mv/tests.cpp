#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern "C" int mv_main(int argc, char *argv[]);

class MvTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Any setup can be done here if needed
    }

    void TearDown() override {
        // Any cleanup can be done here if needed
    }

    void create_file(const char *filename, const char *content) {
        FILE *fp = fopen(filename, "w");
        ASSERT_NE(fp, nullptr) << "Failed to create file: " << filename;
        fputs(content, fp);
        fclose(fp);
    }

    std::string read_file(const char *filename) {
        FILE *fp = fopen(filename, "r");
        if (fp == nullptr) {
            ADD_FAILURE() << "Failed to open file: " << filename;
            return "";
        }
        char buffer[1024];
        std::string content;
        while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            content += buffer;
        }
        fclose(fp);
        return content;
    }

    std::pair<std::string, int> run_mv_command(int argc, char *argv[]) {
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

            // Run the student's mv_main function
            exit(mv_main(argc, argv));
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

TEST_F(MvTest, MoveFile) {
    const char *source = "source.txt";
    const char *destination = "destination.txt";
    const char *content = "This is a test file.";

    create_file(source, content);

    const char *argv[] = {"mv", source, destination, NULL};
    int argc = 3;

    auto result_status = run_mv_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "mv program should return 0 on success.";

    std::string dest_content = read_file(destination);
    ASSERT_STREQ(content, dest_content.c_str()) << "The content of the destination file should match the source file.";

    // Verify source file does not exist
    ASSERT_EQ(access(source, F_OK), -1) << "The source file should be deleted after a successful move.";

    // Clean up
    remove(destination);
}

TEST_F(MvTest, MoveToExistingFile) {
    const char *source = "source.txt";
    const char *destination = "destination.txt";
    const char *content = "This is a test file.";
    const char *existing_content = "Existing content.";

    create_file(source, content);
    create_file(destination, existing_content);

    const char *argv[] = {"mv", source, destination, NULL};
    int argc = 3;

    auto result_status = run_mv_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "mv program should return 0 on success.";

    std::string dest_content = read_file(destination);
    ASSERT_STREQ(content, dest_content.c_str()) << "The content of the destination file should be overridden with the source file content.";

    // Verify source file does not exist
    ASSERT_EQ(access(source, F_OK), -1) << "The source file should be deleted after a successful move.";

    // Clean up
    remove(destination);
}

TEST_F(MvTest, SourceFileDoesNotExist) {
    const char *source = "nonexistent.txt";
    const char *destination = "destination.txt";

    const char *argv[] = {"mv", source, destination, NULL};
    int argc = 3;

    auto result_status = run_mv_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "mv program should return a non-zero exit status if the source file does not exist.";

    // Verify destination file does not exist
    ASSERT_EQ(access(destination, F_OK), -1) << "The destination file should not be created if the source file does not exist.";
}

TEST_F(MvTest, InvalidArguments) {
    const char *argv[] = {"mv", NULL};
    int argc = 1;

    auto result_status = run_mv_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_NE(status, 0) << "mv program should return a non-zero exit status if the number of arguments is less than 3.";

    const char *argv2[] = {"mv", "source.txt", NULL};
    int argc2 = 2;

    result_status = run_mv_command(argc2, const_cast<char**>(argv2));
    result = result_status.first;
    status = result_status.second;

    ASSERT_NE(status, 0) << "mv program should return a non-zero exit status if the number of arguments is less than 3.";
}

TEST_F(MvTest, SmallerNewFile) {
    const char *source = "source.txt";
    const char *destination = "destination.txt";
    const char *initial_content = "This is the initial content of the destination file.";
    const char *new_content = "New content.";

    // Create the destination file with initial content
    create_file(destination, initial_content);

    // Create the source file with new content
    create_file(source, new_content);

    // Run mv command to move source to destination
    const char *argv[] = {"mv", source, destination, NULL};
    int argc = 3;

    auto result_status = run_mv_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "mv program should return 0 on success.";

    // Verify the destination file content is overridden with new content
    std::string dest_content = read_file(destination);
    ASSERT_STREQ(new_content, dest_content.c_str()) << "The content of the destination file should be overridden with the new content from the source file.";

    // Verify source file does not exist
    ASSERT_EQ(access(source, F_OK), -1) << "The source file should be deleted after a successful move.";

    // Clean up
    remove(destination);
}

TEST_F(MvTest, MoveLargeFile) {
    const char *source = "large_source.txt";
    const char *destination = "large_destination.txt";
    std::string content(1000000, 'a'); // Create a large file with 10,000 'a' characters

    create_file(source, content.c_str());

    const char *argv[] = {"mv", source, destination, NULL};
    int argc = 3;

    auto result_status = run_mv_command(argc, const_cast<char**>(argv));
    std::string result = result_status.first;
    int status = result_status.second;

    ASSERT_EQ(status, 0) << "mv program should return 0 on success.";

    std::string dest_content = read_file(destination);
    ASSERT_STREQ(content.c_str(), dest_content.c_str()) << "The content of the large destination file should match the large source file.";

    // Verify source file does not exist
    ASSERT_EQ(access(source, F_OK), -1) << "The source file should be deleted after a successful move.";

    // Clean up
    remove(destination);
}

