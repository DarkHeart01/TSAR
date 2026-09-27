; ModuleID = 'jocky_module'
source_filename = "jocky.jky"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.44.35225"

@.jocky.src = private unnamed_addr constant [10 x i8] c"jocky.jky\00", section "llvm.metadata", align 1
@.jocky.attr.0 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @classify, ptr @.jocky.attr.0, ptr @.jocky.src, i32 0, ptr null }], section "llvm.metadata"

declare void @jocky_exit(i32 noundef)

declare ptr @jocky_alloc(i32 noundef)

declare ptr @jocky_get_stdout()

declare void @jocky_write(ptr noundef, ptr noundef, i32 noundef)

declare ptr @jocky_create_thread(ptr noundef, ptr noundef)

declare i32 @jocky_wait(ptr noundef, i32 noundef)

declare ptr @jocky_alloc_ex(ptr noundef, i32 noundef, i32 noundef, i32 noundef)

declare i32 @jocky_protect(ptr noundef, i32 noundef, i32 noundef, ptr noundef)

declare i32 @jocky_protect_ex(ptr noundef, ptr noundef, i32 noundef, i32 noundef, ptr noundef)

declare i32 @jocky_free(ptr noundef, i32 noundef)

declare void @jocky_memcopy(ptr noundef, ptr noundef, i32 noundef)

declare void @jocky_memzero(ptr noundef, i32 noundef)

declare i32 @jocky_read_proc(ptr noundef, ptr noundef, ptr noundef, i32 noundef)

declare i32 @jocky_write_proc(ptr noundef, ptr noundef, ptr noundef, i32 noundef)

declare ptr @jocky_create_proc(ptr noundef, i32 noundef)

declare ptr @jocky_open_proc(i32 noundef, i32 noundef)

declare i32 @jocky_terminate_proc(ptr noundef, i32 noundef)

declare i32 @jocky_get_pid()

declare i32 @jocky_get_tid()

declare i32 @jocky_get_tick()

declare i32 @jocky_close(ptr noundef)

declare i32 @jocky_wait_all(ptr noundef, i32 noundef, i32 noundef)

declare i32 @jocky_suspend(ptr noundef)

declare i32 @jocky_resume(ptr noundef)

declare i32 @jocky_get_ctx(ptr noundef, ptr noundef)

declare i32 @jocky_set_ctx(ptr noundef, ptr noundef)

declare i32 @jocky_atomic_inc(ptr noundef)

declare i32 @jocky_atomic_dec(ptr noundef)

declare i32 @jocky_nt_unmap(ptr noundef, ptr noundef)

declare i32 @jocky_nt_query_proc(ptr noundef, i32 noundef, ptr noundef, i32 noundef)

declare ptr @jocky_file_open(ptr noundef, i32 noundef, i32 noundef, i32 noundef)

declare i32 @jocky_file_read(ptr noundef, ptr noundef, i32 noundef)

declare i32 @jocky_file_size(ptr noundef)

declare void @jocky_file_close(ptr noundef)

declare ptr @jocky_file_load(ptr noundef)

declare void @jocky_xor_buf(ptr noundef, i32 noundef, ptr noundef, i32 noundef)

declare i32 @jocky_aes_decrypt(ptr noundef, i32 noundef, ptr noundef, i32 noundef)

declare void @jocky_sha256(ptr noundef, i32 noundef, ptr noundef)

declare ptr @jocky_get_proc_addr(ptr noundef, ptr noundef)

declare ptr @jocky_get_module(ptr noundef)

declare ptr @jocky_load_lib(ptr noundef)

declare i32 @jocky_get_last_err()

declare i64 @jocky_read_long(ptr noundef, i32 noundef)

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @classify(i32 noundef %n) #0 {
entry:
  %i = alloca i32, align 4
  %result = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  store i32 0, ptr %result, align 4
  %n.val = load i32, ptr %n1, align 4
  %cmp = icmp slt i32 %n.val, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i32 0, ptr %result, align 4
  br label %if.end

if.end:                                           ; preds = %if.end5, %if.then
  store i32 0, ptr %i, align 4
  br label %while.cond

if.else:                                          ; preds = %entry
  %n.val2 = load i32, ptr %n1, align 4
  %cmp3 = icmp slt i32 %n.val2, 10
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
  %i.val = load i32, ptr %i, align 4
  %n.val7 = load i32, ptr %n1, align 4
  %cmp8 = icmp slt i32 %i.val, %n.val7
  br i1 %cmp8, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %result.val = load i32, ptr %result, align 4
  %add = add i32 %result.val, 1
  store i32 %add, ptr %result, align 4
  %i.val9 = load i32, ptr %i, align 4
  %add10 = add i32 %i.val9, 1
  store i32 %add10, ptr %i, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %result.val11 = load i32, ptr %result, align 4
  ret i32 %result.val11
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @jocky_entry() #0 {
entry:
  %val = alloca i32, align 4
  %call = call i32 @classify(i32 5)
  store i32 %call, ptr %val, align 4
  %val.val = load i32, ptr %val, align 4
  call void @jocky_exit(i32 %val.val)
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable }

!llvm.ident = !{!0}
!llvm.module.flags = !{!1, !2}

!0 = !{!"JOCKY Language Frontend v2.0"}
!1 = !{i32 1, !"wchar_size", i32 2}
!2 = !{i32 7, !"uwtable", i32 2}
