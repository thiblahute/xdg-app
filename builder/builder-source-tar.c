/* builder-source-tar.c
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

#include "builder-source-tar.h"

struct BuilderSourceTar {
  BuilderSource parent;

  char *url;
};

typedef struct {
  BuilderSourceClass parent_class;
} BuilderSourceTarClass;

G_DEFINE_TYPE (BuilderSourceTar, builder_source_tar, BUILDER_TYPE_SOURCE);

enum {
  PROP_0,
  PROP_URL,
  LAST_PROP
};

static void
builder_source_tar_finalize (GObject *object)
{
  BuilderSourceTar *self = (BuilderSourceTar *)object;

  g_free (self->url);

  G_OBJECT_CLASS (builder_source_tar_parent_class)->finalize (object);
}

static void
builder_source_tar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BuilderSourceTar *self = BUILDER_SOURCE_TAR (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_source_tar_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BuilderSourceTar *self = BUILDER_SOURCE_TAR (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_free (self->url);
      self->url = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_source_tar_class_init (BuilderSourceTarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = builder_source_tar_finalize;
  object_class->get_property = builder_source_tar_get_property;
  object_class->set_property = builder_source_tar_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URL,
                                   g_param_spec_string ("url",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}

static void
builder_source_tar_init (BuilderSourceTar *self)
{
}
