

int loop(int a, int b, int c, int d, int e, int f, int g) {

    for(int i=0; i<10; i++){
        c = a * b; 
        for(int k=0; k<10; k++){
            e = k + 2;
            if ( e == 5 )
                return g*2;
        }
    }

    return a ;
}