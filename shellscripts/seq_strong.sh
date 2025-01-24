#!/bin/bash
#SBATCH --job-name=Irene               # Job name
#SBATCH --output=output_seq_%j.txt     # Standard output and error log (with job number)
#SBATCH --nodes=1                      # Run on a single node
#SBATCH --ntasks=1                     # Number of tasks (processes)
#SBATCH --time=01:50:00            

# Define the dataset path
DATASET_PATH="/home/i.dovichi/project/Sequential/data_seq"

for i in 0 1 2 3 4; do
    echo "Run $i of sequential code."
    /home/i.dovichi/project/Sequential/mainseq -C 0 $DATASET_PATH
done