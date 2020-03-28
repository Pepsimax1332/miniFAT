#include <stdio.h>
#include <stdlib.h>
#include "main.h"


meta_data meta;

void init_fat(meta_data *md, FILE *f) {
    filename fn;
    u_int8_t bl;

    printf("Directory entry size -> %ld\n", sizeof(dir_entry));
    printf("Meta Data size       -> %ld\n", sizeof(meta_data));

    f = fopen(PATH, "rb+");

    /* Load in meta data and set pointer to start of file*/
    fread(md, sizeof(meta_data), 1, f);
    fseek(f, 0, SEEK_SET);

    /* Check to see if storage is in correct format */
    if (FS_VERSION != md->version) {

        /* set pointers in queue */
        md->queue.front = -1;
        md->queue.rear = -1;

        printf("Invalid file system %x not recognised\n", md->version);
        printf("Reformatting to correct file system -> %x\n", FS_VERSION);

        /* Initialise blank filesystems metadata */
        md->version = FS_VERSION;

        /* Set directory meta to blank */
        for (fn = 0; fn < MAX_FILES; fn++) {
            md->dir[fn].startblock = FREE_BLOCK;
            md->dir[fn].length = 0;
            md->dir[fn].currpos = 0;
            md->dir[fn].status = CLOSED_FILE;
        }

        /* Set File Allocation Tables to free and push all blocks to queue */
        for (bl = 0; bl < TOTAL_BLOCKS; bl++) {
            md->fat[bl] = FREE_BLOCK;
            enQueue(md, bl);
        }

        fwrite(md, sizeof(meta_data), 1, f);
    } else {
        printf("File System found v.%u\n", md->version);

        /* Set all files to closed */
        for (fn = 0; fn < MAX_FILES; fn++) {
            md->dir[fn].currpos = 0;
            md->dir[fn].status = CLOSED_FILE;

            /* Print files that are currently in storage */
            if (md->dir[fn].startblock != FREE_BLOCK)
                printf("Found file -> %x; length -> %x\n", fn, md->dir[fn].length);
        }
    }
    display(md);
    fclose(f);
}

/* checks to see if there are enough free blocks and returns number of blocks needed */
u_int8_t scan_blocks(meta_data *md, u_int16_t file_size) {

    u_int16_t as;           // available space
    u_int8_t blks;          // number of blocks needed

    /* checks to see if file_size exceeds total storage capacity */
    if (file_size > EEPROM_LEN) {
        printf("ERROR File is too big for storage device ->          File: %dbytes\n", file_size);
        printf("                                            Storage Space: %dbytes\n", EEPROM_LEN);
        return 0;
    } else {
        /* checks to see if file size exceeds available free space */
        as = availableBlocks(md) * BLOCK_SIZE; // available space
        if (file_size > as) {
            printf("ERROR Not enough block space ->        File: %dbytes\n", file_size);
            printf("                                Block Space: %dbytes\n", as);
            return 0;
        } else {
            /* returns number of blocks needed */
            blks = (file_size / BLOCK_SIZE + (file_size % BLOCK_SIZE != 0));
            printf("Blocks needed for file -> No Blocks -> %d\n", blks);
            return blks;
        }
    }
}

/* Frees up blocks used by file */
void free_blocks(meta_data *md, u_int8_t start) {
    u_int8_t bl;
    u_int8_t next;

    if (md->fat[start] == FREE_BLOCK) printf("ERROR Free block found in chain -> Block: %d\n", start);

    bl = md->fat[start];

    /* frees up blocks in chain and adds the to queue */

    do {
        if (bl == FREE_BLOCK) {
            printf("ERROR Free block found in chain -> Block: %d\n", bl);
            break;
        }
        if (bl == END_BLOCK) {
            enQueue(md, bl);
            break;
        }
        else {
            next = md->fat[bl];
            md->fat[bl] = FREE_BLOCK;
            bl = next;
            enQueue(md, bl);
        }
    } while (bl != END_BLOCK);

}

/* Opens a file to be read */
FILE *open_for_read(meta_data *md, u_int8_t file_name) {

    FILE *f;

    /* Checks to see if file is closed */
    if (md->dir[file_name].status != CLOSED_FILE) {
        printf("ERROR File all ready open -> Filename: %d\n", file_name);
        return 0;
    }

    md->dir[file_name].currpos = 0; // sets the current file position to 0
    md->dir[file_name].status = READ; // sets the current file status to read

    f = fopen(PATH, "rb");

    fseek(f, 0, SEEK_SET);

    return f;
}

/* Opens a file to append */
FILE *open_for_append(meta_data *md, u_int8_t file_name) {

    FILE *f;

    /* Checks to see if the file is closed */
    if (md->dir[file_name].status != CLOSED_FILE) {
        printf("ERROR File all ready open -> Filename: %d\n", file_name);
        return 0;
    }

    /* checks to see if there are blocks available */
    if (isEmpty(md)) {
        printf("ERROR out of storage space\n");
        return 0;
    } else {
        md->dir[file_name].currpos = 0; // Sets current file pointer to 0
        md->dir[file_name].status = APPEND; // Sets current file status to APPEND
        f = fopen(PATH, "rb+");
        fseek(f, 0, SEEK_SET);
        return f;
    }
}

/* opens a file for writing */
FILE *open_for_write(meta_data *md, u_int8_t file_name) {

    FILE *f;

    /* checks to see if file name is within total files allowed */
    if (file_name >= MAX_FILES) {
        printf("ERROR File name exceeds max file limit\n");
        return 0;
    }

    /* checks to see if file is open */
    if (md->dir[file_name].status != CLOSED_FILE) {
        printf("ERROR File all ready open -> Filename: %d\n", file_name);
        return 0;
    }
    /* Checks to see if the start block is a FREE BLOCk if not then frees blocks in fat chain */
    else if (md->dir[file_name].startblock != FREE_BLOCK) {
        printf("Overwriting file -> Filename: %d\n", file_name);
        free_blocks(md, md->dir[file_name].startblock);
    }

    md->dir[file_name].length = 0;

    if (isEmpty(md)) {
        printf("ERROR out of storage space\n");
        return 0;
    } else {
        md->dir[file_name].currpos = 0;
        md->dir[file_name].status = WRITE;
        f = fopen(PATH, "rb+");
        fseek(f, 0, SEEK_SET);

        return f;
    }
}

/* writes data to a file */
u_int8_t write(meta_data *md, u_int8_t file_name, char data[], u_int16_t size, FILE *f) {

    u_int8_t blocks_needed;
    u_int16_t pointer;
    u_int8_t blk;
    u_int8_t next;
    int8_t buffs;

    /* checks to see if file name is valid and does not exceed max number of files */
    if (file_name >= MAX_FILES) {
        printf("ERROR File name exceeds max file limit\n");
        return 0;
    }

    /* checks to see if file is open for writing */
    if (md->dir[file_name].status != WRITE) {
        printf("ERROR file is not open for writing -> Filename %d\n", file_name);
        return 0;
    }

    md->dir[file_name].length = size; // sets filename length
    blocks_needed = scan_blocks(md, md->dir[file_name].length); // gets the number of blocks needed to write to the file
    if (blocks_needed == 0) return 0; // if there are no blocks returns 0
    else {
        blk = deQueue(md); // gets next available block from queue
        md->dir[file_name].startblock = blk; // sets file start block

        // loops through the number of blocks needed and writes data to file
        for (int i = 0; i < blocks_needed; i++) {

            pointer = EEPROM_START + sizeof(meta_data) + blk * BLOCK_SIZE; // sets the pointer to the block position

            printf("Used block -> %d\n", blk);
            printf("At memory address -> %x\n", pointer);
            fseek(f, pointer, SEEK_SET);

            /* writes to the file in stages of BUFFER SIZE */
            buffs = ((size- 1)/BUFFER_SIZE)+1;
            for (int j = 0; j < buffs; j++) {
                char buff[BUFFER_SIZE];
                if (size >= BUFFER_SIZE) {
                    for (u_int8_t k = 0; k < BUFFER_SIZE; k++) {
                        buff[k] = data[(j)*BUFFER_SIZE + k]; //
                    }
                    size -= 8;
                    md->dir[blk].currpos += BUFFER_SIZE;
                    fwrite(&buff, BUFFER_SIZE, 1, f);

                } else {
                    for (u_int8_t k = 0 ; k < size ; k++) {
                        buff[k] = data[(j)*BUFFER_SIZE + k];
                    }
                    md->dir[blk].currpos += size;
                    fwrite(&buff, size, 1, f);
                }
            }
            /* if the file needs more than one block then grab the next block available else set fat to END BLOCK*/
            if (i < blocks_needed -1) {
                next = deQueue(md);
                md->fat[blk] = next; // sets the block in the fat chain to the next available block
                blk = next;
            } else {
                md->fat[blk] = END_BLOCK;
                return 1;
            }
        }
    }
    return 0;
}

/* reads a file from storage and prints contents to terminal*/
u_int8_t read(meta_data *md, u_int8_t file_name, FILE *f, char *data[]) {

    u_int16_t char_cnt;
    u_int16_t pointer;
    u_int8_t blk;
    u_int8_t next;

    /* checks to see if file name is valid and does not exceed max number of files */
    if (file_name >= MAX_FILES) {
        printf("ERROR File name exceeds max file limit\n");
        return 0;
    }

    /* checks to see if file is open for reading */
    if (md->dir[file_name].status != READ) {
        printf("ERROR file is not open for reading -> Filename %d\n", file_name);
        return 0;
    }

    blk = md->dir[file_name].startblock;
    pointer = EEPROM_START + sizeof(meta_data) + BLOCK_SIZE*blk;

    fseek(f, pointer, SEEK_SET);

    /* loop to read file contents */
    while (md->dir[file_name].currpos < md->dir[file_name].length) {

        /* determines the size of data to read based on how much is left to read of the file*/
        if (md->dir[file_name].length - md->dir[file_name].currpos > BLOCK_SIZE) {
            char_cnt = BLOCK_SIZE;
        } else {
            char_cnt = md->dir[file_name].length - md->dir[file_name].currpos;
        }

        char buff[char_cnt]; // array used to read contents

        /* reads data and prints it to terminal based on current position in file */
        if (md->dir[file_name].currpos < md->dir[file_name].length) {

            fread(&buff, char_cnt, 1, f);
            md->dir[file_name].currpos += BLOCK_SIZE;
            printf("\nFile contents ->");
            for (int i = 0 ; i < char_cnt ; i ++) printf("%c", buff[i]);
            printf("\n");

            /* if the current block is not the last block get the next block to read from */
            if (md->fat[blk] != END_BLOCK) {
                next = md->fat[blk];
                blk = next;
                pointer = EEPROM_START + sizeof(meta_data) + BLOCK_SIZE*blk;
                fseek(f, pointer, SEEK_SET);
            } else {
                return 1;
            }
        }
    }
    return 0;
}

/* appends data to the end of the file */
u_int8_t append(meta_data *md, u_int8_t file_name, char data[], u_int16_t size, FILE *f){

    u_int8_t no_blks;
    u_int8_t remaining;
    u_int16_t pointer;
    u_int8_t blk;
    u_int8_t next;

    /* checks to see if file name is valid and does not exceed max number of files */
    if (file_name >= MAX_FILES) {
        printf("ERROR File name exceeds max file limit\n");
        return 0;
    }

    fseek(f, 0 , SEEK_SET);

    /* checks to see if file is open for reading */
    if (md->dir[file_name].status != APPEND) {
        printf("ERROR file is not open for appending -> Filename %d\n", file_name);
        return 0;
    }

    /* checks to see if there is enough space left to append to file */
    if (md->dir[file_name].length + size >= EEPROM_LEN) {
        printf("ERROR nor enough space to append file");
        return 0;
    }

    /* determines the number of blocks needed to write to the file */
    no_blks = (md->dir[file_name].length / BLOCK_SIZE + (md->dir[file_name].length % BLOCK_SIZE != 0));

    if (no_blks == 0) no_blks = 1;
    remaining = no_blks * BLOCK_SIZE - md->dir[file_name].length;

    /* gets a block if it the start block is free */
    if (md->dir[file_name].startblock == FREE_BLOCK) {
        md->dir[file_name].startblock = deQueue(md);
        md->fat[file_name] = END_BLOCK;
    }

    blk = md->dir[file_name].startblock;

    /* cycles through blocks to find the last block */
    while (md->fat[blk] != END_BLOCK) {
        next = md->fat[blk];
        blk = next;
    }

    /* sets a pointer to the current block */
    pointer = sizeof(meta_data) + (blk*BLOCK_SIZE) + (BLOCK_SIZE - remaining);
    fseek(f, pointer, SEEK_SET);

    /* writes to end of file if size to write is less than remaining block space */
    if (size <= remaining) {
        fwrite(data, size, 1, f);
        md->dir[file_name].length += size;
        return 1;

        /* otherwise writes to the rest of the block and cysles on to next block as needed */
    } else {
        char buff[remaining];
        char left[size - remaining];

        /* determines the size needed to append to the file and sets up array */
        for (int i = 0; i < size; i++) {
            if (i < remaining) buff[i] = data[i];
            else left[i - remaining] = data[i];
        }


        fwrite(&buff, remaining, 1, f);
        md->dir[file_name].length += remaining; // adds to the file length

        /* gets the next block needed to write to and sets appends fat */
        next = deQueue(md);
        md->fat[blk] = next;
        blk = next;

        /* gets the number of blocks needed to finish appending the file */
        no_blks = scan_blocks(md, size - remaining);

        size -= remaining;

        /* loops through no of needed blocks */
        for (int i = 0; i < no_blks; i++) {

            pointer = EEPROM_START + sizeof(meta_data) + blk * BLOCK_SIZE - 3;
            printf("Used block -> %d\n", blk);
            printf("At memory address -> %x\n", pointer);
            fseek(f, pointer , SEEK_SET);

            /* if the size left needed is less than the block size write to it and return */
            if (size <= BLOCK_SIZE) {
                char buffer[size];
                for (int j = 0; j < size; j++) buffer[j] = left[(i * BLOCK_SIZE) + j];

                fwrite(&buffer, size, 1, f);
                md->dir[file_name].length += size;
                md->fat[blk] = END_BLOCK;

                return 1;

            }
            /* else write to it and grab the next block and continue */
            else {
                char buffer[BLOCK_SIZE];
                for (int j = 0 ; j < BLOCK_SIZE ; j ++) buffer[j] = left[(i * BLOCK_SIZE) + j];

                fwrite(&buffer, BLOCK_SIZE, 1, f);
                md->dir[file_name].length += BLOCK_SIZE;
                size -= BLOCK_SIZE;

                next = deQueue(md);
                md->fat[blk] = next;
                blk = next;
                printf("%s", buffer);
            }
        }
    }
}

/* closes a file*/
u_int8_t close_file(meta_data *md, int8_t file_name, FILE *f) {

    if (file_name >= MAX_FILES) {
        printf("ERROR File name exceeds max file limit\n");
        return 0;
    }

    if (md->dir[file_name].startblock == FREE_BLOCK) {
        printf("ERROR Cannot close file as it doesnt exist -> File name: %d\n", file_name);
        return 0;
    } else if (md->dir[file_name].status == CLOSED_FILE) {
        printf("ERROR Cannot close file as it is all ready closed -> File name: %d\n", file_name);
        return 0;
    }

    md->dir[file_name].status = CLOSED_FILE;

    /* simulates writing to EEPROM */
    fseek(f, 0, SEEK_SET);
    fwrite(md, sizeof(meta_data), 1, f);
    fclose(f);

    return 1;
}

/* deletes a file */
u_int8_t delete(meta_data *md, u_int8_t file_name) {
    FILE *f;
    u_int8_t start;
    start = md->dir[file_name].startblock;

    if (start == FREE_BLOCK) {
        printf("ERROR cannot delete as file does not exist -> Filename: %d\n", file_name);
        return 0;
    }
    free_blocks(md, start);

    /* initialise directory*/
    md->dir[file_name].startblock = FREE_BLOCK;
    md->dir[file_name].length = 0;
    md->dir[file_name].currpos = 0;
    md->dir[file_name].status = CLOSED_FILE;


    /* simulates writing to EEPROM */
    f = fopen(PATH, "rb+");
    fwrite(md, sizeof(meta_data), 1, f);
    fclose(f);

    printf("File deleted -> Filename: %d", file_name);

    return 1;
}

int main() {
    FILE *file;
    init_fat(&meta, file);

//    printf("Free blocks -> %d\n", availableBlocks(&meta));
//    printf("head-> %d; tail -> %d\n", meta.queue.front, meta.queue.rear);
//    char data[] = "HELLO WORLD!!!";
//
//    file = open_for_write(&meta, 0);
//    write(&meta, 0, data, sizeof(data), file);
//    close_file(&meta, 0, file);
//
//    file = open_for_read(&meta, 0);
//    char data2[meta.dir->length];
//    read(&meta, 0, file, data2);
//    close_file(&meta, 0, file);
//
//
//    file = open_for_append(&meta, 0);
//    char data3[] = "abcdefghijklmnopqrstuvwxyz123456789abcdefghijklmnopqrstuvwxyz123456789abcdefghijklmnopqrstuvwxyz123456789";
//    append(&meta, 0, data3, sizeof(data3), file);
//    close_file(&meta, 0, file);

    return 0;
}