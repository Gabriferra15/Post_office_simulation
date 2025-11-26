# 📬 Post Office Simulation

**Operating Systems Project | Università di Torino**

![Language](https://img.shields.io/badge/Language-C11-00599C?style=flat&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat&logo=linux&logoColor=black)
![IPC](https://img.shields.io/badge/IPC-System_V-ff69b4?style=flat&logo=linux&logoColor=black)

> A multi-process simulation of a post office designed to demonstrate **UNIX process management**, **System V IPC primitives**, and **concurrency synchronization techniques**.

---

## 📖 Overview

This project simulates the daily operations of a post office by modeling the interaction between distinct actors. The goal is to build a robust concurrent system that correctly handles shared resources and synchronization without deadlocks.

The simulation involves four main process types:
* **Director:** Manages the office state (open/closed) and the daily timeline.
* **Ticket Machine:** Randomly generates users and assigns them a specific service type.
* **Operators:** Serve users based on the requested service.
* **Users:** Enter the office, queue up for a service, and wait to be served.

---

## 🚀 Key Features

### 🏗️ Multi-Process Architecture
Each component is implemented as an independent process spawned via `fork()` and `execvp()`:

* **Director (`main`):** Controls the "workday," initializes IPCs, and manages the open/close status.
* **Ticket Machine (`ticket`):** A generator process that spawns User processes with assigned service types.
* **Operators (`operatore`):** They consume requests from semaphore-based queues, simulate service time, and update shared statistics.
* **Users (`user`):** They enter the system, signal their presence via semaphores, and contribute to the office load.

### 🔄 Inter-Process Communication (IPC)
Synchronization is achieved exclusively using **System V IPC** primitives:

#### 📌 Shared Memory (`shmget`, `shmat`)
Accessible by all processes to store the global state:
* **Office Status:** Open/Closed flag.
* **Daily Statistics:** Total users served, total service time.
* **Occupancy:** Number of busy desks and waiting users.
* **Mapping:** User-to-Operator assignment tracking.

#### 📌 Semaphores (`semget`, `semop`)
Used for strict synchronization and locking:
* **`SEM_MUTEX`**: Protects access to Shared Memory (Critical Sections).
* **`SEM_START`**: Synchronization barrier to start the simulation.
* **`SEM_QUEUE_BASE + i`**: Represents the queue for each service type (Operators consume, Users signal).
* **Sync:** Coordination between Operators and the Director for breaks/end-of-day.

> **Note:** This version relies entirely on Semaphores and Shared Memory. Message Queues are not used.

### ⚙️ Dynamic Office Logic

#### 🕒 The Timeline (Director)
1.  Initializes resources.
2.  Opens the office (signals start).
3.  Simulates the passing of time.
4.  Closes the office (no new users allowed).
5.  Wait for operators to finish and cleans up IPCs.

#### 👥 User Behavior
* Users enter only if the office is open.
* They select a random service.
* They perform a `V()` operation on the specific service semaphore to signal presence.
* They wait for the office to close (statistical model).

#### 🧑‍💼 Operator Behavior
* Wait for a free spot at a desk.
* Consume requests from their specific service queue (Semaphore `P()` operation).
* Simulate service time (sleep).
* Update global statistics in shared memory.

---

## 📊 Statistics & Logging
The system tracks real-time data in Shared Memory:
* Number of users served per service category.
* Total simulated service time.
* Total users generated vs. users who entered vs. users rejected.
* Desk occupancy.

At the end of the simulation, the Director prints a summary report.

---

## 🛠️ Tech Stack

* **Language:** C (GNU11 Standard)
* **OS:** Linux
* **System Calls:** `fork()`, `waitpid()`, `execvp()`, Signals
* **IPC Primitives:**
    * `shmget`, `shmat`, `shmdt` (Shared Memory)
    * `semget`, `semop`, `semctl` (Semaphores)
* **Build Tool:** GNU Make

---

## 📂 Project Structure

```text
.
├── src/
│   ├── main.c           # Director logic & IPC initialization
│   ├── operatore.c      # Operator process logic
│   ├── user.c           # User process logic
│   └── ticket.c         # Ticket Machine (User generator)
├── include/
│   └── common.h         # Shared constants, structs, and headers
├── conf/
│   ├── config_timeout.conf  # Configuration for timeouts
│   └── config_explode.conf  # Configuration for high-load tests
├── Makefile             # Compilation instructions
└── README.md
