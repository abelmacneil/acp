#include "crypto.h"
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define CIPHERLEN   512
#define TIMEGAP     11
#define RAND_CONST  ((uint32_t) (M_PI * 271287128))
uint32_t key[CIPHERLEN];

void genkey()
{
    int i;
    unsigned int t = time(NULL) / TIMEGAP + RAND_CONST;
    srand(t);
    for (i = 0; i < CIPHERLEN; i++) {
        key[i] = rand();
    }
    //printkey(t);
}

void printkey(uint32_t t)
{
    int i;
    printf("KEY: 0x%08x = {", t);
    for (i = 0; i < 4; i++)
        printf("0x%08x ", key[i]);
    puts("...}");
}

void xorstr(char *str, size_t len)
{
    int i;
    for (i = 0; i < len; i++) {
        str[i] ^= key[i % CIPHERLEN];
    }
}
