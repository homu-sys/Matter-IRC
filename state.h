#ifndef STATE_H
#define STATE_H

#include <pthread.h>

#define MAX_MESSAGES 500
#define MAX_MSG_LEN  512
#define MAX_FIELD    256

typedef enum {
    VIEW_MENU,
    VIEW_CHAT,
    VIEW_SETTINGS
} View;

typedef enum {
    FIELD_NONE,
    FIELD_SERVER,
    FIELD_PORT,
    FIELD_CHANNEL,
    FIELD_PASSWORD
} ActiveField;

typedef struct {
    int         connected;
    int         connecting;
    int         in_channel;
    char        server[MAX_FIELD];
    char        port[16];
    char        channel[128];
    char        password[128];

    char        nickname[64];
    char        realname[128];

    int         auto_reconnect;
    int         show_timestamps;
    int         use_colors;
    int         show_join_parts;
    int         show_info;
    int         color_scheme;

    char        messages[MAX_MESSAGES][MAX_MSG_LEN];
    int         msg_count;
    int         msg_scroll;

    View        current_view;
    ActiveField active_field;
    int         running;
    int         menu_selected;

    int         chat_cursor;
    int         chat_editing;

    char        input_buf[MAX_FIELD];
    int         input_len;

    int         to_backend_fd;
    int         from_backend_fd;
    int         stopping;
    pthread_t   reader_thread;
    pthread_mutex_t mutex;
} AppState;

void state_init(AppState *s);
void state_push_message(AppState *s, const char *msg);

#endif
