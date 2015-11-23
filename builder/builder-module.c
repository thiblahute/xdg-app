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

#include <gio/gio.h>
#include "libglnx/libglnx.h"
#include "libgsystem.h"

#include "builder-utils.h"
#include "builder-module.h"

struct BuilderModule {
  GObject parent;

  char *name;
  char **config_opts;
  char **make_args;
  char **make_install_args;
  gboolean rm_configure;
  gboolean no_autogen;
  BuilderOptions *build_options;
  GPtrArray *changes;
  char **cleanup;
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
  PROP_NO_AUTOGEN,
  PROP_CONFIG_OPTS,
  PROP_MAKE_ARGS,
  PROP_MAKE_INSTALL_ARGS,
  PROP_SOURCES,
  PROP_BUILD_OPTIONS,
  PROP_CLEANUP,
  LAST_PROP
};


static void
builder_module_finalize (GObject *object)
{
  BuilderModule *self = (BuilderModule *)object;

  g_free (self->name);
  g_strfreev (self->config_opts);
  g_strfreev (self->make_args);
  g_strfreev (self->make_install_args);
  g_clear_object (&self->build_options);
  g_list_free_full (self->sources, g_object_unref);
  g_strfreev (self->cleanup);

  g_ptr_array_unref (self->changes);

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

    case PROP_NO_AUTOGEN:
      g_value_set_boolean (value, self->no_autogen);
      break;

    case PROP_CONFIG_OPTS:
      g_value_set_boxed (value, self->config_opts);
      break;

    case PROP_MAKE_ARGS:
      g_value_set_boxed (value, self->make_args);
      break;

    case PROP_MAKE_INSTALL_ARGS:
      g_value_set_boxed (value, self->make_install_args);
      break;

    case PROP_BUILD_OPTIONS:
      g_value_set_object (value, self->build_options);
      break;

    case PROP_SOURCES:
      g_value_set_pointer (value, self->sources);
      break;

    case PROP_CLEANUP:
      g_value_set_boxed (value, self->cleanup);
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

    case PROP_NO_AUTOGEN:
      self->no_autogen = g_value_get_boolean (value);
      break;

    case PROP_CONFIG_OPTS:
      tmp = self->config_opts;
      self->config_opts = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

    case PROP_MAKE_ARGS:
      tmp = self->make_args;
      self->make_args = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

    case PROP_MAKE_INSTALL_ARGS:
      tmp = self->make_install_args;
      self->make_install_args = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

    case PROP_BUILD_OPTIONS:
      g_set_object (&self->build_options,  g_value_get_object (value));
      break;

     case PROP_SOURCES:
      g_list_free_full (self->sources, g_object_unref);
      /* NOTE: This takes ownership of the list! */
      self->sources = g_value_get_pointer (value);
      break;

    case PROP_CLEANUP:
      tmp = self->cleanup;
      self->cleanup = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
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
                                   PROP_NO_AUTOGEN,
                                   g_param_spec_boolean ("no-autogen",
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
  g_object_class_install_property (object_class,
                                   PROP_MAKE_ARGS,
                                   g_param_spec_boxed ("make-args",
                                                       "",
                                                       "",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_MAKE_INSTALL_ARGS,
                                   g_param_spec_boxed ("make-install-args",
                                                       "",
                                                       "",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_BUILD_OPTIONS,
                                   g_param_spec_object ("build-options",
                                                        "",
                                                        "",
                                                        BUILDER_TYPE_OPTIONS,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_CLEANUP,
                                   g_param_spec_boxed ("cleanup",
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
       char **env_vars,
       GError **error,
       const gchar            *argv1,
       ...)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subp = NULL;
  g_autofree char *cwd = NULL;
  g_autoptr(GPtrArray) args = NULL;
  const gchar *arg;
  const gchar **argv;
  g_autofree char *commandline = NULL;
  va_list ap;
  int i;

  args = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (args, g_strdup ("xdg-app"));
  g_ptr_array_add (args, g_strdup ("build"));
  if (env_vars)
    {
      for (i = 0; env_vars[i] != NULL; i++)
        g_ptr_array_add (args, g_strdup_printf ("--env=%s", env_vars[i]));
    }
  g_ptr_array_add (args, g_file_get_path (app_dir));
  va_start (ap, argv1);
  g_ptr_array_add (args, g_strdup (argv1));
  while ((arg = va_arg (ap, const gchar *)))
    {
      if (arg == strv_arg)
        {
          argv = va_arg (ap, const gchar **);
          if (argv != NULL)
            {
              for (i = 0; argv[i] != NULL; i++)
                g_ptr_array_add (args, g_strdup (argv[i]));
            }
        }
      else if (arg != skip_arg)
        g_ptr_array_add (args, g_strdup (arg));
    }
  g_ptr_array_add (args, NULL);
  va_end (ap);

  commandline = g_strjoinv (" ", (char **) args->pdata);
  g_print ("Running: %s\n", commandline);

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
                      BuilderContext *context,
                      GError **error)
{
  GFile *app_dir = builder_context_get_app_dir (context);
  g_autofree char *make_j = NULL;
  g_autofree char *make_l = NULL;
  g_autofree char *makefile_content = NULL;
  g_autoptr(GFile) configure_file = NULL;
  const char *makefile_names[] =  {"Makefile", "makefile", "GNUmakefile", NULL};
  g_autoptr(GFile) build_dir = NULL;
  gboolean has_configure;
  gboolean var_require_builddir;
  gboolean has_notparallel;
  gboolean use_builddir;
  int i;
  g_auto(GStrv) env = NULL;
  const char *cflags, *cxxflags;
  g_autofree char *buildname = NULL;
  g_autoptr(GFile) source_dir = NULL;
  g_autoptr(GFile) source_dir_template = NULL;
  g_autofree char *source_dir_path = NULL;

  buildname = g_strdup_printf ("build-%s-XXXXXX", self->name);

  source_dir_template = g_file_get_child (builder_context_get_state_dir (context),
                                          buildname);
  source_dir_path = g_file_get_path (source_dir_template);;

  if (g_mkdtemp (source_dir_path) == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Can't create build directory");
      return FALSE;
    }

  source_dir = g_file_new_for_path (source_dir_path);

  g_print ("Building module %s in %s\n", self->name, source_dir_path);

  if (!builder_module_extract_sources (self, source_dir, context, error))
    return FALSE;

  env = builder_options_get_env (self->build_options, context);

  cflags = builder_options_get_cflags (self->build_options, context);
  if (cflags)
    env = g_environ_setenv (env, "CFLAGS", cflags, TRUE);

  cxxflags = builder_options_get_cxxflags (self->build_options, context);
  if (cxxflags)
    env = g_environ_setenv (env, "CXXFLAGS", cxxflags, TRUE);

  configure_file = g_file_get_child (source_dir, "configure");

  if (self->rm_configure)
    {
      if (!g_file_delete (configure_file, NULL, error))
        return FALSE;
    }

  has_configure = g_file_query_exists (configure_file, NULL);

  if (!has_configure && !self->no_autogen)
    {
      const char *autogen_names[] =  {"autogen", "autogen.sh", "bootstrap", NULL};
      g_autofree char *autogen_cmd = NULL;
      g_auto(GStrv) env_with_noconfigure = NULL;

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

      env_with_noconfigure = g_environ_setenv (g_strdupv (env), "NOCONFIGURE", "1", TRUE);
      if (!build (app_dir, source_dir, NULL, env_with_noconfigure, error,
                  autogen_cmd, NULL))
        return FALSE;

      if (!g_file_query_exists (configure_file, NULL))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "autogen did not create configure");
          return FALSE;
        }

      has_configure = TRUE;
    }

  if (has_configure)
    {
      const char *configure_cmd;
      g_autofree char *configure_content = NULL;

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

      if (!build (app_dir, source_dir, build_dir, env, error,
                  configure_cmd, "--prefix=/app", strv_arg, self->config_opts, NULL))
        return FALSE;
    }
  else
    build_dir = g_object_ref (source_dir);

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

  if (!build (app_dir, source_dir, build_dir, env, error,
              "make", "all", make_j?make_j:skip_arg, make_l?make_l:skip_arg, strv_arg, self->make_args, NULL))
    return FALSE;

  if (!build (app_dir, source_dir, build_dir, env, error,
              "make", "install", strv_arg, self->make_install_args, NULL))
    return FALSE;

  if (!gs_shutil_rm_rf (source_dir, NULL, error))
    return FALSE;

  return TRUE;
}

void
builder_module_checksum (BuilderModule  *self,
                         GChecksum      *checksum,
                         BuilderContext *context)
{
  GList *l;

  builder_checksum_str (checksum, BUILDER_MODULE_CHECKSUM_VERSION);
  builder_checksum_str (checksum, self->name);
  builder_checksum_strv (checksum, self->config_opts);
  builder_checksum_strv (checksum, self->make_args);
  builder_checksum_strv (checksum, self->make_install_args);
  builder_checksum_boolean (checksum, self->rm_configure);
  builder_checksum_boolean (checksum, self->no_autogen);

  for (l = self->sources; l != NULL; l = l->next)
    {
      BuilderSource *source = l->data;

      builder_source_checksum (source, checksum, context);
    }
}

void
builder_module_checksum_cleanup (BuilderModule  *self,
                                 GChecksum      *checksum,
                                 BuilderContext *context)
{
  builder_checksum_str (checksum, BUILDER_MODULE_CHECKSUM_VERSION);
  builder_checksum_str (checksum, self->name);
  builder_checksum_strv (checksum, self->cleanup);
}

GPtrArray *
builder_module_get_changes (BuilderModule  *self)
{
  return self->changes;
}

void
builder_module_set_changes (BuilderModule  *self,
                            GPtrArray      *changes)
{
  if (self->changes != changes)
    {
      if (self->changes)
        g_ptr_array_unref (self->changes);
      self->changes = g_ptr_array_ref (changes);
    }
}

static void
collect_for_pattern (BuilderModule *self,
                     const char *pattern,
                     const char *path,
                     GHashTable *to_remove_ht)
{
  const char *rest;
  const char *last_slash;
  g_autofree char *dir = NULL;

  if (pattern[0] == '/')
    {
      /* Absolute path match */
      rest = path_prefix_match (pattern+1, path);
    }
  else
    {
      /* Basename match */
      last_slash = strrchr (path, '/');
      if (last_slash)
        {
          dir = g_strndup (path, last_slash - path);
          path = last_slash + 1;
        }
      rest = path_prefix_match (pattern, path);
    }

  while (rest != NULL)
    {
      const char *slash;
      g_autofree char *prefix = g_strndup (path, rest-path);
      g_autofree char *to_remove = NULL;
      if (dir)
        to_remove = g_strconcat (dir, "/", prefix, NULL);
      else
        to_remove = g_strdup (prefix);
      g_hash_table_insert (to_remove_ht, g_steal_pointer (&to_remove), GINT_TO_POINTER (1));
      while (*rest == '/')
        rest++;
      if (*rest == 0)
        break;
      slash = strchr (rest, '/');
      rest = slash ? slash : rest + strlen (rest);
    }
}

void
builder_module_cleanup_collect (BuilderModule *self,
                                char **global_patterns,
                                GHashTable *to_remove_ht)
{
  GPtrArray *changed_files;
  int i, j;

  changed_files = self->changes;
  for (i = 0; i < changed_files->len; i++)
    {
      const char *path = g_ptr_array_index (changed_files, i);

      if (global_patterns)
        {
          for (j = 0; global_patterns[j] != NULL; j++)
            collect_for_pattern (self, global_patterns[j], path, to_remove_ht);
        }

      if (self->cleanup)
        {
          for (j = 0; self->cleanup[j] != NULL; j++)
            collect_for_pattern (self, self->cleanup[j], path, to_remove_ht);
        }
    }
}
