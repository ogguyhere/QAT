#include <stdio.h>

int main()
{
    FILE * file = fopen("data.bin", "rb");
    if (file== NULL)
    {
        printf("Error opening file. \n");
        return 1;
    }
    unsigned char byte;
    while (fread(&byte, sizeof(byte),1, file))
    {
        printf("%02x \n",byte);
    }
    fclose(file);
    return 0;
}