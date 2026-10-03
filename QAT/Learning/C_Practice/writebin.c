#include <stdio.h>

int main(){
    FILE *file; 

    unsigned char data [] = {
        0x41, 0x42, 0x43, 0x44,
        0x10, 0x20, 0x30, 0xFF,
        0x00, 0x7A
    };

    file = fopen("data.bin", "wb");
    if (file == NULL)
    {
        printf("Error opening file.\n");
        return 1;
    }

    fwrite(data, sizeof (unsigned char), sizeof(data), file);

    fclose(file);

    return 0; 

}