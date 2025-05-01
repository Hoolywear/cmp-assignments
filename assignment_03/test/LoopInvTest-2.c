




int foo(int a, int b, int c, int e, int f, int d, int x){

    for ( int i = 0; i<5; i++ ){
        a = b + c;
        if ( i == 3 ){
            e = x + 3;
        }
        else if ( x == a ) {
            return 666;
        }
        d = a + 1 ;
        f = e + 2 ;
    }

    int p = 50;
    return p;

}