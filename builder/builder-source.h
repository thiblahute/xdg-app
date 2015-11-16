/*
 * Copyright © 2015 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 */

#ifndef __BUILDER_SOURCE_H__
#define __BUILDER_SOURCE_H__

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

typedef struct BuilderSource BuilderSource;

#define BUILDER_TYPE_SOURCE (builder_source_get_type())
#define BUILDER_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUILDER_TYPE_SOURCE, BuilderSource))
#define BUILDER_IS_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUILDER_TYPE_SOURCE))

struct BuilderSource {
  GObject parent;

  char *dest;
};

typedef struct {
  GObjectClass parent_class;
} BuilderSourceClass;

GType builder_source_get_type (void);

BuilderSource * builder_source_from_json (JsonNode *node);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(BuilderSource, g_object_unref)

G_END_DECLS

#endif /* __BUILDER_SOURCE_H__ */
