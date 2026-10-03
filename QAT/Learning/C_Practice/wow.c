#include <stdio.h>
#include <stdlib.h>

struct job
{
    int id;
    int value;
};

int main()
{
    // 1. Allocate object at address X
    struct job *ptr = malloc(sizeof(struct job));
     printf("A: id = %d, value = %d\n", ptr->id, ptr->value);

    ptr->id = 1;
    ptr->value = 100;

    printf("Object A address: %p\n", (void *)ptr);
    printf("A: id = %d, value = %d\n", ptr->id, ptr->value);


    // 2. ptr stores address X
    struct job *old_ptr = ptr;


    // 3. Free object
    free(ptr);

    printf("\nObject A freed\n");


    // 4. old_ptr still contains X
    printf("old_ptr still contains: %p\n", (void *)old_ptr);


    // 5. Allocate another object
    // struct job *new_ptr = malloc(sizeof(struct job));

    // new_ptr->id = 2;
    // new_ptr->value = 999;

    // printf("\nObject B address: %p\n", (void *)new_ptr);


    // 6. Try using the old pointer
    printf("\nOld pointer address: %p\n", (void *)old_ptr);

    printf("Old pointer sees: id = %d, value = %d\n",
           old_ptr->id,
           old_ptr->value);


    // free(new_ptr);

    return 0;
}