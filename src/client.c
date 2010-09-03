// A client to hammer our event loop servers.

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ev.h>
#include <string.h>
#include <signal.h>
#include "common.h"

char buf[1 << 20];
struct sockaddr_in addr;
size_t numConns = 1000;
size_t dataSize = 1 << 10;

typedef struct write_watcher_s {
    struct ev_io ww_ev_io;
    int ww_sock;
    size_t ww_off;
} write_watcher_t;

static void sigpipe_cb(int sig) {
    DBG(0, ("ignoring signal %d\n", sig));
}

static void write_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    write_watcher_t *ww = (write_watcher_t*) ew;
    int nbytes;

    if (!(revents & EV_WRITE)) {
        return;
    }

    while ((nbytes = write(ww->ww_sock, buf, dataSize - ww->ww_off)) > 0) {
        DBG(1, ("wrote %d bytes to socket %d\n", nbytes, ww->ww_sock));
        ww->ww_off += nbytes;
    }

    assert((nbytes == 0) == (ww->ww_off == dataSize));

    if (nbytes == 0 ||
        (nbytes < 0 && errno != EAGAIN)) {
        if (nbytes < 0) {
            perror("write");
        }

        DBG(
            0,
            ("closing socket %d with errno %d\n",
                ww->ww_sock, (nbytes < 0) ? errno : 0)
        );

        ev_io_stop(el, &ww->ww_ev_io);
        close(ww->ww_sock);
        free(ww);
    }
}

static void periodic_watcher_cb(struct ev_loop *el, struct ev_periodic *ep, int revents) {
    int sock;
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

    if (--numConns == 0) {
        ev_periodic_stop(el, ep);
    }

    DBG(0, ("created socket %d; %lu connections remaining\n", sock, numConns));
}

void usage(FILE *fp, char *name) {
    fprintf(fp, "usage: %s [options] <host> <port>\n\n", name);
    fprintf(fp, "Run the TCP benchmarking client.\n\n");
    fprintf(fp, "Options:\n");
    fprintf(fp, "  -h                help\n");
    fprintf(fp, "  -n <conns>        total connections to open\n");
    fprintf(fp, "  -r <conns>        connections to open per second\n");
    fprintf(fp, "  -s <bytes>        bytes to send down each connection\n");
    fprintf(fp, "  -v                be verbose; use multiple times for\n");
    fprintf(fp, "                    increased verbosity\n");
}

int main(int argc, char **argv) {
    struct ev_loop *el;
    struct ev_periodic ep;
    int c;

    // Options and defaults
    int connRate = 100;
    struct hostent *hent;

    optreset = optind = 1;
    while ((c = getopt(argc, argv, "r:s:n:hv")) != -1) {
        switch (c) {
        case 'h':
            usage(stdout, argv[0]);
            return 0;

        case 'r':
            connRate = strtoul(optarg, NULL, 10);
            break;

        case 's':
            dataSize = strtoul(optarg, NULL, 10);
            break;

        case 'n':
            numConns = strtoul(optarg, NULL, 10);
            break;

        case 'v':
            dbg_level++;
            break;

        case '?':
            fprintf(stderr, "%s: unknown option %c\n", argv[0], optopt);
            return 1;
        }
    }

    if ((argc - optind) != 2) {
        usage(stderr, argv[0]);
        return 1;
    }

    if (!(hent = gethostbyname(argv[optind]))) {
        fprintf(stderr, "%s: could not resolve host name\n", argv[0]);
        return 1;
    }

    memcpy(&addr.sin_addr, hent->h_addr, sizeof(addr.sin_addr));
    addr.sin_port = htons(strtoul(argv[optind + 1], NULL, 10));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;

    signal(SIGPIPE, sigpipe_cb);

    el = ev_default_loop(EVFLAG_AUTO);

    ev_periodic_init(&ep, periodic_watcher_cb, 0, 1.0f / connRate, NULL);
    ev_periodic_start(el, &ep);

    ev_loop(el, 0);
    ev_default_destroy();

    return 0;
}
