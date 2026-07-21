int helper(int x) { return x * 3; }
int caller(int a, int b) {
int t = helper(a);
return t + b;
}
int main(void) { return caller(4,5); }
