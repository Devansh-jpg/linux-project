# Sandbox — C++ Container Runtime

Inspired by Indexable (YC) — sandbox infrastructure for AI agents.

---

## Project Structure

```
linux-project/
├── CMakeLists.txt
├── include/
│   └── process_launcher.h     ← class definition + ProcessResult struct
└── src/
    ├── process_launcher.cpp   ← fork/exec/pipe logic
    └── main.cpp               ← CLI entry point
```

---

## Milestone 1 — Process Launcher (DONE)

### What it does
Launches any shell command as a child process, captures its stdout and stderr, and returns the exit code.

### Key syscalls used
| Syscall | What it does |
|---|---|
| `fork()` | Clones current process into parent + child |
| `execvp()` | Replaces child process image with your command |
| `pipe()` | Creates a read/write channel between parent and child |
| `dup2()` | Rewires child's stdout/stderr into the pipe |
| `waitpid()` | Parent waits for child to finish, gets exit code |

### Why two threads?
Reading stdout and stderr sequentially risks deadlock — if the child fills the stderr pipe buffer (64KB) while the parent is still reading stdout, both block forever. Two threads drain both pipes concurrently.

### How to build and run (in WSL2)

```bash
cd /mnt/c/Users/DEVANSH/linux-project
mkdir build && cd build
cmake ..
make
./sandbox ls -la
./sandbox python3 --version
./sandbox bash -c "echo hello"
```

---

## Milestone 2 — Filesystem Isolation (NEXT)

### Goal
The child process should only see a fake root filesystem, not your real `/home`, `/etc`, etc.

### File to create
`src/fs_isolator.cpp` + `include/fs_isolator.h`

### What to write
1. Call `unshare(CLONE_NEWNS)` — gives the process its own mount namespace
2. Call `chroot("/tmp/sandbox-root")` — jail the process inside a fake root
3. Call `chdir("/")` — reset working directory inside the jail

### New syscalls to learn before coding
- `man 2 unshare`
- `man 2 chroot`
- `man 7 namespaces`

### Setup needed before running
```bash
# Create a minimal fake root filesystem to chroot into
mkdir -p /tmp/sandbox-root/{bin,lib,lib64}
cp /bin/bash /tmp/sandbox-root/bin/
cp /bin/ls   /tmp/sandbox-root/bin/
# copy required shared libs (ldd /bin/bash will show you which)
```

---

## Milestones Roadmap

- [x] Milestone 1 — Process Launcher (fork + exec + pipe)
- [ ] Milestone 2 — Filesystem Isolation (mount namespace + chroot)
- [ ] Milestone 3 — Process Isolation (PID + user namespace + cgroups)
- [ ] Milestone 4 — State Snapshot & Clone (fork-on-live + CRIU)
