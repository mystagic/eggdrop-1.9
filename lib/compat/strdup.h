/*
 * strdup.h
 *   prototypes for strdup.c
 *
 * $Id: strdup.h,v 1.2 2002/02/07 22:18:59 wcc Exp $
 */
/*
 * Copyright (C) 2000, 2001, 2002 Eggheads Development Team
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
#ifndef _EGG_STRDUP_H
#define _EGG_STRDUP_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_STRDUP
const char *strdup(const char *);
#endif

#endif				/* !_EGG_STRDUP_H */
