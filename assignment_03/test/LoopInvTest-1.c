


int foo(int a, int b, int c, int e, int d, int f, int k, int j) {
    while ( a == 10 ) {
        d = f + 1;  // si
        k = b * c;  // si
        if (e == 3) {
            a = e + d;  // NO
        } else if (f == 4) {
            e = f + 3;  // si
            return e;
        }
    }
    a = b + d;
    return f;
}