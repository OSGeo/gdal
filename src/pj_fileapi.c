/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Implementation of the pj_ctx_* file api, and the default stdio
 *           based implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam
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

static PAFile pj_stdio_fopen(projCtx ctx, const char *filename, 
                             const char *access);
static size_t pj_stdio_fread(void *buffer, size_t size, size_t nmemb, 
                             PAFile file);
static int pj_stdio_fseek(PAFile file, long offset, int whence);
static long pj_stdio_ftell(PAFile file);
static void pj_stdio_fclose(PAFile file);

static projFileAPI default_fileapi = { 
    pj_stdio_fopen, 
    pj_stdio_fread,
    pj_stdio_fseek,
    pj_stdio_ftell,
    pj_stdio_fclose
};

typedef struct {
    projCtx ctx;
    FILE *fp;
} stdio_pafile;

/************************************************************************/
/*                       pj_get_default_fileapi()                       */
/************************************************************************/

projFileAPI *pj_get_default_fileapi() 
{
    return &default_fileapi;
}

/************************************************************************/
/*                           pj_stdio_fopen()                           */
/************************************************************************/

static PAFile pj_stdio_fopen(projCtx ctx, const char *filename, 
                             const char *access)
{
    stdio_pafile *pafile;
    FILE *fp;

    fp = fopen(filename, access);
    if (fp == NULL) 
    {
        return NULL;
    }

    pafile = (stdio_pafile *) malloc(sizeof(stdio_pafile));
    pafile->fp = fp;
    pafile->ctx = ctx;
    return (PAFile) pafile;
}

/************************************************************************/
/*                           pj_stdio_fread()                           */
/************************************************************************/

static size_t pj_stdio_fread(void *buffer, size_t size, size_t nmemb, 
                             PAFile file)
{
    stdio_pafile *pafile = (stdio_pafile *) file;
    return fread(buffer, size, nmemb, pafile->fp);
}

/************************************************************************/
/*                           pj_stdio_fseek()                           */
/************************************************************************/
static int pj_stdio_fseek(PAFile file, long offset, int whence)
{
    stdio_pafile *pafile = (stdio_pafile *) file;
    return fseek(pafile->fp, offset, whence);
}

/************************************************************************/
/*                           pj_stdio_ftell()                           */
/************************************************************************/
static long pj_stdio_ftell(PAFile file)
{
    stdio_pafile *pafile = (stdio_pafile *) file;
    return ftell(pafile->fp);
}

/************************************************************************/
/*                          pj_stdio_fclose()                           */
/************************************************************************/
static void pj_stdio_fclose(PAFile file)
{
    stdio_pafile *pafile = (stdio_pafile *) file;
    fclose(pafile->fp);
    free(pafile);
}

/************************************************************************/
/*                            pj_ctx_fopen()                            */
/*                                                                      */
/*      Open a file using the provided file io hooks.                   */
/************************************************************************/

PAFile pj_ctx_fopen(projCtx ctx, const char *filename, const char *access)
{
    return ctx->fileapi->FOpen(ctx, filename, access);
}

/************************************************************************/
/*                            pj_ctx_fread()                            */
/************************************************************************/
size_t pj_ctx_fread(projCtx ctx, void *buffer, size_t size, size_t nmemb, PAFile file)
{
    return ctx->fileapi->FRead(buffer, size, nmemb, file);
}

/************************************************************************/
/*                            pj_ctx_fseek()                            */
/************************************************************************/
int    pj_ctx_fseek(projCtx ctx, PAFile file, long offset, int whence)
{
    return ctx->fileapi->FSeek(file, offset, whence);
}

/************************************************************************/
/*                            pj_ctx_ftell()                            */
/************************************************************************/
long   pj_ctx_ftell(projCtx ctx, PAFile file)
{
    return ctx->fileapi->FTell(file);
}

/************************************************************************/
/*                           pj_ctx_fclose()                            */
/************************************************************************/
void   pj_ctx_fclose(projCtx ctx, PAFile file)
{
    ctx->fileapi->FClose(file);
}

/************************************************************************/
/*                            pj_ctx_fgets()                            */
/*                                                                      */
/*      A not very optimal implementation of fgets on top of            */
/*      fread().  If we end up using this a lot more care should be     */
/*      taken.                                                          */
/************************************************************************/

char *pj_ctx_fgets(projCtx ctx, char *line, int size, PAFile file) 
{
    long start = pj_ctx_ftell(ctx, file);
    size_t bytes_read;
    int i;

    line[size-1] = '\0';
    bytes_read = pj_ctx_fread(ctx, line, 1, size-1, file);
    if(bytes_read == 0)
        return NULL;
    if(bytes_read < (size_t)size)
    {
        line[bytes_read] = '\0';
    }
    
    for( i = 0; i < size-2; i++) 
    {
        if (line[i] == '\n') 
        {
            line[i+1] = '\0';
            pj_ctx_fseek(ctx, file, start + i + 1, SEEK_SET);
            break;
        }
    }
    return line;
}
