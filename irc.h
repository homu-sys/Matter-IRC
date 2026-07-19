#ifndef IRC_H
#define IRC_H

#include "state.h"

int  irc_backend_start(AppState *s);
void irc_backend_stop(AppState *s);
void irc_backend_send(AppState *s, const char *json);
int  json_escape(char *dst, int dstsz, const char *src);

#endif
