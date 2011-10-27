/*
   nautilus-monitor.c: file and directory change monitoring for nautilus

   Copyright (C) 2000, 2001 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Seth Nickell <seth@eazel.com>
   Darin Adler <darin@bentspoon.com>
   Alex Graveley <alex@ximian.com>
   ammonkey <am.monkeyd@gmail.com>
*/

#include <config.h>
#include "gof-monitor.h"
#include "gof-file.h"
#include <gio/gio.h>
#include <stdio.h>

struct GOFMonitor {
    GFileMonitor        *gfile_monitor;
    GOFDirectoryAsync   *dir;
};

static void
dir_changed (GFileMonitor* gfile_monitor,
             GFile *child,
             GFile *other_file,
             GFileMonitorEvent event_type,
             gpointer user_data)
{
    char *uri, *to_uri;
    GOFMonitor *monitor = user_data;
    GOFDirectoryAsync *dir = monitor->dir;
    GOFFile *file;

    uri = g_file_get_uri (child);
    to_uri = NULL;
    if (other_file) {
        to_uri = g_file_get_uri (other_file);
    }

    switch (event_type) {
    default:
    case G_FILE_MONITOR_EVENT_CHANGED:
        /* ignore */
        break;
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        //nautilus_file_changes_queue_file_changed (child);
        g_message ("file changed %s\n", uri);
        file = gof_file_get (child);
        gof_file_query_update (file);
        g_signal_emit_by_name (dir, "file_changed", file);
        gof_file_unref (file);
        break;
    case G_FILE_MONITOR_EVENT_DELETED:
        //nautilus_file_changes_queue_file_removed (child);
        g_message ("file deleted %s\n", uri);
        file = gof_file_get (child);
        if (!file->is_hidden)
            g_hash_table_remove (dir->file_hash, child);
        else
            g_hash_table_remove (dir->hidden_file_hash, child);
        g_signal_emit_by_name (dir, "file_deleted", file);
        g_object_unref (file);
        break;
    case G_FILE_MONITOR_EVENT_CREATED:
        //nautilus_file_changes_queue_file_added (child);
        g_message ("file added %s\n", uri);
        file = gof_file_get (child);
        if(!g_strcmp0(g_file_get_path(child), g_file_get_path(dir->location)))
        {
            /* in rare case, we can monitor a directory which doesn't exist yet,
             * and this is our dir which has been created, so, we mustn't say
             * that it is a new file in this dir */
             dir->file->exists = TRUE;
        }
        if (file->exists) {
            if (!file->is_hidden)
                g_hash_table_insert (dir->file_hash, g_object_ref (child), file);
            else
                g_hash_table_insert (dir->hidden_file_hash, g_object_ref (child), file);
            g_signal_emit_by_name (dir, "file_added", file);
        } else {
            gof_file_unref (file);
        }
        break;

    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        /* TODO: Do something */
        break;
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
        /* TODO: Do something */
        break;
    }

    g_free (uri);
    g_free (to_uri);
}

GOFMonitor *
gof_monitor_directory (GOFDirectoryAsync *dir)
{
    GOFMonitor *monitor;

    /* TODO: implement GCancellable * */
    monitor = g_new0 (GOFMonitor, 1);
    monitor->gfile_monitor = g_file_monitor_directory (dir->location, G_FILE_MONITOR_WATCH_MOUNTS, NULL, NULL);
    monitor->dir = dir;

    if (monitor->gfile_monitor) {
        g_signal_connect (monitor->gfile_monitor, "changed", (GCallback)dir_changed, monitor);
    }

    return monitor;
}

void 
gof_monitor_cancel (GOFMonitor *monitor)
{
    if (monitor->gfile_monitor != NULL) {
        g_signal_handlers_disconnect_by_func (monitor->gfile_monitor, dir_changed, monitor);
        g_file_monitor_cancel (monitor->gfile_monitor);
        g_object_unref (monitor->gfile_monitor);
    }

    g_free (monitor);
}

void
gof_monitor_file_changed (GOFFile *file)
{
    GOFDirectoryAsync *dir;

    /* get the DirectoryAsync associated to the file */
    dir = gof_directory_async_cache_lookup (file->directory);
    if (dir != NULL)
        g_signal_emit_by_name (dir, "file_changed", file);
    g_object_unref (dir);
}

