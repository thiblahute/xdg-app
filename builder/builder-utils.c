/* builder-utils.c
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

#include "builder-utils.h"

char *
builder_uri_to_filename (const char *uri)
{
  GString *s;
  const char *p;
  gboolean saw_slash = FALSE;
  gboolean saw_after_slash = FALSE;

  s = g_string_new ("");

  for (p = uri; *p != 0; p++)
    {
      if (*p == '/')
        {
          saw_slash = TRUE;
          if (saw_after_slash) /* Skip first slashes */
            g_string_append_c (s, '_');
          continue;
        }
      else if (saw_slash)
        saw_after_slash = TRUE;
      g_string_append_c (s, *p);
    }

  return g_string_free (s, FALSE);
}
