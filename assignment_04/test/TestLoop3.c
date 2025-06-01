int foo(int a, int b, int x, int n) {
  int k = 0;
  do {

    if ( a == b ) {
      a = a * 1919;
    }
    else {
      a = a * 9191;
    }

    k++;
    b = k * 8888;
  } while( k <= 5 );

  int y = 0;
  while ( y < 5 ) {
    if ( a == b ) {
      a = a * 1919;
    }
    else {
      a = a * 9191;
    }

    y++;
    b = y * 2222;
  } 


  return 0;
}