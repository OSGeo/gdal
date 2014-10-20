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
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
    poBlocksLayer = NULL;
    papszLayersToCreate = NULL;
    nNextFID = 80;
    nHANDSEEDOffset = 0;
}

/************************************************************************/
/*                         ~OGRDXFWriterDS()                          */
/************************************************************************/

OGRDXFWriterDS::~OGRDXFWriterDS()

{
    if (fp != NULL)
    {
/* -------------------------------------------------------------------- */
/*      Transfer over the header into the destination file with any     */
/*      adjustments or insertions needed.                               */
/* -------------------------------------------------------------------- */
        CPLDebug( "DXF", "Compose final DXF file from components." );

        TransferUpdateHeader( fp );

        if (fpTemp != NULL)
        {
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
        }

/* -------------------------------------------------------------------- */
/*      Write trailer.                                                  */
/* -------------------------------------------------------------------- */
        if( osTrailerFile != "" )
            TransferUpdateTrailer( fp );

/* -------------------------------------------------------------------- */
/*      Fixup the HANDSEED value now that we know all the entity ids    */
/*      created.                                                        */
/* -------------------------------------------------------------------- */
        FixupHANDSEED( fp );

/* -------------------------------------------------------------------- */
/*      Close file.                                                     */
/* -------------------------------------------------------------------- */

        VSIFCloseL( fp );
        fp = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Destroy layers.                                                 */
/* -------------------------------------------------------------------- */
    delete poLayer;
    delete poBlocksLayer;

    CSLDestroy(papszLayersToCreate);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFWriterDS::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        // Unable to have more than one OGR entities layer in a DXF file, with one options blocks layer.
        return poBlocksLayer == NULL || poLayer == NULL;
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
/*      What entity id do we want to start with when writing?  Small    */
/*      values run a risk of overlapping some undetected entity in      */
/*      the header or trailer despite the prescanning we do.            */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    nNextFID = 80;
#else
    nNextFID = 131072;
#endif

    if( CSLFetchNameValue( papszOptions, "FIRST_ENTITY" ) != NULL )
        nNextFID = atoi(CSLFetchNameValue( papszOptions, "FIRST_ENTITY" ));

/* -------------------------------------------------------------------- */
/*      Prescan the header and trailer for entity codes.                */
/* -------------------------------------------------------------------- */
    ScanForEntities( osHeaderFile, "HEADER" );
    ScanForEntities( osTrailerFile, "TRAILER" );

/* -------------------------------------------------------------------- */
/*      Attempt to read the template header file so we have a list      */
/*      of layers, linestyles and blocks.                               */
/* -------------------------------------------------------------------- */
    if( !oHeaderDS.Open( osHeaderFile, TRUE ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "w+" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open '%s' for writing.", 
                  pszFilename );
        return FALSE;
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
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDXFWriterDS::ICreateLayer( const char *pszName, 
                                       OGRSpatialReference *, 
                                       OGRwkbGeometryType, 
                                       char ** )

{
    if( EQUAL(pszName,"blocks") && poBlocksLayer == NULL )
    {
        poBlocksLayer = new OGRDXFBlocksWriterLayer( this );
        return poBlocksLayer;
    }
    else if( poLayer == NULL )
    {
        poLayer = new OGRDXFWriterLayer( this, fpTemp );
        return poLayer;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to have more than one OGR entities layer in a DXF file, with one options blocks layer." );
        return NULL;
    }
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

static int WriteValue( VSILFILE *fp, int nCode, const char *pszLine )

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
/*                             WriteValue()                             */
/************************************************************************/

static int WriteValue( VSILFILE *fp, int nCode, double dfValue )

{
    char szLinePair[64];

    snprintf(szLinePair, sizeof(szLinePair), "%3d\n%.15g\n", nCode, dfValue );
    char* pszComma = strchr(szLinePair, ',');
    if (pszComma)
        *pszComma = '.';
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

int OGRDXFWriterDS::TransferUpdateHeader( VSILFILE *fpOut )

{
    oHeaderDS.ResetReadPointer( 0 );

/* -------------------------------------------------------------------- */
/*      Copy header, inserting in new objects as needed.                */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];
    int nCode;
    CPLString osSection, osTable, osEntity;

    while( (nCode = oHeaderDS.ReadValue( szLineBuf, sizeof(szLineBuf) )) != -1 
           && osSection != "ENTITIES" )
    {
        if( nCode == 0 && EQUAL(szLineBuf,"ENDTAB") )
        {
            // If we are at the end of the LAYER TABLE consider inserting 
            // missing definitions.
            if( osTable == "LAYER" )
            {
                if( !WriteNewLayerDefinitions( fp ) )
                    return FALSE;
            }

            // If at the end of the BLOCK_RECORD TABLE consider inserting 
            // missing definitions.
            if( osTable == "BLOCK_RECORD" && poBlocksLayer )
            {
                if( !WriteNewBlockRecords( fp ) )
                    return FALSE;
            }

            // If at the end of the LTYPE TABLE consider inserting 
            // missing layer type definitions.
            if( osTable == "LTYPE" )
            {
                if( !WriteNewLineTypeRecords( fp ) )
                    return FALSE;
            }

            osTable = "";
        }

        // If we are at the end of the BLOCKS section, consider inserting
        // suplementary blocks. 
        if( nCode == 0 && osSection == "BLOCKS" && EQUAL(szLineBuf,"ENDSEC") 
            && poBlocksLayer != NULL )
        {
            if( !WriteNewBlockDefinitions( fp ) )
                return FALSE;
        }

        // We need to keep track of where $HANDSEED is so that we can
        // come back and fix it up when we have generated all entity ids. 
        if( nCode == 9 && EQUAL(szLineBuf,"$HANDSEED") )
        {
            if( !WriteValue( fpOut, nCode, szLineBuf ) )
                return FALSE;

            nCode = oHeaderDS.ReadValue( szLineBuf, sizeof(szLineBuf) );

            // ensure we have room to overwrite with a longer value.
            while( strlen(szLineBuf) < 8 )
            {
                memmove( szLineBuf+1, szLineBuf, strlen(szLineBuf)+1 );
                szLineBuf[0] = '0';
            }

            nHANDSEEDOffset = VSIFTellL( fpOut );
        }

        // Patch EXTMIN with minx and miny
        if( nCode == 9 && EQUAL(szLineBuf,"$EXTMIN") )
        {
            if( !WriteValue( fpOut, nCode, szLineBuf ) )
                return FALSE;

            nCode = oHeaderDS.ReadValue( szLineBuf, sizeof(szLineBuf) );
            if (nCode == 10)
            {
                if( !WriteValue( fpOut, nCode, oGlobalEnvelope.MinX ) )
                    return FALSE;

                nCode = oHeaderDS.ReadValue( szLineBuf, sizeof(szLineBuf) );
                if (nCode == 20)
                {
                    if( !WriteValue( fpOut, nCode, oGlobalEnvelope.MinY ) )
                        return FALSE;

                    continue;
                }
            }
        }

        // Patch EXTMAX with maxx and maxy
        if( nCode == 9 && EQUAL(szLineBuf,"$EXTMAX") )
        {
            if( !WriteValue( fpOut, nCode, szLineBuf ) )
                return FALSE;

            nCode = oHeaderDS.ReadValue( szLineBuf, sizeof(szLineBuf) );
            if (nCode == 10)
            {
                if( !WriteValue( fpOut, nCode, oGlobalEnvelope.MaxX ) )
                    return FALSE;

                nCode = oHeaderDS.ReadValue( szLineBuf, sizeof(szLineBuf) );
                if (nCode == 20)
                {
                    if( !WriteValue( fpOut, nCode, oGlobalEnvelope.MaxY ) )
                        return FALSE;

                    continue;
                }
            }
        }

        // Copy over the source line.
        if( !WriteValue( fpOut, nCode, szLineBuf ) )
            return FALSE;

        // Track what entity we are in - that is the last "code 0" object.
        if( nCode == 0  )
            osEntity = szLineBuf;

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
/*                       TransferUpdateTrailer()                        */
/************************************************************************/

int OGRDXFWriterDS::TransferUpdateTrailer( VSILFILE *fpOut )
{
    OGRDXFReader oReader;
    VSILFILE *fp;

/* -------------------------------------------------------------------- */
/*      Open the file and setup a reader.                               */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( osTrailerFile, "r" );

    if( fp == NULL )
        return FALSE;

    oReader.Initialize( fp );

/* -------------------------------------------------------------------- */
/*      Scan ahead to find the OBJECTS section.                         */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];
    int  nCode;

    while( (nCode = oReader.ReadValue( szLineBuf, sizeof(szLineBuf) )) != -1 )
    {
        if( nCode == 0 && EQUAL(szLineBuf,"SECTION") )
        {
            nCode = oReader.ReadValue( szLineBuf, sizeof(szLineBuf) );
            if( nCode == 2 && EQUAL(szLineBuf,"OBJECTS") )
                break;
        }
    }

    if( nCode == -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find OBJECTS section in trailer file '%s'.",
                  osTrailerFile.c_str() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Insert the "end of section" for ENTITIES, and the start of      */
/*      the OBJECTS section.                                            */
/* -------------------------------------------------------------------- */
    WriteValue( fpOut, 0, "ENDSEC" );
    WriteValue( fpOut, 0, "SECTION" );
    WriteValue( fpOut, 2, "OBJECTS" );

/* -------------------------------------------------------------------- */
/*      Copy the remainder of the file.                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = oReader.ReadValue( szLineBuf, sizeof(szLineBuf) )) != -1 )
    {
        if( !WriteValue( fpOut, nCode, szLineBuf ) )
        {
            VSIFCloseL( fp );
            return FALSE;
        }
    }

    VSIFCloseL( fp );

    return TRUE;
}

/************************************************************************/
/*                           FixupHANDSEED()                            */
/*                                                                      */
/*      Fixup the next entity id information in the $HANDSEED header    */
/*      variable.                                                       */
/************************************************************************/

int OGRDXFWriterDS::FixupHANDSEED( VSILFILE *fp )

{
/* -------------------------------------------------------------------- */
/*      What is a good next handle seed?  Scan existing values.         */
/* -------------------------------------------------------------------- */
    unsigned int   nHighestHandle = 0;
    std::set<CPLString>::iterator it;

    for( it = aosUsedEntities.begin(); it != aosUsedEntities.end(); it++ ) 
    {
        unsigned int nHandle;
        if( sscanf( (*it).c_str(), "%x", &nHandle ) == 1 )
        {
            if( nHandle > nHighestHandle )
                nHighestHandle = nHandle;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the existing handseed value, replace it, and write back.   */
/* -------------------------------------------------------------------- */
    char szWorkBuf[30];
    int  i = 0;

    if( nHANDSEEDOffset == 0 )
        return FALSE;

    VSIFSeekL( fp, nHANDSEEDOffset, SEEK_SET );
    VSIFReadL( szWorkBuf, 1, sizeof(szWorkBuf), fp );
    
    while( szWorkBuf[i] != '\n' )
        i++;

    i++;
    if( szWorkBuf[i] == '\r' )
        i++;
    
    CPLString osNewValue;

    osNewValue.Printf( "%08X", nHighestHandle + 1 );
    strncpy( szWorkBuf + i, osNewValue.c_str(), osNewValue.size() );

    VSIFSeekL( fp, nHANDSEEDOffset, SEEK_SET );
    VSIFWriteL( szWorkBuf, 1, sizeof(szWorkBuf), fp );

    return TRUE;
}

/************************************************************************/
/*                      WriteNewLayerDefinitions()                      */
/************************************************************************/

int  OGRDXFWriterDS::WriteNewLayerDefinitions( VSILFILE * fpOut )

{                                               
    int iLayer, nNewLayers = CSLCount(papszLayersToCreate);

    for( iLayer = 0; iLayer < nNewLayers; iLayer++ )
    {
        for( unsigned i = 0; i < aosDefaultLayerText.size(); i++ )
        {
            if( anDefaultLayerCode[i] == 2 )
            {
                if( !WriteValue( fpOut, 2, papszLayersToCreate[iLayer] ) )
                    return FALSE;
            }
            else if( anDefaultLayerCode[i] == 5 )
            {
                WriteEntityID( fpOut );
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

/************************************************************************/
/*                      WriteNewLineTypeRecords()                       */
/************************************************************************/

int OGRDXFWriterDS::WriteNewLineTypeRecords( VSILFILE *fp )

{
    if( poLayer == NULL )
        return TRUE;

    std::map<CPLString,CPLString>::iterator oIt;
    std::map<CPLString,CPLString>& oNewLineTypes = 
        poLayer->GetNewLineTypeMap();

    for( oIt = oNewLineTypes.begin(); 
         oIt != oNewLineTypes.end(); oIt++ )
    {
        WriteValue( fp, 0, "LTYPE" );
        WriteEntityID( fp );
        WriteValue( fp, 100, "AcDbSymbolTableRecord" );
        WriteValue( fp, 100, "AcDbLinetypeTableRecord" );
        WriteValue( fp, 2, (*oIt).first );
        WriteValue( fp, 70, "0" );
        WriteValue( fp, 3, "" );
        WriteValue( fp, 72, "65" );
        VSIFWriteL( (*oIt).second.c_str(), 1, (*oIt).second.size(), fp );

        CPLDebug( "DXF", "Define Line type '%s'.", 
                  (*oIt).first.c_str() );
    }

    return TRUE;
}

/************************************************************************/
/*                        WriteNewBlockRecords()                        */
/************************************************************************/

int OGRDXFWriterDS::WriteNewBlockRecords( VSILFILE * fp )

{
    std::set<CPLString> aosAlreadyHandled;

/* ==================================================================== */
/*      Loop over all block objects written via the blocks layer.       */
/* ==================================================================== */
    for( size_t iBlock=0; iBlock < poBlocksLayer->apoBlocks.size(); iBlock++ )
    {
        OGRFeature* poThisBlockFeat = poBlocksLayer->apoBlocks[iBlock];

/* -------------------------------------------------------------------- */
/*      Is this block already defined in the template header?           */
/* -------------------------------------------------------------------- */
        CPLString osBlockName = poThisBlockFeat->GetFieldAsString("BlockName");

        if( oHeaderDS.LookupBlock( osBlockName ) != NULL )
            continue;

/* -------------------------------------------------------------------- */
/*      Have we already written a BLOCK_RECORD for this block?          */
/* -------------------------------------------------------------------- */
        if( aosAlreadyHandled.find(osBlockName) != aosAlreadyHandled.end() )
            continue;

        aosAlreadyHandled.insert( osBlockName );

/* -------------------------------------------------------------------- */
/*      Write the block record.                                         */
/* -------------------------------------------------------------------- */
        WriteValue( fp, 0, "BLOCK_RECORD" );
        WriteEntityID( fp );
        WriteValue( fp, 100, "AcDbSymbolTableRecord" );
        WriteValue( fp, 100, "AcDbBlockTableRecord" );
        WriteValue( fp, 2, poThisBlockFeat->GetFieldAsString("BlockName") );
        if( !WriteValue( fp, 340, "0" ) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                      WriteNewBlockDefinitions()                      */
/************************************************************************/

int OGRDXFWriterDS::WriteNewBlockDefinitions( VSILFILE * fp )

{
    poLayer->ResetFP( fp );

/* ==================================================================== */
/*      Loop over all block objects written via the blocks layer.       */
/* ==================================================================== */
    for( size_t iBlock=0; iBlock < poBlocksLayer->apoBlocks.size(); iBlock++ )
    {
        OGRFeature* poThisBlockFeat = poBlocksLayer->apoBlocks[iBlock];

/* -------------------------------------------------------------------- */
/*      Is this block already defined in the template header?           */
/* -------------------------------------------------------------------- */
        CPLString osBlockName = poThisBlockFeat->GetFieldAsString("BlockName");

        if( oHeaderDS.LookupBlock( osBlockName ) != NULL )
            continue;

/* -------------------------------------------------------------------- */
/*      Write the block definition preamble.                            */
/* -------------------------------------------------------------------- */
        CPLDebug( "DXF", "Writing BLOCK definition for '%s'.",
                  poThisBlockFeat->GetFieldAsString("BlockName") );

        WriteValue( fp, 0, "BLOCK" );
        WriteEntityID( fp );
        WriteValue( fp, 100, "AcDbEntity" );
        if( strlen(poThisBlockFeat->GetFieldAsString("Layer")) > 0 )
            WriteValue( fp, 8, poThisBlockFeat->GetFieldAsString("Layer") );
        else
            WriteValue( fp, 8, "0" );
        WriteValue( fp, 100, "AcDbBlockBegin" );
        WriteValue( fp, 2, poThisBlockFeat->GetFieldAsString("BlockName") );
        WriteValue( fp, 70, "0" );

        // Origin
        WriteValue( fp, 10, "0.0" );
        WriteValue( fp, 20, "0.0" );
        WriteValue( fp, 30, "0.0" );

        WriteValue( fp, 3, poThisBlockFeat->GetFieldAsString("BlockName") );
        WriteValue( fp, 1, "" );

/* -------------------------------------------------------------------- */
/*      Write out the feature entities.                                 */
/* -------------------------------------------------------------------- */
        if( poLayer->CreateFeature( poThisBlockFeat ) != OGRERR_NONE )
            return FALSE;

/* -------------------------------------------------------------------- */
/*      Write out following features if they are the same block.        */
/* -------------------------------------------------------------------- */
        while( iBlock < poBlocksLayer->apoBlocks.size()-1 
            && EQUAL(poBlocksLayer->apoBlocks[iBlock+1]->GetFieldAsString("BlockName"),
                     osBlockName) )
        {
            iBlock++;
            
            if( poLayer->CreateFeature( poBlocksLayer->apoBlocks[iBlock] )
                != OGRERR_NONE )
                return FALSE;
        }
        
/* -------------------------------------------------------------------- */
/*      Write out the block definition postamble.                       */
/* -------------------------------------------------------------------- */
        WriteValue( fp, 0, "ENDBLK" );
        WriteEntityID( fp );
        WriteValue( fp, 100, "AcDbEntity" );
        if( strlen(poThisBlockFeat->GetFieldAsString("Layer")) > 0 )
            WriteValue( fp, 8, poThisBlockFeat->GetFieldAsString("Layer") );
        else
            WriteValue( fp, 8, "0" );
        WriteValue( fp, 100, "AcDbBlockEnd" );
    }

    return TRUE;
}

/************************************************************************/
/*                          ScanForEntities()                           */
/*                                                                      */
/*      Scan the indicated file for entity ids ("5" records) and        */
/*      build them up as a set so we can be careful to avoid            */
/*      creating new entities with conflicting ids.                     */
/************************************************************************/

void OGRDXFWriterDS::ScanForEntities( const char *pszFilename,
                                      const char *pszTarget )

{
    OGRDXFReader oReader;
    VSILFILE *fp;

/* -------------------------------------------------------------------- */
/*      Open the file and setup a reader.                               */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "r" );

    if( fp == NULL )
        return;

    oReader.Initialize( fp );

/* -------------------------------------------------------------------- */
/*      Add every code "5" line to our entities list.                   */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];
    int  nCode;
    const char *pszPortion = "HEADER";

    while( (nCode = oReader.ReadValue( szLineBuf, sizeof(szLineBuf) )) != -1 )
    {
        if( (nCode == 5 || nCode == 105) && EQUAL(pszTarget,pszPortion) )
        {
            CPLString osEntity( szLineBuf );

            if( CheckEntityID( osEntity ) )
                CPLDebug( "DXF", "Encounted entity '%s' multiple times.",
                          osEntity.c_str() );
            else
                aosUsedEntities.insert( osEntity );
        }

        if( nCode == 0 && EQUAL(szLineBuf,"SECTION") )
        {
            nCode = oReader.ReadValue( szLineBuf, sizeof(szLineBuf) );
            if( nCode == 2 && EQUAL(szLineBuf,"ENTITIES") )
                pszPortion = "BODY";
            if( nCode == 2 && EQUAL(szLineBuf,"OBJECTS") )
                pszPortion = "TRAILER";
        }
    }

    VSIFCloseL( fp );
}

/************************************************************************/
/*                           CheckEntityID()                            */
/*                                                                      */
/*      Does the mentioned entity already exist?                        */
/************************************************************************/

int OGRDXFWriterDS::CheckEntityID( const char *pszEntityID )

{
    std::set<CPLString>::iterator it;

    it = aosUsedEntities.find( pszEntityID );
    if( it != aosUsedEntities.end() )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           WriteEntityID()                            */
/************************************************************************/

long OGRDXFWriterDS::WriteEntityID( VSILFILE *fp, long nPreferredFID )

{
    CPLString osEntityID;

    if( nPreferredFID != OGRNullFID )
    {
        
        osEntityID.Printf( "%X", (unsigned int) nPreferredFID );
        if( !CheckEntityID( osEntityID ) )
        {
            aosUsedEntities.insert( osEntityID );
            WriteValue( fp, 5, osEntityID );
            return nPreferredFID;
        }
    }

    do 
    {
        osEntityID.Printf( "%X", nNextFID++ );
    }
    while( CheckEntityID( osEntityID ) );
    
    aosUsedEntities.insert( osEntityID );
    WriteValue( fp, 5, osEntityID );

    return nNextFID - 1;
}

/************************************************************************/
/*                           UpdateExtent()                             */
/************************************************************************/

void OGRDXFWriterDS::UpdateExtent( OGREnvelope* psEnvelope )
{
    oGlobalEnvelope.Merge(*psEnvelope);
}
