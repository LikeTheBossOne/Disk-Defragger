#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

#define ROOTBLOCK_SIZE 512
#define N_DBLOCKS 10
#define N_IBLOCKS 4

typedef struct {
    int next_inode;             /* list for free inodes */
    int protect;                /* protection field */
    int nlink;                  /* Number of links to this file */
    int size;                   /* Number of bytes in file */
    int uid;                    /* Owner's user ID */
    int gid;                    /* Owner's group ID */
    int ctime;                  /* Time field */
    int mtime;                  /* Time field */
    int atime;                  /* Time field */
    int dblocks[N_DBLOCKS];     /* Pointers to data blocks */
    int iblocks[N_IBLOCKS];     /* Pointers to indirect blocks */
    int i2block;                /* Pointer to doubly indirect block */
    int i3block;                /* Pointer to triply indirect block */
} inode;

typedef struct {
    int blocksize;          /* size of blocks in bytes */
    int inode_offset;       /* offset of inode region in blocks */
    int data_offset;        /* data region offset in blocks */
    int swap_offset;        /* swap region offset in blocks */
    int free_inode;         /* head of free inode list */
    int free_block;         /* head of free block list */
} superblock;

/**
 * This method was taken from the example given for HW6
 */
int readIntAt(unsigned char *p)
{
    return *(p+3) * 256 * 256 * 256 + *(p+2) * 256 * 256 + *(p+1) * 256 + *p;
}

int printSuperBlock(superblock* sb) {
    std::cout << "Superblock -" << std::endl;
    std::cout << "blocksize: " << sb->blocksize << std::endl;
    std::cout << "inode_offset: " << sb->inode_offset << std::endl;
    std::cout << "data_offset: " << sb->data_offset << std::endl;
    std::cout << "swap_offset: " << sb->swap_offset << std::endl;
    std::cout << "free_inode: " << sb->free_inode << std::endl;
    std::cout << "free_block: " << sb->free_block << std::endl;
    std::cout << std::endl;
}

int printInodes(unsigned char* buffer, superblock* sb) {
    // Find the size of the blocks
    int bsize = sb->blocksize;

    // Create an inode pointer to the buffer inodes, to make reading through the inodes easy
    // The equation should just seek out the first element in the inode block and point to it.
    inode *ipointer = (inode *)buffer;

    for (int i = 0; ipointer < (inode *)&(buffer[((sb->data_offset - sb->inode_offset) * bsize) - sizeof(inode)]); ipointer++, i++) {
        std::cout << "Inode " << i << " - " << static_cast<void *>(ipointer) << std::endl;
        std::cout << "next_inode: " << ipointer->next_inode << std::endl;
        std::cout << "protect: " << ipointer->protect << std::endl;
        std::cout << "nlink: " << ipointer->nlink << std::endl;
        std::cout << "size: " << ipointer->size << std::endl;
        std::cout << "uid: " << ipointer->uid << std::endl;
        std::cout << "gid: " << ipointer->gid << std::endl;
        std::cout << "ctime: " << ipointer->ctime << std::endl;
        std::cout << "mtime: " << ipointer->mtime << std::endl;
        std::cout << "atime: " << ipointer->atime << std::endl;

        std::cout << "dblocks: [";
        for (int j = 0; j < N_DBLOCKS; j++) {
            std::cout << ipointer->dblocks[j];
            if (j < N_DBLOCKS - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;

        std::cout << "iblocks: [";
        for (int j = 0; j < N_IBLOCKS; j++) {
            std::cout << ipointer->iblocks[j];
            if (j < N_IBLOCKS - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;

        std::cout << "i2block: " << ipointer->i2block << std::endl;
        std::cout << "i3block: " << ipointer->i3block << std::endl;

        std::cout << std::endl;
    }
}

int printBetween(unsigned char* buffer, superblock* sb) {
    std::cout << "Inbetween ints - "<< static_cast<void *>(buffer) << std::endl;

    int* curr = (int *) buffer;
    int runs = (((sb->data_offset - sb->inode_offset) * sb->blocksize) % sizeof(inode)) / 4;
    for (int i = 0; i < runs; i++) {
        std::cout << curr[i];
        if (i < runs - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl << std::endl;
}

int printDataBlock(unsigned char* buffer, superblock* sb) {
    for (int i = 0; i < (sb->swap_offset - sb->data_offset); i++) {
        std::cout << "Data Block " << i << " - " << static_cast<void *>(buffer) << std::endl;
        
        std::cout << "[";
        for (int j = 0; j < sb->blocksize / 4; j++) {
            std::cout << readIntAt(buffer);
            buffer += sizeof(int);

            if (j < (sb->blocksize / 4) - 1) {
                std::cout << ", ";
            }

            if (j % 16 == 15 && j < (sb->blocksize / 4) - 1) {
                std::cout << std::endl;
            }
        }
        std::cout << "]" << std::endl << std::endl;
    }
}

// int printEverythingAfter(unsigned char* buffer, char* file) {
//     struct stat stBuf;
//     stat(file, &stBuf);
//     while (buffer <  stBuf.st_size)
// }

int main (int argc, char* argv[]) {

    if (argc != 2) {
        printf("Error: Invalid Arguments\nUse: \"./prob_2 <disk to defrag>\"\n");
        return EXIT_FAILURE;
    }

    // Open file
    FILE* frag = fopen(argv[1],"r");
    if (frag == NULL) {
        printf("Error: could not open file.");
        return EXIT_FAILURE;
    }

    // Get stats of file
    struct stat stBuf;
    if(stat(argv[1], &stBuf) == -1) {
        printf("Error: stat() could not run on file.");
        fclose(frag);
        return EXIT_FAILURE;
    }

    unsigned char* buffer = (unsigned char *) malloc(stBuf.st_size);
    int ecv = fread(buffer, stBuf.st_size, 1, frag);
    fclose(frag);
    if (ecv != 1) {
        // Error if fread wasn't able to complete correctly
        printf("Error: Something went wrong when reading the file.\n");

        return EXIT_FAILURE;
    }

    superblock sb;
    sb.blocksize = readIntAt(buffer + ROOTBLOCK_SIZE);
    sb.inode_offset = readIntAt(buffer + ROOTBLOCK_SIZE + sizeof(int));
    sb.data_offset = readIntAt(buffer + ROOTBLOCK_SIZE + sizeof(int)*2);
    sb.swap_offset = readIntAt(buffer + ROOTBLOCK_SIZE + sizeof(int)*3);
    sb.free_inode = readIntAt(buffer + ROOTBLOCK_SIZE + sizeof(int)*4);
    sb.free_block = readIntAt(buffer + ROOTBLOCK_SIZE + sizeof(int)*5);

    printSuperBlock(&sb);

    printInodes(&buffer[(ROOTBLOCK_SIZE * 2) + (sb.blocksize * sb.inode_offset)], &sb);

    int location = (sb.data_offset * sb.blocksize) - (((sb.data_offset - sb.inode_offset) * sb.blocksize) % sizeof(inode));
    printBetween(&buffer[(ROOTBLOCK_SIZE * 2) + location], &sb);

    printDataBlock(&buffer[(ROOTBLOCK_SIZE * 2) + (sb.blocksize * sb.data_offset)], &sb);

    // printEverythingAfter(&buffer[sb.swap_offset], argv[1]);
}