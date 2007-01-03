/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

typedef struct _rs_confbox
{
	GtkWidget *widget;
	GtkListStore *model;
	const gchar *conf_key;
	gpointer user_data;
	void (*callback)(gpointer active, gpointer user_data);
} RS_CONFBOX;

extern void gui_confbox_add_entry(RS_CONFBOX *combo, const gchar *conf_id, const gchar *text, gpointer *user_data);
extern void gui_confbox_load_conf(RS_CONFBOX *combo, gchar *default_value);
extern void gui_confbox_set_callback(RS_CONFBOX *combo, gpointer user_data, void (*callback)(gpointer active, gpointer user_data));
extern RS_CONFBOX *gui_confbox_new(const gchar *conf_key);
extern void gui_confbox_destroy(RS_CONFBOX *combo);
extern GtkWidget *gui_combobox_get_widget(RS_CONFBOX *combo);
extern RS_FILETYPE *gui_filetype_combobox_get_filetype(GtkComboBox *widget);
extern const gchar *gui_filetype_combobox_get_ext(GtkComboBox *widget);
extern GtkWidget *gui_filetype_combobox();
extern void gui_filetype_combobox_set_active(GtkWidget *combo, RS_FILETYPE *set);
extern GtkWidget *gui_filetype_preference(GtkWidget *filetype_combo);
extern void checkbox_set_conf(GtkToggleButton *togglebutton, gpointer user_data);
extern GtkWidget *checkbox_from_conf(const gchar *conf, gchar *label, gboolean default_value);
extern GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
extern void gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
extern void gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_batch_filename_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_batch_filetype_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_export_changed_helper(GtkLabel *label);
extern void gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data);
extern void gui_export_filename_entry_changed(GtkComboBox *combobox, gpointer user_data);
extern GtkWidget *gui_preferences_make_cms_page();
