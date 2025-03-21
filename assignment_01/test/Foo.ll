define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %1, 0
  %4 = mul nsw i32 %3, 1
  %5 = shl i32 %0, 1
  %6 = sdiv i32 %5, 1
  %7 = mul nsw i32 %4, %6
  %8 = sub nsw i32 %7, 0
  %9 = add nsw i32 %8, %1
  ret i32 %7
}
