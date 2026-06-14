# ForkCage — C++ Container Runtime

## What This Project Is

A Linux container runtime built from scratch in C++, inspired by **Indexable** (YC W26 — sandbox infrastructure for AI agents). The goal is to understand and replicate the core of what Docker/runc does at the syscall level.

Indexable built a custom hypervisor on KVM that forks running sandboxes in milliseconds. We are building the same foundational layer — process isolation, filesystem isolation, resource limits, and live snapshotting — in C++.

---

## Developer Profile

- 1 year of software development experience
- Knows C++ well (DSA/competitive programming level)
- Basic multithreading knowledge
- Everything below this (Linux syscalls, namespaces, cgroups, virtual memory) is new territory
- **Explain Linux syscalls from scratch. Draw analogies to C++ concepts already known.**

---

## Product Vision

A lightweight Linux sandbox runtime that lets you run any command in complete isolation:
- No access to the host filesystem
- No visibility into other running processes
- Resource-capped (CPU, memory)
- Snapshotable and cloneable in milliseconds

### Monetisation path (future)
- CLI tool first → C++ library → REST API daemon → SaaS
- Best angle: **self-hosted / on-prem** for enterprises with compliance requirements (can't send code to E2B/Indexable cloud)
- Open core model: open source runtime + paid hosted version

### CV value by milestone
| Stop at | Impact |
|---|---|
| M1 + M2 | Weak — fresher level |
| M1 + M2 + M3 | **Strong — FAANG SDE-1/2, HFT junior** |
| All 4 | **Exceptional — HFT, FAANG SDE-2, infra roles** |

---

## Milestones

### Milestone 1 — Process Launcher ✅ DONE
**What:** Launch any shell command as a child process, capture stdout/stderr, return exit code.

**Key syscalls:** `fork()`, `execvp()`, `pipe()`, `dup2()`, `waitpid()`

**Why two threads:** Reading stdout then stderr sequentially causes deadlock if child fills the 64KB pipe buffer. Two threads drain both pipes concurrently.

**Status:** Built, compiled, tested on WSL2 Ubuntu. `ls`, `whoami`, `echo` all working.

---

### Milestone 2 — Filesystem Isolation ⬜ NEXT
**What:** Child process sees only a fake root filesystem, not the real `/home`, `/etc`, etc.

**Key syscalls:** `unshare(CLONE_NEWNS)`, `chroot()`, `pivot_root()`, `mount()`

**Files to create:** `include/fs_isolator.h`, `src/fs_isolator.cpp`

**Steps to implement:**
1. `unshare(CLONE_NEWNS)` — give process its own mount namespace
2. Set up an overlayfs or minimal rootfs at `/tmp/ForkCage-root`
3. `chroot("/tmp/ForkCage-root")` — jail the process inside fake root
4. `chdir("/")` — reset working dir inside jail

**Setup needed:**
```bash
mkdir -p /tmp/ForkCage-root/{bin,lib,lib64}
cp /bin/bash /tmp/ForkCage-root/bin/
# copy shared libs shown by: ldd /bin/bash
```

**Study before coding:**
- `man 2 unshare`, `man 2 chroot`, `man 7 namespaces`
- [LWN Namespaces series](https://lwn.net/Articles/531114/)
- [Containers from Scratch — Liz Rice (video)](https://www.youtube.com/watch?v=8fi7uSYlOdc)

---

### Milestone 3 — Process Isolation ⬜
**What:** Process cannot see or kill other running apps. Resource-capped.

**Key primitives:**
| Primitive | Flag | What it isolates |
|---|---|---|
| PID namespace | `CLONE_NEWPID` | Process sees only its own subtree |
| User namespace | `CLONE_NEWUSER` | Fake root inside, unprivileged outside |
| Network namespace | `CLONE_NEWNET` | Own network stack |
| cgroups v2 | filesystem API | CPU, memory, I/O limits |
| seccomp | `prctl` + BPF | Block dangerous syscalls |

**cgroups work by writing to files:**
```bash
mkdir /sys/fs/cgroup/ForkCage
echo "512M" > /sys/fs/cgroup/ForkCage/memory.max
echo $PID   > /sys/fs/cgroup/ForkCage/cgroup.procs
```

**Study before coding:**
- LWN namespaces series parts 3–7
- `man 7 cgroups`, [cgroups v2 kernel docs](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html)
- `man 2 seccomp`
- Reference: [google/nsjail](https://github.com/google/nsjail) — C++, read `cgroup.cc`, `ns.cc`

---

### Milestone 4 — State Snapshot & Clone ⬜
**What:** Save sandbox state and fork/clone it (like Indexable's "fork in milliseconds").

**Two approaches:**
1. `fork()` on live sandbox — COW memory, millisecond speed, same machine only
2. CRIU — serialize full process state to disk, restore anywhere

**Study before coding:**
- Virtual memory + Copy-on-Write pages
- `cat /proc/$$/maps` — understand memory regions
- `man 2 mmap`
- [CRIU architecture docs](https://criu.org/How_does_it_work)

---

## Project Structure

```
linux-project/
├── CLAUDE.md                  ← this file
├── README.md                  ← build instructions + milestone roadmap
├── CMakeLists.txt             ← C++17, pthread
├── include/
│   └── process_launcher.h    ← ProcessResult struct + ProcessLauncher class
└── src/
    ├── process_launcher.cpp  ← fork/execvp/pipe implementation
    └── main.cpp              ← CLI entry point
```

---

## Build & Run (WSL2)

```bash
cd /mnt/c/Users/DEVANSH/linux-project
mkdir build && cd build
cmake ..
make
./ForkCage ls
./ForkCage whoami
./ForkCage bash -c "echo hello"
```

**WSL2 apt fix (if apt hangs at 0%):**
```bash
sudo sed -i 's|http://archive.ubuntu.com|http://mirrors.mit.edu|g' /etc/apt/sources.list
sudo apt update
```

---

## LLD — Class Design

```
ForkCage (orchestrator)
  ├── ProcessLauncher    → fork/exec/pipe         (M1 ✅)
  ├── FSIsolator         → mount ns + chroot       (M2)
  ├── ProcessIsolator    → pid/user ns + cgroups   (M3)
  └── SnapshotManager   → COW fork + CRIU          (M4)
```

Each class owns one responsibility. Build and test independently. Later refactor to a `ForkCageBuilder` fluent API.

---

## Key C++ Concepts Needed

| Concept | Needed for | Priority |
|---|---|---|
| RAII + destructors | All milestones — FD leak prevention | Now |
| `errno` + `strerror` | All milestones — syscall error handling | Now |
| `_exit()` vs `exit()` | M1 — never use `exit()` in child after fork | Now |
| `std::unique_ptr` custom deleters | M3 — cgroup dir handles | Before M3 |
| Virtual memory + COW | M4 — snapshot internals | Before M4 |

---

## Reference Repos

| Repo | Language | Why read it |
|---|---|---|
| [google/nsjail](https://github.com/google/nsjail) | C++ | Production sandbox, same primitives — read `cgroup.cc`, `ns.cc`, `mnt.cc` |
| [w-vi/diyC](https://github.com/w-vi/diyC) | C | Simple educational container runtime |
| [Sahilb315/runbox](https://github.com/Sahilb315/runbox) | C | Closest in scope to this project |

---

## Learning Resources

- **Book:** *The Linux Programming Interface* — Kerrisk (chapters 24–29, 44)
- **Book:** *Effective Modern C++* — Scott Meyers (items 18–22 smart pointers)
- **Articles:** [LWN Namespaces in operation](https://lwn.net/Articles/531114/) — 7 parts, read all
- **Video:** [Containers from Scratch — Liz Rice](https://www.youtube.com/watch?v=8fi7uSYlOdc) — 35 min, watch before M2
- **Man pages:** `man 2 fork`, `man 2 execve`, `man 2 unshare`, `man 7 namespaces`, `man 2 clone`
