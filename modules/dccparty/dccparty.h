/* dccparty.h: header for dccparty.c
 *
 * Copyright (C) 2003, 2004 Eggheads Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: dccparty.h,v 1.4 2004/06/29 21:28:17 stdarg Exp $
 */

#ifndef _EGG_MOD_DCCPARTY_DCCPARTY_H_
#define _EGG_MOD_DCCPARTY_DCCPARTY_H_

/* Possible states of the connection. */
#define STATE_RESOLVE	0
#define STATE_NICKNAME	1
#define STATE_PASSWORD	2
#define STATE_PARTYLINE	3

/* Flags for sessions. */
#define STEALTH_LOGIN	1

typedef struct {
	/* Pointer to our entry in the partyline. */
	partymember_t *party;

	/* Who we're connected to. */
	user_t *user;
	char *nick, *ident, *host, *ip;
	int port;
	int idx;
	int pid;

	/* Flags for this connection. */
	int flags;

	/* Connection state we're in. */
	int state, count;
	int ident_id, dns_id;
} dcc_session_t;

typedef struct {
	char *vhost;
	int port;
	int stealth;
	int max_retries;
} dcc_config_t;

extern dcc_config_t dcc_config;

#endif /* !_EGG_MOD_DCCPARTY_DCCPARTY_H_ */
