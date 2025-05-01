

int loop(int a, int b, int c, int d, int e, int f, int g) {

    for(int i=0; i<10; i++){
        c = a + b;  // no
        for(int j=0; j<10; j++){
            d = c + e;  // no
            a =  j + 2;     // no
        }
        for(int k=0; k<10; k++){
            e = f + 2;      // loop ind
            for(int j=0; j<10; j++){
                g = a + b;      // no
            }
        }
    }
}