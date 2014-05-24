/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset (writable vector dataset)
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                      PDFWritableVectorDataset()                      */
/************************************************************************/

PDFWritableVectorDataset::PDFWritableVectorDataset()
{
    papszOptions = NULL;
    nLayers = 0;
    papoLayers = NULL;
    bModified = FALSE;
}

/************************************************************************/
/*                      ~PDFWritableVectorDataset()                     */
/************************************************************************/

PDFWritableVectorDataset::~PDFWritableVectorDataset()
{
    SyncToDisk();

    CSLDestroy(papszOptions);
    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset* PDFWritableVectorDataset::Create( const char * pszName,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions ) 
{
    if( nBands != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "PDFWritableVectorDataset::Create() can only be called with "
                 "nBands = 0 to create a vector-only PDF");
        return NULL;
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
                                char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRLayer* poLayer = new OGRPDFWritableLayer(this, pszLayerName, poSRS, eType);

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
        return NULL;

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

    double dfDPI = atof(CSLFetchNameValueDef(papszOptions, "DPI", "72"));
    if (dfDPI < 72.0)
        dfDPI = 72.0;

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
    int bWriteOGRAttributes = CSLFetchBoolean(papszOptions, "OGR_WRITE_ATTRIBUTES", TRUE);
    const char* pszOGRLinkField = CSLFetchNameValue(papszOptions, "OGR_LINK_FIELD");

    const char* pszOffLayers = CSLFetchNameValue(papszOptions, "OFF_LAYERS");
    const char* pszExclusiveLayers = CSLFetchNameValue(papszOptions, "EXCLUSIVE_LAYERS");

    const char* pszJavascript = CSLFetchNameValue(papszOptions, "JAVASCRIPT");
    const char* pszJavascriptFile = CSLFetchNameValue(papszOptions, "JAVASCRIPT_FILE");

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(GetDescription(), "wb");
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  GetDescription() );
        return OGRERR_FAILURE;
    }

    GDALPDFWriter oWriter(fp);

    double dfRatio = (sGlobalExtent.MaxY - sGlobalExtent.MinY) / (sGlobalExtent.MaxX - sGlobalExtent.MinX);

    int nWidth, nHeight;

    if (dfRatio < 1)
    {
        nWidth = 1024;
        nHeight = nWidth * dfRatio;
    }
    else
    {
        nHeight = 1024;
        nWidth = nHeight / dfRatio;
    }

    GDALDataset* poSrcDS = MEMDataset::Create( "MEM:::", nWidth, nHeight, 0, GDT_Byte, NULL );

    double adfGeoTransform[6];
    adfGeoTransform[0] = sGlobalExtent.MinX;
    adfGeoTransform[1] = (sGlobalExtent.MaxX - sGlobalExtent.MinX) / nWidth;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = sGlobalExtent.MaxY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = - (sGlobalExtent.MaxY - sGlobalExtent.MinY) / nHeight;

    poSrcDS->SetGeoTransform(adfGeoTransform);

    OGRSpatialReference* poSRS = papoLayers[0]->GetSpatialRef();
    if (poSRS)
    {
        char* pszWKT = NULL;
        poSRS->exportToWkt(&pszWKT);
        poSrcDS->SetProjection(pszWKT);
        CPLFree(pszWKT);
    }

    oWriter.SetInfo(poSrcDS, papszOptions);

    oWriter.StartPage(poSrcDS,
                      dfDPI,
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
