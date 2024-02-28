#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main()
{
    uint8 buf[255];
    int p0[2], p1[2];
    pipe(p0);
    pipe(p1);
    if (fork() == 0) {
        close(p0[1]);
        close(p1[0]);
        read(p0[0], buf, 4);
        printf("%d: received ping\n", getpid());
        write(p1[1], "pong", 4);
    } else {
        close(p0[0]);
        close(p1[1]);
        write(p0[1], "ping", 4);
        read(p1[0], buf, 4);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}