/******************************************************************************
 * $Id: ogrcsvdatasource.cpp 17806 2009-10-13 17:27:54Z rouault $
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
#include "cpl_csv.h"

CPL_CVSID("$Id: ogrcsvdatasource.cpp 17806 2009-10-13 17:27:54Z rouault $");

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

/* -------------------------------------------------------------------- */
/*      Destroy loaded block geometries.                                */
/* -------------------------------------------------------------------- */
    std::map<CPLString,OGRGeometry*>::iterator it;
    
    for( it = oBlockMap.begin(); it != oBlockMap.end(); it++ )
        delete it->second;
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

    if( ReadValue( szLineBuf ) != 0 || !EQUAL(szLineBuf,"SECTION") )
        return FALSE;

    if( ReadValue( szLineBuf ) != 2 || !EQUAL(szLineBuf,"HEADER") )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Process the header, picking up a few useful pieces of           */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1 
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        //printf("H:%d/%s\n", nCode, szLineBuf );
    }

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
        while( (nCode = ReadValue( szLineBuf,sizeof(szLineBuf) )) > -1 
               && !EQUAL(szLineBuf,"ENDSEC") )
        {
            //printf("T:%d/%s\n", nCode, szLineBuf );
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
    if( EQUAL(szLineBuf,"ENDSEC") )
        ReadValue(szLineBuf);

    if( EQUAL(szLineBuf,"SECTION") )
        ReadValue(szLineBuf);

    if( EQUAL(szLineBuf,"BLOCKS") )
    {
        ReadBlocksSection();
        ReadValue(szLineBuf);
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

