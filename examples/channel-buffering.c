#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "../neco.h"


int neco_main(int argc, char *argv[]) {

    // Here we make a channel of strings buffering up to 2 values.
    neco_chan *messages;
    neco_chan_make(&messages, sizeof(char*), 2);

    // Because this channel is buffered, we can send these values into the
    // channel without a corresponding concurrent receive.
    char *msg1 = "buffered";
    neco_chan_send(messages, &msg1);
    char *msg2 = "channel";
    neco_chan_send(messages, &msg2);
    
    // Later we can receive these two values as usual.
    char *msg = NULL;
    neco_chan_recv(messages, &msg);
    printf("%s\n", msg);
    neco_chan_recv(messages, &msg);
    printf("%s\n", msg);

    // This coroutine no longer needs this channel.
    neco_chan_release(messages);

    return 0;
}