#include "state.h"
#include <string.h>

void state_init(AppState *s)
{
    memset(s, 0, sizeof(*s));
    s->connected       = 0;
    s->connecting      = 0;
    s->in_channel      = 0;
    s->current_view    = VIEW_MENU;
    s->active_field    = FIELD_NONE;
    s->running         = 1;
    s->menu_selected   = 0;
    s->chat_cursor     = 0;
    s->chat_editing    = 0;
    s->msg_count       = 0;
    s->msg_scroll      = 0;
    s->input_len       = 0;
    s->to_backend_fd   = -1;
    s->from_backend_fd = -1;
    s->stopping        = 0;

    strncpy(s->server,   "inspire.tail0e8d21.ts.net", MAX_FIELD - 1);
    strncpy(s->port,     "8443",            15);
    strncpy(s->channel,  "#general",        127);
    strncpy(s->password, "abc123token",     127);
    strncpy(s->nickname, "irc_user",        63);
    strncpy(s->realname, "IRC User",        127);
    strncpy(s->username, "irc_user",        63);

    s->auto_reconnect  = 1;
    s->show_timestamps = 1;
    s->use_colors      = 1;
    s->show_join_parts = 1;
    s->show_info       = 1;

    pthread_mutex_init(&s->mutex, NULL);
}

void state_push_message(AppState *s, const char *msg)
{
    pthread_mutex_lock(&s->mutex);
    if (s->msg_count < MAX_MESSAGES) {
        strncpy(s->messages[s->msg_count], msg, MAX_MSG_LEN - 1);
        s->messages[s->msg_count][MAX_MSG_LEN - 1] = '\0';
        s->msg_count++;
    } else {
        memmove(s->messages[0], s->messages[1],
                sizeof(s->messages[0]) * (MAX_MESSAGES - 1));
        strncpy(s->messages[MAX_MESSAGES - 1], msg, MAX_MSG_LEN - 1);
        s->messages[MAX_MESSAGES - 1][MAX_MSG_LEN - 1] = '\0';
    }
    s->msg_scroll = s->msg_count;
    pthread_mutex_unlock(&s->mutex);
}
