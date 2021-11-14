#include <stdio.h>

int main (int argc, char** argv)
{
    printf("Hello, world!\n");

    for (size_t i = 0; i < argc; ++i)
    {
        printf("- %s\n", argv[i]);
    }
    return 0;
}
