/*
 * perlscript.c --
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
static const char rcsid[] = "$Id: perlscript.c,v 1.19 2002/05/12 05:59:51 stdarg Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#undef DEBUG
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "lib/egglib/mstack.h"
#include "lib/egglib/msprintf.h"
#include <eggdrop/eggdrop.h>

static PerlInterpreter *ginterp; /* Our global interpreter. */

static XS(my_command_handler);
static SV *c_to_perl_var(script_var_t *v);
static int perl_to_c_var(SV *sv, script_var_t *var, int type);

static int my_load_script(void *ignore, char *fname);
static int my_link_var(void *ignore, script_linked_var_t *linked_var);
static int my_unlink_var(void *ignore, script_linked_var_t *linked_var);
static int my_create_command(void *ignore, script_raw_command_t *info);
static int my_delete_command(void *ignore, script_raw_command_t *info);
static int my_get_arg(void *ignore, script_args_t *args, int num, script_var_t *var, int type);

script_module_t my_script_interface = {
	"Perl", NULL,
	my_load_script,
	my_link_var, my_unlink_var,
	my_create_command, my_delete_command,
	my_get_arg
};

typedef struct {
	SV **sp;
	SV **mark;
	I32 ax;
	I32 items;
} my_args_data_t;

/* Functions from mod_iface.c */
extern void *fake_get_user_by_handle(char *handle);
extern char *fake_get_handle(void *user_record);
extern int log_error(char *msg);
 
static int my_load_script(void *ignore, char *fname)
{
	FILE *fp;
	char *data;
	int size, len;

	/* Check the filename and make sure it ends in .pl */
	len = strlen(fname);
	if (len < 3 || fname[len-1] != 'l' || fname[len-2] != 'p' || fname[len-3] != '.') {
		/* Nope, not ours. */
		return(0);
	}

	fp = fopen(fname, "r");
	if (!fp) return (0);
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	data = (char *)malloc(size + 1);
	fseek(fp, 0, SEEK_SET);
	fread(data, size, 1, fp);
	data[size] = 0;
	fclose(fp);
	eval_pv(data, TRUE);
	if (SvTRUE(ERRSV)) {
		char *msg;
		int len;

		msg = SvPV(ERRSV, len);
		log_error(msg);
	}
	free(data);
	return(0);
}

static void set_linked_var(script_linked_var_t *linked_var, SV *sv)
{
	SV *newsv;
	script_var_t var;

	var.type = linked_var->type & SCRIPT_TYPE_MASK;
	var.len = -1;
	var.value = *(void **)linked_var->value;

	newsv = c_to_perl_var(&var);
	sv_setsv(sv, newsv);
	SvREFCNT_dec(newsv);
}

static int linked_var_get(SV *sv, MAGIC *mg)
{
	script_linked_var_t *linked_var = (script_linked_var_t *)mg->mg_ptr;

	if (linked_var->callbacks && linked_var->callbacks->on_read) {
		int r = (linked_var->callbacks->on_read)(linked_var);
		if (r) return(r);
	}
	set_linked_var(linked_var, sv);
	return(0);
}

static int linked_var_set(SV *sv, MAGIC *mg)
{
	script_linked_var_t *linked_var = (script_linked_var_t *)mg->mg_ptr;
	script_var_t newvalue = {0};

	perl_to_c_var(sv, &newvalue, linked_var->type);
	if (linked_var->callbacks && linked_var->callbacks->on_write) {
		(linked_var->callbacks->on_write)(linked_var, &newvalue);
		return(0);
	}
	switch (linked_var->type) {
		case SCRIPT_UNSIGNED:
		case SCRIPT_INTEGER:
			/* linked_var->value is a pointer to an int/uint */
			*(int *)(linked_var->value) = (int) newvalue.value;
			break;
		case SCRIPT_STRING: {
			/* linked_var->value is a pointer to a (char *) */
			char **charptr = (char **)(linked_var->value);

			/* Free the old value. */
			if (*charptr) free(*charptr);

			/* Set the new value. */
			if (newvalue.type & SCRIPT_FREE) *charptr = newvalue.value;
			else *charptr = strdup(newvalue.value);
			break;
		}
	}
	return(0);
}

/* This function creates the Perl <-> C variable linkage for reads/writes. */
static int my_link_var(void *ignore, script_linked_var_t *linked_var)
{
	MAGIC *mg;
	SV *sv;
	char *name;

	/* Figure out the perl name of the variable. */
	if (linked_var->class && strlen(linked_var->class)) name = msprintf("%s::%s", linked_var->class, linked_var->name);
	else name = strdup(linked_var->name);

	/* Get a pointer to the sv, creating it if necessary. */
	sv = get_sv(name, TRUE);
	free(name);

	/* Set the initial value before we do our magic. */
	set_linked_var(linked_var, sv);

	/* Create the magic virtual table, which hooks in our callbacks.
		We put a pointer to linked_var as the name field, with a length
		of -1, which tells perl to just store the pointer. Then we can
		use it later to know which variable is being read/written. */
	sv_magic(sv, NULL, 'U', (char *)linked_var, sizeof(*linked_var));

	/* This part is based on code generated by SWIG. */
	mg = mg_find(sv, 'U');
	mg->mg_virtual = (MGVTBL *)calloc(1, sizeof(MGVTBL));
	mg->mg_virtual->svt_get = linked_var_get;
	mg->mg_virtual->svt_set = linked_var_set;

	return(0);
}

static int my_unlink_var(void *ignore, script_linked_var_t *linked_var)
{
	MAGIC *mg;
	SV *sv;
	char *name;

	/* Figure out the perl name of the variable. */
	if (linked_var->class && strlen(linked_var->class)) name = msprintf("%s::%s", linked_var->class, linked_var->name);
	else name = strdup(linked_var->name);

	/* Get a pointer to the sv, creating it if necessary. */
	sv = get_sv(name, FALSE);
	free(name);

	if (!sv) return(0);

	mg = mg_find(sv, 'U');
	free(mg->mg_virtual);
	mg->mg_virtual = NULL;

	mg_free(sv);
	mg_clear(sv);
	SvREFCNT_dec(sv);

	return(0);
}

static int my_perl_callbacker(script_callback_t *me, ...)
{
	int retval, i, n, count;
	script_var_t var;
	SV *arg;
	int *al;
	dSP;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	al = (int *)&me;
	al++;
	if (me->syntax) n = strlen(me->syntax);
	else n = 0;
	for (i = 0; i < n; i++) {
		var.type = me->syntax[i];
		var.value = (void *)al[i];
		var.len = -1;
		arg = c_to_perl_var(&var);
		XPUSHs(sv_2mortal(arg));
	}
	PUTBACK;

	count = call_sv((SV *)me->callback_data, G_EVAL|G_SCALAR);

	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		char *msg;
		int len;

		msg = SvPV(ERRSV, len);
		retval = POPi;
		log_error(msg);
	}
	else if (count > 0) {
		retval = POPi;
	}
	else retval = 0;

	PUTBACK;
	FREETMPS;
	LEAVE;

	/* If it's a one-time callback, delete it. */
	if (me->flags & SCRIPT_CALLBACK_ONCE) me->del(me);

	return(retval);
}

static int my_perl_cb_delete(script_callback_t *me)
{
	if (me->syntax) free(me->syntax);
	if (me->name) free(me->name);
	sv_2mortal((SV *)me->callback_data);
	SvREFCNT_dec((SV *)me->callback_data);
	free(me);
	return(0);
}

static int my_create_command(void *ignore, script_raw_command_t *info)
{
	char *cmdname;
	CV *cv;

	if (info->class && strlen(info->class)) {
		cmdname = msprintf("%s_%s", info->class, info->name);
	}
	else {
		cmdname = strdup(info->name);
	}
	cv = newXS(cmdname, my_command_handler, "eggdrop");
	XSANY.any_ptr = info;
	free(cmdname);

	return(0);
}

static int my_delete_command(void *ignore, script_raw_command_t *info)
{
	/* Not sure how to delete CV's in perl yet. */
	return(0);
}

static SV *c_to_perl_var(script_var_t *v)
{
	SV *result;

	if (v->type & SCRIPT_ARRAY) {
		AV *array;
		SV *element;
		int i;

		array = newAV();
		if ((v->type & SCRIPT_TYPE_MASK) == SCRIPT_VAR) {
			script_var_t **v_list;

			v_list = (script_var_t **)v->value;
			for (i = 0; i < v->len; i++) {
				element = c_to_perl_var(v_list[i]);
				av_push(array, element);
			}
		}
		else {
			script_var_t v_sub;
			void **values;

			v_sub.type = v->type & (~SCRIPT_ARRAY);
			values = (void **)v->value;
			for (i = 0; i < v->len; i++) {
				v_sub.value = values[i];
				v_sub.len = -1;
				element = c_to_perl_var(&v_sub);
				av_push(array, element);
			}
		}
		if (v->type & SCRIPT_FREE) free(v->value);
		if (v->type & SCRIPT_FREE_VAR) free(v);
		result = newRV_noinc((SV *)array);
		return(result);
	}

	switch (v->type & SCRIPT_TYPE_MASK) {
		case SCRIPT_INTEGER:
		case SCRIPT_UNSIGNED:
			result = newSViv((int) v->value);
			break;
		case SCRIPT_STRING:
		case SCRIPT_BYTES: {
			char *str = v->value;

			if (!str) str = "";
			if (v->len == -1) v->len = strlen(str);
			result = newSVpv(str, v->len);
			if (v->value && v->type & SCRIPT_FREE) free(v->value);
			break;
		}
		case SCRIPT_POINTER: {
			char str[32];
			int str_len;

			sprintf(str, "#%u", (unsigned int) v->value);
			str_len = strlen(str);
			result = newSVpv(str, str_len);
			break;
		}
		case SCRIPT_USER: {
			char *handle;
			int str_len;

			handle = fake_get_handle(v->value);

			str_len = strlen(handle);
			result = newSVpv(handle, str_len);
			break;
		}
		default:
			result = &PL_sv_undef;
	}
	return(result);
}

static int perl_to_c_var(SV *sv, script_var_t *var, int type)
{
	int len;

	var->type = type;
	var->len = -1;
	var->value = NULL;

	switch (type) {
		case SCRIPT_BYTES: /* Byte-array. */
		case SCRIPT_STRING: { /* String. */
			var->value = SvPV(sv, len);
			break;
		}
		case SCRIPT_UNSIGNED:
		case SCRIPT_INTEGER: { /* Integer. */
			var->value = (void *)SvIV(sv);
			break;
		}
		case SCRIPT_CALLBACK: { /* Callback. */
			script_callback_t *cback;
			char *name;

			cback = (script_callback_t *)calloc(1, sizeof(*cback));
			cback->callback = (Function) my_perl_callbacker;
			cback->del = (Function) my_perl_cb_delete;
			name = SvPV(sv, len);
			cback->name = strdup(name);
			cback->callback_data = newSVsv(sv);
			var->value = cback;
			break;
		}
		case SCRIPT_USER: { /* User. */
			void *user_record;
			char *handle;

			handle = SvPV(sv, len);
			if (handle) user_record = fake_get_user_by_handle(handle);
			else user_record = NULL;
			var->value = user_record;
			break;
		}
		default:
			return(1); /* Error */
	}
	return(0); /* No error */
}

static XS(my_command_handler)
{
	dXSARGS;

	/* Now we have an "items" variable for number of args and also an XSANY.any_ptr variable for client data. This isn't what you would call a "well documented" feature of perl heh. */

	script_raw_command_t *cmd = (script_raw_command_t *) XSANY.any_ptr;
	script_var_t retval;
	SV *result = NULL;
	script_args_t args;
	my_args_data_t argdata;

	argdata.mark = mark;
	argdata.sp = sp;
	argdata.ax = ax;
	argdata.items = items;
	args.module = &my_script_interface;
	args.client_data = &argdata;
	args.len = items;

	retval.type = 0;
	retval.value = NULL;
	retval.len = -1;

	cmd->callback(cmd->client_data, &args, &retval);

	/* No error exceptions right now. */
	/* err = retval.type & SCRIPT_ERROR; */
	result = c_to_perl_var(&retval);

	if (result) {
		XSprePUSH;
		PUSHs(result);
		XSRETURN(1);
	}
	else {
		XSRETURN_EMPTY;
	}
}

static int my_get_arg(void *ignore, script_args_t *args, int num, script_var_t *var, int type)
{
	my_args_data_t *argdata;
	register SV **sp;
	register SV **mark;
	I32 ax;
	I32 items;

	argdata = (my_args_data_t *)args->client_data;
	sp = argdata->sp;
	mark = argdata->mark;
	ax = argdata->ax;
	items = argdata->items;

	if (num >= items) return(-1);

	return perl_to_c_var(ST(num), var, type);
}

char *real_perl_cmd(char *text)
{
	SV *result;
	char *msg, *retval;
	int len;

	result = eval_pv(text, FALSE);
	if (SvTRUE(ERRSV)) {
		msg = SvPV(ERRSV, len);
		retval = msprintf("Perl error: %s", msg);
	}
	else {
		msg = SvPV(result, len);
		retval = msprintf("Perl result: %s\n", msg);
	}

	return(retval);
}

static void init_xs_stuff()
{
	extern void boot_DynaLoader();
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, "eggdrop");
}

int perlscript_init()
{
	char *embedding[] = {"", "-e", "0"};

	ginterp = perl_alloc();
	perl_construct(ginterp);
	perl_parse(ginterp, init_xs_stuff, 3, embedding, NULL);
	return(0);
}

int perlscript_destroy()
{
	PL_perl_destruct_level = 1;
	perl_destruct(ginterp);
	perl_free(ginterp);
	return(0);
}
