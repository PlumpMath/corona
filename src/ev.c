// Benchmark for straight libev I/O loop.

#include <ev.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

static char buf[(1 << 20)];

typedef struct sock_watcher_s {
    struct ev_io sw_ev_io;
    int sw_sock;
} sock_watcher_t;

static void read_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    sock_watcher_t *sw = (sock_watcher_t*) ew;
    int nbytes = 0;

    if (!(revents & EV_READ)) {
        return;
    }

    nbytes = read(sw->sw_sock, buf, sizeof(buf));

    if (nbytes < 0) {
        perror("read");
    }

    if (nbytes <= 0) {
        ev_io_stop(el, &sw->sw_ev_io);
        close(sw->sw_sock);
    }
}

static void accept_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    sock_watcher_t *sw = (sock_watcher_t*) ew;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int sock;
    sock_watcher_t *sw2;

    if (!(revents & EV_READ)) {
        return;
    }

    if((sock = accept(sw->sw_sock, (struct sockaddr*) &addr, &addrlen)) < 0) {
        perror("accept");
        return;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
        perror("fcntl(O_NONBLOCK)");
        close(sock);
    }
    
    sw2 = (sock_watcher_t*) malloc(sizeof(*sw2));

    sw2->sw_sock = sock;
    ev_init(&sw2->sw_ev_io, read_watcher_cb);
    ev_io_set(&sw2->sw_ev_io, sock, EV_READ);
    ev_io_start(el, &sw2->sw_ev_io);
}

int main(int argc, char **argv) {
    int sock;
    struct sockaddr_in addr;
    struct ev_loop *el;
    sock_watcher_t sw;

    // Set up a socket listening on localhost:2001
    
    if ((sock = socket(PF_INET, SOCK_STREAM, 6 /* TCP */)) < 0) {
        perror("socket");
        return 1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
        perror("fcntl(O_NONBLOCK)");
        close(sock);
        return 1;
    }

    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2001);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sock, (struct sockaddr*) &addr, sizeof(addr))) {
        perror("bind");
        close(sock);
        return 1;
    }

    if (listen(sock, 64)) {
        perror("listen");
        close(sock);
        return 1;
    }

    // Fire up the event loop

    el = ev_default_loop(EVFLAG_AUTO);

    sw.sw_sock = sock;
    ev_init(&sw.sw_ev_io, accept_watcher_cb);
    ev_io_set(&sw.sw_ev_io, sock, EV_READ);
    ev_io_start(el, &sw.sw_ev_io);

    ev_loop(el, 0);

    ev_default_destroy();
    return 0;
}
