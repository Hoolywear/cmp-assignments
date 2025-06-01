int foo(int a, int b, int f, int n) {

    for (int i = 0; i < 5; ++i) {
        int f = 3 * a;
    }
 
    // int l = x*n; // uncomment for not adjacent loops
    int x = 0;
    do {

      if ( a == b){
        n = f + 999;
      }
      else{
        n = f - 999;
      }

      x++;
    } while( x<=5 );


    for (int i = 0; i < 5; ++i) {
      int f = 3 * a;
    }


    return 0;
}