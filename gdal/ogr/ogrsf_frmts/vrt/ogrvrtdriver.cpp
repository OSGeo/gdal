/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                           OGRVRTErrorHandler()                       */
/************************************************************************/

static void CPL_STDCALL OGRVRTErrorHandler(CPL_UNUSED CPLErr eErr,
                                           CPL_UNUSED int nType,
                                           const char* pszMsg)
{
    std::vector<CPLString>* paosErrors = (std::vector<CPLString>* )CPLGetErrorHandlerUserData();
    paosErrors->push_back(pszMsg);
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
/*      Skip any leading spaces/blanks.                                 */
/* -------------------------------------------------------------------- */
    const char *pszTestXML = pszFilename;
    while( *pszTestXML != '\0' && isspace( (unsigned char)*pszTestXML ) )
        pszTestXML++;

    if( EQUALN(pszTestXML,"<OGRVRTDataSource>",18) )
    {
        pszXML = CPLStrdup(pszTestXML);
    }

/* -------------------------------------------------------------------- */
/*      Open file and check if it contains appropriate XML.             */
/* -------------------------------------------------------------------- */
    else
    {
        VSILFILE *fp;
        char achHeader[512];

        fp = VSIFOpenL( pszFilename, "rb" );

        if( fp == NULL )
            return NULL;

        memset( achHeader, 0, sizeof(achHeader) );
        VSIFReadL( achHeader, 1, sizeof(achHeader)-1, fp );

        if( strstr(achHeader,"<OGRVRTDataSource") == NULL )
        {
            VSIFCloseL( fp );
            return NULL;
        }

        VSIStatBufL sStatBuf;
        if ( VSIStatL( pszFilename, &sStatBuf ) != 0 ||
             sStatBuf.st_size > 1024 * 1024 )
        {
            CPLDebug( "VRT", "Unreasonable long file, not likely really VRT" );
            VSIFCloseL( fp );
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      It is the right file, now load the full XML definition.         */
/* -------------------------------------------------------------------- */
        int nLen = (int) sStatBuf.st_size;

        VSIFSeekL( fp, 0, SEEK_SET );

        pszXML = (char *) VSIMalloc(nLen+1);
        if (pszXML == NULL)
        {
            VSIFCloseL( fp );
            return NULL;
        }
        pszXML[nLen] = '\0';
        if( ((int) VSIFReadL( pszXML, 1, nLen, fp )) != nLen )
        {
            CPLFree( pszXML );
            VSIFCloseL( fp );

            return NULL;
        }
        VSIFCloseL( fp );
    }

/* -------------------------------------------------------------------- */
/*      Parse the XML.                                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = CPLParseXMLString( pszXML );

    if( psTree == NULL )
    {
        CPLFree( pszXML );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      XML Validation.                                                 */
/* -------------------------------------------------------------------- */
    if( CSLTestBoolean(CPLGetConfigOption("GDAL_XML_VALIDATION", "YES")) )
    {
        const char* pszXSD = CPLFindFile( "gdal", "ogrvrt.xsd" );
        if( pszXSD != NULL )
        {
            std::vector<CPLString> aosErrors;
            CPLPushErrorHandlerEx(OGRVRTErrorHandler, &aosErrors);
            int bRet = CPLValidateXML(pszXML, pszXSD, NULL);
            CPLPopErrorHandler();
            if( !bRet )
            {
                if( aosErrors.size() > 0 &&
                    strstr(aosErrors[0].c_str(), "missing libxml2 support") == NULL )
                {
                    for(size_t i = 0; i < aosErrors.size(); i++)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined, "%s", aosErrors[i].c_str());
                    }
                }
            }
            CPLErrorReset();
        }
    }
    CPLFree( pszXML );

/* -------------------------------------------------------------------- */
/*      Create a virtual datasource configured based on this XML input. */
/* -------------------------------------------------------------------- */
    poDS = new OGRVRTDataSource();
    poDS->SetDriver(this);
    /* psTree is owned by poDS */
    if( !poDS->Initialize( psTree, pszFilename, bUpdate ) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRVRTDriver::TestCapability( CPL_UNUSED const char * pszCap )
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
