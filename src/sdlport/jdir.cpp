/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#ifdef WIN32
# include <Windows.h>
#else
# include <dirent.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

void get_directory(char *path, char **&files, int &tfiles, char **&dirs, int &tdirs)
{
    struct dirent *de;
    files = NULL;
    dirs = NULL;
    tfiles = 0;
    tdirs = 0;
#ifdef WIN32
	// FindFirstFile takes a pattern, not a directory: given "C:\\levels" it
	// matches that one entry, the directory itself, and reports no contents
	// at all. Every caller here passes a directory, so the pattern has to be
	// built. Without it the file selector in the editor lists nothing on
	// Windows, which is how this was found: by reading, since nobody had
	// opened that selector there.
	char pattern[MAX_PATH];
	{
		size_t len = strlen(path);
		bool has_separator = len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/');
		snprintf(pattern, sizeof(pattern), "%s%s*", path,
		         has_separator ? "" : "\\");
	}

	WIN32_FIND_DATA findData;
	HANDLE d = FindFirstFile(pattern, &findData);
	if (d == INVALID_HANDLE_VALUE)
		return;

	do
	{
		// "." and ".." are kept, because readdir hands them to the other
		// side of this function and nothing there drops them either. The
		// file selector shows them, and that is how it goes up a level.
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			tdirs++;
			dirs = (char **)realloc(dirs, sizeof(char *)*tdirs);
			dirs[tdirs - 1] = strdup(findData.cFileName);
		}
		else
		{
			tfiles++;
			files = (char **)realloc(files, sizeof(char *)*tfiles);
			files[tfiles - 1] = strdup(findData.cFileName);
		}
	} while( FindNextFile(d, &findData) );
	FindClose( d );

#else
    DIR *d = opendir( path );

    if( !d )
        return;

    char **tlist = NULL;
    int t = 0;
    char curdir[200];
    getcwd( curdir, 200 );
    chdir( path );

    do
    {
        de = readdir( d );
        if( de )
        {
            t++;
            tlist = (char **)realloc(tlist,sizeof(char *)*t);
            tlist[t-1] = strdup(de->d_name);
        }
    } while( de );
    closedir( d );

    for( int i=0; i < t; i++ )
    {
        d = opendir( tlist[i] );
        if( d )
        {
            tdirs++;
            dirs = (char **)realloc(dirs,sizeof(char *)*tdirs);
            dirs[tdirs-1] = strdup(tlist[i]);
            closedir( d );
        }
        else
        {
            tfiles++;
            files = (char **)realloc(files,sizeof(char *)*tfiles);
            files[tfiles-1] = strdup(tlist[i]);
        }
        free( tlist[i] );
    }
    if( t )
        free( tlist );
    chdir( curdir );
#endif
}
