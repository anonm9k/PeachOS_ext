[BITS 32]

section .asm

global print: function
global getkey: function
global peachos_malloc: function
global peachos_free: function

; void print(const char* message)
print:
    push ebp
    mov ebp, esp
    push dword[ebp+8]
    mov eax, 1
    int 80h
    add esp, 4
    pop ebp
    ret

; int getkey();
getkey:
    push ebp
    mov ebp, esp
    mov eax, 2
    int 80h
    pop ebp
    ret

; void* peachos_malloc(size_t size)
peachos_malloc:
    push ebp
    mov ebp, esp
    mov eax, 4
    push dword[ebp+8]
    int 80h
    add esp, 4
    pop ebp
    ret

; void peachos_free(void* ptr)
peachos_free:
    push ebp
    mov ebp, esp
    mov eax, 5
    push dword[ebp+8]
    int 80h
    add esp, 4
    pop ebp
    ret
