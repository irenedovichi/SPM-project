# Project 2 - SPM course 23/24
## Overview
In this project, a file compression and decompression program was developed using the Miniz library. A sequential implementation was first created to serve as a baseline reference for performance evaluation. Building upon this, two parallel implementations have been developed:
- A shared-memory version using FastFlow, which utilizes only the pipeline and all-to-all parallel building blocks.
- A distributed-memory version using Open MPI, designed to enable parallelism across multiple nodes in a cluster.

## Project Structure
```
./
├── 📂 
│   ├── 📄 
│   ├── 📄 
│   └── 📄 
├── 📂 
│   ├── 📄 
│   └── 📄 
├── 📄 
├── 📄 
└── 📄 
```
### Sequential Implementation
To compile the code make sure that you are in the `Sequential` folder and use the provided Makefile:
```
cd Sequential
make mainseq
```

---
### MPI Implementation
To compile the code make sure that you are in the `Distributed` folder and use the provided Makefile:
```
cd Distributed
make mainmpi
```

---

### FastFlow Implementation
In order to use FastFlow, get the `fastflow` directory trough the command: 
```
git clone https://github.com/fastflow/fastflow.git
```
Inside `fastflow` you will have a folder named `ff`, run this script inside it: `mapping_string.sh`.

To compile the code make sure that you are in the `SharedMemory` folder and use the provided Makefile:
```
cd SharedMemory
make mainff
```

## Experiments
