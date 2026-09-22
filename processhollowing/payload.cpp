// payload.cpp
// No includes — zero CRT
// This binary will be injected into notepad.exe
#define NULL 0
extern "C" {
    int __stdcall MessageBoxA(
        void* hwnd,
        const char* text,
        const char* caption,
        unsigned int type
    );
    void __stdcall ExitProcess(unsigned int code);
}

// Our entry point — no main, no WinMain, no CRT
void payload_entry() {
    MessageBoxA(
        NULL,
        "JOCKY is executing inside notepad.exe\n"
        "Process Hollowing successful.\n"
        "This code was injected.",
        "JOCKY - Hollowing Demo",
        0x40  // MB_ICONINFORMATION
    );
    ExitProcess(0);
}