#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MAX_PATH_LEN 1024
#define MAX_CMD_LEN  8192

static void lnk_append(char* buf, size_t bufsz, const char* s) {
    size_t used = strlen(buf);
    size_t rem  = bufsz - used;
    if (rem > 1) strncat(buf + used, s, rem - 1);
}

struct JockyArgs {
    char input_cpp[MAX_PATH_LEN];
    char output_exe[MAX_PATH_LEN];
    char passes[256];
    char entry[64];
    char spoof_template[MAX_PATH_LEN];
    int  use_poly;
    int  use_spoof;
    int  use_nostdlib;
    int  verbose;
};

static void get_exe_dir(char* out, int size) {
    GetModuleFileNameA(NULL, out, size);
    char* last = out;
    for (char* p = out; *p; p++)
        if (*p == '\\' || *p == '/') last = p;
    *last = '\0';
}

static void resolve_path(const char* in, char* out, int size) {
    GetFullPathNameA(in, size, out, NULL);
}

static int run_cmd(const char* cmd, int verbose) {
    if (verbose) printf("[cmd] %s\n", cmd);
    char wrapped[MAX_CMD_LEN + 16];
    snprintf(wrapped, sizeof(wrapped), "cmd /c \"%s\"", cmd);
    int ret = system(wrapped);
    if (ret != 0) printf("[-] Command failed: exit code %d\n", ret);
    return ret;
}

static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static int has_extension(const char* path, const char* ext) {
    size_t plen = strlen(path);
    size_t elen = strlen(ext);
    if (plen < elen) return 0;
    const char* tail = path + plen - elen;
    for (size_t i = 0; i < elen; i++) {
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)ext[i]))
            return 0;
    }
    return 1;
}

// Helper to check if a file contains a specific symbol/string reference
static int file_contains_string(const char* filepath, const char* str) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return 0; }
    char* buf = (char*)malloc(len + 1);
    if (!buf) { fclose(f); return 0; }
    size_t read_bytes = fread(buf, 1, len, f);
    buf[read_bytes] = '\0';
    fclose(f);
    int found = (strstr(buf, str) != NULL);
    free(buf);
    return found;
}

static void print_usage() {
    printf("\nJOCKY Compiler Framework (Modular Runtime v2)\n=============================================\n\n");
    printf("Usage:\n  jocky input.cpp -o output.exe [options]\n");
    printf("  jocky input.ll  -o output.exe [options]\n\n");
    printf("Passes (-passes=...):\n");
    printf("  fla sub bcf gvenc mba indcall indbr\n\n");
    printf("Options:\n");
    printf("  -no-poly                Skip polymorphic transform\n");
    printf("  -no-spoof               Skip PE header spoofing\n");
    printf("  -nostdlib               No CRT, use modular JOCKY runtime\n");
    printf("  -entry=<name>           Entry point (default: main)\n");
    printf("  -spoof-template=<path>  Rich Header template\n");
    printf("  -v                      Verbose\n\n");
}

static int parse_args(int argc, char* argv[], JockyArgs* args) {
    memset(args, 0, sizeof(JockyArgs));
    strcpy(args->passes, "fla,sub,api-hash");  // gvenc removed: encrypts globals → .rdata entropy spike
    strcpy(args->entry, "main");
    args->use_poly = 1;
    args->use_spoof = 1;

    if (argc < 3) return 0;
    strncpy(args->input_cpp, argv[1], MAX_PATH_LEN-1);

    int found_output = 0;
    for (int i = 2; i < argc; i++) {
        char* arg = argv[i];
        if (strcmp(arg, "-o") == 0 && i+1 < argc) {
            strncpy(args->output_exe, argv[++i], MAX_PATH_LEN-1);
            found_output = 1;
        }
        else if (strncmp(arg, "-passes=", 8) == 0) strncpy(args->passes, arg+8, 255);
        else if (strncmp(arg, "-entry=", 7) == 0) strncpy(args->entry, arg+7, 63);
        else if (strncmp(arg, "-spoof-template=", 16) == 0) strncpy(args->spoof_template, arg+16, MAX_PATH_LEN-1);
        else if (strcmp(arg, "-no-poly") == 0)  args->use_poly = 0;
        else if (strcmp(arg, "-no-spoof") == 0) args->use_spoof = 0;
        else if (strcmp(arg, "-nostdlib") == 0) args->use_nostdlib = 1;
        else if (strcmp(arg, "-v") == 0)        args->verbose = 1;
        else if (arg[0] == '-') { printf("[-] Unknown: %s\n", arg); return 0; }
    }
    if (!found_output) { printf("[-] Missing -o\n"); return 0; }
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || !strcmp(argv[1],"--help") || !strcmp(argv[1],"-h")) {
        print_usage(); return 0;
    }

    JockyArgs args;
    if (!parse_args(argc, argv, &args)) { print_usage(); return 1; }

    int is_ll  = has_extension(args.input_cpp, ".ll");
    int is_jky = has_extension(args.input_cpp, ".jky");
    if (is_ll) {
        args.use_poly = 0;
        if (strcmp(args.entry, "main") == 0)
            strcpy(args.entry, "jocky_entry");
        if (!args.use_nostdlib)
            args.use_nostdlib = 1;
    }
    if (is_jky) {
        if (strcmp(args.entry, "main") == 0)
            strcpy(args.entry, "jocky_entry");
        if (!args.use_nostdlib)
            args.use_nostdlib = 1;
    }

    char exe_dir[MAX_PATH_LEN];
    get_exe_dir(exe_dir, sizeof(exe_dir));

    char tmp[MAX_PATH_LEN];
    char polaris_clang[MAX_PATH_LEN];
    char poly_engine[MAX_PATH_LEN];
    char jocky_frontend[MAX_PATH_LEN];
    char pe_spoofer[MAX_PATH_LEN];
    char default_template[MAX_PATH_LEN];

    // Modular Runtime Paths
    char core_obj[MAX_PATH_LEN];
    char crypto_obj[MAX_PATH_LEN];
    char memory_obj[MAX_PATH_LEN];
    char process_obj[MAX_PATH_LEN];
    char dummy_imports_obj[MAX_PATH_LEN];
    char entropy_pad_obj[MAX_PATH_LEN];
    char syscall_obj[MAX_PATH_LEN];
    char tls_obj[MAX_PATH_LEN];

    snprintf(tmp, sizeof(tmp), "%s\\..\\..\\build\\Release\\bin\\clang.exe", exe_dir);
    resolve_path(tmp, polaris_clang, sizeof(polaris_clang));

    snprintf(tmp, sizeof(tmp), "%s\\..\\polymorphic\\poly_engine.py", exe_dir);
    resolve_path(tmp, poly_engine, sizeof(poly_engine));

    // Search for the JOCKY DSL frontend in common relative locations
    {
        const char* candidates[] = {
            "%s\\..\\..\\jocky_v1\\build\\Release\\jocky.exe",          // JOCKY-TSAR layout
            "%s\\..\\..\\..\\..\\jocky_v1\\build\\Release\\jocky.exe",  // TSAR layout
            NULL
        };
        jocky_frontend[0] = '\0';
        for (int ci = 0; candidates[ci]; ci++) {
            snprintf(tmp, sizeof(tmp), candidates[ci], exe_dir);
            resolve_path(tmp, jocky_frontend, sizeof(jocky_frontend));
            if (file_exists(jocky_frontend)) break;
            jocky_frontend[0] = '\0';
        }
    }

    snprintf(tmp, sizeof(tmp), "%s\\..\\tools\\pe_header_spoofer.py", exe_dir);
    resolve_path(tmp, pe_spoofer, sizeof(pe_spoofer));

    snprintf(tmp, sizeof(tmp), "%s\\..\\..\\config\\msvc_rich_template.bin", exe_dir);
    resolve_path(tmp, default_template, sizeof(default_template));

    // Resolve individual modular runtime object paths
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\core.obj", exe_dir);
    resolve_path(tmp, core_obj, sizeof(core_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\crypto.obj", exe_dir);
    resolve_path(tmp, crypto_obj, sizeof(crypto_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\memory.obj", exe_dir);
    resolve_path(tmp, memory_obj, sizeof(memory_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\process.obj", exe_dir);
    resolve_path(tmp, process_obj, sizeof(process_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\dummy_imports.obj", exe_dir);
    resolve_path(tmp, dummy_imports_obj, sizeof(dummy_imports_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\entropy_pad.obj", exe_dir);
    resolve_path(tmp, entropy_pad_obj, sizeof(entropy_pad_obj));
    char strings_obj[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\strings.obj", exe_dir);
    resolve_path(tmp, strings_obj, sizeof(strings_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\syscall.obj", exe_dir);
    resolve_path(tmp, syscall_obj, sizeof(syscall_obj));
    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\tls.obj", exe_dir);
    resolve_path(tmp, tls_obj, sizeof(tls_obj));

    const char* kernel32_lib = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\kernel32.lib";
    const char* ntdll_lib    = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\ntdll.lib";
    const char* advapi32_lib = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\advapi32.lib";
    const char* crypt32_lib  = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\crypt32.lib";
    const char* ucrt_lib     = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\ucrt\\x64\\ucrt.lib";
    const char* libcmt       = "C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\VC\\Tools\\MSVC\\14.51.36231\\lib\\x64\\msvcrt.lib";
    const char* libvcruntime = "C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\VC\\Tools\\MSVC\\14.51.36231\\lib\\x64\\vcruntime.lib";
    const char* user32_lib   = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\user32.lib";
    const char* shell32_lib  = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\shell32.lib";

    if (!file_exists(polaris_clang)) {
        printf("[!] Polaris clang not found, falling back to system clang\n");
        strcpy(polaris_clang, "clang");
    }

    char input_abs[MAX_PATH_LEN];
    resolve_path(args.input_cpp, input_abs, sizeof(input_abs));

    char input_dir[MAX_PATH_LEN];
    strncpy(input_dir, input_abs, sizeof(input_dir)-1);
    char* sep = input_dir + strlen(input_dir);
    while (sep > input_dir && *sep != '\\' && *sep != '/') sep--;
    *sep = '\0';

    const char* fname = input_abs + strlen(input_dir) + 1;
    char base_name[256];
    strncpy(base_name, fname, sizeof(base_name)-1);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    char poly_cpp[MAX_PATH_LEN];
    char obj_file[MAX_PATH_LEN];
    char raw_exe[MAX_PATH_LEN];

    if (is_jky)
        snprintf(poly_cpp, sizeof(poly_cpp), "%s\\%s_poly.jky", input_dir, base_name);
    else
        snprintf(poly_cpp, sizeof(poly_cpp), "%s\\%s_poly.cpp", input_dir, base_name);
    snprintf(obj_file, sizeof(obj_file), "%s\\%s_temp.obj", input_dir, base_name);
    snprintf(raw_exe,  sizeof(raw_exe),  "%s\\%s_raw.exe",  input_dir, base_name);

    char cmd[MAX_CMD_LEN];
    int ret;

    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║    JOCKY Compiler Framework (Modular)        ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Input:    %-33s ║\n", args.input_cpp);
    printf("║  Output:   %-33s ║\n", args.output_exe);
    printf("║  Passes:   %-33s ║\n", args.passes);
    printf("║  Nostdlib: %-33s ║\n", args.use_nostdlib ? "yes (modular runtime)" : "no");
    printf("╚══════════════════════════════════════════════╝\n\n");

    // Stage 3: Polymorphic transform
    const char* compile_src = input_abs;
    if (args.use_poly) {
        printf("[*] Stage 3: Polymorphic source transform...\n");
        if (file_exists(poly_engine)) {
            snprintf(cmd, sizeof(cmd), "python \"%s\" \"%s\" \"%s\"", poly_engine, input_abs, poly_cpp);
            ret = run_cmd(cmd, args.verbose);
            if (ret == 0) {
                compile_src = poly_cpp;
                printf("[+] Transformed: %s\n\n", poly_cpp);
            }
        }
    }

    // Stage 3B: JOCKY DSL frontend — .jky → .ll
    static char jky_ll[MAX_PATH_LEN];
    int compile_as_ll = is_ll;
    if (has_extension(compile_src, ".jky")) {
        printf("[*] Stage 3B: JOCKY DSL frontend (.jky -> .ll)...\n");
        snprintf(jky_ll, sizeof(jky_ll), "%s\\%s_jky.ll", input_dir, base_name);
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" -o \"%s\"", jocky_frontend, compile_src, jky_ll);
        if (run_cmd(cmd, args.verbose) != 0) goto fail;
        compile_src   = jky_ll;
        compile_as_ll = 1;
        printf("[+] IR: %s\n\n", jky_ll);
    }

    // Stage 1A+2: Compile with Polaris passes
    printf("[*] Stage 1A+2: Compiling [passes=%s]...\n", args.passes[0] ? args.passes : "none");
    {
        char passes_arg[320] = "";
        if (args.passes[0])
            snprintf(passes_arg, sizeof(passes_arg), "-mllvm \"-passes=%s\" ", args.passes);
        if (compile_as_ll) {
            snprintf(cmd, sizeof(cmd), "\"%s\" -c \"%s\" -o \"%s\" %s-target x86_64-pc-windows-msvc",
                polaris_clang, compile_src, obj_file, passes_arg);
        } else {
            snprintf(cmd, sizeof(cmd), "\"%s\" -c \"%s\" -o \"%s\" %s-target x86_64-pc-windows-msvc -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
                polaris_clang, compile_src, obj_file, passes_arg);
        }
    }

    if (run_cmd(cmd, args.verbose) != 0) goto fail;
    printf("[+] Object: %s\n\n", obj_file);

    // Stage 1B: Modular Link Selection
    printf("[*] Stage 1B: Analyzing symbols and selective linking...\n");

    if (args.use_nostdlib) {
        // Build base link command with core runtime
        static char link_cmd[MAX_CMD_LEN];
        link_cmd[0] = '\0';
        const char* ws2_lib   = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\ws2_32.lib";
        const char* ole32_lib = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\ole32.lib";

#define LNK(s)        lnk_append(link_cmd, sizeof(link_cmd), s)
#define LNKQ(s)       LNK("\""); LNK(s); LNK("\" ")
#define LNKF(fmt,...) do { char _t[MAX_PATH_LEN]; snprintf(_t,sizeof(_t),fmt,__VA_ARGS__); LNK(_t); } while(0)

        LNKQ(polaris_clang); LNK("-fuse-ld=lld -nostdlib ");
        LNKF("-Wl,-entry:%s ", args.entry);
        LNK("-Wl,-subsystem:console -target x86_64-pc-windows-msvc ");
        LNK("-Wl,-dynamicbase -Wl,-nxcompat ");
        LNKF("-o \"%s\" ", raw_exe); LNKQ(obj_file);
        LNKQ(core_obj); LNKQ(syscall_obj);

        // Conditionally link additional modules based on symbol usage in source/object
        if (file_contains_string(compile_src, "jocky_aes_decrypt") || file_contains_string(compile_src, "jocky_xor_buf")) {
            LNKQ(crypto_obj);
            printf("[+] Linking module: crypto.obj\n");
        }
        if (file_contains_string(compile_src, "jocky_alloc_ex") || file_contains_string(compile_src, "jocky_read_proc") || file_contains_string(compile_src, "jocky_write_proc")) {
            LNKQ(memory_obj);
            printf("[+] Linking module: memory.obj\n");
        }
        if (file_contains_string(compile_src, "jocky_create_proc") || file_contains_string(compile_src, "jocky_suspend_thread") || file_contains_string(compile_src, "jocky_resume_thread")) {
            LNKQ(process_obj);
            printf("[+] Linking module: process.obj\n");
        }

        LNKQ(dummy_imports_obj); LNKQ(entropy_pad_obj);
        if (file_exists(strings_obj)) LNKQ(strings_obj);
        LNKQ(kernel32_lib); LNKQ(ntdll_lib);
        LNKQ(advapi32_lib); LNKQ(crypt32_lib);
        LNKQ(user32_lib);   LNKQ(shell32_lib);
        LNKQ(ws2_lib);      LNKQ(ole32_lib);

#undef LNK
#undef LNKQ
#undef LNKF

        if (run_cmd(link_cmd, args.verbose) != 0) goto fail;
    } else {
        snprintf(cmd, sizeof(cmd), "clang -fuse-ld=lld -target x86_64-pc-windows-msvc -o \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
            raw_exe, obj_file, ucrt_lib, kernel32_lib, libcmt, libvcruntime, user32_lib);
        if (run_cmd(cmd, args.verbose) != 0) goto fail;
    }
    printf("[+] Linked successfully: %s\n\n", raw_exe);

    // Stage 6: PE Header Spoofing
    if (args.use_spoof) {
        printf("[*] Stage 6: PE header spoofing...\n");
        if (file_exists(pe_spoofer)) {
            char template_path[MAX_PATH_LEN] = "";
            if (args.spoof_template[0]) {
                strncpy(template_path, args.spoof_template, MAX_PATH_LEN-1);
            } else if (file_exists(default_template)) {
                strncpy(template_path, default_template, MAX_PATH_LEN-1);
            }
            if (template_path[0]) {
                snprintf(cmd, sizeof(cmd), "python \"%s\" \"%s\" \"%s\" \"%s\"", pe_spoofer, raw_exe, args.output_exe, template_path);
            } else {
                snprintf(cmd, sizeof(cmd), "python \"%s\" \"%s\" \"%s\"", pe_spoofer, raw_exe, args.output_exe);
            }
            ret = run_cmd(cmd, args.verbose);
            if (ret != 0) {
                rename(raw_exe, args.output_exe);
            } else {
                remove(raw_exe);
                printf("[+] Spoofed: %s\n\n", args.output_exe);
            }
        } else {
            rename(raw_exe, args.output_exe);
        }
    } else {
        rename(raw_exe, args.output_exe);
    }

    if (compile_src == poly_cpp) remove(poly_cpp);
    if (compile_src == jky_ll)   remove(jky_ll);
    remove(obj_file);

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║  Build complete: %-29s ║\n", args.output_exe);
    printf("╚══════════════════════════════════════════════╝\n\n");
    return 0;

fail:
    if (compile_src == poly_cpp) remove(poly_cpp);
    if (compile_src == jky_ll)   remove(jky_ll);
    remove(obj_file);
    remove(raw_exe);
    printf("\n[-] Build failed\n");
    return 1;
}