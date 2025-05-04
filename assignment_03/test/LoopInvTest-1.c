

int foo(int a, int b, int c, int e, int d, int f, int k, int j) {

    while ( 1 )  {
        a = 10 + j;
        if (e == 3) {
            d = 5 + b;
        } else if (f == 4) {
            d = j * a;
        }

        if ( j == f )
            break;
        
    }
    b = a + d;
    return f;
}

int main() {
    int a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8;
    int result = foo(a, b, c, d, e, f, g, h);
    return result;
}