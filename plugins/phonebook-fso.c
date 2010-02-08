/*
 *
 *  OBEX Server
 *
 *  Copyright (C) 2009-2010  Thomas Zimmermann <zimmermann@vdm-design.de>
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

#include <glib.h>
#include <stdlib.h>

#include <openobex/obex.h>
#include <openobex/obex_const.h>
#include <bluetooth/bluetooth.h>

#include "logging.h"
#include "obex.h"
#include "phonebook.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <frameworkd-glib/opimd/frameworkd-glib-opimd-contacts.h>

#define EOL_CHARS "\n"
#define VL_VERSION "<?xml version=\"1.0\"?>" EOL_CHARS
#define VL_TYPE "<!DOCTYPE vcard-listing SYSTEM \"vcard-listing.dtd\">" EOL_CHARS
#define VL_BODY_BEGIN "<vCard-listing version=\"1.0\">" EOL_CHARS
#define VL_BODY_END "</vCard-listing>" EOL_CHARS
#define VL_ELEMENT "<card handle = \"%d.vcf\" name = \"%s\"/>" EOL_CHARS

typedef enum {
	VCARD_21,
	VCARD_30
} VCardFormat;

struct _contact_list_pack {
	gpointer data;
	int *count;
	void (*callback)(gpointer, gpointer);
	DBusGProxy *query;
	obex_t *obex;
	obex_object_t *obj;
	struct apparam_field params;
};

static gint
_compare_contacts(gconstpointer _a, gconstpointer _b)
{
	GHashTable **a = (GHashTable **) _a;
	GHashTable **b = (GHashTable **) _b;
	gpointer p;
	const char *name_a, *name_b;
/* Probably not best (sorting by just Name) but will have to do ATM */
	p = g_hash_table_lookup(*a, "Name");
	if (!p) {
		name_a = "";
		g_debug("name a not found!!!!");
	}
	else
		name_a = g_value_get_string(p);

	p = g_hash_table_lookup(*b, "Name");
	if (!p) {
		name_b = "";
		g_debug("name b not found!!!!");
	}
	else
		name_b = g_value_get_string(p);

	return (strcasecmp(name_a, name_b));
}

static void
_contact_list_result_callback(GError *error, GPtrArray *contacts, void *_data)
{
	debug("INFO: _contact_list_result_callback called");
	(void) error;
	struct _contact_list_pack *data =
		(struct _contact_list_pack *)_data;

	if (error || !contacts) {
		return;
	}
	GString *pb;
	gchar *result;
	gint32 size;
	int i;
	
	pb = g_string_new(NULL);
	
	struct apparam_field *params = &data->params;
	struct obex_session *session = OBEX_GetUserData(data->obex);
/*
	g_ptr_array_sort(contacts, _compare_contacts);
*/
	for (i = 0; i < contacts->len && i < params->maxlistcount; i++)
	{
		debug("INFO: _contact_list_result_callback: contact number %d", i);
		gpointer _c, p;
		const char *tmp, *vcard, *name, *firstname;
		_c = g_ptr_array_index (contacts, i);
		GHashTable **c = (GHashTable **) _c;
		
		g_string_append_printf(pb, "BEGIN:VCARD\n");
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "VERSION:3.0\n");
		else
			g_string_append_printf(pb, "VERSION:2.1\n");
		
		p = g_hash_table_lookup(*c, "Name");
		if (p)
			name = g_value_get_string(p);
		p = g_hash_table_lookup(*c, "Surname");
		if (p)
			firstname = g_value_get_string(p);
		if (!name && !firstname) continue;
		if (!name) name = "";
		if (!firstname) firstname = "";
		g_string_append_printf(pb, "N:%s;%s\n", name, firstname);
		
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "FN:%s %s\n", firstname, name);
		
		p = g_hash_table_lookup(*c, "Phone");
		if (p)
			if (params->format == VCARD_30)
				g_string_append_printf(pb, "TEL;TYPE=voice,pref:%s\n", g_value_get_string(p));
			else
				g_string_append_printf(pb, "TEL;VOICE:%s\n", g_value_get_string(p));
		
		p = g_hash_table_lookup(*c, "Home phone");
		if (p)
			if (params->format == VCARD_30)
				g_string_append_printf(pb, "TEL;TYPE=voice,home:%s\n", g_value_get_string(p));
			else
				g_string_append_printf(pb, "TEL;HOME;VOICE:%s\n", g_value_get_string(p));
		
		p = g_hash_table_lookup(*c, "Cell phone");
		if (p)
			if (params->format == VCARD_30)
				g_string_append_printf(pb, "TEL;TYPE=cell:%s\n", g_value_get_string(p));
			else
				g_string_append_printf(pb, "TEL;CELL:%s\n", g_value_get_string(p));
		
		p = g_hash_table_lookup(*c, "Work phone");
		if (p)
			if (params->format == VCARD_30)
				g_string_append_printf(pb, "TEL;TYPE=voice,work:%s\n", g_value_get_string(p));
			else
				g_string_append_printf(pb, "TEL;WORK;VOICE:%s\n", g_value_get_string(p));
		
		p = g_hash_table_lookup(*c, "Fax phone");
		if (p)
			if (params->format == VCARD_30)
				g_string_append_printf(pb, "TEL;TYPE=fax,home:%s\n", g_value_get_string(p));
			else
				g_string_append_printf(pb, "TEL;HOME;FAX:%s\n", g_value_get_string(p));
		
		/*TODO: add other fields*/
			
		g_string_append_printf(pb, "END:VCARD\n\n");
	}

	result = g_string_free(pb, FALSE);
	size = strlen(result);

	if (size != 0) {
		session->buf = g_realloc(session->buf, session->size + size);
		memcpy(session->buf + session->size, result, size);
		session->size += size;
	}

	session->finished = 1;
	OBEX_ResumeRequest(session->obex);
	
	opimd_contact_query_dispose(data->query, NULL, NULL);
}

static void
_contact_list_count_callback(GError *error, const int count, gpointer _data)
{
	(void) error;
	struct _contact_list_pack *data =
		(struct _contact_list_pack *)_data;
	debug("INFO: Contact query result gave %d entries", count);
	*data->count = count;
	opimd_contact_query_get_multiple_results(data->query,
			count, _contact_list_result_callback, data);
}


static void
_contact_query_callback(GError *error, char *query_path, gpointer _data)
{
	debug("INFO: _contact_query_callback called");
	if (error == NULL) {
		debug("INFO: _contact_query_callback: error == NULL");
		struct _contact_list_pack *data =
			(struct _contact_list_pack *)_data;
		data->query = (DBusGProxy *)
			dbus_connect_to_opimd_contact_query(query_path);
		opimd_contact_query_get_result_count(data->query,
				_contact_list_count_callback, data);
	}
}

int phonebook_pullphonebook(obex_t *obex, obex_object_t *obj,
				struct apparam_field params)
{
	int count = 0;
	debug("INFO: phonebook_pullphonebook called");
	struct _contact_list_pack *data =
		malloc(sizeof(struct _contact_list_pack));
	data->count = (int *)count;
	data->obex = obex;
	data->obj = obj;
	data->params = params;

	GHashTable *qry = g_hash_table_new_full
		(g_str_hash, g_str_equal, NULL, NULL);
	opimd_contacts_query(qry, _contact_query_callback, data);
	g_hash_table_destroy(qry);

	OBEX_SuspendRequest(obex, obj);
	
	return count;
}

static void
_contact_listing_result_callback(GError *error, GPtrArray *contacts, void *_data)
{
	debug("INFO: _contact_listing_result_callback called");
	(void) error;
	struct _contact_list_pack *data =
		(struct _contact_list_pack *)_data;

	if (error || !contacts) {
		return;
	}

	gchar *result;
	gint32 size;
	int i;
	GString *listing;
	guint16 offset = 0, count = 0, index;

	listing = g_string_new(VL_VERSION);
	listing = g_string_append(listing, VL_TYPE);
	listing = g_string_append(listing, VL_BODY_BEGIN);

	struct apparam_field *params = &data->params;
	struct obex_session *session = OBEX_GetUserData(data->obex);

	for (i = offset; i < contacts->len && i < params->maxlistcount; i++)
	{
		debug("INFO: _contact_listing_result_callback: contact number %d", i);
		gchar *element = NULL;
		const char *name = NULL, *path = NULL;
		gpointer _c, p;

		_c = g_ptr_array_index (contacts, i);
		GHashTable **c = (GHashTable **) _c;

		index = -1;

		p = g_hash_table_lookup(*c, "Path");
		if (p)
		{
			path = g_value_get_string(p);
			sscanf(path, "/org/freesmartphone/PIM/Contacts/%d", &index);
		}

		p = g_hash_table_lookup(*c, "Name");
		if (p)
		{
			name = g_value_get_string(p);
		}
		
		if (name && index)
		{
			element = g_strdup_printf(VL_ELEMENT, index, name);
			listing = g_string_append(listing, element);
		}
		g_free(element);
	}
	
	listing = g_string_append(listing, VL_BODY_END);
	result = g_string_free(listing, FALSE);
	size = strlen(result);

	if (size != 0) {
		session->buf = g_realloc(session->buf, session->size + size);
		memcpy(session->buf + session->size, result, size);
		session->size += size;
	}

	session->finished = 1;
	OBEX_ResumeRequest(session->obex);
	
	opimd_contact_query_dispose(data->query, NULL, NULL);
}

static void
_contact_listing_count_callback(GError *error, const int count, gpointer _data)
{
	(void) error;
	struct _contact_list_pack *data =
		(struct _contact_list_pack *)_data;
	debug("Info: Contact query result gave %d entries", count);
	*data->count = count;
	opimd_contact_query_get_multiple_results(data->query,
			count, _contact_listing_result_callback, data);
}

static void
_contact_listing_callback(GError *error, char *query_path, gpointer _data)
{
	debug("INFO: _contact_listing_callback called");
	if (error == NULL) {
		debug("INFO: _contact_listing_callback: error == NULL");
		struct _contact_list_pack *data =
			(struct _contact_list_pack *)_data;
		data->query = (DBusGProxy *)
			dbus_connect_to_opimd_contact_query(query_path);
		opimd_contact_query_get_result_count(data->query,
				_contact_listing_count_callback, data);
	}
}

int phonebook_pullvcardlisting(obex_t *obex, obex_object_t *obj,
				struct apparam_field params)
{
	debug("INFO: phonebook_pullvcardlisting called");
	struct _contact_list_pack *data =
		malloc(sizeof(struct _contact_list_pack));
	data->obex = obex;
	data->obj = obj;
	data->params = params;

	GHashTable *qry = g_hash_table_new_full
		(g_str_hash, g_str_equal, NULL, NULL);
	
	/* All the vCards shall be returned if SearchValue header is
	 * not specified */
	/*
	if (!params.searchval || !strlen((char *) params.searchval)) {
		query = e_book_query_any_field_contains("");
		goto done;
	}

	if (params.searchattrib == 0) {
		value_list = g_strsplit((gchar *) params.searchval, ";", 5);

		if (value_list[0])
			str1 = g_strdup_printf(QUERY_FAMILY_NAME,
						value_list[0]);
		if (value_list[1])
			str2 = g_strdup_printf(QUERY_GIVEN_NAME, value_list[1]);

		if (str1)
			query1 = e_book_query_from_string(str1);
		if (str2)
			query2 = e_book_query_from_string(str2);
		if (query1 && query2)
			query = e_book_query_andv(query1, query2, NULL);
		else
			query = query1;
	} else {
		str1 = g_strdup_printf(QUERY_PHONE, params.searchval);
		query = e_book_query_from_string((char *) params.searchval);
	}
	*/

	opimd_contacts_query(qry, _contact_listing_callback, data);
	g_hash_table_destroy(qry);

	OBEX_SuspendRequest(obex, obj);

	return 0;
}

static void
_contact_entry_callback(GError *error, GHashTable *c, void *_data)
{
	debug("INFO: _contact_entry_callback called");
	(void) error;
	struct _contact_list_pack *data =
		(struct _contact_list_pack *)_data;

	if (error || !c) {
		return;
	}

	GString *pb;
	gchar *result;
	gint32 size;

	pb = g_string_new(NULL);

	struct apparam_field *params = &data->params;
	struct obex_session *session = OBEX_GetUserData(data->obex);

	gpointer  p;
	const char *tmp, *vcard, *name, *firstname;

	g_string_append_printf(pb, "BEGIN:VCARD\n");
	if (params->format == VCARD_30)
		g_string_append_printf(pb, "VERSION:3.0\n");
	else
		g_string_append_printf(pb, "VERSION:2.1\n");

	p = g_hash_table_lookup(c, "Name");
	if (p)
		name = g_value_get_string(p);
	p = g_hash_table_lookup(c, "Surname");
	if (p)
		firstname = g_value_get_string(p);
	if (!name && !firstname) return;
	if (!name) name = "";
	if (!firstname) firstname = "";
	g_string_append_printf(pb, "N:%s;%s\n", name, firstname);

	if (params->format == VCARD_30)
		g_string_append_printf(pb, "FN:%s %s\n", firstname, name);

	p = g_hash_table_lookup(c, "Phone");
	if (p)
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "TEL;TYPE=voice,pref:%s\n", g_value_get_string(p));
		else
			g_string_append_printf(pb, "TEL;VOICE:%s\n", g_value_get_string(p));

	p = g_hash_table_lookup(c, "Home phone");
	if (p)
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "TEL;TYPE=voice,home:%s\n", g_value_get_string(p));
		else
			g_string_append_printf(pb, "TEL;HOME;VOICE:%s\n", g_value_get_string(p));

	p = g_hash_table_lookup(c, "Cell phone");
	if (p)
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "TEL;TYPE=cell:%s\n", g_value_get_string(p));
		else
			g_string_append_printf(pb, "TEL;CELL:%s\n", g_value_get_string(p));

	p = g_hash_table_lookup(c, "Work phone");
	if (p)
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "TEL;TYPE=voice,work:%s\n", g_value_get_string(p));
		else
			g_string_append_printf(pb, "TEL;WORK;VOICE:%s\n", g_value_get_string(p));

	p = g_hash_table_lookup(c, "Fax phone");
	if (p)
		if (params->format == VCARD_30)
			g_string_append_printf(pb, "TEL;TYPE=fax,home:%s\n", g_value_get_string(p));
		else
			g_string_append_printf(pb, "TEL;HOME;FAX:%s\n", g_value_get_string(p));

	/*TODO: add other fields*/

	g_string_append_printf(pb, "END:VCARD\n\n");

	result = g_string_free(pb, FALSE);
	size = strlen(result);

	if (size != 0) {
		session->buf = g_realloc(session->buf, session->size + size);
		memcpy(session->buf + session->size, result, size);
		session->size += size;
	}

	session->finished = 1;
	OBEX_ResumeRequest(session->obex);
}

int phonebook_pullvcardentry(obex_t *obex, obex_object_t *obj,
				struct apparam_field params)
{
	debug("INFO: phonebook_pullvcardentry called");
	guint16 index;
	gchar *path;
	struct _contact_list_pack *data =
		malloc(sizeof(struct _contact_list_pack));
	data->obex = obex;
	data->obj = obj;
	data->params = params;

	struct obex_session *session = OBEX_GetUserData(obex);
	sscanf(session->name, "%hu.vcf", &index);
	g_snprintf(path, 1, "/org/freesmartphone/PIM/Contacts/%d", index);
	
	opimd_contact_get_content(path, _contact_entry_callback, data);

	OBEX_SuspendRequest(obex, obj);

	return 0;
}
