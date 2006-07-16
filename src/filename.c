/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"


gboolean
filename_parse(gchar *in, gint i, RS_PHOTO *photo, gchar *parsed)
{
	/*
	 * %f = filename
	 * %c = incremental counter
	 * %d = date (will have to wait until read from exif)
	 * %t = time (will have to wait until read from exif)
	 */

	gchar temp[1024];
	gchar tempc[32];
	gint n=0, m=0;
	gint c=0;
	gboolean force = FALSE;

	while (in[n])
	{
  		switch(in[n])
		{
			case '%':
				switch (in[n+1])
				{
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						c = (gint) in[n+1];
						switch (in[n+2])
						{
							case 'c':
								if (c == 49)
									sprintf(tempc, "%01d",i);
								else if (c == 50)
									sprintf(tempc, "%02d",i);
								else if (c == 51)
									sprintf(tempc, "%03d",i);
								else if (c == 52)
									sprintf(tempc, "%04d",i);
								else if (c == 53)
									sprintf(tempc, "%05d",i);
								else if (c == 54)
									sprintf(tempc, "%06d",i);
								else if (c == 55)
									sprintf(tempc, "%07d",i);
								else if (c == 56)
									sprintf(tempc, "%08d",i);
								else if (c == 57)
									sprintf(tempc, "%09d",i);
								n += 3;
								strcpy(&temp[m], tempc);
								m += strlen(tempc);
								break;
							default:
								temp[m++] = in[n];
								temp[m++] = in[n+1];
								temp[m++] = in[n+2];
								n += 3;
								break;
						}
						break;
					case 'f':
						strcpy(&temp[m], g_basename(photo->filename));
						m += strlen(g_basename(photo->filename));
						n += 2;
						break;
					case 'c':
						g_sprintf(temp, "%s%d", temp, i);
					    n += 2;
						break;
					default:
						temp[m++] = in[n];
						temp[m++] = in[n+1];
						n += 2;
					break;
				}
				break;
			default:
				temp[m++] = in[n++];
				break;
			}
	}
	temp[m] = (gint) NULL;
	if (parsed)
		g_free(parsed);
	parsed = g_malloc(sizeof(temp));
	strcpy(parsed, temp);
	return force;
}
