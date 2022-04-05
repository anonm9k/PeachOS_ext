#include "file.h"
#include "config.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "fat/fat16.h"
#include "status.h"
#include "kernel.h"
#include "disk/disk.h"
#include "string/string.h"

struct filesystem* filesystems[PEACHOS_MAX_FILESYSTEMS];

/* this is basically the file descriptor table. It contains bunch of addresses(pointers) that points to all the
different file descriptors available. And there are 512 addresses in this table */
struct file_descriptor* file_descriptors[PEACHOS_MAX_FILE_DESCRIPTORS];

static struct filesystem** fs_get_free_filesystem()
{
    int i = 0;
    for (i = 0; i < PEACHOS_MAX_FILESYSTEMS; i++)
    {
        if (filesystems[i] == 0)
        {
            return &filesystems[i];
        }
    }

    return 0;
}

void fs_insert_filesystem(struct filesystem* filesystem)
{
    struct filesystem** fs;
    fs = fs_get_free_filesystem();
    if (!fs)
    {
        print("Problem inserting filesystem"); 
        while(1) {}
    }

    *fs = filesystem;
}

// this is where we insert our own (static) filesystems
static void fs_static_load()
{
    fs_insert_filesystem(fat16_init());
}

void fs_load()
{
    memset(filesystems, 0, sizeof(filesystems));
    fs_static_load();
}

void fs_init()
{
    memset(file_descriptors, 0, sizeof(file_descriptors));
    fs_load();
}

// Note: fress the descriptor from the table and sets that slot to 0 to indicate that its an empty slot
static void file_free_descriptor(struct file_descriptor* desc)
{
    file_descriptors[desc->index-1] = 0x00;
    kfree(desc);
};

// Note: this will find a slot in the descriptor table and return to us the index
static int file_new_descriptor(struct file_descriptor** desc_out)
{
    int res = -ENOMEM;
    for (int i = 0; i < PEACHOS_MAX_FILE_DESCRIPTORS; i++)
    {
        // go through the file descriptor table and look for free slots. where the address is 0 basically
        if (file_descriptors[i] == 0)  
        {
            struct file_descriptor* desc = kzalloc(sizeof(struct file_descriptor));
            // Descriptors start at 1. So we adjust it by adding one to the result
            desc->index = i + 1;
            file_descriptors[i] = desc; // we save our created descriptor in the FDT
            *desc_out = desc; // and then we send the descriptor to the callers desc_out space
            res = 0;
            break;
        }
    }

    return res;
}


// Note: this will get us the whole descriptor, given the index in the file descriptor table
static struct file_descriptor* file_get_descriptor(int fd)
{
    if (fd <= 0 || fd >= PEACHOS_MAX_FILE_DESCRIPTORS)
    {
        return 0;
    }

    // Descriptors start at 1
    int index = fd - 1;
    return file_descriptors[index];
}

// Note: it resolves the filesystem by looking into the disk. And then sets the filesystem field of the disk struct
struct filesystem* fs_resolve(struct disk* disk)
{
    struct filesystem* fs = 0;
    for (int i = 0; i < PEACHOS_MAX_FILESYSTEMS; i++)
    {
        if (filesystems[i] != 0 && filesystems[i]->resolve(disk) == 0)
        {
            fs = filesystems[i];
            break;
        }
    }

    return fs;
}

FILE_MODE file_get_mode_by_string(const char* str) {
    FILE_MODE mode = FILE_MODE_INVALID;
    if (strncmp(str, "r", 1) == 0) {
        mode = FILE_MODE_READ;
    }
    else if (strncmp(str, "w", 1) == 0) {
        mode = FILE_MODE_WRITE;
    }
    else if (strncmp(str, "a", 1) == 0) {
        mode = FILE_MODE_APPEND;
    }
    return mode;
}

// Note: if successful, returns an index in the file_descriptor table, otherwise returns 0
int fopen(const char* filename, const char* mode_str)
{
    int res = 0;

    // Check: if path is in valid format
    struct path_root* root_path = pathparser_parse(filename, NULL);
    if (!root_path) { // root_path didn't return a pointer to heap
        res = -EINVARG;
        goto out;
    }
    if (!root_path->first) { // doesn't have anything after the drive no.
        res = -EINVARG;
        goto out;
    }

    // Check: if the disk exists
    struct disk* disk = disk_get(root_path->drive_no);
    if (!disk) {
        res = -EIO;
        goto out;
    }
    if (!disk->filesystem) {
        res = -EIO;
        goto out;
    }

    // Check: if mode is valid
    FILE_MODE mode = file_get_mode_by_string(mode_str);
    if (mode == FILE_MODE_INVALID) {
        res = -EINVARG;
        goto out;
    }

    /* Note: we call the filesystems open function, and in return get a pointer to a descriptor (which exists somewhere
    on the heap, e.g. in fat16_open function we create a descriptor (=kzalloc(sizeof(struct fat_file_descriptor))) */
    void* descriptor_private_data = disk->filesystem->open(disk, root_path->first, mode);
    if (ISERR(descriptor_private_data)) {
        res = ERROR_I(descriptor_private_data);
        goto out;
    }
    // Here: we create the descriptor
    struct file_descriptor* desc = 0;
    // Here: we give it the descriptor and get an index in the table
    res = file_new_descriptor(&desc);
    if (res < 0) {
        goto out;
    }
    desc->filesystem = disk->filesystem;
    desc->private = descriptor_private_data;
    desc->disk = disk;
    res = desc->index; // index is set when we called file_new_descriptor() function

    out:
    if(res < 0) {
        res = 0; // fopen shouldn't return negative values
    }
        return res;
}

// Note: gets info on a file given descriptor index
int fstat(int fd, struct file_stat* stat) {
    int res = 0;
    struct file_descriptor* desc = file_get_descriptor(fd);
    // Check: if the descriptor exists in the table
    if (!desc) {
        res = -EIO;
        goto out;
    }

    res = desc->filesystem->stat(desc->disk, desc->private, stat);

    out:
        return res;
}

// Note: take file descriptor index and removes it from descriptor table
int fclose(int fd) {
    int res = 0;
    struct file_descriptor* desc = file_get_descriptor(fd);
    
    // Check: if descriptor exists in the table
    if(!desc) {
        res = -EIO;
        goto out;
    }

    res = desc->filesystem->close(desc->private);
    // Check: if filesystems close is able to remove the descriptor from memory
    // Note: then we remove it from the file descriptor table in memory
    if (res == PEACHOS_ALL_OK) {
        file_free_descriptor(desc);
    }
    out:
        print("\nFile closed");    
        return res;
}

// Note: decide where to put the pointer, to read or write to the file
int fseek(int fd, int offset, FILE_SEEK_MODE whence) {
    int res = 0;
    struct file_descriptor* desc = file_get_descriptor(fd);
    // Check: if we get the descriptor
    if (!desc) {
        res = -EIO;
        goto out;
    }

    res = desc->filesystem->seek(desc->private, offset, whence);

    out:
        return res;
}

// Note: consults the filesystems read function, returns the amount read
int fread(void* ptr, uint32_t size, uint32_t nmemb, int fd) {
    int res = 0;
    // Check: if arguments are valid
    if (size == 0 || nmemb == 0 || fd < 1)
    {
        res = -EINVARG;
        goto out;
    }

    struct file_descriptor* desc = file_get_descriptor(fd);
    // Check: if the index passed is actually in the file descriptor table in memory
    if (!desc)
    {
        res = -EINVARG;
        goto out;
    }
    
    // Here: consults the filesystems read function
    res = desc->filesystem->read(desc->disk, desc->private, size, nmemb, (char*) ptr);
    out:
        return res;
}

