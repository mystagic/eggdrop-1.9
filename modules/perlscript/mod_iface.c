/*
 * mod_iface.c --
 */
/*
 * Copyright (C) 2002 Eggheads Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef lint
static const char rcsid[] = "$Id: mod_iface.c,v 1.10 2002/10/11 09:00:34 stdarg Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include "lib/eggdrop/module.h"
#include <eggdrop/eggdrop.h>

#define MODULE_NAME "perlscript"

static eggdrop_t *egg = NULL;

/* Stuff from perlscript.c. */
extern int perlscript_init();
extern int perlscript_destroy();
extern script_module_t my_script_interface;
extern char *real_perl_cmd(char *text);

/* Log an error message. */
int log_error(char *msg)
{
	putlog(LOG_MISC, "*", "Perl error: %s", msg);
	return(0);
}

/* A stub for the .perl command. */
static int dcc_cmd_perl(struct userrec *u, int idx, char *text)
{
	char *retval;

	/* You must be owner to use this command. */
	if (!isowner(dcc[idx].nick)) return(0);

	retval = real_perl_cmd(text);
	dprintf(idx, "%s", retval);
	free(retval);
	return(0);
}

static bind_list_t my_dcc_cmds[] = {
	{"perl", (Function) dcc_cmd_perl},
	{0}
};

EXPORT_SCOPE char *perlscript_LTX_start();
static char *perlscript_close();

static Function perlscript_table[] = {
	(Function) perlscript_LTX_start,
	(Function) perlscript_close,
	(Function) 0,
	(Function) 0
};

char *perlscript_LTX_start(eggdrop_t *eggdrop)
{
	egg = eggdrop;

	module_register("perlscript", perlscript_table, 1, 2);
	if (!module_depend("perlscript", "eggdrop", 107, 0)) {
		module_undepend("perlscript");
		return "This module requires eggdrop1.7.0 of later";
	}

	/* Initialize interpreter. */
	perlscript_init();

	script_register_module(&my_script_interface);
	script_playback(&my_script_interface);

	bind_add_list("dcc", my_dcc_cmds);
	return(NULL);
}

static char *perlscript_close()
{
	bind_rem_list("dcc", my_dcc_cmds);

	/* Destroy interpreter. */
	perlscript_destroy();

	script_unregister_module(&my_script_interface);

	module_undepend("perlscript");
	return(NULL);
}

