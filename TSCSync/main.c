#include <stdio.h>

int main(int argc, char** argv)
{
    printf("Welcome to the jungle...\n");

    while (0x60 != (0x60 & inp(0x3FD)))
        ;

    for (int c = (
        outp(0x4E, 0x55),
        outp(0x4E, 0x07),
        outp(0x4f, 0x01),
        outp(0x4e, 0x30),
        inp(0x4F)),
        x = outp(0x4E, 0xAA)
        ; c != 0x60
        ; c = inp(0x3FD))
        ;
}