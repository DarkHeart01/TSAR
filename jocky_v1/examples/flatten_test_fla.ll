; ModuleID = '.\jocky_v1\examples\flatten_test.ll'
source_filename = "jocky_module"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.51.36256"

@.jocky.src = private unnamed_addr constant [10 x i8] c"jocky.jky\00", section "llvm.metadata"
@.jocky.attr.0 = private unnamed_addr constant [8 x i8] c"flatten\00", section "llvm.metadata"
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @classify, ptr @.jocky.attr.0, ptr @.jocky.src, i32 0, ptr null }], section "llvm.metadata"

declare void @jocky_write(ptr noundef, ptr noundef, i32 noundef)

declare void @jocky_exit(i32 noundef)

declare ptr @jocky_alloc(i32 noundef)

declare ptr @jocky_get_stdout()

declare ptr @jocky_create_thread(ptr noundef, ptr noundef)

declare i32 @jocky_wait(ptr noundef, i32 noundef)

; Function Attrs: noinline nounwind optnone uwtable
define i32 @classify(i32 noundef %n) #0 {
entry:
  %i = alloca i32, align 4
  %result = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  store i32 0, ptr %result, align 4
  %n.value = load i32, ptr %n1, align 4
  %cmp = icmp slt i32 %n.value, 0
  %__hyper_state = alloca i32, align 4
  %__rolling_key = alloca i32, align 4
  store i32 7673, ptr %__rolling_key, align 4
  store i32 7581, ptr %__hyper_state, align 4
  br label %__micro_disp_0

while.cond:                                       ; preds = %__micro_disp_2
  %i.value = load i32, ptr %i, align 4
  %n.value7 = load i32, ptr %n1, align 4
  %cmp8 = icmp slt i32 %i.value, %n.value7
  %0 = select i1 %cmp8, i32 107, i32 108
  %1 = xor i32 %0, 1504884713
  store i32 1504884713, ptr %__rolling_key, align 4
  store i32 %1, ptr %__hyper_state, align 4
  br label %__micro_disp_0

while.body:                                       ; preds = %__micro_disp_2
  %result.value = load i32, ptr %result, align 4
  %add = add i32 %result.value, 1
  store i32 %add, ptr %result, align 4
  %i.value9 = load i32, ptr %i, align 4
  %add10 = add i32 %i.value9, 1
  store i32 %add10, ptr %i, align 4
  store i32 1504881257, ptr %__rolling_key, align 4
  store i32 1504881155, ptr %__hyper_state, align 4
  br label %__micro_disp_0

while.end:                                        ; preds = %__micro_disp_2
  %result.value11 = load i32, ptr %result, align 4
  ret i32 %result.value11

__micro_disp_0:                                   ; preds = %while.body, %while.cond, %__micro_disp_2, %entry
  %2 = load i32, ptr %__rolling_key, align 4
  %3 = load i32, ptr %__hyper_state, align 4
  %4 = xor i32 %3, %2
  switch i32 %4, label %__micro_disp_1 [
    i32 100, label %if.then
    i32 101, label %if.end
    i32 102, label %if.else
  ]

if.then:                                          ; preds = %__micro_disp_0
  store i32 0, ptr %result, align 4
  store i32 1504882217, ptr %__rolling_key, align 4
  store i32 1504882252, ptr %__hyper_state, align 4
  br label %__micro_disp_1

if.end:                                           ; preds = %__micro_disp_0
  store i32 0, ptr %i, align 4
  store i32 1504884521, ptr %__rolling_key, align 4
  store i32 1504884547, ptr %__hyper_state, align 4
  br label %__micro_disp_1

if.else:                                          ; preds = %__micro_disp_0
  %n.value2 = load i32, ptr %n1, align 4
  %cmp3 = icmp slt i32 %n.value2, 10
  %5 = select i1 %cmp3, i32 103, i32 105
  %6 = xor i32 %5, 1504882313
  store i32 1504882313, ptr %__rolling_key, align 4
  store i32 %6, ptr %__hyper_state, align 4
  br label %__micro_disp_1

__micro_disp_1:                                   ; preds = %if.else, %if.end, %if.then, %__micro_disp_0
  %7 = load i32, ptr %__rolling_key, align 4
  %8 = load i32, ptr %__hyper_state, align 4
  %9 = xor i32 %8, %7
  switch i32 %9, label %__micro_disp_2 [
    i32 103, label %if.then4
    i32 104, label %if.end5
    i32 105, label %if.else6
  ]

if.then4:                                         ; preds = %__micro_disp_1
  store i32 1, ptr %result, align 4
  store i32 1504881641, ptr %__rolling_key, align 4
  store i32 1504881537, ptr %__hyper_state, align 4
  br label %__micro_disp_2

if.end5:                                          ; preds = %__micro_disp_1
  store i32 1504884617, ptr %__rolling_key, align 4
  store i32 1504884716, ptr %__hyper_state, align 4
  br label %__micro_disp_2

if.else6:                                         ; preds = %__micro_disp_1
  store i32 2, ptr %result, align 4
  store i32 1504881161, ptr %__rolling_key, align 4
  store i32 1504881249, ptr %__hyper_state, align 4
  br label %__micro_disp_2

__micro_disp_2:                                   ; preds = %if.else6, %if.end5, %if.then4, %__micro_disp_1
  %10 = load i32, ptr %__rolling_key, align 4
  %11 = load i32, ptr %__hyper_state, align 4
  %12 = xor i32 %11, %10
  switch i32 %12, label %__micro_disp_0 [
    i32 106, label %while.cond
    i32 107, label %while.body
    i32 108, label %while.end
  ]
}

; Function Attrs: noinline nounwind optnone uwtable
define void @jocky_entry() #0 {
entry:
  %val = alloca i32, align 4
  %call = call i32 @classify(i32 5)
  store i32 %call, ptr %val, align 4
  %val.value = load i32, ptr %val, align 4
  call void @jocky_exit(i32 %val.value)
  ret void
}

define private void @__rt_init(i8 %0, i32 %1, ptr %2, ptr %3, i32 %4) {
entry:
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable }
