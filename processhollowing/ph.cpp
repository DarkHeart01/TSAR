#include <Windows.h>
#include <stdio.h>

int main(int argc, wchar_t* argv[])
{
    // Check if the correct number of arguments is provided
    if (argc < 3) {
        printf("Usage: Hollowish <original> <replacement>\n");
        return 0;
    }

    // Display the message passed as an argument
    PROCESS_INFORMATION pi;
    STARTUPINFO si{sizeof(si)};

    if(!CreateProcessW(nullptr, argv[1], nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        return 1;
    }

    HANDLE hFile = CreateFileW(argv[2], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 1;
    }


    
    return 0;
}