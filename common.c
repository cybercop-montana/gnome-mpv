/*
 * Copyright (c) 2014 gnome-mpv
 *
 * This file is part of GNOME MPV.
 *
 * GNOME MPV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNOME MPV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNOME MPV.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "common.h"
#include "def.h"
#include "mpv.h"
#include "control_box.h"

inline gchar *get_config_string(	gmpv_handle *ctx,
					const gchar *group,
					const gchar *key )
{
	return g_key_file_get_string(ctx->config_file, group, key, NULL);
}

inline void set_config_string(	gmpv_handle *ctx,
					const gchar *group,
					const gchar *key,
					const gchar *value)
{
	g_key_file_set_string(ctx->config_file, group, key, value);
}

inline gchar *get_config_file_path(void)
{
	return g_strconcat(	g_get_user_config_dir(),
				"/",
				CONFIG_FILE,
				NULL );
}

gboolean load_config(gmpv_handle *ctx)
{
	gboolean result;
	gchar *path = get_config_file_path();

	pthread_mutex_lock(ctx->mpv_event_mutex);

	result = g_key_file_load_from_file(	ctx->config_file,
						path,
						G_KEY_FILE_KEEP_COMMENTS,
						NULL );

	pthread_mutex_unlock(ctx->mpv_event_mutex);

	g_free(path);

	return result;
}

gboolean save_config(gmpv_handle *ctx)
{
	gboolean result;
	gchar *path = get_config_file_path();

	pthread_mutex_lock(ctx->mpv_event_mutex);

	result = g_key_file_save_to_file(ctx->config_file, path, NULL);

	pthread_mutex_unlock(ctx->mpv_event_mutex);

	return result;
}

gchar *get_path_from_uri(const gchar *uri)
{
	GFile *file = g_vfs_get_file_for_uri(g_vfs_get_default(), uri);
	gchar *path = g_file_get_path(file);

	if(file)
	{
		g_object_unref(file);
	}

	return path?path:g_strdup(uri);
}

gchar *get_name_from_path(const gchar *path)
{
	const gchar *scheme = g_uri_parse_scheme(path);
	gchar *basename = NULL;

	/* Check whether the given path is likely to be a local path */
	if(!scheme && path)
	{
		basename = g_path_get_basename(path);
	}

	return basename?basename:g_strdup(path);
}

gboolean update_seek_bar(gpointer data)
{
	gmpv_handle* ctx = data;
	gboolean exit_flag;
	gdouble time_pos;
	gint rc;

	pthread_mutex_lock(ctx->mpv_event_mutex);

	exit_flag = ctx->exit_flag;

	pthread_mutex_unlock(ctx->mpv_event_mutex);

	if(!exit_flag)
	{
		pthread_mutex_lock(ctx->mpv_event_mutex);

		rc = mpv_get_property(	ctx->mpv_ctx,
					"time-pos",
					MPV_FORMAT_DOUBLE,
					&time_pos );

		pthread_mutex_unlock(ctx->mpv_event_mutex);

		if(rc >= 0)
		{
			ControlBox *control_box
				= CONTROL_BOX(ctx->gui->control_box);

			gtk_range_set_value
				(	GTK_RANGE(control_box->seek_bar),
					time_pos );
		}
	}

	return !exit_flag;
}

gboolean control_reset(gpointer data)
{
	main_window_reset(((gmpv_handle *)data)->gui);

	return FALSE;
}

gboolean show_error_dialog(gpointer data)
{
	gmpv_handle* ctx = data;

	if(ctx->log_buffer)
	{
		GtkWidget *dialog
			= gtk_message_dialog_new
				(	GTK_WINDOW(ctx->gui),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					"Error" );

		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG(dialog), "%s", ctx->log_buffer);

		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		g_free(ctx->log_buffer);

		ctx->log_buffer = NULL;
	}

	return FALSE;
}

void seek_relative(gmpv_handle *ctx, gint offset)
{
	const gchar *cmd[] = {"seek", NULL, NULL};
	gchar *offset_str = g_strdup_printf("%d", offset);

	cmd[1] = offset_str;

	if(!ctx->loaded)
	{
		mpv_load(ctx, NULL, FALSE, TRUE);
	}

	mpv_check_error(mpv_command(ctx->mpv_ctx, cmd));

	g_free(offset_str);

	update_seek_bar(ctx);
}

void resize_window_to_fit(gmpv_handle *ctx, gdouble multiplier)
{
	gchar *video = mpv_get_property_string(ctx->mpv_ctx, "video");
	gint64 width;
	gint64 height;
	gint mpv_width_rc;
	gint mpv_height_rc;

	mpv_width_rc = mpv_get_property(	ctx->mpv_ctx,
						"dwidth",
						MPV_FORMAT_INT64,
						&width );

	mpv_height_rc = mpv_get_property(	ctx->mpv_ctx,
						"dheight",
						MPV_FORMAT_INT64,
						&height );

	if(video
	&& strncmp(video, "no", 3) != 0
	&& mpv_width_rc >= 0
	&& mpv_height_rc >= 0)
	{
		gint width_margin
			= gtk_widget_get_allocated_width(GTK_WIDGET(ctx->gui))
			- gtk_widget_get_allocated_width(ctx->gui->vid_area);

		gint height_margin
			= gtk_widget_get_allocated_height(GTK_WIDGET(ctx->gui))
			- gtk_widget_get_allocated_height(ctx->gui->vid_area);

		gtk_window_resize(	GTK_WINDOW(ctx->gui),
					(multiplier*width)+width_margin,
					(multiplier*height)+height_margin );
	}

	mpv_free(video);
}