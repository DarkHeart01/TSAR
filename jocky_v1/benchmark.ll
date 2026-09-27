; ModuleID = 'jocky_module'
source_filename = "jocky.jky"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.44.35225"

@g_processed = dso_local global i32 0
@.jocky.src = private unnamed_addr constant [10 x i8] c"jocky.jky\00", section "llvm.metadata", align 1
@.jocky.attr.0 = private unnamed_addr constant [24 x i8] c"flatten,sub,mba,indcall\00", section "llvm.metadata", align 1
@.jocky.attr.1 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@.jocky.attr.2 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@.jocky.attr.3 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@.jocky.attr.4 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@.jocky.attr.5 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@.jocky.attr.6 = private unnamed_addr constant [21 x i8] c"flatten,substitution\00", section "llvm.metadata", align 1
@llvm.global.annotations = appending global [7 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @cryptographic_mix, ptr @.jocky.attr.0, ptr @.jocky.src, i32 0, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @worker_thread, ptr @.jocky.attr.1, ptr @.jocky.src, i32 0, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @mat_mul, ptr @.jocky.attr.2, ptr @.jocky.src, i32 0, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @fibonacci, ptr @.jocky.attr.3, ptr @.jocky.src, i32 0, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @count_primes, ptr @.jocky.attr.4, ptr @.jocky.src, i32 0, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @gcd, ptr @.jocky.attr.5, ptr @.jocky.src, i32 0, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @collatz, ptr @.jocky.attr.6, ptr @.jocky.src, i32 0, ptr null }], section "llvm.metadata"

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

declare i32 @jocky_read_int(ptr noundef, i32 noundef)

declare void @jocky_write_int(ptr noundef, i32 noundef, i32 noundef)

declare i8 @jocky_read_byte(ptr noundef, i32 noundef)

declare void @jocky_write_byte(ptr noundef, i32 noundef, i8 noundef)

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @custom_rand(ptr noundef %seed) #0 {
entry:
  %s = alloca i32, align 4
  %seed1 = alloca ptr, align 8
  store ptr %seed, ptr %seed1, align 8
  %seed.val = load ptr, ptr %seed1, align 8
  %call = call i32 @jocky_read_int(ptr %seed.val, i32 0)
  store i32 %call, ptr %s, align 4
  %s.val = load i32, ptr %s, align 4
  %mul = mul i32 %s.val, 1103515245
  %add = add i32 %mul, 12345
  store i32 %add, ptr %s, align 4
  %seed.val2 = load ptr, ptr %seed1, align 8
  %s.val3 = load i32, ptr %s, align 4
  call void @jocky_write_int(ptr %seed.val2, i32 0, i32 %s.val3)
  %s.val4 = load i32, ptr %s, align 4
  ret i32 %s.val4
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @cryptographic_mix(ptr noundef %input, i32 noundef %len, ptr noundef %state) #0 {
entry:
  %r = alloca i32, align 4
  %b = alloca i32, align 4
  %i = alloca i32, align 4
  %v3 = alloca i32, align 4
  %v2 = alloca i32, align 4
  %v1 = alloca i32, align 4
  %v0 = alloca i32, align 4
  %state3 = alloca ptr, align 8
  %len2 = alloca i32, align 4
  %input1 = alloca ptr, align 8
  store ptr %input, ptr %input1, align 8
  store i32 %len, ptr %len2, align 4
  store ptr %state, ptr %state3, align 8
  store i32 1732584193, ptr %v0, align 4
  store i32 -271733879, ptr %v1, align 4
  store i32 -1732584194, ptr %v2, align 4
  store i32 271733878, ptr %v3, align 4
  store i32 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.end7, %entry
  %i.val = load i32, ptr %i, align 4
  %len.val = load i32, ptr %len2, align 4
  %cmp = icmp slt i32 %i.val, %len.val
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %input.val = load ptr, ptr %input1, align 8
  %i.val4 = load i32, ptr %i, align 4
  %call = call i8 @jocky_read_byte(ptr %input.val, i32 %i.val4)
  %sext32 = sext i8 %call to i32
  %band = and i32 %sext32, 255
  store i32 %band, ptr %b, align 4
  store i32 0, ptr %r, align 4
  br label %while.cond5

while.end:                                        ; preds = %while.cond
  %state.val = load ptr, ptr %state3, align 8
  %v0.val55 = load i32, ptr %v0, align 4
  %state.val56 = load ptr, ptr %state3, align 8
  %call57 = call i32 @jocky_read_int(ptr %state.val56, i32 0)
  %bxor58 = xor i32 %v0.val55, %call57
  call void @jocky_write_int(ptr %state.val, i32 0, i32 %bxor58)
  %state.val59 = load ptr, ptr %state3, align 8
  %v1.val60 = load i32, ptr %v1, align 4
  %state.val61 = load ptr, ptr %state3, align 8
  %call62 = call i32 @jocky_read_int(ptr %state.val61, i32 4)
  %bxor63 = xor i32 %v1.val60, %call62
  call void @jocky_write_int(ptr %state.val59, i32 4, i32 %bxor63)
  %state.val64 = load ptr, ptr %state3, align 8
  %v2.val65 = load i32, ptr %v2, align 4
  %state.val66 = load ptr, ptr %state3, align 8
  %call67 = call i32 @jocky_read_int(ptr %state.val66, i32 8)
  %bxor68 = xor i32 %v2.val65, %call67
  call void @jocky_write_int(ptr %state.val64, i32 8, i32 %bxor68)
  %state.val69 = load ptr, ptr %state3, align 8
  %v3.val70 = load i32, ptr %v3, align 4
  %state.val71 = load ptr, ptr %state3, align 8
  %call72 = call i32 @jocky_read_int(ptr %state.val71, i32 12)
  %bxor73 = xor i32 %v3.val70, %call72
  call void @jocky_write_int(ptr %state.val69, i32 12, i32 %bxor73)
  ret void

while.cond5:                                      ; preds = %while.body6, %while.body
  %r.val = load i32, ptr %r, align 4
  %cmp8 = icmp slt i32 %r.val, 64
  br i1 %cmp8, label %while.body6, label %while.end7

while.body6:                                      ; preds = %while.cond5
  %v0.val = load i32, ptr %v0, align 4
  %v1.val = load i32, ptr %v1, align 4
  %shl = shl i32 %v1.val, 4
  %v2.val = load i32, ptr %v2, align 4
  %shr = lshr i32 %v2.val, 5
  %bxor = xor i32 %shl, %shr
  %v1.val9 = load i32, ptr %v1, align 4
  %add = add i32 %bxor, %v1.val9
  %add10 = add i32 %v0.val, %add
  %b.val = load i32, ptr %b, align 4
  %r.val11 = load i32, ptr %r, align 4
  %add12 = add i32 %b.val, %r.val11
  %bxor13 = xor i32 %add10, %add12
  store i32 %bxor13, ptr %v0, align 4
  %v1.val14 = load i32, ptr %v1, align 4
  %v2.val15 = load i32, ptr %v2, align 4
  %shl16 = shl i32 %v2.val15, 3
  %v3.val = load i32, ptr %v3, align 4
  %shr17 = lshr i32 %v3.val, 2
  %bxor18 = xor i32 %shl16, %shr17
  %v0.val19 = load i32, ptr %v0, align 4
  %add20 = add i32 %bxor18, %v0.val19
  %bxor21 = xor i32 %v1.val14, %add20
  store i32 %bxor21, ptr %v1, align 4
  %v2.val22 = load i32, ptr %v2, align 4
  %v3.val23 = load i32, ptr %v3, align 4
  %shl24 = shl i32 %v3.val23, 6
  %v0.val25 = load i32, ptr %v0, align 4
  %shr26 = lshr i32 %v0.val25, 3
  %bxor27 = xor i32 %shl24, %shr26
  %v1.val28 = load i32, ptr %v1, align 4
  %add29 = add i32 %bxor27, %v1.val28
  %add30 = add i32 %v2.val22, %add29
  %b.val31 = load i32, ptr %b, align 4
  %bxor32 = xor i32 %add30, %b.val31
  store i32 %bxor32, ptr %v2, align 4
  %v3.val33 = load i32, ptr %v3, align 4
  %v0.val34 = load i32, ptr %v0, align 4
  %shl35 = shl i32 %v0.val34, 5
  %v1.val36 = load i32, ptr %v1, align 4
  %shr37 = lshr i32 %v1.val36, 4
  %bxor38 = xor i32 %shl35, %shr37
  %v2.val39 = load i32, ptr %v2, align 4
  %add40 = add i32 %bxor38, %v2.val39
  %bxor41 = xor i32 %v3.val33, %add40
  store i32 %bxor41, ptr %v3, align 4
  %v0.val42 = load i32, ptr %v0, align 4
  %shl43 = shl i32 %v0.val42, 7
  %v0.val44 = load i32, ptr %v0, align 4
  %shr45 = lshr i32 %v0.val44, 25
  %bor = or i32 %shl43, %shr45
  store i32 %bor, ptr %v0, align 4
  %v2.val46 = load i32, ptr %v2, align 4
  %shl47 = shl i32 %v2.val46, 11
  %v2.val48 = load i32, ptr %v2, align 4
  %shr49 = lshr i32 %v2.val48, 21
  %bor50 = or i32 %shl47, %shr49
  store i32 %bor50, ptr %v2, align 4
  %r.val51 = load i32, ptr %r, align 4
  %add52 = add i32 %r.val51, 1
  store i32 %add52, ptr %r, align 4
  br label %while.cond5

while.end7:                                       ; preds = %while.cond5
  %i.val53 = load i32, ptr %i, align 4
  %add54 = add i32 %i.val53, 1
  store i32 %add54, ptr %i, align 4
  br label %while.cond
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @worker_thread(ptr noundef %param) #0 {
entry:
  %temp = alloca ptr, align 8
  %hash_b = alloca i32, align 4
  %hash_a = alloca i32, align 4
  %rec_b = alloca ptr, align 8
  %rec_a = alloca ptr, align 8
  %inner = alloca i32, align 4
  %outer = alloca i32, align 4
  %hash_ptr = alloca ptr, align 8
  %b = alloca i32, align 4
  %t = alloca i32, align 4
  %rand2 = alloca i32, align 4
  %rand1 = alloca i32, align 4
  %text_ptr = alloca ptr, align 8
  %record = alloca ptr, align 8
  %i = alloca i32, align 4
  %dataset = alloca ptr, align 8
  %seed = alloca ptr, align 8
  %seed_val = alloca i32, align 4
  %HASH_OFFSET = alloca i32, align 4
  %TEXT_OFFSET = alloca i32, align 4
  %RECORD_SIZE = alloca i32, align 4
  %param1 = alloca ptr, align 8
  store ptr %param, ptr %param1, align 8
  store i32 276, ptr %RECORD_SIZE, align 4
  store i32 4, ptr %TEXT_OFFSET, align 4
  store i32 260, ptr %HASH_OFFSET, align 4
  %call = call i32 @jocky_get_tick()
  %call2 = call i32 @jocky_get_tid()
  %add = add i32 %call, %call2
  store i32 %add, ptr %seed_val, align 4
  %call3 = call ptr @jocky_alloc(i32 4)
  store ptr %call3, ptr %seed, align 8
  %seed.val = load ptr, ptr %seed, align 8
  %seed_val.val = load i32, ptr %seed_val, align 4
  call void @jocky_write_int(ptr %seed.val, i32 0, i32 %seed_val.val)
  %param.val = load ptr, ptr %param1, align 8
  store ptr %param.val, ptr %dataset, align 8
  store i32 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.end14, %entry
  %i.val = load i32, ptr %i, align 4
  %cmp = icmp slt i32 %i.val, 1000
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %dataset.val = load ptr, ptr %dataset, align 8
  %i.val4 = load i32, ptr %i, align 4
  %RECORD_SIZE.val = load i32, ptr %RECORD_SIZE, align 4
  %mul = mul i32 %i.val4, %RECORD_SIZE.val
  %ptradd = getelementptr i8, ptr %dataset.val, i32 %mul
  store ptr %ptradd, ptr %record, align 8
  %record.val = load ptr, ptr %record, align 8
  %i.val5 = load i32, ptr %i, align 4
  call void @jocky_write_int(ptr %record.val, i32 0, i32 %i.val5)
  %record.val6 = load ptr, ptr %record, align 8
  %TEXT_OFFSET.val = load i32, ptr %TEXT_OFFSET, align 4
  %ptradd7 = getelementptr i8, ptr %record.val6, i32 %TEXT_OFFSET.val
  store ptr %ptradd7, ptr %text_ptr, align 8
  %seed.val8 = load ptr, ptr %seed, align 8
  %call9 = call i32 @custom_rand(ptr %seed.val8)
  %band = and i32 %call9, 9999
  store i32 %band, ptr %rand1, align 4
  %seed.val10 = load ptr, ptr %seed, align 8
  %call11 = call i32 @custom_rand(ptr %seed.val10)
  store i32 %call11, ptr %rand2, align 4
  store i32 0, ptr %t, align 4
  br label %while.cond12

while.end:                                        ; preds = %while.cond
  store i32 0, ptr %outer, align 4
  br label %while.cond31

while.cond12:                                     ; preds = %while.body13, %while.body
  %t.val = load i32, ptr %t, align 4
  %cmp15 = icmp slt i32 %t.val, 32
  br i1 %cmp15, label %while.body13, label %while.end14

while.body13:                                     ; preds = %while.cond12
  %rand1.val = load i32, ptr %rand1, align 4
  %rand2.val = load i32, ptr %rand2, align 4
  %bxor = xor i32 %rand1.val, %rand2.val
  %t.val16 = load i32, ptr %t, align 4
  %bxor17 = xor i32 %bxor, %t.val16
  %band18 = and i32 %bxor17, 255
  store i32 %band18, ptr %b, align 4
  %text_ptr.val = load ptr, ptr %text_ptr, align 8
  %t.val19 = load i32, ptr %t, align 4
  %b.val = load i32, ptr %b, align 4
  %trunc8 = trunc i32 %b.val to i8
  call void @jocky_write_byte(ptr %text_ptr.val, i32 %t.val19, i8 %trunc8)
  %t.val20 = load i32, ptr %t, align 4
  %add21 = add i32 %t.val20, 1
  store i32 %add21, ptr %t, align 4
  br label %while.cond12

while.end14:                                      ; preds = %while.cond12
  %record.val22 = load ptr, ptr %record, align 8
  %HASH_OFFSET.val = load i32, ptr %HASH_OFFSET, align 4
  %ptradd23 = getelementptr i8, ptr %record.val22, i32 %HASH_OFFSET.val
  store ptr %ptradd23, ptr %hash_ptr, align 8
  %hash_ptr.val = load ptr, ptr %hash_ptr, align 8
  call void @jocky_write_int(ptr %hash_ptr.val, i32 0, i32 1732584193)
  %hash_ptr.val24 = load ptr, ptr %hash_ptr, align 8
  call void @jocky_write_int(ptr %hash_ptr.val24, i32 4, i32 -271733879)
  %hash_ptr.val25 = load ptr, ptr %hash_ptr, align 8
  call void @jocky_write_int(ptr %hash_ptr.val25, i32 8, i32 -1732584194)
  %hash_ptr.val26 = load ptr, ptr %hash_ptr, align 8
  call void @jocky_write_int(ptr %hash_ptr.val26, i32 12, i32 271733878)
  %text_ptr.val27 = load ptr, ptr %text_ptr, align 8
  %hash_ptr.val28 = load ptr, ptr %hash_ptr, align 8
  call void @cryptographic_mix(ptr %text_ptr.val27, i32 32, ptr %hash_ptr.val28)
  %atomic = call i32 @jocky_atomic_inc(ptr @g_processed)
  %i.val29 = load i32, ptr %i, align 4
  %add30 = add i32 %i.val29, 1
  store i32 %add30, ptr %i, align 4
  br label %while.cond

while.cond31:                                     ; preds = %while.end37, %while.end
  %outer.val = load i32, ptr %outer, align 4
  %cmp34 = icmp slt i32 %outer.val, 999
  br i1 %cmp34, label %while.body32, label %while.end33

while.body32:                                     ; preds = %while.cond31
  store i32 0, ptr %inner, align 4
  br label %while.cond35

while.end33:                                      ; preds = %while.cond31
  %seed.val75 = load ptr, ptr %seed, align 8
  %call76 = call i32 @jocky_free(ptr %seed.val75, i32 4)
  ret i32 0

while.cond35:                                     ; preds = %if.end, %while.body32
  %inner.val = load i32, ptr %inner, align 4
  %outer.val38 = load i32, ptr %outer, align 4
  %sub = sub i32 999, %outer.val38
  %cmp39 = icmp slt i32 %inner.val, %sub
  br i1 %cmp39, label %while.body36, label %while.end37

while.body36:                                     ; preds = %while.cond35
  %dataset.val40 = load ptr, ptr %dataset, align 8
  %inner.val41 = load i32, ptr %inner, align 4
  %RECORD_SIZE.val42 = load i32, ptr %RECORD_SIZE, align 4
  %mul43 = mul i32 %inner.val41, %RECORD_SIZE.val42
  %ptradd44 = getelementptr i8, ptr %dataset.val40, i32 %mul43
  store ptr %ptradd44, ptr %rec_a, align 8
  %dataset.val45 = load ptr, ptr %dataset, align 8
  %inner.val46 = load i32, ptr %inner, align 4
  %add47 = add i32 %inner.val46, 1
  %RECORD_SIZE.val48 = load i32, ptr %RECORD_SIZE, align 4
  %mul49 = mul i32 %add47, %RECORD_SIZE.val48
  %ptradd50 = getelementptr i8, ptr %dataset.val45, i32 %mul49
  store ptr %ptradd50, ptr %rec_b, align 8
  %rec_a.val = load ptr, ptr %rec_a, align 8
  %HASH_OFFSET.val51 = load i32, ptr %HASH_OFFSET, align 4
  %ptradd52 = getelementptr i8, ptr %rec_a.val, i32 %HASH_OFFSET.val51
  %call53 = call i32 @jocky_read_int(ptr %ptradd52, i32 0)
  store i32 %call53, ptr %hash_a, align 4
  %rec_b.val = load ptr, ptr %rec_b, align 8
  %HASH_OFFSET.val54 = load i32, ptr %HASH_OFFSET, align 4
  %ptradd55 = getelementptr i8, ptr %rec_b.val, i32 %HASH_OFFSET.val54
  %call56 = call i32 @jocky_read_int(ptr %ptradd55, i32 0)
  store i32 %call56, ptr %hash_b, align 4
  %hash_a.val = load i32, ptr %hash_a, align 4
  %hash_b.val = load i32, ptr %hash_b, align 4
  %cmp57 = icmp sgt i32 %hash_a.val, %hash_b.val
  br i1 %cmp57, label %if.then, label %if.end

while.end37:                                      ; preds = %while.cond35
  %outer.val73 = load i32, ptr %outer, align 4
  %add74 = add i32 %outer.val73, 1
  store i32 %add74, ptr %outer, align 4
  br label %while.cond31

if.then:                                          ; preds = %while.body36
  %RECORD_SIZE.val58 = load i32, ptr %RECORD_SIZE, align 4
  %call59 = call ptr @jocky_alloc(i32 %RECORD_SIZE.val58)
  store ptr %call59, ptr %temp, align 8
  %temp.val = load ptr, ptr %temp, align 8
  %rec_a.val60 = load ptr, ptr %rec_a, align 8
  %RECORD_SIZE.val61 = load i32, ptr %RECORD_SIZE, align 4
  call void @jocky_memcopy(ptr %temp.val, ptr %rec_a.val60, i32 %RECORD_SIZE.val61)
  %rec_a.val62 = load ptr, ptr %rec_a, align 8
  %rec_b.val63 = load ptr, ptr %rec_b, align 8
  %RECORD_SIZE.val64 = load i32, ptr %RECORD_SIZE, align 4
  call void @jocky_memcopy(ptr %rec_a.val62, ptr %rec_b.val63, i32 %RECORD_SIZE.val64)
  %rec_b.val65 = load ptr, ptr %rec_b, align 8
  %temp.val66 = load ptr, ptr %temp, align 8
  %RECORD_SIZE.val67 = load i32, ptr %RECORD_SIZE, align 4
  call void @jocky_memcopy(ptr %rec_b.val65, ptr %temp.val66, i32 %RECORD_SIZE.val67)
  %temp.val68 = load ptr, ptr %temp, align 8
  %RECORD_SIZE.val69 = load i32, ptr %RECORD_SIZE, align 4
  %call70 = call i32 @jocky_free(ptr %temp.val68, i32 %RECORD_SIZE.val69)
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body36
  %inner.val71 = load i32, ptr %inner, align 4
  %add72 = add i32 %inner.val71, 1
  store i32 %add72, ptr %inner, align 4
  br label %while.cond35
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @mat_mul(ptr noundef %a, ptr noundef %b, ptr noundef %c, i32 noundef %n) #0 {
entry:
  %bv = alloca i32, align 4
  %av = alloca i32, align 4
  %k = alloca i32, align 4
  %sum = alloca i32, align 4
  %j = alloca i32, align 4
  %i = alloca i32, align 4
  %n4 = alloca i32, align 4
  %c3 = alloca ptr, align 8
  %b2 = alloca ptr, align 8
  %a1 = alloca ptr, align 8
  store ptr %a, ptr %a1, align 8
  store ptr %b, ptr %b2, align 8
  store ptr %c, ptr %c3, align 8
  store i32 %n, ptr %n4, align 4
  store i32 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.end7, %entry
  %i.val = load i32, ptr %i, align 4
  %n.val = load i32, ptr %n4, align 4
  %cmp = icmp slt i32 %i.val, %n.val
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  store i32 0, ptr %j, align 4
  br label %while.cond5

while.end:                                        ; preds = %while.cond
  ret void

while.cond5:                                      ; preds = %while.end12, %while.body
  %j.val = load i32, ptr %j, align 4
  %n.val8 = load i32, ptr %n4, align 4
  %cmp9 = icmp slt i32 %j.val, %n.val8
  br i1 %cmp9, label %while.body6, label %while.end7

while.body6:                                      ; preds = %while.cond5
  store i32 0, ptr %sum, align 4
  store i32 0, ptr %k, align 4
  br label %while.cond10

while.end7:                                       ; preds = %while.cond5
  %i.val39 = load i32, ptr %i, align 4
  %add40 = add i32 %i.val39, 1
  store i32 %add40, ptr %i, align 4
  br label %while.cond

while.cond10:                                     ; preds = %while.body11, %while.body6
  %k.val = load i32, ptr %k, align 4
  %n.val13 = load i32, ptr %n4, align 4
  %cmp14 = icmp slt i32 %k.val, %n.val13
  br i1 %cmp14, label %while.body11, label %while.end12

while.body11:                                     ; preds = %while.cond10
  %a.val = load ptr, ptr %a1, align 8
  %i.val15 = load i32, ptr %i, align 4
  %n.val16 = load i32, ptr %n4, align 4
  %mul = mul i32 %i.val15, %n.val16
  %k.val17 = load i32, ptr %k, align 4
  %add = add i32 %mul, %k.val17
  %mul18 = mul i32 %add, 4
  %call = call i32 @jocky_read_int(ptr %a.val, i32 %mul18)
  store i32 %call, ptr %av, align 4
  %b.val = load ptr, ptr %b2, align 8
  %k.val19 = load i32, ptr %k, align 4
  %n.val20 = load i32, ptr %n4, align 4
  %mul21 = mul i32 %k.val19, %n.val20
  %j.val22 = load i32, ptr %j, align 4
  %add23 = add i32 %mul21, %j.val22
  %mul24 = mul i32 %add23, 4
  %call25 = call i32 @jocky_read_int(ptr %b.val, i32 %mul24)
  store i32 %call25, ptr %bv, align 4
  %sum.val = load i32, ptr %sum, align 4
  %av.val = load i32, ptr %av, align 4
  %bv.val = load i32, ptr %bv, align 4
  %mul26 = mul i32 %av.val, %bv.val
  %add27 = add i32 %sum.val, %mul26
  store i32 %add27, ptr %sum, align 4
  %k.val28 = load i32, ptr %k, align 4
  %add29 = add i32 %k.val28, 1
  store i32 %add29, ptr %k, align 4
  br label %while.cond10

while.end12:                                      ; preds = %while.cond10
  %c.val = load ptr, ptr %c3, align 8
  %i.val30 = load i32, ptr %i, align 4
  %n.val31 = load i32, ptr %n4, align 4
  %mul32 = mul i32 %i.val30, %n.val31
  %j.val33 = load i32, ptr %j, align 4
  %add34 = add i32 %mul32, %j.val33
  %mul35 = mul i32 %add34, 4
  %sum.val36 = load i32, ptr %sum, align 4
  call void @jocky_write_int(ptr %c.val, i32 %mul35, i32 %sum.val36)
  %j.val37 = load i32, ptr %j, align 4
  %add38 = add i32 %j.val37, 1
  store i32 %add38, ptr %j, align 4
  br label %while.cond5
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @fibonacci(i32 noundef %n) #0 {
entry:
  %c = alloca i32, align 4
  %i = alloca i32, align 4
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %n.val = load i32, ptr %n1, align 4
  %cmp = icmp sle i32 %n.val, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 0

if.end:                                           ; preds = %entry
  %n.val2 = load i32, ptr %n1, align 4
  %cmp3 = icmp eq i32 %n.val2, 1
  br i1 %cmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 1

if.end5:                                          ; preds = %if.end
  store i32 0, ptr %a, align 4
  store i32 1, ptr %b, align 4
  store i32 2, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.body, %if.end5
  %i.val = load i32, ptr %i, align 4
  %n.val6 = load i32, ptr %n1, align 4
  %cmp7 = icmp sle i32 %i.val, %n.val6
  br i1 %cmp7, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %a.val = load i32, ptr %a, align 4
  %b.val = load i32, ptr %b, align 4
  %add = add i32 %a.val, %b.val
  store i32 %add, ptr %c, align 4
  %b.val8 = load i32, ptr %b, align 4
  store i32 %b.val8, ptr %a, align 4
  %c.val = load i32, ptr %c, align 4
  store i32 %c.val, ptr %b, align 4
  %i.val9 = load i32, ptr %i, align 4
  %add10 = add i32 %i.val9, 1
  store i32 %add10, ptr %i, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %b.val11 = load i32, ptr %b, align 4
  ret i32 %b.val11
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @count_primes(i32 noundef %limit) #0 {
entry:
  %k = alloca i32, align 4
  %count = alloca i32, align 4
  %j = alloca i32, align 4
  %i = alloca i32, align 4
  %sieve = alloca ptr, align 8
  %limit1 = alloca i32, align 4
  store i32 %limit, ptr %limit1, align 4
  %limit.val = load i32, ptr %limit1, align 4
  %mul = mul i32 %limit.val, 4
  %call = call ptr @jocky_alloc(i32 %mul)
  store ptr %call, ptr %sieve, align 8
  %sieve.val = load ptr, ptr %sieve, align 8
  %limit.val2 = load i32, ptr %limit1, align 4
  %mul3 = mul i32 %limit.val2, 4
  call void @jocky_memzero(ptr %sieve.val, i32 %mul3)
  store i32 2, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %if.end, %entry
  %i.val = load i32, ptr %i, align 4
  %i.val4 = load i32, ptr %i, align 4
  %mul5 = mul i32 %i.val, %i.val4
  %limit.val6 = load i32, ptr %limit1, align 4
  %cmp = icmp slt i32 %mul5, %limit.val6
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %sieve.val7 = load ptr, ptr %sieve, align 8
  %i.val8 = load i32, ptr %i, align 4
  %mul9 = mul i32 %i.val8, 4
  %call10 = call i32 @jocky_read_int(ptr %sieve.val7, i32 %mul9)
  %cmp11 = icmp eq i32 %call10, 0
  br i1 %cmp11, label %if.then, label %if.end

while.end:                                        ; preds = %while.cond
  store i32 0, ptr %count, align 4
  store i32 2, ptr %k, align 4
  br label %while.cond27

if.then:                                          ; preds = %while.body
  %i.val12 = load i32, ptr %i, align 4
  %i.val13 = load i32, ptr %i, align 4
  %mul14 = mul i32 %i.val12, %i.val13
  store i32 %mul14, ptr %j, align 4
  br label %while.cond15

if.end:                                           ; preds = %while.end17, %while.body
  %i.val25 = load i32, ptr %i, align 4
  %add26 = add i32 %i.val25, 1
  store i32 %add26, ptr %i, align 4
  br label %while.cond

while.cond15:                                     ; preds = %while.body16, %if.then
  %j.val = load i32, ptr %j, align 4
  %limit.val18 = load i32, ptr %limit1, align 4
  %cmp19 = icmp slt i32 %j.val, %limit.val18
  br i1 %cmp19, label %while.body16, label %while.end17

while.body16:                                     ; preds = %while.cond15
  %sieve.val20 = load ptr, ptr %sieve, align 8
  %j.val21 = load i32, ptr %j, align 4
  %mul22 = mul i32 %j.val21, 4
  call void @jocky_write_int(ptr %sieve.val20, i32 %mul22, i32 1)
  %j.val23 = load i32, ptr %j, align 4
  %i.val24 = load i32, ptr %i, align 4
  %add = add i32 %j.val23, %i.val24
  store i32 %add, ptr %j, align 4
  br label %while.cond15

while.end17:                                      ; preds = %while.cond15
  br label %if.end

while.cond27:                                     ; preds = %if.end38, %while.end
  %k.val = load i32, ptr %k, align 4
  %limit.val30 = load i32, ptr %limit1, align 4
  %cmp31 = icmp slt i32 %k.val, %limit.val30
  br i1 %cmp31, label %while.body28, label %while.end29

while.body28:                                     ; preds = %while.cond27
  %sieve.val32 = load ptr, ptr %sieve, align 8
  %k.val33 = load i32, ptr %k, align 4
  %mul34 = mul i32 %k.val33, 4
  %call35 = call i32 @jocky_read_int(ptr %sieve.val32, i32 %mul34)
  %cmp36 = icmp eq i32 %call35, 0
  br i1 %cmp36, label %if.then37, label %if.end38

while.end29:                                      ; preds = %while.cond27
  %sieve.val42 = load ptr, ptr %sieve, align 8
  %limit.val43 = load i32, ptr %limit1, align 4
  %mul44 = mul i32 %limit.val43, 4
  %call45 = call i32 @jocky_free(ptr %sieve.val42, i32 %mul44)
  %count.val46 = load i32, ptr %count, align 4
  ret i32 %count.val46

if.then37:                                        ; preds = %while.body28
  %count.val = load i32, ptr %count, align 4
  %add39 = add i32 %count.val, 1
  store i32 %add39, ptr %count, align 4
  br label %if.end38

if.end38:                                         ; preds = %if.then37, %while.body28
  %k.val40 = load i32, ptr %k, align 4
  %add41 = add i32 %k.val40, 1
  store i32 %add41, ptr %k, align 4
  br label %while.cond27
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @gcd(i32 noundef %a, i32 noundef %b) #0 {
entry:
  %t = alloca i32, align 4
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %b.val = load i32, ptr %b2, align 4
  %cmp = icmp ne i32 %b.val, 0
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %b.val3 = load i32, ptr %b2, align 4
  store i32 %b.val3, ptr %t, align 4
  %a.val = load i32, ptr %a1, align 4
  %a.val4 = load i32, ptr %a1, align 4
  %b.val5 = load i32, ptr %b2, align 4
  %div = sdiv i32 %a.val4, %b.val5
  %b.val6 = load i32, ptr %b2, align 4
  %mul = mul i32 %div, %b.val6
  %sub = sub i32 %a.val, %mul
  store i32 %sub, ptr %b2, align 4
  %t.val = load i32, ptr %t, align 4
  store i32 %t.val, ptr %a1, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %a.val7 = load i32, ptr %a1, align 4
  ret i32 %a.val7
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @collatz(i32 noundef %n) #0 {
entry:
  %steps = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  store i32 0, ptr %steps, align 4
  br label %while.cond

while.cond:                                       ; preds = %if.end, %entry
  %n.val = load i32, ptr %n1, align 4
  %cmp = icmp ne i32 %n.val, 1
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %n.val2 = load i32, ptr %n1, align 4
  %band = and i32 %n.val2, 1
  %cmp3 = icmp eq i32 %band, 0
  br i1 %cmp3, label %if.then, label %if.else

while.end:                                        ; preds = %while.cond
  %steps.val7 = load i32, ptr %steps, align 4
  ret i32 %steps.val7

if.then:                                          ; preds = %while.body
  %n.val4 = load i32, ptr %n1, align 4
  %div = sdiv i32 %n.val4, 2
  store i32 %div, ptr %n1, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %steps.val = load i32, ptr %steps, align 4
  %add6 = add i32 %steps.val, 1
  store i32 %add6, ptr %steps, align 4
  br label %while.cond

if.else:                                          ; preds = %while.body
  %n.val5 = load i32, ptr %n1, align 4
  %mul = mul i32 %n.val5, 3
  %add = add i32 %mul, 1
  store i32 %add, ptr %n1, align 4
  br label %if.end
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @jocky_entry() #0 {
entry:
  %mat_c = alloca ptr, align 8
  %mat_b = alloca ptr, align 8
  %mat_a = alloca ptr, align 8
  %count = alloca i32, align 4
  %i = alloca i32, align 4
  %threads = alloca [4 x i64], align 8
  %dataset = alloca ptr, align 8
  %g = alloca i32, align 4
  %c_steps = alloca i32, align 4
  %primes = alloca i32, align 4
  %fib = alloca i32, align 4
  %NUM_THREADS = alloca i32, align 4
  %NUM_RECORDS = alloca i32, align 4
  %RECORD_SIZE = alloca i32, align 4
  store i32 276, ptr %RECORD_SIZE, align 4
  store i32 1000, ptr %NUM_RECORDS, align 4
  store i32 4, ptr %NUM_THREADS, align 4
  %call = call i32 @fibonacci(i32 35)
  store i32 %call, ptr %fib, align 4
  %call1 = call i32 @count_primes(i32 1000)
  store i32 %call1, ptr %primes, align 4
  %call2 = call i32 @collatz(i32 27)
  store i32 %call2, ptr %c_steps, align 4
  %fib.val = load i32, ptr %fib, align 4
  %primes.val = load i32, ptr %primes, align 4
  %call3 = call i32 @gcd(i32 %fib.val, i32 %primes.val)
  store i32 %call3, ptr %g, align 4
  %NUM_RECORDS.val = load i32, ptr %NUM_RECORDS, align 4
  %RECORD_SIZE.val = load i32, ptr %RECORD_SIZE, align 4
  %mul = mul i32 %NUM_RECORDS.val, %RECORD_SIZE.val
  %call4 = call ptr @jocky_alloc(i32 %mul)
  store ptr %call4, ptr %dataset, align 8
  %dataset.val = load ptr, ptr %dataset, align 8
  %cmp = icmp eq ptr %dataset.val, null
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  call void @jocky_exit(i32 1)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %dataset.val5 = load ptr, ptr %dataset, align 8
  %NUM_RECORDS.val6 = load i32, ptr %NUM_RECORDS, align 4
  %RECORD_SIZE.val7 = load i32, ptr %RECORD_SIZE, align 4
  %mul8 = mul i32 %NUM_RECORDS.val6, %RECORD_SIZE.val7
  call void @jocky_memzero(ptr %dataset.val5, i32 %mul8)
  store [4 x i64] zeroinitializer, ptr %threads, align 8
  store i32 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %if.end16, %if.end
  %i.val = load i32, ptr %i, align 4
  %NUM_THREADS.val = load i32, ptr %NUM_THREADS, align 4
  %cmp9 = icmp slt i32 %i.val, %NUM_THREADS.val
  br i1 %cmp9, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %i.val10 = load i32, ptr %i, align 4
  %dataset.val11 = load ptr, ptr %dataset, align 8
  %call12 = call ptr @jocky_create_thread(ptr @worker_thread, ptr %dataset.val11)
  %elem = getelementptr [4 x i64], ptr %threads, i32 0, i32 %i.val10
  %p2i = ptrtoint ptr %call12 to i64
  store i64 %p2i, ptr %elem, align 8
  %i.val13 = load i32, ptr %i, align 4
  %elem.ptr = getelementptr [4 x i64], ptr %threads, i32 0, i32 %i.val13
  %threads.elem = load i64, ptr %elem.ptr, align 8
  %cmp14 = icmp eq i64 %threads.elem, -1
  br i1 %cmp14, label %if.then15, label %if.end16

while.end:                                        ; preds = %while.cond
  %NUM_THREADS.val18 = load i32, ptr %NUM_THREADS, align 4
  %call19 = call i32 @jocky_wait_all(ptr %threads, i32 %NUM_THREADS.val18, i32 -1)
  store i32 0, ptr %i, align 4
  br label %while.cond20

if.then15:                                        ; preds = %while.body
  call void @jocky_exit(i32 2)
  br label %if.end16

if.end16:                                         ; preds = %if.then15, %while.body
  %i.val17 = load i32, ptr %i, align 4
  %add = add i32 %i.val17, 1
  store i32 %add, ptr %i, align 4
  br label %while.cond

while.cond20:                                     ; preds = %while.body21, %while.end
  %i.val23 = load i32, ptr %i, align 4
  %NUM_THREADS.val24 = load i32, ptr %NUM_THREADS, align 4
  %cmp25 = icmp slt i32 %i.val23, %NUM_THREADS.val24
  br i1 %cmp25, label %while.body21, label %while.end22

while.body21:                                     ; preds = %while.cond20
  %i.val26 = load i32, ptr %i, align 4
  %elem.ptr27 = getelementptr [4 x i64], ptr %threads, i32 0, i32 %i.val26
  %threads.elem28 = load i64, ptr %elem.ptr27, align 8
  %i2p = inttoptr i64 %threads.elem28 to ptr
  %call29 = call i32 @jocky_close(ptr %i2p)
  %i.val30 = load i32, ptr %i, align 4
  %add31 = add i32 %i.val30, 1
  store i32 %add31, ptr %i, align 4
  br label %while.cond20

while.end22:                                      ; preds = %while.cond20
  %g_processed.val = load i32, ptr @g_processed, align 4
  store i32 %g_processed.val, ptr %count, align 4
  %dataset.val32 = load ptr, ptr %dataset, align 8
  %NUM_RECORDS.val33 = load i32, ptr %NUM_RECORDS, align 4
  %RECORD_SIZE.val34 = load i32, ptr %RECORD_SIZE, align 4
  %mul35 = mul i32 %NUM_RECORDS.val33, %RECORD_SIZE.val34
  %call36 = call i32 @jocky_free(ptr %dataset.val32, i32 %mul35)
  %call37 = call ptr @jocky_alloc(i32 64)
  store ptr %call37, ptr %mat_a, align 8
  %call38 = call ptr @jocky_alloc(i32 64)
  store ptr %call38, ptr %mat_b, align 8
  %call39 = call ptr @jocky_alloc(i32 64)
  store ptr %call39, ptr %mat_c, align 8
  %mat_a.val = load ptr, ptr %mat_a, align 8
  call void @jocky_memzero(ptr %mat_a.val, i32 64)
  %mat_b.val = load ptr, ptr %mat_b, align 8
  call void @jocky_memzero(ptr %mat_b.val, i32 64)
  %mat_c.val = load ptr, ptr %mat_c, align 8
  call void @jocky_memzero(ptr %mat_c.val, i32 64)
  %mat_a.val40 = load ptr, ptr %mat_a, align 8
  %mat_b.val41 = load ptr, ptr %mat_b, align 8
  %mat_c.val42 = load ptr, ptr %mat_c, align 8
  call void @mat_mul(ptr %mat_a.val40, ptr %mat_b.val41, ptr %mat_c.val42, i32 4)
  %mat_a.val43 = load ptr, ptr %mat_a, align 8
  %call44 = call i32 @jocky_free(ptr %mat_a.val43, i32 64)
  %mat_b.val45 = load ptr, ptr %mat_b, align 8
  %call46 = call i32 @jocky_free(ptr %mat_b.val45, i32 64)
  %mat_c.val47 = load ptr, ptr %mat_c, align 8
  %call48 = call i32 @jocky_free(ptr %mat_c.val47, i32 64)
  %count.val = load i32, ptr %count, align 4
  %g.val = load i32, ptr %g, align 4
  %add49 = add i32 %count.val, %g.val
  %c_steps.val = load i32, ptr %c_steps, align 4
  %add50 = add i32 %add49, %c_steps.val
  call void @jocky_exit(i32 %add50)
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable }

!llvm.ident = !{!0}
!llvm.module.flags = !{!1, !2}

!0 = !{!"JOCKY Language Frontend v2.0"}
!1 = !{i32 1, !"wchar_size", i32 2}
!2 = !{i32 7, !"uwtable", i32 2}
