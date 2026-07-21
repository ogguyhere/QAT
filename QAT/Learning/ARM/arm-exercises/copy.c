// copy.c
void my_copy(unsigned int *dst, unsigned int *src, int n) {
for (int i = 0; i < n; i++) *dst++ = *src++;
}
int main(void) {
unsigned int a[4] = {1,2,3,4}, b[4] = {0};
my_copy(b, a, 4);
return b[3];
}
