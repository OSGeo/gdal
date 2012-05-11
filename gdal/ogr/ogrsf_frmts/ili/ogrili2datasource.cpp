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
#include "iomhelper.h"

using namespace std;


CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRILI2DataSource()                         */
/************************************************************************/

OGRILI2DataSource::OGRILI2DataSource()

{
    pszName = NULL;
    poReader = NULL;
    fpTransfer = NULL;
    basket = NULL;
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

    if (basket) iom_releasebasket(basket);
    if (fpTransfer)
    {  
      // write file
      iom_save(fpTransfer);
  
      // clean up
      iom_close(fpTransfer);
  
      iom_end();
  
    }
    DestroyILI2Reader( poReader );
    CPLFree( pszName );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRILI2DataSource::Open( const char * pszNewName, int bTestOpen )

{
    FILE        *fp;
    char        szHeader[1000];

    char **modelFilenames = NULL;
    char **filenames = CSLTokenizeString2( pszNewName, ",", 0 );

    pszName = CPLStrdup( filenames[0] );

    if( CSLCount(filenames) > 1 )
        modelFilenames = &filenames[1];

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
        { // "www.interlis.ch/INTERLIS2.2"
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

    if (modelFilenames)
        poReader->ReadModel( modelFilenames );

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

    iom_init();

    // set error listener to a iom provided one, that just 
    // dumps all errors to stderr
    iom_seterrlistener(iom_stderrlistener);

    // compile ili model
    char *iliFiles[1] = {(char *)pszModelFilename};
    IOM_BASKET model=iom_compileIli(1,iliFiles);
    if(!model){
        CPLError( CE_Warning, CPLE_OpenFailed, 
                    "iom_compileIli %s, %s.", 
                    pszName, VSIStrerror( errno ) );
                iom_end();
        CSLDestroy(filenames);
        return FALSE;
    }

    // open new file
    fpTransfer=iom_open(pszName,IOM_CREATE | IOM_DONTREAD,0);
    if(!fpTransfer){
        CPLError( CE_Warning, CPLE_OpenFailed, 
                    "Failed to open %s.", 
                    pszName );
        CSLDestroy(filenames);
        return FALSE;
    }

    // set model of new file
    iom_setmodel(fpTransfer,model);

    iom_setheadsender(fpTransfer, pszModelFilename);

    iom_setheadcomment(fpTransfer,"Created by OGR");

    // create new basket
    static char basketname[512];
    basketname[0] = '\0';
    const char* val = GetAttrObjName(model, "iom04.metamodel.DataModel");
    if (val)
    {
      strcat(basketname, val);
      strcat(basketname, ".");
      val = GetAttrObjName(model, "iom04.metamodel.Topic");
      if (val) strcat(basketname, val);
    }
    else
    {
      strcat(basketname, "Basket");
    }

    CSLDestroy(filenames);

    basket=iom_newbasket(fpTransfer);
    iom_setbaskettag(basket, basketname);
    iom_setbasketoid(basket, "0");
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
    OGRILI2Layer *poLayer = new OGRILI2Layer(pszLayerName, poSRS, TRUE, eType, this);

    nLayers ++;
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
