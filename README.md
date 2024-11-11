Repository for the Excercises and Project code of the SPM course.


To compile a file named `file_name.cpp` in the folder with path `big_folder/folder_name` do in the (main) terminal: 
- `cd big_folder/folder_name` 
- (check if you are in the folder terminal) `make file_name`
- to execute the file do: `./file_name`

Then, to come back to the main terminal (to make commits!), do: `cd ..` as many times as it requires... or just choose the terminal manually (right area of terminal).


To check where you are and what folders you have available do: `ls`.


One nice thing to know if you have to compile many (like thousands) C++ files: if you do `make -j` you compile everything in parallel, if you do `make -j n` you compile with n cores. So basically you can compile many programs in parallel.

