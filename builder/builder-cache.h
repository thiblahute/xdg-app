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

#ifndef __BUILDER_CACHE_H__
#define __BUILDER_CACHE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct BuilderCache BuilderCache;

#define BUILDER_TYPE_CACHE (builder_cache_get_type())
#define BUILDER_CACHE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUILDER_TYPE_CACHE, BuilderCache))
#define BUILDER_IS_CACHE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUILDER_TYPE_CACHE))

GType builder_cache_get_type (void);

BuilderCache *builder_cache_new             (GFile         *cache_dir,
                                             GFile         *app_dir,
                                             const char    *branch);
void          builder_cache_disable_lookups (BuilderCache  *self);
gboolean      builder_cache_open            (BuilderCache  *self,
                                             GError       **error);
GChecksum *   builder_cache_get_checksum    (BuilderCache  *self);
gboolean      builder_cache_lookup          (BuilderCache  *self);
gboolean      builder_cache_commit          (BuilderCache  *self,
                                             const char    *body,
                                             GError       **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(BuilderCache, g_object_unref)

G_END_DECLS

#endif /* __BUILDER_CACHE_H__ */
