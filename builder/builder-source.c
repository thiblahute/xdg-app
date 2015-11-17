/* builder-source.c
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

#include "builder-source.h"
#include "builder-source-tar.h"
#include "builder-source-git.h"
#include "builder-source-file.h"

static void serializable_iface_init (JsonSerializableIface *serializable_iface);

G_DEFINE_TYPE_WITH_CODE (BuilderSource, builder_source, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, serializable_iface_init));

enum {
  PROP_0,
  PROP_DEST,
  LAST_PROP
};


static void
builder_source_finalize (GObject *object)
{
  BuilderSource *self = BUILDER_SOURCE (object);

  g_free (self->dest);

  G_OBJECT_CLASS (builder_source_parent_class)->finalize (object);
}

static void
builder_source_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BuilderSource *self = BUILDER_SOURCE (object);

  switch (prop_id)
    {
    case PROP_DEST:
      g_value_set_string (value, self->dest);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_source_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BuilderSource *self = BUILDER_SOURCE (object);

  switch (prop_id)
    {
    case PROP_DEST:
      g_free (self->dest);
      self->dest = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
builder_source_real_download (BuilderSource *self,
                              BuilderContext *context,
                              GError **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Download not implemented for type %s", g_type_name_from_instance ((GTypeInstance *)self));
  return FALSE;
}

static void
builder_source_class_init (BuilderSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = builder_source_finalize;
  object_class->get_property = builder_source_get_property;
  object_class->set_property = builder_source_set_property;

  klass->download = builder_source_real_download;

  g_object_class_install_property (object_class,
                                   PROP_DEST,
                                   g_param_spec_string ("dest",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}

static void
builder_source_init (BuilderSource *self)
{
}

static void
serializable_iface_init (JsonSerializableIface *serializable_iface)
{
}

BuilderSource *
builder_source_from_json (JsonNode *node)
{
  JsonObject *object = json_node_get_object (node);
  const gchar *type;

  type = json_object_get_string_member  (object, "type");

  if (strcmp (type, "tar") == 0)
    return (BuilderSource *)json_gobject_deserialize (BUILDER_TYPE_SOURCE_TAR, node);
  if (strcmp (type, "git") == 0)
    return (BuilderSource *)json_gobject_deserialize (BUILDER_TYPE_SOURCE_GIT, node);
  if (strcmp (type, "file") == 0)
    return (BuilderSource *)json_gobject_deserialize (BUILDER_TYPE_SOURCE_FILE, node);
  else if (type == NULL)
    g_warning ("Missing source type");
  else
    g_warning ("Unknown source type %s", type);

  return NULL;
}

gboolean
builder_source_download (BuilderSource *self,
                         BuilderContext *context,
                         GError **error)
{
  BuilderSourceClass *class;

  class = BUILDER_SOURCE_GET_CLASS (self);

  return class->download (self, context, error);
}
