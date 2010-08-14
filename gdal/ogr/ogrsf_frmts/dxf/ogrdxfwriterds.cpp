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
    fpTemp = NULL;
    poLayer = NULL;
    papszLayersToCreate = NULL;
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
/*      Transfer over the header into the destination file with any     */
/*      adjustments or insertions needed.                               */
/* -------------------------------------------------------------------- */
    CPLDebug( "DXF", "Compose final DXF file from components." );

    TransferUpdateHeader( fp );

/* -------------------------------------------------------------------- */
/*      Copy in the temporary file contents.                            */
/* -------------------------------------------------------------------- */
    const char *pszLine;

    VSIFCloseL(fpTemp);
    fpTemp = VSIFOpenL( osTempFilename, "r" );

    while( (pszLine = CPLReadLineL(fpTemp)) != NULL )
    {
        VSIFWriteL( pszLine, 1, strlen(pszLine), fp );
        VSIFWriteL( "\n", 1, 1, fp );
    }
            
/* -------------------------------------------------------------------- */
/*      Cleanup temporary file.                                         */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fpTemp );
    VSIUnlink( osTempFilename );

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
/*      Attempt to read the template header file so we have a list      */
/*      of layers, linestyles and blocks.                               */
/* -------------------------------------------------------------------- */
    if( !oHeaderDS.Open( osHeaderFile, TRUE ) )
        return FALSE;

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

/* -------------------------------------------------------------------- */
/*      Establish the temporary file.                                   */
/* -------------------------------------------------------------------- */
    osTempFilename = pszFilename;
    osTempFilename += ".tmp";

    fpTemp = VSIFOpenL( osTempFilename, "w" );
    if( fpTemp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open '%s' for writing.", 
                  osTempFilename.c_str() );
        return FALSE;
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
        poLayer = new OGRDXFWriterLayer( this, fpTemp );
        return poLayer;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to have more than one OGR layer in a DXF file." );
        return NULL;
    }
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

static int WriteValue( FILE *fp, int nCode, const char *pszLine )

{
    char szLinePair[300];

    snprintf(szLinePair, sizeof(szLinePair), "%3d\n%s\n", nCode, pszLine );
    size_t nLen = strlen(szLinePair);
    if( VSIFWriteL( szLinePair, 1, nLen, fp ) != nLen )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Attempt to write line to DXF file failed, disk full?." );
        return FALSE;
    }
    else
        return TRUE;
}

/************************************************************************/
/*                        TransferUpdateHeader()                        */
/************************************************************************/

int OGRDXFWriterDS::TransferUpdateHeader( FILE *fpOut )

{
    oHeaderDS.ResetReadPointer( 0 );

/* -------------------------------------------------------------------- */
/*      Copy header, inserting in new objects as needed.                */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];
    int nCode;
    CPLString osSection, osTable;

    while( (nCode = oHeaderDS.ReadValue( szLineBuf )) != -1 )
    {
        if( nCode == 0 && EQUAL(szLineBuf,"ENDTAB") )
        {
            if( osTable == "LAYER" )
            {
                if( !WriteNewLayerDefinitions( fp ) )
                    return FALSE;
            }

            osTable = "";
        }

        if( !WriteValue( fpOut, nCode, szLineBuf ) )
            return FALSE;

        // Track what section we are in.
        if( nCode == 0 && EQUAL(szLineBuf,"SECTION") )
        {
            nCode = oHeaderDS.ReadValue( szLineBuf );
            if( nCode == -1 )
                break;

            if( !WriteValue( fpOut, nCode, szLineBuf ) )
                return FALSE;
                
            osSection = szLineBuf;
        }

        // Track what TABLE we are in.
        if( nCode == 0 && EQUAL(szLineBuf,"TABLE") )
        {
            nCode = oHeaderDS.ReadValue( szLineBuf );
            if( !WriteValue( fpOut, nCode, szLineBuf ) )
                return FALSE;

            osTable = szLineBuf;
        }

        // If we are starting the first layer, then capture
        // the layer contents while copying so we can duplicate
        // it for any new layer definitions.
        if( nCode == 0 && EQUAL(szLineBuf,"LAYER")
            && osTable == "LAYER" && aosDefaultLayerText.size() == 0 )
        {
            do { 
                anDefaultLayerCode.push_back( nCode );
                aosDefaultLayerText.push_back( szLineBuf );

                if( nCode != 0 && !WriteValue( fpOut, nCode, szLineBuf ) )
                    return FALSE;

                nCode = oHeaderDS.ReadValue( szLineBuf );

                if( nCode == 2 && !EQUAL(szLineBuf,"0") )
                {
                    anDefaultLayerCode.resize(0);
                    aosDefaultLayerText.resize(0);
                    break;
                }
            } while( nCode != 0 );

            oHeaderDS.UnreadValue();
        }
    }

    return TRUE;
}    

/************************************************************************/
/*                      WriteNewLayerDefinitions()                      */
/************************************************************************/

int  OGRDXFWriterDS::WriteNewLayerDefinitions( FILE * fpOut )

{                                               
    int iLayer, nNewLayers = CSLCount(papszLayersToCreate);

    for( iLayer = 0; iLayer < nNewLayers; iLayer++ )
    {
        for( unsigned i = 0; i < aosDefaultLayerText.size(); i++ )
        {
            CPLDebug( "DXF", "%d:%s", anDefaultLayerCode[i], aosDefaultLayerText[i].c_str() );

            if( anDefaultLayerCode[i] == 2 )
            {
                if( !WriteValue( fpOut, 2, papszLayersToCreate[iLayer] ) )
                    return FALSE;
            }
            else
            {
                if( !WriteValue( fpOut,
                                 anDefaultLayerCode[i],
                                 aosDefaultLayerText[i] ) )
                    return FALSE;
            }
        }
    }

    return TRUE;
}
