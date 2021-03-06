/*
 *
 *  OBEX Server
 *
 *  Copyright (C) 2007-2010  Nokia Corporation
 *  Copyright (C) 2007-2010  Marcel Holtmann <marcel@holtmann.org>
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

struct obex_server {
	struct obex_transport_driver *transport;
	void *transport_data;
	gboolean auto_accept;
	char *folder;
	gboolean symlinks;
	char *capability;
	gboolean secure;
	GIOChannel *io;
	unsigned int watch;
	GSList *drivers;
};

int obex_server_init(uint16_t service, const char *folder, gboolean secure,
		gboolean auto_accept, gboolean symlinks,
		const char *capability);

void obex_server_exit(void);

struct obex_service_driver *obex_server_find_driver(struct obex_server *server,
							uint8_t channel);
int obex_server_new_connection(struct obex_server *server, GIOChannel *io,
				uint16_t tx_mtu, uint16_t rx_mtu);
