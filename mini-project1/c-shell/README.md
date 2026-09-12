# C-Shell (Mini-Project 1) - Assumptions & Limits

To keep the shell stable and avoid unexpected crashes, I set a few hard limits on memory and buffer sizes. If a user hits these limits, the shell handles it safely instead of breaking. 

Here are the hardcoded limits I used:

* **Input length:** The command line won't process anything over 1024 characters.
* **Pipelines:** You can chain up to 512 commands together in a single pipeline.
* **Background Jobs:** 
  * The shell tracks up to 1024 background PIDs at once.
  * The full command string stored for a job is capped at 1024 characters.
  * Individual command names tracked within a job are truncated at 63 characters (64-byte buffer).
* **Syscalls (snoop):** 
  * I capped the max tracked syscall ID at 512.
  * The lookup table manually maps 19 common syscalls by name (like `read`, `write`, `execve`, etc.).
  * If a process uses something outside this list, it safely prints `syscall_<ID>` (using a 32-byte buffer).
* **Path limits:** Things like nested directories (used in `spy` and `peek`) are stored in standard 4096-byte buffers.
* **File reading & Redirection:** Commands that read files (`peek`, I/O redirection) process data in 4096-byte chunks.
* **Spy Command Buffers:** When reading system files in `/proc`, it uses a 2048-byte buffer for status/cmdline files, and a 5000-byte buffer to resolve file descriptors.
* **Small buffers:** Minor formatting buffers (like building paths in `activities` or `spy`) are safely capped at 256 bytes.
