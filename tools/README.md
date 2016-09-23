inotify watcher tools
=====================

Miscellaneous tools for use with the inotify watcher.

### What's in here? ###
* `timeline`: Script for combining multiple (compressed) filesystem event logs
  into a single annotated stream

### `timeline`: Example usage ###
Assume a set of files from different sources, names `source1.events.gz`,
`source2.events.gz` and so on, which contain the gzipped output from the
watcher tool. Let's say we want to filter and combine the events from these
multiple sources for a single path (in this case, `/tmp/test/ids/3/`), matching
only file creation (`c`), file closure (for non-write FDs; `x`) and file
renaming (`<`/`>`) from 13:31:58.738 onwards. Here's how to do it and how it
looks:

    % timeline -d '^source[[:digit:]]+' -s 13:31:58.738 -e '[c<>x]' -p 'ids/3/' source*.events.gz
    source1 2016-09-21 13:31:59.324189: Event: < (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 181420)
    source1 2016-09-21 13:31:59.324189: Event: > (path: /tmp/test/ids/3/file.txt.bak, watch: 7, cookie: 181420)
    source1 2016-09-21 13:31:59.328106: Event: c (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 0)
    source2 2016-09-21 13:31:59.329125: Event: < (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 236675)
    source2 2016-09-21 13:31:59.329125: Event: > (path: /tmp/test/ids/3/file.txt.bak, watch: 7, cookie: 236675)
    source3 2016-09-21 13:31:59.339060: Event: < (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 738359)
    source3 2016-09-21 13:31:59.339060: Event: > (path: /tmp/test/ids/3/file.txt.bak, watch: 7, cookie: 738359)
    source3 2016-09-21 13:31:59.343266: Event: c (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 0)
    source1 2016-09-21 13:32:41.057735: Event: < (path: /tmp/test/ids/3/file.txt.bak, watch: 7, cookie: 181427)
    source1 2016-09-21 13:32:41.057735: Event: > (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 181427)
    source2 2016-09-21 13:32:42.261778: Event: < (path: /tmp/test/ids/3/file.txt.bak, watch: 7, cookie: 236681)
    source2 2016-09-21 13:32:42.261778: Event: > (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 236681)
    source3 2016-09-21 13:32:43.508343: Event: < (path: /tmp/test/ids/3/file.txt.bak, watch: 7, cookie: 738367)
    source3 2016-09-21 13:32:43.508343: Event: > (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 738367)
    source2 2016-09-21 13:32:45.049265: Event: x (path: /tmp/test/ids/3/file.txt, watch: 7, cookie: 0)

The `-d` option is a pattern used to transform the source filename into the
prefixed source annotation. The `-s` option indicates the start time. The `-e`
option indicates a pattern to match events, while `-p` matches paths. The
output is coloured by default when output to a terminal, which can be
controlled with `-c`, passing `never`, `always` or `auto`.

For more information, try `timeline -h`.
