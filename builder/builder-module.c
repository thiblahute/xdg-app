/* builder-module.c
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

#include "builder-module.h"

struct BuilderModule {
  GObject parent;

  char *name;
  char **config_opts;
  gboolean rm_configure;
  GList *sources;
};

typedef struct {
  GObjectClass parent_class;
} BuilderModuleClass;

static void serializable_iface_init (JsonSerializableIface *serializable_iface);

G_DEFINE_TYPE_WITH_CODE (BuilderModule, builder_module, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, serializable_iface_init));

enum {
  PROP_0,
  PROP_NAME,
  PROP_RM_CONFIGURE,
  PROP_CONFIG_OPTS,
  PROP_SOURCES,
  LAST_PROP
};


static void
builder_module_finalize (GObject *object)
{
  BuilderModule *self = (BuilderModule *)object;

  g_free (self->name);
  g_list_free_full (self->sources, g_object_unref);

  G_OBJECT_CLASS (builder_module_parent_class)->finalize (object);
}

static void
builder_module_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BuilderModule *self = BUILDER_MODULE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_RM_CONFIGURE:
      g_value_set_boolean (value, self->rm_configure);
      break;

    case PROP_CONFIG_OPTS:
      g_value_set_boxed (value, self->config_opts);
      break;

    case PROP_SOURCES:
      g_value_set_pointer (value, self->sources);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_module_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BuilderModule *self = BUILDER_MODULE (object);
  gchar **tmp;

  switch (prop_id)
    {
    case PROP_NAME:
      g_clear_pointer (&self->name, g_free);
      self->name = g_value_dup_string (value);
      break;

    case PROP_RM_CONFIGURE:
      self->rm_configure = g_value_get_boolean (value);
      break;

    case PROP_CONFIG_OPTS:
      tmp = self->config_opts;
      self->config_opts = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

     case PROP_SOURCES:
      g_list_free_full (self->sources, g_object_unref);
      /* NOTE: This takes ownership of the list! */
      self->sources = g_value_get_pointer (value);
      break;

   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
builder_module_class_init (BuilderModuleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = builder_module_finalize;
  object_class->get_property = builder_module_get_property;
  object_class->set_property = builder_module_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_RM_CONFIGURE,
                                   g_param_spec_boolean ("rm-configure",
                                                         "",
                                                         "",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_SOURCES,
                                   g_param_spec_pointer ("sources",
                                                         "",
                                                         "",
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_CONFIG_OPTS,
                                   g_param_spec_boxed ("config-opts",
                                                       "",
                                                       "",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE));
}

static void
builder_module_init (BuilderModule *self)
{
}

static gboolean
builder_module_deserialize_property (JsonSerializable *serializable,
                                     const gchar      *property_name,
                                     GValue           *value,
                                     GParamSpec       *pspec,
                                     JsonNode         *property_node)
{
  if (strcmp (property_name, "sources") == 0)
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
          GList *sources = NULL;
          BuilderSource *source;

          for (i = 0; i < array_len; i++)
            {
              JsonNode *element_node = json_array_get_element (array, i);

              if (JSON_NODE_TYPE (element_node) != JSON_NODE_OBJECT)
                {
                  g_list_free_full (sources, g_object_unref);
                  return FALSE;
                }

              source = builder_source_from_json (element_node);
              if (source == NULL)
                {
                  g_list_free_full (sources, g_object_unref);
                  return FALSE;
                }

              sources = g_list_prepend (sources, source);
            }

          g_value_set_pointer (value, g_list_reverse (sources));

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
  serializable_iface->deserialize_property = builder_module_deserialize_property;
}

const char *
builder_module_get_name (BuilderModule  *self)
{
  return self->name;
}

GList *
builder_module_get_sources (BuilderModule  *self)
{
  return self->sources;
}

gboolean
builder_module_download_sources (BuilderModule *self,
                                 BuilderContext *context,
                                 GError **error)
{
  GList *l;

  for (l = self->sources; l != NULL; l = l->next)
    {
      BuilderSource *source = l->data;

      if (!builder_source_download (source, context, error))
        return FALSE;
    }

  return TRUE;
}

gboolean
builder_module_extract_sources (BuilderModule *self,
                                GFile *dest,
                                BuilderContext *context,
                                GError **error)
{
  GList *l;

  if (!g_file_query_exists (dest, NULL) &&
      !g_file_make_directory_with_parents (dest, NULL, error))
    return FALSE;

  for (l = self->sources; l != NULL; l = l->next)
    {
      BuilderSource *source = l->data;

      if (!builder_source_extract  (source, dest, context, error))
        return FALSE;
    }

  return TRUE;
}

static const char skip_arg[] = "skip";
static const char strv_arg[] = "strv";

static gboolean
build (GFile *app_dir,
       GFile *source_dir,
       GFile *cwd_dir,
       GError **error,
       const gchar            *argv1,
       ...)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subp = NULL;
  g_autofree char *app_path = g_file_get_path (app_dir);
  g_autofree char *cwd = NULL;
  GPtrArray *args;
  const gchar *arg;
  const gchar **argv;
  g_autofree char *commandline = NULL;
  va_list ap;
  int i;

  args = g_ptr_array_new ();
  g_ptr_array_add (args, "xdg-app");
  g_ptr_array_add (args, "build");
  g_ptr_array_add (args, "--env=NOCONFIGURE=1");
  g_ptr_array_add (args, app_path);
  va_start (ap, argv1);
  g_ptr_array_add (args, (gchar *) argv1);
  while ((arg = va_arg (ap, const gchar *)))
    {
      if (arg == strv_arg)
        {
          argv = va_arg (ap, const gchar **);
          if (argv != NULL)
            {
              for (i = 0; argv[i] != NULL; i++)
                g_ptr_array_add (args, (gchar *) argv[i]);
            }
        }
      else if (arg != skip_arg)
        g_ptr_array_add (args, (gchar *) arg);
    }
  g_ptr_array_add (args, NULL);
  va_end (ap);

  commandline = g_strjoinv (" ", (char **) args->pdata);
  g_print ("Starting: %s\n", commandline);

  launcher = g_subprocess_launcher_new (0);

  if (cwd_dir)
    cwd = g_file_get_path (cwd_dir);
  else
    cwd = g_file_get_path (source_dir);
  g_subprocess_launcher_set_cwd (launcher, cwd);

  subp = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) args->pdata, error);
  g_ptr_array_free (args, TRUE);

  if (subp == NULL ||
      !g_subprocess_wait_check (subp, NULL, error))
    return FALSE;

  return TRUE;
}


gboolean
builder_module_build (BuilderModule *self,
                      GFile *app_dir,
                      GFile *source_dir,
                      BuilderContext *context,
                      BuilderOptions  *global_options,
                      GError **error)
{
  g_autofree char *make_j = NULL;
  g_autofree char *make_l = NULL;
  g_autofree char *configure_content = NULL;
  g_autofree char *makefile_content = NULL;
  g_autoptr(GFile) configure_file = g_file_get_child (source_dir, "configure");
  const char *makefile_names[] =  {"Makefile", "makefile", "GNUmakefile", NULL};
  g_autoptr(GFile) build_dir = NULL;
  const char *configure_cmd;
  gboolean has_configure;
  gboolean var_require_builddir;
  gboolean has_notparallel;
  gboolean use_builddir;
  int i;

  if (self->rm_configure)
    {
      if (!g_file_delete (configure_file, NULL, error))
        return FALSE;
    }

  has_configure = g_file_query_exists (configure_file, NULL);

  if (!has_configure)
    {
      const char *autogen_names[] =  {"autogen", "autogen.sh", "bootstrap", NULL};
      g_autofree char *autogen_cmd = NULL;

      for (i = 0; autogen_names[i] != NULL; i++)
        {
          g_autoptr(GFile) autogen_file = g_file_get_child (source_dir, autogen_names[i]);
          if (g_file_query_exists (autogen_file, NULL))
            {
              autogen_cmd = g_strdup_printf ("./%s", autogen_names[i]);
              break;
            }
        }

      if (autogen_cmd == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Can't find autogen, autogen.sh or bootstrap");
          return FALSE;
        }

      if (!build (app_dir, source_dir, NULL, error,
                  autogen_cmd, NULL))
        return FALSE;
    }

  if (!g_file_load_contents (configure_file, NULL, &configure_content, NULL, NULL, error))
    return FALSE;

  var_require_builddir = strstr (configure_content, "buildapi-variable-require-builddir") != NULL;
  use_builddir = var_require_builddir;

  if (use_builddir)
    {
      build_dir = g_file_get_child (source_dir, "_build");
      configure_cmd = "../configure";
    }
  else
    {
      build_dir = g_object_ref (source_dir);
      configure_cmd = "./configure";
    }

  if (!build (app_dir, source_dir, build_dir, error,
              configure_cmd, "--prefix=/app", strv_arg, self->config_opts, NULL))
    return FALSE;

  for (i = 0; makefile_names[i] != NULL; i++)
    {
      g_autoptr(GFile) makefile_file = g_file_get_child (build_dir, makefile_names[i]);
      if (g_file_query_exists (makefile_file, NULL))
        {
          if (!g_file_load_contents (makefile_file, NULL, &makefile_content, NULL, NULL, error))
            return FALSE;
          break;
        }
    }

  if (makefile_content == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Can't find makefile");
      return FALSE;
    }

  has_notparallel =
    g_str_has_prefix (makefile_content, ".NOTPARALLEL") ||
    (strstr (makefile_content, "\n.NOTPARALLEL") != NULL);

  if (!has_notparallel)
    {
      make_j = g_strdup_printf ("-j%d", builder_context_get_n_cpu (context));
      make_l = g_strdup_printf ("-l%d", 2*builder_context_get_n_cpu (context));
    }

  if (!build (app_dir, source_dir, build_dir, error,
              "make", "all", make_j?make_j:skip_arg, make_l?make_l:skip_arg, NULL))
    return FALSE;

  if (!build (app_dir, source_dir, build_dir, error,
              "make", "install", NULL))
    return FALSE;

  return TRUE;
}
