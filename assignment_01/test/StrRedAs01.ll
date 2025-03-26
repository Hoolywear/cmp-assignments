; ModuleID = 'test/StrRedAs01.bc'
source_filename = "test/StrRedTest.ll"

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) {
  %shift1 = shl i32 %0, 4
  %sub2 = sub i32 %shift1, %0
  ret i32 %sub2
}
