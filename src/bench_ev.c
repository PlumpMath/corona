// Benchmark for straight libev I/O loop.

#include <ev.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

static char buf[1024];

typedef struct accept_watcher_s {
    struct ev_io aw_ev_io;
    int aw_sock;
} accept_watcher_t;

typedef struct read_watcher_s {
    struct ev_io rw_ev_io;
    int rw_sock;
} read_watcher_t;

static void
read_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    read_watcher_t *rw = (read_watcher_t*) ew;
    int nbytes = 0;

    if (!(revents & EV_READ)) {
        return;
    }

    nbytes = read(rw->rw_sock, buf, sizeof(buf));

    if (nbytes < 0) {
        perror("read");
    }

    if (nbytes <= 0) {
        close(rw->rw_sock);
        ev_io_stop(el, &rw->rw_ev_io);
    }
}

static void
accept_watcher_cb(struct ev_loop *el, ev_io *ew, int revents)
{
    accept_watcher_t *aw = (accept_watcher_t*) ew;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int sock;
    read_watcher_t *rw;

    if (!(revents & EV_READ)) {
        return;
    }

    if((sock = accept(aw->aw_sock, (struct sockaddr*) &addr, &addrlen)) < 0) {
        perror("accept");
        return;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK)) {
        perror("fcntl(O_NONBLOCK)");
        close(sock);
    }
    
    rw = (read_watcher_t*) malloc(sizeof(*rw));

    rw->rw_sock = sock;
    ev_init(&rw->rw_ev_io, read_watcher_cb);
    ev_io_set(&rw->rw_ev_io, sock, EV_READ);
    ev_io_start(el, &rw->rw_ev_io);
}

int
main(int argc, char **argv)
{
    int sock;
    struct sockaddr_in addr;
    struct ev_loop *el;
    accept_watcher_t aw;

    // Set up a socket listening on port 2001
    
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
    addr.sin_addr.s_addr = INADDR_ANY;
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

    aw.aw_sock = sock;
    ev_init(&aw.aw_ev_io, accept_watcher_cb);
    ev_io_set(&aw.aw_ev_io, sock, EV_READ);
    ev_io_start(el, &aw.aw_ev_io);

    ev_loop(el, 0);

    ev_default_destroy();
    return 0;
}
