/* builder-manifest.c
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

#include "builder-manifest.h"
#include "builder-utils.h"

struct BuilderManifest {
  GObject parent;

  char *app_id;
  char *version;
  char *runtime;
  char *runtime_version;
  char *sdk;
  BuilderOptions *build_options;
  GList *modules;
};

typedef struct {
  GObjectClass parent_class;
} BuilderManifestClass;

static void serializable_iface_init (JsonSerializableIface *serializable_iface);

G_DEFINE_TYPE_WITH_CODE (BuilderManifest, builder_manifest, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, serializable_iface_init));

enum {
  PROP_0,
  PROP_APP_ID,
  PROP_VERSION,
  PROP_RUNTIME,
  PROP_RUNTIME_VERSION,
  PROP_SDK,
  PROP_BUILD_OPTIONS,
  PROP_MODULES,
  LAST_PROP
};


static void
builder_manifest_finalize (GObject *object)
{
  BuilderManifest *self = (BuilderManifest *)object;

  g_free (self->app_id);
  g_free (self->version);
  g_free (self->runtime);
  g_free (self->runtime_version);
  g_free (self->sdk);
  g_clear_object (&self->build_options);
  g_list_free_full (self->modules, g_object_unref);

  G_OBJECT_CLASS (builder_manifest_parent_class)->finalize (object);
}

static void
builder_manifest_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BuilderManifest *self = BUILDER_MANIFEST(object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_value_set_string (value, self->app_id);
      break;

    case PROP_VERSION:
      g_value_set_string (value, self->version);
      break;

    case PROP_RUNTIME:
      g_value_set_string (value, self->runtime);
      break;

    case PROP_RUNTIME_VERSION:
      g_value_set_string (value, self->runtime_version);
      break;

    case PROP_SDK:
      g_value_set_string (value, self->sdk);
      break;

    case PROP_BUILD_OPTIONS:
      g_value_set_object (value, self->build_options);
      break;

    case PROP_MODULES:
      g_value_set_pointer (value, self->modules);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_manifest_set_property (GObject       *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BuilderManifest *self = BUILDER_MANIFEST (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_free (self->app_id);
      self->app_id = g_value_dup_string (value);
      break;

    case PROP_VERSION:
      g_free (self->version);
      self->version = g_value_dup_string (value);
      break;

    case PROP_RUNTIME:
      g_free (self->runtime);
      self->runtime = g_value_dup_string (value);
      break;

    case PROP_RUNTIME_VERSION:
      g_free (self->runtime_version);
      self->runtime_version = g_value_dup_string (value);
      break;

    case PROP_SDK:
      g_free (self->sdk);
      self->sdk = g_value_dup_string (value);
      break;

    case PROP_BUILD_OPTIONS:
      g_set_object (&self->build_options,  g_value_get_object (value));
      break;

    case PROP_MODULES:
      g_list_free_full (self->modules, g_object_unref);
      /* NOTE: This takes ownership of the list! */
      self->modules = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_manifest_class_init (BuilderManifestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = builder_manifest_finalize;
  object_class->get_property = builder_manifest_get_property;
  object_class->set_property = builder_manifest_set_property;

  g_object_class_install_property (object_class,
                                   PROP_APP_ID,
                                   g_param_spec_string ("app-id",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_VERSION,
                                   g_param_spec_string ("version",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_RUNTIME,
                                   g_param_spec_string ("runtime",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_RUNTIME_VERSION,
                                   g_param_spec_string ("runtime-version",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_SDK,
                                   g_param_spec_string ("sdk",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_BUILD_OPTIONS,
                                   g_param_spec_object ("build-options",
                                                        "",
                                                        "",
                                                        BUILDER_TYPE_OPTIONS,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_MODULES,
                                   g_param_spec_pointer ("modules",
                                                         "",
                                                         "",
                                                         G_PARAM_READWRITE));
}

static void
builder_manifest_init (BuilderManifest *self)
{
}

static gboolean
builder_manifest_deserialize_property (JsonSerializable *serializable,
                                       const gchar      *property_name,
                                       GValue           *value,
                                       GParamSpec       *pspec,
                                       JsonNode         *property_node)
{
  if (strcmp (property_name, "modules") == 0)
    {
      if (JSON_NODE_TYPE (property_node) == JSON_NODE_NULL)
        {
          g_value_set_pointer (value, NULL);
          return TRUE;
        }
      else if (JSON_NODE_TYPE (property_node) == JSON_NODE_ARRAY)
        {
          JsonArray *array = json_node_get_array (property_node);
          guint i, array_len = json_array_get_length (array);
          GList *modules = NULL;
          GObject *module;

          for (i = 0; i < array_len; i++)
            {
              JsonNode *element_node = json_array_get_element (array, i);

              if (JSON_NODE_TYPE (element_node) != JSON_NODE_OBJECT)
                {
                  g_list_free_full (modules, g_object_unref);
                  return FALSE;
                }

              module = json_gobject_deserialize (BUILDER_TYPE_MODULE, element_node);
              if (module == NULL)
                {
                  g_list_free_full (modules, g_object_unref);
                  return FALSE;
                }

              modules = g_list_prepend (modules, module);
            }

          g_value_set_pointer (value, g_list_reverse (modules));

          return TRUE;
        }

      return FALSE;
    }
  else
    return json_serializable_default_deserialize_property (serializable,
                                                           property_name,
                                                           value,
                                                           pspec, property_node);
}

static void
serializable_iface_init (JsonSerializableIface *serializable_iface)
{
  serializable_iface->deserialize_property = builder_manifest_deserialize_property;
}

const char *
builder_manifest_get_app_id  (BuilderManifest *self)
{
  return self->app_id;
}

BuilderOptions *
builder_manifest_get_build_options (BuilderManifest *self)
{
  return self->build_options;
}

GList *
builder_manifest_get_modules (BuilderManifest *self)
{
  return self->modules;
}

static const char *
builder_manifest_get_runtime_version (BuilderManifest *self)
{
  return self->runtime_version ? self->runtime_version : "master";
}

gboolean
builder_manifest_init_app_dir (BuilderManifest *self,
                               BuilderContext *context,
                               GError **error)
{
  g_autoptr(GFile) app_dir = builder_context_get_app_dir (context);
  g_autofree char *app_dir_path = g_file_get_path (app_dir);
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subp = NULL;
  g_autofree char *cwd = NULL;
  g_autoptr(GPtrArray) args = NULL;

  if (self->app_id == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "app id not specified");
      return NULL;
    }

  if (self->runtime == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "runtime not specified");
      return NULL;
    }

  if (self->sdk == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "sdk not specified");
      return NULL;
    }

  subp =
    g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                      error,
                      "xdg-app",
                      "build-init",
                      app_dir_path,
                      self->app_id,
                      self->sdk,
                      self->runtime,
                      builder_manifest_get_runtime_version (self),
                      NULL);

  if (subp == NULL ||
      !g_subprocess_wait_check (subp, NULL, error))
    return FALSE;

  return TRUE;
}

/* This gets the checksum of everything that globally affects the build */
void
builder_manifest_checksum (BuilderManifest *self,
                           GChecksum *checksum,
                           BuilderContext *context)
{
  builder_checksum_str (checksum, BUILDER_MANIFEST_CHECKSUM_VERSION);
  builder_checksum_str (checksum, self->app_id);
  /* No need to include version here, it doesn't affect the build */
  builder_checksum_str (checksum, self->runtime);
  builder_checksum_str (checksum, builder_manifest_get_runtime_version (self));
  builder_checksum_str (checksum, self->sdk);
  if (self->build_options)
    builder_options_checksum (self->build_options, checksum, context);
}

gboolean
builder_manifest_download (BuilderManifest *self,
                           BuilderContext *context,
                           GError **error)
{
  GList *l;

  g_print ("Downloading sources\n");
  for (l = self->modules; l != NULL; l = l->next)
    {
      BuilderModule *m = l->data;

      if (! builder_module_download_sources (m, context, error))
        return FALSE;
    }

  return TRUE;
}

gboolean
builder_manifest_build (BuilderManifest *self,
                        BuilderCache     *cache,
                        BuilderContext *context,
                        GError **error)
{
  GList *l;

  builder_context_set_options (context, self->build_options);

  g_print ("Starting build of %s\n", self->app_id ? self->app_id : "app");
  for (l = self->modules; l != NULL; l = l->next)
    {
      BuilderModule *m = l->data;

      builder_module_checksum (m, builder_cache_get_checksum (cache), context);

      if (!builder_cache_lookup (cache))
        {
          g_autofree char *body =
            g_strdup_printf ("Built %s\n", builder_module_get_name (m));
          if (!builder_module_build (m, context, error))
            return FALSE;
          if (!builder_cache_commit (cache, body, error))
            return FALSE;
        }
      else
        g_print ("Cache hit for %s, skipping build\n",
                 builder_module_get_name (m));
    }
  return TRUE;
}
