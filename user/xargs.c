#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

char *
xgets(char *buf)
{
    int i, cc;
    char c;

    for (i = 0;;){
        cc = read(0, &c, 1);
        if (cc < 1)
            break;
        buf[i++] = c;
        if (c == '\n' || c == '\r')
            break;
    }
    buf[i] = '\0';
    return buf;
}

int main(int argc, char *argv[])
{
    char buf[512], *args[MAXARG + 1], *p;
    int len, argsc;

    if (argc > MAXARG){
        printf("too many arguments\n");
        exit(0);
    }

    for (int i = 1; i < argc; ++i){
        args[i - 1] = argv[i];
    }
    while ((len = strlen(xgets(buf))) != 0){
        argsc = argc - 1;
        p = buf;
        
        for (int i = 0; i < len; ++i){
            if (buf[i] == ' ' || i == len - 1) {
                buf[i] = '\0';
                args[argsc] = p;
                ++argsc;
                p = buf + i + 1;
            }
        }
        args[argsc] = 0;

        if (fork() == 0) {
            exec(argv[1], args);
            exit(0);
        } else {
            wait(0);
        }
    }
    exit(0);
}