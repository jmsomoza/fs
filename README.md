<h1>CMPUT379: A Trivial UNIX File System</h1>
<h2>Jeanna Somoza </h2>
<h4>Design</h4>
For this assignment, we are tasked to write a file system simulator, which will reside on a 128KB disk. Our implementation should be able to handle 10 commands given in a text file with a specified format. To run the simulator, an input file with the list of commands is passed as an argument. 
<br>
The ten commands our file system is able to handle are:

* <code>M [disk name] </code><br>
  This command calls the <i>fs_mount</i> function which takes a disk name as the input and performs six consistency checks before mounting the disk. In performing the consistency checks, I first loaded the superblock of the disk and checked its free block list against its inodes. To check uniqueness of filenames, I maintained a map of directory names and a vector of the indeces of its children. This also helps a lot in other functions. It is only after passing the consistency checks do I load the superblock and set the current working directory to root, which is represented as a variable <code>cwd</code> storing the integer of the inode of the current working directory (127 for root).

* <code>C [file name] [size]</code><br>
  This command calls the <i>fs_create</i> function which takes a file name and its size (in blocks) as the input. If the specified size is 0, that means a directory is to be created. The main challenge to this implementation is finding contiguous blocks which can accomodate a file of that size. I found this was easier done by checking the free block list. The first available inode is used, which is done by iterating through the superblock's inode list. From there, the inode attributes are updated based on the start block, parent directory (which is the current working directory), file size, file type, and of course name and state. The free block list and map of parent directory names are also updated.

* <code>D [file name]</code><br>
  This command calls the <i>fs_delete</i> function deletes the given file or directory if it exists in the current working directory. To achieve this, the map of directory names and its children are checked. In fact, for most commands involving files located in the current working directory, this map is often referred to. If the file/directory exists, its inode and data blocks are cleared. The free block list and directory map are updated.
  
* <code>R [file name] [block number]</code><br>
   This command calls the <i>fs_read</i> reads from the nth block of a file and writes the data into a 1KB buffer used by the entire file system for holding data for read/write operations. Once again, the file name is checked against the files in the current working directory using the directory map.

* <code>W [file name] [block number]</code><br>
  This command calls the <i>fs_write</i> function which, similar to the R command, uses the system-wide data buffer to write into the nth block of the file (both given as arguments). The file name must also exist in the current working directory.
  
* <code>B [new buffer characters]</code><br>
  This command calls <code>fs_buff</code> which does not interact directly with disk data. It populates the data buffer with the characters provided as arguments.
  
* <code>L</code><br>
  This command calls the <code>fs_ls</code> function which is similar to <code>ls</code> command in that it prints out the files and directories located in the current working directory. Files would display their file size and directories would display the number of files/directories they contain. The map of parent directories is once again very useful for this. The directories '.' and '..' are also included.
  
* <code>E [file name] [new size]</code><br>
  This command calls the <code>fs_resize</code> function which changes the size of the file to the specified new size. If the new size is less than the current size, the last blocks are cleared to reduce the size. If the size is increased, it searches for contiguous free blocks of that size to hold the file. If no contiguous blocks are found, the file cannot be resized. Finding the free blocks are achieved by checking the free block list, similar to how files were created. The old data blocks are then moved and the inode's attributes (used size and start block) are updated.
  
* <code>O </code><br>
  This command calls the <code>fs_defrag</code> function, which reorganizes the disk's data blocks to clear out free blocks between used blocks. I first sorted the file inodes by start block, moving ones with the lower start blocks first. I would then go through the free block list, update the inode's start block to the first available data block found. Similar to the resize function, I would move the data blocks, update the inode attributes (only the start block this time) and update the free block list.
  
* <code>Y [directory name]</code><br>
  This command calls the  <code>fs_cd</code> function, which is similar to the <code>cd</code> command in that it changes the current working directory to the directory named passed as an argument to this command. First, the directory name is checked against the directories that exist within the current directory. The arguments '.' and '..' are also considered. The variable <code>cwd</code> which holds the inode index of the current directory is updated.

<h4>Testing</h4>
For testing and debugging, I made use of the four sample test cases, as well as the consistency checks made available to us on eClass. All of the test cases have passed.

<h4>Sources</h4>
  * Class notes and slides on file systems and their implementation
  * [C++ Reference Page](http://www.cplusplus.com/reference/)
  * Various StackOverflow pages for debugging
  * eClass Discussion Board
