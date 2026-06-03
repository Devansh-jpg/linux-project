#pragma once
#include <string>
#include <vector>

struct ProcessResult {
    std::string stdout_output;
    std::string stderr_output;
    int exit_code;
};

class ProcessLauncher {
public:
    ProcessResult run(const std::string& command);
    ProcessResult run(const std::vector<std::string>& args);
};
