/******************************************************************************
 * $Id$
 *
 * Project:  Azavea Raster Grid format driver.
 * Purpose:  Implements support for reading and writing Azavea Raster Grid
 *           format.
 * Author:   David Zwarg <dzwarg@azavea.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, David Zwarg <dzwarg@azavea.com>
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "jsonc/json.h"
#include "jsonc/json_util.h"
#include <ogr_spatialref.h>

CPL_CVSID("$Id$");

#define MAX_FILENAME_LEN 4096

#ifndef NAN
#  ifdef HUGE_VAL
#    define NAN (HUGE_VAL * 0.0)
#  else

static float CPLNaN(void)
{
    float fNan;
    int nNan = 0x7FC00000;
    memcpy(&fNan, &nNan, 4);
    return fNan;
}

#    define NAN CPLNan()
#  endif
#endif

/************************************************************************/
/* ==================================================================== */
/*				ARGDataset				                                */
/* ==================================================================== */
/************************************************************************/

class ARGDataset : public RawDataset
{
        VSILFILE	*fpImage;	// image data file.
    
        double	adfGeoTransform[6];
        char * pszFilename;

    public:
        ARGDataset();
        ~ARGDataset();

        CPLErr 	GetGeoTransform( double * padfTransform );
   
        static int Identify( GDALOpenInfo * );
        static GDALDataset *Open( GDALOpenInfo * );
        static GDALDataset *CreateCopy( const char *, GDALDataset *, int, 
            char **, GDALProgressFunc, void *);
        virtual char ** GetFileList(void);
}; 

/************************************************************************/
/*                            ARGDataset()                              */
/************************************************************************/

ARGDataset::ARGDataset()
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    fpImage = NULL;
}

/************************************************************************/
/*                            ~ARGDataset()                             */
/************************************************************************/

ARGDataset::~ARGDataset()

{
    CPLFree(pszFilename);

    FlushCache();
    if( fpImage != NULL )
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
CPLString GetJsonFilename(CPLString pszFilename) 
{
    return CPLSPrintf( "%s/%s.json", CPLGetDirname(pszFilename), CPLGetBasename(pszFilename) );
}

/************************************************************************/
/*                           GetJsonObject()                            */
/************************************************************************/
json_object * GetJsonObject(CPLString pszFilename) 
{
    json_object * pJSONObject = NULL;
    CPLString pszJSONFilename = GetJsonFilename(pszFilename);

    pJSONObject = json_object_from_file((char *)pszJSONFilename.c_str());
    if (is_error(pJSONObject) || pJSONObject == NULL) {
        CPLDebug("ARGDataset", "GetJsonObject(): "
            "Could not parse JSON file.");
        return NULL;
    }

    return pJSONObject;
}

/************************************************************************/
/*                          GetJsonValueStr()                           */
/************************************************************************/
const char * GetJsonValueStr(json_object * pJSONObject, CPLString pszKey) 
{
    json_object * pJSONItem = json_object_object_get(pJSONObject, pszKey.c_str());
    if (pJSONItem == NULL) {
        CPLDebug("ARGDataset", "GetJsonValueStr(): "
            "Could not find '%s' in JSON.", pszKey.c_str());
        return NULL;
    }

    return json_object_get_string(pJSONItem);
}

/************************************************************************/
/*                          GetJsonValueDbl()                           */
/************************************************************************/
double GetJsonValueDbl(json_object * pJSONObject, CPLString pszKey) 
{
    const char *pszJSONStr = GetJsonValueStr(pJSONObject, pszKey.c_str());
    char *pszTmp;
    double fTmp;
    if (pszJSONStr == NULL) {
        return NAN;
    }
    pszTmp = (char *)pszJSONStr;
    fTmp = CPLStrtod(pszJSONStr, &pszTmp);
    if (pszTmp == pszJSONStr) {
        CPLDebug("ARGDataset", "GetJsonValueDbl(): "
            "Key value is not a numeric value: %s:%s", pszKey.c_str(), pszTmp);
        return NAN;
    }

    return fTmp;
}

/************************************************************************/
/*                           GetJsonValueInt()                          */
/************************************************************************/
int GetJsonValueInt(json_object * pJSONObject, CPLString pszKey) 
{
    double fTmp = GetJsonValueDbl(pJSONObject, pszKey.c_str());
    if (CPLIsNan(fTmp)) {
        return -1;
    }

    return (int)fTmp;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/
char ** ARGDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();
    CPLString pszJSONFilename = GetJsonFilename(pszFilename);

    papszFileList = CSLAddString( papszFileList, pszJSONFilename );

    return papszFileList;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ARGDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    json_object * pJSONObject;
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "arg")) {
        return FALSE;
    }

    pJSONObject = GetJsonObject(poOpenInfo->pszFilename);
    if (pJSONObject == NULL) {
        return FALSE;
    }

    if (is_error(pJSONObject)) {
        json_object_put(pJSONObject);
        pJSONObject = NULL;

        return FALSE;
    }

    json_object_put(pJSONObject);
    pJSONObject = NULL;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *ARGDataset::Open( GDALOpenInfo * poOpenInfo )
{
    json_object * pJSONObject;
    const char * pszJSONStr;
    /***** items from the json metadata *****/
    GDALDataType eType = GDT_Unknown;
    double fXmin = 0.0;
    double fYmin = 0.0;
    double fXmax = 0.0;
    double fYmax = 0.0;
    double fCellwidth = 1.0;
    double fCellheight = 1.0;
    double fXSkew = 0.0;
    double fYSkew = 0.0;
    int nRows = 0;
    int nCols = 0;
    int nSrs = 3857;
    /***** items from the json metadata *****/
    int nPixelOffset = 0;
    double fNoDataValue = NAN;

    char * pszWKT = NULL;
    OGRSpatialReference oSRS;
    OGRErr nErr = OGRERR_NONE;

    if ( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check metadata settings in JSON.                                */
/* -------------------------------------------------------------------- */

    pJSONObject = GetJsonObject(poOpenInfo->pszFilename);

    if (is_error(pJSONObject)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Error parsing JSON.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the type (always 'arg')
    pszJSONStr = GetJsonValueStr(pJSONObject, "type");
    if (pszJSONStr == NULL ) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'type' is missing from the JSON file.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }
    else if (!EQUAL(pszJSONStr, "arg")) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'type' is not recognized: '%s'.", pszJSONStr);
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the layer (always the file basename)
    pszJSONStr = GetJsonValueStr(pJSONObject, "layer");
    if (pszJSONStr == NULL) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'layer' is missing from the JSON file.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }
    else if (!EQUAL(pszJSONStr, CPLGetBasename(poOpenInfo->pszFilename))) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'layer' does not match the filename.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the datatype
    pszJSONStr = GetJsonValueStr(pJSONObject, "datatype");
    if (pszJSONStr == NULL) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'datatype' is missing from the JSON file.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }
    else if (EQUAL(pszJSONStr, "int8")) {
        CPLDebug("ARGDataset", "Open(): "
            "int8 data is not supported in GDAL -- mapped to uint8");
        eType = GDT_Byte; 
        nPixelOffset = 1;
        fNoDataValue = 255;
    }
    else if (EQUAL(pszJSONStr, "int16")) {
        eType = GDT_Int16;
        nPixelOffset = 2;
        fNoDataValue = -32767;
    }
    else if (EQUAL(pszJSONStr, "int32")) {
        eType = GDT_Int32;
        nPixelOffset = 4;
        fNoDataValue = -2e31;
    }
    else if (EQUAL(pszJSONStr, "uint8")) {
        eType = GDT_Byte; 
        nPixelOffset = 1;
        fNoDataValue = 255;
    }
    else if (EQUAL(pszJSONStr, "uint16")) {
        eType = GDT_UInt16;
        nPixelOffset = 2;
        fNoDataValue = 65535;
    }
    else if (EQUAL(pszJSONStr, "uint32")) {
        eType = GDT_UInt32;
        nPixelOffset = 4;
        fNoDataValue = -2e31;
    }
    else if (EQUAL(pszJSONStr, "float32")) {
        eType = GDT_Float32;
        nPixelOffset = 4;
        fNoDataValue = NAN;
    }
    else if (EQUAL(pszJSONStr, "float64")) { 
        eType = GDT_Float64;
        nPixelOffset = 8;
        fNoDataValue = NAN;
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
        pJSONObject = NULL;
        return NULL;
    }

    // get the xmin of the bounding box
    fXmin = GetJsonValueDbl(pJSONObject, "xmin");
    if (CPLIsNan(fXmin)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'xmin' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }
    
    // get the ymin of the bounding box
    fYmin = GetJsonValueDbl(pJSONObject, "ymin");
    if (CPLIsNan(fYmin)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'ymin' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the xmax of the bounding box
    fXmax = GetJsonValueDbl(pJSONObject, "xmax");
    if (CPLIsNan(fXmax)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'xmax' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the ymax of the bounding box
    fYmax = GetJsonValueDbl(pJSONObject, "ymax");
    if (CPLIsNan(fYmax)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'ymax' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the cell width
    fCellwidth = GetJsonValueDbl(pJSONObject, "cellwidth");
    if (CPLIsNan(fCellwidth)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'cellwidth' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the cell height
    fCellheight = GetJsonValueDbl(pJSONObject, "cellheight");
    if (CPLIsNan(fCellheight)) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'cellheight' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    fXSkew = GetJsonValueDbl(pJSONObject, "xskew");
    if (CPLIsNan(fXSkew)) {
        // not an error -- default to 0.0
        fXSkew = 0.0f;
    }

    fYSkew = GetJsonValueDbl(pJSONObject, "yskew");
    if (CPLIsNan(fYSkew)) {
        // not an error -- default to 0.0
        fYSkew = 0.0f;
    }

    // get the rows
    nRows = GetJsonValueInt(pJSONObject, "rows");
    if (nRows < 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'rows' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    // get the columns
    nCols = GetJsonValueInt(pJSONObject, "cols");
    if (nCols < 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The ARG 'cols' is missing or invalid.");
        json_object_put(pJSONObject);
        pJSONObject = NULL;
        return NULL;
    }

    nSrs = GetJsonValueInt(pJSONObject, "epsg");
    if (nSrs < 0) {
        // not an error -- default to web mercator
        nSrs = 3857;
    }

    nErr = oSRS.importFromEPSG(nSrs);
    if (nErr != OGRERR_NONE) {
        nErr = oSRS.importFromEPSG(3857);

        if (nErr == OGRERR_NONE) {
            CPLDebug("ARGDataset", "Open(): "
                "The EPSG provided did not import cleanly. Defaulting to EPSG:3857");
        }
        else {
            CPLError(CE_Failure, CPLE_AppDefined,
                "The 'epsg' value did not transate to a known spatial reference."
                " Please check the 'epsg' value and try again.");

            json_object_put(pJSONObject);
            pJSONObject = NULL;

            return NULL;
        }
    }

    nErr = oSRS.exportToWkt(&pszWKT);
    if (nErr != OGRERR_NONE) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The spatial reference is known, but could not be set on the "
            "dataset. Please check the 'epsg' value and try again.");

        json_object_put(pJSONObject);
        pJSONObject = NULL;

        return NULL;
    }

    // done with the json object now
    json_object_put(pJSONObject);
    pJSONObject = NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ARGDataset *poDS;

    poDS = new ARGDataset();

    poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->SetProjection( pszWKT );

    // done with the projection string
    CPLFree(pszWKT);

/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (poDS->fpImage == NULL)
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
            "Could not open dataset '%s'", poOpenInfo->pszFilename);
        return NULL;
    }

    poDS->adfGeoTransform[0] = fXmin;
    poDS->adfGeoTransform[1] = fCellwidth;
    poDS->adfGeoTransform[2] = fXSkew;
    poDS->adfGeoTransform[3] = fYmax;
    poDS->adfGeoTransform[4] = fYSkew;
    poDS->adfGeoTransform[5] = -fCellheight;
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    RawRasterBand *poBand;
#ifdef CPL_LSB
    int	bNative = TRUE;
#else
    int bNative = FALSE;
#endif

    poBand = new RawRasterBand( poDS, 1, poDS->fpImage,
                                0, nPixelOffset, nPixelOffset * nCols,
                                eType, bNative, TRUE );
    poDS->SetBand( 1, poBand );

    poBand->SetNoDataValue( fNoDataValue );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();
    
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/
GDALDataset * ARGDataset::CreateCopy( const char * pszFilename, 
    GDALDataset * poSrcDS, int bStrict, char ** papszOptions, 
    GDALProgressFunc pfnProgress, void * pProgressData ) 
{
    int nBands = poSrcDS->GetRasterCount();
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    int nXBlockSize, nYBlockSize, nPixelOffset;
    GDALDataType eType;
    CPLString pszJSONFilename;
    CPLString pszDataType;
    json_object * poJSONObject = NULL;
    double adfTransform[6];
    VSILFILE * fpImage = NULL;
    GDALRasterBand * poBand = NULL;
    GByte * pabyData;
    OGRSpatialReference oSRS;
    char * pszWKT = NULL;
    int nSrs = 0;
    OGRErr nErr = OGRERR_NONE;

    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
              "ARG driver doesn't support %d bands.  Must be 1 band.", nBands );
        return NULL;
    }

    eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    if( eType == GDT_Unknown || 
        eType == GDT_CInt16 || 
        eType == GDT_CInt32 ||
        eType == GDT_CFloat32 || 
        eType == GDT_CFloat64 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ARG driver doesn't support data type %s.",
                  GDALGetDataTypeName(eType) );
        return NULL;
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

    poSrcDS->GetGeoTransform( adfTransform );

    pszWKT = (char *)poSrcDS->GetProjectionRef();
    nErr = oSRS.importFromWkt(&pszWKT);
    if (nErr != OGRERR_NONE) {
        CPLError( CE_Failure, CPLE_NotSupported, 
              "Cannot import spatial reference WKT from source dataset.");
        return NULL;
    }

    if (oSRS.GetAuthorityCode("PROJCS") != NULL) {
        nSrs = atoi(oSRS.GetAuthorityCode("PROJCS"));
    }
    else if (oSRS.GetAuthorityCode("GEOGCS") != NULL) {
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
    pszJSONFilename = GetJsonFilename(pszFilename);
    if (pszJSONFilename == NULL) {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ARG driver can't determine filename for companion file.");
        return NULL;
    }

    poJSONObject = json_object_new_object();

    // Set the layer
    json_object_object_add(poJSONObject, "layer", json_object_new_string(
        CPLGetBasename(pszJSONFilename)
    ));
    // Set the type
    json_object_object_add(poJSONObject, "type", json_object_new_string("arg"));
    // Set the datatype
    json_object_object_add(poJSONObject, "datatype", json_object_new_string(pszDataType));
    // Set the number of rows
    json_object_object_add(poJSONObject, "rows", json_object_new_int(nXSize));
    // Set the number of columns
    json_object_object_add(poJSONObject, "cols", json_object_new_int(nYSize));
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

    if (json_object_to_file((char *)pszJSONFilename.c_str(), poJSONObject) < 0) {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ARG driver can't write companion file.");

        json_object_put(poJSONObject);
        poJSONObject = NULL;

        return NULL;
    }

    json_object_put(poJSONObject);
    poJSONObject = NULL;

    fpImage = VSIFOpenL(pszFilename, "wb");
    if (fpImage == NULL)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
              "ARG driver can't create data file %s.", pszFilename);

        // remove JSON file
        VSIUnlink( pszJSONFilename.c_str() );

        return NULL;
    }

    // only 1 raster band
    poBand = poSrcDS->GetRasterBand( 1 );

    poBand->GetBlockSize(&nXBlockSize, &nYBlockSize);

    pabyData = (GByte *) CPLMalloc(nXBlockSize * nYBlockSize);

    // convert any blocks into scanlines
    for (int nYBlock = 0; nYBlock * nYBlockSize < nYSize; nYBlock++) {
        for (int nYScanline = 0; nYScanline < nYBlockSize; nYScanline++) {
            for (int nXBlock = 0; nXBlock * nXBlockSize < nXSize; nXBlock++) {
                int nXValid;

                poBand->ReadBlock(nXBlock, nYBlock, pabyData);

                if( (nXBlock+1) * nXBlockSize > poBand->GetXSize() )
                    nXValid = poBand->GetXSize() - nXBlock * nXBlockSize;
                else
                    nXValid = nXBlockSize;

                VSIFWriteL( pabyData + (nYScanline * nXBlockSize * nPixelOffset), nPixelOffset, nXValid, fpImage );
            }
        }
    }

    CPLFree( pabyData );
    VSIFCloseL( fpImage );

    return (GDALDataset *)GDALOpen( pszFilename, GA_ReadOnly );
}

/************************************************************************/
/*                          GDALRegister_ARG()                          */
/************************************************************************/

void GDALRegister_ARG()
{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ARG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ARG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Azavea Raster Grid format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#ARG" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = ARGDataset::Identify;
        poDriver->pfnOpen = ARGDataset::Open;
        poDriver->pfnCreateCopy = ARGDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

