This project is a complete simulation of a post office implemented in C as part of the Operating Systems course.
It models the interaction between users, workers, and a director, using real UNIX process management and IPC mechanisms.

🚀 Main Features

Multi-process architecture
The system spawns multiple processes (director, workers, users), each with a specific role.

Inter-Process Communication (IPC)
Implemented using

- Shared memory (shm)
- Semaphores
- Message queues
- Ensures correct synchronization between processes.
  
Dynamic office logic
- The director can open/close the office dynamically.
- Workers serve users based on office state and queue availability.
- Users attempt to access the office respecting capacity limits and office rules.
  
Statistics & logging
- Daily summary of served users
- Waiting times
- Peak load moments
- Graceful shutdown of all processes

🛠️ Technologies Used

- C (GNU11)
- POSIX / System V IPC
- make
- Linux environment

📁 Project Structure

- director.c — Office supervisor logic
- worker.c — Worker processes
- user.c — Incoming customers
- ipc/ — Shared memory, semaphores, message queue utilities
- include/ — Header files
- Makefile

📚 What I learned

- Process creation and lifecycle (fork, wait)
- Synchronization between concurrent processes
- Avoiding deadlocks and race conditions
- Designing complex systems with modular C code
- Debugging multi-process applications with gdb and logging
