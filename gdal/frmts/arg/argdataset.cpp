/******************************************************************************
 *
 * Project:  Azavea Raster Grid format driver.
 * Purpose:  Implements support for reading and writing Azavea Raster Grid
 *           format.
 * Author:   David Zwarg <dzwarg@azavea.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, David Zwarg <dzwarg@azavea.com>
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include "ogrgeojsonreader.h"
#include <limits>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              ARGDataset                              */
/* ==================================================================== */
/************************************************************************/

class ARGDataset final: public RawDataset
{
    VSILFILE *fpImage;  // image data file.
    double adfGeoTransform[6];
    char *pszFilename;

  public:
    ARGDataset();
    ~ARGDataset() override;

    CPLErr GetGeoTransform( double * padfTransform ) override;

    static int Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char *, GDALDataset *, int,
                                    char **, GDALProgressFunc, void *);
    char **GetFileList(void) override;
};

/************************************************************************/
/*                            ARGDataset()                              */
/************************************************************************/

ARGDataset::ARGDataset() :
    fpImage(nullptr),
    pszFilename(nullptr)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ARGDataset()                             */
/************************************************************************/

ARGDataset::~ARGDataset()

{
    CPLFree(pszFilename);

    FlushCache(true);
    if( fpImage != nullptr )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ARGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                         GetJsonFilename()                            */
/************************************************************************/
static CPLString GetJsonFilename(CPLString pszFilename)
{
    return CPLSPrintf( "%s/%s.json", CPLGetDirname(pszFilename),
                       CPLGetBasename(pszFilename) );
}

/************************************************************************/
/*                           GetJsonObject()                            */
/************************************************************************/
static json_object * GetJsonObject(CPLString pszFilename)
{
    CPLString osJSONFilename = GetJsonFilename(pszFilename);

    json_object *pJSONObject =
        json_object_from_file(const_cast<char *>(osJSONFilename.c_str()));
    if (pJSONObject == nullptr) {
        CPLDebug("ARGDataset", "GetJsonObject(): Could not parse JSON file.");
        return nullptr;
    }

    return pJSONObject;
}

/************************************************************************/
/*                          GetJsonValueStr()                           */
/************************************************************************/
static const char *GetJsonValueStr( json_object * pJSONObject,
                                    CPLString pszKey )
{
    json_object *pJSONItem =
        CPL_json_object_object_get(pJSONObject, pszKey.c_str());
    if( pJSONItem == nullptr )
    {
        CPLDebug("ARGDataset", "GetJsonValueStr(): "
                 "Could not find '%s' in JSON.", pszKey.c_str());
        return nullptr;
    }

    return json_object_get_string(pJSONItem);
}

/************************************************************************/
/*                          GetJsonValueDbl()                           */
/************************************************************************/
static double GetJsonValueDbl( json_object * pJSONObject, CPLString pszKey )
{
    const char *pszJSONStr = GetJsonValueStr(pJSONObject, pszKey.c_str());
    if (pszJSONStr == nullptr) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    char *pszTmp = const_cast<char *>(pszJSONStr);
    double dfTmp = CPLStrtod(pszJSONStr, &pszTmp);
    if (pszTmp == pszJSONStr) {
        CPLDebug("ARGDataset", "GetJsonValueDbl(): "
                 "Key value is not a numeric value: %s:%s",
                 pszKey.c_str(), pszTmp);
        return std::numeric_limits<double>::quiet_NaN();
    }

    return dfTmp;
}

/************************************************************************/
/*                           GetJsonValueInt()                          */
/************************************************************************/
static int GetJsonValueInt( json_object *pJSONObject, CPLString pszKey )
{
    const double dfTmp = GetJsonValueDbl(pJSONObject, pszKey.c_str());
    if (CPLIsNan(dfTmp)) {
        return -1;
    }

    return static_cast<int>(dfTmp);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/
char **ARGDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();
    CPLString osJSONFilename = GetJsonFilename(pszFilename);

    papszFileList = CSLAddString( papszFileList, osJSONFilename );

    return papszFileList;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ARGDataset::Identify( GDALOpenInfo *poOpenInfo )
{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "arg")) {
        return FALSE;
    }
#endif

    json_object *pJSONObject = GetJsonObject(poOpenInfo->pszFilename);
    if (pJSONObject == nullptr) {
        return FALSE;
    }

    json_object_put(pJSONObject);
    pJSONObject = nullptr;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *ARGDataset::Open( GDALOpenInfo *poOpenInfo )
{
    if ( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The ARG driver does not support update access to existing"
                  " datasets." );
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Check metadata settings in JSON.                                */
/* -------------------------------------------------------------------- */

    json_object *pJSONObject = GetJsonObject(poOpenInfo->pszFilename);

    if (pJSONObject == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "Error parsing JSON.");
        return nullptr;
    }

    // get the type (always 'arg')
    const char *pszJSONStr = GetJsonValueStr(pJSONObject, "type");
    if (pszJSONStr == nullptr ) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'type' is missing from the JSON file.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }
    else if (!EQUAL(pszJSONStr, "arg")) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'type' is not recognized: '%s'.", pszJSONStr);
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    double dfNoDataValue;
    GDALDataType eType;
    int nPixelOffset;

    // get the datatype
    pszJSONStr = GetJsonValueStr(pJSONObject, "datatype");
    if (pszJSONStr == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'datatype' is missing from the JSON file.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }
    else if (EQUAL(pszJSONStr, "int8")) {
        CPLDebug("ARGDataset", "Open(): "
            "int8 data is not supported in GDAL -- mapped to uint8");
        eType = GDT_Byte;
        nPixelOffset = 1;
        dfNoDataValue = 128;
    }
    else if (EQUAL(pszJSONStr, "int16")) {
        eType = GDT_Int16;
        nPixelOffset = 2;
        dfNoDataValue = -32767;
    }
    else if (EQUAL(pszJSONStr, "int32")) {
        eType = GDT_Int32;
        nPixelOffset = 4;
        dfNoDataValue = -2e31;
    }
    else if (EQUAL(pszJSONStr, "uint8")) {
        eType = GDT_Byte;
        nPixelOffset = 1;
        dfNoDataValue = 255;
    }
    else if (EQUAL(pszJSONStr, "uint16")) {
        eType = GDT_UInt16;
        nPixelOffset = 2;
        dfNoDataValue = 65535;
    }
    else if (EQUAL(pszJSONStr, "uint32")) {
        eType = GDT_UInt32;
        nPixelOffset = 4;
        dfNoDataValue = -2e31;
    }
    else if (EQUAL(pszJSONStr, "float32")) {
        eType = GDT_Float32;
        nPixelOffset = 4;
        dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
    }
    else if (EQUAL(pszJSONStr, "float64")) {
        eType = GDT_Float64;
        nPixelOffset = 8;
        dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
    }
    else {
        if (EQUAL(pszJSONStr, "int64") ||
            EQUAL(pszJSONStr, "uint64")) {
            CPLError(CE_Failure, CPLE_AppDefined,
                "The ARG 'datatype' is unsupported in GDAL: '%s'.", pszJSONStr);
        }
        else {
            CPLError(CE_Failure, CPLE_AppDefined,
                "The ARG 'datatype' is unknown: '%s'.", pszJSONStr);
        }
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the xmin of the bounding box
    const double dfXmin = GetJsonValueDbl(pJSONObject, "xmin");
    if (CPLIsNan(dfXmin)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'xmin' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the ymin of the bounding box
    const double dfYmin = GetJsonValueDbl(pJSONObject, "ymin");
    if (CPLIsNan(dfYmin)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'ymin' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the xmax of the bounding boxfpL
    const double dfXmax = GetJsonValueDbl(pJSONObject, "xmax");
    if (CPLIsNan(dfXmax)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'xmax' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the ymax of the bounding box
    const double dfYmax = GetJsonValueDbl(pJSONObject, "ymax");
    if (CPLIsNan(dfYmax)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'ymax' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the cell width
    const double dfCellwidth = GetJsonValueDbl(pJSONObject, "cellwidth");
    if (CPLIsNan(dfCellwidth)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'cellwidth' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the cell height
    const double dfCellheight = GetJsonValueDbl(pJSONObject, "cellheight");
    if (CPLIsNan(dfCellheight)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'cellheight' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    double dfXSkew = GetJsonValueDbl(pJSONObject, "xskew");
    if (CPLIsNan(dfXSkew)) {
        // not an error -- default to 0.0
        dfXSkew = 0.0f;
    }

    double dfYSkew = GetJsonValueDbl(pJSONObject, "yskew");
    if (CPLIsNan(dfYSkew)) {
        // not an error -- default to 0.0
        dfYSkew = 0.0f;
    }

    // get the rows
    const int nRows = GetJsonValueInt(pJSONObject, "rows");
    if (nRows < 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'rows' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    // get the columns
    const int nCols = GetJsonValueInt(pJSONObject, "cols");
    if (nCols < 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'cols' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        return nullptr;
    }

    int nSrs = GetJsonValueInt(pJSONObject, "epsg");
    if (nSrs < 0) {
        // not an error -- default to web mercator
        nSrs = 3857;
    }

    OGRSpatialReference oSRS;
    OGRErr nErr = oSRS.importFromEPSG(nSrs);
    if (nErr != OGRERR_NONE) {
        nErr = oSRS.importFromEPSG(3857);

        if (nErr == OGRERR_NONE) {
            CPLDebug("ARGDataset", "Open(): "
                     "The EPSG provided did not import cleanly. "
                     "Defaulting to EPSG:3857");
        }
        else {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "The 'epsg' value did not translate to a known "
                      "spatial reference. "
                      "Please check the 'epsg' value and try again.");

            json_object_put(pJSONObject);
            pJSONObject = nullptr;

            return nullptr;
        }
    }

    char *pszWKT = nullptr;
    nErr = oSRS.exportToWkt(&pszWKT);
    if (nErr != OGRERR_NONE) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The spatial reference is known, but could not be set on the "
            "dataset. Please check the 'epsg' value and try again.");

        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        CPLFree(pszWKT);
        return nullptr;
    }

    // get the layer (always the file basename)
    pszJSONStr = GetJsonValueStr(pJSONObject, "layer");
    if (pszJSONStr == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'layer' is missing from the JSON file.");
        json_object_put(pJSONObject);
        pJSONObject = nullptr;
        CPLFree(pszWKT);
        return nullptr;
    }

    char *pszLayer = CPLStrdup(pszJSONStr);

    // done with the json object now
    json_object_put(pJSONObject);
    pJSONObject = nullptr;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ARGDataset *poDS = new ARGDataset();

    poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);
    poDS->SetMetadataItem("LAYER",pszLayer,nullptr);
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->SetProjection( pszWKT );

    // done with the projection string
    CPLFree(pszWKT);
    CPLFree(pszLayer);

/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    poDS->adfGeoTransform[0] = dfXmin;
    poDS->adfGeoTransform[1] = dfCellwidth;
    poDS->adfGeoTransform[2] = dfXSkew;
    poDS->adfGeoTransform[3] = dfYmax;
    poDS->adfGeoTransform[4] = dfYSkew;
    poDS->adfGeoTransform[5] = -dfCellheight;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
#ifdef CPL_LSB
    bool bNative = false;
#else
    bool bNative = true;
#endif

    RawRasterBand *poBand
        = new RawRasterBand( poDS, 1, poDS->fpImage,
                             0, nPixelOffset, nPixelOffset * nCols,
                             eType, bNative, RawRasterBand::OwnFP::NO );
    poDS->SetBand( 1, poBand );

    poBand->SetNoDataValue( dfNoDataValue );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/
GDALDataset *ARGDataset::CreateCopy( const char *pszFilename,
                                     GDALDataset *poSrcDS,
                                     int /* bStrict */ ,
                                     char ** /* papszOptions */ ,
                                     GDALProgressFunc /* pfnProgress */ ,
                                     void * /*pProgressData */ )
{
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
              "ARG driver doesn't support %d bands.  Must be 1 band.", nBands );
        return nullptr;
    }

    CPLString pszDataType;
    int nPixelOffset = 0;

    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    if( eType == GDT_Unknown ||
        eType == GDT_CInt16 ||
        eType == GDT_CInt32 ||
        eType == GDT_CFloat32 ||
        eType == GDT_CFloat64 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ARG driver doesn't support data type %s.",
                  GDALGetDataTypeName(eType) );
        return nullptr;
    }
    else if (eType == GDT_Int16) {
        pszDataType = "int16";
        nPixelOffset = 2;
    }
    else if (eType == GDT_Int32) {
        pszDataType = "int32";
        nPixelOffset = 4;
    }
    else if (eType == GDT_Byte) {
        pszDataType = "uint8";
        nPixelOffset = 1;
    }
    else if (eType == GDT_UInt16) {
        pszDataType = "uint16";
        nPixelOffset = 2;
    }
    else if (eType == GDT_UInt32) {
        pszDataType = "uint32";
        nPixelOffset = 4;
    }
    else if (eType == GDT_Float32) {
        pszDataType = "float32";
        nPixelOffset = 4;
    }
    else if (eType == GDT_Float64) {
        pszDataType = "float64";
        nPixelOffset = 8;
    }

    double adfTransform[6];
    poSrcDS->GetGeoTransform( adfTransform );

    const char *pszWKT = poSrcDS->GetProjectionRef();
    OGRSpatialReference oSRS;
    OGRErr nErr = oSRS.importFromWkt(pszWKT);
    if (nErr != OGRERR_NONE) {
        CPLError( CE_Failure, CPLE_NotSupported,
              "Cannot import spatial reference WKT from source dataset.");
        return nullptr;
    }

    int nSrs = 0;
    if (oSRS.GetAuthorityCode("PROJCS") != nullptr) {
        nSrs = atoi(oSRS.GetAuthorityCode("PROJCS"));
    }
    else if (oSRS.GetAuthorityCode("GEOGCS") != nullptr) {
        nSrs = atoi(oSRS.GetAuthorityCode("GEOGCS"));
    }
    else {
        // could not determine projected or geographic code
        // default to EPSG:3857 if no code could be found
        nSrs = 3857;
    }

    /********************************************************************/
    /* Create JSON companion file.                                      */
    /********************************************************************/
    const CPLString osJSONFilename = GetJsonFilename(pszFilename);

    json_object *poJSONObject = json_object_new_object();

    char **pszTokens = poSrcDS->GetMetadata();
    const char *pszLayer = CSLFetchNameValue(pszTokens, "LAYER");

    if ( pszLayer == nullptr) {
        // Set the layer
        json_object_object_add(poJSONObject, "layer", json_object_new_string(
            CPLGetBasename(osJSONFilename)
        ));
    }
    else {
        // Set the layer
        json_object_object_add(poJSONObject, "layer", json_object_new_string(
            pszLayer
        ));
    }

    // Set the type
    json_object_object_add(poJSONObject, "type", json_object_new_string("arg"));
    // Set the datatype
    json_object_object_add(poJSONObject, "datatype", json_object_new_string(pszDataType));

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    // Set the number of rows
    json_object_object_add(poJSONObject, "rows", json_object_new_int(nYSize));
    // Set the number of columns
    json_object_object_add(poJSONObject, "cols", json_object_new_int(nXSize));
    // Set the xmin
    json_object_object_add(poJSONObject, "xmin", json_object_new_double(adfTransform[0]));
    // Set the ymax
    json_object_object_add(poJSONObject, "ymax", json_object_new_double(adfTransform[3]));
    // Set the cellwidth
    json_object_object_add(poJSONObject, "cellwidth", json_object_new_double(adfTransform[1]));
    // Set the cellheight
    json_object_object_add(poJSONObject, "cellheight", json_object_new_double(-adfTransform[5]));
    // Set the xmax
    json_object_object_add(poJSONObject, "xmax", json_object_new_double(adfTransform[0] + nXSize * adfTransform[1]));
    // Set the ymin
    json_object_object_add(poJSONObject, "ymin", json_object_new_double(adfTransform[3] + nYSize * adfTransform[5]));
    // Set the xskew
    json_object_object_add(poJSONObject, "xskew", json_object_new_double(adfTransform[2]));
    // Set the yskew
    json_object_object_add(poJSONObject, "yskew", json_object_new_double(adfTransform[4]));
    if (nSrs > 0) {
        // Set the epsg
        json_object_object_add(poJSONObject, "epsg", json_object_new_int(nSrs));
    }

    if (json_object_to_file(const_cast<char *>(osJSONFilename.c_str()), poJSONObject) < 0) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ARG driver can't write companion file.");

        json_object_put(poJSONObject);
        poJSONObject = nullptr;

        return nullptr;
    }

    json_object_put(poJSONObject);
    poJSONObject = nullptr;

    VSILFILE *fpImage = VSIFOpenL(pszFilename, "wb");
    if (fpImage == nullptr)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
              "ARG driver can't create data file %s.", pszFilename);

        // remove JSON file
        VSIUnlink( osJSONFilename.c_str() );

        return nullptr;
    }

    // only 1 raster band
    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( 1 );

#ifdef CPL_LSB
    bool bNative = false;
#else
    bool bNative = true;
#endif

    RawRasterBand *poDstBand = new RawRasterBand( fpImage, 0, nPixelOffset,
                                                  nPixelOffset * nXSize, eType,
                                                  bNative,
                                                  nXSize, nYSize,
                                                  RawRasterBand::OwnFP::NO);
    poDstBand->SetAccess(GA_Update);

    int nXBlockSize, nYBlockSize;
    poSrcBand->GetBlockSize(&nXBlockSize, &nYBlockSize);

    void *pabyData = CPLMalloc(nXBlockSize * nPixelOffset);

    // convert any blocks into scanlines
    for (int nYBlock = 0; nYBlock * nYBlockSize < nYSize; nYBlock++) {
        for (int nYScanline = 0; nYScanline < nYBlockSize; nYScanline++) {
            if ((nYScanline+1) + nYBlock * nYBlockSize > poSrcBand->GetYSize() )
            {
                continue;
            }

            for (int nXBlock = 0; nXBlock * nXBlockSize < nXSize; nXBlock++) {
                int nXValid;

                if( (nXBlock+1) * nXBlockSize > poSrcBand->GetXSize() )
                    nXValid = poSrcBand->GetXSize() - nXBlock * nXBlockSize;
                else
                    nXValid = nXBlockSize;

                CPLErr eErr = poSrcBand->RasterIO(GF_Read, nXBlock * nXBlockSize,
                    nYBlock * nYBlockSize + nYScanline, nXValid, 1, pabyData, nXBlockSize,
                    1, eType, 0, 0, nullptr);

                if (eErr != CE_None) {
                    CPLError(CE_Failure, CPLE_AppDefined, "Error reading.");

                    CPLFree( pabyData );
                    delete poDstBand;
                    VSIFCloseL( fpImage );

                    return nullptr;
                }

                eErr = poDstBand->RasterIO(GF_Write, nXBlock * nXBlockSize,
                    nYBlock * nYBlockSize + nYScanline, nXValid, 1, pabyData, nXBlockSize,
                    1, eType, 0, 0, nullptr);

                if (eErr != CE_None) {
                    CPLError(CE_Failure, CPLE_AppDefined, "Error writing.");

                    CPLFree( pabyData );
                    delete poDstBand;
                    VSIFCloseL( fpImage );

                    return nullptr;
                }
            }
        }
    }

    CPLFree( pabyData );
    delete poDstBand;
    VSIFCloseL( fpImage );

    return reinterpret_cast<GDALDataset *>( GDALOpen( pszFilename, GA_ReadOnly ) );
}

/************************************************************************/
/*                          GDALRegister_ARG()                          */
/************************************************************************/

void GDALRegister_ARG()
{
    if( GDALGetDriverByName( "ARG" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ARG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Azavea Raster Grid format" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/arg.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = ARGDataset::Identify;
    poDriver->pfnOpen = ARGDataset::Open;
    poDriver->pfnCreateCopy = ARGDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
