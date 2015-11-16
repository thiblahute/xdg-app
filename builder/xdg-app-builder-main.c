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

#include "config.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gio/gio.h>
#include "libglnx/libglnx.h"
#include "libgsystem.h"

#include "builder-manifest.h"

static gboolean opt_verbose;
static gboolean opt_version;

static GOptionEntry entries[] = {
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &opt_verbose, "Print debug information during command processing", NULL },
  { "version", 0, 0, G_OPTION_ARG_NONE, &opt_version, "Print version information and exit", NULL },
  { NULL }
};

static void
message_handler (const gchar *log_domain,
                 GLogLevelFlags log_level,
                 const gchar *message,
                 gpointer user_data)
{
  /* Make this look like normal console output */
  if (log_level & G_LOG_LEVEL_DEBUG)
    g_printerr ("XAB: %s\n", message);
  else
    g_printerr ("%s: %s\n", g_get_prgname (), message);
}

int
main (int    argc,
      char **argv)
{
  g_autofree const char *old_env = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(BuilderManifest) manifest = NULL;
  BuilderOptions *options;
  GOptionContext *context;
  GList *modules, *l;
  g_autofree gchar *json = NULL;

  setlocale (LC_ALL, "");

  g_log_set_handler (NULL, G_LOG_LEVEL_MESSAGE, message_handler, NULL);

  g_set_prgname (argv[0]);

  /* avoid gvfs (http://bugzilla.gnome.org/show_bug.cgi?id=526454) */
  old_env = g_strdup (g_getenv ("GIO_USE_VFS"));
  g_setenv ("GIO_USE_VFS", "local", TRUE);
  g_vfs_get_default ();
  if (old_env)
    g_setenv ("GIO_USE_VFS", old_env, TRUE);
  else
    g_unsetenv ("GIO_USE_VFS");

  context = g_option_context_new ("- application builder helper");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("option parsing failed: %s\n", error->message);
      return 1;
    }

  if (opt_verbose)
    g_log_set_handler (NULL, G_LOG_LEVEL_DEBUG, message_handler, NULL);

  if (argc < 2)
    {
      g_printerr ("No filename");
      return 1;
    }

  if (!g_file_get_contents (argv[1],
                            &json, NULL, &error))
    {
      g_printerr ("Can't parse json: %s\n", error->message);
      return 1;
    }

  manifest = (BuilderManifest *)json_gobject_from_data (BUILDER_TYPE_MANIFEST, json, -1, &error);
  if (manifest == NULL)
    {
      g_printerr ("Can't deserialize manifest: %s\n", error->message);
      return 1;
    }

  g_print ("app id %s\n", builder_manifest_get_app_id (manifest));
  options = builder_manifest_get_build_options (manifest);
  g_print ("options %p\n", options);
  if (options)
    g_print ("cflags %s\n", builder_options_get_cflags (options));
  modules = builder_manifest_get_modules (manifest);
  g_print ("modules\n");
  for (l = modules; l != NULL; l = l->next)
    {
      BuilderModule *m = l->data;
      GList *sources, *ll;
      g_print ("  %s (%p)\n", builder_module_get_name (m), m);
      g_print ("  sources\n");
      sources = builder_module_get_sources (m);
      for (ll = sources; ll != NULL; ll = ll->next)
        {
          BuilderSource *s = ll->data;
          g_print ("    %s (%p)\n", g_type_name_from_instance ((GTypeInstance *)s), s);
        }
    }

  return 0;
}
