#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
sieve(int frpipe) {
    int p[2];
    int prime, pre, readstatus, forked = 0;
    /* 
    the first number we recieve must be a prime.
    we guarantee that once a process enters this function,
    the process will recieve at least one number from the pipe.
    */
    readstatus = read(frpipe, &prime, 4);
    printf("prime: %d\n", prime);
    for (;;) {
        readstatus = read(frpipe, &pre, 4);
        if (readstatus == 0) {
            close(frpipe);
            if (forked) {
                close(p[1]);
            }
            wait(0);
            return;
        }
        if (pre % prime == 0) {
            continue;
        }
        // pre % prime != 0
        if (!forked) {
            forked = 1;
            pipe(p);
            if (fork() == 0) {
                close(p[1]);
                close(frpipe);
                sieve(p[0]);
                exit(0);
            }
            close(p[0]);
        }
        write(p[1], &pre, 4);
    }
}

int
main()
{
    int p[2];
    pipe(p);
    for (int i = 2; i <= 35; ++i) {
        write(p[1], &i, 4);
    }
    close(p[1]);
    sieve(p[0]);
    exit(0);
}

/*
2 -> 3
     5
*/