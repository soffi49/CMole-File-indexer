# CMole (aka File Indexer)
The project was realized as my final assignment for Operating Systems course at WUT. It's a **low-level file indexer** which can be used on **UNIX systems**.\
Remeber, that to use the code, you have to firstly compile it, which you can easily do with the following command (run the command in terminal started in the directory where you put both `mole.c` and `config.h`):
```
gcc main.c -o main -lpthread
```
# 1. Purpose of the project 💡
Program traverses all files in a given directory as well as its subdirectories, creates a data structure containing the requested information about the files and then waits for user input. User inputs commands that query the data gathered in the data structure (which I'll call `index`).
# 2. Commandline arguments ⚙
To use the program write the following command in your terminal (same as where you compile the program): 
```
./mole -d path -f file -t time
```
Where:
- `path`- a path to a directory that will be traversed
- `file`- a path to a file where `index` is stored.
- `time`- an integer from the range [30,7200]. It denotes the time between rebuilds of `index`.

# 3. Specification 📄
## 3.1 Index structure
`Index` stores the following file types:
- directories
- JPEG images
- PNG images
- gzip compressed files
- any files based on zip 
<p>
And about each file, it stores the following informations:
</p>

- file name
- a full (absolute) path
- size
- owners's uid
- type of the file

To perform the *indexing procedure*, the program starts a new thread. The user gets the *stdout notification* when the indexing ends.
## 3.2 User input commands
Program reads commands from stdin (which happens parallely to potential indexing/re-indexing). Following commands are available:
- `exit`- program stops reading commands from stdin. If an indexing is currently in progress, the program waits for it to finish and then the program ends
- `exit!`- quick termintion (works smilarlly to exit). If any indexing is in progress it is canceled. If the result of the indexing if currently being written to a file, the program waits for it to be finished
- `index`- starts re-indexing. If there is currently running indexing operation a warining message is printed
- `count`- calculates the counts of each file type in `index` 
- `largerthan x`- where x is the requested file size, prints full path, size and type of all files in `index` that have size larger than x.
- `namepart y`- where y is a part of a filename, prints information about all files that contain y in the name.
- `owner uid`- where uid is owner's identifier, prints information about all files that owner is uid.
## 3.3 Additional features
1. The last three commands support pagination. It means that the user is able to display output with pagers like *less*. For that, one has to set the `$PAGER` variable.
2. The program is able to perform auto re-indexing if the parameter `-t` is specified
