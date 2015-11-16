/* builder-context.c
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

#include "builder-context.h"

struct BuilderContext {
  GObject parent;

  GFile *base_dir;
};

typedef struct {
  GObjectClass parent_class;
} BuilderContextClass;

G_DEFINE_TYPE (BuilderContext, builder_context, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_BASE_DIR,
  LAST_PROP
};


static void
builder_context_finalize (GObject *object)
{
  BuilderContext *self = (BuilderContext *)object;

  g_clear_object (&self->base_dir);

  G_OBJECT_CLASS (builder_context_parent_class)->finalize (object);
}

static void
builder_context_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BuilderContext *self = BUILDER_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_BASE_DIR:
      g_value_set_object (value, self->base_dir);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_context_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BuilderContext *self = BUILDER_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_BASE_DIR:
      g_set_object (&self->base_dir, g_value_get_object (value));
      break;

   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_context_class_init (BuilderContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = builder_context_finalize;
  object_class->get_property = builder_context_get_property;
  object_class->set_property = builder_context_set_property;

  g_object_class_install_property (object_class,
                                   PROP_BASE_DIR,
                                   g_param_spec_object ("base-dir",
                                                        "",
                                                        "",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}

static void
builder_context_init (BuilderContext *self)
{
}

GFile *
builder_context_get_base_dir (BuilderContext  *self)
{
  return self->base_dir;
}

BuilderContext *
builder_context_new (GFile *base_dir)
{
  return g_object_new (BUILDER_TYPE_CONTEXT,
                       "base-dir", base_dir,
                       NULL);
}
