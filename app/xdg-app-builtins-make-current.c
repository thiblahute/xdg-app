/*
 * Copyright © 2014 Red Hat, Inc
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
#include <unistd.h>
#include <string.h>

#include "libgsystem.h"
#include "libglnx/libglnx.h"

#include "xdg-app-builtins.h"
#include "xdg-app-utils.h"

static char *opt_arch;

static GOptionEntry options[] = {
  { "arch", 0, 0, G_OPTION_ARG_STRING, &opt_arch, "Arch to make current for", "ARCH" },
  { NULL }
};

gboolean
xdg_app_builtin_make_current_app (int argc, char **argv, GCancellable *cancellable, GError **error)
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(XdgAppDir) dir = NULL;
  g_autoptr(GFile) deploy_base = NULL;
  const char *app;
  const char *branch = "master";
  g_autofree char *ref = NULL;

  context = g_option_context_new ("APP BRANCH - Make branch of application current");

  if (!xdg_app_option_context_parse (context, options, &argc, &argv, 0, &dir, cancellable, error))
    return FALSE;

  if (argc < 3)
    return usage_error (context, "APP and BRANCH must be specified", error);

  app  = argv[1];
  branch = argv[2];

  if (!xdg_app_is_valid_name (app))
    return xdg_app_fail (error, "'%s' is not a valid application name", app);

  if (!xdg_app_is_valid_branch (branch))
    return xdg_app_fail (error, "'%s' is not a valid branch name", branch);

  ref = xdg_app_build_app_ref (app, branch, opt_arch);

  deploy_base = xdg_app_dir_get_deploy_dir (dir, ref);
  if (!g_file_query_exists (deploy_base, cancellable))
    return xdg_app_fail (error, "App %s branch %s is not installed", app, branch);

  if (!xdg_app_dir_make_current_ref (dir, ref, cancellable, error))
    return FALSE;

  if (!xdg_app_dir_update_exports (dir, app, cancellable, error))
    return FALSE;

  return TRUE;
}
