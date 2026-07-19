#include "state.h"
#include "ui.h"
#include "irc.h"

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void ensure_config_dir(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/matterirc", getenv("HOME"));
    mkdir(path, 0755);
}

static int read_last_sha(char *buf, int bufsz)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/matterirc/.last_commit", getenv("HOME"));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(buf, bufsz, f)) { fclose(f); return 0; }
    buf[strcspn(buf, "\n")] = 0;
    fclose(f);
    return buf[0] != '\0';
}

static void write_last_sha(const char *sha)
{
    ensure_config_dir();
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/matterirc/.last_commit", getenv("HOME"));
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s\n", sha); fclose(f); }
}

static void save_update_state(int updated)
{
    ensure_config_dir();
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/matterirc/.update_state", getenv("HOME"));
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%d\n", updated); fclose(f); }
}

static int read_update_state(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/matterirc/.update_state", getenv("HOME"));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[16] = "";
    fgets(buf, sizeof(buf), f);
    fclose(f);
    return atoi(buf);
}

static void check_updates(AppState *s)
{
    FILE *fp;
    char buf[256];
    char cmd[512];

    ensure_config_dir();

    snprintf(cmd, sizeof(cmd), "git -C /home/homu/irc rev-parse HEAD 2>/dev/null");
    fp = popen(cmd, "r");
    if (!fp) return;
    buf[0] = '\0';
    if (fgets(buf, sizeof(buf), fp)) buf[strcspn(buf, "\n")] = 0;
    pclose(fp);
    if (!buf[0]) return;
    char local_sha[64];
    strncpy(local_sha, buf, sizeof(local_sha) - 1);
    local_sha[sizeof(local_sha) - 1] = '\0';

    snprintf(cmd, sizeof(cmd), "git -C /home/homu/irc fetch origin 2>/dev/null");
    fp = popen(cmd, "r");
    if (fp) pclose(fp);

    snprintf(cmd, sizeof(cmd), "git -C /home/homu/irc rev-parse origin/main 2>/dev/null");
    fp = popen(cmd, "r");
    if (!fp) return;
    buf[0] = '\0';
    if (fgets(buf, sizeof(buf), fp)) buf[strcspn(buf, "\n")] = 0;
    pclose(fp);
    if (!buf[0]) return;
    char remote_sha[64];
    strncpy(remote_sha, buf, sizeof(remote_sha) - 1);
    remote_sha[sizeof(remote_sha) - 1] = '\0';

    if (strcmp(local_sha, remote_sha) == 0) {
        write_last_sha(remote_sha);
        return;
    }

    char last_prompted[64] = "";
    read_last_sha(last_prompted, sizeof(last_prompted));
    int prev = read_update_state();

    if (strcmp(last_prompted, remote_sha) == 0 && prev == 1) return;

    write_last_sha(remote_sha);
    save_update_state(0);

    state_push_message(s, "*** Update available! Type /update to install.");

    snprintf(cmd, sizeof(cmd), "git -C /home/homu/irc rev-parse --short origin/main 2>/dev/null");
    fp = popen(cmd, "r");
    if (fp) {
        buf[0] = '\0';
        if (fgets(buf, sizeof(buf), fp)) buf[strcspn(buf, "\n")] = 0;
        pclose(fp);
        if (buf[0]) {
            char msg[128];
            snprintf(msg, sizeof(msg), "*** Latest: %s", buf);
            state_push_message(s, msg);
        }
    }
}

int main(void)
{
    AppState state;
    state_init(&state);

    ui_init();

    check_updates(&state);

    while (state.running) {
        ui_draw(&state);
        int ch = getch();
        if (ui_handle_input(&state, ch))
            break;
    }

    irc_backend_stop(&state);
    ui_cleanup();
    return 0;
}
