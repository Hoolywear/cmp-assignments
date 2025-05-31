int foo(int a, int b, int f, int n) {

    for (int i = 0; i < 5; ++i) {
        int f = 3 * a;
    }
 
    // int l = x*n; // uncomment for not adjacent loops
    int x = 0;
    while ( x < 5 ) {

      if ( a == b){
        n = f + 999;
      }
      else{
        n = f - 999;
      }

      x = x+1;
    }


    return 0;
}