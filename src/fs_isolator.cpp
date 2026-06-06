#include "fs_isolator.h"

#include <unistd.h>

#include <sys/types.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>


#include <filesystem>    // ← for std::filesystem::create_directories
#include <cerrno>        // ← for errno
#include <stdexcept>
#include <cstring>

FSIsolator::FSIsolator(const std::string& root_path)
    : root_path_(root_path) {}
void FSIsolator::setup_rootfs(){
std::filesystem::create_directories(root_path_);
std::filesystem::create_directories(root_path_+"/bin");
std::filesystem::create_directories(root_path_+"/lib");
std::filesystem::create_directories(root_path_+"/lib64");
if (!std::filesystem::exists(root_path_ + "/bin/sh")) {
    throw std::runtime_error("binary not found in jail — run setup script first");
}
}
void FSIsolator::enter_jail(){
    if(unshare(CLONE_NEWNS)==-1){
        perror("unshare failed");  // ← simplest correct way
        _exit(126);
    }
    if(chroot(root_path_.c_str())==-1){
        perror("chroot error");
        _exit(126);
    }
       if(chdir("/")==-1){
        perror("chdir error");
        _exit(126);
    }
}

