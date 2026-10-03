#include <stdio.h>
#include <stdlib.h>

struct Node {
    int data; 
    struct Node *next;
};

int main ()
{
    struct Node *head = (struct Node*)malloc(sizeof(struct Node));
    struct Node *second = (struct Node*)malloc(sizeof(struct Node));
    struct Node *third = (struct Node*)malloc(sizeof(struct Node));

    if (!head | !second | !third)
    {
        printf ("Memory allocation failed\n");
        return 1;
    }

    head->data = 1;
    head->next = second;

    second->data = 10;
    second->next = third;

    third->data = 100;
    third->next = NULL;

    struct Node * temp = head;

    while(!temp == NULL)
    {
        printf("%d->", temp->data);
        temp = temp->next;
    }

    printf ("NULL \n");

    temp = head;
    while(!temp == NULL)
    {
        struct Node * next = temp->next;
        free(temp);
        temp = next;
    }

    return 0;   

}

