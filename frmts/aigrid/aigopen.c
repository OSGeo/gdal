/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file access cover API for non-GDAL use.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * $Log$
 * Revision 1.2  1999/02/04 22:15:33  warmerda
 * fleshed out implementation
 *
 * Revision 1.1  1999/02/03 14:12:56  warmerda
 * New
 *
 */

#include "aigrid.h"

/************************************************************************/
/*                              AIGOpen()                               */
/************************************************************************/

AIGInfo_t *AIGOpen( const char * pszCoverName, const char * pszAccess )

{
    AIGInfo_t	*psInfo;
    char	*pszHDRFilename;
    
/* -------------------------------------------------------------------- */
/*      Verify that the target is a directory, and has appropriate      */
/*      subfiles.                                                       */
/* -------------------------------------------------------------------- */
    /* notdef */

/* -------------------------------------------------------------------- */
/*      Allocate info structure.                                        */
/* -------------------------------------------------------------------- */
    psInfo = (AIGInfo_t *) CPLCalloc(sizeof(AIGInfo_t),1);

/* -------------------------------------------------------------------- */
/*      Read the header file.                                           */
/* -------------------------------------------------------------------- */
    if( AIGReadHeader( pszCoverName, psInfo ) != CE_None )
    {
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file w001001.adf file itself.                          */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/w001001.adf", pszCoverName );

    psInfo->fpGrid = VSIFOpen( pszHDRFilename, "rb" );
    
    if( psInfo->fpGrid == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open grid file:\n%s\n",
                  pszHDRFilename );

        CPLFree( psInfo );
        CPLFree( pszHDRFilename );
        return( NULL );
    }

    CPLFree( pszHDRFilename );

/* -------------------------------------------------------------------- */
/*      Read the block index file.                                      */
/* -------------------------------------------------------------------- */
    if( AIGReadBlockIndex( pszCoverName, psInfo ) != CE_None )
    {
        VSIFClose( psInfo->fpGrid );
        
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the extents.                                               */
/* -------------------------------------------------------------------- */
    if( AIGReadBounds( pszCoverName, psInfo ) != CE_None )
    {
        VSIFClose( psInfo->fpGrid );
        
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Compute the number of pixels and lines.                         */
/* -------------------------------------------------------------------- */
    psInfo->nPixels = (int)
        (psInfo->dfURX - psInfo->dfLLX) / psInfo->dfCellSizeX;
    psInfo->nLines = (int)
        (psInfo->dfURY - psInfo->dfLLY) / psInfo->dfCellSizeY;
    
    return( psInfo );
}

/************************************************************************/
/*                              AIGClose()                              */
/************************************************************************/

void AIGClose( AIGInfo_t * psInfo )

{
    VSIFClose( psInfo->fpGrid );

    CPLFree( psInfo->panBlockOffset );
    CPLFree( psInfo->panBlockSize );
    CPLFree( psInfo );
}
