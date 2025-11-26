# 📬 Post Office Simulation

**Operating Systems Project | Università di Torino**

![C](https://img.shields.io/badge/Language-C11-00599C?style=flat&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat&logo=linux&logoColor=black)
![Build](https://img.shields.io/badge/Build-Make-00CC00?style=flat&logo=gnu&logoColor=white)

> A robust multi-process simulation of a post office environment, implemented in C to demonstrate **UNIX process management**, **IPC mechanisms**, and **concurrency control**.

---

## 📖 Overview

This project models the daily operations of a post office, managing the complex interactions between **Customers** (Users), **Clerks** (Workers), and the **Manager** (Director). The goal was to build a system that handles resource contention, synchronization, and process lifecycle management using low-level system calls.

## 🚀 Key Features

### 🏗️ Multi-Process Architecture
The simulation spawns distinct processes for each role, mimicking a real-world distributed system:
* **Director:** Manages the office state (open/close) and resources.
* **Workers:** Serve customers from the queue.
* **Users:** Generate requests and attempt to access the office.

### 🔄 Inter-Process Communication (IPC)
Rigorous synchronization implemented using **System V** / **POSIX** standards:
* **Shared Memory (shm):** For sharing office status and statistics across processes.
* **Semaphores:** To manage access to critical sections and queues.
* **Message Queues:** For structured communication between users and workers.

### ⚙️ Dynamic Office Logic
* **Adaptive Behavior:** Workers serve users based on queue load and office status.
* **Capacity Management:** Users respect physical capacity limits; new processes wait or leave if the office is full.
* **Graceful Shutdown:** Ensures all resources (memory, semaphores) are released correctly upon termination.

### 📊 Statistics & Logging
* Daily summary reports of served vs. rejected users.
* Tracking of waiting times and service times.
* Identification of peak load moments.

---

## 🛠️ Tech Stack

* **Language:** C (GNU11 Standard)
* **OS:** Linux Environment
* **Core Concepts:** `fork()`, `waitpid()`, `execvp()`, Signals
* **IPC Tools:** `shmget`, `semop`, `shmdt`, `shmat`
* **Build Tool:** GNU Make

---

## 📂 Project Structure

```text
.
├── src/
│   ├── main.c       
│   ├── operatore.c  
│   ├── user.c        
│   └── ticket.c     
├── include/          
│   └── common.h     
├── conf/
│   ├── config_timeout.conf      
│   └── config_explode.conf   
├── Makefile      
└── README.md
