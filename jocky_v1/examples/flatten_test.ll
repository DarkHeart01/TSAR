; ModuleID = 'jocky_module'
source_filename = "jocky_module"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.44.35225"

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
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i32 0, ptr %result, align 4
  br label %if.end

if.end:                                           ; preds = %if.end5, %if.then
  store i32 0, ptr %i, align 4
  br label %while.cond

if.else:                                          ; preds = %entry
  %n.value2 = load i32, ptr %n1, align 4
  %cmp3 = icmp slt i32 %n.value2, 10
  br i1 %cmp3, label %if.then4, label %if.else6

if.then4:                                         ; preds = %if.else
  store i32 1, ptr %result, align 4
  br label %if.end5

if.end5:                                          ; preds = %if.else6, %if.then4
  br label %if.end

if.else6:                                         ; preds = %if.else
  store i32 2, ptr %result, align 4
  br label %if.end5

while.cond:                                       ; preds = %while.body, %if.end
  %i.value = load i32, ptr %i, align 4
  %n.value7 = load i32, ptr %n1, align 4
  %cmp8 = icmp slt i32 %i.value, %n.value7
  br i1 %cmp8, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %result.value = load i32, ptr %result, align 4
  %add = add i32 %result.value, 1
  store i32 %add, ptr %result, align 4
  %i.value9 = load i32, ptr %i, align 4
  %add10 = add i32 %i.value9, 1
  store i32 %add10, ptr %i, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %result.value11 = load i32, ptr %result, align 4
  ret i32 %result.value11
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

attributes #0 = { noinline nounwind optnone uwtable }
