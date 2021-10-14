# File explorer application with restricted feature set for Linux

### Basic requirements fulfilled

1. Provides 2 modes: Normal(default) to explore current directory and navigate filesystem, and Command mode to enter shell commands
2. Allows working with both absolute and relative paths
3. Implemented using only C/C++, STL, Linux system calls and a restricted set of libraries including termios.h, dirent.h, fcntl.h etc. 
4. System(), ncurses.h and system commands like ls, mv, cp etc were not used.
5. Root and Home directories are assumed to be the system root "/" and home "/home/username" directories.

### Normal Mode:

1. Displays list of directories and files in current folder, along with their names, sizes, ownership, permissions and last modified time
2. "." and ".." directories are visible
3. Possible to correctly render all text despite any terminal window size
4. Allows navigation using up and arrow keys and opening directories/text files using enter.
5. Allows scrolling via two methods: using up and down arrow keys when cursor is at top or bottom OR pressing k and l keys.
6. Can go to previous or next directory using left and right arrow keys.
7. Can scroll left and right to see truncated content with A and D keys respectively.
8. Can go to parent directory with backspace
9. Can go to directory home directory (/home/username) with h key.
10. Shows "Normal mode" written in last line of window.
11. Exit with q button.

### Command Mode:

1. Entered from normal mode whenever ":" is pressed
2. Can go back to normal mode on pressing ESC key or any button that starts with escape character.
3. Allows typing commands in last line of window
4. Current directory visible even during command mode.
5. If first key pressed is q, exits.
6. Can perform following commands for given actions:- <br />
	-> Copy - `copy sourcepath(s) destpath`: Copies all files/directories referred to in sourcepath(s) to destpath. <br />
	-> Move - `move sourcepath(s) destpath`: Moves all files/directories referred to in sourcepath(s) to destpath. <br />
	-> Rename - `rename foo.txt bar.txt`: Renames foo.txt to bar.txt in current directory. <br />
	-> Create file - `create_file filename destpath`: Creates file 'filename' in existing location 'destpath'. <br />
	-> Create folder - `create_dir dirname destpath`: Creates directory 'dirname' in existing location 'destpath'. <br />
	-> Goto - `goto somepath`: Changes current working directory to somepath and updates screen. <br />
	-> Search - `search filename` or `search dirname`: Returns true or false depending on whether the file or directory exists. Prints accordingly to screen. <br />
7. All above paths and names can allow spaces if enclosed within single quotes

### Display

1. Last line is reserved for showing current working directory in normal mode, and entering shell commands in command mode.
2. Second to last line is reserved as a blank in normal mode, and for showing messages in command mode (like last successful shell command, or any encountered errors).
3. Everything above is used to display a list of all files and folders present in the current working directory in both modes. Details shown include serial number (in current display), filesize (in B, KB, MB... TB), permissions, owner, group, last modified time and finally name. The first character 'd' in permissions denotes whether given item is a directory or not.

### Running:

Compile main.cpp and run the resulting object file. Using g++ for example,

```
$ gcc main.c / g++ main.cpp
$ ./a.out
```

