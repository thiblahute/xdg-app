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

#ifndef __BUILDER_MANIFEST_H__
#define __BUILDER_MANIFEST_H__

#include <json-glib/json-glib.h>

#include "builder-options.h"
#include "builder-module.h"

G_BEGIN_DECLS

typedef struct BuilderManifest BuilderManifest;

#define BUILDER_TYPE_MANIFEST (builder_manifest_get_type())
#define BUILDER_MANIFEST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUILDER_TYPE_MANIFEST, BuilderManifest))
#define BUILDER_IS_MANIFEST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUILDER_TYPE_MANIFEST))

GType builder_manifest_get_type (void);

const char *    builder_manifest_get_app_id        (BuilderManifest  *self);
BuilderOptions *builder_manifest_get_build_options (BuilderManifest  *self);
GList *         builder_manifest_get_modules       (BuilderManifest  *self);

gboolean        builder_manifest_init_app_dir      (BuilderManifest  *self,
                                                    BuilderContext   *context,
                                                    GError          **error);
gboolean        builder_manifest_download          (BuilderManifest  *self,
                                                    BuilderContext   *context,
                                                    GError          **error);
gboolean        builder_manifest_build             (BuilderManifest  *self,
                                                    BuilderContext   *context,
                                                    GError          **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(BuilderManifest, g_object_unref)

G_END_DECLS

#endif /* __BUILDER_MANIFEST_H__ */
