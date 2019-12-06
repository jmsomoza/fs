#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>

using namespace std;

typedef struct {
	char name[5];        // Name of the file or directory
	uint8_t used_size;   // Inode state and the size of the file or directory
	uint8_t start_block; // Index of the start file block
	uint8_t dir_parent;  // Inode mode and the index of the parent inode
} Inode;

typedef struct {
	char free_block_list[16];
	Inode inode[126];
} Super_block;

typedef struct { uint8_t block[1024]; } Block; 
Block blocks[127]; // Holds representation of the disk data blocks
uint8_t buffer[1024]; // Data buffer for read/write operations
char * input_file; // Input filename for running file system commands
char * disk; // Name of mounted disk
int line_no; // Input file line number for error handling
int cwd; // Current working directory
char root[5] = "root";
std::map<char *, std::vector<int>> dir_names; // Map to store parent directory names and its children
Super_block * superblock; 

void init(){
	disk = (char*)malloc(sizeof(uint8_t) * 20);
	superblock = new Super_block();
	line_no = 0;
}

/**
 * @brief Function to set the ith bit of a byte
 *
 * @param ch - The unsigned char containing the bit to be set
 * @param i - bit offset
 * @param val - The value to be set(1 or 0)
 * @return The new byte with modified bit
 */
unsigned char setBit(unsigned char ch, int i, int val ){
	i = 8-i;
	unsigned char mask = 1 << i ;
	if( val == 1){ return ch | mask;}
	else{ 
		mask = ~mask;
		return ch & mask;
	}
} 

/**
 * @brief Function to zero out the members of an inode
 *
 * @param index - Inode index
 */
void clear_inode(int index){
	char mask[5] =  {'\0', '\0', '\0', '\0', '\0'};
	memcpy(superblock->inode[index].name, mask, 5);
	superblock->inode[index].used_size = 0;
	superblock->inode[index].dir_parent = 0;
	superblock->inode[index].start_block = 0;
}

/**
 * @brief Function to zero out data block
 *
 * @param index - block number
 */
void clear_block(int index){
	uint8_t mask[1024] = "";
	memcpy(blocks[index].block, mask, 1024);
}

/**
 * @brief Checks if inode represents a director
 *
 * @param index - Inode index
 * @return Boolean true if inode is a directory
 */
bool isDir(int index){
	return ((superblock->inode[index].dir_parent & 128) == 128);
}

/**
 * @brief Checks if the given file or directory name exists within the given directory name
 *
 * @param dir - directory name
 * @param name - file or directory name
 * @return Inode index of the file/directory name or -1 if not found
 */
int check_dir_names(char * dir, char * name){
	std::map<char *, std::vector<int>>::iterator dir_it;
	std::vector<char *> names; // each dir has vector of file names
	for (dir_it = dir_names.begin(); dir_it != dir_names.end(); dir_it++ ){ 
		if(strncmp(dir_it->first, dir, 5)==0){
			std::vector<int> val = dir_it->second; // list of inode indeces belonging to that dir
			for (int i=0; i<(int)val.size(); i++){
				if (strncmp(superblock->inode[val.at(i)].name, name, 5)==0) { return val.at(i); } // check if file names match
			}
		}
	}
	return -1;
}

/**
 * @brief Writes data to the mounted disk. Called after every write operation
 *
 * @param disk_name - name of the disk to write to
 */
void write_to_disk(char *disk_name){
	// update disk contents
	ofstream fs;
	fs.open(disk_name);
	if(fs.fail()){ fprintf(stderr, "Error: Failure to write to disk %s\n", disk_name); }
	else {
		char block_buf[1024];
		char buf[1];
		fs.write(superblock->free_block_list, sizeof(superblock->free_block_list));
		for(int i=0; i<126; i++){
			fs.write( superblock->inode[i].name, 5);
			buf[0] = superblock->inode[i].used_size;
			fs.write( buf, 1);
			buf[0] = superblock->inode[i].start_block;
			fs.write( buf, 1);
			buf[0] = superblock->inode[i].dir_parent;
			fs.write( buf, 1);
		}
		for(int i=0; i<127; i++){
			memcpy(block_buf, blocks[i].block, 1024);
			fs.write(block_buf, 1024);
		}
		fs.close();
	}
}

/**
 * @brief Function to mount the disk. Loads the disk superblock and performs six consistency checks and error handling. 
 *
 * @param new_disk_name - name of the disk to mount
 */
void fs_mount(char *new_disk_name){
	if (strlen(disk)!=0) { write_to_disk(disk); }
	dir_names.empty(); 
	Super_block * loaded_superblock = new Super_block();
	int constraint = 0;
	ifstream fs;
	fs.open(new_disk_name);
	if(fs.fail()){ fprintf(stderr, "Error: Cannot find disk %s\n", new_disk_name); return; }
	else {
		fs.read(loaded_superblock->free_block_list, sizeof(loaded_superblock->free_block_list)); // load free block list
		char buf[1];
		for (int i=0; i<126; i++){
			fs.read(loaded_superblock->inode[i].name, 5);
			fs.read(buf, 1);
			loaded_superblock->inode[i].used_size = buf[0];
			fs.read(buf, 1);
			loaded_superblock->inode[i].start_block = buf[0];
			fs.read(buf, 1);
			loaded_superblock->inode[i].dir_parent = buf[0];
		}
	}

	// populates a free block list to check against list from loaded superblock
	char free_block_check[16] = "";
	for (int i=0; i<126; i++){
		int size = loaded_superblock->inode[i].used_size & 127;
		if ((loaded_superblock->inode[i].dir_parent & 128) == 0){
			if(loaded_superblock->inode[i].start_block < 1 || loaded_superblock->inode[i].start_block > 127) { continue; };
			for (int j = 0; j<size; j++){
				unsigned char prev = free_block_check[(int)((loaded_superblock->inode[i].start_block+j)/8)];
				unsigned char next = setBit(free_block_check[(int)((loaded_superblock->inode[i].start_block+j)/8)], ((loaded_superblock->inode[i].start_block+j)%8)+1, 1);
				if(prev == next){ // to ensure multiple files are not allocated to the same block
					constraint=1;
				} else {
					free_block_check[(int)((loaded_superblock->inode[i].start_block+j)/8)] = 
					setBit(free_block_check[(int)((loaded_superblock->inode[i].start_block+j)/8)], ((loaded_superblock->inode[i].start_block+j)%8)+1, 1);
				}
			}
		}
	}
	free_block_check[0] = setBit(free_block_check[0], 1, 1);

	// if both lists don't match, disk is inconsistent
	if (memcmp(free_block_check, loaded_superblock->free_block_list, 16)!=0) { constraint = 1; }

	// second consistency check: names in same dir must be unique
	if (constraint==0){
		dir_names.clear();
		for (int i=0; i<126; i++){
			if((loaded_superblock->inode[i].used_size & 128)!=0){ // dont check free inodes
				int parent = (loaded_superblock->inode[i].dir_parent & 127);
				if(parent == 127){
					if(dir_names.find(root)==dir_names.end()){ // if parent name does not exist in map of directory names and their indeces, create new entry
						std::vector<int> index;
						index.push_back(i);
						dir_names[root] = index;
					} else {
						dir_names[root].push_back(i); // update list of indeces with same parent name
					}
				} else {
					if(dir_names.find(loaded_superblock->inode[parent].name)==dir_names.end()){ // if parent name does not exist in dir map
						std::vector<int> index;
						index.push_back(i);
						dir_names[loaded_superblock->inode[parent].name] = index;
					} else {
						dir_names[loaded_superblock->inode[parent].name].push_back(i); // update list of inodes indeces with the same parent name
					}
				}
			}
		}

		std::map<char *, std::vector<int>>::iterator dir_it;
		std::vector<char *> names; // each dir has vector of file names
		for (dir_it = dir_names.begin(); dir_it != dir_names.end(); dir_it++ ){
			names.clear();
			std::vector<int> val = dir_it->second; // list of inode indeces belonging to that dir
			names.push_back(loaded_superblock->inode[val.at(0)].name); // populate list of names
			for (int i=1; i<(int)val.size(); i++){
				for (int j=0; j<(int)names.size(); j++){
					if (memcmp(loaded_superblock->inode[val.at(i)].name, names.at(j), 5)!=0){ // check new name against list of names
						names.push_back(loaded_superblock->inode[val.at(i)].name); // if unique, add to list
						break;
					} else { constraint = 2; }
				}
			}
		}
	}

	if (constraint==0){
	// third consistency check: if inode state is free, all bits must be zero, otherwise name will have at least one bit not zero
		char name_mask[5] = "";
		for (int i=0; i<126; i++){
			if ((loaded_superblock->inode[i].used_size & 128)==0){
				if (memcmp(loaded_superblock->inode[i].name, name_mask, 5)!=0 || loaded_superblock->inode[i].used_size!=0 || loaded_superblock->inode[i].start_block!=0 || loaded_superblock->inode[i].dir_parent!=0){
					constraint = 3;
				}
			} else { if(memcmp(loaded_superblock->inode[i].name, name_mask, 5)==0){ constraint = 3; } }
		}
	}
				
	// fourth consistency check: start block of every inode marked as a file must have a value between 1 and 127
	if (constraint==0){
		for (int i=0; i<126; i++){
			if((loaded_superblock->inode[i].dir_parent & 128)==0 && (loaded_superblock->inode[i].used_size & 128)!=0){ // file and in use
				if ((loaded_superblock->inode[i].start_block & 127)==0 || (loaded_superblock->inode[i].start_block & 128)==128){
					constraint = 4;
				}
			}
		}
	}

	// fifth consistency check: size and startblock of dir must be marked as zero
	if (constraint==0){
		for (int i=0; i<126; i++){
			if((loaded_superblock->inode[i].dir_parent & 128)!=0){
				if((loaded_superblock->inode[i].used_size & 127)!=0 || (loaded_superblock->inode[i].start_block & 255)!=0){
					constraint = 5;
				}
			}
		}
	}

	// sixth consistency check: index of parent inode cannot be 126 and must be in use and a dir
	if (constraint==0){
		for (int i=0; i<126; i++){
			if ((loaded_superblock->inode[i].used_size & 128)!=0){
				unsigned char dir = (loaded_superblock->inode[i].dir_parent & 127);
				if (dir == 126){
					constraint = 6;
				} else if (dir >= 0 && dir < 126) {
				 	if ((loaded_superblock->inode[dir].dir_parent & 128)==0 || (loaded_superblock->inode[dir].used_size & 128)==0){ constraint = 6; }
				}
			}
		}
	}

	if(constraint!=0){ // Error handling
		fprintf(stderr, "Error: File system in %s is inconsistent (error code: %i)\n", new_disk_name, constraint);
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
	} else { // load superblock, set mounted disk name and set current working directory to root
		char block_buffer[1024];
		strcpy(disk, new_disk_name);
		superblock = loaded_superblock;
		for(int i=0; i<127; i++){ // copy disk data blocks
			fs.read(block_buffer, 1024);
			memcpy(blocks[i].block, block_buffer, 1024);
		}
		cwd = 127;
	}
	fs.close();

}

/**
 * @brief Function to create a new file or directory (if size is 0) in the current working directory
 *
 * @param name - file/directory name
 * @param size - file size
 */
void fs_create(char name[5], int size){
	int index = 127;
	int exists = -1;
	bool space = 0;
	for (int i=0; i<126; i++){
		if (superblock->inode[i].used_size<128){ //inode is available
			index = i;
			break; // use the first available inode
		}
	}
	if (index == 127) { fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %s\n", disk, name); return; }
	else {
		if (cwd == 127){ exists = check_dir_names(root, name); } 
		else { exists = check_dir_names(superblock->inode[cwd].name, name); } // check if filename exists in the current working directory
		if (exists!=-1) { fprintf(stderr, "Error: File or directory %s already exists\n", name); }
		else { //create the file or dir
			clear_inode(index);
			if (size == 0) { // create dir
				memcpy(superblock->inode[index].name, name, 5);
				superblock->inode[index].used_size = 128;
				superblock->inode[index].dir_parent = (cwd | 128);
				dir_names.insert(std::pair<char *,std::vector<int>>(name, std::vector<int>()));
			} else { //check free block list to create file
				int count = 0;
				int block = 0; 
				int start_block = 0;
				for (int i=0; i<16; i++){ // check contiguous free blocks in free block list of size 
					for (int j=0; j<8; j++){
						if (!(superblock->free_block_list[i] >> (7-j)) & 1) { count +=1; } else { count = 0; }
						block +=1;
						if (count == size ) { space = true; start_block = block-size+1; break; } // found space
					}
				}
				if (!space) { fprintf(stderr, "Error: Cannot allocate %i KB on %s\n", size, disk); }
				else {
					// set inode attributes
					memcpy(superblock->inode[index].name, name, 5);
					superblock->inode[index].used_size = size | 128;
					superblock->inode[index].dir_parent = cwd;
					superblock->inode[index].start_block = start_block;
					for(int j=0; j<size; j++){ // update free block list
						superblock->free_block_list[(int)((start_block+j)/8)] = setBit(superblock->free_block_list[(int)((start_block+j)/8)], ((start_block+j)%8)+1, 1);
					}
				}
			}
			// update map of parent directories to include new inode
			if ( cwd==127 ){ dir_names[root].push_back(index); }  else { dir_names[superblock->inode[cwd].name].push_back(index); }

		}
	}
}

/**
 * @brief Function to delete a file or directory and its files recursively, within the current working directory
 *
 * @param name - file/directory name
 */
void fs_delete(char name[5]){
	int exists = -1;
	char * dir;
	if (cwd==127) { dir = root; } else { dir = superblock->inode[cwd].name; }
	exists = check_dir_names(dir, name);
	if (exists == -1) { fprintf(stderr, "Error: File or directory %s does not exist\n", name); }
	else {
		if(isDir(exists)){ // if name is a dir
			for(int i=0; i<(int)dir_names[name].size(); i++){ // delete its children recursively
				fs_delete(superblock->inode[dir_names[name].at(i)].name);
			}
			dir_names.erase(name); // remove from map of directory names
			clear_inode(exists);
		} else {
			for (int i=0; i<(superblock->inode[exists].used_size & 127); i++){ // update free block list and clear blocks
				superblock->free_block_list[(int)((superblock->inode[exists].start_block+i)/8)] = setBit(superblock->free_block_list[(int)((superblock->inode[exists].start_block+i)/8)], ((superblock->inode[exists].start_block+i)%8)+1, 0);
				clear_block(superblock->inode[exists].start_block+i);
			}
			for (int j=0; j<(int)dir_names[dir].size(); j++){ // update map of directory names
				if (dir_names[dir].at(j) == exists) { dir_names[dir].erase(dir_names[dir].begin()+j); }
			}
			clear_inode(exists);
		}
	}
}

/**
 * @brief Function to read from a specified block from a file and writes the data into the buffer
 *
 * @param name - file name
 * @param block_num - the nth block of the file
 */
void fs_read(char name[5], int block_num){
	int exists = -1;
	if (cwd==127) { exists = check_dir_names(root, name); }
	else { exists = check_dir_names(superblock->inode[cwd].name, name); }
	if (exists==-1){ fprintf(stderr, "Error: File %s does not exist\n", name);}
	else if (isDir(exists)){ fprintf(stderr, "Error: File %s does not exist\n", name);}
	else {
		if ( block_num > ((superblock->inode[exists].used_size & 127)-1) || block_num < 0){
			fprintf(stderr, "Error: %s does not have block %i\n", name, block_num);
		} else { 
			memcpy(buffer, blocks[superblock->inode[exists].start_block + block_num].block, 1024);
		}
	}
}

/**
 * @brief Function to write the contents of the data buffer to a specified block of a file
 *
 * @param name - file name
 * @param block_num - the nth block of the file
 */
void fs_write(char name[5], int block_num){
	int exists = -1;
	if (cwd==127) { exists = check_dir_names(root, name); }
	else { exists = check_dir_names(superblock->inode[cwd].name, name); }
	if (exists==-1){ fprintf(stderr, "Error: File %s does not exist\n", name);}
	else if (isDir(exists)){ fprintf(stderr, "Error: File %s does not exist, with index %i\n", name, exists);}
	else {
		if ( block_num > ((superblock->inode[exists].used_size & 127)-1) || block_num < 0){
			fprintf(stderr, "Error: %s does not have block %i\n", name, block_num);
		} else { 
			memcpy(blocks[superblock->inode[exists].start_block + block_num].block, buffer, 1024);
		}
	}
}

/**
 * @brief Populates the buffer with given data
 *
 * @param buff - data buffer with contents to write into the file system buffer
 */
void fs_buff(uint8_t buff[1024]){
	uint8_t b[1024] = "";
	memcpy(buffer, b, sizeof(b));
	memcpy(buffer, buff, 1024);
}

/**
 * @brief Function to print out files and directories located in the current working directory. Files will display its size and
 *  directories will display its number of children.
 */
void fs_ls(void){
	int index;
	char * cwd_name;
	int parent_child;
	int child;
	if (cwd == 127 ) {
		cwd_name = root;
		child = dir_names[cwd_name].size();
		parent_child = child;
	} else {
		cwd_name = superblock->inode[cwd].name;
		int parent = (superblock->inode[cwd].dir_parent & 127);
		if (parent==127){ parent_child = dir_names[root].size(); } else { parent_child = dir_names[superblock->inode[parent].name].size(); }
		child = dir_names[superblock->inode[cwd].name].size();
	}
	printf(".       %3d\n", child);
	printf("..      %3d\n", parent_child);
	for (int i=0; i<(int)dir_names[cwd_name].size(); i++){
		index = dir_names[cwd_name].at(i);
		if (isDir(index)){
			int num_child = dir_names[superblock->inode[index].name].size();
			printf("%-5.5s   %3d\n", superblock->inode[index].name, num_child);
		} else {
			if (superblock->inode[index].used_size!=0) { printf("%-5.5s   %3d KB\n", superblock->inode[index].name, (superblock->inode[index].used_size & 127));}
		}
	}
}

/**
 * @brief Function to resize a file in the current working directory to a specified size. 
 * If there are no sufficient free blocks after its allocated blocks, find contiguous free blocks of the specified size.
 *
 * @param name - filename to be resized
 * @param new_size 
 */
void fs_resize(char name[5], int new_size){
	int exists = -1;
	int start_block = 0;		
	if (cwd==127) { exists = check_dir_names(root, name); }
	else { exists = check_dir_names(superblock->inode[cwd].name, name); }	
	if (exists==-1){ fprintf(stderr, "Error: File %s does not exist\n", name);}
	else if (isDir(exists)){ fprintf(stderr, "Error: File %s does not exist\n", name);}
	else {
		if (new_size < (superblock->inode[exists].used_size & 127)){ // if size is reduced, clear out end blocks 
			for (int i=0; i<((superblock->inode[exists].used_size & 127) - new_size); i++){ // update free block list and clear data blocks
				superblock->free_block_list[(int)((superblock->inode[exists].start_block+new_size+i)/8)] = 
					setBit(superblock->free_block_list[(int)((superblock->inode[exists].start_block+new_size+i/8))], ((superblock->inode[exists].start_block+new_size+i)%8)+1, 0);
				clear_block(superblock->inode[exists].start_block+new_size+i);
			}
			superblock->inode[exists].used_size = new_size;
		} else { // find space
			bool space = false;
			int count = 0;
			int block = 0; 
			for (int i=0; i<16; i++){ // check each block in free block list
				for(int j=0; j<8; j++){
					if (!(superblock->free_block_list[i] >> (7-j)) & 1) { count +=1; } else { count = 0; }
					block +=1;
					if (count == new_size ) { space = true; start_block = block-new_size+1; break; } // space found
				}
			}
			if (!space) { fprintf(stderr, "Error: File %s cannot expand to size %i\n", name, new_size); }
			else {
				for(int j=0; j<new_size; j++){ // update free block list
					superblock->free_block_list[(int)((start_block+j)/8)] = setBit(superblock->free_block_list[(int)((start_block+j)/8)], ((start_block+j)%8)+1, 1);
				}

				for(int j=0; j<((superblock->inode[exists].used_size) & 127); j++){ // transfer data to new data blocks and clear old ones, update free block list
					memcpy(blocks[start_block+j].block, blocks[(superblock->inode[exists].start_block + j)].block, sizeof(blocks[start_block+j].block));
					clear_block(superblock->inode[exists].start_block+j);
					superblock->free_block_list[(int)((superblock->inode[exists].start_block+j)/8)] = setBit(superblock->free_block_list[(int)((superblock->inode[exists].start_block+j)/8)], ((superblock->inode[exists].start_block+j)%8)+1, 0);
				}
				
				// update inode attributes
				superblock->inode[exists].start_block = start_block;
				superblock->inode[exists].used_size = (new_size | 128);
			}
		}
	}
}

 /**
 * @brief Function to reorganize file blocks to reduce fragmentation (no free blocks between used blocks)
 */
void fs_defrag(void){
	std::map<int, int> block_map = std::map<int, int>(); // ordered map of start block and inode number to start from files with lowest start_block
	for (int i=0; i<126; i++){ // populate the map
		if((superblock->inode[i].used_size & 128)!=0 && (superblock->inode[i].dir_parent & 128)==0){
			block_map[superblock->inode[i].start_block] = i;
		}
	}
	std::map<int, int>::iterator it;
	for(it = block_map.begin(); it!=block_map.end(); it++){
		int new_start_block = 0;
		int start_block = (superblock->inode[(it->second)].used_size & 127);
		int block = 0;
		int size = (superblock->inode[(it->second)].used_size & 127);
		for (int i=0; i<16; i++){ // check each block in free block list
			for(int j=0; j<8; j++){
				if (!((superblock->free_block_list[i] >> (7-j)) & 1) && new_start_block == 0) { new_start_block = block;  }
			}
		}
		for (int i=0; i<size; i++){
			memcpy(blocks[new_start_block+i].block, blocks[(it->first)+i].block, 1024); // move data blocks
			if ((new_start_block+size) < (start_block+size) && (new_start_block+size)>(start_block)){ //overlap
				for (int j=0; j<(start_block-new_start_block); j++){
					clear_block(new_start_block+size+j);
				}
			} else {
				clear_block(start_block+i);
			}
			superblock->free_block_list[(int)((start_block+i)/8)] = setBit(superblock->free_block_list[(int)((start_block+i)/8)], ((start_block+i)%8)+1, 0);
		}
		// set free block list
		for (int i=0; i<size; i++){ superblock->free_block_list[(int)((new_start_block+i)/8)] = setBit(superblock->free_block_list[(int)((new_start_block+i)/8)], ((start_block+i)%8)+1, 1); } 
	}
}

 /**
 * @brief Function to change the current working directory to the given directory name
 *
 * @param name - directory name
 */
void fs_cd(char name[5]){
	if (strcmp(name, ".")==0){ // stay in current working directory
		return; 
	} else if (strcmp(name, "..")==0){ //go to parent
		if (cwd==127){ return; } // root has no parent
		else{
			cwd = (superblock->inode[cwd].dir_parent & 127);
		}
	} else {
		char * dir;
		if(cwd==127){ dir = root; } else { dir = superblock->inode[cwd].name; }
		int index = check_dir_names(dir, name); // find directory in the current working directory
		if(index!=-1 && isDir(index)){ cwd = index; }
		else { fprintf(stderr, "Error: Directory %s does not exist\n", name); }
	}
}

/**
 * @brief This function handles the commands read line by line from the input file
 *
 * @param line - the line from the file
 * @param line_no - line number (mostly for error handling)
 */
void process_command(string line, int line_no){
	char cline[line.size()+1];
	strcpy(cline, line.c_str());
 	char* chars_array = strtok(cline, " ");
	std::vector<char*> command_args;
   	while(chars_array)
   	{
	        command_args.push_back(chars_array);
       		chars_array = strtok(NULL, " ");
    	}
	if (cline[0]=='M' && command_args.size()==2 && strlen(command_args.at(1))<=20){ // mount disk
		fs_mount(command_args.at(1));
		if (strlen(disk)!=0) { write_to_disk(disk); }
	} else if (cline[0]=='C' && command_args.size()==3 && strlen(command_args.at(1))<=5 && stoi(command_args.at(2))<128){ // create file/directory
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			fs_create(command_args.at(1), stoi(command_args.at(2)));
			write_to_disk(disk);
		}
	} else if (cline[0]=='D' && command_args.size()==2 && strlen(command_args.at(1))<=5){ // delete file/directory
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			fs_delete(command_args.at(1));
			write_to_disk(disk);
		}
	} else if (cline[0]=='R' && command_args.size()==3 && strlen(command_args.at(1))<=5 && stoi(command_args.at(2))<128 && stoi(command_args.at(2))>=0){ // read from file
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			fs_read(command_args.at(1), stoi(command_args.at(2)));
			write_to_disk(disk);
		}
	} else if (cline[0]=='W' && command_args.size()==3 && strlen(command_args.at(1))<=5 && stoi(command_args.at(2))<128 && stoi(command_args.at(2))>=0){ // write to file
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			fs_write(command_args.at(1), stoi(command_args.at(2)));
			write_to_disk(disk);
		}
	} else if (cline[0]=='B' && command_args.size()>1){ // update data buffer
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			uint8_t b[1024];
			for (int i = 1; i<(int)command_args.size(); i++){
				memcpy(b, command_args.at(i), sizeof(b));
			}
			fs_buff(b);
		}
	} else if (cline[0]=='L' && command_args.size()==1){ // list files and directories in current working directory
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{ fs_ls(); }
	} else if (cline[0]=='E' && command_args.size()==3 && strlen(command_args.at(1))<=5){ // resize file
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			fs_resize(command_args.at(1), stoi(command_args.at(2)));
			write_to_disk(disk);
		}
	} else if (cline[0]=='O' && command_args.size()==1){ // defragment disk
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{
			fs_defrag();
			write_to_disk(disk);
		}
	} else if (cline[0]=='Y' && command_args.size()==2 && strlen(command_args.at(1))<=5){ // change working directory
		if(strlen(disk)==0){ fprintf(stderr, "Error: No file system is mounted\n"); }
		else{ fs_cd(command_args.at(1)); }
	} else {
		fprintf(stderr, "Command Error: %s, %i\n", input_file, line_no);
	}
}

int main(int argc, char *argv[]){
	init();
	if (argc!=2){
		fprintf(stderr, "Error: Incorrect number of arguments");
	}
	else{
		input_file = argv[1];
		string line;
		ifstream inFile;
		inFile.open(input_file);
		if (inFile.is_open()) {
			while (!inFile.eof()) {
				getline(inFile, line);
				line_no +=1;
				process_command(line, line_no);

			}
			inFile.close();
		}
	}
}


