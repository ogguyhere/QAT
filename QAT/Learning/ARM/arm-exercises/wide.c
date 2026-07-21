// wide.c
long long add64(int a, long long b, int c) { return a + b + c; }
int main(void) { return (int)add64(1, 100000000000LL, 2); }
