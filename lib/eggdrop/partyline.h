#ifndef _EGG_PARTYLINE_H_
#define _EGG_PARTYLINE_H_

int partyline_init();
int partyline_on_connect(int idx, user_t *u, const char *nick, const char *ident, const char *host);
int partyline_is_command(const char *text);
int partyline_on_input(int pid, const char *text, int len);
int partyline_on_command(int pid, const char *text, int len);
int partyline_update_info(int pid, const char *ident, const char *host);
int partyline_write(int pid, const char *text);

#endif
