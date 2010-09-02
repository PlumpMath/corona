// A client to hammer our event loop servers.

#include <assert.h>
#include <ev.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

char buf[1 << 20];

typedef struct write_watcher_s {
    struct ev_io ww_ev_io;
    int ww_sock;
    size_t ww_off;
} write_watcher_t;

static void write_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    write_watcher_t *ww = (write_watcher_t*) ew;
    int nbytes;

    if (!(revents & EV_WRITE)) {
        return;
    }

    while ((nbytes = write(ww->ww_sock, buf + ww->ww_off, sizeof(buf) - ww->ww_off)) > 0) {
        ww->ww_off += nbytes;
    }

    assert((nbytes == 0) == (ww->ww_off == sizeof(buf)));

    if (nbytes == 0 ||
        (nbytes < 0 && errno != EAGAIN)) {
        if (nbytes < 0) {
            perror("write");
        }

        ev_io_stop(el, &ww->ww_ev_io);
        close(ww->ww_sock);
        free(ww);
    }
}

static void periodic_watcher_cb(struct ev_loop *el, struct ev_periodic *ep, int revents) {
    int sock;
    struct sockaddr_in addr;
    write_watcher_t *ww;

    if ((sock = socket(PF_INET, SOCK_STREAM, 6 /* TCP */)) < 0) {
        perror("socket");
        return;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
        perror("fcntl(O_NONBLOCK)");
        close(sock);
        return;
    }

    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2001);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == -1 &&
        errno != EINPROGRESS) {
        perror("connect");
        return;
    }

    ww = (write_watcher_t*) malloc(sizeof(*ww));

    ww->ww_sock = sock;
    ww->ww_off = 0;
    ev_init(&ww->ww_ev_io, write_watcher_cb);
    ev_io_set(&ww->ww_ev_io, sock, EV_WRITE);
    ev_io_start(el, &ww->ww_ev_io);
}

int main(int argc, char **argv) {
    struct ev_loop *el;
    struct ev_periodic ep;

    el = ev_default_loop(EVFLAG_AUTO);

    ev_periodic_init(&ep, periodic_watcher_cb, 0, 0.001, NULL);
    ev_periodic_start(el, &ep);

    ev_loop(el, 0);

    ev_default_destroy();
    return 0;
}
