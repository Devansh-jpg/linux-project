# Milestone 1 — Process Launcher: Full Explanation

---

## High-Level Design (HLD)

```
User types:   ./sandbox ls -la

main.cpp            process_launcher.cpp              Linux kernel
─────────           ────────────────────              ────────────
parse argv    →     create 2 pipes
                    fork()            ────────────→   creates child PID
                    
                    [CHILD]                           execvp("ls", ...)
                      rewire stdout → pipe           (child process image
                      rewire stderr → pipe            is REPLACED by ls)
                    
                    [PARENT]
                      thread 1: drain stdout pipe
                      thread 2: drain stderr pipe
                      waitpid()       ←────────────   child exits, kernel
                                                       sends SIGCHLD
                    return { stdout, stderr, code }
                    
print result  ←
```

The **key design constraint** driving everything: you want to capture a child's output without it going to the terminal. The only OS-level mechanism for that is **pipes**. Everything else in the design follows from that.

---

## Low-Level Design (LLD)

```
ProcessResult struct
  string stdout_output
  string stderr_output
  int    exit_code

ProcessLauncher class
  run(string)          → tokenize → run(vector<string>)
  run(vector<string>)  → the real logic
  
static read_all(fd)    → helper, not part of the class
```

The split into two `run()` overloads is classic **convenience overload** — the string version is a user-friendly entry point, the vector version is the real engine. The class itself holds no state; it is essentially a namespace with methods.

---

## Every Function, Line by Line

---

### `read_all(int fd)` — `src/process_launcher.cpp:13`

```cpp
static std::string read_all(int fd) {
    std::string result;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        result.append(buf, n);
    return result;
}
```

**What it does:** drains a file descriptor completely and returns everything as a string.

**`read(fd, buf, size)`** — raw Linux syscall. Reads up to `size` bytes from `fd` into `buf`. Returns:
- `> 0` — number of bytes actually read
- `0` — EOF (write end of the pipe was closed, meaning child exited)
- `-1` — error

The `while > 0` loop keeps reading in 4096-byte chunks until EOF. Think of `fd` like `istream` — you don't know how much data is coming, so you loop until the stream closes.

**`static`** means this function is file-scoped — a private helper, invisible outside this `.cpp`.

---

### `run(const std::string& command)` — `src/process_launcher.cpp:22`

```cpp
std::istringstream iss(command);
std::string token;
while (iss >> token)
    args.push_back(token);
return run(args);
```

Tokenizes the string by whitespace using `istringstream`. `"ls -la /home"` becomes `["ls", "-la", "/home"]`. Then delegates to the real `run()`.

---

### `run(const std::vector<std::string>& args)` — `src/process_launcher.cpp:31`

This is the core. Broken into phases:

---

#### Phase 1 — Create the pipes

```cpp
int stdout_pipe[2];
int stderr_pipe[2];
pipe(stdout_pipe);
pipe(stderr_pipe);
```

`pipe(fd[2])` creates a pair of file descriptors where anything written to `fd[1]` can be read from `fd[0]`. A one-directional byte channel in kernel memory.

```
stdout_pipe[0]  ←──────────  stdout_pipe[1]
   (read end)                  (write end)
```

Two pipes created — one for stdout, one for stderr. After `fork()`, both parent and child share all 4 fds.

---

#### Phase 2 — `fork()`

```cpp
pid_t pid = fork();
```

`fork()` clones the current process. After this line, two processes run the same code. The only difference:
- In the **parent**: `fork()` returns the child's PID (a positive number)
- In the **child**: `fork()` returns `0`

Think of it like a function that runs twice — once in the original process returning the child's id, and once in a full copy of the program returning 0. Both copies share the same pipe fds.

---

#### Phase 3 — Child process (`pid == 0`)

```cpp
close(stdout_pipe[0]);   // child won't read from stdout pipe
close(stderr_pipe[0]);   // child won't read from stderr pipe

dup2(stdout_pipe[1], STDOUT_FILENO);  // fd 1 now points to pipe
dup2(stderr_pipe[1], STDERR_FILENO);  // fd 2 now points to pipe

close(stdout_pipe[1]);   // original write-end fd no longer needed
close(stderr_pipe[1]);
```

`dup2(oldfd, newfd)` — "make `newfd` point to the same thing as `oldfd`". `STDOUT_FILENO` is just the integer `1`. After `dup2(stdout_pipe[1], 1)`, whenever any code in the child writes to fd `1` (`printf`, `cout`, etc.), it actually writes into the pipe. The child has no idea — from its perspective it's just writing to stdout.

Why close the originals after dup2? The pipe write-end is now accessible via two fds (`stdout_pipe[1]` and `1`). If you leave `stdout_pipe[1]` open, the parent's `read_all` will never see EOF because the write-end is still open. Close the duplicate to keep exactly one reference.

```cpp
execvp(argv[0], argv.data());
_exit(127);
```

`execvp` replaces the current process image with the command to run. The child stops being "a copy of the sandbox program" and becomes `ls` (or whatever). The pipes survive because `execvp` preserves open file descriptors.

`_exit(127)` only runs if `execvp` fails. We use `_exit` not `exit` because `exit()` flushes C++ destructors and atexit handlers inherited from the parent — things the parent, not the child, should clean up.

---

#### Phase 4 — Parent process closes write ends

```cpp
close(stdout_pipe[1]);
close(stderr_pipe[1]);
```

The parent only reads. It must close the write ends. If it doesn't, `read_all` will block forever — the pipe stays "open for writing" from the parent's perspective, so it will never send EOF to the reader.

---

#### Phase 5 — Two threads to drain pipes

```cpp
std::thread stdout_reader([&]() { stdout_output = read_all(stdout_pipe[0]); });
std::thread stderr_reader([&]() { stderr_output = read_all(stderr_pipe[0]); });

stdout_reader.join();
stderr_reader.join();
```

**Why two threads?** Linux kernel pipe buffers are 64 KB. If you read stdout first (sequentially), and the child is writing enough stderr to fill the 64 KB buffer, the child blocks waiting for someone to drain stderr — but you're stuck reading stdout. **Classic deadlock.** Two threads drain both pipes concurrently so neither ever blocks.

---

#### Phase 6 — Collect exit code

```cpp
waitpid(pid, &status, 0);
int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
```

`waitpid` blocks until the child process exits and reaps its exit status from the kernel. Without this, the child becomes a **zombie process** — dead but its entry in the process table isn't cleaned up.

`WIFEXITED(status)` checks if the process exited normally (vs. killed by a signal). `WEXITSTATUS(status)` extracts the actual exit code from the packed `status` integer.

---

### `main.cpp`

```cpp
std::vector<std::string> args(argv + 1, argv + argc);
```

Converts the C-style `argv` array into a `std::vector<std::string>`, skipping `argv[0]` (which is `"sandbox"` itself). So `./sandbox ls -la` gives `["ls", "-la"]`.

Then runs `launcher.run(args)`, prints stdout/stderr, and returns the child's exit code — so the sandbox is transparent to any shell script wrapping it.

---

## Design Decisions Table

| Decision | Why |
|---|---|
| Two overloads for `run()` | Clean API — callers can pass a string or a pre-split vector |
| `pipe()` before `fork()` | Pipes must exist before fork so both processes inherit the same fds |
| `dup2` to rewire stdout/stderr | The child command has no idea it's being captured — zero cooperation needed |
| Two reader threads | Prevents deadlock on 64 KB pipe buffer fill |
| `_exit` not `exit` in child | Avoids running parent's destructors/atexit in a copied process |
| Close write-ends in parent | Without this, `read_all` never sees EOF and hangs forever |
| `waitpid` after joining threads | Reap the child; threads must finish first since they read the child's output |
| `ProcessResult` as a plain struct | Just data, no behavior — a C++ value type returned by value |

---

## Full Execution Flow for `./sandbox ls`

```
1. main.cpp:       args = ["ls"]
2. launcher.run(): pipe() × 2  →  creates 4 fds
3.                 fork()       →  two processes now exist
4. child:          dup2 rewires fd 1 and fd 2 into pipes
5. child:          execvp("ls") → child becomes ls
6. ls runs:        writes directory listing to fd 1 (which is the pipe)
7. ls exits:       pipe write-end closes, read_all() returns in parent thread
8. parent:         threads join, waitpid() reaps child
9. main.cpp:       prints captured output, returns exit code
```
