#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "../neco.h"

void coroutine(int argc, void *argv[]) {
    neco_chan *messages = argv[0];
    
    // Send a message of the 'messages' channel. 
    char *msg = "ping";
    neco_chan_send(messages, &msg);

    // This coroutine no longer needs this channel.
    neco_chan_release(messages);
}

int neco_main(int argc, char *argv[]) {

    // Create a new channel that is used to send 'char*' string messages.
    neco_chan *messages;
    neco_chan_make(&messages, sizeof(char*), 0);

    // Start a coroutine that will send messages over the channel. 
    // It's a good idea to use neco_chan_retain on a channel before using it
    // in a new coroutine. This will avoid potential use-after-free bugs.
    neco_chan_retain(messages);
    neco_start(coroutine, 1, messages);

    // Receive the next incoming message. Here we’ll receive the "ping"
    // message we sent above and print it out.
    char *msg = NULL;
    neco_chan_recv(messages, &msg);
    printf("%s\n", msg);
    
    // This coroutine no longer needs the channel.
    neco_chan_release(messages);

    return 0;
}