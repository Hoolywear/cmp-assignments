int foo(int b, int a) {
    int c = b + 0;
    int d = c + 10; // c --> b
    int e = a * 1;
    int f = e + 2;  // e --> a
    return c + e;
}