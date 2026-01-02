# Kernel-Level Roadmap to Serenity OS Maturity

## Current Status Analysis
✅ **Completed:**
- Basic multitasking & scheduling
- Bitmap Physical Memory Manager (PMM)
- Slab allocator for heap efficiency
- Paging with identity mapping
- Basic VFS with FAT16
- GUI running in kernel mode
- ELF loading (basic)
- Syscall infrastructure

🔶 **Critical Gaps:**
- No proper process isolation (memory spaces)
- No fork/exec implementation
- Filesystem is read-only
- No signal handling
- GUI is monolithic (kernel-mode)
- No dynamic memory for userspace

---

## Phase 1: Memory Management Hardening (2-3 weeks)
*Foundation for everything else.*

### 1.1 Per-Process Page Directories
**Why:** Currently all processes share kernel page directory. Need isolation.
**What:**
- Implement `clone_page_directory()` for fork
- Each process gets own page directory
- Kernel space (3GB-4GB) stays mapped, user space (0-3GB) is per-process
- Page fault handler creates pages on-demand (Copy-on-Write)

**Files to Create/Modify:**
- `src/kernel/vm.cpp` - Virtual memory subsystem
- Upgrade `paging.cpp` to support per-process directories

### 1.2 Dynamic User Memory (mmap/brk)
**Why:** Userspace programs need dynamic memory (malloc in libc).
**What:**
- Implement `sys_brk()` - extend heap
- Implement `sys_mmap()` - map anonymous memory
- Implement `sys_munmap()` - unmap regions

**Result:** Userspace malloc works without kernel heap.

---

## Phase 2: True Userspace Separation (3-4 weeks)
*Get the GUI out of the kernel.*

### 2.1 Robust Fork Implementation
**Why:** Need to spawn processes properly.
**What:**
- `sys_fork()` clones parent's page directory
- Copy kernel stack for child
- Set up proper TSS for child
- Return 0 to child, child PID to parent

### 2.2 Exec Implementation
**Why:** Fork creates copies; exec replaces with new program.
**What:**
- `sys_execve(path, argv, envp)`
- Clear user memory
- Load new ELF binary
- Set up argv/envp on new stack
- Jump to entry point

### 2.3 Wait/Exit Implementation
**Why:** Parent needs to know when child finishes.
**What:**
- `sys_wait()` - block until child exits
- `sys_exit()` - terminate current process
- Zombie process handling (keep PCB until parent waits)

**Result:** Can run `/bin/init` properly.

---

## Phase 3: IPC Foundation (2 weeks)
*Processes need to talk to each other.*

### 3.1 Pipes
**Why:** Shell needs to pipe commands.
**What:**
- `sys_pipe()` creates two file descriptors
- Ring buffer in kernel (4KB)
- Blocking read/write when empty/full

### 3.2 Unix Domain Sockets
**Why:** WindowServer needs to communicate with GUI apps.
**What:**
- `sys_socket(AF_UNIX, SOCK_STREAM, 0)`
- `bind()`, `connect()`, `accept()`
- Message passing via kernel buffers

**Result:** GUI apps can talk to WindowServer.

---

## Phase 4: Signal Handling (1-2 weeks)
*Processes need to be interrupted.*

### 4.1 Signal Infrastructure
**What:**
- Signal table in PCB (32 signal handlers)
- `sys_signal(signum, handler)` - register handler
- `sys_kill(pid, sig)` - send signal
- Kernel delivers signal during context switch
- Set up user stack with signal handler frame

**Signals to support:**
- SIGINT (Ctrl+C)
- SIGTERM (graceful exit)
- SIGSEGV (segfault)
- SIGCHLD (child died)
- SIGKILL (cannot be caught)

---

## Phase 5: Filesystem Completion (2-3 weeks)
*Make storage actually persistent.*

### 5.1 Write Support for FAT16
**Why:** Currently read-only.
**What:**
- `fat16_write_file()` - allocate clusters, update FAT
- `fat16_create_file()` - new directory entries
- `fat16_delete_file()` - free clusters

### 5.2 VFS Enhancement
**What:**
- `sys_open()` returns file descriptor (FD table in PCB)
- `sys_read(fd, buf, count)`
- `sys_write(fd, buf, count)`
- `sys_lseek(fd, offset, whence)`
- `sys_close(fd)`

### 5.3 DevFS
**Why:** Unix philosophy: "everything is a file"
**What:**
- `/dev/null` - bit bucket
- `/dev/zero` - infinite zeros
- `/dev/random` - randomness
- `/dev/tty` - current terminal
- `/dev/fb0` - framebuffer device

**Result:** Can redirect to `/dev/null`, read randomness, etc.

---

## Phase 6: Scheduler Improvements (1 week)
*Better multitasking.*

### 6.1 Priority Scheduling
**What:**
- Each process has priority (0-139, like Linux)
- Higher priority = more CPU time
- Implement time slices (quantum)

### 6.2 Sleep/Wake Primitives
**What:**
- `sleep_on(wait_queue)` - block process
- `wake_up(wait_queue)` - unblock
- Used by pipes, sockets, timers

---

## Phase 7: Hardware Abstraction (2 weeks)
*Clean driver model.*

### 7.1 Block Device Layer
**Why:** Abstract disk access.
**What:**
- `struct block_device` with `read_block()`/`write_block()`
- ATA implements this interface
- VFS uses block_device, not ATA directly

### 7.2 Character Device Layer
**What:**
- `struct char_device` with `read()`/`write()`
- Keyboard, mouse, serial implement this

### 7.3 PCI Enumeration
**Why:** Need to find network cards, sound cards.
**What:**
- Scan PCI bus
- Enumerate devices (vendor ID, device ID)
- Match to drivers

---

## Phase 8: Moving GUI to Userspace (4-6 weeks)
*The big migration.*

### 8.1 Shared Memory
**What:**
- `sys_shmget()` - create shared memory segment
- `sys_shmat()` - attach to process
- Used for framebuffers between WindowServer and clients

### 8.2 WindowServer as User Process
**What:**
- Move `WindowServer.cpp` to userspace
- Uses `/dev/fb0` for display
- Listens on Unix socket `/tmp/window-server.sock`

### 8.3 LibGUI Client Library
**What:**
- `libgui.a` in userspace links against apps
- Handles IPC protocol with WindowServer
- `Window::create()` sends message to server

### 8.4 Port Existing Apps
**What:**
- Rewrite dock, icons as separate processes
- Each app opens connection to WindowServer

---

## Phase 9: Essential System Services (2-3 weeks)

### 9.1 Init Process
**What:**
- `/sbin/init` spawned as PID 1
- Mounts filesystems
- Spawns WindowServer
- Spawns getty (login) on TTY

### 9.2 TTY/PTY System
**What:**
- `struct tty` - terminal device
- Line discipline (canonical mode, echo, signals)
- PTY master/slave for GUI terminal

---

## Suggested Implementation Order

| Priority | Phase | Estimated Time | Why Critical |
|----------|-------|----------------|--------------|
| 🔴 HIGH | Phase 1 (Memory) | 2-3 weeks | Foundation for isolation |
| 🔴 HIGH | Phase 2.1-2.2 (Fork/Exec) | 2 weeks | Can't run userspace properly |
| 🟠 MED | Phase 3.1 (Pipes) | 1 week | Shell needs this |
| 🔴 HIGH | Phase 5.2 (VFS FDs) | 1 week | POSIX compatibility |
| 🟠 MED | Phase 3.2 (Sockets) | 1 week | Needed before GUI migration |
| 🔴 HIGH | Phase 8 (GUI → Userspace) | 4-6 weeks | Architecture cleanup |
| 🟢 LOW | Phase 4 (Signals) | 1 week | Nice to have |
| 🟢 LOW | Phase 6 (Scheduler) | 1 week | Optimization |

---

## Quick Wins (Do These First!)

1. **File Descriptor Table** (2 days)
   - Add `int fd_table[128]` to PCB
   - Make `sys_open()` work properly

2. **Copy-on-Write Pages** (3 days)
   - Mark pages as read-only during fork
   - Page fault handler copies on write
   - Huge memory savings

3. **Basic Signals** (3 days)
   - Just SIGKILL and SIGSEGV
   - Enough to terminate misbehaving processes

4. **DevFS** (2 days)
   - `/dev/null`, `/dev/zero`
   - Teaches VFS mounting

---

## Comparison to Serenity OS

**Serenity's Strengths You Should Adopt:**
1. **AK Library** - Custom template library (Vector, String)
2. **LibCore** - Event loop model
3. **Strong typing** - Extensive use of `RefPtr`, `NonnullRefPtr`
4. **IPC Codegen** - Auto-generate IPC stubs from `.ipc` files

**Your Unique Advantages:**
1. **Modern C++** - You can use C++17/20
2. **NeoPop Aesthetic** - Distinct visual identity
3. **Simpler Start** - Don't need POSIX perfection immediately

---

## Realistic Timeline

- **3 months:** Phase 1-2 complete, basic fork/exec works
- **6 months:** VFS stable, pipes working, basic shell runs
- **12 months:** GUI fully in userspace, IPC working, LibGUI functional
- **18-24 months:** Desktop apps, networking, approaching Serenity's current state

**Focus first on making the kernel SOLID before fancy features.**
