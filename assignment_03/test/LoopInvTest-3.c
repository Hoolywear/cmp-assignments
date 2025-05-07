void print() {
    return;
}

void foo(int a, int b, int c, int d, int e, int f) {

    for (int i = 0; i < 100; i++) {
        a = b + c;
        if (a == 15) {
            e = b + 3;
        } else {
            e = 2;
        }
        d = a + 1;
        f = e + 2;
    }
}