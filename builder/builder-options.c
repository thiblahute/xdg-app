/* builder-options.c
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

#include "builder-options.h"

struct BuilderOptions {
  GObject parent;

  char *cflags;
};

typedef struct {
  GObjectClass parent_class;
} BuilderOptionsClass;

static void serializable_iface_init (JsonSerializableIface *serializable_iface);

G_DEFINE_TYPE_WITH_CODE (BuilderOptions, builder_options, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, serializable_iface_init));

enum {
  PROP_0,
  PROP_CFLAGS,
  LAST_PROP
};


static void
builder_options_finalize (GObject *object)
{
  BuilderOptions *self = (BuilderOptions *)object;

  g_clear_pointer (&self->cflags, g_free);

  G_OBJECT_CLASS (builder_options_parent_class)->finalize (object);
}

static void
builder_options_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BuilderOptions *self = BUILDER_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_CFLAGS:
      g_value_set_string (value, self->cflags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_options_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BuilderOptions *self = BUILDER_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_CFLAGS:
      g_clear_pointer (&self->cflags, g_free);
      self->cflags = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_options_class_init (BuilderOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = builder_options_finalize;
  object_class->get_property = builder_options_get_property;
  object_class->set_property = builder_options_set_property;

  g_object_class_install_property (object_class,
                                   PROP_CFLAGS,
                                   g_param_spec_string ("cflags",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}

static void
builder_options_init (BuilderOptions *self)
{
}

/*
static JsonNode *
builder_options_serialize_property (JsonSerializable *serializable,
                                     const gchar      *property_name,
                                     const GValue     *value,
                                     GParamSpec       *pspec)
{
}

static gboolean
builder_options_deserialize_property (JsonSerializable *serializable,
                                       const gchar      *property_name,
                                       GValue           *value,
                                       GParamSpec       *pspec,
                                       JsonNode         *property_node)
{
}

static GParamSpec *
builder_options_find_property (JsonSerializable *serializable,
                                const char       *name)
{
}

static GParamSpec **
builder_options_list_properties (JsonSerializable *serializable,
                                  guint            *n_pspecs)
{
}

static void
builder_options_set_property (JsonSerializable *serializable,
                               GParamSpec       *pspec,
                               const GValue     *value)
{
}

static void
builder_options_get_property (JsonSerializable *serializable,
                               GParamSpec       *pspec,
                               GValue           *value)
{
}

*/

static void
serializable_iface_init (JsonSerializableIface *serializable_iface)
{
}

const char *
builder_options_get_cflags (BuilderOptions  *self)
{
  return self->cflags;
}
