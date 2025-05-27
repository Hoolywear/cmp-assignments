int foo(int a, int b, int x, int n) {

  int i = 0;
  int k = 0;
  if (n > 0) {
    do {
      // Loop body
      i += 1;
    } while (i < n);
  }
  
  // check if the instruction is in the guard block or in a previous BB
  if (n > 0) {
    do {
      // Loop body
      k += 1;
    } while (k < n);
  }

    return 0;
}