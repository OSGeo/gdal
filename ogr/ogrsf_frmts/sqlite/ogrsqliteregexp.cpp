/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite REGEXP function
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
 ****************************************************************************/

/* WARNING: VERY IMPORTANT NOTE: This file MUST not be directly compiled as */
/* a standalone object. It must be included from ogrsqlitevirtualogr.cpp */
/* (actually from ogrsqlitesqlfunctions.cpp) */
#ifndef COMPILATION_ALLOWED
#error See comment in file
#endif

/* This code originates from pcre.c from the sqlite3-pcre extension */
/* from http://laltromondo.dynalias.net/~iki/informatica/soft/sqlite3-pcre/ */
/* whose header is : */
/*
 * Written by Alexey Tourbin <at@altlinux.org>.
 *
 * The author has dedicated the code to the public domain.  Anyone is free
 * to copy, modify, publish, use, compile, sell, or distribute the original
 * code, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 */


#include "ogrsqliteregexp.h"

#ifdef HAVE_PCRE

#include <pcre.h>

typedef struct {
    char *s;
    pcre *p;
    pcre_extra *e;
} cache_entry;

#ifndef CACHE_SIZE
#define CACHE_SIZE 16
#endif

/************************************************************************/
/*                         OGRSQLiteREGEXPFunction()                    */
/************************************************************************/

static
void OGRSQLiteREGEXPFunction(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
    const char *re, *str;
    pcre *p;
    pcre_extra *e;

    CPLAssert(argc == 2);

    re = (const char *) sqlite3_value_text(argv[0]);
    if (!re) {
        sqlite3_result_error(ctx, "no regexp", -1);
        return;
    }

    if( sqlite3_value_type(argv[1]) == SQLITE_NULL )
    {
        sqlite3_result_int(ctx, 0);
        return;
    }

    str = (const char *) sqlite3_value_text(argv[1]);
    if (!str) {
        sqlite3_result_error(ctx, "no string", -1);
        return;
    }

    /* simple LRU cache */
    int i;
    int found = 0;
    cache_entry *cache = (cache_entry*) sqlite3_user_data(ctx);

    CPLAssert(cache);

    for (i = 0; i < CACHE_SIZE && cache[i].s; i++)
    {
        if (strcmp(re, cache[i].s) == 0) {
            found = 1;
            break;
        }
    }

    if (found)
    {
        if (i > 0)
        {
            cache_entry c = cache[i];
            memmove(cache + 1, cache, i * sizeof(cache_entry));
            cache[0] = c;
        }
    }
    else
    {
        cache_entry c;
        const char *err;
        int pos;
        c.p = pcre_compile(re, 0, &err, &pos, NULL);
        if (!c.p)
        {
            char *e2 = sqlite3_mprintf("%s: %s (offset %d)", re, err, pos);
            sqlite3_result_error(ctx, e2, -1);
            sqlite3_free(e2);
            return;
        }
        c.e = pcre_study(c.p, 0, &err);
        c.s = VSIStrdup(re);
        if (!c.s)
        {
            sqlite3_result_error(ctx, "strdup: ENOMEM", -1);
            pcre_free(c.p);
            pcre_free(c.e);
            return;
        }
        i = CACHE_SIZE - 1;
        if (cache[i].s)
        {
            CPLFree(cache[i].s);
            CPLAssert(cache[i].p);
            pcre_free(cache[i].p);
            pcre_free(cache[i].e);
        }
        memmove(cache + 1, cache, i * sizeof(cache_entry));
        cache[0] = c;
    }
    p = cache[0].p;
    e = cache[0].e;

    int rc;
    CPLAssert(p);
    rc = pcre_exec(p, e, str, strlen(str), 0, 0, NULL, 0);
    sqlite3_result_int(ctx, rc >= 0);
}

#endif // HAVE_PCRE

/************************************************************************/
/*                        OGRSQLiteRegisterRegExpFunction()             */
/************************************************************************/

void* OGRSQLiteRegisterRegExpFunction(sqlite3* hDB)
{
#ifdef HAVE_PCRE

    /* For debugging purposes mostly */
    if( !CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_REGEXP", "YES")) )
        return NULL;

    /* Check if we really need to define our own REGEXP function */
    int rc = sqlite3_exec(hDB, "SELECT 'a' REGEXP 'a'", NULL, NULL, NULL);
    if( rc == SQLITE_OK )
    {
        CPLDebug("SQLITE", "REGEXP already available");
        return NULL;
    }

    cache_entry *cache = (cache_entry*) CPLCalloc(CACHE_SIZE, sizeof(cache_entry));
    sqlite3_create_function(hDB, "REGEXP", 2, SQLITE_UTF8, cache,
                            OGRSQLiteREGEXPFunction, NULL, NULL);

    /* To clear the error flag */
    sqlite3_exec(hDB, "SELECT 1", NULL, NULL, NULL);

    return cache;
#else // HAVE_PCRE
    return NULL;
#endif // HAVE_PCRE
}

/************************************************************************/
/*                         OGRSQLiteFreeRegExpCache()                   */
/************************************************************************/

void OGRSQLiteFreeRegExpCache(void* hRegExpCache)
{
#ifdef HAVE_PCRE
    if( hRegExpCache == NULL )
        return;

    cache_entry *cache = (cache_entry*) hRegExpCache;
    int i;
    for (i = 0; i < CACHE_SIZE && cache[i].s; i++)
    {
        CPLFree(cache[i].s);
        CPLAssert(cache[i].p);
        pcre_free(cache[i].p);
        pcre_free(cache[i].e);
    }
    CPLFree(cache);
#endif // HAVE_PCRE
}
