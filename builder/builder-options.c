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
#include <string.h>
#include <stdlib.h>
#include <sys/statfs.h>

#include "builder-options.h"
#include "builder-context.h"
#include "builder-utils.h"

struct BuilderOptions {
  GObject parent;

  char *cflags;
  char *cxxflags;
  char **env;
  GHashTable *arch;
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
  PROP_CXXFLAGS,
  PROP_ENV,
  PROP_ARCH,
  LAST_PROP
};


static void
builder_options_finalize (GObject *object)
{
  BuilderOptions *self = (BuilderOptions *)object;

  g_free (self->cflags);
  g_free (self->cxxflags);
  g_strfreev (self->env);

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

    case PROP_CXXFLAGS:
      g_value_set_string (value, self->cxxflags);
      break;

    case PROP_ENV:
      g_value_set_boxed (value, self->env);
      break;

    case PROP_ARCH:
      g_value_set_boxed (value, self->arch);
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
  gchar **tmp;

  switch (prop_id)
    {
    case PROP_CFLAGS:
      g_clear_pointer (&self->cflags, g_free);
      self->cflags = g_value_dup_string (value);
      break;

    case PROP_CXXFLAGS:
      g_clear_pointer (&self->cxxflags, g_free);
      self->cxxflags = g_value_dup_string (value);
      break;

    case PROP_ENV:
      tmp = self->env;
      self->env = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

    case PROP_ARCH:
      g_hash_table_destroy (self->arch);
      /* NOTE: This takes ownership of the hash table! */
      self->arch = g_value_dup_boxed (value);
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
  g_object_class_install_property (object_class,
                                   PROP_CXXFLAGS,
                                   g_param_spec_string ("cxxflags",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_ENV,
                                   g_param_spec_boxed ("env",
                                                       "",
                                                       "",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_ARCH,
                                   g_param_spec_boxed ("arch",
                                                       "",
                                                       "",
                                                       G_TYPE_HASH_TABLE,
                                                       G_PARAM_READWRITE));
}

static void
builder_options_init (BuilderOptions *self)
{
  self->arch = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static gboolean
builder_options_deserialize_property (JsonSerializable *serializable,
                                       const gchar      *property_name,
                                       GValue           *value,
                                       GParamSpec       *pspec,
                                       JsonNode         *property_node)
{
  if (strcmp (property_name, "arch") == 0)
    {
      if (JSON_NODE_TYPE (property_node) == JSON_NODE_NULL)
        {
          g_value_set_boxed (value, NULL);
          return TRUE;
        }
      else if (JSON_NODE_TYPE (property_node) == JSON_NODE_OBJECT)
        {
          JsonObject *object = json_node_get_object (property_node);
          g_autoptr(GPtrArray) env = g_ptr_array_new_with_free_func (g_free);
          g_autoptr(GHashTable) hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
          g_autoptr(GList) members = NULL;
          GList *l;

          members = json_object_get_members (object);
          for (l = members; l != NULL; l = l->next)
            {
              const char *member_name = l->data;
              JsonNode *val;
              GObject *option;

              val = json_object_get_member (object, member_name);
              option = json_gobject_deserialize (BUILDER_TYPE_OPTIONS, val);
              if (option == NULL)
                return FALSE;

              g_hash_table_insert (hash, g_strdup (member_name), option);
            }

          g_value_set_boxed (value, hash);
          return TRUE;
        }

      return FALSE;
    }
  else if (strcmp (property_name, "env") == 0)
    {
      if (JSON_NODE_TYPE (property_node) == JSON_NODE_NULL)
        {
          g_value_set_boxed (value, NULL);
          return TRUE;
        }
      else if (JSON_NODE_TYPE (property_node) == JSON_NODE_OBJECT)
        {
          JsonObject *object = json_node_get_object (property_node);
          g_autoptr(GPtrArray) env = g_ptr_array_new_with_free_func (g_free);
          g_autoptr(GList) members = NULL;
          GList *l;

          members = json_object_get_members (object);
          for (l = members; l != NULL; l = l->next)
            {
              const char *member_name = l->data;
              JsonNode *val;
              const char *val_str;

              val = json_object_get_member (object, member_name);
              val_str = json_node_get_string (val);
              if (val_str == NULL)
                return FALSE;

              g_ptr_array_add (env, g_strdup_printf ("%s=%s", member_name, val_str));
            }

          g_ptr_array_add (env, NULL);
          g_value_set_boxed (value, g_ptr_array_free (g_steal_pointer (&env), FALSE));
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
  serializable_iface->deserialize_property = builder_options_deserialize_property;
}

static GList *
get_arched_options (BuilderOptions  *self, BuilderContext *context)
{
  GList *options = NULL;
  const char *arch = builder_context_get_arch (context);
  BuilderOptions *arch_options;

  arch_options = g_hash_table_lookup (self->arch, arch);
  if (arch_options)
    options = g_list_prepend (options, arch_options);

  options = g_list_prepend (options, self);

  return options;
}

static GList *
get_all_options (BuilderOptions  *self, BuilderContext *context)
{
  GList *options = NULL;
  BuilderOptions *global_options = builder_context_get_options (context);

  if (self)
    options = get_arched_options (self, context);

  if (global_options && global_options != self)
    options = g_list_concat (options,  get_arched_options (global_options, context));

  return options;
}

const char *
builder_options_get_cflags (BuilderOptions *self, BuilderContext *context)
{
  g_autoptr(GList) options = get_all_options (self, context);
  GList *l;

  for (l = options; l != NULL; l = l->next)
    {
      BuilderOptions *o = l->data;
      if (o->cflags)
        return o->cflags;
    }

  return NULL;
}

const char *
builder_options_get_cxxflags (BuilderOptions *self, BuilderContext *context)
{
  g_autoptr(GList) options = get_all_options (self, context);
  GList *l;

  for (l = options; l != NULL; l = l->next)
    {
      BuilderOptions *o = l->data;
      if (o->cxxflags)
        return o->cxxflags;
    }

  return NULL;
}

char **
builder_options_get_env (BuilderOptions *self, BuilderContext *context)
{
  g_autoptr(GList) options = get_all_options (self, context);
  GList *l;
  int i;
  char **envp = NULL;

  for (l = options; l != NULL; l = l->next)
    {
      BuilderOptions *o = l->data;

      if (o->env)
        {
          for (i = 0; o->env[i] != NULL; i++)
            {
              const char *line = o->env[i];
              const char *eq = strchr (line, '=');
              const char *value = "";
              g_autofree char *key = NULL;

              if (eq)
                {
                  key = g_strndup (line, eq - line);
                  value = eq + 1;
                }
              else
                key = g_strdup (key);

              envp = g_environ_setenv (envp, key, value, FALSE);
            }
        }
    }

  return envp;
}

void
builder_options_checksum (BuilderOptions *self,
                          GChecksum      *checksum,
                          BuilderContext *context)
{
  BuilderOptions *arch_options;

  builder_checksum_str (checksum, BUILDER_OPTION_CHECKSUM_VERSION);
  builder_checksum_str (checksum, self->cflags);
  builder_checksum_str (checksum, self->cxxflags);
  builder_checksum_strv (checksum, self->env);

  arch_options = g_hash_table_lookup (self->arch, builder_context_get_arch (context));
  if (arch_options)
    builder_options_checksum (arch_options, checksum, context);
}
