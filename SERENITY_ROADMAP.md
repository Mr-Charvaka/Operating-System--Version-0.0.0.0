# The 100-Phase NeoPop/SerenityOS Roadmap

This roadmap transforms the current kernel-with-GUI prototype into a fully capable, industry-ready operating system following the architectural philosophy of SerenityOS (from-scratch libraries, userspace GUI, heavy C++ usage) but retaining the NeoPop visual identity.

## 🟢 Era 1: Kernel Foundation (The "Stable Base")
*Goal: Robust memory management, multitasking, and hardware abstraction.*

1.  **Heap Allocator Upgrade:** Implement a `kmalloc`/`kfree` with slab allocation for efficiency (replace current primitive placement malloc).
2.  **Paging & Virtual Memory:** Full higher-half kernel mapping, recursive page directory, and page fault handling.
3.  **Physical Memory Manager:** Bitmap-based physical frame allocator.
4.  **Global Descriptor Table (GDT) Refinement:** Proper user/kernel ring separation (Ring 0 vs Ring 3).
5.  **Interrupt Descriptor Table (IDT):** Robust exception handling (page faults, division by zero) with register dumping.
6.  **Multitasking Support:** Preemptive scheduler with round-robin priority queues.
7.  **Process Control Blocks (PCB):** Detailed process tracking (PID, PPID, state, open file descriptors).
8.  **Kernel Threads:** Ability to spawn kernel-mode background workers.
9.  **Advanced Timer:** High-precision Event Timer (HPET) or APIC timer support.
10. **Real-Time Clock (RTC):** Accurate system time and date tracking.

## 🔵 Era 2: Filesystem & Storage (The "Data Layer")
*Goal: Persistent storage and virtual file abstraction.*

11. **Virtual Filesystem (VFS):** Abstract base class for mounting different filesystems.
12. **ATA/PIO Driver:** Read/Write support for hard drives.
13. **FAT32 Implementation:** Upgrade from FAT16 for larger disk support.
14. **Ext2 Implementation:** Introduction of a proper Unix-like filesystem (inodes, permissions).
15. **DevFS:** `/dev` filesystem implementation (keyboard, mouse, null, zero).
16. **ProcFS:** `/proc` filesystem exposing kernel process lists and stats.
17. **File Descriptors:** `open`, `close`, `read`, `write`, `seek` syscalls fully wired to VFS.
18. **Directory Operations:** `mkdir`, `rmdir`, `readdir` support.
19. **Disk Caching:** Buffer cache to speed up disk I/O.
20. **Partition Table:** MBR/GPT parsing to handle multiple partitions.

## 🟣 Era 3: Userspace & System Calls (The "Separation")
*Goal: Getting out of Kernel Mode.*

21. **ELF Loader:** Robust loading of executable and linkable format files.
22. **Syscall Interface:** `int 0x80` handler or `sysenter` instruction support.
23. **Process Creation:** `fork()`, `execve()`, and `waitpid()` implementation.
24. **Signal Handling:** POSIX-like signals (SIGINT, SIGKILL, SIGSEGV).
25. **Userspace Memory:** `mmap` and `munmap` for dynamic memory allocation in userspace.
26. **User Mode Switch:** Reliable context switching entering/exiting User Mode.
27. **Kernel Panic:** Graphical "Red Screen of Death" (Guru Meditation) for unrecoverable errors.
28. **Argv/Envp:** Passing command line arguments and environment variables to new processes.
29. **Thread Local Storage (TLS):** Support for `fs/gs` registers in userspace.
30. **LibC (The Beginning):** Start `libc.a` with `stdio.h`, `string.h`, `stdlib.h` wrappers around syscalls.

## 🟠 Era 4: Core Libraries (The "Serenity Pillars")
*Goal: Custom C++ libraries to replace standard STL.*

31. **AK (Almost Korrect):** Creation of the foundational template library (Vector, String, HashMap, RefPtr).
32. **LibCore:** Event loops, Timers, File watchers, custom Object model (`CObject`).
33. **LibGfx:** Low-level graphics context, bitmaps, color management, shape rendering primitives.
34. **LibIPC:** Inter-Process Communication primitive (Sockets / Shared Memory).
35. **LibAudio:** WAV parsing and mixing logic.
36. **LibThread:** Threading primitives (Mutex, Semaphore) for userspace.
37. **LibCompress:** Deflate/Gzip implementation for reading assets.
38. **LibCrypto:** Basic hashing (SHA-256) and randomness.
39. **Crash Reporter:** Userspace service to catch crashes and generate backtraces.
40. **Dynamic Linker:** Support for shared libraries (`.so` / `.dll`).

## 🔴 Era 5: The Graphics Stack (The "Visuals")
*Goal: Moving the GUI from Kernel to Userspace.*

41. **WindowServer (Userspace):** Porting `WindowServer.cpp` to run as a user process.
42. **Shared Memory Framebuffers:** Clients draw to shared RAM; WindowServer composites it.
43. **IPC Windowing Protocol:** Clients send "Create Window" messages; Server replies with ID.
44. **Double Buffering:** Elimination of screen tearing in the compositor.
45. **Dirty Rectangles:** Optimizing rendering to only redraw changed screen areas.
46. **Cursor Compositing:** Hardware or software mouse cursor overlay in WindowServer.
47. **Wallpaper Service:** Daemon to handle background rendering.
48. **Screen Resolutions:** Dynamic resolution changing via VESA/BGA ioctls.
49. **TTF Font Engine:** Implementation or port of a TrueType font rasterizer (e.g., LibGfx/Font).
50. **Themes Support:** Loading colors and window metrics from `.ini` files.

## 🟡 Era 6: The User Interface Toolkit (The "Widget Set")
*Goal: LibGUI - The API developers actually use.*

51. **GWidget Base:** Base class for all UI elements with event handling.
52. **GWindow:** Class wrapping the WindowServer IPC calls.
53. **GButton & GLabel:** Standard interactive controls.
54. **GTextBox:** Single-line text input with cursor handling.
55. **GScrollbar & GScrollableWidget:** Handling content larger than the container.
56. **GLayout:** VBox, HBox, and Grid layout managers (no more manual pixel math!).
57. **GMenu & GMenuBar:** Global or per-window menu systems.
58. **GDialog:** MessageBox, FilePicker, ColorPicker.
59. **GClipboard:** System-wide clipboard integration for Text/Images.
60. **Drag & Drop:** Data transfer between widgets and windows.

## 🟢 Era 7: System Services & Hardware (The "Plumbing")
*Goal: Sound, Networking, and Input.*

61. **PS/2 Mouse & Keyboard:** Robust drivers including scroll wheel and multimedia keys.
62. **PCI Enumeration:** Scanning the bus for devices.
63. **Network Card Driver:** E1000 or RTL8139 driver implementation.
64. **TCP/IP Stack:** ARP, IP, ICMP, UDP, and finally TCP state machine.
65. **Socket API:** BSD-socket compatible interface (`socket`, `bind`, `connect`).
66. **DHCP Client:** Auto-configuration of IP addresses.
67. **DNS Resolver:** Resolving domain names to IPs.
68. **Sound Blaster / AC97:** Audio hardware driver.
69. **AudioServer:** Mixing multiple audio streams from apps.
70. **Pseudo Terminals (PTY):** Backend for terminal emulators.

## 🔵 Era 8: The Shell & CLI Ecosystem (The "Power User")
*Goal: A capable command-line environment.*

71. **Shell Implementation:** Command parsing, pipes (`|`), redirection (`>`), background jobs (`&`).
72. **Coreutils:** `ls`, `cat`, `echo`, `cp`, `mv`, `rm`, `touch`, `mkdir`.
73. **Text Editor (CLI):** Implementation of a Nano/Vim clone (`micro`).
74. **Grep & Sed:** Pattern matching and stream editing tools.
75. **Top / Htop:** Process viewer in the terminal.
76. **Scripting Language:** Porting Lua or Python, or writing a custom shell script engine.
77. **Git Lite:** Basic version control system client.
78. **Man Pages:** Documentation viewer.
79. **Telnet / SSH:** Remote access capability.
80. **Build System:** Porting GCC/Binutils or a smaller C compiler (TCC) to run ON the OS.

## 🟣 Era 9: Desktop Applications (The "Experience")
*Goal: Useful built-in software suite.*

81. **Terminal Emulator (GUI):** A proper LibGUI app using PTYs (ANSI colors, history).
82. **FileManager (GUI):** Icon view, list view, file operations, thumbnails.
83. **Text Editor (GUI):** A Notepad++ clone with syntax highlighting (GTextEditor).
84. **ImageViewer:** supporting PNG, JPG, BMP.
85. **Paint:** Bitmap editor (brush, pencil, shapes).
86. **Calculator:** Scientific mode, graphing capability.
87. **System Monitor:** Graphs for CPU/RAM/Network history.
88. **Profiler:** A tool to analyze running process performance (flamegraphs).
89. **Games:** Minesweeper, Snake, Solitaire (Porting Doom is mandatory).
90. **Audio Player:** Visualization and playlist support.

## ⚫ Era 10: Advanced Tech & Distribution (The "Industry Ready")
*Goal: Web, Security, and Polish.*

91. **LibHTML / LibWeb:** HTML parser and layout engine (The "Ladybird" goal).
92. **LibJS:** JavaScript interpreter/JIT.
93. **Browser:** A functional web browser application.
94. **SMP (Symmetric Multiprocessing):** utilizing multiple CPU cores.
95. **Security Hardening:** ASLR, DEP/NX, UID/GID permission enforcement.
96. **Package Manager:** `opkg` or `apt`-like tool to install ported software.
97. **Installation Wizard:** Graphical installer to install OS to disk.
98. **Documentation:** Self-hosted wiki or help system.
99. **Accessibility:** Screen reader API, High contrast themes.
100. **Self-Hosting:** The ability to compile the entire OS *from within the OS*.

---

**Current Status:** We are transitioning from **Late Phase 1** (Multitasking) to **Early Phase 2** (VFS/Filesystems). The GUI is currently a Phase 40-equivalent prototype running in kernel mode.
