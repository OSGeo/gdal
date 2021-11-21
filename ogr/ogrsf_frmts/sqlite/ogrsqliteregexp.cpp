/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite REGEXP function
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

// The pcre2 variant has been ported from https://github.com/pfmoore/sqlite-pcre2/blob/main/src/pcre.c
// which has the same license as above.

#include "ogrsqliteregexp.h"
#include "sqlite3.h"

#ifdef HAVE_PCRE2

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

typedef struct {
    char *s;
    pcre2_code *p;
} cache_entry;

constexpr int CACHE_SIZE = 16;

static
pcre2_code *re_compile_with_cache(sqlite3_context *ctx, const char *re)
{
    cache_entry *cache = static_cast<cache_entry*>(sqlite3_user_data(ctx));

    CPLAssert(cache);

    bool found = false;
    int i;
    for (i = 0; i < CACHE_SIZE && cache[i].s; i++)
        if (strcmp(re, cache[i].s) == 0) {
            found = true;
            break;
        }

    if (found) {
        if (i > 0) {
            /* Get the found entry */
            cache_entry c = cache[i];
            /* Move 0..i-1 up one - args are (dest, src, size) */
            memmove(cache + 1, cache, i * sizeof(cache_entry));
            /* Put the found entry at the start */
            cache[0] = c;
        }
    }
    else {
        /* Create a new entry */
        int errorcode = 0;
        PCRE2_SIZE pos = 0;
        uint32_t has_jit = 0;
        PCRE2_UCHAR8 err_buff[256];

        pcre2_code* pat = pcre2_compile(reinterpret_cast<const PCRE2_UCHAR8*>(re),
                            PCRE2_ZERO_TERMINATED, 0, &errorcode, &pos, nullptr);
        if (!pat) {
            pcre2_get_error_message(errorcode, err_buff, sizeof(err_buff));
            char *e2 = sqlite3_mprintf("%s: %s (offset %d)", re, err_buff, pos);
            sqlite3_result_error(ctx, e2, -1);
            sqlite3_free(e2);
            return nullptr;
        }
        pcre2_config(PCRE2_CONFIG_JIT, &has_jit);
        if (has_jit) {
            errorcode = pcre2_jit_compile(pat, 0);
            if (errorcode) {
                pcre2_get_error_message(errorcode, err_buff, sizeof(err_buff));
                char *e2 = sqlite3_mprintf("%s: %s", re, err_buff);
                sqlite3_result_error(ctx, e2, -1);
                sqlite3_free(e2);
                pcre2_code_free(pat);
                return nullptr;
            }
        }
        /* Free the last cache entry if necessary */
        i = CACHE_SIZE - 1;
        if (cache[i].s) {
            VSIFree(cache[i].s);
            CPLAssert(cache[i].p);
            pcre2_code_free(cache[i].p);
        }
        /* Move everything up to make space */
        memmove(cache + 1, cache, i * sizeof(cache_entry));
        cache[0].s = VSIStrdup(re);
        cache[0].p = pat;
    }

    return cache[0].p;
}

/************************************************************************/
/*                         OGRSQLiteREGEXPFunction()                    */
/************************************************************************/

static
void OGRSQLiteREGEXPFunction( sqlite3_context *ctx,
                              CPL_UNUSED int argc, sqlite3_value **argv )
{
    CPLAssert(argc == 2);

    const char *re = (const char *) sqlite3_value_text(argv[0]);
    if (!re) {
        sqlite3_result_error(ctx, "no regexp", -1);
        return;
    }

    if( sqlite3_value_type(argv[1]) == SQLITE_NULL )
    {
        sqlite3_result_int(ctx, 0);
        return;
    }

    const char *str = (const char *) sqlite3_value_text(argv[1]);
    if (!str) {
        sqlite3_result_error(ctx, "no string", -1);
        return;
    }

    pcre2_code* p = re_compile_with_cache(ctx, re);
    if (!p)
        return;

    pcre2_match_data *md = pcre2_match_data_create_from_pattern(p, nullptr);
    if (!md) {
        sqlite3_result_error(ctx, "could not create match data block", -1);
        return;
    }
    int rc = pcre2_match(p, reinterpret_cast<const PCRE2_UCHAR8*>(str),
                         PCRE2_ZERO_TERMINATED, 0, 0, md, nullptr);
    sqlite3_result_int(ctx, rc >= 0);
}


#elif defined(HAVE_PCRE)

#include <pcre.h>

typedef struct {
    char *s;
    pcre *p;
    pcre_extra *e;
} cache_entry;

constexpr int CACHE_SIZE = 16;

/************************************************************************/
/*                         OGRSQLiteREGEXPFunction()                    */
/************************************************************************/

static
void OGRSQLiteREGEXPFunction( sqlite3_context *ctx,
                              CPL_UNUSED int argc, sqlite3_value **argv )
{
    CPLAssert(argc == 2);

    const char *re = (const char *) sqlite3_value_text(argv[0]);
    if (!re) {
        sqlite3_result_error(ctx, "no regexp", -1);
        return;
    }

    if( sqlite3_value_type(argv[1]) == SQLITE_NULL )
    {
        sqlite3_result_int(ctx, 0);
        return;
    }

    const char *str = (const char *) sqlite3_value_text(argv[1]);
    if (!str) {
        sqlite3_result_error(ctx, "no string", -1);
        return;
    }

    /* simple LRU cache */
    cache_entry *cache = (cache_entry*) sqlite3_user_data(ctx);
    CPLAssert(cache);

    bool found = false;
    int i = 0;  // Used after for.
    for( ; i < CACHE_SIZE && cache[i].s; i++ )
    {
        if (strcmp(re, cache[i].s) == 0) {
            found = true;
            break;
        }
    }

    if( found )
    {
        if( i > 0 )
        {
            cache_entry c = cache[i];
            memmove(cache + 1, cache, i * sizeof(cache_entry));
            cache[0] = c;
        }
    }
    else
    {
        cache_entry c;
        const char *err = nullptr;
        int pos = 0;
        c.p = pcre_compile(re, 0, &err, &pos, nullptr);
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
    pcre *p = cache[0].p;
    CPLAssert(p);
    pcre_extra *e = cache[0].e;

    int rc = pcre_exec(p, e, str, static_cast<int>(strlen(str)), 0, 0, nullptr, 0);
    sqlite3_result_int(ctx, rc >= 0);
}

#endif // HAVE_PCRE

/************************************************************************/
/*                        OGRSQLiteRegisterRegExpFunction()             */
/************************************************************************/

static
void* OGRSQLiteRegisterRegExpFunction(sqlite3*
#if defined(HAVE_PCRE) || defined(HAVE_PCRE2)
                                       hDB
#endif
                                      )
{
#if defined(HAVE_PCRE) || defined(HAVE_PCRE2)

    /* For debugging purposes mostly */
    if( !CPLTestBool(CPLGetConfigOption("OGR_SQLITE_REGEXP", "YES")) )
        return nullptr;

    /* Check if we really need to define our own REGEXP function */
    int rc = sqlite3_exec(hDB, "SELECT 'a' REGEXP 'a'", nullptr, nullptr, nullptr);
    if( rc == SQLITE_OK )
    {
        CPLDebug("SQLITE", "REGEXP already available");
        return nullptr;
    }

    cache_entry *cache = (cache_entry*) CPLCalloc(CACHE_SIZE, sizeof(cache_entry));
    sqlite3_create_function(hDB, "REGEXP", 2, SQLITE_UTF8, cache,
                            OGRSQLiteREGEXPFunction, nullptr, nullptr);

    /* To clear the error flag */
    sqlite3_exec(hDB, "SELECT 1", nullptr, nullptr, nullptr);

    return cache;
#else // HAVE_PCRE
    return nullptr;
#endif // HAVE_PCRE
}

/************************************************************************/
/*                         OGRSQLiteFreeRegExpCache()                   */
/************************************************************************/

static
void OGRSQLiteFreeRegExpCache(void*
#if defined(HAVE_PCRE) || defined(HAVE_PCRE2)
                              hRegExpCache
#endif
                              )
{
#ifdef HAVE_PCRE2
    if( hRegExpCache == nullptr )
        return;

    cache_entry *cache = (cache_entry*) hRegExpCache;
    for( int i = 0; i < CACHE_SIZE && cache[i].s; i++ )
    {
        CPLFree(cache[i].s);
        CPLAssert(cache[i].p);
        pcre2_code_free(cache[i].p);
    }
    CPLFree(cache);
#elif defined(HAVE_PCRE)
    if( hRegExpCache == nullptr )
        return;

    cache_entry *cache = (cache_entry*) hRegExpCache;
    for( int i = 0; i < CACHE_SIZE && cache[i].s; i++ )
    {
        CPLFree(cache[i].s);
        CPLAssert(cache[i].p);
        pcre_free(cache[i].p);
        pcre_free(cache[i].e);
    }
    CPLFree(cache);
#endif // HAVE_PCRE
}
