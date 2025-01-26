#!/bin/bash
#SBATCH --job-name=Irene                     # Job name
#SBATCH --output=output_mpi_%j.txt           # Standard output and error log
#SBATCH --nodes=8                            
#SBATCH --ntasks-per-node=1                  # Number of MPI processes per node
#SBATCH --time=01:50:00                      # Time limit hrs:min:sec


DATASET_PATH="/home/i.dovichi/project/Distributed/data_strong"

for i in 0 1 2 3 4; do
    echo "Run,$i"
    for p in 2 3 4 5 6 7 8; do
        echo "Compression,$p,$(mpirun -np $p --map-by ppr:1:node /home/i.dovichi/project/Distributed/mainmpi -C 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"
        echo "Decompression,$p,$(mpirun -np $p --map-by ppr:1:node /home/i.dovichi/project/Distributed/mainmpi -D 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"
    done
done