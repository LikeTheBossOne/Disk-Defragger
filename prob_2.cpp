/* apott blboss - Aaron Ott and Brendan Boss */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <unistd.h>


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


/*
 * This is a function just to clean up the reading of the main method
 * It should read all the inodes into a single array.
 * THIS LIST IS MALLOC'D. IT /MUST/ BE FREED AT THE END OF MAIN()
 * (Once we decide to call it)
 */
std::vector<inode> readAllinodes(unsigned char *buf, superblock *sb) {
    // Find the size of the blocks
    int bsize = sb->blocksize;

    // Create a list of inodes
    std::vector<inode> list;

    // Create an inode pointer to the buffer inodes, to make reading through the inodes easy
    // The equation should just seek out the first element in the inode block and point to it.
    inode *ipointer = (inode *)&(buf[ROOTBLOCK_SIZE *2 + (sb->inode_offset * bsize)]);

    for (int i = 0; ipointer < (inode *)&(buf[1024 + (sb->data_offset * bsize) - sizeof(inode)]); ipointer++, i++) {
        // printf("%d", i);
        list.push_back(*ipointer);
    }


    return list;
}


/* Here's a function to update data block pointer values
*   It needs to take a pointer to the data block pointer (p, as an int pointer),
*   A pointer to int array where data block pointers are being stored
*   A pointer to the next index in the data block array
*   A pointer to the current data Section Index
*   and a pointer to a doneflag (for clearing values)
*
*   If the inode is already done with data blocks, the pointer should be -1 (sets it just to be sure)
*   If the doneflag is false, but the pointer is -1, set the doneflag to 1 (true)
*   Otherwise, push the value in p to the outputList, and update the value of p to dSI, also update dSI
*
*   Returns the value that was in p
*/
int updateDataPointer(int* p, int* list, int* listSize, int* dataSectionIndex, int* doneflag) {
    int returnValue = *p;

    if (*doneflag == 1) {
        *p = -1;
    } else if (!(*doneflag == 1) && *p == -1) {
        *doneflag = 1;
    } else {

        list[(*listSize)++] = *p;
        // I HAVE A VERY BAD FEELING ABOUT THIS //////////////////////////////////////////////////////
        *p = (*dataSectionIndex)++;
    }

    // printf("UPDATED DATA POINTER (no.%d)\nOriginal Value: %d\ndoneflag: %d\n", (*dataSectionIndex) - 1, returnValue, *doneflag);
    // usleep(1500000);
    return returnValue;
}


/*  Here's a function to update data pointers within indirect pointers.
*   Piggybacks off of updateDataPointer.
*   Designed to update the data pointer of the indirect pointer,
*   If the doneflag is still false, we need to read through the pointer's data.
*   We do this by setting a pointer to the beginning of the block,
*   then iterating through the datablock int by int using updateDataPointer
*
*   p is a pointer to the indirect pointer value
*   buf is a pointer to the disk buffer (for referencing in the data)
*   blocksize is the size in bytes of each block
*   dataOffset is the offset of the data blocks
*
*
*/
int updateIndirectPointer(int level, int* p, unsigned char* buf, int blocksize, int dataOffset, int* list, int* listSize, int* dataSectionIndex, int* doneflag) {

    int rc = updateDataPointer(p, list, listSize, dataSectionIndex, doneflag);

    if (!(*doneflag)) {
        int intsPerBlock = blocksize / 4;

        int* datablock = (int*)&(buf[ROOTBLOCK_SIZE * 2 + (dataOffset * blocksize) + (blocksize * rc)]);
        int* limit = datablock + intsPerBlock;

        // NOTE: WE MIGHT NEED TO CHANGE THIS, THIS IS SKETCHY ///////////////////////////////////////
        if (level > 1) {
            while (datablock < limit && rc != -1) {
                rc = updateIndirectPointer((level - 1), datablock, buf, blocksize, dataOffset, list, listSize, dataSectionIndex, doneflag);

                datablock++;
            }
        } else {
            while (datablock < limit && rc != -1) {
                rc = updateDataPointer(datablock, list, listSize, dataSectionIndex, doneflag);

                datablock++;
            }
        }

    }

    return rc;
}


int main(int argc, char *argv[]) {

    // Check to make sure the correct number of arguments is passed
    if (argc != 3) {
        printf("Error: Invalid Arguments\nUse: \"./prob_2 <disk to defrag> <output disk>\"\n");
        return EXIT_FAILURE;
    }

    // Check that the sizes of the data structures are correct
    if (sizeof(inode) != 100 || sizeof(superblock) != 24) {
        printf("Error: Internal. Size of inode and/or superblock struct is wrong.");
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

    // Read in the entire file into an array of unsigned chars
    // Note: ecv = error-checking variable. Can be used elsewhere as needed.
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



    printf("SuperBlock Data\nblocksize: %d\ninode_offset: %d\ndata_offset: %d\nswap_offset: %d\nfree_inode: %d\nfree_block: %d\n",
            sb.blocksize, sb.inode_offset, sb.data_offset,
            sb.swap_offset,sb.free_inode, sb.free_block);


    // Make list of inodes
    std::vector<inode> inodes = readAllinodes(buffer, &sb);



    // Buffer for inodes to be written eventually
    // inode* nodeBuffer = (inode*) malloc (sizeof(inode)*inodes.size());

    // Current index of data section
    int dataSectionIndex = 0;

    int* outputBlockList = (int*) malloc(sizeof(int) * (sb.swap_offset - sb.data_offset));
    int oblSize = 0;




    // Loop through each node and recursively build data section based on pointers.
    // Also update node with new pointers and put it in nodeBuffer
    for (int i = 0; i < (int)inodes.size(); i++) {

        if (inodes[i].nlink == 0) {

            // Code to run if this is an unused inode
            // Making sure it's set up correctly (it's sometimes not)
            // May need to add more code here to make sure the entire node is built correctly
            for (int j = 0; j < 10; j++) {
                inodes[i].dblocks[j] = -1;
            }
            for (int j = 0; j < 4; j++) {
                inodes[i].iblocks[j] = -1;
            }
            inodes[i].i2block = -1;
            inodes[i].i3block = -1;


            // Assign the inode as the next in the list
            // nodeBuffer[i] = inodes[i];
        } else {

            // Code to run when the inode is in use
            inode curNode = inodes[i];

            int doneflag = 0;


            // SO
            // Here's my idea
            // We keep a list (probably a vector, to keep it super simple) of all the data pointers,
            //      in the order they should be loaded into the new disk in.
            // Each time we load a value into the list, we fix the inode value to be what it should be.
            // Then, when we're writing out to the defrag disk, we just read through the list and load
            //      the data blocks in order.




            // FIRST, we need to read through the dblocks
            for (int j = 0; j < 10; j++) {

                updateDataPointer(&(curNode.dblocks[j]), outputBlockList, &oblSize, &dataSectionIndex, &doneflag);

            }

            // SECOND, we need to read through the iblocks
            for (int j = 0; j < 4; j++) {

                updateIndirectPointer(1, &(curNode.iblocks[j]), buffer, sb.blocksize, sb.data_offset, outputBlockList, &oblSize, &dataSectionIndex, &doneflag);

            }

            // THIRD, we need to read through the i2block

            updateIndirectPointer(2, &(curNode.i2block), buffer, sb.blocksize, sb.data_offset, outputBlockList, &oblSize, &dataSectionIndex, &doneflag);

            // FOURTH, we need to read through the i3block

            updateIndirectPointer(3, &(curNode.i3block), buffer, sb.blocksize, sb.data_offset, outputBlockList, &oblSize, &dataSectionIndex, &doneflag);


            inodes[i] = curNode;
        }

    }

    // /* Print statements for testing the readAllinodes function
    //  * Can be modified print whatever you need it to print
    //  *
    //  */
    // for (int i = 0; i < inodes.size(); i++) {
    //     printf("inode data (no%d)\nnext_inode: %d\nprotect %d\nnlink %d\nsize %d\nuid %d\ngid %d\nctime %d\nmtime %d\natime %d\ndblocks [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\niblocks [%d, %d, %d, %d]\ni2block %d\ni3block %d\n", i, inodes[i].next_inode, inodes[i].protect, inodes[i].nlink, inodes[i].size, inodes[i].uid, inodes[i].gid, inodes[i].ctime, inodes[i].mtime, inodes[i].atime, inodes[i].dblocks[0], inodes[i].dblocks[1],inodes[i].dblocks[2],inodes[i].dblocks[3],inodes[i].dblocks[4],inodes[i].dblocks[5],inodes[i].dblocks[6],inodes[i].dblocks[7],inodes[i].dblocks[8],inodes[i].dblocks[9], inodes[i].iblocks[0], inodes[i].iblocks[1], inodes[i].iblocks[2], inodes[i].iblocks[3], inodes[i].i2block, inodes[i].i3block);
    // }


    sb.free_block = dataSectionIndex;



    // Create new disk image
    FILE* defrag = fopen(argv[2], "w");

    // Write RootBlock to defrag
    fwrite(buffer, ROOTBLOCK_SIZE, 1, defrag);
    // Write Superblock to defrag
    fwrite(&sb, sizeof(superblock), 1, defrag);

    // Write the extra space in the superblock Block
    int* superBlockExtra = (int *) calloc((ROOTBLOCK_SIZE - sizeof(superblock)), sizeof(char));
    fwrite(superBlockExtra, (ROOTBLOCK_SIZE - sizeof(superblock)), 1, defrag);
    free(superBlockExtra);

    // Write out all of the inodes
    for (int i = 0; i < (int)inodes.size(); i++) {
        void* nodeP = &(inodes[i]);
        fwrite(nodeP, sizeof(inode), 1, defrag);
    }

    // Write out any extra space for the inode blocks
    int postInodeSpace = ((sb.data_offset - sb.inode_offset) * sb.blocksize) % sizeof(inode);
    int pISVal = 0;
    for (int i = 0; i < postInodeSpace; i++) {
        fwrite(&pISVal, 1, 1, defrag);
    }

    // Write out all of the in-use data blocks
    int dsAddress = ROOTBLOCK_SIZE * 2 + (sb.data_offset * sb.blocksize);
    for (int i = 0; i < dataSectionIndex; i++) {
        void* blockPointer = &(buffer[dsAddress + (sb.blocksize * outputBlockList[i])]);
        fwrite(blockPointer, sb.blocksize, 1, defrag);
    }

    // Write out all of the free data blocks
    for (int i = dataSectionIndex + 1; i < (sb.swap_offset - sb.data_offset); i++) {
        fwrite(&i, sizeof(int), 1, defrag);
        for (int j = 0; j < sb.blocksize - sizeof(int); j++) {
            fputc('\0', defrag);
        }
    }
    int neg1 = -1;
    fwrite(&neg1, sizeof(int), 1, defrag);
    for (int j = 0; j < sb.blocksize - sizeof(int); j++) {
        fputc('\0', defrag);
    }

    // Write out any empty space in the swap section (beyond the data blocks)
    fwrite(&buffer[(ROOTBLOCK_SIZE * 2) + (sb.swap_offset * sb.blocksize)], (stBuf.st_size - (sb.swap_offset * sb.blocksize) - (ROOTBLOCK_SIZE * 2)), 1, defrag);

    // Close buffers
    fclose(defrag);
    free(buffer);

    // Return and cry for joy
    return EXIT_SUCCESS;
}
