define i32 @example(i32 %x, i32 %y) {
entry:
  %m1 = mul i32 %x, %y
  %cond = icmp eq i32 %x, %y
  br i1 %cond, label %equal, label %not_equal

equal:
  %a1 = add i32 %x, %y
  %a2 = add i32 %x, %y
  ret i32 1

not_equal:
  %s1 = sub i32 %x, %y
  ret i32 0
}

