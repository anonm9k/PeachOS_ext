#include "kernel.h"
#include "fat16.h"
#include "string/string.h"
#include "disk/disk.h"
#include "disk/streamer.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "status.h"
#include <stdint.h>

#define PEACHOS_FAT16_SIGNATURE 0x29
#define PEACHOS_FAT16_FAT_ENTRY_SIZE 0x02
#define PEACHOS_FAT16_BAD_SECTOR 0xFF7
#define PEACHOS_FAT16_UNUSED 0x00


typedef unsigned int FAT_ITEM_TYPE;
#define FAT_ITEM_TYPE_DIRECTORY 0
#define FAT_ITEM_TYPE_FILE 1

// Fat directory entry attributes bitmask
#define FAT_FILE_READ_ONLY 0x01
#define FAT_FILE_HIDDEN 0x02
#define FAT_FILE_SYSTEM 0x04
#define FAT_FILE_VOLUME_LABEL 0x08
#define FAT_FILE_SUBDIRECTORY 0x10
#define FAT_FILE_ARCHIVED 0x20
#define FAT_FILE_DEVICE 0x40
#define FAT_FILE_RESERVED 0x80


struct fat_header_extended
{
    uint8_t drive_number;
    uint8_t win_nt_bit;
    uint8_t signature;
    uint32_t volume_id;
    uint8_t volume_id_string[11];
    uint8_t system_id_string[8];
} __attribute__((packed));

struct fat_header
{
    uint8_t short_jmp_ins[3];
    uint8_t oem_identifier[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_copies;
    uint16_t root_dir_entries;
    uint16_t number_of_sectors;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_setors;
    uint32_t sectors_big;
} __attribute__((packed));

struct fat_h
{
    struct fat_header primary_header;
    union fat_h_e
    {
        struct fat_header_extended extended_header;
    } shared;
};

struct fat_directory_item
{
    uint8_t filename[8];
    uint8_t ext[3];
    uint8_t attribute;
    uint8_t reserved;
    uint8_t creation_time_tenths_of_a_sec;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access;
    uint16_t high_16_bits_first_cluster;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t low_16_bits_first_cluster;
    uint32_t filesize;
} __attribute__((packed));

struct fat_directory
{
    struct fat_directory_item* item;
    int total;
    int sector_pos;
    int ending_sector_pos;
};

struct fat_item
{
    union 
    {
        struct fat_directory_item* item;
        struct fat_directory* directory;
    };
    
    FAT_ITEM_TYPE type;
};

struct fat_file_descriptor
{
    struct fat_item* item;
    uint32_t pos;
};

struct fat_private
{
    struct fat_h header;
    struct fat_directory root_directory;

    // Used to stream data clusters
    struct disk_stream* cluster_read_stream;
    // Used to stream the file allocation table
    struct disk_stream* fat_read_stream;


    // Used in situations where we stream the directory
    struct disk_stream* directory_stream;
};

int fat16_resolve(struct disk* disk);
void* fat16_open(struct disk* disk, struct path_part* path, FILE_MODE mode);
int fat16_read(struct disk* disk, void* descriptor, uint32_t size, uint32_t nmemb, char* out_ptr);
int fat16_seek(void* private, uint32_t offset, FILE_SEEK_MODE seek_mode);
int fat16_stat(struct disk* disk, void* private, struct file_stat* stat);
int fat16_close(void* private);


struct filesystem fat16_fs =
{
    .resolve = fat16_resolve,
    .open = fat16_open,
    .read = fat16_read,
    .seek = fat16_seek,
    .stat = fat16_stat,
    .close = fat16_close
};

struct filesystem* fat16_init()
{
    strcpy(fat16_fs.name, "FAT16");
    return &fat16_fs;
}

// Note: Partially initializes the fat_private with 3 disk_stream structures
static void fat16_init_private(struct disk* disk, struct fat_private* private)
{
    memset(private, 0, sizeof(struct fat_private));
    private->cluster_read_stream = diskstreamer_new(disk->id);
    private->fat_read_stream = diskstreamer_new(disk->id);
    private->directory_stream = diskstreamer_new(disk->id);
}

// Get size in bytes from sector no.
int fat16_sector_to_absolute(struct disk* disk, int sector)
{
    return sector * disk->sector_size;
}

// Note: Loops through the Data Cluster sector of the FAT16 Disk Layout and goes through each file and counts it.
int fat16_get_total_items_for_directory(struct disk* disk, uint32_t directory_start_sector)
{
    struct fat_directory_item item;
    struct fat_directory_item empty_item;
    memset(&empty_item, 0, sizeof(empty_item));
    
    struct fat_private* fat_private = disk->fs_private;

    int res = 0;
    int i = 0;
    int directory_start_pos = directory_start_sector * disk->sector_size; // Meaning: right after the root directory part in the disk layout
    struct disk_stream* stream = fat_private->directory_stream;
    if(diskstreamer_seek(stream, directory_start_pos) != PEACHOS_ALL_OK)
    {
        res = -EIO;
        goto out;
    }
    /* Here we loop through all the items. diskstreamer_read will read an item, then increment by size of item
    So it can point to the next item. And so on. */
    while(1)
    {
        if (diskstreamer_read(stream, &item, sizeof(item)) != PEACHOS_ALL_OK)
        {
            res = -EIO;
            goto out;
        }

        if (item.filename[0] == 0x00)
        {
            // We are done
            break;
        }

        // Is the item unused
        if (item.filename[0] == 0xE5)
        {
            continue;
        }

        i++; // So we can know the total number if items, and return it.
    }

    res = i;

out:
    return res;
}

// Note: It fills in the root directory: builds the whole table of directories, total, pos, ending pos
int fat16_get_root_directory(struct disk* disk, struct fat_private* fat_private, struct fat_directory* directory)
{
    int res = 0;
    struct fat_header* primary_header = &fat_private->header.primary_header;
    int root_dir_sector_pos = (primary_header->fat_copies * primary_header->sectors_per_fat) + primary_header->reserved_sectors;
    int root_dir_entries = fat_private->header.primary_header.root_dir_entries;
    int root_dir_size = (root_dir_entries * sizeof(struct fat_directory_item));
    int total_sectors = root_dir_size / disk->sector_size;
    if (root_dir_size % disk->sector_size)
    {
        total_sectors += 1;
    }

    int total_items = fat16_get_total_items_for_directory(disk, root_dir_sector_pos);

    struct fat_directory_item* dir = kzalloc(root_dir_size);
    // Check: for memory
    if (!dir)
    {
        res = -ENOMEM;
        goto out;
    }
    
    struct disk_stream* stream = fat_private->directory_stream;
    if (diskstreamer_seek(stream, fat16_sector_to_absolute(disk, root_dir_sector_pos)) != PEACHOS_ALL_OK)
    {
        res = -EIO;
        goto out;
    }

    if (diskstreamer_read(stream, dir, root_dir_size) != PEACHOS_ALL_OK)
    {
        res = -EIO;
        goto out;
    }

    directory->item = dir;
    directory->total = total_items;
    directory->sector_pos = root_dir_sector_pos;
    directory->ending_sector_pos = root_dir_sector_pos + (root_dir_size / disk->sector_size);
out:
    return res;
}

// Note: This function basically creates and initializes the fs_private structure of the disk
int fat16_resolve(struct disk* disk)
{
    int res = 0;
    struct fat_private* fat_private = kzalloc(sizeof(struct fat_private));
    // (3) Here: we get stream cluster_read_stream
    // (4) Here: we get stream fat_read_stream
    // (5) Here: we get directory_stream
    fat16_init_private(disk, fat_private); 
    disk->fs_private = fat_private;
    disk->filesystem = &fat16_fs;
    
    struct disk_stream* stream = diskstreamer_new(disk->id);
    if(!stream)
    {
        res = -ENOMEM;
        goto out;
    }
    // (1) Here: we get the header from boot sector (sector 0) and read only the header (sizeof(fat_private->header))
    // Remember: The header is coming from the disk!!
    // Check: if disk can be read
    if (diskstreamer_read(stream, &fat_private->header, sizeof(fat_private->header)) != PEACHOS_ALL_OK)
    {
        res = -EIO;
        goto out;
    }
    // Check: if it is fat16
    if (fat_private->header.shared.extended_header.signature != 0x29) 
    {
        res = -EFSNOTUS;
        goto out;
    }
    // (2) Here: we get the root directory
    if (fat16_get_root_directory(disk, fat_private, &fat_private->root_directory) != PEACHOS_ALL_OK)
    {
        res = -EIO;
        goto out;
    }

out:
    if (stream)
    {
        diskstreamer_close(stream);
    }

    if (res < 0)
    {
        kfree(fat_private);
        disk->fs_private = 0;
    }
    return res;
}

// Note: will check the file name for spaces. If it gets a space than it'll just take the first part of the name
void fat16_to_proper_string(char** out, const char* in) {
    while (*in != 0x00 && *in != 0x20) { // until we hit a space or null terminator
        **out = *in;
        *out += 1;
        in += 1;
    }
    if (*in == 0x20) {
        **out = 0x00;
    }
}

void fat16_get_full_relative_filename(struct fat_directory_item* item, char* out, int max_len) {
    memset(out, 0x00, max_len);
    char* out_tmp = out;
    // Here: we get the filename
    fat16_to_proper_string(&out_tmp, (const char*) item->filename);
    // Check: if there is extension
    if (item->ext[0] != 0x00 && item->ext[0] != 0x20) {
        // add a dot in between the name and extension, then increment the output pointer
        *out_tmp++ = '.';
        // add the extension
        fat16_to_proper_string(&out_tmp, (const char*) item->ext);
    }
}

// Note: will create a new memory location and copy the fat_directory_item structure there
struct fat_directory_item* fat16_clone_directory_item(struct fat_directory_item* item, int size) {
    struct fat_directory_item* item_copy = 0;
    // Check: if the provided (size) is valid
    if (size < sizeof(struct fat_directory_item)) {
        return 0;
    }
    item_copy = kzalloc(size);
    // Check: if there is enough memory
    if (!item_copy) {
        return 0;
    }
    // Here: we do the cloning
    memcpy(item_copy, item, size);

    return item_copy;
}

// Note: from the fat_directory_item subdirectory, it'll calculate the cluster number
// Note: clusters starts from address 2, then 3, 4, 5 etc
// Note: here we get a address (cluster number) of fat_directory from a fat_directory_item
static uint32_t fat16_get_first_cluster(struct fat_directory_item* item) {
    return (item->high_16_bits_first_cluster) | item->low_16_bits_first_cluster;
}

// Note: converts a cluster number to sector number
static int fat16_cluster_to_sector(struct fat_private* private, int cluster) {
    /* So basically we go to the address where root directory ends in the disk.  Now we calculate the sector number by
    multiplying number of sectors per cluster with the cluster number (but we reduce 2, because clusters starts from
    address 2 and not 0). Then we add the sector number with the ending address of root directory*/
    return private->root_directory.ending_sector_pos + ((cluster - 2) * private->header.primary_header.sectors_per_cluster);
}

// Note: goes to the privates header and get the number of reserved sectors
// Note: beacuse we know the first FAT table is stored after the reserved sector
static uint32_t fat16_get_first_fat_sector(struct fat_private* private) {
    return private->header.primary_header.reserved_sectors;
}

static int fat16_get_fat_entry(struct disk* disk, int cluster) {
    int res = -1;
    struct fat_private* private = disk->fs_private;
    struct disk_stream* stream = private->fat_read_stream;
    if (!stream) {
        goto out;
    }
    uint32_t fat_table_position = fat16_get_first_fat_sector(private) * disk->sector_size;
    // Note: so if it is cluster 2, in the FAT table we go to 2*0x02
    res = diskstreamer_seek(stream, fat_table_position * (cluster * PEACHOS_FAT16_FAT_ENTRY_SIZE));
    if (res < 0) {
        goto out;
    }
    
    uint16_t result = 0;
    res = diskstreamer_read(stream, &result, sizeof(result));
    if (res < 0) {
        goto out;
    }

    res = result;

    out:
        return res;
}

// Note: get correct cluster to use based on the starting cluster and offset
// Note: reads from FAT table and gets the actual cluster number for the file
static int fat16_get_cluster_for_offset(struct disk* disk, int starting_cluster, int offset) {
    int res = 0;
    struct fat_private* private = disk->fs_private;
    int size_of_cluster_bytes = private->header.primary_header.sectors_per_cluster * disk->sector_size;
    int cluster_to_use = starting_cluster;
    int clusters_ahead = offset / size_of_cluster_bytes;
    for (int i = 0; i < clusters_ahead; i++) {
        /* Here: each FAT table entry is of size {PEACHOS_FAT16_FAT_ENTRY_SIZE = 0x02} and holds the sector number
        of where the file is. So the file can occupy more than one sector. It will either have a sector address or 
        addresses linked together, or it will have some error value or end of file value indicating there is no
        more sectors for this file*/
        int entry = fat16_get_fat_entry(disk, cluster_to_use);

        // Check: is this the last entry of the file
        if (entry == 0xFF8 || entry == 0xFFF) {
            res = -EIO;
            goto out;
        }

        // Check: is the sector corrupt
        if (entry == PEACHOS_FAT16_BAD_SECTOR) {
            res = -EIO;
            goto out;
        }

        // Check: is it a reserved sector
        if (entry == 0xFF0 || entry == 0xFF6) {
            res = -EIO;
            goto out;
        }

        // Check: FAT table corrupted?
        // Note: cluster number cannot be 0 because it starts from 2
        if (entry == 0x00) {
            res = -EIO;
            goto out;
        }

        cluster_to_use = entry;
    }

    res = cluster_to_use;
    out: 
        return res;
}

/* Note: this is responsible for reading a cluster given the stream, offet in the cluster, to the out buffer*/
static int fat16_read_internal_from_stream(struct disk* disk, struct disk_stream* stream, int cluster, int offset, int total, void* out) {
    int res = 0;
    struct fat_private* private = disk->fs_private;
    int size_of_cluster_bytes = private->header.primary_header.sectors_per_cluster * disk->sector_size;
    int cluster_to_use = fat16_get_cluster_for_offset(disk, cluster, offset);
    if (cluster_to_use < 0) {
        res = cluster_to_use;
        goto out;
    }

    int offset_from_cluster = offset % size_of_cluster_bytes;

    int starting_sector = fat16_cluster_to_sector(private, cluster_to_use);

    // Here: we get the absolute position in bytes. from sector number
    int starting_pos = (starting_sector*disk->sector_size) + offset_from_cluster;

    // Here: basically we want to read every file in the subdirectory/folder
    /* Note: we can only read a single sector at a time. If more than a sector, we have to go back to the FAT table
    and get the address of the next sector as well */ 
    int total_to_read = total > size_of_cluster_bytes ? size_of_cluster_bytes : total;

    // Here: we point to that location and read
    res = diskstreamer_seek(stream, starting_pos);
    if (res != PEACHOS_ALL_OK) {
        goto out;
    }
    res = diskstreamer_read(stream, out, total_to_read);
    if (res != PEACHOS_ALL_OK) {
        goto out;
    }

    // we adjust the total value, maybe there is more to read...
    total -= total_to_read; 
    // Check: is there more clusters to read?
    if (total > 0) {
        res = fat16_read_internal_from_stream(disk, stream, cluster, offset+total_to_read, total, out+total_to_read);
    }
    out:
        return res;
}

/* Note: this is responsible for reading a cluster given the offet in the cluster, to the out buffer*/
static int fat16_read_internal(struct disk* disk, int starting_cluster, int offset, int total, void* out) {
    struct fat_private* fs_private = disk->fs_private;
    struct disk_stream* stream = fs_private->cluster_read_stream;
    return fat16_read_internal_from_stream(disk, stream, starting_cluster, offset, total, out);
}

// Note: just frees up the space in the heap. Because we create the directory using kzalloc
void fat16_free_directory(struct fat_directory* directory) {
    if (!directory) {
        return;
    }

    if (directory->item) {
        kfree(directory->item);
    }

    kfree(directory);
}

// Note: Note: just frees up the space in the heap. Because we create the files using kzalloc
void fat16_fat_item_free(struct fat_item* item) {
    if (item->type == FAT_ITEM_TYPE_DIRECTORY) {
        fat16_free_directory(item->directory);
    }

    else if (item->type == FAT_ITEM_TYPE_FILE) {
        kfree(item->item);
    }

    kfree(item);
}

/* Note: This function is supposed to take a fat_directory_item that is really a directory. And then go to 
where this file/directory is pointing to to get to the real directory, and giving us that directory as a 
structure fat_directory */
// Note: we are coverting fat_directory_item to fat_directory!!!
struct fat_directory* fat16_load_fat_directory(struct disk* disk, struct fat_directory_item* item) {

    int res = 0;
    struct fat_directory* directory = 0; // This is what we are aiming for
    struct fat_private* fat_private = disk->fs_private; // We need the infos

    // Check: if the fat_directory_item is a sub-directory (again?)
    if (!(item->attribute & FAT_FILE_SUBDIRECTORY)) {
        res = -EINVARG;
        goto out;
    }

    // Here: if it is a sub-directory
    directory = kzalloc(sizeof(struct fat_directory));
    // Check: if enough memory
    if (!directory) {
        res = -ENOMEM;
        goto out;
    }

    // Remember: cluster address starts from 2.
    // Remember: here the item is of directory type
    int cluster = fat16_get_first_cluster(item); // we get cluster numebr of fat_directory here from fat_directory_item
    int cluster_sector = fat16_cluster_to_sector(fat_private, cluster); // Convert cluster no to sector number
    int total_items = fat16_get_total_items_for_directory(disk, cluster_sector);
    directory->total = total_items;
    int directory_size = directory->total * sizeof(struct fat_directory_item);
    directory->item = kzalloc(directory_size);
    if (!directory->item)
    {
        res = -ENOMEM;
        goto out;
    }

    /* Here: we read the whole fat_directory_item of the fat_directory, which is basically a list of all the items 
    in that directory. So bunch of entries of structure fat_directory_item.*/
    res = fat16_read_internal(disk, cluster, 0x00, directory_size, directory->item);
    if (res != PEACHOS_ALL_OK)
    {
        goto out;
    }

    out:
        if (res != PEACHOS_ALL_OK) {
            fat16_free_directory(directory);
        } 
        return directory; 
}

// Note: Constructs a fat_item for fat_file_descriptor
// Note: (item) is coming directly from fat_directory table
struct fat_item* fat16_new_fat_item_for_directory_item(struct disk* disk, struct fat_directory_item* item) {
    struct fat_item* f_item = kzalloc(sizeof(struct fat_item));
    // Check: if there is memory to create this fat_item
    if (!f_item) {
        return 0;
    }
    // Check: if it is a subdirectory
    if (item->attribute & FAT_FILE_SUBDIRECTORY) {
        // Note: we put the directory in fat_item structures directory field and set the type to directory
        f_item->directory = fat16_load_fat_directory(disk, item);
        f_item->type = FAT_ITEM_TYPE_DIRECTORY;
    }

    // Here: if it is a file
    f_item->type = FAT_ITEM_TYPE_FILE;
    /* Note: here we clone the directory from the fat_directory_item (which is a file) that we got from the fat_directory table
    (it contains all the fat_directory_items structures). And then we make a clone of this structure and create a new location
    in the memory and put it there. We work with the cloned version of this instead of the real one in the fat_directory table for 
    safety reasons */
    f_item->item = fat16_clone_directory_item(item, sizeof(struct fat_directory_item));

    return f_item;
}

// Note: search in the item directory table and return the fat_item
// Note: we send the item to {fat16_new_fat_item_for_directory_item} function
struct fat_item* fat16_find_item_in_directory(struct disk* disk, struct fat_directory* directory, const char* name) {
    struct fat_item* f_item = 0;
    char tmp_filename[PEACHOS_MAX_PATH];
    // Check: if the file exists
    for (int i = 0; i < directory->total; i++) {
        fat16_get_full_relative_filename(&directory->item[i], tmp_filename, sizeof(tmp_filename));
        if (istrncmp(tmp_filename, name, sizeof(tmp_filename)) == 0) {
            // Here: we create a fat item
            // Note: we take it directly from the directory table
            f_item = fat16_new_fat_item_for_directory_item(disk, &directory->item[i]);
        }
    }

    return f_item;
}

struct fat_item* fat16_get_directory_entry(struct disk* disk, struct path_part* path) {
    struct fat_private* fat_private = disk->fs_private;
    struct fat_item* current_item = 0;
    struct fat_item* root_item = fat16_find_item_in_directory(disk, &fat_private->root_directory, path->part);

    // Check: if root_item exists
    if (!root_item) {
        goto out;
    }

    struct path_part* next_part = path->next;
    current_item = root_item;
    
    // Check: if there is a next part to the path provided
    while(next_part != 0) {
        // Check: if there is a next part, then our current item must be a directory right?
        if (current_item->type != FAT_ITEM_TYPE_DIRECTORY) {
            print("Cannot access file. Not a directory");
            current_item = 0;
            break;
        }

        struct fat_item* tmp_item = fat16_find_item_in_directory(disk, current_item->directory, next_part->part);
        fat16_fat_item_free(current_item);
        current_item = tmp_item;
        next_part = next_part->next;
    }

    out:
        return current_item;

}

// Note: creates a fat_file_descriptor 
void* fat16_open(struct disk* disk, struct path_part* path, FILE_MODE mode)
{
    // Check: mode should only be READ for now
    if (mode != FILE_MODE_READ) {
        return ERROR(-ERDONLY);
    }

    // Check: enough memory to create a fat file descriptor
    struct fat_file_descriptor* descriptor = 0;
    descriptor = kzalloc(sizeof(struct fat_file_descriptor));
    if (!descriptor) {
        return ERROR(-ENOMEM);
    }

    // Note: Get the item from the disk by path
    descriptor->item = fat16_get_directory_entry(disk, path);
    if (!descriptor->item) {
        print("\nFile doesnt exist");
        return ERROR(-EIO);
    }

    descriptor->pos = 0; // the file will start to be read from beginning thus position is 0

    return descriptor;
}

// Note: closes a file given the fat_file_descriptor
static void fat16_free_file_descriptor(struct fat_file_descriptor* desc)
{
    fat16_fat_item_free(desc->item);
    kfree(desc);
}

// Note: closes a file given the fat_file_descriptor, by calling {fat16_free_file_descriptor}
int fat16_close(void* private) {
    fat16_free_file_descriptor((struct fat_file_descriptor*) private);
    return 0;
}

// Note: give us information about a file (size)
int fat16_stat(struct disk* disk, void* private, struct file_stat* stat) {
    int res = 0;
    struct fat_file_descriptor* descriptor = (struct fat_file_descriptor*) private;
    struct fat_item* desc_item = descriptor->item;
    if (desc_item->type != FAT_ITEM_TYPE_FILE)
    {
        res = -EINVARG;
        goto out;
    }

    struct fat_directory_item* ritem = desc_item->item;
    stat->filesize = ritem->filesize;
    stat->flags = 0x00;

    if (ritem->attribute & FAT_FILE_READ_ONLY)
    {
        stat->flags |= FILE_STAT_READ_ONLY;
    }
    out:
        return res;
}

// Note: reads the file contents into the out_ptr
int fat16_read(struct disk* disk, void* descriptor, uint32_t size, uint32_t nmemb, char* out_ptr) {
    int res = 0;
    struct fat_file_descriptor* fat_desc = descriptor;
    struct fat_directory_item* item = fat_desc->item->item;
    int offset = fat_desc->pos;

    for (uint32_t i = 0; i < nmemb; i++) {
        res = fat16_read_internal(disk, fat16_get_first_cluster(item), offset, size, out_ptr);
        if (ISERR(res)) {
            goto out;
        }
        out_ptr += size;
        offset += size;
    }

    res = nmemb; // Because response should be the total amount read
    out:
        return res;
}

// Note: decide where to put the pointer, to read or write to the file 
int fat16_seek(void* private, uint32_t offset, FILE_SEEK_MODE seek_mode) {
    int res = 0;
    struct fat_file_descriptor* desc = private;
    struct fat_item* desc_item = desc->item;

    // Check: if it is a file or directory
    if (desc_item->type != FAT_ITEM_TYPE_FILE) {
        res = -EINVARG;
        goto out;
    }

    struct fat_directory_item* ritem = desc_item->item;
    // Check: if offset is valid
    if (offset >= ritem->filesize) {
        res = -EIO;
        goto out;
    }

    switch(seek_mode) {
        case SEEK_SET:
            desc->pos = offset;
            break;
        case SEEK_END:
            res = -EUNIMP;
            break;
        case SEEK_CUR:
            desc->pos += offset;
            break;
        default:
            res = -EINVARG;
            break;
    }
    out:
        return res;
}