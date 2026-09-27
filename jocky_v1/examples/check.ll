; ModuleID = 'C:\Users\Ameya\Documents\GitHub\JOCKY-TSAR\jocky_v1\examples\output.ll'
source_filename = "jocky_module"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.51.36256"

declare void @jocky_write(ptr noundef, ptr noundef, i32 noundef)

declare void @jocky_exit(i32 noundef)

declare ptr @jocky_alloc(i32 noundef)

declare ptr @jocky_get_stdout()

declare ptr @jocky_create_thread(ptr noundef, ptr noundef)

declare i32 @jocky_wait(ptr noundef, i32 noundef)

; Function Attrs: noinline nounwind optnone uwtable
define i32 @add(i32 noundef %a, i32 noundef %b) #0 {
entry:
  %result = alloca i32, align 4
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a.value = load i32, ptr %a1, align 4
  %b.value = load i32, ptr %b2, align 4
  %add = add i32 %a.value, %b.value
  store i32 %add, ptr %result, align 4
  %result.value = load i32, ptr %result, align 4
  ret i32 %result.value
}

; Function Attrs: noinline nounwind optnone uwtable
define void @jocky_entry() #0 {
entry:
  %value = alloca i32, align 4
  %call = call i32 @add(i32 40, i32 20)
  store i32 %call, ptr %value, align 4
  %value.value = load i32, ptr %value, align 4
  call void @jocky_exit(i32 %value.value)
  ret void
}

define private void @__rt_init(i8 %0, i32 %1, ptr %2, ptr %3, i32 %4) {
entry:
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable }
