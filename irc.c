#include "irc.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

int json_escape(char *dst, int dstsz, const char *src)
{
    int o = 0;
    for (int i = 0; src[i] && o < dstsz - 2; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            dst[o++] = '\\';
            dst[o++] = c;
        } else if (c == '\n') {
            dst[o++] = '\\'; dst[o++] = 'n';
        } else if (c == '\r') {
            dst[o++] = '\\'; dst[o++] = 'r';
        } else if (c == '\t') {
            dst[o++] = '\\'; dst[o++] = 't';
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = '\0';
    return o;
}

#define MAX_LINE 4096

void irc_backend_send(AppState *s, const char *json)
{
    if (s->to_backend_fd < 0) return;
    dprintf(s->to_backend_fd, "%s\n", json);
}

static void push_event(AppState *s, const char *json)
{
    char ev[64] = "";
    const char *p = strstr(json, "\"event\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p == ' ') p++;
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < (int)sizeof(ev) - 1)
                    ev[i++] = *p++;
                ev[i] = '\0';
            }
        }
    }

    char val[MAX_MSG_LEN] = "";
    #define GET_VAL(key) do { \
        val[0] = '\0'; \
        char _needle[128]; \
        snprintf(_needle, sizeof(_needle), "\"%s\"", key); \
        const char *_vp = strstr(json, _needle); \
        if (_vp) { \
            _vp = strchr(_vp, ':'); \
            if (_vp) { \
                _vp++; \
                while (*_vp == ' ') _vp++; \
                if (*_vp == '"') { \
                    _vp++; \
                    int _i = 0; \
                    while (*_vp && *_vp != '"' && _i < (int)sizeof(val) - 1) { \
                        if (*_vp == '\\' && *(_vp+1)) { _vp++; } \
                        val[_i++] = *_vp++; \
                    } \
                    val[_i] = '\0'; \
                } \
            } \
        } \
    } while(0)

    if (strcmp(ev, "connecting") == 0) {
        pthread_mutex_lock(&s->mutex);
        s->connecting = 1;
        pthread_mutex_unlock(&s->mutex);
        GET_VAL("server");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** Connecting to %s ...", val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "connected") == 0) {
        pthread_mutex_lock(&s->mutex);
        s->connected = 1;
        s->connecting = 0;
        pthread_mutex_unlock(&s->mutex);
        state_push_message(s, "*** Connected to server");

    } else if (strcmp(ev, "joined") == 0) {
        pthread_mutex_lock(&s->mutex);
        s->in_channel = 1;
        pthread_mutex_unlock(&s->mutex);
        GET_VAL("channel");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** Joined %s", val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "message") == 0) {
        GET_VAL("sender");
        char sender[MAX_MSG_LEN];
        strncpy(sender, val, sizeof(sender) - 1);
        sender[sizeof(sender) - 1] = '\0';

        GET_VAL("text");
        char text[MAX_MSG_LEN];
        strncpy(text, val, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';

        char buf[MAX_MSG_LEN];
        if (s->show_timestamps) {
            time_t now = time(NULL);
            struct tm *tm = localtime(&now);
            char ts[16];
            strftime(ts, sizeof(ts), "%H:%M", tm);
            snprintf(buf, sizeof(buf), "[%s] <%s> %s", ts, sender, text);
        } else {
            snprintf(buf, sizeof(buf), "<%s> %s", sender, text);
        }
        state_push_message(s, buf);

    } else if (strcmp(ev, "join") == 0) {
        if (!s->show_join_parts) return;
        GET_VAL("nick");
        char jnick[MAX_MSG_LEN];
        strncpy(jnick, val, sizeof(jnick) - 1);
        jnick[sizeof(jnick) - 1] = '\0';
        GET_VAL("channel");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** %s has joined %s", jnick, val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "part") == 0) {
        if (!s->show_join_parts) return;
        GET_VAL("nick");
        char pnick[MAX_MSG_LEN];
        strncpy(pnick, val, sizeof(pnick) - 1);
        pnick[sizeof(pnick) - 1] = '\0';
        GET_VAL("reason");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** %s has left (%s)", pnick, val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "quit") == 0) {
        if (!s->show_join_parts) return;
        GET_VAL("nick");
        char qnick[MAX_MSG_LEN];
        strncpy(qnick, val, sizeof(qnick) - 1);
        qnick[sizeof(qnick) - 1] = '\0';
        GET_VAL("reason");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** %s has quit (%s)", qnick, val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "nick") == 0) {
        GET_VAL("old");
        char oldn[MAX_MSG_LEN];
        strncpy(oldn, val, sizeof(oldn) - 1);
        oldn[sizeof(oldn) - 1] = '\0';
        GET_VAL("new");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** %s is now known as %s", oldn, val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "system") == 0) {
        GET_VAL("text");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** %s", val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "error") == 0) {
        GET_VAL("text");
        char buf[MAX_MSG_LEN];
        snprintf(buf, sizeof(buf), "*** Error: %s", val);
        state_push_message(s, buf);

    } else if (strcmp(ev, "disconnected") == 0) {
        pthread_mutex_lock(&s->mutex);
        s->connected = 0;
        s->connecting = 0;
        s->in_channel = 0;
        pthread_mutex_unlock(&s->mutex);
        state_push_message(s, "*** Disconnected");
    }

    #undef GET_VAL
}

static void *reader_fn(void *arg)
{
    AppState *s = (AppState *)arg;
    char line[MAX_LINE];
    int pos = 0;

    while (1) {
        char c;
        ssize_t n = read(s->from_backend_fd, &c, 1);
        if (n <= 0) break;

        if (c == '\n') {
            line[pos] = '\0';
            if (pos > 0) push_event(s, line);
            pos = 0;
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }

    pthread_mutex_lock(&s->mutex);
    s->connected = 0;
    s->connecting = 0;
    s->in_channel = 0;
    int was_stopping = s->stopping;
    pthread_mutex_unlock(&s->mutex);
    if (!was_stopping)
        state_push_message(s, "*** Backend disconnected");

    return NULL;
}

int irc_backend_start(AppState *s)
{
    int to_child[2];
    int from_child[2];

    if (pipe(to_child) < 0 || pipe(from_child) < 0) {
        state_push_message(s, "*** Failed to create pipes");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        state_push_message(s, "*** Failed to fork");
        return -1;
    }

    if (pid == 0) {
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[0]);
        close(from_child[1]);

        execlp("python3", "python3", "irc_backend.py", (char *)NULL);
        execlp("python", "python", "irc_backend.py", (char *)NULL);
        _exit(127);
    }

    close(to_child[0]);
    close(from_child[1]);
    s->to_backend_fd   = to_child[1];
    s->from_backend_fd = from_child[0];

    pthread_create(&s->reader_thread, NULL, reader_fn, s);
    pthread_detach(s->reader_thread);

    return 0;
}

void irc_backend_stop(AppState *s)
{
    pthread_mutex_lock(&s->mutex);
    s->stopping = 1;
    pthread_mutex_unlock(&s->mutex);

    irc_backend_send(s, "{\"cmd\":\"quit\"}");

    if (s->to_backend_fd >= 0) {
        close(s->to_backend_fd);
        s->to_backend_fd = -1;
    }
    if (s->from_backend_fd >= 0) {
        close(s->from_backend_fd);
        s->from_backend_fd = -1;
    }

    pthread_mutex_lock(&s->mutex);
    s->connected = 0;
    s->connecting = 0;
    s->in_channel = 0;
    pthread_mutex_unlock(&s->mutex);
}
