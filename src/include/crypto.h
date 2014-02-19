#ifndef CRYPTO_H_INCLUDED
#define CRYPTO_H_INCLUDED
#include <stddef.h>
void genkey();

void printkey();

void xorstr(char *str, size_t len);
#endif /*CRYPTO_H_INCLUDED*/
