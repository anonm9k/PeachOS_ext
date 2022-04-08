#include "peachos.h"
#include "stdlib.h"


int main(int argc, char** argv) {
    print("Hello World! from stdlib\n");

    void* ptr = malloc(512);
    free(ptr);

    while (1)
    {
        
    }
    
    return 0;
}