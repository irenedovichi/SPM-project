#!/bin/bash
#SBATCH --job-name=Irene                      # Job name
#SBATCH --output=output_ff_weak_%j.txt        # Standard output and error log
#SBATCH --nodes=1                             # Run on a single node
#SBATCH --ntasks=1                            # Number of tasks (processes)
#SBATCH --time=01:50:00 


# Loop over different values of -r (l is fixed to 2)
for i in 0 1 2 3 4; do
    echo "Run,$i"
    for l in 2; do
        for r in 1 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32 34 36 38 40; do
            # Dynamically generate the DATASET_PATH based on r
            DATASET_PATH="/home/i.dovichi/project/SharedMemory/data_weak/$(($r * 5))txt.txt"

            # Compression
            compression_time=$(/home/i.dovichi/project/SharedMemory/mainff -l $l -r $r -C 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')
            echo "Compression,$l,$r,$compression_time"
            

            DATASET_PATH="/home/i.dovichi/project/SharedMemory/data_weak/$(($r * 5))txt.txt.zip"

            # Decompression
            decompression_time=$(/home/i.dovichi/project/SharedMemory/mainff -l $l -r $r -D 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')
            echo "Decompression,$l,$r,$decompression_time"
        done
    done
done