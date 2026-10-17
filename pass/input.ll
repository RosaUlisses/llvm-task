define i32 @example(i32 %x, i32 %y) {
entry:
  %a = mul i32 %x, %y
  %k = mul i32 100, 100
  %cond = icmp ne i32 %x, 0
  br i1 %cond, label %then, label %else

then:
  %c = add i32 %x, %y
  ret i32 %c

else:
  ret i32 %a
}

