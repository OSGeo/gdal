/******************************************************************************
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset (writable vector dataset)
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_pdf.h"
#include "pdfcreatecopy.h"
#include "memdataset.h"
#include "pdfcreatefromcomposition.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      PDFWritableVectorDataset()                      */
/************************************************************************/

PDFWritableVectorDataset::PDFWritableVectorDataset() :
    papszOptions(nullptr),
    nLayers(0),
    papoLayers(nullptr),
    bModified(FALSE)
{}

/************************************************************************/
/*                      ~PDFWritableVectorDataset()                     */
/************************************************************************/

PDFWritableVectorDataset::~PDFWritableVectorDataset()
{
    PDFWritableVectorDataset::SyncToDisk();

    CSLDestroy(papszOptions);
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset* PDFWritableVectorDataset::Create( const char * pszName,
                                               int nXSize,
                                               int nYSize,
                                               int nBands,
                                               GDALDataType eType,
                                               char ** papszOptions )
{
    if( nBands == 0 && nXSize == 0 && nYSize == 0 && eType == GDT_Unknown )
    {
        const char* pszFilename = CSLFetchNameValue(papszOptions, "COMPOSITION_FILE");
        if( pszFilename )
        {
            if( CSLCount(papszOptions) != 1 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "All others options than COMPOSITION_FILE are ignored");
            }
            return GDALPDFCreateFromCompositionFile(pszName, pszFilename);
        }
    }

    if( nBands != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "PDFWritableVectorDataset::Create() can only be called with "
                 "nBands = 0 to create a vector-only PDF");
        return nullptr;
    }
    PDFWritableVectorDataset* poDataset = new PDFWritableVectorDataset();

    poDataset->SetDescription(pszName);
    poDataset->papszOptions = CSLDuplicate(papszOptions);

    return poDataset;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
PDFWritableVectorDataset::ICreateLayer( const char * pszLayerName,
                                        OGRSpatialReference *poSRS,
                                        OGRwkbGeometryType eType,
                                        char ** )
{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    auto poSRSClone = poSRS;
    if( poSRSClone )
    {
        poSRSClone = poSRSClone->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    OGRLayer* poLayer = new OGRPDFWritableLayer(this, pszLayerName, poSRSClone, eType);
    if( poSRSClone )
        poSRSClone->Release();

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int PDFWritableVectorDataset::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *PDFWritableVectorDataset::GetLayer( int iLayer )

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int PDFWritableVectorDataset::GetLayerCount()
{
    return nLayers;
}

/************************************************************************/
/*                            SyncToDisk()                              */
/************************************************************************/

OGRErr PDFWritableVectorDataset::SyncToDisk()
{
    if (nLayers == 0 || !bModified)
        return OGRERR_NONE;

    bModified = FALSE;

    OGREnvelope sGlobalExtent;
    int bHasExtent = FALSE;
    for(int i=0;i<nLayers;i++)
    {
        OGREnvelope sExtent;
        if (papoLayers[i]->GetExtent(&sExtent) == OGRERR_NONE)
        {
            bHasExtent = TRUE;
            sGlobalExtent.Merge(sExtent);
        }
    }
    if (!bHasExtent ||
        sGlobalExtent.MinX == sGlobalExtent.MaxX ||
        sGlobalExtent.MinY == sGlobalExtent.MaxY)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot compute spatial extent of features");
        return OGRERR_FAILURE;
    }

    double dfRatio = (sGlobalExtent.MaxY - sGlobalExtent.MinY) / (sGlobalExtent.MaxX - sGlobalExtent.MinX);

    int nWidth, nHeight;

    if (dfRatio < 1.0)
    {
        nWidth = 1024;
        const double dfHeight = nWidth * dfRatio;
        if( dfHeight < 1 || dfHeight > INT_MAX || CPLIsNan(dfHeight) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid image dimensions");
            return OGRERR_FAILURE;
        }
        nHeight = static_cast<int>(dfHeight);
    }
    else
    {
        nHeight = 1024;
        const double dfWidth = nHeight / dfRatio;
        if( dfWidth < 1 || dfWidth > INT_MAX || CPLIsNan(dfWidth) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid image dimensions");
            return OGRERR_FAILURE;
        }
        nWidth = static_cast<int>(dfWidth);
    }

    double adfGeoTransform[6];
    adfGeoTransform[0] = sGlobalExtent.MinX;
    adfGeoTransform[1] = (sGlobalExtent.MaxX - sGlobalExtent.MinX) / nWidth;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = sGlobalExtent.MaxY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = - (sGlobalExtent.MaxY - sGlobalExtent.MinY) / nHeight;

    // Do again a check against 0, because the above divisions might
    // transform a difference close to 0, to plain 0.
    if (adfGeoTransform[1] == 0 || adfGeoTransform[5] == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot compute spatial extent of features");
        return OGRERR_FAILURE;
    }

    PDFCompressMethod eStreamCompressMethod = COMPRESS_DEFLATE;
    const char* pszStreamCompressMethod = CSLFetchNameValue(papszOptions, "STREAM_COMPRESS");
    if (pszStreamCompressMethod)
    {
        if( EQUAL(pszStreamCompressMethod, "NONE") )
            eStreamCompressMethod = COMPRESS_NONE;
        else if( EQUAL(pszStreamCompressMethod, "DEFLATE") )
            eStreamCompressMethod = COMPRESS_DEFLATE;
        else
        {
            CPLError( CE_Warning, CPLE_NotSupported,
                    "Unsupported value for STREAM_COMPRESS.");
        }
    }

    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    const char* pszDPI = CSLFetchNameValue(papszOptions, "DPI");
    double dfDPI = DEFAULT_DPI;
    if( pszDPI != nullptr )
    {
        dfDPI = CPLAtof(pszDPI);
        if (dfDPI < DEFAULT_DPI)
            dfDPI = DEFAULT_DPI;
    }
    else
    {
        dfDPI = DEFAULT_DPI;
    }

    const char* pszWriteUserUnit = CSLFetchNameValue(papszOptions, "WRITE_USERUNIT");
    bool bWriteUserUnit;
    if( pszWriteUserUnit != nullptr )
        bWriteUserUnit = CPLTestBool( pszWriteUserUnit );
    else
        bWriteUserUnit = ( pszDPI == nullptr );

    const char* pszNEATLINE = CSLFetchNameValue(papszOptions, "NEATLINE");

    int nMargin = atoi(CSLFetchNameValueDef(papszOptions, "MARGIN", "0"));

    PDFMargins sMargins;
    sMargins.nLeft = nMargin;
    sMargins.nRight = nMargin;
    sMargins.nTop = nMargin;
    sMargins.nBottom = nMargin;

    const char* pszLeftMargin = CSLFetchNameValue(papszOptions, "LEFT_MARGIN");
    if (pszLeftMargin) sMargins.nLeft = atoi(pszLeftMargin);

    const char* pszRightMargin = CSLFetchNameValue(papszOptions, "RIGHT_MARGIN");
    if (pszRightMargin) sMargins.nRight = atoi(pszRightMargin);

    const char* pszTopMargin = CSLFetchNameValue(papszOptions, "TOP_MARGIN");
    if (pszTopMargin) sMargins.nTop = atoi(pszTopMargin);

    const char* pszBottomMargin = CSLFetchNameValue(papszOptions, "BOTTOM_MARGIN");
    if (pszBottomMargin) sMargins.nBottom = atoi(pszBottomMargin);

    const char* pszExtraImages = CSLFetchNameValue(papszOptions, "EXTRA_IMAGES");
    const char* pszExtraStream = CSLFetchNameValue(papszOptions, "EXTRA_STREAM");
    const char* pszExtraLayerName = CSLFetchNameValue(papszOptions, "EXTRA_LAYER_NAME");

    const char* pszOGRDisplayField = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_FIELD");
    const char* pszOGRDisplayLayerNames = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_LAYER_NAMES");
    const bool bWriteOGRAttributes =
        CPLFetchBool(papszOptions, "OGR_WRITE_ATTRIBUTES", true);
    const char* pszOGRLinkField = CSLFetchNameValue(papszOptions, "OGR_LINK_FIELD");

    const char* pszOffLayers = CSLFetchNameValue(papszOptions, "OFF_LAYERS");
    const char* pszExclusiveLayers = CSLFetchNameValue(papszOptions, "EXCLUSIVE_LAYERS");

    const char* pszJavascript = CSLFetchNameValue(papszOptions, "JAVASCRIPT");
    const char* pszJavascriptFile = CSLFetchNameValue(papszOptions, "JAVASCRIPT_FILE");

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(GetDescription(), "wb");
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  GetDescription() );
        return OGRERR_FAILURE;
    }

    GDALPDFWriter oWriter(fp);

    GDALDataset* poSrcDS = MEMDataset::Create( "MEM:::", nWidth, nHeight, 0, GDT_Byte, nullptr );

    poSrcDS->SetGeoTransform(adfGeoTransform);

    OGRSpatialReference* poSRS = papoLayers[0]->GetSpatialRef();
    if (poSRS)
    {
        char* pszWKT = nullptr;
        poSRS->exportToWkt(&pszWKT);
        poSrcDS->SetProjection(pszWKT);
        CPLFree(pszWKT);
    }

    oWriter.SetInfo(poSrcDS, papszOptions);

    oWriter.StartPage(poSrcDS,
                      dfDPI,
                      bWriteUserUnit,
                      pszGEO_ENCODING,
                      pszNEATLINE,
                      &sMargins,
                      eStreamCompressMethod,
                      bWriteOGRAttributes);

    int iObj = 0;

    char** papszLayerNames = CSLTokenizeString2(pszOGRDisplayLayerNames,",",0);

    for(int i=0;i<nLayers;i++)
    {
        CPLString osLayerName;
        if (CSLCount(papszLayerNames) < nLayers)
            osLayerName = papoLayers[i]->GetName();
        else
            osLayerName = papszLayerNames[i];

        oWriter.WriteOGRLayer((OGRDataSourceH)this,
                              i,
                              pszOGRDisplayField,
                              pszOGRLinkField,
                              osLayerName,
                              bWriteOGRAttributes,
                              iObj);
    }

    CSLDestroy(papszLayerNames);

    oWriter.EndPage(pszExtraImages,
                    pszExtraStream,
                    pszExtraLayerName,
                    pszOffLayers,
                    pszExclusiveLayers);

    if (pszJavascript)
        oWriter.WriteJavascript(pszJavascript);
    else if (pszJavascriptFile)
        oWriter.WriteJavascriptFile(pszJavascriptFile);

    oWriter.Close();

    delete poSrcDS;

    return OGRERR_NONE;
}
