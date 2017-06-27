/******************************************************************************
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1DataSource class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2007-2008, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili1reader.h"
#include "ogr_ili1.h"

#include <string>

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRILI1DataSource()                         */
/************************************************************************/

OGRILI1DataSource::OGRILI1DataSource() :
    pszName(NULL),
    poImdReader(new ImdReader(1)),
    poReader(NULL),
    fpTransfer(NULL),
    pszTopic(NULL),
    nLayers(0),
    papoLayers(NULL)
{}

/************************************************************************/
/*                        ~OGRILI1DataSource()                         */
/************************************************************************/

OGRILI1DataSource::~OGRILI1DataSource()

{
    for( int i=0; i<nLayers; i++)
    {
        delete papoLayers[i];
    }
    CPLFree( papoLayers );

    CPLFree( pszName );
    CPLFree( pszTopic );
    DestroyILI1Reader( poReader );
    delete poImdReader;
    if( fpTransfer )
    {
        VSIFPrintfL( fpTransfer, "ETAB\n" );
        VSIFPrintfL( fpTransfer, "ETOP\n" );
        VSIFPrintfL( fpTransfer, "EMOD\n" );
        VSIFPrintfL( fpTransfer, "ENDE\n" );
        VSIFCloseL(fpTransfer);
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRILI1DataSource::Open( const char * pszNewName,
                             char** papszOpenOptionsIn, int bTestOpen )

{
    if (strlen(pszNewName) == 0)
    {
        return FALSE;
    }

    std::string osBasename;
    std::string osModelFilename;
    if( CSLFetchNameValue(papszOpenOptionsIn, "MODEL") != NULL )
    {
        osBasename = pszNewName;
        osModelFilename = CSLFetchNameValue(papszOpenOptionsIn, "MODEL");
    }
    else
    {
        char **filenames = CSLTokenizeString2( pszNewName, ",", 0 );
        int nCount = CSLCount(filenames);
        if( nCount == 0 )
        {
            CSLDestroy(filenames);
            return FALSE;
        }
        osBasename = filenames[0];

        if( nCount > 1 )
            osModelFilename = filenames[1];

        CSLDestroy( filenames );
    }

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osBasename.c_str(), "r" );
    if( fp == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open ILI1 file `%s'.",
                      pszNewName );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is ILI1, load a header chunk and check      */
/*      for signs it is ILI1                                             */
/* -------------------------------------------------------------------- */
    char szHeader[1000];

    if( bTestOpen )
    {
        int nLen = (int)VSIFReadL( szHeader, 1, sizeof(szHeader), fp );
        if (nLen == sizeof(szHeader))
            szHeader[sizeof(szHeader)-1] = '\0';
        else
            szHeader[nLen] = '\0';

        if( strstr(szHeader,"SCNT") == NULL )
        {
            VSIFCloseL( fp );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      We assume now that it is ILI1.  Close and instantiate a          */
/*      ILI1Reader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );

    poReader = CreateILI1Reader();
    if( poReader == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s appears to be ILI1 but the ILI1 reader cannot\n"
                  "be instantiated, likely because Xerces support was not\n"
                  "configured in.",
                  pszNewName );
        return FALSE;
     }

    poReader->OpenFile( osBasename.c_str() );

    pszName = CPLStrdup( osBasename.c_str() );

    if (osModelFilename.length() > 0 )
        poReader->ReadModel( poImdReader, osModelFilename.c_str(), this );

    int bResetConfigOption = FALSE;
    if (EQUAL(CPLGetConfigOption("OGR_ARC_STEPSIZE", ""), ""))
    {
        bResetConfigOption = TRUE;
        CPLSetThreadLocalConfigOption("OGR_ARC_STEPSIZE", "0.96");
    }

    // Parse model and read data - without surface join and area polygonizing.
    poReader->ReadFeatures();

    if( bResetConfigOption )
        CPLSetThreadLocalConfigOption("OGR_ARC_STEPSIZE", NULL);

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRILI1DataSource::Create( const char *pszFilename,
                               char ** /* papszOptions */)
{
    char **filenames = CSLTokenizeString2( pszFilename, ",", 0 );

    std::string osBasename = filenames[0];

    std::string osModelFilename;
    if( CSLCount(filenames) > 1 )
        osModelFilename = filenames[1];

    CSLDestroy( filenames );

/* -------------------------------------------------------------------- */
/*      Create the empty file.                                          */
/* -------------------------------------------------------------------- */
    fpTransfer = VSIFOpenL( osBasename.c_str(), "w+b" );

    if( fpTransfer == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create %s:\n%s",
                  osBasename.c_str(), VSIStrerror( errno ) );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Parse model                                                     */
/* -------------------------------------------------------------------- */
    if( osModelFilename.length() == 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
            "Creating Interlis transfer file without model definition." );
    } else {
        poImdReader->ReadModel(osModelFilename.c_str());
    }

    pszTopic = CPLStrdup(poImdReader->mainTopicName.c_str());

/* -------------------------------------------------------------------- */
/*      Write headers                                                   */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fpTransfer, "SCNT\n" );
    VSIFPrintfL( fpTransfer, "OGR/GDAL %s, INTERLIS Driver\n",
                GDAL_RELEASE_NAME );
    VSIFPrintfL( fpTransfer, "////\n" );
    VSIFPrintfL( fpTransfer, "MTID INTERLIS1\n" );
    const char* modelname = poImdReader->mainModelName.c_str();
    VSIFPrintfL( fpTransfer, "MODL %s\n", modelname );

    return TRUE;
}

static char *ExtractTopic(const char * pszLayerName)
{
  const char *table = strchr(pszLayerName, '_');
  while (table && table[1] !=  '_') table = strchr(table+1, '_');
  return (table) ? CPLScanString(
      pszLayerName, static_cast<int>(table-pszLayerName), FALSE, FALSE) : NULL;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRILI1DataSource::ICreateLayer( const char * pszLayerName,
                               CPL_UNUSED OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               CPL_UNUSED char ** papszOptions )
{
    FeatureDefnInfo featureDefnInfo
        = poImdReader->GetFeatureDefnInfo(pszLayerName);
    const char *table = pszLayerName;
    char * topic = ExtractTopic(pszLayerName);
    if (nLayers) VSIFPrintfL( fpTransfer, "ETAB\n" );
    if (topic)
    {
      table = pszLayerName+strlen(topic)+2; //after "__"
      if (pszTopic == NULL || !EQUAL(topic, pszTopic))
      {
        if (pszTopic)
        {
          VSIFPrintfL( fpTransfer, "ETOP\n" );
          CPLFree(pszTopic);
        }
        pszTopic = topic;
        VSIFPrintfL( fpTransfer, "TOPI %s\n", pszTopic );
      }
      else
      {
        CPLFree(topic);
      }
    }
    else
    {
      if (pszTopic == NULL) pszTopic = CPLStrdup("Unknown");
      VSIFPrintfL( fpTransfer, "TOPI %s\n", pszTopic );
    }
    VSIFPrintfL( fpTransfer, "TABL %s\n", table );

    OGRFeatureDefn* poFeatureDefn = new OGRFeatureDefn(table);
    poFeatureDefn->SetGeomType( eType );
    OGRILI1Layer *poLayer
        = new OGRILI1Layer(poFeatureDefn, featureDefnInfo.poGeomFieldInfos,
                           this);

    nLayers ++;
    papoLayers = static_cast<OGRILI1Layer **>(
        CPLRealloc(papoLayers, sizeof(OGRILI1Layer*) * nLayers) );
    papoLayers[nLayers-1] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI1DataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRILI1DataSource::GetLayer( int iLayer )
{
  return poReader->GetLayer( iLayer );
}

/************************************************************************/
/*                              GetLayerByName()                              */
/************************************************************************/

OGRILI1Layer *OGRILI1DataSource::GetLayerByName( const char* pszLayerName )
{
  return reinterpret_cast<OGRILI1Layer *>(
      poReader->GetLayerByName( pszLayerName ) );
}
