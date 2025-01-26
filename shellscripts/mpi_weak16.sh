#!/bin/bash
#SBATCH --job-name=Irene                     # Job name
#SBATCH --output=output_mpi_weak_16_%j.txt   # Standard output and error log
#SBATCH --nodes=8                            
#SBATCH --ntasks-per-node=2                  # Number of MPI processes per node
#SBATCH --time=01:50:00                      # Time limit hrs:min:sec


for i in 0 1 2 3 4; do
    echo "Run,$i"
    for p in 8; do
        # Dynamically generate the DATASET_PATH based on p
        DATASET_PATH="/home/i.dovichi/project/Distributed/data_weak/$(($p * 20))txt.txt"
        echo "Compression,$(($p * 2)),$(mpirun -np $p --map-by ppr:2:node /home/i.dovichi/project/Distributed/mainmpi -C 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"

        DATASET_PATH="/home/i.dovichi/project/Distributed/data_weak/$(($p * 20))txt.txt.zip"
        echo "Decompression,$(($p * 2)),$(mpirun -np $p --map-by ppr:2:node /home/i.dovichi/project/Distributed/mainmpi -D 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"
    done
done