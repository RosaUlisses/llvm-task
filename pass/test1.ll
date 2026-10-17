define i32 @example(i32 %x, i32 %y, ptr %p) {
  %mult = mul i32 %x, %y
  
  %ld = load i32, ptr %p
  %u = add i32 %mult, %ld
  %summ = add i32 %x, %y

  ret i32 %summ
}

