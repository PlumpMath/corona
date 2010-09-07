### Mac OSX

When running tests with lots of connections (high `-n NNNN` value), you'll want
to change the ephemeral port range to something larger than the 16k default

    % sudo sysctl -w net.inet.ip.portrange.first=1024
    % sudo sysctl -w net.inet.ip.portrange.hifirst=1024

### Updating V8

Grab V8 snapshots by doing something like

    % cd deps
    % svn co http://v8.googlecode.com/svn/tags/2.4.1 v8-2.4.1
    % git add v8-2.4.1

... and don't forget to update `V8_VERS` in the top-level `Makefile`.
