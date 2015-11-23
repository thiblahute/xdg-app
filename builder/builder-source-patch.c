/* builder-source-patch.c
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
#include "builder-source-patch.h"

struct BuilderSourcePatch {
  BuilderSource parent;

  char *path;
  guint strip_components;
};

typedef struct {
  BuilderSourceClass parent_class;
} BuilderSourcePatchClass;

G_DEFINE_TYPE (BuilderSourcePatch, builder_source_patch, BUILDER_TYPE_SOURCE);

enum {
  PROP_0,
  PROP_PATH,
  PROP_STRIP_COMPONENTS,
  LAST_PROP
};

static void
builder_source_patch_finalize (GObject *object)
{
  BuilderSourcePatch *self = (BuilderSourcePatch *)object;

  g_free (self->path);

  G_OBJECT_CLASS (builder_source_patch_parent_class)->finalize (object);
}

static void
builder_source_patch_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  BuilderSourcePatch *self = BUILDER_SOURCE_PATCH (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_STRIP_COMPONENTS:
      g_value_set_uint (value, self->strip_components);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_source_patch_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BuilderSourcePatch *self = BUILDER_SOURCE_PATCH (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_free (self->path);
      self->path = g_value_dup_string (value);
      break;

    case PROP_STRIP_COMPONENTS:
      self->strip_components = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GFile *
get_source_file (BuilderSourcePatch *self,
                 BuilderContext *context,
                 GError **error)
{
  if (self->path == NULL || self->path[0] == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "path not specified");
      return NULL;
    }

  return g_file_resolve_relative_path (builder_context_get_base_dir (context), self->path);
}

static gboolean
builder_source_patch_download (BuilderSource *source,
                               BuilderContext *context,
                               GError **error)
{
  BuilderSourcePatch *self = BUILDER_SOURCE_PATCH (source);
  g_autoptr(GFile) src = NULL;

  src = get_source_file (self, context, error);
  if (src == NULL)
    return FALSE;

  if (!g_file_query_exists (src, NULL))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Can't find file at %s", self->path);
      return FALSE;
    }

  return TRUE;
}

static gboolean
patch (GFile *dir,
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
  g_ptr_array_add (args, "patch");
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
builder_source_patch_extract (BuilderSource *source,
                              GFile *dest,
                              BuilderContext *context,
                              GError **error)
{
  BuilderSourcePatch *self = BUILDER_SOURCE_PATCH (source);
  g_autoptr(GFile) patchfile = NULL;
  g_autofree char *patch_path = NULL;
  g_autofree char *strip_components = NULL;

  patchfile = get_source_file (self, context, error);
  if (patchfile == NULL)
    return FALSE;

  strip_components = g_strdup_printf ("-p%u", self->strip_components);
  patch_path = g_file_get_path (patchfile);
  if (!patch (dest, error, strip_components, "-i", patch_path, NULL))
    return FALSE;

  return TRUE;
}

static void
builder_source_patch_checksum (BuilderSource  *source,
                               GChecksum      *checksum,
                               BuilderContext *context)
{
  BuilderSourcePatch *self = BUILDER_SOURCE_PATCH (source);
  g_autoptr(GFile) src = NULL;
  g_autofree char *data = NULL;
  gsize len;

  src = get_source_file (self, context, NULL);
  if (src == NULL)
    return;

  if (g_file_load_contents (src, NULL, &data, &len, NULL, NULL))
    g_checksum_update (checksum, (guchar *)data, len);
  
  builder_checksum_str (checksum, self->path);
  builder_checksum_uint32 (checksum, self->strip_components);
}

static void
builder_source_patch_class_init (BuilderSourcePatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BuilderSourceClass *source_class = BUILDER_SOURCE_CLASS (klass);

  object_class->finalize = builder_source_patch_finalize;
  object_class->get_property = builder_source_patch_get_property;
  object_class->set_property = builder_source_patch_set_property;

  source_class->download = builder_source_patch_download;
  source_class->extract = builder_source_patch_extract;
  source_class->checksum = builder_source_patch_checksum;

  g_object_class_install_property (object_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
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
builder_source_patch_init (BuilderSourcePatch *self)
{
  self->strip_components = 1;
}
