#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_PATH_LEN 1024
#define MAX_CMD_LEN  8192


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
    // Windows cmd.exe requires wrapping when command starts with quoted path
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


static void print_usage() {
    printf("\nJOCKY Compiler Framework\n========================\n\n");
    printf("Usage:\n  jocky input.cpp -o output.exe [options]\n\n");
    printf("Passes (-passes=...):\n");
    printf("  fla sub bcf gvenc mba indcall indbr\n\n");
    printf("Options:\n");
    printf("  -no-poly               Skip polymorphic transform\n");
    printf("  -no-spoof              Skip PE header spoofing\n");
    printf("  -nostdlib              No CRT, use JOCKY runtime\n");
    printf("  -entry=<name>          Entry point (default: main)\n");
    printf("  -spoof-template=<path> Rich Header template\n");
    printf("  -v                     Verbose\n\n");
    printf("Note: On PowerShell quote passes: \"-passes=fla,sub,gvenc\"\n\n");
}


static int parse_args(int argc, char* argv[], JockyArgs* args) {
    memset(args, 0, sizeof(JockyArgs));
    strcpy(args->passes, "fla,sub,gvenc,api-hash");
    strcpy(args->entry, "main");
    args->use_poly = 1;
    args->use_spoof = 1;

    if (argc < 4) return 0;
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

    // ── Resolve all tool paths ─────────────────────────────────
    char exe_dir[MAX_PATH_LEN];
    get_exe_dir(exe_dir, sizeof(exe_dir));

    char tmp[MAX_PATH_LEN];
    char polaris_clang[MAX_PATH_LEN];
    char poly_engine[MAX_PATH_LEN];
    char pe_spoofer[MAX_PATH_LEN];
    char runtime_obj[MAX_PATH_LEN];
    char default_template[MAX_PATH_LEN];

    // jocky.exe is at JOCKY-TSAR\jocky\driver\jocky.exe
    // so exe_dir = JOCKY-TSAR\jocky\driver
    // ..\      = JOCKY-TSAR\jocky\
    // ..\..\ = JOCKY-TSAR\

    snprintf(tmp, sizeof(tmp), "%s\\..\\..\\build\\Release\\bin\\clang.exe", exe_dir);
    resolve_path(tmp, polaris_clang, sizeof(polaris_clang));

    snprintf(tmp, sizeof(tmp), "%s\\..\\polymorphic\\poly_engine.py", exe_dir);
    resolve_path(tmp, poly_engine, sizeof(poly_engine));

    snprintf(tmp, sizeof(tmp), "%s\\..\\tools\\pe_header_spoofer.py", exe_dir);
    resolve_path(tmp, pe_spoofer, sizeof(pe_spoofer));

    snprintf(tmp, sizeof(tmp), "%s\\..\\runtime\\runtime.obj", exe_dir);
    resolve_path(tmp, runtime_obj, sizeof(runtime_obj));

    snprintf(tmp, sizeof(tmp), "%s\\..\\..\\config\\msvc_rich_template.bin", exe_dir);
    resolve_path(tmp, default_template, sizeof(default_template));

    const char* kernel32_lib =
        "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\kernel32.lib";
    const char* ucrt_lib =
        "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\ucrt\\x64\\ucrt.lib";
    const char* libcmt =
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\18"
        "\\BuildTools\\VC\\Tools\\MSVC\\14.51.36231\\lib\\x64\\msvcrt.lib";
    const char* libvcruntime =
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\18"
        "\\BuildTools\\VC\\Tools\\MSVC\\14.51.36231\\lib\\x64\\vcruntime.lib";

    if (!file_exists(polaris_clang)) {
        printf("[!] Polaris clang not found: %s\n", polaris_clang);
        printf("[!] Falling back to system clang\n");
        strcpy(polaris_clang, "clang");
    } else {
        printf("[+] Polaris clang: %s\n", polaris_clang);
    }


    // ── Derive temp file paths ─────────────────────────────────
    // All temp files go in the same dir as the input file
    char input_abs[MAX_PATH_LEN];
    resolve_path(args.input_cpp, input_abs, sizeof(input_abs));

    // Get input directory
    char input_dir[MAX_PATH_LEN];
    strncpy(input_dir, input_abs, sizeof(input_dir)-1);
    char* sep = input_dir + strlen(input_dir);
    while (sep > input_dir && *sep != '\\' && *sep != '/') sep--;
    *sep = '\0';

    // Get base filename without extension
    const char* fname = input_abs + strlen(input_dir) + 1;
    char base_name[256];
    strncpy(base_name, fname, sizeof(base_name)-1);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    char poly_cpp[MAX_PATH_LEN];
    char obj_file[MAX_PATH_LEN];
    char raw_exe[MAX_PATH_LEN];

    snprintf(poly_cpp, sizeof(poly_cpp), "%s\\%s_poly.cpp", input_dir, base_name);
    snprintf(obj_file, sizeof(obj_file), "%s\\%s_temp.obj", input_dir, base_name);
    snprintf(raw_exe,  sizeof(raw_exe),  "%s\\%s_raw.exe",  input_dir, base_name);

    char cmd[MAX_CMD_LEN];
    int ret;


    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║        JOCKY Compiler Framework              ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Input:    %-35s║\n", args.input_cpp);
    printf("║  Output:   %-35s║\n", args.output_exe);
    printf("║  Passes:   %-35s║\n", args.passes);
    printf("║  Poly:     %-35s║\n", args.use_poly ? "yes" : "no");
    printf("║  Spoof:    %-35s║\n", args.use_spoof ? "yes" : "no");
    printf("║  Nostdlib: %-35s║\n", args.use_nostdlib ? "yes" : "no");
    if (args.use_nostdlib)
        printf("║  Entry:    %-35s║\n", args.entry);
    printf("╚══════════════════════════════════════════════╝\n\n");


    // ── Stage 3: Polymorphic transform ────────────────────────
    const char* compile_src = input_abs;

    if (args.use_poly) {
        printf("[*] Stage 3: Polymorphic source transform...\n");
        if (file_exists(poly_engine)) {
            snprintf(cmd, sizeof(cmd),
                "python \"%s\" \"%s\" \"%s\"",
                poly_engine, input_abs, poly_cpp);
            ret = run_cmd(cmd, args.verbose);
            if (ret == 0) {
                compile_src = poly_cpp;
                printf("[+] Transformed: %s\n\n", poly_cpp);
            } else {
                printf("[!] Poly failed, using original source\n\n");
            }
        } else {
            printf("[!] poly_engine.py not found, skipping\n\n");
        }
    }


    // ── Stage 1A+2: Compile with Polaris passes ───────────────
    printf("[*] Stage 1A+2: Compiling [passes=%s]...\n",
           args.passes[0] ? args.passes : "none");

    // Build compile command — space between -o and path is critical
    if (args.passes[0]) {
        ret = snprintf(cmd, sizeof(cmd),
            "\"%s\" -c \"%s\" -o \"%s\" -mllvm \"-passes=%s\" "
            "-target x86_64-pc-windows-msvc "
            "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
            polaris_clang, compile_src, obj_file, args.passes);
    } else {
        ret = snprintf(cmd, sizeof(cmd),
            "\"%s\" -c \"%s\" -o \"%s\" "
            "-target x86_64-pc-windows-msvc "
            "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
            polaris_clang, compile_src, obj_file);
    }


    if (ret >= MAX_CMD_LEN) {
        printf("[-] Command too long (%d chars). Paths too deep.\n", ret);
        goto fail;
    }


    if (run_cmd(cmd, args.verbose) != 0) goto fail;
    printf("[+] Object: %s\n\n", obj_file);


    // ── Stage 1B: Link ────────────────────────────────────────
    printf("[*] Stage 1B: Linking...\n");

    if (args.use_nostdlib) {
        snprintf(cmd, sizeof(cmd),
            "clang -fuse-ld=lld -nostdlib "
            "-Wl,-entry:%s -Wl,-subsystem:console "
            "-target x86_64-pc-windows-msvc "
            "-o \"%s\" \"%s\" \"%s\" \"%s\"",
            args.entry, raw_exe, obj_file,
            runtime_obj, kernel32_lib);
    } else {
        const char* user32_lib =
            "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\user32.lib";

        snprintf(cmd, sizeof(cmd),
            "clang -fuse-ld=lld "
            "-target x86_64-pc-windows-msvc "
            "-o \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
            raw_exe, obj_file,
            ucrt_lib, kernel32_lib, libcmt, libvcruntime, user32_lib);
    }


    if (run_cmd(cmd, args.verbose) != 0) goto fail;
    printf("[+] Linked: %s\n\n", raw_exe);


    // ── Stage 6: PE Header Spoofing ───────────────────────────
    if (args.use_spoof) {
        printf("[*] Stage 6: PE header spoofing...\n");

        if (file_exists(pe_spoofer)) {
            char template_path[MAX_PATH_LEN] = "";

            if (args.spoof_template[0]) {
                strncpy(template_path, args.spoof_template, MAX_PATH_LEN-1);
            } else if (file_exists(default_template)) {
                strncpy(template_path, default_template, MAX_PATH_LEN-1);
                printf("[+] Template: %s\n", template_path);
            } else {
                printf("[!] No Rich Header template — run extract_rich_header.py first\n");
            }

            if (template_path[0]) {
                snprintf(cmd, sizeof(cmd),
                    "python \"%s\" \"%s\" \"%s\" \"%s\"",
                    pe_spoofer, raw_exe, args.output_exe, template_path);
            } else {
                snprintf(cmd, sizeof(cmd),
                    "python \"%s\" \"%s\" \"%s\"",
                    pe_spoofer, raw_exe, args.output_exe);
            }

            ret = run_cmd(cmd, args.verbose);
            if (ret != 0) {
                printf("[!] Spoofing failed, using raw binary\n");
                rename(raw_exe, args.output_exe);
            } else {
                remove(raw_exe);
                printf("[+] Spoofed: %s\n\n", args.output_exe);
            }
        } else {
            printf("[!] pe_header_spoofer.py not found, skipping\n");
            rename(raw_exe, args.output_exe);
        }
    } else {
        rename(raw_exe, args.output_exe);
    }


    // ── Cleanup ───────────────────────────────────────────────
    if (compile_src == poly_cpp) remove(poly_cpp);
    remove(obj_file);

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║  Build complete: %-29s║\n", args.output_exe);
    printf("╚══════════════════════════════════════════════╝\n\n");
    return 0;

fail:
    if (compile_src == poly_cpp) remove(poly_cpp);
    remove(obj_file);
    remove(raw_exe);
    printf("\n[-] Build failed\n");
    return 1;
}
