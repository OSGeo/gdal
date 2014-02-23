/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2DataSource class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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

#include "ogr_ili2.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili2reader.h"

using namespace std;


CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRILI2DataSource()                         */
/************************************************************************/

OGRILI2DataSource::OGRILI2DataSource()

{
    pszName = NULL;
    poImdReader = new ImdReader(2);
    poReader = NULL;
    fpOutput = NULL;
    nLayers = 0;
    papoLayers = NULL;
}

/************************************************************************/
/*                        ~OGRILI2DataSource()                         */
/************************************************************************/

OGRILI2DataSource::~OGRILI2DataSource()

{
    int i;

    for(i=0;i<nLayers;i++)
    {
        delete papoLayers[i];
    }
    CPLFree( papoLayers );

    if ( fpOutput != NULL )
    {
        VSIFPrintfL(fpOutput, "</%s>\n", poImdReader->mainBasketName.c_str());
        VSIFPrintfL(fpOutput, "</DATASECTION>\n");
        VSIFPrintfL(fpOutput, "</TRANSFER>\n");
        VSIFCloseL(fpOutput);
    }

    DestroyILI2Reader( poReader );
    delete poImdReader;
    CPLFree( pszName );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRILI2DataSource::Open( const char * pszNewName, int bTestOpen )

{
    FILE        *fp;
    char        szHeader[1000];

    char *modelFilename = NULL;
    char **filenames = CSLTokenizeString2( pszNewName, ",", 0 );

    pszName = CPLStrdup( filenames[0] );

    if( CSLCount(filenames) > 1 )
        modelFilename = CPLStrdup( filenames[1] );

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszName, "r" );
    if( fp == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open ILI2 file `%s'.",
                      pszNewName );

        CSLDestroy( filenames );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is ILI2, load a header chunk and check      */
/*      for signs it is ILI2                                             */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        int nLen = (int)VSIFRead( szHeader, 1, sizeof(szHeader), fp );
        if (nLen == sizeof(szHeader))
            szHeader[sizeof(szHeader)-1] = '\0';
        else
            szHeader[nLen] = '\0';

        if( szHeader[0] != '<' 
            || strstr(szHeader,"interlis.ch/INTERLIS2") == NULL )
        { // "www.interlis.ch/INTERLIS2.3"
            VSIFClose( fp );
            CSLDestroy( filenames );
            return FALSE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      We assume now that it is ILI2.  Close and instantiate a          */
/*      ILI2Reader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );
    
    poReader = CreateILI2Reader();
    if( poReader == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File %s appears to be ILI2 but the ILI2 reader can't\n"
                  "be instantiated, likely because Xerces support wasn't\n"
                  "configured in.", 
                  pszNewName );
        CSLDestroy( filenames );
        return FALSE;
    }

    if (modelFilename)
        poReader->ReadModel( poImdReader, modelFilename );

    if( getenv( "ARC_DEGREES" ) != NULL ) {
      //No better way to pass arguments to the reader (it could even be an -lco arg)
      poReader->SetArcDegrees( atof( getenv("ARC_DEGREES") ) );
    }

    poReader->SetSourceFile( pszName );

    poReader->SaveClasses( pszName );

    listLayer = poReader->GetLayers();
    list<OGRLayer *>::const_iterator layerIt;
    for (layerIt = listLayer.begin(); layerIt != listLayer.end(); ++layerIt) {
        (*layerIt)->ResetReading();
    }

    CSLDestroy( filenames );

    return TRUE;
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRILI2DataSource::Create( const char *pszFilename, 
                              char **papszOptions )

{
    char **filenames = CSLTokenizeString2( pszFilename, ",", 0 );
    pszName = CPLStrdup(filenames[0]);
    const char  *pszModelFilename = (CSLCount(filenames)>1) ? filenames[1] : NULL;

    if( pszModelFilename == NULL )
    {
        CPLError( CE_Warning, CPLE_OpenFailed,
                  "Model file '%s' (%s) not found : %s.",
                  pszModelFilename, pszFilename, VSIStrerror( errno ) );
        CSLDestroy(filenames);
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */

    if( strcmp(pszName,"/vsistdout/") == 0 ||
        strncmp(pszName,"/vsigzip/", 9) == 0 )
    {
        fpOutput = VSIFOpenL(pszName, "wb");
    }
    else if ( strncmp(pszName,"/vsizip/", 8) == 0)
    {
        if (EQUAL(CPLGetExtension(pszName), "zip"))
        {
            CPLFree(pszName);
            pszName = CPLStrdup(CPLFormFilename(pszName, "out.xtf", NULL));
        }

        fpOutput = VSIFOpenL(pszName, "wb");
    }
    else
        fpOutput = VSIFOpenL( pszName, "wb+" );
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create XTF file %s.",
                  pszName );
        return FALSE;
    }


/* -------------------------------------------------------------------- */
/*      Parse model                                                     */
/* -------------------------------------------------------------------- */
    poImdReader->ReadModel(pszModelFilename);

/* -------------------------------------------------------------------- */
/*      Write headers                                                   */
/* -------------------------------------------------------------------- */
    VSIFPrintfL(fpOutput, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
    VSIFPrintfL(fpOutput, "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">\n");
    VSIFPrintfL(fpOutput, "<HEADERSECTION SENDER=\"OGR/GDAL %s\" VERSION=\"2.3\">\n", GDAL_RELEASE_NAME);
    VSIFPrintfL(fpOutput, "<MODELS>\n");
    for (IliModelInfos::const_iterator it = poImdReader->modelInfos.begin(); it != poImdReader->modelInfos.end(); ++it)
    {
        VSIFPrintfL(fpOutput, "<MODEL NAME=\"%s\" URI=\"%s\" VERSION=\"%s\"/>\n",
            it->name.c_str(), it->uri.c_str(), it->version.c_str());
    }
    VSIFPrintfL(fpOutput, "</MODELS>\n");
    VSIFPrintfL(fpOutput, "</HEADERSECTION>\n");
    VSIFPrintfL(fpOutput, "<DATASECTION>\n");
    const char* basketName = poImdReader->mainBasketName.c_str();
    VSIFPrintfL(fpOutput, "<%s BID=\"%s\">\n", basketName, basketName);

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRILI2DataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )

{
    if (fpOutput == NULL)
        return NULL;

    FeatureDefnInfo featureDefnInfo = poImdReader->GetFeatureDefnInfo(pszLayerName);
    OGRFeatureDefn* poFeatureDefn = featureDefnInfo.poTableDefn;
    if (poFeatureDefn == NULL)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Layer '%s' not found in model definition. Creating adhoc layer", pszLayerName);
        poFeatureDefn = new OGRFeatureDefn(pszLayerName);
        poFeatureDefn->SetGeomType( eType );
    }
    OGRILI2Layer *poLayer = new OGRILI2Layer(poFeatureDefn, featureDefnInfo.poGeomFieldInfos, this);

    nLayers++;
    papoLayers = (OGRILI2Layer**)CPLRealloc(papoLayers, sizeof(OGRILI2Layer*) * nLayers);
    papoLayers[nLayers-1] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI2DataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRILI2DataSource::GetLayer( int iLayer )

{
  list<OGRLayer *>::const_iterator layerIt = listLayer.begin();
  int i = 0;
  while (i < iLayer && layerIt != listLayer.end()) {
    i++;
    layerIt++;
  }
  
  if (i == iLayer) {
    OGRILI2Layer *tmpLayer = (OGRILI2Layer *)*layerIt;
    return tmpLayer;
  } else
    return NULL;
}
