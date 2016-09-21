#include <signal.h>
#include <linux/limits.h>
#include <sys/inotify.h>
#include <sys/time.h>
#include <unistd.h>

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

using wd_to_dir_lookup_t = unordered_map<int, string>;

namespace
{

    wd_to_dir_lookup_t setup_watches(int inotify_fd,
                                     uint32_t mask,
                                     const char * const * begin_dir,
                                     const char * const * end_dir);
    void configure_signal_handlers(void);
    void deconfigure_signal_handlers(void);
    void sig_orderly_stop(int);
    void sig_no_op(int);
    void configure_flush_timer(void);
    void deconfigure_flush_timer(void);
    void print_legend(void);

    sig_atomic_t g_stop = 0;

    struct run_stats
    {
        uint64_t events {};
        uint64_t overflows {};
        uint32_t buffer_resizes {};
        timeval start {0, 0};
        timeval end  {0, 0};

        run_stats();
        void end_run(void);
        void print() const;
    };
}

int main(int argc, const char * const argv[])
{
    // TODO: Better command-line parsing/help.
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <directory> [<directory> ...]\n", argv[0]);
        return EXIT_FAILURE;
    }
    int infd = inotify_init();
    if (infd == -1)
    {
        perror("Error (inotify_init)");
        return EXIT_FAILURE;
    }
    const uint32_t mask = IN_ACCESS |
                          IN_CREATE |
                          IN_DELETE |
                          IN_MOVE |
                          IN_OPEN | 
                          IN_CLOSE | 
                          IN_MODIFY;
    wd_to_dir_lookup_t wd_to_dir_map = setup_watches(infd, mask, argv + 1, argv + argc);
    const size_t MAX_EVENT_SIZE = sizeof(inotify_event) + NAME_MAX + 1;
    // Note: Default event queue size is 16384:
    // root@7e3a25bd2ab5:/# cat /proc/sys/fs/inotify/max_queued_events
    // 16384
    const size_t MAX_EVENTS_PER_WD = 128; // Arbitrary.
    const size_t MAX_BUFFER_SIZE = MAX_EVENT_SIZE * wd_to_dir_map.size() * MAX_EVENTS_PER_WD;
    vector<uint8_t> buffer(wd_to_dir_map.size() * MAX_EVENT_SIZE);
    size_t buffer_resize_threshold = buffer.size() - MAX_EVENT_SIZE;
    bool can_resize_buffer = (buffer.size() < MAX_BUFFER_SIZE);
    configure_signal_handlers();
    configure_flush_timer();
    run_stats stats;
    print_legend();
    printf("Waiting for events...\n");
    fflush(stdout);
    while (!g_stop)
    {
        // TODO: Could I async the output and/or flushing?
        //       I think the standard library is safe, but it might be tricky to
        //       incur no flushing overhead on the primary thread or else data
        //       structures for queueing would be a faff.
        ssize_t read_bytes = read(infd, buffer.data(), buffer.size());
        if (read_bytes > 0)
        {
            timeval now;
            static_cast<void>(gettimeofday(&now, nullptr));
            const tm *tm_now = gmtime(&now.tv_sec);
            char datetime[27]; // Length of date/time, assuming 4-digit year and null terminator.
            snprintf(datetime, sizeof(datetime), "%d-%02d-%02d %02d:%02d:%02d.%06ld",
                tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday, tm_now->tm_hour,
                tm_now->tm_min, tm_now->tm_sec, static_cast<long>(now.tv_usec));

            uint8_t * const read_end = buffer.data() + read_bytes;
            uint8_t * event_ptr = buffer.data();
            while (event_ptr < read_end)
            {
                assert(static_cast<size_t>(read_end - event_ptr) >= sizeof(inotify_event));

                const inotify_event *event = reinterpret_cast<inotify_event *>(event_ptr);
                auto wfd_i = wd_to_dir_map.find(event->wd);
                if (wfd_i == wd_to_dir_map.end())
                {
                    if (event->wd == -1 && (event->mask & IN_Q_OVERFLOW) != 0)
                    {
                        printf("%s: Erp! inotify event queue overflow\n", datetime);
                        ++stats.overflows;
                    }
                    event_ptr += sizeof(inotify_event) + event->len; // TODO: Repetition.
                    continue;
                }
                char operations[16] = {};
                char *operation = operations;
                // TODO: This is a bit gruesome on the reader.
                // TODO: Did I even hit all the cases?
                if (event->mask & IN_ACCESS)
                {
                    *operation++ = 'r';
                }
                /*if (event->mask & IN_ATTRIB)
                {
                    *operation++ = 'a';
                }*/
                if (event->mask & IN_CLOSE_WRITE)
                {
                    *operation++ = 'X';
                }
                if (event->mask & IN_CLOSE_NOWRITE)
                {
                    *operation++ = 'x';
                }
                if (event->mask & IN_CREATE)
                {
                    *operation++ = 'c';
                }
                if (event->mask & IN_DELETE)
                {
                    *operation++ = 'd';
                }
                // IN_DELETE_SELF
                if (event->mask & IN_MODIFY)
                {
                    *operation++ = 'w';
                }
                // IN_MOVE_SELF
                if (event->mask & IN_MOVED_FROM)
                {
                    *operation++ = '<';
                }
                if (event->mask & IN_MOVED_TO)
                {
                    *operation++ = '>';
                }
                if (event->mask & IN_OPEN)
                {
                    *operation++ = 'o';
                }
                // TODO: Handle non-zero cookies better?
                // TODO: More elegantly handle dir/file events?
                const char *name = event->len ? event->name : "";
                // TODO: When watches overlap it can produce duplicate events.
                //       Can we fold them somehow? Is that even wise?
                printf("%s: Event: %s (path: %s/%s, watch: %d, cookie: %" PRIu32 ")\n",
                    datetime, operations, wfd_i->second.c_str(), name, event->wd,
                    event->cookie);
                event_ptr += sizeof(inotify_event) + event->len;
                ++stats.events;
            }
            // Resize the buffer if we're running high in it.
            if (static_cast<size_t>(read_bytes) >= buffer_resize_threshold && can_resize_buffer)
            {
                buffer.resize(buffer.size() * 2);
                // TODO: Repetition of these calculations.
                buffer_resize_threshold = buffer.size() - MAX_EVENT_SIZE;
                can_resize_buffer = (buffer.size() < MAX_BUFFER_SIZE);
                ++stats.buffer_resizes;
            }
        }
        else
        {
            // Assume we were woken by a signal.
            // TODO: This might be iffy and we ought to consider taking action if errno isn't what we expect.
            fflush(stdout);
        }
    }
    stats.end_run();
    deconfigure_flush_timer();
    deconfigure_signal_handlers();
    stats.print();
    fflush(stdout);
    close(infd);
}

namespace
{
    // Create directory watches on the specified inotify instance.
    wd_to_dir_lookup_t setup_watches(int inotify_fd,
                                     uint32_t mask,
                                     const char * const * begin_dir,
                                     const char * const * end_dir)
    {
        wd_to_dir_lookup_t wd_to_dir_map;
        for (auto dir_i = begin_dir; dir_i < end_dir; ++dir_i)
        {
            // TODO: Make sense of relative paths.
            const char *dir_name = *dir_i;
            int wfd = inotify_add_watch(inotify_fd, dir_name, mask);
            if (wfd == -1)
            {
                perror("Error (inotify_add_watch)");
                // TODO: It'd be nicer/safer to throw and tear down politely.
                exit(EXIT_FAILURE);
            }
            string &name_to_print = wd_to_dir_map[wfd] = dir_name;
            // Trim the trailing slash for presentation purposes.
            // (This includes for the root directory.)
            while (!name_to_print.empty() && name_to_print.back() == '/')
            {
                name_to_print.pop_back();
            }
        }
        return wd_to_dir_map;
    }

    // Signal handler indicating the program should stop in an orderly fashion.
    void sig_orderly_stop(int)
    {
        g_stop = 1;
    }

    // No-op signal handler.
    void sig_no_op(int)
    {}

    void configure_signal_handlers(void)
    {
        // From signal(7), read(2) on inotify instances never restart.
        // However, within docker, they apparently do (obeying the BSD default
        // of applying SA_RESTART implicitly for signal(2) invocations).
        // So be explicit about not restarting by using sigaction(2) with
        // SA_RESTART clear.
        using sa_t = struct sigaction;
        auto sa = sa_t();
        sa.sa_handler = sig_no_op;
        static_cast<void>(sigaction(SIGALRM, &sa, nullptr));
        sa.sa_handler = sig_orderly_stop;
        static_cast<void>(sigaction(SIGINT, &sa, nullptr));
        static_cast<void>(sigaction(SIGTERM, &sa, nullptr));
    }

    void deconfigure_signal_handlers(void)
    {
        static_cast<void>(signal(SIGTERM, SIG_IGN));
        static_cast<void>(signal(SIGINT, SIG_IGN));
        static_cast<void>(signal(SIGALRM, SIG_IGN));
    }

    void configure_flush_timer(void)
    {
        const itimerval alarm_timer =
        {
            /*.it_interval =*/ { /*.tv_sec =*/ 10, /*.tv_usec =*/ 0 },
            /*.it_value =*/ { /*.tv_sec =*/ 10, /*.tv_usec =*/ 0 }
        };
        if (setitimer(ITIMER_REAL, &alarm_timer, nullptr) != 0)
        {
            perror("Warning (setitimer)"); // TODO: Be stronger than that?
        }
    }

    void deconfigure_flush_timer(void)
    {
        // NB: Unified init syntax here leads to warning on older GCCs.
        const itimerval disable_timer = itimerval();
        static_cast<void>(setitimer(ITIMER_REAL, &disable_timer, nullptr));
    }

    void print_legend(void)
    {
        struct event_desc
        {
            char event;
            const char *desc;
        };
        // TODO: Some repetition of the event-decoding loop.
        const event_desc event_descriptions[] = 
        {
            {'c', "Object was created"},
            {'o', "Object was opened"},
            {'r', "Object was accessed"},
            {'w', "Object was modified"},
            {'<', "Object was moved (original name)"},
            {'>', "Object was moved (new name)"},
            {'d', "Object was deleted"},
            {'x', "Object not opened for writing was closed"},
            {'X', "Object opened for writing was closed"},
        };
        printf("Event descriptions:\n");
        for (auto &e : event_descriptions)
        {
            printf("Event %c: %s\n", e.event, e.desc);
        }
    }

    run_stats::run_stats()
    {
        gettimeofday(&start, nullptr);
    }

    void run_stats::end_run(void)
    {
        gettimeofday(&end, nullptr);
    }

    void run_stats::print() const
    {
        double seconds = difftime(end.tv_sec, start.tv_sec);
        suseconds_t useconds = end.tv_usec - start.tv_usec;
        if (useconds < 0)
        {
            useconds += 1000000;
            seconds -= 1;
            assert(useconds >= 0);
        }
        // TODO: The cast from double to long is iffy, esp. for long run times.
        printf("Run time: %ld.%06lds\n", static_cast<long>(seconds), static_cast<long>(useconds));
        printf("Total events: %" PRIu64 "\n", events);
        printf("inotify queue overflows: %" PRIu64 "\n", overflows);
        printf("read() buffer resizes: %" PRIu32 "\n", buffer_resizes);
    }
} // end anonymous namespace
