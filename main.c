#include "state.h"
#include "ui.h"
#include "irc.h"

#include <ncurses.h>
#include <stdlib.h>

int main(void)
{
    AppState state;
    state_init(&state);

    ui_init();

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
