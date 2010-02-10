/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFDataSource class
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
/*                          OGRDXFDataSource()                          */
/************************************************************************/

OGRDXFDataSource::OGRDXFDataSource()

{
    fp = NULL;

    iSrcBufferOffset = 0;
    nSrcBufferBytes = 0;
    iSrcBufferFileOffset = 0;

    nLastValueSize = 0;
}

/************************************************************************/
/*                         ~OGRDXFDataSource()                          */
/************************************************************************/

OGRDXFDataSource::~OGRDXFDataSource()

{
/* -------------------------------------------------------------------- */
/*      Destroy layers.                                                 */
/* -------------------------------------------------------------------- */
    while( apoLayers.size() > 0 )
    {
        delete apoLayers.back();
        apoLayers.pop_back();
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

int OGRDXFDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/


OGRLayer *OGRDXFDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= (int) apoLayers.size() )
        return NULL;
    else
        return apoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDXFDataSource::Open( const char * pszFilename )

{
    if( !EQUAL(CPLGetExtension(pszFilename),"dxf") )
        return FALSE;

    osName = pszFilename;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "r" );
    if( fp == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Confirm we have a header section.                               */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];
    int  nCode;
    int  bEntitiesOnly = FALSE;

    if( ReadValue( szLineBuf ) != 0 || !EQUAL(szLineBuf,"SECTION") )
        return FALSE;

    if( ReadValue( szLineBuf ) != 2 
        || (!EQUAL(szLineBuf,"HEADER") && !EQUAL(szLineBuf,"ENTITIES")) )
        return FALSE;

    if( EQUAL(szLineBuf,"ENTITIES") )
        bEntitiesOnly = TRUE;

/* -------------------------------------------------------------------- */
/*      Process the header, picking up a few useful pieces of           */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( !bEntitiesOnly )
    {
        ReadHeaderSection();
        ReadValue(szLineBuf);

/* -------------------------------------------------------------------- */
/*      Process the CLASSES section, if present.                        */
/* -------------------------------------------------------------------- */
        if( EQUAL(szLineBuf,"ENDSEC") )
            ReadValue(szLineBuf);

        if( EQUAL(szLineBuf,"SECTION") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"CLASSES") )
        {
            while( (nCode = ReadValue( szLineBuf,sizeof(szLineBuf) )) > -1 
                   && !EQUAL(szLineBuf,"ENDSEC") )
            {
                //printf("C:%d/%s\n", nCode, szLineBuf );
            }
        }

/* -------------------------------------------------------------------- */
/*      Process the TABLES section, if present.                         */
/* -------------------------------------------------------------------- */
        if( EQUAL(szLineBuf,"ENDSEC") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"SECTION") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"TABLES") )
        {
            ReadTablesSection();
            ReadValue(szLineBuf);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create out layer object - we will need it when interpreting     */
/*      blocks.                                                         */
/* -------------------------------------------------------------------- */
    apoLayers.push_back( new OGRDXFLayer( this ) );

/* -------------------------------------------------------------------- */
/*      Process the BLOCKS section if present.                          */
/* -------------------------------------------------------------------- */
    if( !bEntitiesOnly )
    {
        if( EQUAL(szLineBuf,"ENDSEC") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"SECTION") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"BLOCKS") )
        {
            ReadBlocksSection();
            ReadValue(szLineBuf);
        }
    }

/* -------------------------------------------------------------------- */
/*      Now we are at the entities section, hopefully.  Confirm.        */
/* -------------------------------------------------------------------- */
    if( EQUAL(szLineBuf,"SECTION") )
        ReadValue(szLineBuf);

    if( !EQUAL(szLineBuf,"ENTITIES") )
        return FALSE;

    iEntitiesSectionOffset = iSrcBufferFileOffset + iSrcBufferOffset;
    apoLayers[0]->ResetReading();

    return TRUE;
}

/************************************************************************/
/*                         ReadTablesSection()                          */
/************************************************************************/

void OGRDXFDataSource::ReadTablesSection()

{
    char szLineBuf[257];
    int  nCode;

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1 
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        // We are only interested in extracting tables.
        if( nCode != 0 || !EQUAL(szLineBuf,"TABLE") )
            continue;

        // Currently we are only interested in the LAYER table.
        nCode = ReadValue( szLineBuf, sizeof(szLineBuf) );

        if( nCode != 2 || !EQUAL(szLineBuf,"LAYER") )
            continue;

        while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1
               && !EQUAL(szLineBuf,"ENDTAB") )
        {
            if( nCode == 0 && EQUAL(szLineBuf,"LAYER") )
                ReadLayerDefinition();
        }
    }

    CPLDebug( "DXF", "Read %d layer definitions.", (int) oLayerTable.size() );
}

/************************************************************************/
/*                        ReadLayerDefinition()                         */
/************************************************************************/

void OGRDXFDataSource::ReadLayerDefinition()

{
    char szLineBuf[257];
    int  nCode;
    std::map<CPLString,CPLString> oLayerProperties;
    CPLString osLayerName = "";

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > 0 )
    {
        switch( nCode )
        {
          case 2:
            osLayerName = szLineBuf;
            break;

          case 6:
            oLayerProperties["Linetype"] = szLineBuf;
            break;
            
          case 62:
            oLayerProperties["Color"] = szLineBuf;
            break;
            
          case 70:
            oLayerProperties["Flags"] = szLineBuf;
            break;

          case 370:
          case 39:
            oLayerProperties["LineWeight"] = szLineBuf;
            break;

          default:
            break;
        }
    }

    if( oLayerProperties.size() > 0 )
        oLayerTable[osLayerName] = oLayerProperties;
    
    UnreadValue();
}

/************************************************************************/
/*                        LookupLayerProperty()                         */
/************************************************************************/

const char *OGRDXFDataSource::LookupLayerProperty( const char *pszLayer,
                                                   const char *pszProperty )

{
    if( pszLayer == NULL )
        return NULL;

    try {
        return (oLayerTable[pszLayer])[pszProperty];
    } catch( ... ) {
        return NULL;
    }
}

/************************************************************************/
/*                         ReadHeaderSection()                          */
/************************************************************************/

void OGRDXFDataSource::ReadHeaderSection()

{
    char szLineBuf[257];
    int  nCode;

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1 
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        if( nCode != 9 )
            continue;

        CPLString osName = szLineBuf;

        ReadValue( szLineBuf, sizeof(szLineBuf) );

        CPLString osValue = szLineBuf;

        oHeaderVariables[osName] = osValue;
    }

    CPLDebug( "DXF", "Read %d header variables.", 
              (int) oHeaderVariables.size() );
}

/************************************************************************/
/*                            GetVariable()                             */
/*                                                                      */
/*      Fetch a variable that came from the HEADER section.             */
/************************************************************************/

const char *OGRDXFDataSource::GetVariable( const char *pszName, 
                                           const char *pszDefault )

{
    if( oHeaderVariables.count(pszName) == 0 )
        return pszDefault;
    else 
        return oHeaderVariables[pszName];
}

