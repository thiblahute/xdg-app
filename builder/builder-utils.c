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

#include <string.h>

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

void
builder_checksum_str (GChecksum *checksum, const char *str)
{
  /* We include the terminating zero so that we make
   * a difference between NULL and "". */

  if (str)
    g_checksum_update (checksum, (const guchar *)str, strlen (str) + 1);
  else
    /* Always add something so we can't be fooled by a sequence like
       NULL, "a" turning into "a", NULL. */
    g_checksum_update (checksum, (const guchar *)"\1", 1);
}

void
builder_checksum_strv (GChecksum *checksum, char **strv)
{
  int i;

  if (strv)
    {
      g_checksum_update (checksum, (const guchar *)"\1", 1);
      for (i = 0; strv[i] != NULL; i++)
        builder_checksum_str (checksum, strv[i]);
    }
  else
    g_checksum_update (checksum, (const guchar *)"\2", 1);
}

void
builder_checksum_boolean (GChecksum *checksum, gboolean val)
{
  if (val)
    g_checksum_update (checksum, (const guchar *)"\1", 1);
  else
    g_checksum_update (checksum, (const guchar *)"\0", 1);
}

void
builder_checksum_uint32 (GChecksum *checksum,
                         guint32 val)
{
  guchar v[4];
  v[0] = (val >> 0) & 0xff;
  v[1] = (val >> 8) & 0xff;
  v[1] = (val >> 16) & 0xff;
  v[1] = (val >> 24) & 0xff;
  g_checksum_update (checksum, v, 4);
}
