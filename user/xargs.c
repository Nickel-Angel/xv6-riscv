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
    char buf[512], *args[MAXARG + 1];
    int len;

    if (argc > MAXARG){
        printf("too many arguments\n");
        exit(0);
    }

    for (int i = 1; i < argc; ++i){
        args[i - 1] = argv[i];
    }
    while ((len = strlen(xgets(buf))) != 0){
        args[argc - 1] = buf;
        args[argc] = 0;
        // for (int i = 0; i <= argc; ++i){
        //     printf("%s ", args[i]);
        // }
        // printf("\n");
        // printf("total exec: %s", args[argc - 1]);
        if (fork() == 0) {
            exec(argv[1], args);
            exit(0);
        } else {
            wait(0);
        }
    }
    exit(0);
}