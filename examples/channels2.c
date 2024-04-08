#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "../neco.h"

void sum(int argc, void *argv[]) {
    int *s = argv[0];
    int n = *(int*)argv[1];
    neco_chan *c = argv[2];

    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += s[i];
    }

    neco_chan_send(c, &sum);
    neco_chan_release(c);
}

int neco_main(int argc, char *argv[]) {
    int s[] = {7, 2, 8, -9, 4, 0};
    int n = sizeof(s)/sizeof(int);

    neco_chan *c;
    neco_chan_make(&c, sizeof(int), 0);
    
    neco_chan_retain(c);
    neco_start(sum, 3, &s[0], &(int){n/2}, c);

    neco_chan_retain(c);
    neco_start(sum, 3, &s[n/2], &(int){n/2}, c);

    int x, y;
    neco_chan_recv(c, &x);
    neco_chan_recv(c, &y);
    
    printf("%d %d %d\n", x, y, x+y);
    neco_chan_release(c);
    return 0;
}
