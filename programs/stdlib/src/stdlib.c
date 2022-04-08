#include "stdlib.h"
#include "peachos.h"
#include <stddef.h>

void* malloc(size_t size) {
    return peachos_malloc(size);
}

void free(void* ptr){
    peachos_free(ptr);
}