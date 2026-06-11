#include "fs_isolator.h"

#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <sys/mount.h>

#include <filesystem>    // std::filesystem::create_directories / copy_file
#include <cerrno>        // errno
#include <stdexcept>
#include <cstring>
#include <cstdio>        // popen / pclose / fgets
#include <sstream>
#include <memory>

namespace fs = std::filesystem;

FSIsolator::FSIsolator(const std::string& root_path)
    : root_path_(root_path) {}

// --- helper: parse `ldd <binary>` and return absolute .so paths -------------
std::vector<std::string> FSIsolator::shared_libs(const std::string& binary) {
    std::vector<std::string> libs;

    std::string cmd = "ldd " + binary + " 2>/dev/null";

    // RAII wrapper so the pipe is always closed, even if we throw mid-parse.
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        throw std::runtime_error("popen(ldd) failed: " + std::string(strerror(errno)));

    char line[1024];
    while (fgets(line, sizeof(line), pipe.get())) {
        // A line is either:
        //   "libc.so.6 => /lib/.../libc.so.6 (0x..)"   -> want 3rd token
        //   "/lib64/ld-linux-x86-64.so.2 (0x..)"       -> want 1st token
        //   "linux-vdso.so.1 (0x..)"                   -> no path, skip
        std::istringstream iss(line);
        std::string tok;
        while (iss >> tok) {
            // Pick the token that is an absolute path to a shared object.
            if (tok.front() == '/' && tok.find(".so") != std::string::npos) {
                libs.push_back(tok);
                break;
            }
        }
    }
    return libs;
}

// --- helper: copy a host file into the jail at the same path ----------------
void FSIsolator::copy_into_jail(const std::string& host_path) {
    if (!fs::exists(host_path))
        return;  // binary/lib not present on this host; skip silently

    fs::path dest = root_path_ + host_path;   // "/tmp/sandbox-root" + "/bin/sh"
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    if (ec)
        throw std::runtime_error("mkdir " + dest.parent_path().string() +
                                 " failed: " + ec.message());

    fs::copy_file(host_path, dest, fs::copy_options::overwrite_existing, ec);
    if (ec)
        throw std::runtime_error("copy " + host_path + " -> " + dest.string() +
                                 " failed: " + ec.message());
}

// --- build the fake root (runs in PARENT, before fork/chroot) ---------------
void FSIsolator::setup_rootfs(const std::vector<std::string>& binaries) {
    fs::create_directories(root_path_ + "/bin");
    fs::create_directories(root_path_ + "/lib");
    fs::create_directories(root_path_ + "/lib64");

    for (const auto& bin : binaries) {
        copy_into_jail(bin);                       // the binary itself
        for (const auto& lib : shared_libs(bin))   // + every .so it needs
            copy_into_jail(lib);
    }

    if (!fs::exists(root_path_ + "/bin/sh"))
        throw std::runtime_error("setup_rootfs: /bin/sh missing after copy");
}

// --- enter the jail (runs in CHILD, after fork + dup2) ----------------------
void FSIsolator::enter_jail(){
    // 1. Give this process its own mount namespace.
    if (unshare(CLONE_NEWNS) == -1) {
        perror("unshare failed");
        _exit(126);
    }

    // 2. Mark all mounts private so nothing we do leaks back to the host.
    if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == -1) {
        perror("mount MS_PRIVATE failed");
        _exit(126);
    }

    // 3. Make root_path_ the new "/".
    if (chroot(root_path_.c_str()) == -1) {
        perror("chroot error");
        _exit(126);
    }

    // 4. Land inside the new root.
    if (chdir("/") == -1) {
        perror("chdir error");
        _exit(126);
    }
}
