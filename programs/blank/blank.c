#include "peachos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char** argv) {
    char* ptr = malloc(15);
    strncpy(ptr, "KHITA BA", 15);
    print(ptr);
    
    while (1) {}
    return 0;
}