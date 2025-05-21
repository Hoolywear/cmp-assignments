int foo(int a, int b, int x, int n) {

  int i = 0;
  if (n > 0) {
    while (i < n) {
      // Loop body
      i += 1;
    };
  }

  // check if the instruction is in the guard block or in a previous BB
  int k = x + 99;
  if (n > 0) {
    do {
      // Loop body
      k += 1;
    } while (k < n);
  }

    return 0;
}