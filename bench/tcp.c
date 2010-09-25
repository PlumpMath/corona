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

#define MAX_TICK_FREQUENCY                          500

#define DBG(l, x) \
    do { \
        if ((l) < dbg_level) \
            printf x; \
        } \
    while (0)

#define ERRNO_TAB_ENTRY(x) \
    { x, #x }

struct {
    int val;
    char *name;
} errno_tab[] = {
    ERRNO_TAB_ENTRY(EPERM),
    ERRNO_TAB_ENTRY(ENOENT),
    ERRNO_TAB_ENTRY(ESRCH),
    ERRNO_TAB_ENTRY(EINTR),
    ERRNO_TAB_ENTRY(EIO),
    ERRNO_TAB_ENTRY(ENXIO),
    ERRNO_TAB_ENTRY(E2BIG),
    ERRNO_TAB_ENTRY(ENOEXEC),
    ERRNO_TAB_ENTRY(EBADF),
    ERRNO_TAB_ENTRY(ECHILD),
    ERRNO_TAB_ENTRY(EDEADLK),
    ERRNO_TAB_ENTRY(ENOMEM),
    ERRNO_TAB_ENTRY(EACCES),
    ERRNO_TAB_ENTRY(EFAULT),
    ERRNO_TAB_ENTRY(EBUSY),
    ERRNO_TAB_ENTRY(EEXIST),
    ERRNO_TAB_ENTRY(EXDEV),
    ERRNO_TAB_ENTRY(ENODEV),
    ERRNO_TAB_ENTRY(ENOTDIR),
    ERRNO_TAB_ENTRY(EISDIR),
    ERRNO_TAB_ENTRY(EINVAL),
    ERRNO_TAB_ENTRY(ENFILE),
    ERRNO_TAB_ENTRY(EMFILE),
    ERRNO_TAB_ENTRY(ENOTTY),
    ERRNO_TAB_ENTRY(ETXTBSY),
    ERRNO_TAB_ENTRY(EFBIG),
    ERRNO_TAB_ENTRY(ENOSPC),
    ERRNO_TAB_ENTRY(ESPIPE),
    ERRNO_TAB_ENTRY(EROFS),
    ERRNO_TAB_ENTRY(EMLINK),
    ERRNO_TAB_ENTRY(EPIPE),
    ERRNO_TAB_ENTRY(EDOM),
    ERRNO_TAB_ENTRY(ERANGE),
    ERRNO_TAB_ENTRY(EAGAIN),
    ERRNO_TAB_ENTRY(EINPROGRESS),
    ERRNO_TAB_ENTRY(EALREADY),
    ERRNO_TAB_ENTRY(ENOTSOCK),
    ERRNO_TAB_ENTRY(EDESTADDRREQ),
    ERRNO_TAB_ENTRY(EMSGSIZE),
    ERRNO_TAB_ENTRY(EPROTOTYPE),
    ERRNO_TAB_ENTRY(ENOPROTOOPT),
    ERRNO_TAB_ENTRY(EPROTONOSUPPORT),
    ERRNO_TAB_ENTRY(ESOCKTNOSUPPORT),
    ERRNO_TAB_ENTRY(ENOTSUP),
    ERRNO_TAB_ENTRY(ENETDOWN),
    ERRNO_TAB_ENTRY(ENETUNREACH),
    ERRNO_TAB_ENTRY(ENETRESET),
    ERRNO_TAB_ENTRY(ECONNABORTED),
    ERRNO_TAB_ENTRY(ECONNRESET),
    ERRNO_TAB_ENTRY(ENOBUFS),
    ERRNO_TAB_ENTRY(EISCONN),
    ERRNO_TAB_ENTRY(ENOTCONN),
    ERRNO_TAB_ENTRY(ESHUTDOWN),
    ERRNO_TAB_ENTRY(ETOOMANYREFS),
    ERRNO_TAB_ENTRY(ETIMEDOUT),
    ERRNO_TAB_ENTRY(ECONNREFUSED)
};

typedef struct write_watcher_s {
    struct ev_io ww_ev_io;
    int ww_sock;
    size_t ww_off;
} write_watcher_t;

typedef size_t errno_cnt_t[ELAST + 1];

char buf[1 << 20];
struct sockaddr_in addr;
size_t connRate = 100;
size_t numConns = 1000;
size_t dataSize = 1 << 10;
size_t conns_cnt = 0;

size_t dbg_level = 0;

// stats
size_t watcher_run_cnt = 0;
errno_cnt_t socket_errors_cnt;
errno_cnt_t fcntl_errors_cnt;
errno_cnt_t setsockopt_errors_cnt;
errno_cnt_t connect_errors_cnt;
errno_cnt_t write_errors_cnt;

static inline char *
errno_name(int v) {
    int i;

    for (i = 0; i < sizeof(errno_tab)/sizeof(errno_tab[0]); i++) {
        if (errno_tab[i].val == v) {
            return errno_tab[i].name;
        }
    }

    return NULL;
}

static inline void
errno_cnt_incr(errno_cnt_t *ecp, int v) {
    assert(errno <= ELAST);
    (*ecp)[(v >= 0) ? 0 : errno]++;
}

static inline void
errno_cnt_init(errno_cnt_t *ecp) {
    bzero(ecp, sizeof(*ecp));
}

static inline int
errno_cnt_empty(errno_cnt_t *ecp) {
    int i;

    for (i = 0; i <= ELAST && (*ecp)[i] == 0; i++)
        ;

    return (i > ELAST);
}

static inline void
errno_cnt_dump(FILE *fp, char *name, errno_cnt_t *ecp) {
    int i;

    if (errno_cnt_empty(ecp)) {
        return;
    }

    fprintf(fp, "%s: ", name);
    for (i = 0; i <= ELAST; i++) {
        char *name;

        if ((*ecp)[i] == 0) {
            continue;
        }

        if ((name = errno_name(i))) {
            fprintf(fp, "%s", name);
        } else {
            fprintf(fp, "%d", i);
        }            

        fprintf(fp, "=%lu ", (*ecp)[i]);
    }
    fprintf(fp, "\n");
}

static void
write_watcher_cb(struct ev_loop *el, ev_io *ew, int revents) {
    write_watcher_t *ww = (write_watcher_t*) ew;
    int nbytes = 0;

    watcher_run_cnt++;

    if (!(revents & EV_WRITE)) {
        return;
    }
    
    while (ww->ww_off < dataSize) {
        nbytes = write(ww->ww_sock, buf, MIN(sizeof(buf), dataSize - ww->ww_off));
        errno_cnt_incr(&write_errors_cnt, nbytes);
        DBG(2, ("wrote %d bytes to socket %d\n", nbytes, ww->ww_sock));

        if (nbytes < 0) {
            break;
        }

        ww->ww_off += nbytes;
    }

    assert(nbytes < 0 || ww->ww_off == dataSize);

    if (ww->ww_off == dataSize ||
        (nbytes < 0 && errno != EAGAIN)) {
        DBG(
            1,
            ("closing socket %d with errno %d\n",
                ww->ww_sock, (nbytes < 0) ? errno : 0)
        );

        ev_io_stop(el, &ww->ww_ev_io);
        close(ww->ww_sock);
        free(ww);
    }
}

static void
spawn_connection(struct ev_loop *el) {
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

    err = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &sock, sizeof(sock));
    errno_cnt_incr(&setsockopt_errors_cnt, err);
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
        1,
        ("created socket %d; %lu connections remaining\n",
            sock, numConns - conns_cnt)
    );
}

static void
periodic_watcher_cb(struct ev_loop *el, struct ev_periodic *ep, int revents) {
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

void
atexit_cb(void) {
    printf("watcher invocations: %lu\n", watcher_run_cnt);
    errno_cnt_dump(stdout, "socket", &socket_errors_cnt);
    errno_cnt_dump(stdout, "fcntl", &fcntl_errors_cnt);
    errno_cnt_dump(stdout, "setsockopt", &setsockopt_errors_cnt);
    errno_cnt_dump(stdout, "connect", &connect_errors_cnt);
    errno_cnt_dump(stdout, "write", &write_errors_cnt);
}

void
sigint_cb(int sig) {
    exit(0);
}

void
usage(FILE *fp, char *name) {
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

int
main(int argc, char **argv) {
    struct ev_loop *el;
    struct ev_periodic ep;
    struct sigaction act;
    int c;

    // Initialize stats
    errno_cnt_init(&socket_errors_cnt);
    errno_cnt_init(&fcntl_errors_cnt);
    errno_cnt_init(&setsockopt_errors_cnt);
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

    bzero(&addr, sizeof(addr));
    memcpy(&addr.sin_addr, hent->h_addr, sizeof(addr.sin_addr));
    addr.sin_port = htons(strtoul(argv[optind + 1], NULL, 10));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;

    bzero(&act, sizeof(act));
    act.sa_handler = sigint_cb;
    if (sigaction(SIGINT, &act, NULL)) {
        perror("sigaction");
        exit(1);
    }

    el = ev_default_loop(EVFLAG_AUTO);

    if (connRate > 0) {
        ev_periodic_init(&ep, periodic_watcher_cb, 0, period, NULL);
        ev_periodic_start(el, &ep);
    } else {
        while (numConns-- > 0) {
            spawn_connection(el);
        }
    }

    atexit(atexit_cb);

    ev_loop(el, 0);
    ev_default_destroy();

    return 0;
}
