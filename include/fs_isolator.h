#pragma once

#include <string>


class FSIsolator{
    private:
    std::string root_path = "/tmp/sandbox-root";
    public:
    
    void setup_rootfs();
    void enter_jail();
    FSIsolator(const std::string& root_path= "/tmp/sandbox-root");
};