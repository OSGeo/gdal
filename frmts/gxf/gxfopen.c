/******************************************************************************
 * Copyright (c) 1998, Global Geomatics
 * Copyright (c) 1998, Frank Warmerdam
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
 ******************************************************************************
 *
 * gxfopen.c: 
 *
 * Supporting routines for reading Geosoft GXF files.
 *
 * $Log$
 * Revision 1.1  1998/12/02 19:37:04  warmerda
 * New
 *
 */

#include "gxfopen.h"


typedef struct {
    FILE	*fp;

    int		nRawXSize;
    int		nRawYSize;
    int		nSense;		/* GXFS_ codes */
    int		nGType;		/* 0 is uncompressed */

    double	dfXPixelSize;
    double	dfYPixelSize;
    double	dfRotation;

    char	szDummy[64];

    char	*pszTitle;

    double	dfTransformScale;
    double	dfTransformOffset;
    char	*pszTransformName;

    char	*pszUnitName;
    double	dfUnitToMeter;

    double	dfZMaximum;
    double	dfZMinimum;

    int		*panRawLineOffset;
    
} GXFInfo_t;

/************************************************************************/
/*                            CPLReadLine()                             */
/*                                                                      */
/*      Read a line of text from the given file handle, taking care     */
/*      to capture CR and/or LF and strip off ... equivelent of         */
/*      DKReadLine().  Pointer to an internal buffer is returned.       */
/*      The application shouldn't free it, or depend on it's value      */
/*      past the next call to CPLReadLine()                             */
/************************************************************************/

const char *CPLReadLine( FILE * fp )

{
    static char	*pszRLBuffer = NULL;
    static int	nRLBufferSize = 0;

    if( nRLBufferSize < 512 )
    {
        nRLBufferSize = 512;
        pszRLBuffer = (char *) CPLRealloc(pszRLBuffer, nRLBufferSize);
    }

    
}


/************************************************************************/
/*                         GXFReadHeaderValue()                         */
/*                                                                      */
/*      Read one entry from the file header, and return it and it's     */
/*      value in clean form.                                            */
/************************************************************************/

static char **GXFReadHeaderValue( FILE * fp, char * pszHTitle )

{
    char	szLine[256];
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Try to read a line.  If we fail or if this isn't a proper       */
/*      header value then return the failure.                           */
/* -------------------------------------------------------------------- */
    if( fgets( szLine, sizeof(szLine), fp ) == NULL || szLine[0] != '#' )
    {
        strcpy( pszHTitle, "#EOF" );
        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Extract the title.  It should be terminated by some sort of     */
/*      white space.                                                    */
/* -------------------------------------------------------------------- */
    for( i = 0;
         szLine[i] != ' ' && szLine[i] != 10 && szLine[i] != 13
             		  && szLine[i] != 0 && i < 70;
         i++ ) {}

    strncpy( pszHTitle, szLine, i );
    pszHTitle[i] = '\0';

/* -------------------------------------------------------------------- */
/*      Skip white space.                                               */
/* -------------------------------------------------------------------- */
    while( isspace(szLine[i]) )
        i++;

/* -------------------------------------------------------------------- */
/*      If we have reached the end of the line, try to read another line.*/
/* -------------------------------------------------------------------- */
    if( szLine[i] == '\0' )
    {
        if( fgets( szLine, sizeof(szLine), fp ) == NULL )
            szLine[0] = '\0';

        i = 0;
    }

    
        
}


/************************************************************************/
/*                              GXFOpen()                               */
/*                                                                      */
/*      Open a GXF file, and collect contents of the header.            */
/************************************************************************/

GXFHandle GXFOpen( const char * pszFilename )

{
    FILE	*fp;
    GXFInfo_t	*psGXF;

/* -------------------------------------------------------------------- */
/*      We open in binary to ensure that we can efficiently seek()      */
/*      to any location when reading scanlines randomly.  If we         */
/*      opened as text we might still be able to seek(), but I          */
/*      believe that on Windows, the C library has to read through      */
/*      all the data to find the right spot taking into account DOS     */
/*      CRs.                                                            */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "rb" );

    if( fp == NULL )
    {
        /* how to effectively communicate this error out? */
        GBSError( GE_Failure, "Unable to open file: %s\n", pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the GXF Information object.                              */
/* -------------------------------------------------------------------- */
    psGXF = (GXFInfo_t *) VSICalloc( sizeof(GXFInfo_t), 1 );
    psGXF->fp = fp;
    
/* -------------------------------------------------------------------- */
/*      Read the header, one line at a time.                            */
/* -------------------------------------------------------------------- */
    while( fgets( szLine, sizeof(szLine), fp ) )
    {
        if( EQUALN(szLine,"#TITLE",
    }
    
    return( (GXFHandle) psGXF );
}
