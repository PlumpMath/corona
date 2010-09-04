// A client to hammer our event loop servers.

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
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

#define MAX_TICK_FREQUENCY                          1000

typedef struct write_watcher_s {
    struct ev_io ww_ev_io;
    int ww_sock;
    size_t ww_off;
} write_watcher_t;

char buf[1 << 20];
struct sockaddr_in addr;
size_t connRate = 100;
size_t numConns = 1000;
size_t dataSize = 1 << 10;
size_t conns_cnt = 0;

// stats
errno_cnt_t socket_errors_cnt;
errno_cnt_t fcntl_errors_cnt;
errno_cnt_t connect_errors_cnt;
errno_cnt_t write_errors_cnt;

static void sigpipe_cb(int sig) {
    DBG(0, ("ignoring signal %d\n", sig));
}

static void write_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    write_watcher_t *ww = (write_watcher_t*) ew;
    int nbytes;

    if (!(revents & EV_WRITE)) {
        return;
    }

    do {
        nbytes = write(ww->ww_sock, buf, MIN(sizeof(buf), dataSize - ww->ww_off));
        errno_cnt_incr(&write_errors_cnt, nbytes);
        DBG(1, ("wrote %d bytes to socket %d\n", nbytes, ww->ww_sock));
        if (nbytes > 0) {
            ww->ww_off += nbytes;
        }
    } while (nbytes > 0);

    assert((nbytes == 0) == (ww->ww_off == dataSize));

    if (nbytes == 0 ||
        (nbytes < 0 && errno != EAGAIN)) {
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

static void spawn_connection(struct ev_loop *el) {
    int sock;
    int err;
    write_watcher_t *ww;

    sock = socket(PF_INET, SOCK_STREAM, 6 /* TCP */);
    errno_cnt_incr(&socket_errors_cnt, sock);
    if (sock < 0) {
        return;
    }

    err = fcntl(sock, F_SETFL, O_NONBLOCK);
    errno_cnt_incr(&fcntl_errors_cnt, err);
    if (err < 0) {
        close(sock);
        return;
    }

    err = connect(sock, (struct sockaddr*) &addr, sizeof(addr));
    errno_cnt_incr(&connect_errors_cnt, err);
    if (err < 0 && errno != EINPROGRESS) {
        close(sock);
        return;
    }

    ww = (write_watcher_t*) malloc(sizeof(*ww));

    ww->ww_sock = sock;
    ww->ww_off = 0;
    ev_init(&ww->ww_ev_io, write_watcher_cb);
    ev_io_set(&ww->ww_ev_io, sock, EV_WRITE);
    ev_io_start(el, &ww->ww_ev_io);

    DBG(
        0,
        ("created socket %d; %lu connections remaining\n",
            sock, numConns - conns_cnt)
    );
}

static void periodic_watcher_cb(struct ev_loop *el, struct ev_periodic *ep, int revents) {
    static float conns_per_tick = 0.0f;
    static float conns_remainder = 0.0f;

    if (conns_per_tick == 0) {
        conns_per_tick = (connRate <= MAX_TICK_FREQUENCY) ?
            1.0f :
            (connRate / MAX_TICK_FREQUENCY);
        DBG(0, ("making %f connections per tick\n", conns_per_tick));
    }

    for (conns_remainder += conns_per_tick;
         conns_remainder >= 1.0f && numConns > conns_cnt;
         conns_remainder--, conns_cnt++) {
        spawn_connection(el);
    }

    if (numConns <= conns_cnt) {
        ev_periodic_stop(el, ep);
    }
}

void usage(FILE *fp, char *name) {
    fprintf(fp,
"usage: %s [options] <host> <port>\n\n", name);
    fprintf(fp,
"Run the TCP benchmarking client.\n\n");
    fprintf(fp,
"Options:\n");
    fprintf(fp,
"  -h                help\n");
    fprintf(fp,
"  -n <conns>        total connections to open (default: %lu)\n", numConns);
    fprintf(fp,
"  -r <conns>        connections to open per second; a value of 0 will cause\n"
"                    all connections to be opened before any data is sent\n"
"                    (default: %lu)\n", connRate);
    fprintf(fp,
"  -s <bytes>        bytes to send down each connection (default: %lu)\n",
dataSize);
    fprintf(fp,
"  -v                be verbose; use multiple times for increased verbosity\n");
}

int main(int argc, char **argv) {
    struct ev_loop *el;
    struct ev_periodic ep;
    int c;

    // Initialize stats
    errno_cnt_init(&socket_errors_cnt);
    errno_cnt_init(&fcntl_errors_cnt);
    errno_cnt_init(&connect_errors_cnt);
    errno_cnt_init(&write_errors_cnt);

    // Options and defaults
    struct hostent *hent;
    float period = 1.0f / connRate;

    optreset = optind = 1;
    while ((c = getopt(argc, argv, "r:s:n:hv")) != -1) {
        switch (c) {
        case 'h':
            usage(stdout, argv[0]);
            return 0;

        case 'r':
            if ((connRate = strtoul(optarg, NULL, 10)) > 0) {
                period = 1.0f / MIN(connRate, MAX_TICK_FREQUENCY);
            }
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

    if (connRate > 0) {
        ev_periodic_init(&ep, periodic_watcher_cb, 0, period, NULL);
        ev_periodic_start(el, &ep);
    } else {
        while (numConns-- > 0) {
            spawn_connection(el);
        }
    }

    ev_loop(el, 0);
    ev_default_destroy();

    errno_cnt_dump(stdout, "socket", &socket_errors_cnt);
    errno_cnt_dump(stdout, "fcntl", &fcntl_errors_cnt);
    errno_cnt_dump(stdout, "connect", &connect_errors_cnt);
    errno_cnt_dump(stdout, "write", &write_errors_cnt);

    return 0;
}
