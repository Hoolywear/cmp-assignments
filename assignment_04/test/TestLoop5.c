int foo(int size, int A[]) {
    
    for (int i = 0; i < size; i++) {
      int b = A[i];
    }

    for (int j = 0; j < size; j++) {
      A[j] = j+1;
    }

    return 0;
}
