/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1DataSource class.
 * Author:   Pirmin Kalberer, Sourcepole AG
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

#include "ogr_ili1.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili1reader.h"

#include "iomhelper.h"
#include "iom/iom.h"

#include <string>

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRILI1DataSource()                         */
/************************************************************************/

OGRILI1DataSource::OGRILI1DataSource()

{
    pszName = NULL;
    poReader = NULL;
    fpTransfer = NULL;
    pszTopic = NULL;
    nLayers = 0;
    papoLayers = NULL;
}

/************************************************************************/
/*                        ~OGRILI1DataSource()                         */
/************************************************************************/

OGRILI1DataSource::~OGRILI1DataSource()

{
    int i;

    for(i=0;i<nLayers;i++)
    {
        delete papoLayers[i];
    }
    CPLFree( papoLayers );

    CPLFree( pszName );
    CPLFree( pszTopic );
    DestroyILI1Reader( poReader );
    if( fpTransfer )
    {
        VSIFPrintf( fpTransfer, "ETAB\n" );
        VSIFPrintf( fpTransfer, "ETOP\n" );
        VSIFPrintf( fpTransfer, "EMOD\n" );
        VSIFPrintf( fpTransfer, "ENDE\n" );
        fclose(fpTransfer);
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRILI1DataSource::Open( const char * pszNewName, int bTestOpen )

{
    FILE        *fp;
    char        szHeader[1000];
    std::string osBasename, osModelFilename;

    if (strlen(pszNewName) == 0)
    {
        return FALSE;
    }

    char **filenames = CSLTokenizeString2( pszNewName, ",", 0 );

    osBasename = filenames[0];

    if( CSLCount(filenames) > 1 )
        osModelFilename = filenames[1];

    CSLDestroy( filenames );

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( osBasename.c_str(), "r" );
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
    if( bTestOpen )
    {
        int nLen = (int)VSIFRead( szHeader, 1, sizeof(szHeader), fp );
        if (nLen == sizeof(szHeader))
            szHeader[sizeof(szHeader)-1] = '\0';
        else
            szHeader[nLen] = '\0';

        if( strstr(szHeader,"SCNT") == NULL )
        {
            VSIFClose( fp );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      We assume now that it is ILI1.  Close and instantiate a          */
/*      ILI1Reader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );

    poReader = CreateILI1Reader();
    if( poReader == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s appears to be ILI1 but the ILI1 reader can't\n"
                  "be instantiated, likely because Xerces support wasn't\n"
                  "configured in.",
                  pszNewName );
        return FALSE;
     }

    poReader->OpenFile( osBasename.c_str() );

    pszName = CPLStrdup( osBasename.c_str() );

    if (osModelFilename.length() > 0 )
        poReader->ReadModel( osModelFilename.c_str() );

    if( getenv( "ARC_DEGREES" ) != NULL ) {
      //No better way to pass arguments to the reader (it could even be an -lco arg)
      poReader->SetArcDegrees( atof( getenv("ARC_DEGREES") ) );
    }

    //Parse model and read data - without surface joing and polygonizing
    poReader->ReadFeatures();

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRILI1DataSource::Create( const char *pszFilename,
                              char **papszOptions )

{
    std::string osBasename, osModelFilename;
    char **filenames = CSLTokenizeString2( pszFilename, ",", 0 );

    osBasename = filenames[0];

    if( CSLCount(filenames) > 1 )
        osModelFilename = filenames[1];

    CSLDestroy( filenames );

    if( osModelFilename.length() == 0 )
    {
      //TODO: create automatic model
    }

/* -------------------------------------------------------------------- */
/*      Create the empty file.                                          */
/* -------------------------------------------------------------------- */
    fpTransfer = VSIFOpen( osBasename.c_str(), "w+b" );

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
    iom_init();

    // set error listener to a iom provided one, that just
    // dumps all errors to stderr
    iom_seterrlistener(iom_stderrlistener);

    IOM_BASKET model = 0;
    if( osModelFilename.length() != 0 ) {
      // compile ili model
      char *iliFiles[1] = {(char *)osModelFilename.c_str()};
      model=iom_compileIli(1,iliFiles);
      if(!model){
        CPLError( CE_Warning, CPLE_OpenFailed,
                  "iom_compileIli %s, %s.",
                  pszName, VSIStrerror( errno ) );
        iom_end();
        return FALSE;
      }
    }

    pszTopic = CPLStrdup(model ?
                         GetAttrObjName(model, "iom04.metamodel.Topic") :
                         CPLGetBasename(osBasename.c_str()));

/* -------------------------------------------------------------------- */
/*      Write headers                                                   */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fpTransfer, "SCNT\n" );
    VSIFPrintf( fpTransfer, "OGR/GDAL %s, INTERLIS Driver\n", GDAL_RELEASE_NAME );
    VSIFPrintf( fpTransfer, "////\n" );
    VSIFPrintf( fpTransfer, "MTID INTERLIS1\n" );
    const char* modelname = model ?
                            GetAttrObjName(model, "iom04.metamodel.DataModel") :
                            CPLGetBasename(osBasename.c_str());
    VSIFPrintf( fpTransfer, "MODL %s\n", modelname );

    return TRUE;
}

static char *ExtractTopic(const char * pszLayerName)
{
  const char *table = strchr(pszLayerName, '_');
  while (table && table[1] !=  '_') table = strchr(table+1, '_');
  return (table) ? CPLScanString(pszLayerName, table-pszLayerName, FALSE, FALSE) : NULL;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRILI1DataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )

{
    const char *table = pszLayerName;
    char * topic = ExtractTopic(pszLayerName);
    if (nLayers) VSIFPrintf( fpTransfer, "ETAB\n" );
    if (topic)
    {
      table = pszLayerName+strlen(topic)+2; //after "__"
      if (pszTopic == NULL || !EQUAL(topic, pszTopic))
      {
        if (pszTopic)
        {
          VSIFPrintf( fpTransfer, "ETOP\n" );
          CPLFree(pszTopic);
        }
        pszTopic = topic;
        VSIFPrintf( fpTransfer, "TOPI %s\n", pszTopic );
      }
      else
      {
        CPLFree(topic);
      }
    }
    else
    {
      if (pszTopic == NULL) pszTopic = CPLStrdup("Unknown");
      VSIFPrintf( fpTransfer, "TOPI %s\n", pszTopic );
    }
    VSIFPrintf( fpTransfer, "TABL %s\n", table );

    OGRILI1Layer *poLayer = new OGRILI1Layer(table, poSRS, TRUE, eType, this);

    nLayers ++;
    papoLayers = (OGRILI1Layer**)CPLRealloc(papoLayers, sizeof(OGRILI1Layer*) * nLayers);
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
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRILI1DataSource::GetLayer( int iLayer )
{
  return poReader->GetLayer( iLayer );
}
