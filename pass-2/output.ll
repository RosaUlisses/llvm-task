; ModuleID = 'input.ll'
source_filename = "input.ll"

define i32 @example(i32 %x, i32 %y) {
entry:
  br label %b2

b2:                                               ; preds = %entry
  ret i32 0
}
