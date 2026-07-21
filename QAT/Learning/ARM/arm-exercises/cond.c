// cond.c
int abs_diff(int a, int b) {
if (a > b) return a - b;
else return b - a;
}

int main(int argc, char **argv) {
    return abs_diff(argc + 10, argc + 3);
}
