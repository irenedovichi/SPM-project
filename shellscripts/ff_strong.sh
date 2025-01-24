#!/bin/bash
#SBATCH --job-name=Irene                      # Job name
#SBATCH --output=output_ff_%j.txt             # Standard output and error log
#SBATCH --nodes=1                             # Run on a single node
#SBATCH --ntasks=1                            # Number of tasks (processes)
#SBATCH --time=01:50:00          

# Define the dataset path
DATASET_PATH="/home/i.dovichi/project/SharedMemory/data_ff"


# Loop over different values of -l and -r
for i in 0 1 2 3 4; do
    echo "Run,$i"
    for l in 1 2 4 7; do
        for r in 1 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32 34 36 38 40; do
            echo "Compression,$l,$r,$(/home/i.dovichi/project/SharedMemory/mainff -l $l -r $r -C 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"
            echo "Decompression,$l,$r,$(/home/i.dovichi/project/SharedMemory/mainff -l $l -r $r -D 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"
        done
    done
done