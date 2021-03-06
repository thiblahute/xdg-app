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
static char *opt_commit;
static gboolean opt_force_remove;

static GOptionEntry options[] = {
  { "arch", 0, 0, G_OPTION_ARG_STRING, &opt_arch, "Arch to update for", "ARCH" },
  { "commit", 0, 0, G_OPTION_ARG_STRING, &opt_commit, "Commit to deploy", "COMMIT" },
  { "force-remove", 0, 0, G_OPTION_ARG_NONE, &opt_force_remove, "Remove old files even if running", NULL },
  { NULL }
};

gboolean
xdg_app_builtin_update_runtime (int argc, char **argv, GCancellable *cancellable, GError **error)
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(XdgAppDir) dir = NULL;
  const char *runtime;
  const char *branch = "master";
  g_autofree char *previous_deployment = NULL;
  g_autofree char *ref = NULL;
  g_autofree char *repository = NULL;
  GError *my_error;

  context = g_option_context_new ("RUNTIME [BRANCH] - Update a runtime");

  if (!xdg_app_option_context_parse (context, options, &argc, &argv, 0, &dir, cancellable, error))
    return FALSE;

  if (argc < 2)
    return usage_error (context, "RUNTIME must be specified", error);

  runtime = argv[1];
  if (argc >= 3)
    branch = argv[2];

  if (!xdg_app_is_valid_name (runtime))
    return xdg_app_fail (error, "'%s' is not a valid runtime name", runtime);

  if (!xdg_app_is_valid_branch (branch))
    return xdg_app_fail (error, "'%s' is not a valid branch name", branch);

  ref = xdg_app_build_runtime_ref (runtime, branch, opt_arch);

  repository = xdg_app_dir_get_origin (dir, ref, cancellable, error);
  if (repository == NULL)
    return FALSE;

  if (!xdg_app_dir_pull (dir, repository, ref,
                         cancellable, error))
    return FALSE;

  previous_deployment = xdg_app_dir_read_active (dir, ref, cancellable);

  my_error = NULL;
  if (!xdg_app_dir_deploy (dir, ref, opt_commit, cancellable, &my_error))
    {
      if (g_error_matches (my_error, XDG_APP_DIR_ERROR, XDG_APP_DIR_ERROR_ALREADY_DEPLOYED))
        g_error_free (my_error);
      else
        {
          g_propagate_error (error, my_error);
          return FALSE;
        }
    }
  else
    {
      if (previous_deployment != NULL)
        {
          if (!xdg_app_dir_undeploy (dir, ref, previous_deployment,
                                     opt_force_remove,
                                     cancellable, error))
            return FALSE;

          if (!xdg_app_dir_prune (dir, cancellable, error))
            return FALSE;
        }
    }

  return TRUE;
}

gboolean
xdg_app_builtin_update_app (int argc, char **argv, GCancellable *cancellable, GError **error)
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(XdgAppDir) dir = NULL;
  const char *app;
  const char *branch = "master";
  g_autofree char *ref = NULL;
  g_autofree char *repository = NULL;
  g_autofree char *previous_deployment = NULL;
  GError *my_error;

  context = g_option_context_new ("APP [BRANCH] - Update an application");

  if (!xdg_app_option_context_parse (context, options, &argc, &argv, 0, &dir, cancellable, error))
    return FALSE;

  if (argc < 2)
    return usage_error (context, "APP must be specified", error);

  app = argv[1];
  if (argc >= 3)
    branch = argv[2];

  if (!xdg_app_is_valid_name (app))
    return xdg_app_fail (error, "'%s' is not a valid application name", app);

  if (!xdg_app_is_valid_branch (branch))
    return xdg_app_fail (error, "'%s' is not a valid branch name", branch);

  ref = xdg_app_build_app_ref (app, branch, opt_arch);

  repository = xdg_app_dir_get_origin (dir, ref, cancellable, error);
  if (repository == NULL)
    return FALSE;

  if (!xdg_app_dir_pull (dir, repository, ref,
                         cancellable, error))
    return FALSE;

  previous_deployment = xdg_app_dir_read_active (dir, ref, cancellable);

  my_error = NULL;
  if (!xdg_app_dir_deploy (dir, ref, opt_commit, cancellable, &my_error))
    {
      if (g_error_matches (my_error, XDG_APP_DIR_ERROR, XDG_APP_DIR_ERROR_ALREADY_DEPLOYED))
        g_error_free (my_error);
      else
        {
          g_propagate_error (error, my_error);
          return FALSE;
        }
    }
  else
    {
      if (previous_deployment != NULL)
        {
          if (!xdg_app_dir_undeploy (dir, ref, previous_deployment,
                                     opt_force_remove,
                                     cancellable, error))
            return FALSE;

          if (!xdg_app_dir_prune (dir, cancellable, error))
            return FALSE;
        }

      if (!xdg_app_dir_update_exports (dir, app, cancellable, error))
        return FALSE;
    }

  return  TRUE;
}
