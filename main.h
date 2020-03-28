#define MAX_FILES       0x000A      // 10
#define EEPROM_LEN      0x0A00      // 2560 bytes
#define EEPROM_START    0x0000      // start at address 0
#define BLOCK_SIZE      0x0040      // 64 bytes
#define BUFFER_SIZE     0x0008      // 8 byte buffer size -- should be set to factor of BLOCK_SIZE

#define TOTAL_BLOCKS    EEPROM_LEN/BLOCK_SIZE   // 40 total number of available blocks
#define FS_VERSION      MAX_FILES + EEPROM_LEN + EEPROM_START + BLOCK_SIZE // automatically reformat if changes to file structure are made

#define PATH            "/home/max/Documents/Semester2/ComputerSystems2/Labs/Lab4/test.img" // change to path name of you "test.img"

#define FREE_BLOCK      0xFE        // entry for free block
#define END_BLOCK       0xFF        // entry for end block
#define CLOSED_FILE     0xFF        // sets current position if file is unopened
#define READ            0xFD        // open for reading
#define WRITE           0xFC        // open for writing
#define APPEND          0xFB        // open for appending

typedef u_int8_t block;
typedef u_int8_t filename;

typedef struct entry {
    block startblock;               // contains the start block of the file
    u_int8_t status;                // contains status flags
    u_int16_t length;               // contains the file length
    u_int16_t currpos;              // contains the current pos
} dir_entry;

typedef struct circ_queue {
    block q[TOTAL_BLOCKS];          // contains free blocks in circular array
    int8_t front;                   // points to front
    int8_t rear;                    // points to rear
} queue;

typedef struct meta {
    u_int16_t version;              // used to check the file system for initialization
    dir_entry dir[MAX_FILES];       // point to entry point in fat for block start
    u_int8_t fat[TOTAL_BLOCKS];     // links block together for file las block ends in EOF
    queue queue;                    // implement circular queue with length of MAX BLOCKS
} meta_data;

/* functions used for circular array */
int8_t isEmpty(meta_data *md);
int8_t isFull(meta_data *md);
void enQueue(meta_data *md, u_int8_t blk);
int8_t deQueue(meta_data *md);
signed short availableBlocks(meta_data *md);
void display(meta_data *md);

/* functions used for manipulating storage */
void init_fat(meta_data *md, FILE *f);
u_int8_t scan_blocks(meta_data *md, u_int16_t file_size);
void free_blocks(meta_data *md, u_int8_t start_pos);
FILE *open_for_read(meta_data *md, u_int8_t file_name);
FILE *open_for_write(meta_data *md, u_int8_t file_name);
FILE *open_for_append(meta_data *md, u_int8_t file_name);
u_int8_t read(meta_data *md, u_int8_t file_name, FILE *f, char *data[]);
u_int8_t write(meta_data *md, u_int8_t file_name, char data[], u_int16_t size, FILE *f);
u_int8_t append(meta_data *md, u_int8_t file_name,char data[], u_int16_t size, FILE *f);
u_int8_t close_file(meta_data *md, int8_t file_name, FILE *f);
u_int8_t delete(meta_data *md, u_int8_t file_name);

/* The following are used functions used to create a circular
 * queue. This was done to add some level of wear leveling to
 * the device. the idea hear is that the first blocks of memory
 * freed for use are to be the last blocks used in storage.
 *
 * However, this only solves the problem of block wear levelling.
 * and does not solve the issue of the wear leveling associated
 * to writing the meta data to the same location.
 *
 * This issue will be left as a task to whoever looks at this code
 * and wants to modify it.
*/

/* Checks to see if the queue is empty. An empty queue implies all free blocks are in use */
int8_t isEmpty(meta_data *md) {
    if (md->queue.front == -1) return -1;
    return 0;
}

/* Checks to see if the queue is full. A full queue implies all blocks are free */
int8_t isFull(meta_data *md) {
    if ((md->queue.front == md->queue.rear + 1) || (md->queue.front == 0 && md->queue.rear == TOTAL_BLOCKS - 1)) {
        return 1;
    }
    return 0;
}

/* Adds a block to the rear of the queue, this helps for wear levelling */
void enQueue(meta_data *md, u_int8_t blk) {

    if (isFull(md)) {
        printf("Cannot free more blocks as all blocks are free\n");
    } else {
        if (md->queue.front == -1) md->queue.front = 0;

        md->queue.rear = (md->queue.rear + 1);                      // two lines work as mod as mod can have
        if (md->queue.rear == TOTAL_BLOCKS) {
            md->queue.rear = 0;     // irregular behavior in c
        }

        md->queue.q[md->queue.rear] = blk;
    }
}

/* Takes a block from the front of the queue */
int8_t deQueue(meta_data *md) {
    int8_t block;

    if (isEmpty(md)) {
        printf("No free blocks available\n");
        return (-1);
    } else {
        block = md->queue.q[md->queue.front];
        if (md->queue.front == md->queue.rear) {
            md->queue.front = -1;
            md->queue.rear = -1;
        } else {
            md->queue.front = (md->queue.front + 1);                    // two lines work as mod as mod can have
            if (md->queue.front >= TOTAL_BLOCKS) md->queue.front = 0;   // irregular behavior in c
        }
        return block;
    }
}

/* Calculates the remaining available blocks based on the circular queue front and rear pointers*/
signed short availableBlocks(meta_data *md) {
    short signed int head = md->queue.front;
    short signed int tail = md->queue.rear;

    if (head==-1 || tail==-1) {
        printf("TRUE");
        return TOTAL_BLOCKS;
    }
    else if ((head-tail)<0) return (tail-head) + 1;
    else return TOTAL_BLOCKS - (head-tail) + 1;
}

/* Displays if the queue is empty or the position of the first and last block currently in the queue */
void display(meta_data *md) {
    int8_t i;
    if (isEmpty(md)) printf("All blocks in use\n");
    else {
        printf("Next available block -> %d\n", md->queue.front);
        printf("Last available block -> %d\n", md->queue.rear);
    }
}