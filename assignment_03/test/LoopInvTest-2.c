

void foo(int a, int b, int c, int e, int f, int d, int x){

    for ( int i = 0; i<5; i++ ){
        
        a = b + c;
        
        if ( i == 3 ){
            e = x + 3;
        }
        else {
            e = 5 ;
        }

        d = a + 1 ;
        f = e + 2 ;
    }
}