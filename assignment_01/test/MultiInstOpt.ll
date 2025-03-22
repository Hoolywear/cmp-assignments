; int foo(int e, int a) {
;   int b = a + 1;
;   int c = b * 2;
;   b = e << 1;
;   int d = b / 4;
;   return c * d;
; }

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %1, 1
  %4 = sub nsw i32 %3, 1
  %5 = mul i32 %4, 44
  %6 = sdiv i32 %4, 8
  %7 = sub nsw i32 %3, 1
  ret i32 %7
}