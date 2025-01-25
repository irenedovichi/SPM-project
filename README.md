# Project 2 - SPM course 23/24
## Overview
In this project, a file compression and decompression program was developed using the Miniz library. A sequential implementation was first created to serve as a baseline reference for performance evaluation. Building upon this, two parallel implementations have been developed:
- A shared-memory version using FastFlow, which utilizes only the pipeline and all-to-all parallel building blocks.
- A distributed-memory version using Open MPI, designed to enable parallelism across multiple nodes in a cluster.

## Project Structure
```
./
├── 📂 Distributed
│   ├── 📄 Makefile
│   ├── 📄 cmdline_mpi.hpp
│   ├── 📄 mainmpi.cpp
│   └── 📄 utility_mpi.hpp
├── 📂 Sequential
│   ├── 📄 Makefile
│   ├── 📄 cmdline_seq.hpp
│   ├── 📄 mainseq.cpp
│   └── 📄 utility_seq.hpp
├── 📂 SharedMemory
│   ├── 📄 Makefile
│   ├── 📄 cmdline_ff.hpp
│   ├── 📄 mainff.cpp
│   └── 📄 utility_ff.hpp
├── 📂 miniz 
├── 📂 shellscripts
├── 📂 src
│   └── 📄 file_generator.py
├── 📄 plots_decomp.ipynb
└── 📄 plots.ipynb
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
The shell scripts that were used to test on the SPM Cluster Machine Backend nodes can be found in the `shellscripts` folder. 

To run one of those script use:
```
sbatch [path-to-script]
```
