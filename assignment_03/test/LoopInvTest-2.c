int foo(int a, int b, int c, int e, int f, int d, int x){

    for ( int i = 0; i<5; i++ ){

        a = b + c;
        a = b * 3;

        if ( i == 3 ){
            e = x + 3;  
        }
        else if ( i == 4 ) {
            c = a + 10;
            break;
        }

        d = a + 2 ;

    }

    int p = b + 5;
    return p;

}