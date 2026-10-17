; ModuleID = 'input.ll'
source_filename = "input.ll"

define i32 @example(i32 %x, i32 %y, ptr %p) {
entry:
  %cond = icmp eq i32 %x, %y
  br i1 %cond, label %equal, label %not_equal

equal:                                            ; preds = %entry
  %a2 = add i32 %x, %y
  store i32 %a2, ptr %p, align 4
  ret i32 1

not_equal:                                        ; preds = %entry
  %res = load i32, ptr %p, align 4
  ret i32 %res
}
