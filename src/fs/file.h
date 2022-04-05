#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include "pparser.h"

typedef unsigned int FILE_SEEK_MODE;
enum
{
    SEEK_SET,
    SEEK_CUR,
    SEEK_END
};


typedef unsigned int FILE_MODE;
enum
{   // here we are creating binary modes
    FILE_MODE_READ,
    FILE_MODE_WRITE,
    FILE_MODE_APPEND,
    FILE_MODE_INVALID
};

enum {
    FILE_STAT_READ_ONLY = 0b00000001
};
typedef unsigned int FILE_STAT_FLAGS;

struct file_stat {
    FILE_STAT_FLAGS flags;
    uint32_t filesize;
};


struct disk;
typedef void*(*FS_OPEN_FUNCTION)(struct disk* disk, struct path_part* path, FILE_MODE mode_str);
typedef int (*FS_READ_FUNCTION)(struct disk* disk, void* private, uint32_t size, uint32_t nmemb, char* out);
typedef int (*FS_RESOLVE_FUNCTION)(struct disk* disk);
typedef int (*FS_CLOSE_FUNCTION) (void* private);
typedef int (*FS_SEEK_FUNCTION)(void* private, uint32_t offset, FILE_SEEK_MODE seek_mode);
typedef int (*FS_STAT_FUNCTION) (struct disk* disk, void* private, struct file_stat* stat);


struct filesystem
{
    // Filesystem should return zero from resolve if the provided disk is using its filesystem
    FS_RESOLVE_FUNCTION resolve;
    FS_OPEN_FUNCTION open;
    FS_READ_FUNCTION read;
    FS_SEEK_FUNCTION seek;
    FS_STAT_FUNCTION stat;
    FS_CLOSE_FUNCTION close;

    char name[20];
};

struct file_descriptor
{
    // The descriptor index
    int index;
    struct filesystem* filesystem;

    // This is basically a pointer to filesystems (e.g. FAT16) file descriptor (e.g. fat_file_descriptor structure)
    void* private;

    // The disk that the file descriptor should be used on
    struct disk* disk;
};

void fs_init();

void fs_insert_filesystem(struct filesystem* filesystem);
struct filesystem* fs_resolve(struct disk* disk);

int fopen(const char* filename, const char* mode);
int fseek(int fd, int offset, FILE_SEEK_MODE whence);
int fread(void* ptr, uint32_t size, uint32_t nmemb, int fd);
int fstat(int fd, struct file_stat* stat);
int fclose(int fd);


#endif