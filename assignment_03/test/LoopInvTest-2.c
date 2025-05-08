int foo(int a, int b, int c, int e, int f, int d, int x){

    while (1){

        a = b + c; //linv and movable
        a = b * 3; //linv and movable

        if ( x == 3 ){
            e = x + 3;  
        }
        else if ( x == 4 ) {
            c = a + 10;
            break;
        }

        d = c + 2 ;

    }

    int p = b + 5;
    return p;

}