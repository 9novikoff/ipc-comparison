# IPC-COMPARISON


## Overview
This repository provides a comprehensive comparison of various Inter-Process Communication (IPC) mechanisms using C. It measures the performance of different IPC methods, including shared memory, mmap, Unix domain sockets, and file read/write operations. The goal is to evaluate and compare their latency, throughput, and efficiency when transmitting data of varying sizes between processes.

## Setup Instructions
```sh
git clone https://github.com/yourusername/ipc-comparison.git
cd ipc-comparison
```

```sh
gcc ipc_comparison.c -o ipc_comparison
```

## Running the Program
Using the default size (1 MB):
```sh
./ipc_comparison
```

Specify a custom message size in bytes:
```sh
./ipc_comparison 1024
```
