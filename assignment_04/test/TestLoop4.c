int foo(int size, int A[]) {

    for (int i = 1; i <= size; i++) {
      A[i-1] = i;
      if ( size == 10 ){
        int b = A[i-1];
        continue;
      }
      else {
        int c = size;
        continue;
      }
    }

    for (int j = 0 ; j < size; j++) {
      int k = size + 99999;
      A[j] = A[j];
    }

    return 0;
}