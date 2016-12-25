/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  init file definition cache.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam
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

#include <projects.h>
#include <string.h>

static int cache_count = 0;
static int cache_alloc = 0;
static char **cache_key = NULL;
static paralist **cache_paralist = NULL;

/************************************************************************/
/*                            pj_clone_paralist()                       */
/*                                                                      */
/*     Allocate a copy of a parameter list.                             */
/************************************************************************/

paralist *pj_clone_paralist( const paralist *list)
{
  paralist *list_copy = NULL, *next_copy = NULL;

  for( ; list != NULL; list = list->next )
    {
      paralist *newitem = (paralist *)
	pj_malloc(sizeof(paralist) + strlen(list->param));

      newitem->used = 0;
      newitem->next = 0;
      strcpy( newitem->param, list->param );
      
      if( list_copy == NULL )
	list_copy = newitem;
      else
	next_copy->next = newitem;

      next_copy = newitem;
    }

  return list_copy;
}

/************************************************************************/
/*                            pj_clear_initcache()                      */
/*                                                                      */
/*      Clear out all memory held in the init file cache.               */
/************************************************************************/

void pj_clear_initcache()
{
    if( cache_alloc > 0 )
    {
        int i;

        pj_acquire_lock();

        for( i = 0; i < cache_count; i++ )
        {
            paralist *n, *t = cache_paralist[i];
		
            pj_dalloc( cache_key[i] );

            /* free parameter list elements */
            for (; t != NULL; t = n) {
                n = t->next;
                pj_dalloc(t);
            }
        }

        pj_dalloc( cache_key );
        pj_dalloc( cache_paralist );
        cache_count = 0;
        cache_alloc= 0;
        cache_key = NULL;
        cache_paralist = NULL;

        pj_release_lock();
    }
}

/************************************************************************/
/*                            pj_search_initcache()                     */
/*                                                                      */
/*      Search for a matching definition in the init cache.             */
/************************************************************************/

paralist *pj_search_initcache( const char *filekey )

{
    int i;
    paralist *result = NULL;

    pj_acquire_lock();

    for( i = 0; result == NULL && i < cache_count; i++)
    {
        if( strcmp(filekey,cache_key[i]) == 0 )
	{
            result = pj_clone_paralist( cache_paralist[i] );
	}
    }

    pj_release_lock();

    return result;
}

/************************************************************************/
/*                            pj_insert_initcache()                     */
/*                                                                      */
/*      Insert a paralist definition in the init file cache.            */
/************************************************************************/

void pj_insert_initcache( const char *filekey, const paralist *list )

{
    pj_acquire_lock();

    /* 
    ** Grow list if required.
    */
    if( cache_count == cache_alloc )
    {
        char **cache_key_new;
        paralist **cache_paralist_new;

        cache_alloc = cache_alloc * 2 + 15;

        cache_key_new = (char **) pj_malloc(sizeof(char*) * cache_alloc);
        memcpy( cache_key_new, cache_key, sizeof(char*) * cache_count);
        pj_dalloc( cache_key );
        cache_key = cache_key_new;

        cache_paralist_new = (paralist **) 
            pj_malloc(sizeof(paralist*) * cache_alloc);
        memcpy( cache_paralist_new, cache_paralist, 
                sizeof(paralist*) * cache_count );
        pj_dalloc( cache_paralist );
        cache_paralist = cache_paralist_new;
    }

    /*
    ** Duplicate the filekey and paralist, and insert in cache.
    */
    cache_key[cache_count] = (char *) pj_malloc(strlen(filekey)+1);
    strcpy( cache_key[cache_count], filekey );

    cache_paralist[cache_count] = pj_clone_paralist( list );

    cache_count++;

    pj_release_lock();
}

