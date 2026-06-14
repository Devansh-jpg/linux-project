#include "process_launcher.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ForkCage <command> [args...]\n";
        return 1;
    }

    std::vector<std::string> args(argv + 1, argv + argc);

    ProcessLauncher launcher;
    ProcessResult result = launcher.run(args);
    std::cout << "STDOUT: [" << result.stdout_output << "]\n";
    std::cout << "STDERR: [" << result.stderr_output << "]\n";
    std::cout << "EXIT: " << result.exit_code << "\n";

    if (!result.stdout_output.empty())
        std::cout << result.stdout_output;

    if (!result.stderr_output.empty())
        std::cerr << result.stderr_output;

    return result.exit_code;
}


