/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Code to manage the list of currently loaded (cached) PJ_GRIDINFOs
 *           See pj_gridinfo.c for details of loading individual grids.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#define PJ_LIB__

#include <projects.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32_WCE
/* assert.h includes all Windows API headers and causes 'LP' name clash.
 * Here assert we disable assert() for Windows CE.
 * TODO - mloskot: re-implement porting friendly assert
 */
# define assert(exp)	((void)0)
#else
# include <assert.h>
#endif /* _WIN32_WCE */

static PJ_GRIDINFO *grid_list = NULL;
#define PJ_MAX_PATH_LENGTH 1024

/************************************************************************/
/*                        pj_deallocate_grids()                         */
/*                                                                      */
/*      Deallocate all loaded grids.                                    */
/************************************************************************/

void pj_deallocate_grids()

{
    while( grid_list != NULL )
    {
        PJ_GRIDINFO *item = grid_list;
        grid_list = grid_list->next;
        item->next = NULL;

        pj_gridinfo_free( pj_get_default_ctx(), item );
    }
}

/************************************************************************/
/*                       pj_gridlist_merge_grid()                       */
/*                                                                      */
/*      Find/load the named gridfile and merge it into the              */
/*      last_nadgrids_list.                                             */
/************************************************************************/

static int pj_gridlist_merge_gridfile( projCtx ctx, 
                                       const char *gridname,
                                       PJ_GRIDINFO ***p_gridlist,
                                       int *p_gridcount, 
                                       int *p_gridmax )

{
    int got_match=0;
    PJ_GRIDINFO *this_grid, *tail = NULL;

/* -------------------------------------------------------------------- */
/*      Try to find in the existing list of loaded grids.  Add all      */
/*      matching grids as with NTv2 we can get many grids from one      */
/*      file (one shared gridname).                                     */
/* -------------------------------------------------------------------- */
    for( this_grid = grid_list; this_grid != NULL; this_grid = this_grid->next)
    {
        if( strcmp(this_grid->gridname,gridname) == 0 )
        {
            got_match = 1;

            /* dont add to the list if it is invalid. */
            if( this_grid->ct == NULL )
                return 0;

            /* do we need to grow the list? */
            if( *p_gridcount >= *p_gridmax - 2 )
            {
                PJ_GRIDINFO **new_list;
                int new_max = *p_gridmax + 20;

                new_list = (PJ_GRIDINFO **) pj_malloc(sizeof(void*) * new_max);
                if( *p_gridlist != NULL )
                {
                    memcpy( new_list, *p_gridlist,
                            sizeof(void*) * (*p_gridmax) );
                    pj_dalloc( *p_gridlist );
                }

                *p_gridlist = new_list;
                *p_gridmax = new_max;
            }

            /* add to the list */
            (*p_gridlist)[(*p_gridcount)++] = this_grid;
            (*p_gridlist)[*p_gridcount] = NULL;
        }

        tail = this_grid;
    }

    if( got_match )
        return 1;

/* -------------------------------------------------------------------- */
/*      Try to load the named grid.                                     */
/* -------------------------------------------------------------------- */
    this_grid = pj_gridinfo_init( ctx, gridname );

    if( this_grid == NULL )
    {
        /* we should get at least a stub grid with a missing "ct" member */
        assert( FALSE );
        return 0;
    }
    
    if( tail != NULL )
        tail->next = this_grid;
    else
        grid_list = this_grid;

/* -------------------------------------------------------------------- */
/*      Recurse to add the grid now that it is loaded.                  */
/* -------------------------------------------------------------------- */
    return pj_gridlist_merge_gridfile( ctx, gridname, p_gridlist, 
                                       p_gridcount, p_gridmax );
}

/************************************************************************/
/*                     pj_gridlist_from_nadgrids()                      */
/*                                                                      */
/*      This functions loads the list of grids corresponding to a       */
/*      particular nadgrids string into a list, and returns it.  The    */
/*      list is kept around till a request is made with a different     */
/*      string in order to cut down on the string parsing cost, and     */
/*      the cost of building the list of tables each time.              */
/************************************************************************/

PJ_GRIDINFO **pj_gridlist_from_nadgrids( projCtx ctx, const char *nadgrids, 
                                         int *grid_count)

{
    const char *s;
    PJ_GRIDINFO **gridlist = NULL;
    int grid_max = 0;

    pj_errno = 0;
    *grid_count = 0;

    pj_acquire_lock();

/* -------------------------------------------------------------------- */
/*      Loop processing names out of nadgrids one at a time.            */
/* -------------------------------------------------------------------- */
    for( s = nadgrids; *s != '\0'; )
    {
        size_t end_char;
        int    required = 1;
        char   name[PJ_MAX_PATH_LENGTH];

        if( *s == '@' )
        {
            required = 0;
            s++;
        }

        for( end_char = 0; 
             s[end_char] != '\0' && s[end_char] != ','; 
             end_char++ ) {}

        if( end_char >= sizeof(name) )
        {
            pj_ctx_set_errno( ctx, -38 );
            pj_release_lock();
            return NULL;
        }
        
        strncpy( name, s, end_char );
        name[end_char] = '\0';

        s += end_char;
        if( *s == ',' )
            s++;

        if( !pj_gridlist_merge_gridfile( ctx, name, &gridlist, grid_count, 
                                         &grid_max) 
            && required )
        {
            pj_ctx_set_errno( ctx, -38 );
            pj_release_lock();
            return NULL;
        }
        else
            pj_errno = 0;
    }

    pj_release_lock();

    return gridlist;
}
