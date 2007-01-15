/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2003/11/07 21:55:12  warmerda
 * complete fid support, relative dsname, fixes
 *
 * Revision 1.1  2003/11/07 17:50:36  warmerda
 * New
 *
 */

#include "ogr_vrt.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRVRTDriver()                            */
/************************************************************************/

OGRVRTDriver::~OGRVRTDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRVRTDriver::GetName()

{
    return "VRT";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRVRTDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRVRTDataSource     *poDS;
    char *pszXML = NULL;

/* -------------------------------------------------------------------- */
/*      Are we being passed the XML definition directly?                */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename,"<OGRVRTDataSource>",18) )
    {
        pszXML = CPLStrdup(pszFilename);
    }

/* -------------------------------------------------------------------- */
/*      Open file and check if it contains appropriate XML.             */
/* -------------------------------------------------------------------- */
    else
    {
        FILE *fp;
        char achHeader[18];

        fp = VSIFOpen( pszFilename, "rb" );

        if( fp == NULL )
            return NULL;

        if( VSIFRead( achHeader, sizeof(achHeader), 1, fp ) != 1 )
        {
            VSIFClose( fp );
            return NULL;
        }

        if( !EQUALN(achHeader,"<OGRVRTDataSource>",18) )
        {
            VSIFClose( fp );
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      It is the right file, now load the full XML definition.         */
/* -------------------------------------------------------------------- */
        int nLen;

        VSIFSeek( fp, 0, SEEK_END );
        nLen = VSIFTell( fp );
        VSIFSeek( fp, 0, SEEK_SET );
        
        pszXML = (char *) CPLMalloc(nLen+1);
        pszXML[nLen] = '\0';
        if( ((int) VSIFRead( pszXML, 1, nLen, fp )) != nLen )
        {
            CPLFree( pszXML );
            VSIFClose( fp );

            return NULL;
        }
        VSIFClose( fp );
    }

/* -------------------------------------------------------------------- */
/*      We don't allow update access at this time through VRT           */
/*      datasources.                                                    */
/* -------------------------------------------------------------------- */
    if( bUpdate )
    {
        CPLFree( pszXML );
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Update access not supported for VRT datasources." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse the XML.                                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = CPLParseXMLString( pszXML );
    CPLFree( pszXML );

    if( psTree == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a virtual datasource configured based on this XML input. */
/* -------------------------------------------------------------------- */
    poDS = new OGRVRTDataSource();
    if( !poDS->Initialize( psTree, pszFilename ) )
    {
        CPLDestroyXMLNode( psTree );
        delete poDS;
        return NULL;
    }

    CPLDestroyXMLNode( psTree );

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRVRTDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRVRT()                           */
/************************************************************************/

void RegisterOGRVRT()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRVRTDriver );
}

