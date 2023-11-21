/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonDataSource class.
 * Author:   Abel Pau, a.pau@creaf.uab.cat
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogrmiramon.h"

/************************************************************************/
/*                          OGRMiraMonDataSource()                          */
/************************************************************************/

OGRMiraMonDataSource::OGRMiraMonDataSource()
    : papoLayers(nullptr), nLayers(0), pszName(nullptr), bUpdate(false)
{
}

/************************************************************************/
/*                         ~OGRMiraMonDataSource()                          */
/************************************************************************/

OGRMiraMonDataSource::~OGRMiraMonDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    CPLFree(pszName);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRMiraMonDataSource::Open(const char *pszFilename, VSILFILE *fp,
                           const OGRSpatialReference *poSRS, int bUpdateIn)

{
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    OGRMiraMonLayer *poLayer = new OGRMiraMonLayer(pszFilename, fp, poSRS, bUpdate);
    if (!poLayer->bValidFile)
    {
        delete poLayer;
        return FALSE;
    }

    papoLayers = static_cast<OGRMiraMonLayer **>(
        CPLRealloc(papoLayers, (size_t)(sizeof(OGRMiraMonLayer *) * ((size_t)nLayers + (size_t)1))));
    papoLayers[nLayers] = poLayer;
    nLayers++;

    CPLFree(pszName);
    pszName = CPLStrdup(pszFilename);

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new datasource.  This does not really do anything      */
/*      currently but save the name.                                    */
/************************************************************************/

int OGRMiraMonDataSource::Create(const char *pszDSName, char ** /* papszOptions */)

{
    pszName = CPLStrdup(pszDSName);

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRMiraMonDataSource::ICreateLayer(const char *pszLayerName,
                                         OGRSpatialReference *poSRS,
                                         OGRwkbGeometryType eType,
                                         char **papszOptions)
{
    CPLAssert(nullptr != pszLayerName);

    const char *pszExtension= "pol";

    /* -------------------------------------------------------------------- */
    /*      Establish the geometry type.  Note this logic                   */
    /* -------------------------------------------------------------------- */
    int layerType= 0;
    const char *pszVersion =
        CSLFetchNameValue(papszOptions, "Version");
    int nMMVersion;

    if(pszVersion && EQUAL(pszVersion, "V11"))
        nMMVersion=MM_32BITS_VERSION;
    else //if(EQUAL(pszVersion, "V20") ||
        //EQUAL(pszVersion, "last_version"))
        nMMVersion=MM_64BITS_VERSION;

    switch (eType)
    {
        case wkbPoint:
		case wkbMultiPoint:
            layerType=MM_LayerType_Point;
            pszExtension = "pnt";
			break;
        case wkbPoint25D:
			layerType=MM_LayerType_Point3d;
            pszExtension = "pnt";
			break;
		case wkbLineString:
		case wkbMultiLineString:
			layerType=MM_LayerType_Arc;
            pszExtension = "arc";
			break;
        case wkbLineString25D:
			layerType=MM_LayerType_Arc3d;
            pszExtension = "arc";
			break;
		case wkbPolygon:
		case wkbMultiPolygon:
            layerType=MM_LayerType_Pol;
            pszExtension = "pol";
			break;
        case wkbPolygon25D:
        case wkbMultiPolygon25D:
            layerType=MM_LayerType_Pol3d;
            pszExtension = "pol";
			break;
        case wkbGeometryCollection:
		case wkbUnknown:
		default:
            layerType=MM_LayerType_Unknown;
			break;
    }

    /* -------------------------------------------------------------------- */
    /*      We will override the provided layer name                        */
    /*      with the name from the file with the appropiate extension       */
    /* -------------------------------------------------------------------- */

    CPLString osPath = CPLGetPath(pszName);
    CPLString osFilename(pszName);
    const char *pszFlags = "wb+";

    if (osFilename == "/dev/stdout")
        osFilename = "/vsistdout";

    if (STARTS_WITH(osFilename, "/vsistdout"))
        pszFlags = "wb";
    else if(layerType!=MM_LayerType_Unknown)
    {
        // Extension is determined only for the type of the layer
        osFilename = CPLFormFilename(osPath, pszLayerName, pszExtension);
    }
    else
    {
        // If type undefined, no extensions is added until type is defined
        osFilename = CPLFormFilename(osPath, pszLayerName, "~mm");
    }

    /* -------------------------------------------------------------------- */
    /*      Open the file.                                                  */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(osFilename, pszFlags);
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "open(%s) failed: %s",
                 osFilename.c_str(), VSIStrerror(errno));
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out header.                                               */
    /* -------------------------------------------------------------------- */
    if(MMWriteEmptyHeader(fp, layerType, nMMVersion))
        return nullptr;

    // Revisar
    /* -------------------------------------------------------------------- */
    /*      Write the projection, if possible.                              */
    /* -------------------------------------------------------------------- */
    /*if (poSRS != nullptr)
    {
        hMiraMonLayer.ePlainLT=layerType;

        if (poSRS->GetAuthorityName(nullptr) &&
                EQUAL(poSRS->GetAuthorityName(nullptr), "EPSG"))
            hMiraMonLayer.pSRS=CPLStrdup(poSRS->GetAuthorityCode(nullptr));

        if (MMWriteVectorMetadata(&hMiraMonLayer))
        {
            CPLFree(hMiraMonLayer.pSRS);
            return nullptr;
        }
        CPLFree(hMiraMonLayer.pSRS);
    }*/

    /* -------------------------------------------------------------------- */
    /*      Return open layer handle.                                       */
    /* -------------------------------------------------------------------- */
    if (Open(osFilename, fp, poSRS, TRUE))
    {
        auto poLayer = papoLayers[nLayers - 1];
        if (layerType==MM_LayerType_Unknown)
        {
            poLayer->GetLayerDefn()->SetGeomType(wkbFlatten(eType));
        }
        return poLayer;
    }

    VSIFCloseL(fp);
    return nullptr;

}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMiraMonDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return TRUE; 

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMiraMonDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}


