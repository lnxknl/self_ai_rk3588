#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_env_var(const char* var_name) {
    char* value = getenv(var_name);
    printf("%s=%s\n", var_name, value ? value : "not set");
}

int main() {
    printf("X11 Display Environment Check:\n");
    printf("============================\n\n");

    // Check common X11 environment variables
    print_env_var("DISPLAY");
    print_env_var("XAUTHORITY");
    print_env_var("XDG_SESSION_TYPE");
    print_env_var("WAYLAND_DISPLAY");
    print_env_var("XDG_RUNTIME_DIR");
    
    // Check session information
    printf("\nSession Information:\n");
    system("who");
    
    // Check if X server is running
    printf("\nX Server Status:\n");
    system("ps aux | grep X");
    
    // Check XRDP session
    printf("\nXRDP Session Status:\n");
    system("ps aux | grep xrdp");
    
    return 0;
}
