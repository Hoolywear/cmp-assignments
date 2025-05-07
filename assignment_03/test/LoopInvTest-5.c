void foo(int a, int b, int c) {
    a = b + 3;
    if (a == 5) {
        for (int i = 0; i < 100; i++) {
            a = b + c;
        }
    }

    int x = a + 6;
}