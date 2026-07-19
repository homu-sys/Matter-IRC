#ifndef UI_H
#define UI_H

#include "state.h"

void ui_init(void);
void ui_cleanup(void);
void ui_draw(AppState *s);
int  ui_handle_input(AppState *s, int ch);
void ui_apply_colors(AppState *s);

#endif
