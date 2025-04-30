

void foo(){

    int a = 100; 
    int b = 200;
    int c = 300;
    
    for ( int i = 0; i<5; i++ ){
        
        if ( i == 3 ){
            a = a + b;
            b = b + b;
            c = b + a;
        }
        else {
            int e = 3;
        }
        int d = a + 1;
    }
}