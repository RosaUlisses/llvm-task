define i32 @example(i32 %x, i32 %y) {
entry:
  br label %b2
b2:
  br label %b3

b3:
  br label %b2
}
