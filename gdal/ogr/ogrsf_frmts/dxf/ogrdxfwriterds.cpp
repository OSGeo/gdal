/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFWriterDS - the OGRDataSource class used for
 *           writing a DXF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRDXFWriterDS()                          */
/************************************************************************/

OGRDXFWriterDS::OGRDXFWriterDS()

{
    fp = NULL;
    poLayer = NULL;
}

/************************************************************************/
/*                         ~OGRDXFWriterDS()                          */
/************************************************************************/

OGRDXFWriterDS::~OGRDXFWriterDS()

{
/* -------------------------------------------------------------------- */
/*      Destroy layers.                                                 */
/* -------------------------------------------------------------------- */
    delete poLayer;

/* -------------------------------------------------------------------- */
/*      Write trailer.                                                  */
/* -------------------------------------------------------------------- */
    if( osTrailerFile != "" )
    {
        FILE *fpSrc = VSIFOpenL( osTrailerFile, "r" );
        
        if( fpSrc == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open template trailer file '%s' for reading.", 
                      osTrailerFile.c_str() );
        }

/* -------------------------------------------------------------------- */
/*      Copy into our DXF file.                                         */
/* -------------------------------------------------------------------- */
        else
        {
            const char *pszLine;
            
            while( (pszLine = CPLReadLineL(fpSrc)) != NULL )
            {
                VSIFWriteL( pszLine, 1, strlen(pszLine), fp );
                VSIFWriteL( "\n", 1, 1, fp );
            }
            
            VSIFCloseL( fpSrc );
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Close file.                                                     */
/* -------------------------------------------------------------------- */
    if( fp != NULL )
    {
        VSIFCloseL( fp );
        fp = NULL;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFWriterDS::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/


OGRLayer *OGRDXFWriterDS::GetLayer( int iLayer )

{
    if( iLayer == 0 )
        return poLayer;
    else
        return NULL;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRDXFWriterDS::GetLayerCount()

{
    if( poLayer )
        return 1;
    else
        return 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDXFWriterDS::Open( const char * pszFilename, char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Open the standard header, or a user provided header.            */
/* -------------------------------------------------------------------- */
    CPLString osHeaderFile;

    if( CSLFetchNameValue(papszOptions,"HEADER") != NULL )
        osHeaderFile = CSLFetchNameValue(papszOptions,"HEADER");
    else
    {
        const char *pszValue = CPLFindFile( "gdal", "header.dxf" );
        if( pszValue == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to find template header file header.dxf for reading,\nis GDAL_DATA set properly?" );
            return FALSE;
        }
        osHeaderFile = pszValue;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "w" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open '%s' for writing.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Open the template file.                                         */
/* -------------------------------------------------------------------- */
    FILE *fpSrc = VSIFOpenL( osHeaderFile, "r" );
    if( fpSrc == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open template header file '%s' for reading.", 
                  osHeaderFile.c_str() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Copy into our DXF file.                                         */
/* -------------------------------------------------------------------- */
    const char *pszLine;

    while( (pszLine = CPLReadLineL(fpSrc)) != NULL )
    {
        VSIFWriteL( pszLine, 1, strlen(pszLine), fp );
        VSIFWriteL( "\n", 1, 1, fp );
    }

    VSIFCloseL( fpSrc );

/* -------------------------------------------------------------------- */
/*      Establish the name for our trailer file.                        */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"TRAILER") != NULL )
        osTrailerFile = CSLFetchNameValue(papszOptions,"TRAILER");
    else
    {
        const char *pszValue = CPLFindFile( "gdal", "trailer.dxf" );
        if( pszValue != NULL )
            osTrailerFile = pszValue;
    }

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDXFWriterDS::CreateLayer( const char *, 
                                       OGRSpatialReference *, 
                                       OGRwkbGeometryType, 
                                       char ** )

{
    if( poLayer == NULL )
    {
        poLayer = new OGRDXFWriterLayer( fp );
        return poLayer;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to more than one OGR layer in a DXF file." );
        return NULL;
    }
}
