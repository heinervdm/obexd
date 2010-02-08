/*
 *
 *  OBEX Server
 *
 *  Copyright (C) 2009  Thomas Zimmermann <zimmermann@vdm-design.de>
 *  Copyright (C) 2009  Intel Corporation
 *  Copyright (C) 2007-2009  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

#include "telephony.h"
#include "logging.h"

#include <frameworkd-glib/opimd/frameworkd-glib-opimd-calls.h>

static void _missed_calls_callback(GError *error, const int amount, gpointer userdata);
static int called;
static int calls;

int telephony_pullmissedcalls(guint8 *missedcalls)
{
	called = -1;
	calls = 0;

	opimd_calls_get_new_missed_calls(_missed_calls_callback, NULL);

	while (called != 0)
	{
#ifdef HAVE_UNISTD_H
		usleep(100);
#endif
	}

	*missedcalls = calls;
	return calls;
}

static void _missed_calls_callback(GError *error, const int amount, gpointer userdata)
{
	(void)userdata;

	if (error) {
		DBG("_missed_calls_callback: error %d: %s", error->code, error->message);
		return;
	}

	calls = amount;
	called = 0;
}
