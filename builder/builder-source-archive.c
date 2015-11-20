/* builder-source-archive.c
 *
 * Copyright (C) 2015 Red Hat, Inc
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/statfs.h>

#include "xdg-app-utils.h"

#include "builder-utils.h"
#include "builder-source-archive.h"

struct BuilderSourceArchive {
  BuilderSource parent;

  char *url;
  char *checksum;
  guint strip_components;
};

typedef struct {
  BuilderSourceClass parent_class;
} BuilderSourceArchiveClass;

G_DEFINE_TYPE (BuilderSourceArchive, builder_source_archive, BUILDER_TYPE_SOURCE);

enum {
  PROP_0,
  PROP_URL,
  PROP_CHECKSUM,
  PROP_STRIP_COMPONENTS,
  LAST_PROP
};

static void
builder_source_archive_finalize (GObject *object)
{
  BuilderSourceArchive *self = (BuilderSourceArchive *)object;

  g_free (self->url);
  g_free (self->checksum);

  G_OBJECT_CLASS (builder_source_archive_parent_class)->finalize (object);
}

static void
builder_source_archive_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  BuilderSourceArchive *self = BUILDER_SOURCE_ARCHIVE (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;

    case PROP_CHECKSUM:
      g_value_set_string (value, self->checksum);
      break;

    case PROP_STRIP_COMPONENTS:
      g_value_set_uint (value, self->strip_components);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_source_archive_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BuilderSourceArchive *self = BUILDER_SOURCE_ARCHIVE (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_free (self->url);
      self->url = g_value_dup_string (value);
      break;

    case PROP_CHECKSUM:
      g_free (self->checksum);
      self->checksum = g_value_dup_string (value);
      break;

    case PROP_STRIP_COMPONENTS:
      self->strip_components = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static SoupURI *
get_uri (BuilderSourceArchive *self,
         GError **error)
{
  SoupURI *uri;

  if (self->url == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "URL not specified");
      return NULL;
    }

  uri = soup_uri_new (self->url);
  if (uri == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid URL '%s'", self->url);
      return NULL;
    }
  return uri;
}

static GFile *
get_download_location (BuilderSourceArchive *self,
                       BuilderContext *context,
                       GError **error)
{
  g_autoptr(SoupURI) uri = NULL;
  const char *path;
  g_autofree char *base_name = NULL;
  g_autoptr(GFile) download_dir = NULL;
  g_autoptr(GFile) checksum_dir = NULL;
  g_autoptr(GFile) file = NULL;

  uri = get_uri (self, error);
  if (uri == NULL)
    return FALSE;

  path = soup_uri_get_path (uri);

  base_name = g_path_get_basename (path);

  if (self->checksum == NULL || *self->checksum == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Checksum not specified");
      return FALSE;
    }

  download_dir = builder_context_get_download_dir (context);
  checksum_dir = g_file_get_child (download_dir, self->checksum);
  file = g_file_get_child (checksum_dir, base_name);

  return g_steal_pointer (&file);
}

static gboolean
builder_source_archive_download (BuilderSource *source,
                                 BuilderContext *context,
                                 GError **error)
{
  BuilderSourceArchive *self = BUILDER_SOURCE_ARCHIVE (source);
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) dir = NULL;
  g_autoptr(SoupURI) uri = NULL;
  SoupSession *session;
  g_autofree char *url = NULL;
  g_autofree char *dir_path = NULL;
  g_autofree char *checksum = NULL;
  g_autofree char *base_name = NULL;
  g_autoptr(SoupMessage) msg = NULL;

  file = get_download_location (self, context, error);
  if (file == NULL)
    return FALSE;

  if (g_file_query_exists (file, NULL))
    return TRUE;

  base_name = g_file_get_basename (file);

  uri = get_uri (self, error);
  if (uri == NULL)
    return FALSE;

  url = g_strdup (self->url);

  session = builder_context_get_soup_session (context);

  while (TRUE)
    {
      g_clear_object (&msg);
      msg = soup_message_new ("GET", url);
      g_debug ("GET %s", self->url);
      g_print ("Downloading %s...", base_name);
      soup_session_send_message (session, msg);
      g_print ("done\n");

      g_debug ("response: %d %s", msg->status_code, msg->reason_phrase);

      if (SOUP_STATUS_IS_REDIRECTION (msg->status_code))
        {
          const char *header = soup_message_headers_get_one (msg->response_headers, "Location");
          if (header)
            {
              g_autoptr(SoupURI) new_uri = soup_uri_new_with_base (soup_message_get_uri (msg), header);
              g_free (url);
              url = soup_uri_to_string (uri, FALSE);
              g_debug ("  -> %s", header);
              continue;
            }
        }
      else if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Failed to download %s (error %d): %s", base_name, msg->status_code, msg->reason_phrase);
          return FALSE;
        }

      break; /* No redirection */
    }

  checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256,
                                            msg->response_body->data,
                                            msg->response_body->length);

  if (strcmp (checksum, self->checksum) != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Wrong checksum for %s, expected %s, was %s", base_name, self->checksum, checksum);
      return FALSE;
    }

  dir = g_file_get_parent (file);
  dir_path = g_file_get_path (dir);
  g_mkdir_with_parents (dir_path, 0755);

  if (!g_file_replace_contents (file,
                                msg->response_body->data,
                                msg->response_body->length,
                                NULL, FALSE, G_FILE_CREATE_NONE, NULL,
                                NULL, error))
    return FALSE;

  return TRUE;
}

static gboolean
tar (GFile *dir,
     GError **error,
     const gchar            *argv1,
     ...)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subp = NULL;
  GPtrArray *args;
  const gchar *arg;
  va_list ap;

  args = g_ptr_array_new ();
  g_ptr_array_add (args, "tar");
  va_start (ap, argv1);
  g_ptr_array_add (args, (gchar *) argv1);
  while ((arg = va_arg (ap, const gchar *)))
    g_ptr_array_add (args, (gchar *) arg);
  g_ptr_array_add (args, NULL);
  va_end (ap);

  launcher = g_subprocess_launcher_new (0);

  if (dir)
    {
      g_autofree char *path = g_file_get_path (dir);
      g_subprocess_launcher_set_cwd (launcher, path);
    }

  subp = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);

  if (subp == NULL ||
      !g_subprocess_wait_check (subp, NULL, error))
    return FALSE;

  return TRUE;
}

static gboolean
builder_source_archive_extract (BuilderSource *source,
                                GFile *dest,
                                BuilderContext *context,
                                GError **error)
{
  BuilderSourceArchive *self = BUILDER_SOURCE_ARCHIVE (source);
  g_autoptr(GFile) archivefile = NULL;
  g_autofree char *archive_path = NULL;
  g_autofree char *strip_components = NULL;

  archivefile = get_download_location (self, context, error);
  if (archivefile == NULL)
    return FALSE;

  strip_components = g_strdup_printf ("--strip-components=%u", self->strip_components);
  archive_path = g_file_get_path (archivefile);
  if (!tar (dest, error, "xf", archive_path, strip_components, NULL))
    return FALSE;

  return TRUE;
}

static void
builder_source_archive_checksum (BuilderSource  *source,
                                 GChecksum      *checksum,
                                 BuilderContext *context)
{
  BuilderSourceArchive *self = BUILDER_SOURCE_ARCHIVE (source);

  builder_checksum_str (checksum, self->url);
  builder_checksum_str (checksum, self->checksum);
  builder_checksum_uint32 (checksum, self->strip_components);
}


static void
builder_source_archive_class_init (BuilderSourceArchiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BuilderSourceClass *source_class = BUILDER_SOURCE_CLASS (klass);

  object_class->finalize = builder_source_archive_finalize;
  object_class->get_property = builder_source_archive_get_property;
  object_class->set_property = builder_source_archive_set_property;

  source_class->download = builder_source_archive_download;
  source_class->extract = builder_source_archive_extract;
  source_class->checksum = builder_source_archive_checksum;

  g_object_class_install_property (object_class,
                                   PROP_URL,
                                   g_param_spec_string ("url",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_CHECKSUM,
                                   g_param_spec_string ("checksum",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_STRIP_COMPONENTS,
                                   g_param_spec_uint ("strip-components",
                                                      "",
                                                      "",
                                                      0, G_MAXUINT,
                                                      1,
                                                      G_PARAM_READWRITE));
}

static void
builder_source_archive_init (BuilderSourceArchive *self)
{
  self->strip_components = 1;
}
