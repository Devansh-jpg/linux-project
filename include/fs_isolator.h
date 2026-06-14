#pragma once

#include <string>
#include <vector>


class FSIsolator{
    private:
    std::string root_path_ = "/tmp/ForkCage-root";

    // Copy one host file into the jail at the SAME path, creating parent dirs.
    //   host_path = "/bin/sh"  ->  writes "/tmp/ForkCage-root/bin/sh"
    void copy_into_jail(const std::string& host_path);

    // Return absolute paths of all .so dependencies of `binary` (parses ldd).
    std::vector<std::string> shared_libs(const std::string& binary);

    public:

    // Build a working fake root: copy each binary + its libraries in.
    void setup_rootfs(const std::vector<std::string>& binaries =
                          {"/bin/sh", "/bin/ls", "/bin/cat", "/bin/echo"});
    void enter_jail();
    explicit FSIsolator(const std::string& root_path = "/tmp/ForkCage-root");
};
