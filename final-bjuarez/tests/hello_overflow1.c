#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char *x = malloc(1);
    strcpy(x, "Hello World!");
    printf("%s\n", x);
    free(x);
}

// 11

// stderr: Invalid heap access: address 0x100002000 is not in an allocation or was already freed
// stderr: at /lib/x86_64-linux-gnu/libc.so.6(+0x18ced8)
// stderr: __strcpy_avx2
// stderr: strcpy-avx2.S:612
// stderr: at bin/hello_overflow1(main+0x24)
// stderr: main
// stderr: hello_overflow1.c:8
