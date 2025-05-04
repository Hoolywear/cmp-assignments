int foo(int x) {
    int i = 0;
    int n = 1000;
    int a = 0;
    do {
        x = a + n;
        i = i + 1;
    } while(i<n);
}