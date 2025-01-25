#!/bin/bash
#SBATCH --job-name=Irene                    # Job name
#SBATCH --output=output_seq_weak_%j.txt     # Standard output and error log (with job number)
#SBATCH --nodes=1                           # Run on a single node
#SBATCH --ntasks=1                          # Number of tasks (processes)
#SBATCH --time=01:50:00  


for i in 0 1 2 3 4; do
    echo "Run,$i"
    for m in 5 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160 170 180 190 200; do
        DATASET_PATH="/home/i.dovichi/project/Sequential/data_weak/$(($m))txt.txt"
        echo "Compression,$m,$(/home/i.dovichi/project/Sequential/mainseq -C 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"

        DATASET_PATH="/home/i.dovichi/project/Sequential/data_weak/$(($m))txt.txt.zip"
        echo "Decompression,$m,$(/home/i.dovichi/project/Sequential/mainseq -D 1 $DATASET_PATH | grep 'Elapsed time' | awk '{print $3}')"
    done
done