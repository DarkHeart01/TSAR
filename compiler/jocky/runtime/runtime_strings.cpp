// runtime_strings.cpp — Benign string table compiled naturally into .rdata
// Purpose: shift .rdata string distribution toward legitimate app profile.
// These are generic Windows application strings — no IOC-listed content.
// Being compiled in (not appended as a PE section) they look identical to
// strings in any real Windows desktop application.

// Suppresses "unused variable" warnings without a CRT header
#ifdef _MSC_VER
#pragma warning(disable: 4505)
#endif

static const char* const jky_err_table[] = {
    "Failed to initialize application resources.",
    "Unable to open configuration file.",
    "Access denied: insufficient privileges.",
    "The requested operation could not be completed.",
    "One or more required components are missing.",
    "Registry key could not be opened.",
    "File not found in search path.",
    "Memory allocation failed.",
    "Unexpected end of data stream.",
    "Network connection timed out.",
    "Invalid parameter passed to function.",
    "The specified module could not be found.",
    "Operation cancelled by user.",
    "Disk quota exceeded.",
    "The process cannot access the file because it is in use.",
    "An unknown error occurred. Please try again.",
    "Version mismatch detected. Please reinstall.",
    "License verification failed.",
    "Unable to write to the output directory.",
    "Checksum verification failed.",
};

static const char* const jky_path_table[] = {
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
    "SOFTWARE\\Classes\\",
    "SYSTEM\\CurrentControlSet\\Services\\",
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
    "%APPDATA%\\Roaming\\",
    "%LOCALAPPDATA%\\Temp\\",
    "%PROGRAMFILES%\\Common Files\\",
    "%WINDIR%\\System32\\",
    "%WINDIR%\\SysWOW64\\",
    "C:\\Users\\Public\\Documents\\",
    "C:\\ProgramData\\",
};

static const char* const jky_ui_table[] = {
    "OK",
    "Cancel",
    "Apply",
    "Close",
    "Yes",
    "No",
    "Retry",
    "Ignore",
    "Help",
    "About",
    "Settings",
    "Preferences",
    "File",
    "Edit",
    "View",
    "Tools",
    "Window",
    "Save",
    "Open",
    "Exit",
    "Print",
    "Copy",
    "Paste",
    "Undo",
    "Redo",
    "Find",
    "Replace",
    "Select All",
    "Properties",
    "Options",
};

static const char* const jky_log_table[] = {
    "[INFO]  Application started successfully.",
    "[INFO]  Loading user configuration.",
    "[WARN]  Default settings applied.",
    "[INFO]  Connecting to local service endpoint.",
    "[INFO]  Enumerating installed components.",
    "[WARN]  Optional feature not available.",
    "[INFO]  Applying configuration changes.",
    "[INFO]  Flushing write buffers.",
    "[INFO]  Releasing allocated resources.",
    "[INFO]  Application shutdown complete.",
};

static const char* const jky_ext_table[] = {
    ".cfg", ".ini", ".log", ".tmp", ".dat",
    ".xml", ".json", ".txt", ".csv", ".bak",
};

// Referenced by jocky_entry so the linker keeps this translation unit.
extern "C" const char* jky_get_string(int table, int idx) {
    switch (table) {
        case 0: if (idx >= 0 && idx < 20) return jky_err_table[idx];  break;
        case 1: if (idx >= 0 && idx < 12) return jky_path_table[idx]; break;
        case 2: if (idx >= 0 && idx < 30) return jky_ui_table[idx];   break;
        case 3: if (idx >= 0 && idx < 10) return jky_log_table[idx];  break;
        case 4: if (idx >= 0 && idx < 10) return jky_ext_table[idx];  break;
    }
    return (const char*)0;
}
