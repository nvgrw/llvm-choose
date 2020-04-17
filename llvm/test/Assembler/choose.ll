; RUN: llvm-as < %s | llvm-dis | llvm-as | llvm-dis
; Check choose statement

define void @test() {
  choose i8 1, label %prob1 [ i8 99, label %prob2 ]
; CHECK: choose i8 1, label %prob1 [ i8 99, label %prob2 ]
prob1:
  br label %end
prob2:
  br label %end
end:
  ret void
}
