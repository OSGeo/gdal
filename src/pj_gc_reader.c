/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Code to read a grid catalog from a .cvs file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
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
#include <ctype.h>

static int pj_gc_readentry(projCtx ctx, PAFile fid, PJ_GridCatalogEntry *entry);

/************************************************************************/
/*                         pj_gc_readcatalog()                          */
/*                                                                      */
/*      Read a grid catalog from a .csv file.                           */
/************************************************************************/

PJ_GridCatalog *pj_gc_readcatalog( projCtx ctx, const char *catalog_name )
{
    PAFile fid;
    PJ_GridCatalog *catalog;
    int entry_max;
    char line[302];
    
    fid = pj_open_lib( ctx, (char *) catalog_name, "r" );
    if (fid == NULL) 
        return NULL;

    /* discard title line */
    pj_ctx_fgets(ctx, line, sizeof(line)-1, fid);

    catalog = (PJ_GridCatalog *) calloc(1,sizeof(PJ_GridCatalog));
    if( !catalog )
        return NULL;
    
    catalog->catalog_name = strdup(catalog_name);
    
    entry_max = 10;
    catalog->entries = (PJ_GridCatalogEntry *) 
        malloc(entry_max * sizeof(PJ_GridCatalogEntry));
    
    while( pj_gc_readentry( ctx, fid, 
                            catalog->entries+catalog->entry_count) == 0)
    {
        catalog->entry_count++;
        
        if( catalog->entry_count == entry_max ) 
        {
            entry_max = entry_max * 2;
            catalog->entries = (PJ_GridCatalogEntry *) 
                realloc(catalog->entries, 
                        entry_max * sizeof(PJ_GridCatalogEntry));
            if (catalog->entries == NULL )
                return NULL;
        }
    }

    return catalog;
}

/************************************************************************/
/*                        pj_gc_read_csv_line()                         */
/*                                                                      */
/*      Simple csv line splitter with fixed maximum line size and       */
/*      token count.                                                    */
/************************************************************************/

static int pj_gc_read_csv_line( projCtx ctx, PAFile fid, 
                                char **tokens, int max_tokens ) 
{
    char line[302];
   
    while( pj_ctx_fgets(ctx, line, sizeof(line)-1, fid) != NULL )
    {
        char *next = line;
        int token_count = 0;
        
        while( isspace(*next) ) 
            next++;
        
        /* skip blank and comment lines */
        if( next[0] == '#' || next[0] == '\0' )
            continue;
        
        while( token_count < max_tokens && *next != '\0' ) 
        {
            const char *start = next;
            
            while( *next != '\0' && *next != ',' ) 
                next++;
            
            if( *next == ',' )
            {
                *next = '\0';
                next++;
            }
            
            tokens[token_count++] = strdup(start);
        }

        return token_count;
    }
    
    return 0; 
}

/************************************************************************/
/*                          pj_gc_parsedate()                           */
/*                                                                      */
/*      Parse a date into a floating point year value.  Acceptable      */
/*      values are "yyyy.fraction" and "yyyy-mm-dd".  Anything else     */
/*      returns 0.0.                                                    */
/************************************************************************/

double pj_gc_parsedate( projCtx ctx, const char *date_string )
{
    (void) ctx;

    if( strlen(date_string) == 10 
        && date_string[4] == '-' && date_string[7] == '-' ) 
    {
        int year = atoi(date_string);
        int month = atoi(date_string+5);
        int day = atoi(date_string+8);

        /* simplified calculation so we don't need to know all about months */
        return year + ((month-1) * 31 + (day-1)) / 372.0;
    }
    else 
    {
        return pj_atof(date_string);
    }
}


/************************************************************************/
/*                          pj_gc_readentry()                           */
/*                                                                      */
/*      Read one catalog entry from the file                            */
/*                                                                      */
/*      Format:                                                         */
/*        gridname,ll_long,ll_lat,ur_long,ur_lat,priority,date          */
/************************************************************************/

static int pj_gc_readentry(projCtx ctx, PAFile fid, PJ_GridCatalogEntry *entry) 
{
#define MAX_TOKENS 30
    char *tokens[MAX_TOKENS];
    int token_count, i;
    int error = 0;

    memset( entry, 0, sizeof(PJ_GridCatalogEntry) );
    
    token_count = pj_gc_read_csv_line( ctx, fid, tokens, MAX_TOKENS );
    if( token_count < 5 )
    {
        error = 1; /* TODO: need real error codes */
        if( token_count != 0 )
            pj_log( ctx, PJ_LOG_ERROR, "Short line in grid catalog." );
    }
    else
    {
        memset( entry, 0, sizeof(PJ_GridCatalogEntry));
        
        entry->definition = strdup( tokens[0] );
        entry->region.ll_long = dmstor_ctx( ctx, tokens[1], NULL );
        entry->region.ll_lat = dmstor_ctx( ctx, tokens[2], NULL );
        entry->region.ur_long = dmstor_ctx( ctx, tokens[3], NULL );
        entry->region.ur_lat = dmstor_ctx( ctx, tokens[4], NULL );
        if( token_count > 5 )
            entry->priority = atoi( tokens[5] ); /* defaults to zero */
        if( token_count > 6 )
            entry->date = pj_gc_parsedate( ctx, tokens[6] );
    }

    for( i = 0; i < token_count; i++ )
        free( tokens[i] );

    return error;
}



