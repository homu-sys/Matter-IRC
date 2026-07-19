#include "ui.h"
#include "irc.h"
#include "state.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define C_BORDER   1
#define C_TITLE    2
#define C_TEXT     3
#define C_DIM      4
#define C_FIELD    5
#define C_FIELD_A  6
#define C_SELECT   7
#define C_STATUS   8
#define C_ON       9
#define C_OFF     10
#define C_SEP     11
#define C_NICK    12
#define C_SYS     13

static int rows, cols;

static void get_exe_dir(char *buf, size_t len)
{
    ssize_t r = readlink("/proc/self/exe", buf, len - 1);
    if (r <= 0) {
        strncpy(buf, ".", len);
        return;
    }
    buf[r] = '\0';
    char *slash = strrchr(buf, '/');
    if (slash) *slash = '\0';
}

void ui_init(void)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(50);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(C_BORDER,  240,  -1);
        init_pair(C_TITLE,   255,  -1);
        init_pair(C_TEXT,    252,  -1);
        init_pair(C_DIM,     240,  -1);
        init_pair(C_FIELD,   252,  235);
        init_pair(C_FIELD_A, 255,  238);
        init_pair(C_SELECT,  235,  117);
        init_pair(C_STATUS,  252,  236);
        init_pair(C_ON,      114,  -1);
        init_pair(C_OFF,     167,  -1);
        init_pair(C_SEP,     238,  -1);
        init_pair(C_NICK,    173,  -1);
        init_pair(C_SYS,     243,  -1);
    }

    getmaxyx(stdscr, rows, cols);
}

void ui_cleanup(void) { endwin(); }

void ui_apply_colors(AppState *s)
{
    if (!has_colors()) return;
    int themes[][26] = {
        { 240,-1, 255,-1, 252,-1, 240,-1, 252,235, 255,238, 235,117, 252,236, 114,-1, 167,-1, 238,-1, 123,-1, 243,-1 },
        { 240,-1, 255,-1, 252,-1, 240,-1, 252,235, 255,238, 235,114, 252,236, 114,-1, 167,-1, 238,-1, 150,-1, 243,-1 },
        { 240,-1, 255,-1, 252,-1, 240,-1, 252,235, 255,238, 235, 75, 252,236, 114,-1, 167,-1, 238,-1,  75,-1, 243,-1 },
        { 240,-1, 255,-1, 252,-1, 240,-1, 252,235, 255,238, 235,141, 252,236, 114,-1, 167,-1, 238,-1, 141,-1, 243,-1 },
        { 240,-1, 245,-1, 248,-1, 240,-1, 248,237, 250,239, 237,245, 250,238, 145,-1, 138,-1, 239,-1, 248,-1, 244,-1 },
    };
    int idx = s->color_scheme;
    if (idx < 0 || idx > 4) idx = 0;
    int *t = themes[idx];
    init_pair(C_BORDER,  t[0],  t[1]);
    init_pair(C_TITLE,   t[2],  t[3]);
    init_pair(C_TEXT,    t[4],  t[5]);
    init_pair(C_DIM,     t[6],  t[7]);
    init_pair(C_FIELD,   t[8],  t[9]);
    init_pair(C_FIELD_A, t[10], t[11]);
    init_pair(C_SELECT,  t[12], t[13]);
    init_pair(C_STATUS,  t[14], t[15]);
    init_pair(C_ON,      t[16], t[17]);
    init_pair(C_OFF,     t[18], t[19]);
    init_pair(C_SEP,     t[20], t[21]);
    init_pair(C_NICK,    t[22], t[23]);
    init_pair(C_SYS,     t[24], t[25]);
}

static void box_draw(int y, int x, int h, int w)
{
    attron(COLOR_PAIR(C_BORDER));
    mvaddch(y,     x,       ACS_ULCORNER);
    mvaddch(y,     x+w-1,   ACS_URCORNER);
    mvaddch(y+h,   x,       ACS_LLCORNER);
    mvaddch(y+h,   x+w-1,   ACS_LRCORNER);
    mvhline(y,     x+1,     ACS_HLINE, w-2);
    mvhline(y+h,   x+1,     ACS_HLINE, w-2);
    mvvline(y+1,   x,       ACS_VLINE, h-2);
    mvvline(y+1,   x+w-1,   ACS_VLINE, h-2);
    attroff(COLOR_PAIR(C_BORDER));
}

static void cprint(int y, const char *text, int pair, int bold)
{
    int x = (cols - (int)strlen(text)) / 2;
    if (x < 0) x = 0;
    attron(COLOR_PAIR(pair));
    if (bold) attron(A_BOLD);
    mvprintw(y, x, "%s", text);
    if (bold) attroff(A_BOLD);
    attroff(COLOR_PAIR(pair));
}

static void btn_draw(int y, const char *text, int sel, int w)
{
    if (w <= 0) w = (int)strlen(text) + 6;
    int x   = (cols - w) / 2;
    int pad = (w - (int)strlen(text)) / 2;
    int p   = sel ? C_SELECT : C_FIELD;
    attron(COLOR_PAIR(p));
    mvhline(y, x, ' ', w);
    mvprintw(y, x + pad, "%s", text);
    attroff(COLOR_PAIR(p));
}

static void sep_draw(int y, int x, int w)
{
    attron(COLOR_PAIR(C_SEP));
    mvhline(y, x, ACS_HLINE, w);
    attroff(COLOR_PAIR(C_SEP));
}

static void draw_menu(AppState *s)
{
    int bw = 34, bh = 11;
    int bx = (cols - bw) / 2;
    int by = (rows - bh) / 2;

    box_draw(by, bx, bh, bw);
    cprint(by + 2, "IRC CLIENT", C_TITLE, 1);
    sep_draw(by + 3, bx + 2, bw - 4);

    const char *labels[] = { "Chat", "Settings" };
    for (int i = 0; i < 2; i++)
        btn_draw(by + 5 + i * 2, labels[i], s->menu_selected == i, 20);

    cprint(by + bh - 2, "j/k: move  enter: open  q: quit", C_DIM, 0);
}

static void draw_chat_setup(AppState *s)
{
    int bw = 44, bh = 15;
    int bx = (cols - bw) / 2;
    int by = (rows - bh) / 2;

    box_draw(by, bx, bh, bw);
    cprint(by + 2, "CONNECT", C_TITLE, 1);
    sep_draw(by + 3, bx + 2, bw - 4);

    struct { const char *label; const char *val; } fields[] = {
        { "Nickname",  s->nickname  },
        { "Password",  s->password  },
        { "Server",    s->server    },
        { "Port",      s->port      },
        { "Channel",   s->channel   },
    };

    int fw = 24;
    for (int i = 0; i < 5; i++) {
        int y   = by + 5 + i;
        int lw  = (int)strlen(fields[i].label);
        int tot = lw + 2 + fw;
        int fx  = (cols - tot) / 2;

        int is_sel  = (s->chat_cursor == i);
        int is_edit = (is_sel && s->chat_editing);

        attron(COLOR_PAIR(C_TEXT) | A_BOLD);
        mvprintw(y, fx, "%s:", fields[i].label);
        attroff(COLOR_PAIR(C_TEXT) | A_BOLD);

        int cp = is_edit ? C_FIELD_A : (is_sel ? C_SELECT : C_FIELD);
        attron(COLOR_PAIR(cp));
        mvhline(y, fx + lw + 2, ' ', fw);
        if (is_edit)
            mvprintw(y, fx + lw + 2, " %-*.*s", fw - 1, fw - 1, s->input_buf);
        else
            mvprintw(y, fx + lw + 2, " %-*.*s", fw - 1, fw - 1, fields[i].val);
        attroff(COLOR_PAIR(cp));
    }

    btn_draw(by + 11, "[ Connect ]", s->chat_cursor == 5, 18);

    cprint(by + bh - 2, "j/k: move  enter: edit/connect  esc: back", C_DIM, 0);
}

static void draw_chat_live(AppState *s)
{
    int bw = cols - 4, bh = rows - 2;
    int bx = 2, by = 1;

    box_draw(by, bx, bh, bw);

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "%s  --  %s:%s", s->channel, s->server, s->port);
    cprint(0, hdr, C_TITLE, 1);

    sep_draw(by + 1, bx + 1, bw - 2);

    int msg_top = by + 2;
    int msg_bot = by + bh - 3;
    int visible = msg_bot - msg_top + 1;
    if (visible < 1) visible = 1;

    pthread_mutex_lock(&s->mutex);
    int total  = s->msg_count;
    int scroll = s->msg_scroll;
    pthread_mutex_unlock(&s->mutex);

    int max_s = total - visible;
    if (max_s < 0) max_s = 0;
    if (scroll > max_s) scroll = max_s;
    if (scroll < 0) scroll = 0;

    pthread_mutex_lock(&s->mutex);
    for (int i = 0; i < visible; i++) {
        int idx = scroll + i;
        if (idx < total) {
            const char *m = s->messages[idx];
            int cp = C_TEXT;
            if (m[0] == '<') cp = C_NICK;
            else if (m[0] == '[') cp = C_NICK;
            else if (m[0] == '*') cp = C_SYS;
            else if (m[0] == '-') cp = C_SYS;
            attron(COLOR_PAIR(cp));
            mvprintw(msg_top + i, bx + 2, "%-.*s", bw - 4, m);
            attroff(COLOR_PAIR(cp));
        } else {
            mvhline(msg_top + i, bx + 1, ' ', bw - 2);
        }
    }
    pthread_mutex_unlock(&s->mutex);

    sep_draw(by + bh - 2, bx + 1, bw - 2);

    int iy = by + bh - 1;
    int iw = bw - 4;
    attron(COLOR_PAIR(C_DIM));
    mvprintw(iy, bx + 1, "> ");
    attroff(COLOR_PAIR(C_DIM));
    attron(COLOR_PAIR(C_FIELD));
    mvhline(iy, bx + 3, ' ', iw);
    mvprintw(iy, bx + 3, "%-*.*s", iw, iw, s->input_buf);
    attroff(COLOR_PAIR(C_FIELD));

    const char *st = s->connected && s->in_channel ? " joined "  :
                     s->connected                   ? " joining " :
                     s->connecting                  ? " connecting " :
                                                     " disconnected ";
    attron(COLOR_PAIR(C_STATUS));
    mvprintw(rows - 1, 0, "%-*s", cols, st);
    attroff(COLOR_PAIR(C_STATUS));
}

static void draw_chat(AppState *s)
{
    if (s->connected || s->connecting)
        draw_chat_live(s);
    else
        draw_chat_setup(s);
}

static void draw_settings(AppState *s)
{
    int bw = 46, bh = 17;
    int bx = (cols - bw) / 2;
    int by = (rows - bh) / 2;

    box_draw(by, bx, bh, bw);
    cprint(by + 2, "SETTINGS", C_TITLE, 1);
    sep_draw(by + 3, bx + 2, bw - 4);

    const char *opts[] = {
        "Auto Reconnect",
        "Show Timestamps",
        "Use Colors",
        "Show Joins/Parts",
        "Show Info"
    };
    int vals[] = {
        s->auto_reconnect,
        s->show_timestamps,
        s->use_colors,
        s->show_join_parts,
        s->show_info
    };

    int r = by + 5;
    for (int i = 0; i < 5; i++) {
        int sel = (s->chat_cursor == i);

        int cp = sel ? C_SELECT : C_TEXT;
        attron(COLOR_PAIR(cp));
        mvprintw(r, bx + 4, "%s", opts[i]);
        attroff(COLOR_PAIR(cp));

        int tp = vals[i] ? C_ON : C_OFF;
        attron(COLOR_PAIR(tp) | A_BOLD);
        mvprintw(r, bx + bw - 10, "%s", vals[i] ? "ON " : "OFF");
        attroff(COLOR_PAIR(tp) | A_BOLD);

        r++;
    }

    r++;
    btn_draw(r, "[Back]", s->chat_cursor == 5, 16);
}

void ui_draw(AppState *s)
{
    getmaxyx(stdscr, rows, cols);
    erase();

    switch (s->current_view) {
    case VIEW_MENU:     draw_menu(s);     break;
    case VIEW_CHAT:     draw_chat(s);     break;
    case VIEW_SETTINGS: draw_settings(s); break;
    }

    refresh();
}

static void field_edit_input(char *buf, int bufsize, int *len, int ch)
{
    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (*len > 0) { (*len)--; buf[*len] = '\0'; }
    } else if (ch >= 32 && ch < 127 && *len < bufsize - 1) {
        buf[(*len)++] = (char)ch;
        buf[*len] = '\0';
    }
}

static void chat_setup_input(AppState *s, int ch)
{
    if (s->chat_editing) {
        if (ch == 27 || ch == '\t' || ch == '\n' || ch == KEY_ENTER) {
            char *targets[] = { s->nickname, s->password,
                                s->server, s->port, s->channel };
            int sizes[]     = { 64, 128, MAX_FIELD, 16, 128 };
            if (s->chat_cursor < 5) {
                strncpy(targets[s->chat_cursor], s->input_buf, sizes[s->chat_cursor] - 1);
                targets[s->chat_cursor][sizes[s->chat_cursor] - 1] = '\0';
            }
            s->input_len = 0;
            s->input_buf[0] = '\0';
            s->chat_editing = 0;
            return;
        }
        field_edit_input(s->input_buf, MAX_FIELD, &s->input_len, ch);
        return;
    }

    switch (ch) {
    case KEY_UP: case 'k':
        if (s->chat_cursor > 0) s->chat_cursor--;
        break;
    case KEY_DOWN: case 'j':
        if (s->chat_cursor < 5) s->chat_cursor++;
        break;
    case '\n': case KEY_ENTER:
        if (s->chat_cursor < 5) {
            const char *srcs[] = { s->nickname, s->password,
                                   s->server, s->port, s->channel };
            strncpy(s->input_buf, srcs[s->chat_cursor], MAX_FIELD - 1);
            s->input_buf[MAX_FIELD - 1] = '\0';
            s->input_len = (int)strlen(s->input_buf);
            s->chat_editing = 1;
        } else {
            irc_backend_start(s);
            {
                char cmd[MAX_MSG_LEN * 4];
                char *p = cmd;
                int rem = sizeof(cmd);
                int n;
                p[0] = '\0';
                n = snprintf(p, rem, "{\"cmd\":\"connect\","
                    "\"server\":\""); p += n; rem -= n;
                json_escape(p, rem, s->server); n = (int)strlen(p); p += n; rem -= n;
                n = snprintf(p, rem, "\",\"port\":\""); p += n; rem -= n;
                json_escape(p, rem, s->port); n = (int)strlen(p); p += n; rem -= n;
                n = snprintf(p, rem, "\",\"channel\":\""); p += n; rem -= n;
                json_escape(p, rem, s->channel); n = (int)strlen(p); p += n; rem -= n;
                n = snprintf(p, rem, "\",\"nick\":\""); p += n; rem -= n;
                json_escape(p, rem, s->nickname); n = (int)strlen(p); p += n; rem -= n;
                n = snprintf(p, rem, "\",\"password\":\""); p += n; rem -= n;
                json_escape(p, rem, s->password); n = (int)strlen(p); p += n; rem -= n;
                snprintf(p, rem, "\"}");
                irc_backend_send(s, cmd);
            }
        }
        break;
    case 27: case 'q':
        s->current_view = VIEW_MENU;
        s->chat_cursor = 0;
        s->chat_editing = 0;
        break;
    }
}

static void chat_live_input(AppState *s, int ch)
{
    switch (ch) {
    case '\n': case KEY_ENTER:
        if (s->input_len > 0) {
            if (s->input_buf[0] == '/') {
                if (strcmp(s->input_buf, "/update") == 0) {
                    state_push_message(s, "*** Updating...");
                    char exe_dir[512];
                    get_exe_dir(exe_dir, sizeof(exe_dir));
                    char cmd[768];
                    snprintf(cmd, sizeof(cmd), "sh -c 'cd \"%s\" && git stash -u && git pull --ff-only && git stash pop 2>/dev/null'", exe_dir);
                    FILE *fp = popen(cmd, "r");
                    if (fp) {
                        char ubuf[256];
                        while (fgets(ubuf, sizeof(ubuf), fp)) {
                            ubuf[strcspn(ubuf, "\n")] = 0;
                            if (ubuf[0]) {
                                char msg[280];
                                snprintf(msg, sizeof(msg), "*** %s", ubuf);
                                state_push_message(s, msg);
                            }
                        }
                        int rc = pclose(fp);
                        if (rc == 0) {
                            state_push_message(s, "*** Done! Restart for latest version.");
                            fp = popen("git -C /home/homu/irc rev-parse HEAD 2>/dev/null", "r");
                            if (fp) {
                                char sha[64] = "";
                                if (fgets(sha, sizeof(sha), fp)) sha[strcspn(sha, "\n")] = 0;
                                pclose(fp);
                                if (sha[0]) {
                                    char path[512];
                                    snprintf(path, sizeof(path), "%s/.config/matterirc/.last_commit", getenv("HOME"));
                                    FILE *f = fopen(path, "w");
                                    if (f) { fprintf(f, "%s\n", sha); fclose(f); }
                                }
                            }
                        } else {
                            state_push_message(s, "*** Update failed.");
                        }
                    }
                } else if (strcmp(s->input_buf, "/themes") == 0) {
                    state_push_message(s, "*** Available themes:");
                    state_push_message(s, "  1. Cyan");
                    state_push_message(s, "  2. Green");
                    state_push_message(s, "  3. Blue");
                    state_push_message(s, "  4. Purple");
                    state_push_message(s, "  5. Muted");
                    state_push_message(s, "*** Use /theme <name> to switch.");
                } else if (strncmp(s->input_buf, "/theme ", 7) == 0) {
                    const char *name = s->input_buf + 7;
                    int found = -1;
                    const char *names[] = { "cyan", "green", "blue", "purple", "muted" };
                    for (int i = 0; i < 5; i++) {
                        if (strcasecmp(name, names[i]) == 0) { found = i; break; }
                    }
                    if (found >= 0) {
                        s->color_scheme = found;
                        ui_apply_colors(s);
                        char msg[128];
                        snprintf(msg, sizeof(msg), "*** Theme set to %s", name);
                        state_push_message(s, msg);
                    } else {
                        state_push_message(s, "*** Unknown theme. Type /themes to see available themes.");
                    }
                } else if (strcmp(s->input_buf, "/help") == 0) {
                    state_push_message(s, "*** Commands:");
                    state_push_message(s, "  /help      - Show this list");
                    state_push_message(s, "  /update    - Pull latest from git");
                    state_push_message(s, "  /themes    - List color themes");
                    state_push_message(s, "  /theme <n> - Switch theme (cyan/green/blue/purple/muted)");
                    state_push_message(s, "  /quit      - Disconnect and return to menu");
                } else if (strcmp(s->input_buf, "/quit") == 0) {
                    irc_backend_stop(s);
                    s->current_view = VIEW_MENU;
                    s->chat_cursor = 0;
                    s->chat_editing = 0;
                } else {
                    char esc[MAX_MSG_LEN], cmd[MAX_MSG_LEN * 2];
                    json_escape(esc, sizeof(esc), s->input_buf + 1);
                    snprintf(cmd, sizeof(cmd), "{\"cmd\":\"raw\",\"line\":\"%s\"}", esc);
                    irc_backend_send(s, cmd);
                }
            } else if (s->in_channel) {
                char es_tgt[MAX_FIELD], es_txt[MAX_MSG_LEN];
                char cmd[MAX_MSG_LEN * 2];
                json_escape(es_tgt, sizeof(es_tgt), s->channel);
                json_escape(es_txt, sizeof(es_txt), s->input_buf);
                snprintf(cmd, sizeof(cmd), "{\"cmd\":\"msg\",\"target\":\"%s\",\"text\":\"%s\"}",
                         es_tgt, es_txt);
                irc_backend_send(s, cmd);
            }
            s->input_len = 0;
            s->input_buf[0] = '\0';
        }
        break;
    case KEY_BACKSPACE: case 127: case '\b':
        if (s->input_len > 0) {
            s->input_len--;
            s->input_buf[s->input_len] = '\0';
        }
        break;
    case KEY_UP:
        if (s->msg_scroll > 0) s->msg_scroll--;
        break;
    case KEY_DOWN: {
        pthread_mutex_lock(&s->mutex);
        int total = s->msg_count;
        pthread_mutex_unlock(&s->mutex);
        int mx = total - (rows - 6);
        if (mx < 0) mx = 0;
        if (s->msg_scroll < mx) s->msg_scroll++;
        break;
    }
    case 27:
        irc_backend_stop(s);
        s->chat_cursor = 0;
        s->chat_editing = 0;
        s->input_len = 0;
        s->input_buf[0] = '\0';
        break;
    default:
        if (ch >= 32 && ch < 127 && s->input_len < MAX_FIELD - 1) {
            s->input_buf[s->input_len++] = (char)ch;
            s->input_buf[s->input_len] = '\0';
        }
        break;
    }
}

static void settings_input(AppState *s, int ch)
{
    switch (ch) {
    case KEY_UP: case 'k':
        if (s->chat_cursor > 0) s->chat_cursor--;
        break;
    case KEY_DOWN: case 'j':
        if (s->chat_cursor < 5) s->chat_cursor++;
        break;
    case '\n': case ' ':
        switch (s->chat_cursor) {
        case 0: s->auto_reconnect  = !s->auto_reconnect;  break;
        case 1: s->show_timestamps = !s->show_timestamps; break;
        case 2: s->use_colors      = !s->use_colors;      break;
        case 3: s->show_join_parts = !s->show_join_parts; break;
        case 4: s->show_info       = !s->show_info;       break;
        case 5:
            s->current_view = VIEW_MENU;
            s->chat_cursor = 0;
            break;
        }
        break;
    case 27: case 'q':
        s->current_view = VIEW_MENU;
        s->chat_cursor = 0;
        break;
    }
}

int ui_handle_input(AppState *s, int ch)
{
    if (ch == ERR) return 0;

    if (ch == 'q' && s->current_view == VIEW_MENU
        && !s->chat_editing) {
        s->running = 0;
        return 1;
    }

    switch (s->current_view) {
    case VIEW_MENU:
        switch (ch) {
        case KEY_UP: case 'k':
            if (s->menu_selected > 0) s->menu_selected--;
            break;
        case KEY_DOWN: case 'j':
            if (s->menu_selected < 1) s->menu_selected++;
            break;
        case '\n': case KEY_ENTER: case ' ':
            switch (s->menu_selected) {
            case 0:
                s->current_view = VIEW_CHAT;
                s->chat_cursor = 0;
                s->chat_editing = 0;
                break;
            case 1:
                s->current_view = VIEW_SETTINGS;
                s->chat_cursor = 0;
                s->chat_editing = 0;
                break;
            }
            break;
        case 'c':
            s->current_view = VIEW_CHAT;
            s->chat_cursor = 0;
            s->chat_editing = 0;
            break;
        case 's':
            s->current_view = VIEW_SETTINGS;
            s->chat_cursor = 0;
            s->chat_editing = 0;
            break;
        }
        break;

    case VIEW_CHAT:
        if (s->connected || s->connecting)
            chat_live_input(s, ch);
        else
            chat_setup_input(s, ch);
        break;

    case VIEW_SETTINGS:
        settings_input(s, ch);
        break;
    }

    return 0;
}
