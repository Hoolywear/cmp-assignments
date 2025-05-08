void foo(int a, int b, int c, int d, int e, int f) {

    for (int i = 0; i < 100; i++) {
        a = b + c; //movable
        if (a == 15) {
            e = b + 3; //linv and not movable
        } else {
            e = 2 - c; //linv and not movable
        }
        d = a + 1; //movable
        f = e + 2; //not linv
    }
}