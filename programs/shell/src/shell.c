#include "shell.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "peachos.h"

char* current_directory = "0:/";

int main(int argc, char** argv) {
    
    size_t o = strlen(current_directory);
    if (o) {}
    print("PeachOS v2.0.0\n");
    printf("Shell ID: %s\n", argv[0]);
    print("Press '~' to switch between shells\n");
    while(1) {
        print(current_directory);
        print("> ");
        char buf[1024];
        peachos_terminal_readline(buf, sizeof(buf), true);
        char* command = buf;

        parsenexec(command);
        print("\n");
        //peachos_system_run(command);
        //print("\n");
    }
    return 0;
}

void parsenexec(char* command) {
    struct command_argument* commands = peachos_parse_command(command, 1024);

    if(istrncmp("cd", commands->argument, 1025) == 0) {
        print("\nChanging directory...");
    }

    if(istrncmp("ls", commands->argument, 1025) == 0) {
        print("\n filename          date            permissions     ");
    }

    if(istrncmp("pwd", commands->argument, 1025) == 0) {
        print("\n");
        print(current_directory);
    }

    if(istrncmp("-h", commands->argument, 1025) == 0) {
        print("\nls - list directory contents\n");
        print("pwd - print current working directory\n");
        print("cd - change current directory to the given directory\n");
    }
}