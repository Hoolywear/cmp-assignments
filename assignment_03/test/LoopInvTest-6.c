int foo(int a, int b) {
    int z,x;
    while(1) {
        if (a !=0) {
            x = 5*b;
            if(b != 1) {
                z = x + 2;
                return z;
            }
        } else if (a != 1) {
            return 0;
        }
    }
}