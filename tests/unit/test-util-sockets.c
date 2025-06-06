/*
 * Tests for util/qemu-sockets.c
 *
 * Copyright 2018 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qemu/sockets.h"
#include "qapi/error.h"
#include "socket-helpers.h"
#include "monitor/monitor.h"

static void test_fd_is_socket_bad(void)
{
    char *tmp = g_strdup("qemu-test-util-sockets-XXXXXX");
    int fd = mkstemp(tmp);
    if (fd != 0) {
        unlink(tmp);
    }
    g_free(tmp);

    g_assert(fd >= 0);

    g_assert(!fd_is_socket(fd));
    close(fd);
}

static void test_fd_is_socket_good(void)
{
    int fd = qemu_socket(PF_INET, SOCK_STREAM, 0);

    g_assert(fd >= 0);

    g_assert(fd_is_socket(fd));
    close(fd);
}

static int mon_fd = -1;
static const char *mon_fdname;
__thread Monitor *cur_mon;

int monitor_get_fd(Monitor *mon, const char *fdname, Error **errp)
{
    g_assert(cur_mon);
    g_assert(mon == cur_mon);
    if (mon_fd == -1 || !g_str_equal(mon_fdname, fdname)) {
        error_setg(errp, "No fd named %s", fdname);
        return -1;
    }
    return dup(mon_fd);
}

/*
 * Syms of stubs in libqemuutil.a are discarded at .o file
 * granularity.  To replace monitor_get_fd() and monitor_cur(), we
 * must ensure that we also replace any other symbol that is used in
 * the binary and would be taken from the same stub object file,
 * otherwise we get duplicate syms at link time.
 */
Monitor *monitor_cur(void) { return cur_mon; }
Monitor *monitor_set_cur(Coroutine *co, Monitor *mon) { abort(); }
int monitor_vprintf(Monitor *mon, const char *fmt, va_list ap) { abort(); }

#ifndef _WIN32
static void test_socket_fd_pass_name_good(void)
{
    SocketAddress addr;
    int fd;

    cur_mon = g_malloc(1); /* Fake a monitor */
    mon_fdname = "myfd";
    mon_fd = qemu_socket(AF_INET, SOCK_STREAM, 0);
    g_assert_cmpint(mon_fd, >, STDERR_FILENO);

    addr.type = SOCKET_ADDRESS_TYPE_FD;
    addr.u.fd.str = g_strdup(mon_fdname);

    fd = socket_connect(&addr, &error_abort);
    g_assert_cmpint(fd, !=, -1);
    g_assert_cmpint(fd, !=, mon_fd);
    close(fd);

    fd = socket_listen(&addr, 1, &error_abort);
    g_assert_cmpint(fd, !=, -1);
    g_assert_cmpint(fd, !=, mon_fd);
    close(fd);

    g_free(addr.u.fd.str);
    mon_fdname = NULL;
    close(mon_fd);
    mon_fd = -1;
    g_free(cur_mon);
    cur_mon = NULL;
}

static void test_socket_fd_pass_name_bad(void)
{
    SocketAddress addr;
    Error *err = NULL;
    int fd;

    cur_mon = g_malloc(1); /* Fake a monitor */
    mon_fdname = "myfd";
    mon_fd = dup(STDOUT_FILENO);
    g_assert_cmpint(mon_fd, >, STDERR_FILENO);

    addr.type = SOCKET_ADDRESS_TYPE_FD;
    addr.u.fd.str = g_strdup(mon_fdname);

    fd = socket_connect(&addr, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    fd = socket_listen(&addr, 1, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    g_free(addr.u.fd.str);
    mon_fdname = NULL;
    close(mon_fd);
    mon_fd = -1;
    g_free(cur_mon);
    cur_mon = NULL;
}

static void test_socket_fd_pass_name_nomon(void)
{
    SocketAddress addr;
    Error *err = NULL;
    int fd;

    g_assert(cur_mon == NULL);

    addr.type = SOCKET_ADDRESS_TYPE_FD;
    addr.u.fd.str = g_strdup("myfd");

    fd = socket_connect(&addr, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    fd = socket_listen(&addr, 1, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    g_free(addr.u.fd.str);
}


static void test_socket_fd_pass_num_good(void)
{
    SocketAddress addr;
    int fd, sfd;

    g_assert(cur_mon == NULL);
    sfd = qemu_socket(AF_INET, SOCK_STREAM, 0);
    g_assert_cmpint(sfd, >, STDERR_FILENO);

    addr.type = SOCKET_ADDRESS_TYPE_FD;
    addr.u.fd.str = g_strdup_printf("%d", sfd);

    fd = socket_connect(&addr, &error_abort);
    g_assert_cmpint(fd, ==, sfd);

    fd = socket_listen(&addr, 1, &error_abort);
    g_assert_cmpint(fd, ==, sfd);

    g_free(addr.u.fd.str);
    close(sfd);
}

static void test_socket_fd_pass_num_bad(void)
{
    SocketAddress addr;
    Error *err = NULL;
    int fd, sfd;

    g_assert(cur_mon == NULL);
    sfd = dup(STDOUT_FILENO);

    addr.type = SOCKET_ADDRESS_TYPE_FD;
    addr.u.fd.str = g_strdup_printf("%d", sfd);

    fd = socket_connect(&addr, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    fd = socket_listen(&addr, 1, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    g_free(addr.u.fd.str);
    close(sfd);
}

static void test_socket_fd_pass_num_nocli(void)
{
    SocketAddress addr;
    Error *err = NULL;
    int fd;

    cur_mon = g_malloc(1); /* Fake a monitor */

    addr.type = SOCKET_ADDRESS_TYPE_FD;
    addr.u.fd.str = g_strdup_printf("%d", STDOUT_FILENO);

    fd = socket_connect(&addr, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    fd = socket_listen(&addr, 1, &err);
    g_assert_cmpint(fd, ==, -1);
    error_free_or_abort(&err);

    g_free(addr.u.fd.str);
}
#endif

#ifdef CONFIG_LINUX

#define ABSTRACT_SOCKET_VARIANTS 3

typedef struct {
    SocketAddress *server, *client[ABSTRACT_SOCKET_VARIANTS];
    bool expect_connect[ABSTRACT_SOCKET_VARIANTS];
} abstract_socket_matrix_row;

static gpointer unix_client_thread_func(gpointer user_data)
{
    abstract_socket_matrix_row *row = user_data;
    Error *err = NULL;
    int i, fd;

    for (i = 0; i < ABSTRACT_SOCKET_VARIANTS; i++) {
        if (row->expect_connect[i]) {
            fd = socket_connect(row->client[i], &error_abort);
            g_assert_cmpint(fd, >=, 0);
        } else {
            fd = socket_connect(row->client[i], &err);
            g_assert_cmpint(fd, ==, -1);
            error_free_or_abort(&err);
        }
        close(fd);
    }
    return NULL;
}

static void test_socket_unix_abstract_row(abstract_socket_matrix_row *test)
{
    int fd, connfd, i;
    GThread *cli;
    struct sockaddr_un un;
    socklen_t len = sizeof(un);

    /* Last one must connect, or else accept() below hangs */
    assert(test->expect_connect[ABSTRACT_SOCKET_VARIANTS - 1]);

    fd = socket_listen(test->server, 1, &error_abort);
    g_assert_cmpint(fd, >=, 0);
    g_assert(fd_is_socket(fd));

    cli = g_thread_new("abstract_unix_client",
                       unix_client_thread_func,
                       test);

    for (i = 0; i < ABSTRACT_SOCKET_VARIANTS; i++) {
        if (test->expect_connect[i]) {
            connfd = accept(fd, (struct sockaddr *)&un, &len);
            g_assert_cmpint(connfd, !=, -1);
            close(connfd);
        }
    }

    close(fd);
    g_thread_join(cli);
}

static void test_socket_unix_abstract(void)
{
    SocketAddress addr, addr_tight, addr_padded;
    abstract_socket_matrix_row matrix[ABSTRACT_SOCKET_VARIANTS] = {
        { &addr,
          { &addr_tight, &addr_padded, &addr },
          { true, false, true } },
        { &addr_tight,
          { &addr_padded, &addr, &addr_tight },
          { false, true, true } },
        { &addr_padded,
          { &addr, &addr_tight, &addr_padded },
          { false, false, true } }
    };
    int i;

    i = g_file_open_tmp("unix-XXXXXX", &addr.u.q_unix.path, NULL);
    g_assert_true(i >= 0);
    close(i);

    addr.type = SOCKET_ADDRESS_TYPE_UNIX;
    addr.u.q_unix.has_abstract = true;
    addr.u.q_unix.abstract = true;
    addr.u.q_unix.has_tight = false;
    addr.u.q_unix.tight = false;

    addr_tight = addr;
    addr_tight.u.q_unix.has_tight = true;
    addr_tight.u.q_unix.tight = true;

    addr_padded = addr;
    addr_padded.u.q_unix.has_tight = true;
    addr_padded.u.q_unix.tight = false;

    for (i = 0; i < ABSTRACT_SOCKET_VARIANTS; i++) {
        test_socket_unix_abstract_row(&matrix[i]);
    }

    unlink(addr.u.q_unix.path);
    g_free(addr.u.q_unix.path);
}

#endif  /* CONFIG_LINUX */

static void inet_parse_test_helper(const char *str,
                                   InetSocketAddress *exp_addr, bool success)
{
    InetSocketAddress addr;
    Error *error = NULL;

    int rc = inet_parse(&addr, str, &error);

    if (success) {
        if (error) {
            error_report_err(error);
        }
        g_assert_cmpint(rc, ==, 0);
    } else {
        error_free(error);
        g_assert_cmpint(rc, <, 0);
    }
    if (exp_addr != NULL) {
        g_assert_cmpstr(addr.host, ==, exp_addr->host);
        g_assert_cmpstr(addr.port, ==, exp_addr->port);
        /* Own members: */
        g_assert_cmpint(addr.has_numeric, ==, exp_addr->has_numeric);
        g_assert_cmpint(addr.numeric, ==, exp_addr->numeric);
        g_assert_cmpint(addr.has_to, ==, exp_addr->has_to);
        g_assert_cmpint(addr.to, ==, exp_addr->to);
        g_assert_cmpint(addr.has_ipv4, ==, exp_addr->has_ipv4);
        g_assert_cmpint(addr.ipv4, ==, exp_addr->ipv4);
        g_assert_cmpint(addr.has_ipv6, ==, exp_addr->has_ipv6);
        g_assert_cmpint(addr.ipv6, ==, exp_addr->ipv6);
        g_assert_cmpint(addr.has_keep_alive, ==, exp_addr->has_keep_alive);
        g_assert_cmpint(addr.keep_alive, ==, exp_addr->keep_alive);
#ifdef HAVE_TCP_KEEPCNT
        g_assert_cmpint(addr.has_keep_alive_count, ==,
                        exp_addr->has_keep_alive_count);
        g_assert_cmpint(addr.keep_alive_count, ==,
                        exp_addr->keep_alive_count);
#endif
#ifdef HAVE_TCP_KEEPIDLE
        g_assert_cmpint(addr.has_keep_alive_idle, ==,
                        exp_addr->has_keep_alive_idle);
        g_assert_cmpint(addr.keep_alive_idle, ==,
                        exp_addr->keep_alive_idle);
#endif
#ifdef HAVE_TCP_KEEPINTVL
        g_assert_cmpint(addr.has_keep_alive_interval, ==,
                        exp_addr->has_keep_alive_interval);
        g_assert_cmpint(addr.keep_alive_interval, ==,
                        exp_addr->keep_alive_interval);
#endif
#ifdef HAVE_IPPROTO_MPTCP
        g_assert_cmpint(addr.has_mptcp, ==, exp_addr->has_mptcp);
        g_assert_cmpint(addr.mptcp, ==, exp_addr->mptcp);
#endif
    }

    g_free(addr.host);
    g_free(addr.port);
}

static void test_inet_parse_nohost_good(void)
{
    char host[] = "";
    char port[] = "5000";
    InetSocketAddress exp_addr = {
        .host = host,
        .port = port,
    };
    inet_parse_test_helper(":5000", &exp_addr, true);
}

static void test_inet_parse_empty_bad(void)
{
    inet_parse_test_helper("", NULL, false);
}

static void test_inet_parse_only_colon_bad(void)
{
    inet_parse_test_helper(":", NULL, false);
}

static void test_inet_parse_ipv4_good(void)
{
    char host[] = "127.0.0.1";
    char port[] = "5000";
    InetSocketAddress exp_addr = {
        .host = host,
        .port = port,
    };
    inet_parse_test_helper("127.0.0.1:5000", &exp_addr, true);
}

static void test_inet_parse_ipv4_noport_bad(void)
{
    inet_parse_test_helper("127.0.0.1", NULL, false);
}

static void test_inet_parse_ipv6_good(void)
{
    char host[] = "::1";
    char port[] = "5000";
    InetSocketAddress exp_addr = {
        .host = host,
        .port = port,
    };
    inet_parse_test_helper("[::1]:5000", &exp_addr, true);
}

static void test_inet_parse_ipv6_noend_bad(void)
{
    inet_parse_test_helper("[::1", NULL, false);
}

static void test_inet_parse_ipv6_noport_bad(void)
{
    inet_parse_test_helper("[::1]:", NULL, false);
}

static void test_inet_parse_ipv6_empty_bad(void)
{
    inet_parse_test_helper("[]:5000", NULL, false);
}

static void test_inet_parse_hostname_good(void)
{
    char host[] = "localhost";
    char port[] = "5000";
    InetSocketAddress exp_addr = {
        .host = host,
        .port = port,
    };
    inet_parse_test_helper("localhost:5000", &exp_addr, true);
}

static void test_inet_parse_all_options_good(void)
{
    char host[] = "::1";
    char port[] = "5000";
    InetSocketAddress exp_addr = {
        .host = host,
        .port = port,
        .has_numeric = true,
        .numeric =  true,
        .has_to = true,
        .to = 5006,
        .has_ipv4 = true,
        .ipv4 = false,
        .has_ipv6 = true,
        .ipv6 = true,
        .has_keep_alive = true,
        .keep_alive = true,
#ifdef HAVE_TCP_KEEPCNT
        .has_keep_alive_count = true,
        .keep_alive_count = 10,
#endif
#ifdef HAVE_TCP_KEEPIDLE
        .has_keep_alive_idle = true,
        .keep_alive_idle = 60,
#endif
#ifdef HAVE_TCP_KEEPINTVL
        .has_keep_alive_interval = true,
        .keep_alive_interval = 30,
#endif
#ifdef HAVE_IPPROTO_MPTCP
        .has_mptcp = true,
        .mptcp = false,
#endif
    };
    inet_parse_test_helper(
        "[::1]:5000,numeric=on,to=5006,ipv4=off,ipv6=on,keep-alive=on"
#ifdef HAVE_TCP_KEEPCNT
        ",keep-alive-count=10"
#endif
#ifdef HAVE_TCP_KEEPIDLE
        ",keep-alive-idle=60"
#endif
#ifdef HAVE_TCP_KEEPINTVL
        ",keep-alive-interval=30"
#endif
#ifdef HAVE_IPPROTO_MPTCP
        ",mptcp=off"
#endif
        , &exp_addr, true);
}

static void test_inet_parse_all_implicit_bool_good(void)
{
    char host[] = "::1";
    char port[] = "5000";
    InetSocketAddress exp_addr = {
        .host = host,
        .port = port,
        .has_numeric = true,
        .numeric =  true,
        .has_to = true,
        .to = 5006,
        .has_ipv4 = true,
        .ipv4 = true,
        .has_ipv6 = true,
        .ipv6 = true,
        .has_keep_alive = true,
        .keep_alive = true,
#ifdef HAVE_IPPROTO_MPTCP
        .has_mptcp = true,
        .mptcp = true,
#endif
    };
    inet_parse_test_helper(
        "[::1]:5000,numeric,to=5006,ipv4,ipv6,keep-alive"
#ifdef HAVE_IPPROTO_MPTCP
        ",mptcp"
#endif
        , &exp_addr, true);
}

int main(int argc, char **argv)
{
    bool has_ipv4, has_ipv6;

    qemu_init_main_loop(&error_abort);
    socket_init();

    g_test_init(&argc, &argv, NULL);

    /* We're creating actual IPv4/6 sockets, so we should
     * check if the host running tests actually supports
     * each protocol to avoid breaking tests on machines
     * with either IPv4 or IPv6 disabled.
     */
    if (socket_check_protocol_support(&has_ipv4, &has_ipv6) < 0) {
        g_printerr("socket_check_protocol_support() failed\n");
        goto end;
    }

    if (has_ipv4) {
        g_test_add_func("/util/socket/is-socket/bad",
                        test_fd_is_socket_bad);
        g_test_add_func("/util/socket/is-socket/good",
                        test_fd_is_socket_good);
#ifndef _WIN32
        g_test_add_func("/socket/fd-pass/name/good",
                        test_socket_fd_pass_name_good);
        g_test_add_func("/socket/fd-pass/name/bad",
                        test_socket_fd_pass_name_bad);
        g_test_add_func("/socket/fd-pass/name/nomon",
                        test_socket_fd_pass_name_nomon);
        g_test_add_func("/socket/fd-pass/num/good",
                        test_socket_fd_pass_num_good);
        g_test_add_func("/socket/fd-pass/num/bad",
                        test_socket_fd_pass_num_bad);
        g_test_add_func("/socket/fd-pass/num/nocli",
                        test_socket_fd_pass_num_nocli);
#endif
    }

#ifdef CONFIG_LINUX
    g_test_add_func("/util/socket/unix-abstract",
                    test_socket_unix_abstract);
#endif

    g_test_add_func("/util/socket/inet-parse/nohost-good",
                    test_inet_parse_nohost_good);
    g_test_add_func("/util/socket/inet-parse/empty-bad",
                    test_inet_parse_empty_bad);
    g_test_add_func("/util/socket/inet-parse/only-colon-bad",
                    test_inet_parse_only_colon_bad);
    g_test_add_func("/util/socket/inet-parse/ipv4-good",
                    test_inet_parse_ipv4_good);
    g_test_add_func("/util/socket/inet-parse/ipv4-noport-bad",
                    test_inet_parse_ipv4_noport_bad);
    g_test_add_func("/util/socket/inet-parse/ipv6-good",
                    test_inet_parse_ipv6_good);
    g_test_add_func("/util/socket/inet-parse/ipv6-noend-bad",
                    test_inet_parse_ipv6_noend_bad);
    g_test_add_func("/util/socket/inet-parse/ipv6-noport-bad",
                    test_inet_parse_ipv6_noport_bad);
    g_test_add_func("/util/socket/inet-parse/ipv6-empty-bad",
                    test_inet_parse_ipv6_empty_bad);
    g_test_add_func("/util/socket/inet-parse/hostname-good",
                    test_inet_parse_hostname_good);
    g_test_add_func("/util/socket/inet-parse/all-options-good",
                    test_inet_parse_all_options_good);
    g_test_add_func("/util/socket/inet-parse/all-bare-bool-good",
                    test_inet_parse_all_implicit_bool_good);

end:
    return g_test_run();
}
