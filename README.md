inotify watcher
===============
A simple file/directory watcher for Linux built on top of
[`inotify`] [`inotify(7)`].

### Example usage ###
Here's the result of watching `/root` while performing `ls /root` in another
shell, then opening another `bash` and finally exiting with `Ctrl-C`:

    root@7e3a25bd2ab5:/opt/64091/inotify-watcher# ./watcher /root/
    Event descriptions:
    Event c: Object was created
    Event o: Object was opened
    Event r: Object was accessed
    Event w: Object was modified
    Event <: Object was moved (original name)
    Event >: Object was moved (new name)
    Event d: Object was deleted
    Event x: Object not opened for writing was closed
    Event X: Object opened for writing was closed
    Waiting for events...
    2016-09-21 10:19:06.451041: Event: o (path: /root/, watch: 1, cookie: 0)
    2016-09-21 10:19:06.451095: Event: r (path: /root/, watch: 1, cookie: 0)
    2016-09-21 10:19:06.451123: Event: r (path: /root/, watch: 1, cookie: 0)
    2016-09-21 10:19:06.451146: Event: x (path: /root/, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681225: Event: o (path: /root/.bashrc, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681282: Event: r (path: /root/.bashrc, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681312: Event: x (path: /root/.bashrc, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681385: Event: o (path: /root/.bash_history, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681415: Event: r (path: /root/.bash_history, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681433: Event: x (path: /root/.bash_history, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681477: Event: o (path: /root/.bash_history, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681500: Event: r (path: /root/.bash_history, watch: 1, cookie: 0)
    2016-09-21 10:19:08.681519: Event: x (path: /root/.bash_history, watch: 1, cookie: 0)
    ^CRun time: 6.406790s
    Total events: 13
    inotify queue overflows: 0
    read() buffer resizes: 1

It can watch as many directories as specified on the command line (although
it's limited by the number of watches available to an `inotify` instance, and
it's single-threaded to boot, meaning that watching many files/directories may
lead to more lost events).

### See also ###

* [`inotify(7)`] [] provides background on how the watch mechanism works
* [`inotifywait`] [] is a similar tool

[`inotify(7)`]: http://man7.org/linux/man-pages/man7/inotify.7.html
[`inotifywait`]: http://man7.org/linux/man-pages/man1/inotifywait.1.html
