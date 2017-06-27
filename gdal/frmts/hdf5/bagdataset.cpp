/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read BAG datasets.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gh5_convenience.h"

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "iso19115_srs.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                               BAGDataset                             */
/* ==================================================================== */
/************************************************************************/
class BAGDataset : public GDALPamDataset
{
    friend class BAGRasterBand;

    hid_t        hHDF5;

    char        *pszProjection;
    double       adfGeoTransform[6];

    void         LoadMetadata();

    char        *pszXMLMetadata;
    char        *apszMDList[2];

public:
    BAGDataset();
    virtual ~BAGDataset();

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *GetProjectionRef(void) override;
    virtual char      **GetMetadataDomainList() override;
    virtual char      **GetMetadata( const char * pszDomain = "" ) override;

    static GDALDataset  *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

    OGRErr ParseWKTFromXML( const char *pszISOXML );
};

/************************************************************************/
/* ==================================================================== */
/*                               BAGRasterBand                          */
/* ==================================================================== */
/************************************************************************/
class BAGRasterBand : public GDALPamRasterBand
{
    friend class BAGDataset;

    hid_t       hDatasetID;
    hid_t       native;
    hid_t       dataspace;

    bool        bMinMaxSet;
    double      dfMinimum;
    double      dfMaximum;

public:
    BAGRasterBand( BAGDataset *, int );
    virtual ~BAGRasterBand();

    bool                    Initialize( hid_t hDataset, const char *pszName );

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual double          GetNoDataValue( int * ) override;

    virtual double GetMinimum( int *pbSuccess = NULL ) override;
    virtual double GetMaximum( int *pbSuccess = NULL ) override;
};

/************************************************************************/
/*                           BAGRasterBand()                            */
/************************************************************************/
BAGRasterBand::BAGRasterBand( BAGDataset *poDS_, int nBand_ ) :
    hDatasetID(-1),
    native(-1),
    dataspace(-1),
    bMinMaxSet(false),
    dfMinimum(0.0),
    dfMaximum(0.0)
{
    poDS = poDS_;
    nBand = nBand_;
}

/************************************************************************/
/*                           ~BAGRasterBand()                           */
/************************************************************************/

BAGRasterBand::~BAGRasterBand()
{
    if( dataspace > 0 )
        H5Sclose(dataspace);

    if( native > 0 )
        H5Tclose(native);

    if( hDatasetID > 0 )
        H5Dclose(hDatasetID);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

bool BAGRasterBand::Initialize( hid_t hDatasetIDIn, const char *pszName )

{
    SetDescription(pszName);

    hDatasetID = hDatasetIDIn;

    const hid_t datatype = H5Dget_type(hDatasetIDIn);
    dataspace = H5Dget_space(hDatasetIDIn);
    const int n_dims = H5Sget_simple_extent_ndims(dataspace);
    native = H5Tget_native_type(datatype, H5T_DIR_ASCEND);
    hsize_t dims[3] = {
      static_cast<hsize_t>(0),
      static_cast<hsize_t>(0),
      static_cast<hsize_t>(0)
    };
    hsize_t maxdims[3] = {
      static_cast<hsize_t>(0),
      static_cast<hsize_t>(0),
      static_cast<hsize_t>(0)
    };

    eDataType = GH5_GetDataType(native);

    if( n_dims == 2 )
    {
        H5Sget_simple_extent_dims(dataspace, dims, maxdims);

        nRasterXSize = static_cast<int>(dims[1]);
        nRasterYSize = static_cast<int>(dims[0]);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset not of rank 2.");
        return false;
    }

    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;

    // Check for chunksize, and use it as blocksize for optimized reading.
    const hid_t listid = H5Dget_create_plist(hDatasetIDIn);
    if( listid > 0 )
    {
        if(H5Pget_layout(listid) == H5D_CHUNKED)
        {
            hsize_t panChunkDims[3] = {
              static_cast<hsize_t>(0),
              static_cast<hsize_t>(0),
              static_cast<hsize_t>(0)
            };
            const int nDimSize = H5Pget_chunk(listid, 3, panChunkDims);
            nBlockXSize = static_cast<int>(panChunkDims[nDimSize - 1]);
            nBlockYSize = static_cast<int>(panChunkDims[nDimSize - 2]);
        }

        int nfilters = H5Pget_nfilters(listid);

        char name[120] = {};
        size_t cd_nelmts = 20;
        unsigned int cd_values[20] = {};
        unsigned int flags = 0;
        for( int i = 0; i < nfilters; i++ )
        {
            const H5Z_filter_t filter = H5Pget_filter(
                listid, i, &flags, &cd_nelmts, cd_values, 120, name);
            if( filter == H5Z_FILTER_DEFLATE )
                poDS->SetMetadataItem("COMPRESSION", "DEFLATE",
                                      "IMAGE_STRUCTURE");
            else if( filter == H5Z_FILTER_NBIT )
                poDS->SetMetadataItem("COMPRESSION", "NBIT", "IMAGE_STRUCTURE");
            else if( filter == H5Z_FILTER_SCALEOFFSET )
                poDS->SetMetadataItem("COMPRESSION", "SCALEOFFSET",
                                      "IMAGE_STRUCTURE");
            else if( filter == H5Z_FILTER_SZIP )
                poDS->SetMetadataItem("COMPRESSION", "SZIP", "IMAGE_STRUCTURE");
        }

        H5Pclose(listid);
    }

    // Load min/max information.
    if( EQUAL(pszName,"elevation") &&
        GH5_FetchAttribute(hDatasetIDIn, "Maximum Elevation Value",
                           dfMaximum) &&
        GH5_FetchAttribute(hDatasetIDIn, "Minimum Elevation Value", dfMinimum) )
    {
        bMinMaxSet = true;
    }
    else if( EQUAL(pszName, "uncertainty") &&
             GH5_FetchAttribute(hDatasetIDIn, "Maximum Uncertainty Value",
                                dfMaximum) &&
             GH5_FetchAttribute(hDatasetIDIn, "Minimum Uncertainty Value",
                                dfMinimum) )
    {
        // Some products where uncertainty band is completely set to nodata
        // wrongly declare minimum and maximum to 0.0.
        if( dfMinimum != 0.0 && dfMaximum != 0.0 )
            bMinMaxSet = true;
    }
    else if( EQUAL(pszName, "nominal_elevation") &&
             GH5_FetchAttribute(hDatasetIDIn, "max_value", dfMaximum) &&
             GH5_FetchAttribute(hDatasetIDIn, "min_value", dfMinimum) )
    {
        bMinMaxSet = true;
    }

    return true;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double BAGRasterBand::GetMinimum( int * pbSuccess )

{
    if( bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfMinimum;
    }

    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double BAGRasterBand::GetMaximum( int *pbSuccess )

{
    if( bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfMaximum;
    }

    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double BAGRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = TRUE;

    if( EQUAL(GetDescription(), "elevation") )
        return 1000000.0;
    if( EQUAL(GetDescription(), "uncertainty") )
        return 1000000.0;
    if( EQUAL(GetDescription(), "nominal_elevation") )
        return 1000000.0;

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr BAGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void *pImage )
{
    const int nXOff = nBlockXOff * nBlockXSize;
    H5OFFSET_TYPE offset[3] = {
        static_cast<H5OFFSET_TYPE>(
            std::max(0, nRasterYSize - (nBlockYOff + 1) * nBlockYSize)),
        static_cast<H5OFFSET_TYPE>(nXOff),
        static_cast<H5OFFSET_TYPE>(0)
    };

    const int nSizeOfData = static_cast<int>(H5Tget_size(native));
    memset(pImage, 0, nBlockXSize * nBlockYSize * nSizeOfData);

    //  Blocksize may not be a multiple of imagesize.
    hsize_t count[3] = {
        std::min(static_cast<hsize_t>(nBlockYSize), GetYSize() - offset[0]),
        std::min(static_cast<hsize_t>(nBlockXSize), GetXSize() - offset[1]),
        static_cast<hsize_t>(0)
    };

    if( nRasterYSize - (nBlockYOff + 1) * nBlockYSize < 0 )
    {
        count[0] += (nRasterYSize - (nBlockYOff + 1) * nBlockYSize);
    }

    // Select block from file space.
    {
        const herr_t status =
            H5Sselect_hyperslab(dataspace, H5S_SELECT_SET,
                                 offset, NULL, count, NULL);
        if( status < 0 )
            return CE_Failure;
    }

    // Create memory space to receive the data.
    hsize_t col_dims[3] = {
        static_cast<hsize_t>(nBlockYSize),
        static_cast<hsize_t>(nBlockXSize),
        static_cast<hsize_t>(0)
    };
    const int rank = 2;
    const hid_t memspace = H5Screate_simple(rank, col_dims, NULL);
    H5OFFSET_TYPE mem_offset[3] = { 0, 0, 0 };
    const herr_t status =
        H5Sselect_hyperslab(memspace, H5S_SELECT_SET,
                            mem_offset, NULL, count, NULL);
    if( status < 0 )
        return CE_Failure;

    const herr_t status_read =
        H5Dread(hDatasetID, native, memspace, dataspace, H5P_DEFAULT, pImage);

    H5Sclose(memspace);

    // Y flip the data.
    const int nLinesToFlip = static_cast<int>(count[0]);
    const int nLineSize = nSizeOfData * nBlockXSize;
    GByte * const pabyTemp = static_cast<GByte *>(CPLMalloc(nLineSize));
    GByte * const pbyImage = static_cast<GByte *>(pImage);

    for( int iY = 0; iY < nLinesToFlip / 2; iY++ )
    {
        memcpy(pabyTemp, pbyImage + iY * nLineSize, nLineSize);
        memcpy(pbyImage + iY * nLineSize,
               pbyImage + (nLinesToFlip - iY - 1) * nLineSize,
               nLineSize);
        memcpy(pbyImage + (nLinesToFlip - iY - 1) * nLineSize, pabyTemp,
               nLineSize);
    }

    CPLFree(pabyTemp);

    // Return success or failure.
    if( status_read < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "H5Dread() failed for block.");
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              BAGDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             BAGDataset()                             */
/************************************************************************/

BAGDataset::BAGDataset() :
    hHDF5(-1),
    pszProjection(NULL),
    pszXMLMetadata(NULL)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    apszMDList[0] = NULL;
    apszMDList[1] = NULL;
}

/************************************************************************/
/*                            ~BAGDataset()                             */
/************************************************************************/
BAGDataset::~BAGDataset()
{
    FlushCache();

    if( hHDF5 >= 0 )
        H5Fclose(hHDF5);

    CPLFree(pszProjection);
    CPLFree(pszXMLMetadata);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BAGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    // Is it an HDF5 file?
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if( poOpenInfo->pabyHeader == NULL ||
        memcmp(poOpenInfo->pabyHeader, achSignature, 8) != 0 )
        return FALSE;

    // Does it have the extension .bag?
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "bag") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BAGDataset::Open( GDALOpenInfo *poOpenInfo )

{
    // Confirm that this appears to be a BAG file.
    if( !Identify(poOpenInfo) )
        return NULL;

    // Confirm the requested access is supported.
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The BAG driver does not support update access.");
        return NULL;
    }

    // Open the file as an HDF5 file.
    hid_t hHDF5 = H5Fopen(poOpenInfo->pszFilename, H5F_ACC_RDONLY, H5P_DEFAULT);

    if( hHDF5 < 0 )
        return NULL;

    // Confirm it is a BAG dataset by checking for the
    // BAG_Root/Bag Version attribute.
    const hid_t hBagRoot = H5Gopen(hHDF5, "/BAG_root");
    const hid_t hVersion =
        hBagRoot >= 0 ? H5Aopen_name(hBagRoot, "Bag Version") : -1;

    if( hVersion < 0 )
    {
        if( hBagRoot >= 0 )
            H5Gclose(hBagRoot);
        H5Fclose(hHDF5);
        return NULL;
    }
    H5Aclose(hVersion);

    // Create a corresponding dataset.
    BAGDataset *const poDS = new BAGDataset();

    poDS->hHDF5 = hHDF5;

    // Extract version as metadata.
    CPLString osVersion;

    if( GH5_FetchAttribute(hBagRoot, "Bag Version", osVersion) )
        poDS->SetMetadataItem("BagVersion", osVersion);

    H5Gclose(hBagRoot);

    // Fetch the elevation dataset and attach as a band.
    int nNextBand = 1;
    const hid_t hElevation = H5Dopen(hHDF5, "/BAG_root/elevation");
    if( hElevation < 0 )
    {
        delete poDS;
        return NULL;
    }

    BAGRasterBand *poElevBand = new BAGRasterBand(poDS, nNextBand);

    if( !poElevBand->Initialize(hElevation, "elevation") )
    {
        delete poElevBand;
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poElevBand->nRasterXSize;
    poDS->nRasterYSize = poElevBand->nRasterYSize;

    poDS->SetBand(nNextBand++, poElevBand);

    // Try to do the same for the uncertainty band.
    const hid_t hUncertainty = H5Dopen(hHDF5, "/BAG_root/uncertainty");
    BAGRasterBand *poUBand = new BAGRasterBand(poDS, nNextBand);

    if( hUncertainty >= 0 && poUBand->Initialize(hUncertainty, "uncertainty") )
    {
        poDS->SetBand(nNextBand++, poUBand);
    }
    else
    {
        delete poUBand;
    }

    // Try to do the same for the nominal_elevation band.
    hid_t hNominal = -1;

    H5E_BEGIN_TRY {
        hNominal = H5Dopen(hHDF5, "/BAG_root/nominal_elevation");
    } H5E_END_TRY;

    BAGRasterBand *const poNBand = new BAGRasterBand(poDS, nNextBand);
    if( hNominal >= 0 && poNBand->Initialize(hNominal, "nominal_elevation") )
    {
        poDS->SetBand(nNextBand++, poNBand);
    }
    else
    {
        delete poNBand;
    }

    // Load the XML metadata.
    poDS->LoadMetadata();

    // Setup/check for pam .aux.xml.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Setup overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return poDS;
}

/************************************************************************/
/*                            LoadMetadata()                            */
/************************************************************************/

void BAGDataset::LoadMetadata()

{
    // Load the metadata from the file.
    const hid_t hMDDS = H5Dopen(hHDF5, "/BAG_root/metadata");
    const hid_t datatype = H5Dget_type(hMDDS);
    const hid_t dataspace = H5Dget_space(hMDDS);
    const hid_t native = H5Tget_native_type(datatype, H5T_DIR_ASCEND);
    hsize_t dims[3] = {
        static_cast<hsize_t>(0),
        static_cast<hsize_t>(0),
        static_cast<hsize_t>(0)
    };
    hsize_t maxdims[3] = {
        static_cast<hsize_t>(0),
        static_cast<hsize_t>(0),
        static_cast<hsize_t>(0)
    };

    H5Sget_simple_extent_dims(dataspace, dims, maxdims);

    pszXMLMetadata =
        static_cast<char *>(CPLCalloc(static_cast<int>(dims[0] + 1), 1));

    H5Dread(hMDDS, native, H5S_ALL, dataspace, H5P_DEFAULT, pszXMLMetadata);

    H5Tclose(native);
    H5Sclose(dataspace);
    H5Tclose(datatype);
    H5Dclose(hMDDS);

    if( strlen(pszXMLMetadata) == 0 )
        return;

    // Try to get the geotransform.
    CPLXMLNode *psRoot = CPLParseXMLString(pszXMLMetadata);

    if( psRoot == NULL )
        return;

    CPLStripXMLNamespace(psRoot, NULL, TRUE);

    CPLXMLNode *const psGeo = CPLSearchXMLNode(psRoot, "=MD_Georectified");

    if( psGeo != NULL )
    {
        char **papszCornerTokens = CSLTokenizeStringComplex(
            CPLGetXMLValue(psGeo, "cornerPoints.Point.coordinates", ""), " ,",
            FALSE, FALSE);

        if( CSLCount(papszCornerTokens) == 4 )
        {
            const double dfLLX = CPLAtof(papszCornerTokens[0]);
            const double dfLLY = CPLAtof(papszCornerTokens[1]);
            const double dfURX = CPLAtof(papszCornerTokens[2]);
            const double dfURY = CPLAtof(papszCornerTokens[3]);

            adfGeoTransform[0] = dfLLX;
            adfGeoTransform[1] = (dfURX - dfLLX) / (GetRasterXSize() - 1);
            adfGeoTransform[3] = dfURY;
            adfGeoTransform[5] = (dfLLY - dfURY) / (GetRasterYSize() - 1);

            adfGeoTransform[0] -= adfGeoTransform[1] * 0.5;
            adfGeoTransform[3] -= adfGeoTransform[5] * 0.5;
        }
        CSLDestroy(papszCornerTokens);
    }

    // Try to get the coordinate system.
    OGRSpatialReference oSRS;

    if( OGR_SRS_ImportFromISO19115(&oSRS, pszXMLMetadata) == OGRERR_NONE )
    {
        oSRS.exportToWkt(&pszProjection);
    }
    else
    {
        ParseWKTFromXML(pszXMLMetadata);
    }

    // Fetch acquisition date.
    CPLXMLNode *const psDateTime = CPLSearchXMLNode(psRoot, "=dateTime");
    if( psDateTime != NULL )
    {
        const char *pszDateTimeValue = CPLGetXMLValue(psDateTime, NULL, "");
        if( pszDateTimeValue )
            SetMetadataItem("BAG_DATETIME", pszDateTimeValue);
    }

    CPLDestroyXMLNode(psRoot);
}

/************************************************************************/
/*                          ParseWKTFromXML()                           */
/************************************************************************/
OGRErr BAGDataset::ParseWKTFromXML( const char *pszISOXML )
{
    CPLXMLNode *const psRoot = CPLParseXMLString(pszISOXML);

    if( psRoot == NULL )
        return OGRERR_FAILURE;

    CPLStripXMLNamespace(psRoot, NULL, TRUE);

    CPLXMLNode *psRSI = CPLSearchXMLNode(psRoot, "=referenceSystemInfo");
    if( psRSI == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find <referenceSystemInfo> in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    OGRSpatialReference oSRS;
    oSRS.Clear();

    const char *pszSRCodeString =
        CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                       "RS_Identifier.code.CharacterString", NULL);
    if( pszSRCodeString == NULL )
    {
        CPLDebug("BAG",
                 "Unable to find /MI_Metadata/referenceSystemInfo[1]/"
                 "MD_ReferenceSystem[1]/referenceSystemIdentifier[1]/"
                 "RS_Identifier[1]/code[1]/CharacterString[1] in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    const char *pszSRCodeSpace =
        CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                       "RS_Identifier.codeSpace.CharacterString", "");
    if( !EQUAL(pszSRCodeSpace, "WKT") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Spatial reference string is not in WKT.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    char *pszWKT = const_cast<char *>(pszSRCodeString);
    if( oSRS.importFromWkt(&pszWKT) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed parsing WKT string \"%s\".", pszSRCodeString);
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    oSRS.exportToWkt(&pszProjection);

    psRSI = CPLSearchXMLNode(psRSI->psNext, "=referenceSystemInfo");
    if( psRSI == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find second instance of <referenceSystemInfo> "
                 "in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_NONE;
    }

    pszSRCodeString =
      CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                     "RS_Identifier.code.CharacterString", NULL);
    if( pszSRCodeString == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find /MI_Metadata/referenceSystemInfo[2]/"
                 "MD_ReferenceSystem[1]/referenceSystemIdentifier[1]/"
                 "RS_Identifier[1]/code[1]/CharacterString[1] in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_NONE;
    }

    pszSRCodeSpace =
        CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                       "RS_Identifier.codeSpace.CharacterString", "");
    if( !EQUAL(pszSRCodeSpace, "WKT") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Spatial reference string is not in WKT.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_NONE;
    }

    if( STARTS_WITH_CI(pszSRCodeString, "VERTCS") )
    {
        CPLString oString(pszProjection);
        CPLFree(pszProjection);
        oString += ",";
        oString += pszSRCodeString;
        pszProjection = CPLStrdup(oString);
    }

    CPLDestroyXMLNode(psRoot);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BAGDataset::GetGeoTransform( double *padfGeoTransform )

{
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[3] != 0.0 )
    {
        memcpy(padfGeoTransform, adfGeoTransform, sizeof(double)*6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BAGDataset::GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **BAGDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:BAG", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **BAGDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"xml:BAG") )
    {
        apszMDList[0] = pszXMLMetadata;
        apszMDList[1] = NULL;

        return apszMDList;
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GDALRegister_BAG()                          */
/************************************************************************/
void GDALRegister_BAG()

{
    if( !GDAL_CHECK_VERSION("BAG") )
        return;

    if( GDALGetDriverByName("BAG") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("BAG");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Bathymetry Attributed Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_bag.html");
    poDriver->pfnOpen = BAGDataset::Open;
    poDriver->pfnIdentify = BAGDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
