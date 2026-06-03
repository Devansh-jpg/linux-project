#include "process_launcher.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <thread>
#include <sstream>
#include <stdexcept>
#include <cstring>

// Reads everything from a file descriptor until it's closed.
static std::string read_all(int fd) {
    std::string result;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        result.append(buf, n);
    return result;
}

ProcessResult ProcessLauncher::run(const std::string& command) {
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string token;
    while (iss >> token)
        args.push_back(token);
    return run(args);
}

ProcessResult ProcessLauncher::run(const std::vector<std::string>& args) {
    if (args.empty())
        throw std::invalid_argument("No command provided");

    // pipe[0] = read end, pipe[1] = write end
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1)
        throw std::runtime_error(std::string("pipe() failed: ") + strerror(errno));

    pid_t pid = fork();

    if (pid == -1)
        throw std::runtime_error(std::string("fork() failed: ") + strerror(errno));

    if (pid == 0) {
        // ---- CHILD PROCESS ----

        // Child only writes to pipes, so close the read ends.
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Rewire stdout/stderr to point to the pipe write ends.
        // After dup2, fd 1 (STDOUT) writes into stdout_pipe[1].
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Build a null-terminated argv array that execvp expects.
        std::vector<char*> argv;
        for (const auto& a : args)
            argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        // Replace this child process image with the requested command.
        execvp(argv[0], argv.data());

        // execvp only returns on failure.
        _exit(127);
    }

    // ---- PARENT PROCESS ----

    // Parent only reads from pipes, so close the write ends.
    // If we don't close these, read_all() will block forever waiting
    // for more data even after the child exits.
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Read stdout and stderr concurrently using threads.
    // If we read them sequentially, the child can deadlock: it fills
    // the stderr pipe buffer (64 KB) while we're stuck waiting on stdout.
    std::string stdout_output, stderr_output;

    std::thread stdout_reader([&]() {
        stdout_output = read_all(stdout_pipe[0]);
    });
    std::thread stderr_reader([&]() {
        stderr_output = read_all(stderr_pipe[0]);
    });

    stdout_reader.join();
    stderr_reader.join();

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status;
    waitpid(pid, &status, 0);

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return {stdout_output, stderr_output, exit_code};
}
