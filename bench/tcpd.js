var fd = sys.socket(sys.AF_INET, sys.SOCK_STREAM, sys.PROTO_TCP);
if (fd < 0) {
    throw new Error('socket');
}

var err = sys.setsockopt(fd, sys.SOL_SOCKET, sys.SO_REUSEPORT, 1);
if (err < 0) {
    throw new Error('setsockopt');
}

err = sys.bind(fd, 4000);
if (err < 0) {
    throw new Error('bind');
}

err = sys.listen(fd, 64);
if (err < 0) {
    throw new Error('listen');
}

err = sys.fcntl(fd, sys.F_SETFL, sys.O_NONBLOCK);
if (err < 0) {
    throw new Errror('fcntl');
}

var i = 0;
var err = sys.accept(fd, function(fd2) {
    // console.log('Got new TCP connection: ' + (++i));
    sys.close(fd2);
});
