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
#include "xdg-app-utils.h"

struct BuilderContext {
  GObject parent;

  GFile *app_dir;
  GFile *base_dir;
  SoupSession *soup_session;
  char *arch;

  BuilderOptions *options;
};

typedef struct {
  GObjectClass parent_class;
} BuilderContextClass;

G_DEFINE_TYPE (BuilderContext, builder_context, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_APP_DIR,
  PROP_BASE_DIR,
  LAST_PROP
};


static void
builder_context_finalize (GObject *object)
{
  BuilderContext *self = (BuilderContext *)object;

  g_clear_object (&self->app_dir);
  g_clear_object (&self->base_dir);
  g_clear_object (&self->soup_session);
  g_clear_object (&self->options);
  g_free (self->arch);

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

    case PROP_APP_DIR:
      g_value_set_object (value, self->app_dir);
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

    case PROP_APP_DIR:
      g_set_object (&self->app_dir, g_value_get_object (value));
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
                                   PROP_APP_DIR,
                                   g_param_spec_object ("app-dir",
                                                        "",
                                                        "",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
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
  return g_object_ref (self->base_dir);
}

GFile *
builder_context_get_app_dir (BuilderContext  *self)
{
  return g_object_ref (self->app_dir);
}

GFile *
builder_context_get_download_dir (BuilderContext  *self)
{
  return g_file_get_child (self->base_dir, "downloads");
}

SoupSession *
builder_context_get_soup_session (BuilderContext *self)
{
  if (self->soup_session == NULL)
    {
      const char *http_proxy;

      self->soup_session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, "xdg-app-builder ",
                                                          SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
                                                          SOUP_SESSION_USE_THREAD_CONTEXT, TRUE,
                                                          SOUP_SESSION_TIMEOUT, 60,
                                                          SOUP_SESSION_IDLE_TIMEOUT, 60,
                                                          NULL);
      http_proxy = g_getenv ("http_proxy");
      if (http_proxy)
        {
          g_autoptr(SoupURI) proxy_uri = soup_uri_new (http_proxy);
          if (!proxy_uri)
            g_warning ("Invalid proxy URI '%s'", http_proxy);
          else
            g_object_set (self->soup_session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
        }
    }

  return self->soup_session;
}

const char *
builder_context_get_arch (BuilderContext *self)
{
  if (self->arch == NULL)
    self->arch = g_strdup (xdg_app_get_arch ());

  return (const char *)self->arch;
}

void
builder_context_set_arch (BuilderContext *self,
                          const char     *arch)
{
  g_free (self->arch);
  self->arch = g_strdup (arch);
}

BuilderOptions *
builder_context_get_options (BuilderContext *self)
{
  return self->options;
}

void
builder_context_set_options (BuilderContext *self,
                             BuilderOptions *option)
{
  g_set_object (&self->options, option);
}

int
builder_context_get_n_cpu (BuilderContext *self)
{
  return (int)sysconf (_SC_NPROCESSORS_ONLN);
}

BuilderContext *
builder_context_new (GFile *base_dir,
                     GFile *app_dir)
{
  return g_object_new (BUILDER_TYPE_CONTEXT,
                       "base-dir", base_dir,
                       "app-dir", app_dir,
                       NULL);
}
