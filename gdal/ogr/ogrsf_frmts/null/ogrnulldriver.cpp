/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  NULL output driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

/* NOTE: this driver is only usefull for debugging and is not included in the build process */
/* To compile it as a pluing under Linux :
    g++ -Wall -DDEBUG -fPIC -g ogr/ogrsf_frmts/null/ogrnulldriver.cpp  -shared -o ogr_NULL.so -L. -lgdal -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
*/

#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

extern "C" void CPL_DLL RegisterOGRNULL();

/************************************************************************/
/*                           OGRNULLLayer                               */
/************************************************************************/

class OGRNULLLayer : public OGRLayer
{
    OGRFeatureDefn      *poFeatureDefn;
    OGRSpatialReference *poSRS;

  public:
                        OGRNULLLayer( const char *pszLayerName,
                                      OGRSpatialReference *poSRS,
                                      OGRwkbGeometryType eType );
    virtual             ~OGRNULLLayer();

    virtual OGRFeatureDefn *GetLayerDefn() {return poFeatureDefn;}
    virtual OGRSpatialReference * GetSpatialRef() { return poSRS; }

    virtual void        ResetReading() {}
    virtual int         TestCapability( const char * );

    virtual OGRFeature *GetNextFeature() { return NULL; }

    virtual OGRErr      CreateFeature( OGRFeature *poFeature ) { return OGRERR_NONE; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
};

/************************************************************************/
/*                        OGRNULLDataSource                             */
/************************************************************************/

class OGRNULLDataSource : public OGRDataSource
{
    int                 nLayers;
    OGRLayer**          papoLayers;
    char*               pszName;

  public:
                        OGRNULLDataSource(const char* pszNameIn);
                        ~OGRNULLDataSource();

    virtual const char *GetName() { return pszName; }
    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer   *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *pszLayerName,
                                      OGRSpatialReference *poSRS,
                                      OGRwkbGeometryType eType,
                                      char **papszOptions );

    virtual int         TestCapability( const char * );

};

/************************************************************************/
/*                            OGRNULLDriver                             */
/************************************************************************/

class OGRNULLDriver : public OGRSFDriver
{
  public:
                ~OGRNULLDriver() {};

    virtual const char    *GetName() { return "NULL"; }
    virtual OGRDataSource *Open( const char *, int ) { return NULL; }
    virtual OGRDataSource *CreateDataSource( const char * pszName,
                                             char **papszOptions );

    virtual int            TestCapability( const char * );
};

/************************************************************************/
/*                            OGRNULLLayer()                            */
/************************************************************************/

OGRNULLLayer::OGRNULLLayer( const char *pszLayerName,
                            OGRSpatialReference *poSRSIn,
                            OGRwkbGeometryType eType )
{
    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->SetGeomType(eType);
    poFeatureDefn->Reference();

    poSRS = poSRSIn ? poSRSIn : NULL;
    if (poSRS)
        poSRS->Reference();
}

/************************************************************************/
/*                            ~OGRNULLLayer()                           */
/************************************************************************/

OGRNULLLayer::~OGRNULLLayer()
{
    poFeatureDefn->Release();

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNULLLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCSequentialWrite) )
        return TRUE;
    if( EQUAL(pszCap, OLCCreateField) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                             CreateField()                           */
/************************************************************************/

OGRErr OGRNULLLayer::CreateField( OGRFieldDefn *poField,
                                  int bApproxOK )
{
    poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                          OGRNULLDataSource()                         */
/************************************************************************/

OGRNULLDataSource::OGRNULLDataSource(const char* pszNameIn)
{
    pszName = CPLStrdup(pszNameIn);
    nLayers = 0;
    papoLayers = NULL;
}

/************************************************************************/
/*                         ~OGRNULLDataSource()                         */
/************************************************************************/

OGRNULLDataSource::~OGRNULLDataSource()
{
    int i;
    for(i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree(papoLayers);

    CPLFree(pszName);
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer    *OGRNULLDataSource::ICreateLayer( const char *pszLayerName,
                                             OGRSpatialReference *poSRS,
                                             OGRwkbGeometryType eType,
                                             char **papszOptions )
{
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, sizeof(OGRLayer*) * (nLayers + 1));
    papoLayers[nLayers] = new OGRNULLLayer(pszLayerName, poSRS, eType);
    nLayers ++;
    return papoLayers[nLayers-1];
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNULLDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRNULLDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRNULLDriver::CreateDataSource( const char * pszName,
                                                char **papszOptions )
{
    return new OGRNULLDataSource(pszName);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNULLDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODrCCreateDataSource) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                        RegisterOGRNULL()                             */
/************************************************************************/

void RegisterOGRNULL()
{
    if (! GDAL_CHECK_VERSION("OGR/NULL driver"))
        return;

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRNULLDriver );
}
