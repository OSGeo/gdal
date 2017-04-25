/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2007-2016, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2010, Kyle Shannon <kyle at pobox dot com>
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

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Must be included after standard includes, otherwise VS2015 fails when
// including <ctime>
#include "netcdfdataset.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_progress.h"
#include "cpl_time.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_version.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

// Internal function declarations.

static bool NCDFIsGDALVersionGTE(const char *pszVersion, int nTarget);

static void NCDFAddGDALHistory(
    int fpImage,
    const char *pszFilename, const char *pszOldHist,
    const char *pszFunctionName,
    const char *pszCFVersion = NCDF_CONVENTIONS_CF_V1_5 );

static void NCDFAddHistory( int fpImage, const char *pszAddHist,
                            const char *pszOldHist );

static bool NCDFIsCfProjection( const char *pszProjection );

static void NCDFWriteProjAttribs( const OGR_SRSNode *poPROJCS,
                                  const char *pszProjection,
                                  const int fpImage, const int NCDFVarID );

static CPLErr NCDFSafeStrcat( char **ppszDest, const char *pszSrc,
                              size_t *nDestSize );

// Var / attribute helper functions.
static CPLErr NCDFPutAttr( int nCdfId, int nVarId,
                           const char *pszAttrName, const char *pszValue );

// Replace this where used.
static CPLErr NCDFGet1DVar( int nCdfId, int nVarId, char **pszValue );
static CPLErr NCDFPut1DVar( int nCdfId, int nVarId, const char *pszValue );

static double NCDFGetDefaultNoDataValue( int nVarType );

// Dimension check functions.
static bool NCDFIsVarLongitude( int nCdfId, int nVarId=-1,
                                const char *nVarName=NULL );
static bool NCDFIsVarLatitude( int nCdfId, int nVarId=-1,
                               const char *nVarName=NULL );
static bool NCDFIsVarProjectionX( int nCdfId, int nVarId=-1,
                                  const char *pszVarName=NULL );
static bool NCDFIsVarProjectionY( int nCdfId, int nVarId=-1,
                                  const char *pszVarName=NULL );
static bool NCDFIsVarVerticalCoord( int nCdfId, int nVarId=-1,
                                    const char *nVarName=NULL );
static bool NCDFIsVarTimeCoord( int nCdfId, int nVarId=-1,
                                const char *nVarName=NULL );

// Replace this where used.
static char **NCDFTokenizeArray( const char *pszValue );
static void CopyMetadata( void  *poDS, int fpImage, int CDFVarID,
                          const char *pszMatchPrefix=NULL, bool bIsBand=true );

// Uncomment this for more debug output.
// #define NCDF_DEBUG 1

CPLMutex *hNCMutex = NULL;

/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand : public GDALPamRasterBand
{
    friend class netCDFDataset;

    nc_type     nc_datatype;
    int         cdfid;
    int         nZId;
    int         nZDim;
    int         nLevel;
    int         nBandXPos;
    int         nBandYPos;
    int         *panBandZPos;
    int         *panBandZLev;
    bool        bNoDataSet;
    double      dfNoDataValue;
    double      adfValidRange[2];
    bool        bHaveScale;
    bool        bHaveOffset;
    double      dfScale;
    double      dfOffset;
    CPLString   osUnitType;
    bool        bSignedData;
    bool        bCheckLongitude;

    CPLErr          CreateBandMetadata( const int *paDimIds );
    template <class T> void CheckData ( void *pImage, void *pImageNC,
                                        size_t nTmpBlockXSize,
                                        size_t nTmpBlockYSize,
                                        bool bCheckIsNan=false ) ;

  protected:
    CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;

  public:
    netCDFRasterBand( netCDFDataset *poDS,
                      int nZId,
                      int nZDim,
                      int nLevel,
                      const int *panBandZLen,
                      const int *panBandPos,
                      const int *paDimIds,
                      int nBand );
    netCDFRasterBand( netCDFDataset *poDS,
                      GDALDataType eType,
                      int nBand,
                      bool bSigned=true,
                      const char *pszBandName=NULL,
                      const char *pszLongName=NULL,
                      int nZId=-1,
                      int nZDim=2,
                      int nLevel=0,
                      const int *panBandZLev=NULL,
                      const int *panBandZPos=NULL,
                      const int *paDimIds=NULL );
    virtual ~netCDFRasterBand();

    virtual double GetNoDataValue( int * ) override;
    virtual CPLErr SetNoDataValue( double ) override;
    // virtual CPLErr DeleteNoDataValue();
    virtual double GetOffset( int * ) override;
    virtual CPLErr SetOffset( double ) override;
    virtual double GetScale( int * ) override;
    virtual CPLErr SetScale( double ) override;
    virtual const char *GetUnitType() override;
    virtual CPLErr SetUnitType( const char * ) override;
    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;
};

/************************************************************************/
/*                          netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::netCDFRasterBand( netCDFDataset *poNCDFDS,
                                    int nZIdIn,
                                    int nZDimIn,
                                    int nLevelIn,
                                    const int *panBandZLevIn,
                                    const int *panBandZPosIn,
                                    const int *paDimIds,
                                    int nBandIn ) :
    nc_datatype(NC_NAT),
    cdfid(poNCDFDS->GetCDFID()),
    nZId(nZIdIn),
    nZDim(nZDimIn),
    nLevel(nLevelIn),
    nBandXPos(panBandZPosIn[0]),
    nBandYPos(panBandZPosIn[1]),
    panBandZPos(NULL),
    panBandZLev(NULL),
    bNoDataSet(false),
    dfNoDataValue(0.0),
    bHaveScale(false),
    bHaveOffset(false),
    dfScale(1.0),
    dfOffset(0.0),
    bSignedData(true),   // Default signed, except for Byte.
    bCheckLongitude(false)
{
    poDS = poNCDFDS;
    nBand = nBandIn;

    // Take care of all other dimensions.
    if( nZDim > 2 )
    {
        panBandZPos = static_cast<int *>(CPLCalloc(nZDim - 1, sizeof(int)));
        panBandZLev = static_cast<int *>(CPLCalloc(nZDim - 1, sizeof(int)));

        for( int i = 0; i < nZDim - 2; i++ )
        {
            panBandZPos[i] = panBandZPosIn[i + 2];
            panBandZLev[i] = panBandZLevIn[i];
        }
    }

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    // Get the type of the "z" variable, our target raster array.
    if( nc_inq_var(cdfid, nZId, NULL, &nc_datatype, NULL, NULL,
                   NULL) != NC_NOERR )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error in nc_var_inq() on 'z'.");
        return;
    }

    if( nc_datatype == NC_BYTE )
        eDataType = GDT_Byte;
    else if( nc_datatype == NC_CHAR )
        eDataType = GDT_Byte;
    else if( nc_datatype == NC_SHORT )
        eDataType = GDT_Int16;
    else if( nc_datatype == NC_INT )
        eDataType = GDT_Int32;
    else if( nc_datatype == NC_FLOAT )
        eDataType = GDT_Float32;
    else if( nc_datatype == NC_DOUBLE )
        eDataType = GDT_Float64;
#ifdef NETCDF_HAS_NC4
    // NC_UBYTE (unsigned byte) is only available for NC4.
    else if( nc_datatype == NC_UBYTE )
        eDataType = GDT_Byte;
    else if( nc_datatype == NC_USHORT )
        eDataType = GDT_UInt16;
    else if( nc_datatype == NC_UINT )
        eDataType = GDT_UInt32;
#endif
    else
    {
        if( nBand == 1 )
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unsupported netCDF datatype (%d), treat as Float32.",
                     static_cast<int>(nc_datatype));
        eDataType = GDT_Float32;
        nc_datatype = NC_FLOAT;
    }

    // Find and set No Data for this variable.
    nc_type atttype = NC_NAT;
    size_t attlen = 0;
    const char *pszNoValueName = NULL;

    // Find attribute name, either _FillValue or missing_value.
    int status = nc_inq_att(cdfid, nZId, _FillValue, &atttype, &attlen);
    if( status == NC_NOERR )
    {
        pszNoValueName = _FillValue;
    }
    else
    {
        status = nc_inq_att(cdfid, nZId, "missing_value", &atttype, &attlen);
        if( status == NC_NOERR )
        {
            pszNoValueName = "missing_value";
        }
    }

    // Fetch missing value.
    double dfNoData = 0.0;
    bool bGotNoData = false;
    if( status == NC_NOERR )
    {
        if( NCDFGetAttr(cdfid, nZId, pszNoValueName, &dfNoData) == CE_None )
        {
            bGotNoData = true;
        }
    }

    // If NoData was not found, use the default value.
    nc_type vartype = NC_NAT;
    if( !bGotNoData )
    {
        nc_inq_vartype(cdfid, nZId, &vartype);
        dfNoData = NCDFGetDefaultNoDataValue(vartype);
        // bGotNoData = true;
        CPLDebug("GDAL_netCDF",
                 "did not get nodata value for variable #%d, using default %f",
                 nZId, dfNoData);
    }

    // Look for valid_range or valid_min/valid_max.

    // Set valid_range to nodata, then check for actual values.
    adfValidRange[0] = dfNoData;
    adfValidRange[1] = dfNoData;
    // First look for valid_range.
    bool bGotValidRange = false;
    status = nc_inq_att(cdfid, nZId, "valid_range", &atttype, &attlen);
    if( (status == NC_NOERR) && (attlen == 2) &&
        CPLFetchBool(poNCDFDS->GetOpenOptions(), "HONOUR_VALID_RANGE", true) )
    {
        int vrange[2] = { 0, 0 };
        status = nc_get_att_int(cdfid, nZId, "valid_range", vrange);
        if( status == NC_NOERR )
        {
            bGotValidRange = true;
            adfValidRange[0] = vrange[0];
            adfValidRange[1] = vrange[1];
        }
        // If not found look for valid_min and valid_max.
        else
        {
            int vmin = 0;
            status = nc_get_att_int(cdfid, nZId, "valid_min", &vmin);
            if( status == NC_NOERR )
            {
                adfValidRange[0] = vmin;
                int vmax = 0;
                status = nc_get_att_int(cdfid, nZId, "valid_max", &vmax);
                if( status == NC_NOERR )
                {
                    adfValidRange[1] = vmax;
                    bGotValidRange = true;
                }
            }
        }
    }

    // Special For Byte Bands: check for signed/unsigned byte.
    if( nc_datatype == NC_BYTE )
    {
        // netcdf uses signed byte by default, but GDAL uses unsigned by default
        // This may cause unexpected results, but is needed for back-compat.
        if( poNCDFDS->bIsGdalFile )
            bSignedData = false;
        else
            bSignedData = true;

        // For NC4 format NC_BYTE is signed, NC_UBYTE is unsigned.
        if( poNCDFDS->eFormat == NCDF_FORMAT_NC4 )
        {
            bSignedData = true;
        }
        else
        {
            // If we got valid_range, test for signed/unsigned range.
            // http://www.unidata.ucar.edu/software/netcdf/docs/netcdf/Attribute-Conventions.html
            if( bGotValidRange )
            {
                // If we got valid_range={0,255}, treat as unsigned.
                if( adfValidRange[0] == 0 && adfValidRange[1] == 255 )
                {
                    bSignedData = false;
                    // Reset valid_range.
                    adfValidRange[0] = dfNoData;
                    adfValidRange[1] = dfNoData;
                }
                // If we got valid_range={-128,127}, treat as signed.
                else if( adfValidRange[0] == -128 && adfValidRange[1] == 127 )
                {
                    bSignedData = true;
                    // Reset valid_range.
                    adfValidRange[0] = dfNoData;
                    adfValidRange[1] = dfNoData;
                }
            }
            // Else test for _Unsigned.
            // http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html
            else
            {
                char *pszTemp = NULL;
                if( NCDFGetAttr(cdfid, nZId, "_Unsigned", &pszTemp) == CE_None )
                {
                    if( EQUAL(pszTemp, "true") )
                        bSignedData = false;
                    else if( EQUAL(pszTemp, "false") )
                        bSignedData = true;
                    CPLFree(pszTemp);
                }
            }
        }

        if( bSignedData )
        {
            // set PIXELTYPE=SIGNEDBYTE
            // See http://trac.osgeo.org/gdal/wiki/rfc14_imagestructure
            SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
        }
        else
        {
            // Fix nodata value as it was stored signed.
            if( dfNoData < 0 )
                dfNoData += 256;
        }
    }

#ifdef NETCDF_HAS_NC4
    if( nc_datatype == NC_UBYTE ||
        nc_datatype == NC_USHORT ||
        nc_datatype == NC_UINT )
        bSignedData = false;
#endif

    CPLDebug("GDAL_netCDF", "netcdf type=%d gdal type=%d signedByte=%d",
             nc_datatype, eDataType, static_cast<int>(bSignedData));

    // Set nodata value.
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "SetNoDataValue(%f) read", dfNoData);
#endif
    SetNoDataValue(dfNoData);

    // Create Band Metadata.
    CreateBandMetadata(paDimIds);

    // Attempt to fetch the scale_factor and add_offset attributes for the
    // variable and set them.  If these values are not available, set
    // offset to 0 and scale to 1.
    if( nc_inq_attid (cdfid, nZId, CF_ADD_OFFSET, NULL) == NC_NOERR )
    {
        status = nc_get_att_double(cdfid, nZId, CF_ADD_OFFSET, &dfOffset);
        CPLDebug("GDAL_netCDF", "got add_offset=%.16g, status=%d",
                 dfOffset, status);
        SetOffset(dfOffset);
    }

    if( nc_inq_attid(cdfid, nZId, CF_SCALE_FACTOR, NULL) == NC_NOERR )
    {
        status = nc_get_att_double(cdfid, nZId, CF_SCALE_FACTOR, &dfScale);
        CPLDebug("GDAL_netCDF", "got scale_factor=%.16g, status=%d",
                 dfScale, status);
        SetScale(dfScale);
    }

    // Should we check for longitude values > 360?
    bCheckLongitude =
        CPLTestBool(CPLGetConfigOption("GDAL_NETCDF_CENTERLONG_180", "YES")) &&
        NCDFIsVarLongitude(cdfid, nZId, NULL);

    // Attempt to fetch the units attribute for the variable and set it.
    SetUnitType(GetMetadataItem(CF_UNITS));

    // Check for variable chunking (netcdf-4 only).
    // GDAL block size should be set to hdf5 chunk size.
#ifdef NETCDF_HAS_NC4
    int nTmpFormat = 0;
    status = nc_inq_format(cdfid, &nTmpFormat);
    NetCDFFormatEnum eTmpFormat = static_cast<NetCDFFormatEnum>(nTmpFormat);
    if( (status == NC_NOERR) &&
        (eTmpFormat == NCDF_FORMAT_NC4 || eTmpFormat == NCDF_FORMAT_NC4C) )
    {
        size_t chunksize[MAX_NC_DIMS] = {};
        // Check for chunksize and set it as the blocksize (optimizes read).
        status = nc_inq_var_chunking(cdfid, nZId, &nTmpFormat, chunksize);
        if( (status == NC_NOERR) && (nTmpFormat == NC_CHUNKED) )
        {
            CPLDebug("GDAL_netCDF",
                     "setting block size to chunk size : %ld x %ld",
                     static_cast<long>(chunksize[nZDim - 1]),
                     static_cast<long>(chunksize[nZDim - 2]));
            nBlockXSize = (int)chunksize[nZDim - 1];
            nBlockYSize = (int)chunksize[nZDim - 2];
        }
    }
#endif

    // Force block size to 1 scanline for bottom-up datasets if
    // nBlockYSize != 1.
    if( poNCDFDS->bBottomUp && nBlockYSize != 1 )
    {
        nBlockXSize = nRasterXSize;
        nBlockYSize = 1;
    }
}

// Constructor in create mode.
// If nZId and following variables are not passed, the band will have 2
// dimensions.
// TODO: Get metadata, missing val from band #1 if nZDim > 2.
netCDFRasterBand::netCDFRasterBand( netCDFDataset *poNCDFDS,
                                    const GDALDataType eTypeIn,
                                    int nBandIn,
                                    bool bSigned,
                                    const char *pszBandName,
                                    const char *pszLongName,
                                    int nZIdIn,
                                    int nZDimIn,
                                    int nLevelIn,
                                    const int *panBandZLevIn,
                                    const int *panBandZPosIn,
                                    const int *paDimIds ) :
    nc_datatype(NC_NAT),
    cdfid(poNCDFDS->GetCDFID()),
    nZId(nZIdIn),
    nZDim(nZDimIn),
    nLevel(nLevelIn),
    nBandXPos(1),
    nBandYPos(0),
    panBandZPos(NULL),
    panBandZLev(NULL),
    bNoDataSet(false),
    dfNoDataValue(0.0),
    bHaveScale(false),
    bHaveOffset(false),
    dfScale(0.0),
    dfOffset(0.0),
    bSignedData(bSigned),
    bCheckLongitude(false)
{
    poDS = poNCDFDS;
    nBand = nBandIn;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if( poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset is not in update mode, "
                 "wrong netCDFRasterBand constructor");
        return;
    }

    // Take care of all other dimensions.
    if( nZDim > 2 && paDimIds != NULL )
    {
        nBandXPos = panBandZPosIn[0];
        nBandYPos = panBandZPosIn[1];

        panBandZPos = static_cast<int *>(CPLCalloc(nZDim - 1, sizeof(int)));
        panBandZLev = static_cast<int *>(CPLCalloc(nZDim - 1, sizeof(int)));

        for( int i = 0; i < nZDim - 2; i++ )
        {
            panBandZPos[i] = panBandZPosIn[i + 2];
            panBandZLev[i] = panBandZLevIn[i];
        }
    }

    // Get the type of the "z" variable, our target raster array.
    eDataType = eTypeIn;

    switch( eDataType )
    {
        case GDT_Byte:
            nc_datatype = NC_BYTE;
#ifdef NETCDF_HAS_NC4
            // NC_UBYTE (unsigned byte) is only available for NC4.
            if( !bSignedData && (poNCDFDS->eFormat == NCDF_FORMAT_NC4) )
                nc_datatype = NC_UBYTE;
#endif
            break;
        case GDT_Int16:
            nc_datatype = NC_SHORT;
            break;
        case GDT_Int32:
            nc_datatype = NC_INT;
            break;
        case GDT_Float32:
            nc_datatype = NC_FLOAT;
            break;
        case GDT_Float64:
            nc_datatype = NC_DOUBLE;
            break;
#ifdef NETCDF_HAS_NC4
        case GDT_UInt16:
            if( poNCDFDS->eFormat == NCDF_FORMAT_NC4 )
            {
                nc_datatype = NC_USHORT;
                break;
            }
            CPL_FALLTHROUGH
        case GDT_UInt32:
            if( poNCDFDS->eFormat == NCDF_FORMAT_NC4 )
            {
                nc_datatype = NC_UINT;
                break;
            }
            CPL_FALLTHROUGH
#endif
        default:
            if( nBand == 1 )
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unsupported GDAL datatype (%d), treat as NC_FLOAT.",
                         static_cast<int>(eDataType));
            nc_datatype = NC_FLOAT;
            eDataType = GDT_Float32;
            break;
    }

    // Define the variable if necessary (if nZId == -1).
    bool bDefineVar = false;

    if( nZId == -1 )
    {
        bDefineVar = true;

        // Make sure we are in define mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        char szTempPrivate[256 + 1];
        const char *pszTemp = NULL;
        if( !pszBandName || EQUAL(pszBandName, "") )
        {
            snprintf(szTempPrivate, sizeof(szTempPrivate), "Band%d", nBand);
            pszTemp = szTempPrivate;
        }
        else
        {
            pszTemp = pszBandName;
        }

        int status;
        if( nZDim > 2 && paDimIds != NULL )
        {
            status = nc_def_var(cdfid, pszTemp, nc_datatype,
                                 nZDim, paDimIds, &nZId);
        }
        else
        {
            int anBandDims[2] = { poNCDFDS->nYDimID, poNCDFDS->nXDimID };
            status =
                nc_def_var(cdfid, pszTemp, nc_datatype, 2, anBandDims, &nZId);
        }
        NCDF_ERR(status);
        CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d) id=%d",
                 cdfid, pszTemp, nc_datatype, nZId);

        if( !pszLongName || EQUAL(pszLongName, "") )
        {
            snprintf(szTempPrivate, sizeof(szTempPrivate),
                     "GDAL Band Number %d", nBand);
            pszTemp = szTempPrivate;
        }
        else
        {
            pszTemp = pszLongName;
        }
        status =
            nc_put_att_text(cdfid, nZId, CF_LNG_NAME, strlen(pszTemp), pszTemp);
        NCDF_ERR(status);

        poNCDFDS->DefVarDeflate(nZId, true);
    }

    // For Byte data add signed/unsigned info.
    if( eDataType == GDT_Byte )
    {
        if( bDefineVar )
        {
            // Only add attributes if creating variable.
            // For unsigned NC_BYTE (except NC4 format),
            // add valid_range and _Unsigned ( defined in CF-1 and NUG ).
            int status = NC_NOERR;
            if( nc_datatype == NC_BYTE &&
                poNCDFDS->eFormat != NCDF_FORMAT_NC4 )
            {
                CPLDebug("GDAL_netCDF",
                         "adding valid_range attributes for Byte Band");
                short l_adfValidRange[2] = { 0, 0 };
                if( bSignedData )
                {
                    l_adfValidRange[0] = -128;
                    l_adfValidRange[1] = 127;
                    status =
                        nc_put_att_text(cdfid, nZId, "_Unsigned", 5, "false");
                }
                else
                {
                    l_adfValidRange[0] = 0;
                    l_adfValidRange[1] = 255;
                    status =
                        nc_put_att_text(cdfid, nZId, "_Unsigned", 4, "true");
                }
                NCDF_ERR(status);
                status = nc_put_att_short(cdfid, nZId, "valid_range",
                                          NC_SHORT, 2, l_adfValidRange);
                NCDF_ERR(status);
            }
        }
        // For unsigned byte set PIXELTYPE=SIGNEDBYTE.
        // See http://trac.osgeo.org/gdal/wiki/rfc14_imagestructure
        if( bSignedData )
            SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
    }

    // Set default nodata.
    double dfNoData = NCDFGetDefaultNoDataValue(nc_datatype);
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "SetNoDataValue(%f) default", dfNoData);
#endif
    SetNoDataValue(dfNoData);
}

/************************************************************************/
/*                         ~netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::~netCDFRasterBand()
{
    FlushCache();
    CPLFree(panBandZPos);
    CPLFree(panBandZLev);
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/
double netCDFRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess != NULL )
        *pbSuccess = static_cast<int>(bHaveOffset);

    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/
CPLErr netCDFRasterBand::SetOffset( double dfNewOffset )
{
    CPLMutexHolderD(&hNCMutex);

    dfOffset = dfNewOffset;
    bHaveOffset = true;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // Make sure we are in define mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        const int status = nc_put_att_double(cdfid, nZId, CF_ADD_OFFSET,
                                             NC_DOUBLE, 1, &dfOffset);

        NCDF_ERR(status);
        if( status == NC_NOERR )
            return CE_None;

        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/
double netCDFRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess != NULL )
        *pbSuccess = static_cast<int>(bHaveScale);

    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/
CPLErr netCDFRasterBand::SetScale( double dfNewScale )
{
    CPLMutexHolderD(&hNCMutex);

    dfScale = dfNewScale;
    bHaveScale = true;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // Make sure we are in define mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        const int status = nc_put_att_double(cdfid, nZId, CF_SCALE_FACTOR,
                                             NC_DOUBLE, 1, &dfScale);

        NCDF_ERR(status);
        if( status == NC_NOERR )
            return CE_None;

        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *netCDFRasterBand::GetUnitType()

{
    if( !osUnitType.empty() )
        return osUnitType;

    return GDALRasterBand::GetUnitType();
}

/************************************************************************/
/*                           SetUnitType()                              */
/************************************************************************/

CPLErr netCDFRasterBand::SetUnitType( const char *pszNewValue )

{
    CPLMutexHolderD(&hNCMutex);

    osUnitType = (pszNewValue != NULL ? pszNewValue : "");

    if( !osUnitType.empty() )
    {
        // Write value if in update mode.
        if( poDS->GetAccess() == GA_Update )
        {
            // Make sure we are in define mode.
            static_cast<netCDFDataset *>(poDS)->SetDefineMode(TRUE);

            const int status = nc_put_att_text(
                cdfid, nZId, CF_UNITS, osUnitType.size(), osUnitType.c_str());

            NCDF_ERR(status);
            if( status == NC_NOERR )
                return CE_None;

            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double netCDFRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = static_cast<int>(bNoDataSet);

    if( bNoDataSet )
        return dfNoDataValue;

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr netCDFRasterBand::SetNoDataValue( double dfNoData )

{
    CPLMutexHolderD(&hNCMutex);

    // If already set to new value, don't do anything.
    if( bNoDataSet && CPLIsEqual(dfNoData, dfNoDataValue) )
        return CE_None;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // netcdf-4 does not allow to set _FillValue after leaving define mode,
        // but it's ok if variable has not been written to, so only print debug.
        // See bug #4484.
        if( bNoDataSet &&
            !reinterpret_cast<netCDFDataset *>(poDS)->GetDefineMode() )
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to %.18g (previously set to %.18g) "
                     "but file is no longer in define mode (id #%d, band #%d)",
                     dfNoData, dfNoDataValue, cdfid, nBand);
        }
#ifdef NCDF_DEBUG
        else
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to %.18g (id #%d, band #%d)",
                     dfNoData, cdfid, nBand);
        }
#endif
        // Make sure we are in define mode.
        reinterpret_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        int status;
        if( eDataType == GDT_Byte)
        {
            if( bSignedData )
            {
                signed char cNoDataValue = static_cast<signed char>(dfNoData);
                status = nc_put_att_schar(cdfid, nZId, _FillValue,
                                          nc_datatype, 1, &cNoDataValue);
            }
            else
            {
                const unsigned char ucNoDataValue =
                    static_cast<unsigned char>(dfNoData);
                status = nc_put_att_uchar(cdfid, nZId, _FillValue,
                                          nc_datatype, 1, &ucNoDataValue);
            }
        }
        else if( eDataType == GDT_Int16 )
        {
            short nsNoDataValue = static_cast<short>(dfNoData);
            status = nc_put_att_short(cdfid, nZId, _FillValue,
                                      nc_datatype, 1, &nsNoDataValue);
        }
        else if( eDataType == GDT_Int32)
        {
            int nNoDataValue = static_cast<int>(dfNoData);
            status = nc_put_att_int(cdfid, nZId, _FillValue,
                                     nc_datatype, 1, &nNoDataValue);
        }
        else if( eDataType == GDT_Float32)
        {
            float fNoDataValue = static_cast<float>(dfNoData);
            status = nc_put_att_float(cdfid, nZId, _FillValue,
                                      nc_datatype, 1, &fNoDataValue);
        }
#ifdef NETCDF_HAS_NC4
        else if( eDataType == GDT_UInt16 &&
                 reinterpret_cast<netCDFDataset *>(poDS)->eFormat ==
                 NCDF_FORMAT_NC4 )
        {
            unsigned short usNoDataValue =
                static_cast<unsigned short>(dfNoData);
            status = nc_put_att_ushort(cdfid, nZId, _FillValue,
                                       nc_datatype, 1, &usNoDataValue);
        }
        else if( eDataType == GDT_UInt32 &&
                 reinterpret_cast<netCDFDataset *>(poDS)->eFormat ==
                 NCDF_FORMAT_NC4 )
        {
            unsigned int unNoDataValue = static_cast<unsigned int>(dfNoData);
            status = nc_put_att_uint(cdfid, nZId, _FillValue,
                                     nc_datatype, 1, &unNoDataValue);
        }
#endif
        else
        {
            status = nc_put_att_double(cdfid, nZId, _FillValue,
                                       nc_datatype, 1, &dfNoData);
        }

        NCDF_ERR(status);

        // Update status if write worked.
        if( status == NC_NOERR )
        {
            dfNoDataValue = dfNoData;
            bNoDataSet = true;
            return CE_None;
        }

        return CE_Failure;
    }

    dfNoDataValue = dfNoData;
    bNoDataSet = true;
    return CE_None;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

#ifdef notdef
CPLErr netCDFRasterBand::DeleteNoDataValue()

{
    CPLMutexHolderD(&hNCMutex);

    if( !bNoDataSet )
        return CE_None;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // Make sure we are in define mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        status = nc_del_att(cdfid, nZId, _FillValue);

        NCDF_ERR(status);

        // Update status if write worked.
        if( status == NC_NOERR )
        {
            dfNoDataValue = 0.0;
            bNoDataSet = false;
            return CE_None;
        }

        return CE_Failure;
    }

    dfNoDataValue = 0.0;
    bNoDataSet = false;
    return CE_None;
}
#endif

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *netCDFRasterBand::SerializeToXML( const char * /* pszUnused */ )
{
    // Overridden from GDALPamDataset to add only band histogram
    // and statistics. See bug #4244.
    if( psPam == NULL )
        return NULL;

    // Setup root node and attributes.
    CPLXMLNode *psTree = CPLCreateXMLNode(NULL, CXT_Element, "PAMRasterBand");

    if( GetBand() > 0 )
    {
        CPLString oFmt;
        CPLSetXMLValue(psTree, "#band", oFmt.Printf("%d", GetBand()));
    }

    // Histograms.
    if( psPam->psSavedHistograms != NULL )
        CPLAddXMLChild(psTree, CPLCloneXMLTree(psPam->psSavedHistograms));

    // Metadata (statistics only).
    GDALMultiDomainMetadata oMDMDStats;
    const char *papszMDStats[] = { "STATISTICS_MINIMUM", "STATISTICS_MAXIMUM",
                                   "STATISTICS_MEAN", "STATISTICS_STDDEV",
                                   NULL };
    for( int i = 0; i < CSLCount(papszMDStats); i++ )
    {
        if( GetMetadataItem(papszMDStats[i]) != NULL )
            oMDMDStats.SetMetadataItem(papszMDStats[i],
                                       GetMetadataItem(papszMDStats[i]));
    }
    CPLXMLNode *psMD = oMDMDStats.Serialize();

    if( psMD != NULL )
    {
        if( psMD->psChild == NULL )
            CPLDestroyXMLNode(psMD);
        else
            CPLAddXMLChild(psTree, psMD);
    }

    // We don't want to return anything if we had no metadata to attach.
    if( psTree->psChild == NULL || psTree->psChild->psNext == NULL )
    {
        CPLDestroyXMLNode(psTree);
        psTree = NULL;
    }

    return psTree;
}

/************************************************************************/
/*                         CreateBandMetadata()                         */
/************************************************************************/

CPLErr netCDFRasterBand::CreateBandMetadata( const int *paDimIds )

{
    netCDFDataset *l_poDS = reinterpret_cast<netCDFDataset *>(poDS);

    // Compute all dimensions from Band number and save in Metadata.
    char szVarName[NC_MAX_NAME + 1] = {};
    int status = nc_inq_varname(cdfid, nZId, szVarName);
    NCDF_ERR(status);

    int nd = 0;
    nc_inq_varndims(cdfid, nZId, &nd);
    // Compute multidimention band position.
    //
    // BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)
    // if Data[2,3,4,x,y]
    //
    //  BandPos0 = (nBand) / (3*4)
    //  BandPos1 = (nBand - BandPos0*(3*4)) / (4)
    //  BandPos2 = (nBand - BandPos0*(3*4)) % (4)

    SetMetadataItem("NETCDF_VARNAME", szVarName);
    int Sum = 1;
    if( nd == 3 )
    {
        Sum *= panBandZLev[0];
    }

    // Loop over non-spatial dimensions.
    int nVarID = -1;
    int result = 0;
    int Taken = 0;

    for( int i = 0; i < nd - 2; i++ )
    {
        if( i != nd - 2 - 1 )
        {
            Sum = 1;
            for( int j = i + 1; j < nd - 2; j++ )
            {
                Sum *= panBandZLev[j];
            }
            result = static_cast<int>((nLevel - Taken) / Sum);
        }
        else
        {
            result = static_cast<int>((nLevel - Taken) % Sum);
        }

        snprintf(szVarName, sizeof(szVarName), "%s",
                 l_poDS->papszDimName[paDimIds[panBandZPos[i]]]);

        // TODO: Make sure all the status checks make sense.

        status = nc_inq_varid(cdfid, szVarName, &nVarID);
        if( status != NC_NOERR )
        {
            // Try to uppercase the first letter of the variable.
            // Note: Why is this needed?  Leaving for safety.
            szVarName[0] = static_cast<char>(toupper(szVarName[0]));
            /* status = */ nc_inq_varid(cdfid, szVarName, &nVarID);
        }

        nc_type nVarType = NC_NAT;
        /* status = */ nc_inq_vartype(cdfid, nVarID, &nVarType);

        int nDims = 0;
        /* status = */ nc_inq_varndims(cdfid, nVarID, &nDims);

        char szMetaTemp[256] = {};
        if( nDims == 1 )
        {
            size_t count[1] = { 1 };
            size_t start[1] = { static_cast<size_t>(result) };

            switch( nVarType )
            {
                case NC_BYTE:
                    // TODO: Check for signed/unsigned byte.
                    signed char cData;
                    /* status = */ nc_get_vara_schar(cdfid, nVarID,
                                                 start,
                                                 count, &cData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", cData);
                    break;
                case NC_SHORT:
                    short sData;
                    /* status = */ nc_get_vara_short(cdfid, nVarID,
                                                 start,
                                                 count, &sData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", sData);
                    break;
                case NC_INT:
                {
                    int nData;
                    /* status = */ nc_get_vara_int(cdfid, nVarID,
                                               start,
                                               count, &nData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", nData);
                    break;
                }
                case NC_FLOAT:
                    float fData;
                    /* status = */ nc_get_vara_float(cdfid, nVarID,
                                                 start,
                                                 count, &fData);
                    CPLsnprintf(szMetaTemp, sizeof(szMetaTemp),
                                 "%.8g", fData);
                    break;
                case NC_DOUBLE:
                    double dfData;
                    /* status = */ nc_get_vara_double(cdfid, nVarID,
                                                  start,
                                                  count, &dfData);
                    CPLsnprintf(szMetaTemp, sizeof(szMetaTemp),
                                 "%.16g", dfData);
                    break;
#ifdef NETCDF_HAS_NC4
                case NC_UBYTE:
                    unsigned char ucData;
                    /* status = */ nc_get_vara_uchar(cdfid, nVarID,
                                                 start,
                                                 count, &ucData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), "%u", ucData);
                    break;
                case NC_USHORT:
                    unsigned short usData;
                    /* status = */ nc_get_vara_ushort(cdfid, nVarID,
                                                  start,
                                                  count, &usData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), "%u", usData);
                    break;
                case NC_UINT:
                {
                    unsigned int unData;
                    /* status = */ nc_get_vara_uint(cdfid, nVarID,
                                                start,
                                                count, &unData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), "%u", unData);
                    break;
                }
                case NC_INT64:
                {
                    long long nData;
                    /* status = */ nc_get_vara_longlong(cdfid, nVarID,
                                                start,
                                                count, &nData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), CPL_FRMT_GIB, nData);
                    break;
                }
                case NC_UINT64:
                {
                    unsigned long long unData;
                    /* status = */ nc_get_vara_ulonglong(cdfid, nVarID,
                                                start,
                                                count, &unData);
                    snprintf(szMetaTemp, sizeof(szMetaTemp), CPL_FRMT_GUIB, unData);
                    break;
                }
#endif
                default:
                    CPLDebug("GDAL_netCDF", "invalid dim %s, type=%d",
                             szMetaTemp, nVarType);
                    break;
            }
        }
        else
        {
            snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", result + 1);
        }

        // Save dimension value.
        // NOTE: removed #original_units as not part of CF-1.

        char szMetaName[NC_MAX_NAME + 1 + 32];
        snprintf(szMetaName, sizeof(szMetaName), "NETCDF_DIM_%s", szVarName);
        SetMetadataItem(szMetaName, szMetaTemp);

        Taken += result * Sum;
    }  // End loop non-spatial dimensions.

    // Get all other metadata.
    int nAtt = 0;
    nc_inq_varnatts(cdfid, nZId, &nAtt);

    for( int i = 0; i < nAtt; i++ )
    {
        char szMetaName[NC_MAX_NAME + 1] = {};
        status = nc_inq_attname(cdfid, nZId, i, szMetaName);
        if( status != NC_NOERR )
            continue;

        char *pszMetaValue = NULL;
        if( NCDFGetAttr(cdfid, nZId, szMetaName, &pszMetaValue) == CE_None )
        {
            SetMetadataItem(szMetaName, pszMetaValue);
        }
        else
        {
            CPLDebug("GDAL_netCDF", "invalid Band metadata %s", szMetaName);
        }

        if( pszMetaValue )
        {
            CPLFree(pszMetaValue);
            pszMetaValue = NULL;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             CheckData()                              */
/************************************************************************/
template <class T>
void netCDFRasterBand::CheckData( void *pImage, void *pImageNC,
                                  size_t nTmpBlockXSize, size_t nTmpBlockYSize,
                                  bool bCheckIsNan )
{
    CPLAssert(pImage != NULL && pImageNC != NULL);

    // If this block is not a full block (in the x axis), we need to re-arrange
    // the data this is because partial blocks are not arranged the same way in
    // netcdf and gdal.
    if( nTmpBlockXSize != static_cast<size_t>(nBlockXSize) )
    {
        T *ptrWrite = (T *)pImage;
        T *ptrRead = (T *)pImageNC;
        for( size_t j = 0;
             j < nTmpBlockYSize;
             j++, ptrWrite += nBlockXSize, ptrRead += nTmpBlockXSize)
        {
            memmove(ptrWrite, ptrRead, nTmpBlockXSize * sizeof(T));
        }
    }

    // Is valid data checking needed or requested?
    if( adfValidRange[0] != dfNoDataValue ||
        adfValidRange[1] != dfNoDataValue ||
        bCheckIsNan )
    {
        for( size_t j = 0; j < nTmpBlockYSize; j++ )
        {
            // k moves along the gdal block, skipping the out-of-range pixels.
            size_t k = j * nBlockXSize;
            for( size_t i = 0; i < nTmpBlockXSize; i++, k++ )
            {
                // Check for nodata and nan.
                // TODO(schwehr): static_casts.
                if( CPLIsEqual((double)((T *)pImage)[k], dfNoDataValue) )
                    continue;
                if( bCheckIsNan && CPLIsNan((double)(((T *)pImage))[k]) )
                {
                    ((T *)pImage)[k] = (T)dfNoDataValue;
                    continue;
                }
                // Check for valid_range.
                if( ((adfValidRange[0] != dfNoDataValue) &&
                     (((T *)pImage)[k] < (T)adfValidRange[0]))
                    ||
                    ((adfValidRange[1] != dfNoDataValue) &&
                     (((T *)pImage)[k] > (T)adfValidRange[1])) )
                {
                    ((T *)pImage)[k] = (T)dfNoDataValue;
                }
            }
        }
    }

    // If minimum longitude is > 180, subtract 360 from all.
    // If not, disable checking for further calls (check just once).
    // Only check first and last block elements since lon must be monotonic.
    const bool bIsSigned = std::numeric_limits<T>::is_signed;
    if( bCheckLongitude && bIsSigned &&
        std::min(((T *)pImage)[0], ((T *)pImage)[nTmpBlockXSize - 1]) > 180.0 )
    {
        for( size_t j = 0; j < nTmpBlockYSize; j++ )
        {
            size_t k = j * nBlockXSize;
            for( size_t i = 0; i < nTmpBlockXSize; i++, k++ )
            {
                if( !CPLIsEqual((double)((T *)pImage)[k], dfNoDataValue) )
                    ((T *)pImage)[k] = static_cast<T>(((T *)pImage)[k] - 360);
            }
        }
    }
    else
    {
        bCheckLongitude = false;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr netCDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void *pImage )

{
    CPLMutexHolderD(&hNCMutex);

    int nd = 0;
    nc_inq_varndims(cdfid, nZId, &nd);

#ifdef NCDF_DEBUG
    if( (nBlockYOff == 0) || (nBlockYOff == nRasterYSize - 1) )
        CPLDebug("GDAL_netCDF",
                 "netCDFRasterBand::IReadBlock( %d, %d, ...) nBand=%d nd=%d",
                 nBlockXOff, nBlockYOff, nBand, nd);
#endif

    // Locate X, Y and Z position in the array.

    size_t start[MAX_NC_DIMS] = {};
    start[nBandXPos] = nBlockXOff * nBlockXSize;

    // Check y order.
    if( static_cast<netCDFDataset *>(poDS)->bBottomUp )
    {
#ifdef NCDF_DEBUG
        if( (nBlockYOff == 0) || (nBlockYOff == nRasterYSize - 1) )
            CPLDebug(
                "GDAL_netCDF",
                "reading bottom-up dataset, nBlockYSize=%d nRasterYSize=%d",
                nBlockYSize, nRasterYSize);
#endif
        // Check block size - return error if not 1.
        // reading upside-down rasters with nBlockYSize!=1 needs further
        // development.  perhaps a simple solution is to invert geotransform and
        // not use bottom-up.
        if( nBlockYSize == 1 )
        {
            start[nBandYPos] = nRasterYSize - 1 - nBlockYOff;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "nBlockYSize = %d, only 1 supported when "
                     "reading bottom-up dataset",
                     nBlockYSize);
            return CE_Failure;
        }
    }
    else
    {
        start[nBandYPos] = nBlockYOff * nBlockYSize;
    }

    size_t edge[MAX_NC_DIMS] = {};

    edge[nBandXPos] = nBlockXSize;
    if( (start[nBandXPos] + edge[nBandXPos]) > (size_t)nRasterXSize )
        edge[nBandXPos] = nRasterXSize - start[nBandXPos];
    edge[nBandYPos] = nBlockYSize;
    if( (start[nBandYPos] + edge[nBandYPos]) > (size_t)nRasterYSize )
        edge[nBandYPos] = nRasterYSize - start[nBandYPos];

#ifdef NCDF_DEBUG
    if( nBlockYOff == 0 || (nBlockYOff == nRasterYSize - 1) )
        CPLDebug("GDAL_netCDF", "start={%ld,%ld} edge={%ld,%ld} bBottomUp=%d",
                  start[nBandXPos], start[nBandYPos],
                  edge[nBandXPos], edge[nBandYPos],
                  ((netCDFDataset *)poDS)->bBottomUp);
#endif

    if( nd == 3 )
    {
        start[panBandZPos[0]] = nLevel;  // z
        edge[panBandZPos[0]] = 1;
    }

    // Compute multidimention band position.
    //
    // BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)
    // if Data[2,3,4,x,y]
    //
    //  BandPos0 = (nBand) / (3*4)
    //  BandPos1 = (nBand - (3*4)) / (4)
    //  BandPos2 = (nBand - (3*4)) % (4)
    if( nd > 3 )
    {
        int Sum = -1;
        int Taken = 0;
        for( int i=0; i < nd - 2 ; i++ )
        {
            if( i != nd - 2 - 1 )
            {
                Sum = 1;
                for( int j = i + 1; j < nd - 2; j++ )
                {
                    Sum *= panBandZLev[j];
                }
                start[panBandZPos[i]] = (int)((nLevel - Taken) / Sum);
                edge[panBandZPos[i]] = 1;
            }
            else
            {
                start[panBandZPos[i]] = (int)((nLevel - Taken) % Sum);
                edge[panBandZPos[i]] = 1;
            }
            Taken += static_cast<int>(start[panBandZPos[i]]) * Sum;
        }
    }

    // Make sure we are in data mode.
    static_cast<netCDFDataset *>(poDS)->SetDefineMode(false);

    // If this block is not a full block in the x axis, we need to
    // re-arrange the data because partial blocks are not arranged the
    // same way in netcdf and gdal, so we first we read the netcdf data at
    // the end of the gdal block buffer then re-arrange rows in CheckData().
    void *pImageNC = pImage;
    if( edge[nBandXPos] != static_cast<size_t>(nBlockXSize) )
    {
        pImageNC = static_cast<GByte *>(pImage)
            + ((nBlockXSize * nBlockYSize - edge[nBandXPos] * edge[nBandYPos])
                * (GDALGetDataTypeSize(eDataType) / 8));
    }

    // Read data according to type.
    int status;
    if( eDataType == GDT_Byte )
    {
        if( bSignedData )
        {
            status = nc_get_vara_schar(cdfid, nZId, start, edge,
                                       static_cast<signed char *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<signed char>(pImage, pImageNC, edge[nBandXPos],
                                       edge[nBandYPos], false);
        }
        else
        {
            status = nc_get_vara_uchar(cdfid, nZId, start, edge,
                                       static_cast<unsigned char *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<unsigned char>(pImage, pImageNC, edge[nBandXPos],
                                         edge[nBandYPos], false);
        }
    }
    else if( eDataType == GDT_Int16 )
    {
        status = nc_get_vara_short(cdfid, nZId, start, edge,
                                   static_cast<short *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<short>(pImage, pImageNC, edge[nBandXPos], edge[nBandYPos],
                             false);
    }
    else if( eDataType == GDT_Int32 )
    {
        if( sizeof(long) == 4 )
        {
            status = nc_get_vara_long(cdfid, nZId, start, edge,
                                      static_cast<long *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<long>(pImage, pImageNC, edge[nBandXPos],
                                edge[nBandYPos], false);
        }
        else
        {
            status = nc_get_vara_int(cdfid, nZId, start, edge,
                                     static_cast<int *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<int>(pImage, pImageNC, edge[nBandXPos],
                               edge[nBandYPos], false);
        }
    }
    else if( eDataType == GDT_Float32 )
    {
        status = nc_get_vara_float(cdfid, nZId, start, edge,
                                   static_cast<float *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<float>(pImage, pImageNC, edge[nBandXPos], edge[nBandYPos],
                             true);
    }
    else if( eDataType == GDT_Float64 )
    {
        status = nc_get_vara_double(cdfid, nZId, start, edge,
                                    static_cast<double *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<double>(pImage, pImageNC, edge[nBandXPos],
                              edge[nBandYPos], true);
    }
#ifdef NETCDF_HAS_NC4
    else if( eDataType == GDT_UInt16 )
    {
        status = nc_get_vara_ushort(cdfid, nZId, start, edge,
                                    static_cast<unsigned short *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<unsigned short>(pImage, pImageNC, edge[nBandXPos],
                                      edge[nBandYPos], false);
    }
    else if( eDataType == GDT_UInt32 )
    {
        status = nc_get_vara_uint(cdfid, nZId, start, edge,
                                  static_cast<unsigned int *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<unsigned int>(pImage, pImageNC, edge[nBandXPos],
                                    edge[nBandYPos], false);
    }
#endif
    else
        status = NC_EBADTYPE;

    if( status != NC_NOERR )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "netCDF scanline fetch failed: #%d (%s)", status,
                 nc_strerror(status));
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr netCDFRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff,
                                      int nBlockYOff,
                                      void *pImage )
{
    CPLMutexHolderD(&hNCMutex);

#ifdef NCDF_DEBUG
    if( nBlockYOff == 0 || (nBlockYOff == nRasterYSize - 1) )
        CPLDebug("GDAL_netCDF",
                 "netCDFRasterBand::IWriteBlock( %d, %d, ...) nBand=%d",
                 nBlockXOff, nBlockYOff, nBand);
#endif

    int nd = 0;
    nc_inq_varndims(cdfid, nZId, &nd);

    // Locate X, Y and Z position in the array.

    size_t start[MAX_NC_DIMS];
    memset(start, 0, sizeof(start));

    start[nBandXPos] = 0;  // x dim can move around in array.
    // check y order.
    if( static_cast<netCDFDataset *>(poDS)->bBottomUp )
    {
        start[nBandYPos] = nRasterYSize - 1 - nBlockYOff;
    }
    else
    {
        start[nBandYPos] = nBlockYOff;  // y
    }

    size_t edge[MAX_NC_DIMS] = {};

    edge[nBandXPos] = nBlockXSize;
    edge[nBandYPos] = 1;

    if( nd == 3 )
    {
        start[panBandZPos[0]] = nLevel;  // z
        edge[panBandZPos[0]] = 1;
    }

    // Compute multidimention band position.
    //
    // BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)
    // if Data[2,3,4,x,y]
    //
    //  BandPos0 = (nBand) / (3*4)
    //  BandPos1 = (nBand - (3*4)) / (4)
    //  BandPos2 = (nBand - (3*4)) % (4)
    if( nd > 3 )
    {
        int Sum = -1;
        int Taken = 0;
        for( int i = 0; i < nd - 2 ; i++ )
        {
            if( i != nd - 2 - 1 )
            {
                Sum = 1;
                for( int j = i + 1; j < nd - 2; j++ )
                {
                    Sum *= panBandZLev[j];
                }
                start[panBandZPos[i]] = (int)((nLevel - Taken) / Sum);
                edge[panBandZPos[i]] = 1;
            }
            else
            {
                start[panBandZPos[i]] = (int)((nLevel - Taken) % Sum);
                edge[panBandZPos[i]] = 1;
            }
            Taken += static_cast<int>(start[panBandZPos[i]]) * Sum;
        }
    }

    // Make sure we are in data mode.
    static_cast<netCDFDataset *>(poDS)->SetDefineMode(false);

    // Copy data according to type.
    int status = 0;
    if( eDataType == GDT_Byte )
    {
        if( bSignedData )
            status = nc_put_vara_schar(cdfid, nZId, start, edge,
                                       static_cast<signed char *>(pImage));
        else
            status = nc_put_vara_uchar(cdfid, nZId, start, edge,
                                       static_cast<unsigned char *>(pImage));
    }
    else if( eDataType == GDT_Int16 )
    {
        status = nc_put_vara_short(cdfid, nZId, start, edge,
                                   static_cast<short *>(pImage));
    }
    else if( eDataType == GDT_Int32 )
    {
        status = nc_put_vara_int(cdfid, nZId, start, edge,
                                 static_cast<int *>(pImage));
    }
    else if( eDataType == GDT_Float32 )
    {
        status = nc_put_vara_float(cdfid, nZId, start, edge,
                                   static_cast<float *>(pImage));
    }
    else if( eDataType == GDT_Float64 )
    {
        status = nc_put_vara_double(cdfid, nZId, start, edge,
                                    static_cast<double *>(pImage));
    }
#ifdef NETCDF_HAS_NC4
    else if( eDataType == GDT_UInt16 &&
             static_cast<netCDFDataset *>(poDS)->eFormat == NCDF_FORMAT_NC4 )
    {
        status = nc_put_vara_ushort(cdfid, nZId, start, edge,
                                    static_cast<unsigned short *>(pImage));
    }
    else if( eDataType == GDT_UInt32 &&
             static_cast<netCDFDataset *>(poDS)->eFormat == NCDF_FORMAT_NC4 )
    {
        status = nc_put_vara_uint(cdfid, nZId, start, edge,
                                  static_cast<unsigned int *>(pImage));
    }
#endif
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The NetCDF driver does not support GDAL data type %d",
                 eDataType);
        status = NC_EBADTYPE;
    }
    NCDF_ERR(status);

    if( status != NC_NOERR )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "netCDF scanline write failed: %s", nc_strerror(status));
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              netCDFDataset                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           netCDFDataset()                            */
/************************************************************************/

netCDFDataset::netCDFDataset() :
    // Basic dataset vars.
    cdfid(-1),
    papszSubDatasets(NULL),
    papszMetadata(NULL),
    bBottomUp(true),
    eFormat(NCDF_FORMAT_NONE),
    bIsGdalFile(false),
    bIsGdalCfFile(false),

    pszCFProjection(NULL),
    pszCFCoordinates(NULL),
    eMultipleLayerBehaviour(SINGLE_LAYER),

    // projection/GT.
    pszProjection(NULL),
    nXDimID(-1),
    nYDimID(-1),
    bIsProjected(false),
    bIsGeographic(false),  // Can be not projected, and also not geographic

    // State vars.
    bDefineMode(true),
    bSetProjection(false),
    bSetGeoTransform(false),
    bAddedProjectionVars(false),
    bAddedGridMappingRef(false),

    // Create vars.
    papszCreationOptions(NULL),
    eCompress(NCDF_COMPRESS_NONE),
    nZLevel(NCDF_DEFLATE_LEVEL),
#ifdef NETCDF_HAS_NC4
    bChunking(false),
#endif
    nCreateMode(NC_CLOBBER),
    bSignedData(true),
    nLayers(0),
    papoLayers(NULL)
{
    // Projection/GT.
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~netCDFDataset()                           */
/************************************************************************/

netCDFDataset::~netCDFDataset()

{
    CPLMutexHolderD(&hNCMutex);

#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF",
             "netCDFDataset::~netCDFDataset(), cdfid=%d filename=%s", cdfid,
             osFilename.c_str());
#endif

    // Ensure projection is written if GeoTransform OR Projection are missing.
    if( GetAccess() == GA_Update && !bAddedProjectionVars )
    {
        if( bSetProjection && !bSetGeoTransform )
            AddProjectionVars();
        else if( bSetGeoTransform && !bSetProjection )
            AddProjectionVars();
    }

    FlushCache();

    for(int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);

    for(size_t i = 0; i < apoVectorDatasets.size(); i++)
        delete apoVectorDatasets[i];

    // Make sure projection variable is written to band variable.
    if( GetAccess() == GA_Update && !bAddedGridMappingRef )
        AddGridMappingRef();

    CSLDestroy(papszMetadata);
    CSLDestroy(papszSubDatasets);
    CSLDestroy(papszCreationOptions);

    CPLFree(pszProjection);
    CPLFree(pszCFProjection);
    CPLFree(pszCFCoordinates);

    if( cdfid > 0 )
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "calling nc_close( %d)", cdfid);
#endif
        int status = nc_close(cdfid);
        NCDF_ERR(status);
    }
}

/************************************************************************/
/*                            SetDefineMode()                           */
/************************************************************************/
bool netCDFDataset::SetDefineMode( bool bNewDefineMode )
{
    // Do nothing if already in new define mode
    // or if dataset is in read-only mode.
    if( bDefineMode == bNewDefineMode || GetAccess() == GA_ReadOnly )
        return true;

    CPLDebug("GDAL_netCDF", "SetDefineMode(%d) old=%d",
             static_cast<int>(bNewDefineMode), static_cast<int>(bDefineMode));

    bDefineMode = bNewDefineMode;

    int status;
    if( bDefineMode )
        status = nc_redef(cdfid);
    else
        status = nc_enddef(cdfid);

    NCDF_ERR(status);
    return status == NC_NOERR;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **netCDFDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(), TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/
char **netCDFDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != NULL && STARTS_WITH_CI(pszDomain, "SUBDATASETS") )
        return papszSubDatasets;

    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *netCDFDataset::GetProjectionRef()
{
    if( bSetProjection )
        return pszProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *netCDFDataset::SerializeToXML( const char *pszUnused )

{
    // Overridden from GDALPamDataset to add only band histogram
    // and statistics. See bug #4244.

    if( psPam == NULL )
        return NULL;

    // Setup root node and attributes.
    CPLXMLNode *psDSTree = CPLCreateXMLNode(NULL, CXT_Element, "PAMDataset");

    // Process bands.
    for( int iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        netCDFRasterBand *poBand =
            static_cast<netCDFRasterBand *>(GetRasterBand(iBand + 1));

        if( poBand == NULL || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        CPLXMLNode *psBandTree = poBand->SerializeToXML(pszUnused);

        if( psBandTree != NULL )
            CPLAddXMLChild(psDSTree, psBandTree);
    }

    // We don't want to return anything if we had no metadata to attach.
    if( psDSTree->psChild == NULL )
    {
        CPLDestroyXMLNode(psDSTree);
        psDSTree = NULL;
    }

    return psDSTree;
}

/************************************************************************/
/*                           FetchCopyParm()                            */
/************************************************************************/

double netCDFDataset::FetchCopyParm( const char *pszGridMappingValue,
                                     const char *pszParm, double dfDefault )

{
    char szTemp[256] = {};
    snprintf(szTemp, sizeof(szTemp), "%s#%s", pszGridMappingValue, pszParm);
    const char *pszValue = CSLFetchNameValue(papszMetadata, szTemp);

    if( pszValue )
    {
        return CPLAtofM(pszValue);
    }

    return dfDefault;
}

/************************************************************************/
/*                           FetchStandardParallels()                   */
/************************************************************************/

char **netCDFDataset::FetchStandardParallels( const char *pszGridMappingValue )
{
    char szTemp[256] = {};
    // cf-1.0 tags
    snprintf(szTemp, sizeof(szTemp), "%s#%s", pszGridMappingValue,
             CF_PP_STD_PARALLEL);
    const char *pszValue = CSLFetchNameValue(papszMetadata, szTemp);

    char **papszValues = NULL;
    if( pszValue != NULL )
    {
        papszValues = NCDFTokenizeArray(pszValue);
    }
    // Try gdal tags.
    else
    {
        snprintf(szTemp, sizeof(szTemp),
                 "%s#%s", pszGridMappingValue, CF_PP_STD_PARALLEL_1);

        pszValue = CSLFetchNameValue(papszMetadata, szTemp);

        if( pszValue != NULL )
            papszValues = CSLAddString(papszValues, pszValue);

        snprintf(szTemp, sizeof(szTemp),
                 "%s#%s", pszGridMappingValue, CF_PP_STD_PARALLEL_2);

        pszValue = CSLFetchNameValue(papszMetadata, szTemp);

        if( pszValue != NULL )
            papszValues = CSLAddString(papszValues, pszValue);
    }

    return papszValues;
}

/************************************************************************/
/*                      SetProjectionFromVar()                          */
/************************************************************************/
void netCDFDataset::SetProjectionFromVar( int nVarId, bool bReadSRSOnly )
{
    bool bGotGeogCS = false;
    bool bGotCfSRS = false;
    bool bGotGdalSRS = false;
    bool bGotCfGT = false;
    bool bGotGdalGT = false;

    // These values from CF metadata.
    OGRSpatialReference oSRS;
    double *pdfXCoord = NULL;
    double *pdfYCoord = NULL;
    char szDimNameX[NC_MAX_NAME + 1];
    // char szDimNameY[NC_MAX_NAME + 1];
    size_t xdim = nRasterXSize;
    size_t ydim = nRasterYSize;

    // These values from GDAL metadata.
    const char *pszWKT = NULL;
    const char *pszGeoTransform = NULL;

    netCDFDataset *poDS = this;  // Perhaps this should be removed for clarity.

    CPLDebug("GDAL_netCDF", "\n=====\nSetProjectionFromVar( %d)", nVarId);

    // Get x/y range information.

    // Temp variables to use in SetGeoTransform() and SetProjection().
    double adfTempGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    char *pszTempProjection = NULL;

    if( !bReadSRSOnly && (xdim == 1 || ydim == 1) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "1-pixel width/height files not supported, "
                 "xdim: %ld ydim: %ld",
                 static_cast<long>(xdim), static_cast<long>(ydim));
        return;
    }

    // Look for grid_mapping metadata.

    char szGridMappingName[NC_MAX_NAME + 1];
    strcpy(szGridMappingName, "");

    char szGridMappingValue[NC_MAX_NAME + 1];
    strcpy(szGridMappingValue, "");

    char szVarName[NC_MAX_NAME + 1];
    szVarName[0] = '\0';
    {
        int status = nc_inq_varname(cdfid, nVarId, szVarName);
        NCDF_ERR(status);
    }
    char szTemp[NC_MAX_NAME + 1];
    snprintf(szTemp, sizeof(szTemp), "%s#%s", szVarName, CF_GRD_MAPPING);

    const char *pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
    if( pszValue )
    {
        snprintf(szGridMappingName, sizeof(szGridMappingName), "%s", szTemp);
        snprintf(szGridMappingValue, sizeof(szGridMappingValue),
                 "%s", pszValue);
    }

    if( !EQUAL(szGridMappingValue, "") )
    {
        // Read grid_mapping metadata.
        int nVarProjectionID = -1;
        nc_inq_varid(cdfid, szGridMappingValue, &nVarProjectionID);
        poDS->ReadAttributes(cdfid, nVarProjectionID);

        // Look for GDAL spatial_ref and GeoTransform within grid_mapping.
        CPLDebug("GDAL_netCDF", "got grid_mapping %s", szGridMappingValue);
        snprintf(szTemp, sizeof(szTemp),
                 "%s#%s", szGridMappingValue, NCDF_SPATIAL_REF);

        pszWKT = CSLFetchNameValue(poDS->papszMetadata, szTemp);

        if( pszWKT != NULL )
        {
            snprintf(szTemp, sizeof(szTemp),
                     "%s#%s", szGridMappingValue, NCDF_GEOTRANSFORM);
            pszGeoTransform = CSLFetchNameValue(poDS->papszMetadata, szTemp);
        }
    }

    // Get information about the file.
    //
    // Was this file created by the GDAL netcdf driver?
    // Was this file created by the newer (CF-conformant) driver?
    //
    // 1) If GDAL netcdf metadata is set, and version >= 1.9,
    //    it was created with the new driver
    // 2) Else, if spatial_ref and GeoTransform are present in the
    //    grid_mapping variable, it was created by the old driver
    pszValue = CSLFetchNameValue(poDS->papszMetadata, "NC_GLOBAL#GDAL");

    if( pszValue && NCDFIsGDALVersionGTE(pszValue, 1900))
    {
        bIsGdalFile = true;
        bIsGdalCfFile = true;
    }
    else if( pszWKT != NULL && pszGeoTransform != NULL )
    {
        bIsGdalFile = true;
        bIsGdalCfFile = false;
    }

    // Set default bottom-up default value.
    // Y axis dimension and absence of GT can modify this value.
    // Override with Config option GDAL_NETCDF_BOTTOMUP.

    // New driver is bottom-up by default.
    if( bIsGdalFile && !bIsGdalCfFile )
        poDS->bBottomUp = false;
    else
        poDS->bBottomUp = true;

    CPLDebug("GDAL_netCDF", "bIsGdalFile=%d bIsGdalCfFile=%d bBottomUp=%d",
             static_cast<int>(bIsGdalFile), static_cast<int>(bIsGdalCfFile),
             static_cast<int>(bBottomUp));

    // Look for dimension: lon.

    memset(szDimNameX, '\0', sizeof(szDimNameX));
    // memset(szDimNameY, '\0', sizeof(szDimNameY));

    if( !bReadSRSOnly )
    {
        for( unsigned int i = 0;
             i < strlen(poDS->papszDimName[poDS->nXDimID]) && i < 3;
             i++ )
        {
            szDimNameX[i] =
                (char)tolower((poDS->papszDimName[poDS->nXDimID])[i]);
        }
        szDimNameX[3] = '\0';
        // for( unsigned int i = 0;
        //      (i < strlen(poDS->papszDimName[poDS->nYDimID])
        //                        && i < 3 ); i++ ) {
        //    szDimNameY[i]=(char)tolower((poDS->papszDimName[poDS->nYDimID])[i]);
        // }
        // szDimNameY[3] = '\0';
    }

    // Read grid_mapping information and set projections.

    if( !EQUAL(szGridMappingName, "") )
    {

        snprintf(szTemp, sizeof(szTemp),
                 "%s#%s", szGridMappingValue, CF_GRD_MAPPING_NAME);
        pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);

        if( pszValue != NULL )
        {
            // Check for datum/spheroid information.
            double dfEarthRadius = poDS->FetchCopyParm(
                szGridMappingValue, CF_PP_EARTH_RADIUS, -1.0);

            const double dfLonPrimeMeridian = poDS->FetchCopyParm(
                szGridMappingValue, CF_PP_LONG_PRIME_MERIDIAN, 0.0);

            const char *pszPMName = NULL;

            // Should try to find PM name from its value if not Greenwich.
            if( !CPLIsEqual(dfLonPrimeMeridian, 0.0) )
                pszPMName = "unknown";

            double dfInverseFlattening = poDS->FetchCopyParm(
                szGridMappingValue, CF_PP_INVERSE_FLATTENING, -1.0);

            double dfSemiMajorAxis = poDS->FetchCopyParm(
                szGridMappingValue, CF_PP_SEMI_MAJOR_AXIS, -1.0);

            const double dfSemiMinorAxis = poDS->FetchCopyParm(
                szGridMappingValue, CF_PP_SEMI_MINOR_AXIS, -1.0);

            // See if semi-major exists if radius doesn't.
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = dfSemiMajorAxis;

            // If still no radius, check old tag.
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_EARTH_RADIUS_OLD, -1.0);

            // Has radius value.
            if( dfEarthRadius > 0.0 )
            {
                // Check for inv_flat tag.
                if( dfInverseFlattening < 0.0 )
                {
                    // No inv_flat tag, check for semi_minor.
                    if( dfSemiMinorAxis < 0.0 )
                    {
                        // No way to get inv_flat, use sphere.
                        oSRS.SetGeogCS("unknown",
                                        NULL,
                                        "Sphere",
                                        dfEarthRadius, 0.0,
                                        pszPMName, dfLonPrimeMeridian);
                        bGotGeogCS = true;
                    }
                    else
                    {
                        if( dfSemiMajorAxis < 0.0 )
                            dfSemiMajorAxis = dfEarthRadius;
                        //set inv_flat using semi_minor/major
                        dfInverseFlattening = OSRCalcInvFlattening(dfSemiMajorAxis, dfSemiMinorAxis);

                        oSRS.SetGeogCS("unknown",
                                        NULL,
                                        "Spheroid",
                                        dfEarthRadius, dfInverseFlattening,
                                        pszPMName, dfLonPrimeMeridian);
                        bGotGeogCS = true;
                    }
                }
                else
                {
                    oSRS.SetGeogCS("unknown",
                                    NULL,
                                    "Spheroid",
                                    dfEarthRadius, dfInverseFlattening,
                                        pszPMName, dfLonPrimeMeridian);
                    bGotGeogCS = true;
                }

                if( bGotGeogCS )
                    CPLDebug("GDAL_netCDF", "got spheroid from CF: (%f , %f)",
                             dfEarthRadius, dfInverseFlattening);
            }
            // no radius, set as wgs84 as default?
            else
            {
                // This would be too indiscriminate.  But we should set
                // it if we know the data is geographic.
                // oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Transverse Mercator.
            double dfCenterLat = 0.0;
            double dfCenterLon = 0.0;
            double dfScale = 1.0;
            double dfFalseEasting = 0.0;
            double dfFalseNorthing = 0.0;

            if( EQUAL(pszValue, CF_PT_TM) )
            {
                dfScale = poDS->FetchCopyParm(szGridMappingValue,
                                              CF_PP_SCALE_FACTOR_MERIDIAN, 1.0);

                dfCenterLon = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                dfCenterLat = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetTM(dfCenterLat,
                           dfCenterLon,
                           dfScale,
                           dfFalseEasting,
                           dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Albers Equal Area.
            double dfStdP1 = 0.0;
            double dfStdP2 = 0.0;

            if( EQUAL(pszValue, CF_PT_AEA) )
            {
                char **papszStdParallels = NULL;

                dfCenterLon =
                    poDS->FetchCopyParm(szGridMappingValue,
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                dfFalseEasting =
                    poDS->FetchCopyParm(szGridMappingValue,
                                         CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing =
                    poDS->FetchCopyParm(szGridMappingValue,
                                         CF_PP_FALSE_NORTHING, 0.0);

                papszStdParallels =
                    FetchStandardParallels(szGridMappingValue);

                if( papszStdParallels != NULL )
                {
                    if( CSLCount(papszStdParallels) == 1 )
                    {
                        // TODO CF-1 standard says it allows AEA to be encoded
                        // with only 1 standard parallel.  How should this
                        // actually map to a 2StdP OGC WKT version?
                        CPLError(
                            CE_Warning, CPLE_NotSupported,
                            "NetCDF driver import of AEA-1SP is not tested, "
                            "using identical std. parallels.");
                        dfStdP1 = CPLAtofM(papszStdParallels[0]);
                        dfStdP2 = dfStdP1;
                    }
                    else if( CSLCount(papszStdParallels) == 2 )
                    {
                        dfStdP1 = CPLAtofM(papszStdParallels[0]);
                        dfStdP2 = CPLAtofM(papszStdParallels[1]);
                    }
                }
                // Old default.
                else
                {
                    dfStdP1 =
                        poDS->FetchCopyParm(szGridMappingValue,
                                             CF_PP_STD_PARALLEL_1, 0.0);

                    dfStdP2 =
                        poDS->FetchCopyParm(szGridMappingValue,
                                             CF_PP_STD_PARALLEL_2, 0.0);
                }

                dfCenterLat =
                    poDS->FetchCopyParm(szGridMappingValue,
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0);

                bGotCfSRS = true;
                oSRS.SetACEA(dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                             dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                CSLDestroy(papszStdParallels);
            }

            // Cylindrical Equal Area
            else if( EQUAL(pszValue, CF_PT_CEA) || EQUAL(pszValue, CF_PT_LCEA) )
            {
                char **papszStdParallels =
                    FetchStandardParallels(szGridMappingValue);

                if( papszStdParallels != NULL )
                {
                    dfStdP1 = CPLAtofM(papszStdParallels[0]);
                }
                else
                {
                    // TODO: Add support for 'scale_factor_at_projection_origin'
                    // variant to standard parallel.  Probably then need to calc
                    // a std parallel equivalent.
                    CPLError(
                        CE_Failure, CPLE_NotSupported,
                        "NetCDF driver does not support import of CF-1 LCEA "
                        "'scale_factor_at_projection_origin' variant yet.");
                }

                const double dfCentralMeridian = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetCEA(dfStdP1, dfCentralMeridian,
                             dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                CSLDestroy(papszStdParallels);
            }

            // lambert_azimuthal_equal_area.
            else if( EQUAL(pszValue, CF_PT_LAEA) )
            {
                dfCenterLon = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                dfCenterLat = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetLAEA(dfCenterLat, dfCenterLon,
                              dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                if( oSRS.GetAttrValue("DATUM") != NULL &&
                    EQUAL(oSRS.GetAttrValue("DATUM"), "WGS_1984") )
                {
                    oSRS.SetProjCS("LAEA (WGS84)");
                }
            }

            // Azimuthal Equidistant.
            else if( EQUAL(pszValue, CF_PT_AE) )
            {
                dfCenterLon = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                dfCenterLat = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetAE( dfCenterLat, dfCenterLon,
                            dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Lambert conformal conic.
            else if( EQUAL(pszValue, CF_PT_LCC) )
            {
                char **papszStdParallels = NULL;

                dfCenterLon = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                dfCenterLat = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                papszStdParallels = FetchStandardParallels(szGridMappingValue);

                // 2SP variant.
                if( CSLCount(papszStdParallels) == 2 )
                {
                    dfStdP1 = CPLAtofM(papszStdParallels[0]);
                    dfStdP2 = CPLAtofM(papszStdParallels[1]);
                    oSRS.SetLCC(dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                                dfFalseEasting, dfFalseNorthing);
                }
                // 1SP variant (with standard_parallel or center lon).
                // See comments in netcdfdataset.h for this projection.
                else
                {
                    dfScale = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_SCALE_FACTOR_ORIGIN, -1.0);

                    // CF definition, without scale factor.
                    if( CPLIsEqual(dfScale, -1.0) )
                    {
                        // With standard_parallel.
                        if( CSLCount(papszStdParallels) == 1 )
                            dfStdP1 = CPLAtofM(papszStdParallels[0]);
                        // With center lon instead.
                        else
                            dfStdP1 = dfCenterLat;
                        // dfStdP2 = dfStdP1;

                        // Test if we should actually compute scale factor.
                        if( !CPLIsEqual(dfStdP1, dfCenterLat) )
                        {
                            CPLError(
                                CE_Warning, CPLE_NotSupported,
                                "NetCDF driver import of LCC-1SP with "
                                "standard_parallel1 != "
                                "latitude_of_projection_origin "
                                "(which forces a computation of scale_factor) "
                                "is experimental (bug #3324)");
                            // Use Snyder eq. 15-4 to compute dfScale from
                            // dfStdP1 and dfCenterLat.  Only tested for
                            // dfStdP1=dfCenterLat and (25,26), needs more data
                            // for testing.  Other option: use the 2SP variant -
                            // how to compute new standard parallels?
                            dfScale =
                                (cos(dfStdP1) *
                                 pow(tan(M_PI/4 + dfStdP1/2),
                                     sin(dfStdP1))) /
                                (cos(dfCenterLat) *
                                 pow(tan(M_PI/4 + dfCenterLat/2),
                                     sin(dfCenterLat)));
                        }
                        // Default is 1.0.
                        else
                        {
                            dfScale = 1.0;
                        }

                        oSRS.SetLCC1SP(dfCenterLat, dfCenterLon, dfScale,
                                       dfFalseEasting, dfFalseNorthing);
                        // Store dfStdP1 so we can output it to CF later.
                        oSRS.SetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,
                                             dfStdP1);
                    }
                    // OGC/PROJ.4 definition with scale factor.
                    else
                    {
                        oSRS.SetLCC1SP(dfCenterLat, dfCenterLon, dfScale,
                                       dfFalseEasting, dfFalseNorthing);
                    }
                }

                bGotCfSRS = true;
                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                CSLDestroy(papszStdParallels);
            }

            // Is this Latitude/Longitude Grid explicitly?

            else if( EQUAL(pszValue, CF_PT_LATITUDE_LONGITUDE) )
            {
                bGotCfSRS = true;
                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Mercator.
            else if( EQUAL(pszValue, CF_PT_MERCATOR) )
            {

                char **papszStdParallels = NULL;

                // If there is a standard_parallel, know it is Mercator 2SP.
                papszStdParallels = FetchStandardParallels(szGridMappingValue);

                if(NULL != papszStdParallels)
                {
                    // CF-1 Mercator 2SP always has lat centered at equator.
                    dfStdP1 = CPLAtofM(papszStdParallels[0]);

                    dfCenterLat = 0.0;

                    dfCenterLon = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_LON_PROJ_ORIGIN, 0.0);

                    dfFalseEasting = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_FALSE_EASTING, 0.0);

                    dfFalseNorthing = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                    oSRS.SetMercator2SP(dfStdP1, dfCenterLat, dfCenterLon,
                                        dfFalseEasting, dfFalseNorthing);
                }
                else
                {
                    dfCenterLon = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_LON_PROJ_ORIGIN, 0.0);

                    dfCenterLat = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_LAT_PROJ_ORIGIN, 0.0);

                    dfScale = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

                    dfFalseEasting = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_FALSE_EASTING, 0.0);

                    dfFalseNorthing = poDS->FetchCopyParm(
                        szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                    oSRS.SetMercator(dfCenterLat, dfCenterLon, dfScale,
                                     dfFalseEasting, dfFalseNorthing);
                }

                bGotCfSRS = true;

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                CSLDestroy(papszStdParallels);
            }

            // Orthographic.
            else if( EQUAL (pszValue, CF_PT_ORTHOGRAPHIC) )
            {
                dfCenterLon = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                dfCenterLat = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;

                oSRS.SetOrthographic(dfCenterLat, dfCenterLon,
                                      dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Polar Stereographic.
            else if( EQUAL(pszValue, CF_PT_POLAR_STEREO) )
            {
                char **papszStdParallels = NULL;

                dfScale = poDS->FetchCopyParm(szGridMappingValue,
                                              CF_PP_SCALE_FACTOR_ORIGIN, -1.0);

                papszStdParallels = FetchStandardParallels(szGridMappingValue);

                // CF allows the use of standard_parallel (lat_ts) OR
                // scale_factor (k0), make sure we have standard_parallel, using
                // Snyder eq. 22-7 with k=1 and lat=standard_parallel.
                if( papszStdParallels != NULL )
                {
                    dfStdP1 = CPLAtofM(papszStdParallels[0]);
                    // Compute scale_factor from standard_parallel.
                    // This creates WKT that is inconsistent, don't write for
                    // now.  Also, proj4 does not seem to use this parameter.
                    // dfScale =
                    //     (1.0 + fabs(sin(dfStdP1 * M_PI / 180.0))) / 2.0;
                }
                else
                {
                    if( !CPLIsEqual(dfScale, -1.0) )
                    {
                        // Compute standard_parallel from scale_factor.
                        dfStdP1 = asin(2 * dfScale - 1) * 180.0 / M_PI;

                        // Fetch latitude_of_projection_origin (+90/-90).
                        // Used here for the sign of standard_parallel.
                        double dfLatProjOrigin = poDS->FetchCopyParm(
                            szGridMappingValue, CF_PP_LAT_PROJ_ORIGIN, 0.0);
                        if( !CPLIsEqual(dfLatProjOrigin, 90.0) &&
                            !CPLIsEqual(dfLatProjOrigin, -90.0) )
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                     "Polar Stereographic must have a %s "
                                     "parameter equal to +90 or -90.",
                                     CF_PP_LAT_PROJ_ORIGIN);
                            dfLatProjOrigin = 90.0;
                        }
                        if( CPLIsEqual(dfLatProjOrigin, -90.0) )
                            dfStdP1 = -dfStdP1;
                    }
                    else
                    {
                        dfStdP1 = 0.0;  // Just to avoid warning at compilation.
                        CPLError(
                            CE_Failure, CPLE_NotSupported,
                            "The NetCDF driver does not support import "
                            "of CF-1 Polar stereographic "
                            "without standard_parallel and "
                            "scale_factor_at_projection_origin parameters.");
                    }
                }

                // Set scale to default value 1.0 if it was not set.
                if( CPLIsEqual(dfScale, -1.0) )
                    dfScale = 1.0;

                dfCenterLon = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_VERT_LONG_FROM_POLE, 0.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                // Map CF CF_PP_STD_PARALLEL_1 to WKT SRS_PP_LATITUDE_OF_ORIGIN.
                oSRS.SetPS(dfStdP1, dfCenterLon, dfScale,
                            dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                CSLDestroy(papszStdParallels);
            }

            // Stereographic.
            else if( EQUAL(pszValue, CF_PT_STEREO) )
            {
                dfCenterLon = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                dfCenterLat = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                dfScale = poDS->FetchCopyParm(szGridMappingValue,
                                              CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetStereographic(dfCenterLat, dfCenterLon, dfScale,
                                      dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Geostationary.
            else if( EQUAL(pszValue, CF_PT_GEOS) )
            {
                dfCenterLon = poDS->FetchCopyParm(szGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                double dfSatelliteHeight =
                    poDS->FetchCopyParm(szGridMappingValue,
                                        CF_PP_PERSPECTIVE_POINT_HEIGHT, 35785831.0);

                snprintf(szTemp, sizeof(szTemp), "%s#%s", szGridMappingValue,
                         CF_PP_SWEEP_ANGLE_AXIS);
                const char *pszSweepAxisAngle =
                    CSLFetchNameValue(papszMetadata, szTemp);

                dfFalseEasting = poDS->FetchCopyParm(szGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                dfFalseNorthing = poDS->FetchCopyParm(
                    szGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetGEOS(dfCenterLon, dfSatelliteHeight,
                              dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                if( pszSweepAxisAngle != NULL && EQUAL(pszSweepAxisAngle, "x") )
                {
                    char *pszProj4 = NULL;
                    oSRS.exportToProj4(&pszProj4);
                    CPLString osProj4 = pszProj4;
                    osProj4 += " +sweep=x";
                    oSRS.SetExtension(oSRS.GetRoot()->GetValue(),
                                      "PROJ4", osProj4);
                    CPLFree(pszProj4);
                }
            }

        // Is this Latitude/Longitude Grid, default?
        }
        else if( EQUAL(szDimNameX, NCDF_DIMNAME_LON) )
        {
            oSRS.SetWellKnownGeogCS("WGS84");
        }
        else
        {
            // This would be too indiscriminate.  But we should set
            // it if we know the data is geographic.
            // oSRS.SetWellKnownGeogCS("WGS84");
        }
    }
    // Read projection coordinates.

    int nVarDimXID = -1;
    int nVarDimYID = -1;
    if( !bReadSRSOnly )
    {
        nc_inq_varid(cdfid, poDS->papszDimName[nXDimID], &nVarDimXID);
        nc_inq_varid(cdfid, poDS->papszDimName[nYDimID], &nVarDimYID);
    }

    if( !bReadSRSOnly && (nVarDimXID != -1) && (nVarDimYID != -1) )
    {
        pdfXCoord = static_cast<double *>(CPLCalloc(xdim, sizeof(double)));
        pdfYCoord = static_cast<double *>(CPLCalloc(ydim, sizeof(double)));

        size_t start[2] = { 0, 0 };
        size_t edge[2] = { xdim, 0 };
        int status =
            nc_get_vara_double(cdfid, nVarDimXID, start, edge, pdfXCoord);
        NCDF_ERR(status);

        edge[0] = ydim;
        status = nc_get_vara_double(cdfid, nVarDimYID, start, edge, pdfYCoord);
        NCDF_ERR(status);

        // Check for bottom-up from the Y-axis order.
        // See bugs #4284 and #4251.
        poDS->bBottomUp = (pdfYCoord[0] <= pdfYCoord[1]);

        CPLDebug("GDAL_netCDF", "set bBottomUp = %d from Y axis",
                 static_cast<int>(poDS->bBottomUp));

        // Convert ]180,360] longitude values to [-180,180].
        if( NCDFIsVarLongitude(cdfid, nVarDimXID, NULL) &&
            CPLTestBool(CPLGetConfigOption("GDAL_NETCDF_CENTERLONG_180",
                                           "YES")) )
        {
            // If minimum longitude is > 180, subtract 360 from all.
            if( std::min(pdfXCoord[0], pdfXCoord[xdim - 1]) > 180.0 )
            {
                for( size_t i = 0; i < xdim; i++ )
                        pdfXCoord[i] -= 360;
            }
        }

        // Set Projection from CF.
        if( bGotGeogCS || bGotCfSRS )
        {
            // Set SRS Units.

            // Check units for x and y.
            if( oSRS.IsProjected() )
            {
                snprintf(szTemp, sizeof(szTemp),
                         "%s#units", poDS->papszDimName[nXDimID]);
                const char *pszUnitsX =
                    CSLFetchNameValue(poDS->papszMetadata, szTemp);

                snprintf(szTemp, sizeof(szTemp),
                         "%s#units", poDS->papszDimName[nYDimID]);
                const char *pszUnitsY =
                    CSLFetchNameValue(poDS->papszMetadata, szTemp);

                const char *pszUnits = NULL;

                // TODO: What to do if units are not equal in X and Y.
                if( (pszUnitsX != NULL) && (pszUnitsY != NULL) &&
                     EQUAL(pszUnitsX, pszUnitsY) )
                    pszUnits = pszUnitsX;

                // Add units to PROJCS.
                if( pszUnits != NULL && !EQUAL(pszUnits, "") )
                {
                    CPLDebug("GDAL_netCDF", "units=%s", pszUnits);
                    if( EQUAL(pszUnits, "m") )
                    {
                        oSRS.SetLinearUnits("metre", 1.0);
                        oSRS.SetAuthority("PROJCS|UNIT", "EPSG", 9001);
                    }
                    else if( EQUAL(pszUnits, "km") )
                    {
                        oSRS.SetLinearUnits("kilometre", 1000.0);
                        oSRS.SetAuthority("PROJCS|UNIT", "EPSG", 9036);
                    }
                    else if( EQUAL(pszUnits, "US_survey_foot") ||
                             EQUAL(pszUnits, "US_survey_feet") )
                    {
                        oSRS.SetLinearUnits("US survey foot",
                                            CPLAtof(SRS_UL_US_FOOT_CONV));
                        oSRS.SetAuthority("PROJCS|UNIT", "EPSG", 9003);
                    }
                    // TODO: Check for other values.
                    // else
                    //     oSRS.SetLinearUnits(pszUnits, 1.0);
                }
            }
            else if( oSRS.IsGeographic() )
            {
                oSRS.SetAngularUnits(CF_UNITS_D, CPLAtof(SRS_UA_DEGREE_CONV));
                oSRS.SetAuthority("GEOGCS|UNIT", "EPSG", 9122);
            }

            // Set projection.
            oSRS.exportToWkt(&pszTempProjection);
            CPLDebug("GDAL_netCDF", "setting WKT from CF");
            SetProjection(pszTempProjection);
            CPLFree(pszTempProjection);

            if( !bGotCfGT )
                CPLDebug("GDAL_netCDF", "got SRS but no geotransform from CF!");
        }

        // Is pixel spacing uniform across the map?

        // Check Longitude.

        bool bLonSpacingOK = false;
        int nSpacingBegin = 0;
        int nSpacingMiddle = 0;
        int nSpacingLast = 0;

        if( xdim == 2 )
        {
            bLonSpacingOK = true;
        }
        else
        {
            nSpacingBegin = static_cast<int>(
                poDS->rint((pdfXCoord[1] - pdfXCoord[0]) * 1000));

            nSpacingMiddle = static_cast<int>(poDS->rint(
                (pdfXCoord[xdim / 2 + 1] - pdfXCoord[xdim / 2]) * 1000));

            nSpacingLast = static_cast<int>(
                poDS->rint((pdfXCoord[xdim - 1] - pdfXCoord[xdim - 2]) * 1000));

            CPLDebug("GDAL_netCDF",
                     "xdim: %ld nSpacingBegin: %d nSpacingMiddle: %d "
                     "nSpacingLast: %d",
                     static_cast<long>(xdim),
                     nSpacingBegin, nSpacingMiddle, nSpacingLast);
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF",
                     "xcoords: %f %f %f %f %f %f",
                     pdfXCoord[0], pdfXCoord[1],
                     pdfXCoord[xdim / 2], pdfXCoord[(xdim / 2) + 1],
                     pdfXCoord[xdim - 2], pdfXCoord[xdim - 1]);
#endif

            if( (abs(abs(nSpacingBegin) - abs(nSpacingLast))  <= 1) &&
                (abs(abs(nSpacingBegin) - abs(nSpacingMiddle)) <= 1) &&
                (abs(abs(nSpacingMiddle) - abs(nSpacingLast)) <= 1) )
            {
                bLonSpacingOK = true;
            }
        }

        if( bLonSpacingOK == false )
        {
            CPLDebug("GDAL_netCDF", "Longitude is not equally spaced.");
        }

        // Check Latitude.
        bool bLatSpacingOK = false;

        if( ydim == 2 )
        {
            bLatSpacingOK = true;
        }
        else
        {
            nSpacingBegin = static_cast<int>(
                poDS->rint((pdfYCoord[1] - pdfYCoord[0]) * 1000));

            nSpacingMiddle = static_cast<int>(poDS->rint(
                (pdfYCoord[ydim / 2 + 1] - pdfYCoord[ydim / 2]) * 1000));

            nSpacingLast = static_cast<int>(
                poDS->rint((pdfYCoord[ydim - 1] - pdfYCoord[ydim - 2]) * 1000));

            CPLDebug("GDAL_netCDF",
                     "ydim: %ld nSpacingBegin: %d nSpacingMiddle: %d nSpacingLast: %d",
                     (long)ydim, nSpacingBegin, nSpacingMiddle, nSpacingLast);
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF",
                     "ycoords: %f %f %f %f %f %f",
                     pdfYCoord[0], pdfYCoord[1], pdfYCoord[ydim / 2], pdfYCoord[(ydim / 2) + 1],
                     pdfYCoord[ydim - 2], pdfYCoord[ydim - 1]);
#endif

            // For Latitude we allow an error of 0.1 degrees for gaussian
            // gridding (only if this is not a projected SRS).

            if( (abs(abs(nSpacingBegin) - abs(nSpacingLast))  <= 1) &&
                (abs(abs(nSpacingBegin) - abs(nSpacingMiddle)) <= 1) &&
                (abs(abs(nSpacingMiddle) - abs(nSpacingLast)) <= 1) )
            {
                bLatSpacingOK = true;
            }
            else if( !oSRS.IsProjected() &&
                     (((abs(abs(nSpacingBegin) - abs(nSpacingLast))) <= 100) &&
                      ((abs(abs(nSpacingBegin) - abs(nSpacingMiddle))) <= 100) &&
                      ((abs(abs(nSpacingMiddle) - abs(nSpacingLast))) <= 100)) )
            {
                bLatSpacingOK = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Latitude grid not spaced evenly.  "
                         "Setting projection for grid spacing is "
                         "within 0.1 degrees threshold.");

                CPLDebug("GDAL_netCDF",
                         "Latitude grid not spaced evenly, but within 0.1 "
                         "degree threshold (probably a Gaussian grid).  "
                         "Saving original latitude values in Y_VALUES "
                         "geolocation metadata");
                Set1DGeolocation(nVarDimYID, "Y");
            }

            if( bLatSpacingOK == false )
            {
                CPLDebug("GDAL_netCDF", "Latitude is not equally spaced.");
            }
        }

        if( bLonSpacingOK && bLatSpacingOK )
        {
            // We have gridded data so we can set the Gereferencing info.

            // Enable GeoTransform.

            // In the following "actual_range" and "node_offset"
            // are attributes used by netCDF files created by GMT.
            // If we find them we know how to proceed. Else, use
            // the original algorithm.
            bGotCfGT = true;

            int node_offset = 0;
            nc_get_att_int(cdfid, NC_GLOBAL, "node_offset", &node_offset);

            double adfActualRange[2] = { 0.0, 0.0 };
            double xMinMax[2] = { 0.0, 0.0 };
            double yMinMax[2] = { 0.0, 0.0 };

            if( !nc_get_att_double(cdfid, nVarDimXID,
                                   "actual_range", adfActualRange) )
            {
                xMinMax[0] = adfActualRange[0];
                xMinMax[1] = adfActualRange[1];
            }
            else
            {
                xMinMax[0] = pdfXCoord[0];
                xMinMax[1] = pdfXCoord[xdim - 1];
                node_offset = 0;
            }

            if( !nc_get_att_double(cdfid, nVarDimYID,
                                   "actual_range", adfActualRange) )
            {
                yMinMax[0] = adfActualRange[0];
                yMinMax[1] = adfActualRange[1];
            }
            else
            {
                yMinMax[0] = pdfYCoord[0];
                yMinMax[1] = pdfYCoord[ydim - 1];
                node_offset = 0;
            }

            // Check for reverse order of y-coordinate.
            if( yMinMax[0] > yMinMax[1] )
            {
                const double dfTmp = yMinMax[0];
                yMinMax[0] = yMinMax[1];
                yMinMax[1] = dfTmp;
            }

            double dfCoordOffset = 0.0;
            double dfCoordScale = 1.0;
            if( !nc_get_att_double(cdfid, nVarDimXID,
                                   CF_ADD_OFFSET, &dfCoordOffset) &&
                !nc_get_att_double(cdfid, nVarDimXID,
                                   CF_SCALE_FACTOR, &dfCoordScale) )
            {
                xMinMax[0] = dfCoordOffset + xMinMax[0] * dfCoordScale;
                xMinMax[1] = dfCoordOffset + xMinMax[1] * dfCoordScale;
            }

            if ( !nc_get_att_double(cdfid, nVarDimYID,
                                    CF_ADD_OFFSET, &dfCoordOffset) &&
                 !nc_get_att_double(cdfid, nVarDimYID,
                                    CF_SCALE_FACTOR, &dfCoordScale) )
            {
                yMinMax[0] = dfCoordOffset + yMinMax[0] * dfCoordScale;
                yMinMax[1] = dfCoordOffset + yMinMax[1] * dfCoordScale;
            }

            adfTempGeoTransform[0] = xMinMax[0];
            adfTempGeoTransform[2] = 0;
            adfTempGeoTransform[3] = yMinMax[1];
            adfTempGeoTransform[4] = 0;
            adfTempGeoTransform[1] = (xMinMax[1] - xMinMax[0]) /
                                     (poDS->nRasterXSize + (node_offset - 1));
            adfTempGeoTransform[5] = (yMinMax[0] - yMinMax[1]) /
                                     (poDS->nRasterYSize + (node_offset - 1));

            // Compute the center of the pixel.
            if( !node_offset )
            {
                // Otherwise its already the pixel center.
                adfTempGeoTransform[0] -= (adfTempGeoTransform[1] / 2);
                adfTempGeoTransform[3] -= (adfTempGeoTransform[5] / 2);
            }
        }

        CPLFree(pdfXCoord);
        CPLFree(pdfYCoord);
    }  // end if(has dims)

    // Process custom GDAL values (spatial_ref, GeoTransform).
    if( !EQUAL(szGridMappingValue, "") )
    {
        if( pszWKT != NULL )
        {
            // Compare SRS obtained from CF attributes and GDAL WKT.
            // If possible use the more complete GDAL WKT.

            // Set the SRS to the one written by GDAL.
            if( !bGotCfSRS || poDS->pszProjection == NULL || ! bIsGdalCfFile )
            {
                bGotGdalSRS = true;
                CPLDebug("GDAL_netCDF", "setting WKT from GDAL");
                SetProjection(pszWKT);
            }
            else
            {
                // Use the SRS from GDAL if it doesn't conflict with the one
                // from CF.
                char *pszProjectionGDAL = (char *)pszWKT;
                OGRSpatialReference oSRSGDAL;
                oSRSGDAL.importFromWkt(&pszProjectionGDAL);
                // Set datum to unknown or else datums will not match, see bug
                // #4281.
                if( oSRSGDAL.GetAttrNode("DATUM") )
                    oSRSGDAL.GetAttrNode("DATUM")->GetChild(0)->SetValue("unknown");
                // Need this for setprojection autotest.
                if( oSRSGDAL.GetAttrNode("PROJCS") )
                    oSRSGDAL.GetAttrNode("PROJCS")->GetChild(0)->SetValue("unnamed");
                if( oSRSGDAL.GetAttrNode("GEOGCS") )
                    oSRSGDAL.GetAttrNode("GEOGCS")->GetChild(0)->SetValue("unknown");
                oSRSGDAL.GetRoot()->StripNodes("UNIT");
                OGRSpatialReference oSRSForComparison(oSRS);
                oSRSForComparison.GetRoot()->StripNodes("UNIT");
                if( oSRSForComparison.IsSame(&oSRSGDAL) )
                {
#ifdef NCDF_DEBUG
                    CPLDebug("GDAL_netCDF", "ARE SAME, using GDAL WKT");
#endif
                    bGotGdalSRS = true;
                    CPLDebug("GDAL_netCDF", "setting WKT from GDAL");
                    SetProjection(pszWKT);
                }
                else
                {
                    CPLDebug("GDAL_netCDF",
                             "got WKT from GDAL \n[%s]\nbut not using it "
                             "because conflicts with CF\n[%s]",
                             pszWKT, poDS->pszProjection);
                }
            }

            // Look for GeoTransform Array, if not found in CF.
            if( !bGotCfGT )
            {
                // TODO: Read the GT values and detect for conflict with CF.
                // This could resolve the GT precision loss issue.

                if( pszGeoTransform != NULL )
                {
                    char** papszGeoTransform =
                        CSLTokenizeString2(pszGeoTransform,
                                            " ",
                                            CSLT_HONOURSTRINGS);
                    if( CSLCount(papszGeoTransform) == 6 )
                    {
                        bGotGdalGT = true;
                        for( int i = 0; i < 6; i++ )
                            adfTempGeoTransform[i] =
                                CPLAtof(papszGeoTransform[i]);
                    }
                    CSLDestroy(papszGeoTransform);
                    // Look for corner array values.
                }
                else
                {
                    // CPLDebug("GDAL_netCDF",
                    //           "looking for geotransform corners");

                    snprintf(szTemp, sizeof(szTemp),
                             "%s#Northernmost_Northing", szGridMappingValue);
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
                    bool bGotNN = false;
                    double dfNN = 0.0;
                    if( pszValue != NULL )
                    {
                        dfNN = CPLAtof(pszValue);
                        bGotNN = true;
                    }

                    snprintf(szTemp, sizeof(szTemp),
                             "%s#Southernmost_Northing", szGridMappingValue);
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
                    bool bGotSN = false;
                    double dfSN = 0.0;
                    if( pszValue != NULL )
                    {
                        dfSN = CPLAtof(pszValue);
                        bGotSN = true;
                    }

                    snprintf(szTemp, sizeof(szTemp), "%s#Easternmost_Easting",
                             szGridMappingValue);
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
                    bool bGotEE = false;
                    double dfEE = 0.0;
                    if( pszValue != NULL )
                    {
                        dfEE = CPLAtof(pszValue);
                        bGotEE = true;
                    }

                    snprintf(szTemp, sizeof(szTemp), "%s#Westernmost_Easting",
                             szGridMappingValue);
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
                    bool bGotWE = false;
                    double dfWE = 0.0;
                    if( pszValue != NULL )
                    {
                        dfWE = CPLAtof(pszValue);
                        bGotWE = true;
                    }

                    // Only set the GeoTransform if we got all the values.
                    if( bGotNN && bGotSN && bGotEE && bGotWE )
                    {
                        bGotGdalGT = true;

                        adfTempGeoTransform[0] = dfWE;
                        adfTempGeoTransform[1] =
                            (dfEE - dfWE) / (poDS->GetRasterXSize() - 1);
                        adfTempGeoTransform[2] = 0.0;
                        adfTempGeoTransform[3] = dfNN;
                        adfTempGeoTransform[4] = 0.0;
                        adfTempGeoTransform[5] =
                            (dfSN - dfNN) / (poDS->GetRasterYSize() - 1);
                        // Compute the center of the pixel.
                        adfTempGeoTransform[0] =
                            dfWE - (adfTempGeoTransform[1] / 2);
                        adfTempGeoTransform[3] =
                            dfNN - (adfTempGeoTransform[5] / 2);
                    }
                } // (pszGeoTransform != NULL)

                if( bGotGdalSRS && !bGotGdalGT )
                    CPLDebug("GDAL_netCDF",
                             "Got SRS but no geotransform from GDAL!");
            }  // if( !bGotCfGT )
        }
    }

    // Some netCDF files have a srid attribute (#6613) like
    // urn:ogc:def:crs:EPSG::6931
    snprintf(szTemp, sizeof(szTemp), "%s#%s", szGridMappingValue, "srid");
    const char *pszSRID = CSLFetchNameValue(poDS->papszMetadata, szTemp);
    if( pszSRID != NULL )
    {
        oSRS.Clear();
        if( oSRS.SetFromUserInput(pszSRID) == OGRERR_NONE )
        {
            char *pszWKTExport = NULL;
            CPLDebug("GDAL_netCDF", "Got SRS from %s", szTemp);
            oSRS.exportToWkt(&pszWKTExport);
            SetProjection(pszWKTExport);
            CPLFree(pszWKTExport);
        }
    }

    // Set GeoTransform if we got a complete one - after projection has been set
    if( bGotCfGT || bGotGdalGT )
    {
        SetGeoTransform(adfTempGeoTransform);
    }

    // Process geolocation arrays from CF "coordinates" attribute.
    // Perhaps we should only add if is not a (supported) CF projection
    // (bIsCfProjection).
    ProcessCFGeolocation(nVarId);

    // Debugging reports.
    CPLDebug("GDAL_netCDF",
             "bGotGeogCS=%d bGotCfSRS=%d bGotCfGT=%d bGotGdalSRS=%d "
             "bGotGdalGT=%d",
             static_cast<int>(bGotGeogCS), static_cast<int>(bGotCfSRS),
             static_cast<int>(bGotCfGT), static_cast<int>(bGotGdalSRS),
             static_cast<int>(bGotGdalGT));

    if( !bGotCfGT && !bGotGdalGT )
        CPLDebug("GDAL_netCDF", "did not get geotransform from CF nor GDAL!");

    if( !bGotGeogCS && !bGotCfSRS && !bGotGdalSRS && !bGotCfGT )
        CPLDebug("GDAL_netCDF", "did not get projection from CF nor GDAL!");

    // Search for Well-known GeogCS if got only CF WKT
    // Disabled for now, as a named datum also include control points
    // (see mailing list and bug#4281
    // For example, WGS84 vs. GDA94 (EPSG:3577) - AEA in netcdf_cf.py

    // Disabled for now, but could be set in a config option.
#if 0
    bool bLookForWellKnownGCS = false;  // This could be a Config Option.

    if( bLookForWellKnownGCS && bGotCfSRS && !bGotGdalSRS )
    {
        // ET - Could use a more exhaustive method by scanning all EPSG codes in
        // data/gcs.csv as proposed by Even in the gdal-dev mailing list "help
        // for comparing two WKT".
        // This code could be contributed to a new function.
        // OGRSpatialReference * OGRSpatialReference::FindMatchingGeogCS(
        //     const OGRSpatialReference *poOther) */
        CPLDebug("GDAL_netCDF", "Searching for Well-known GeogCS");
        const char *pszWKGCSList[] = { "WGS84", "WGS72", "NAD27", "NAD83" };
        char *pszWKGCS = NULL;
        oSRS.exportToPrettyWkt(&pszWKGCS);
        for( size_t i = 0; i < sizeof(pszWKGCSList) / 8; i++ )
        {
            pszWKGCS = CPLStrdup(pszWKGCSList[i]);
            OGRSpatialReference oSRSTmp;
            oSRSTmp.SetWellKnownGeogCS(pszWKGCSList[i]);
            // Set datum to unknown, bug #4281.
            if( oSRSTmp.GetAttrNode("DATUM" ) )
                oSRSTmp.GetAttrNode("DATUM")->GetChild(0)->SetValue("unknown");
            // Could use OGRSpatialReference::StripCTParms(), but let's keep
            // TOWGS84.
            oSRSTmp.GetRoot()->StripNodes("AXIS");
            oSRSTmp.GetRoot()->StripNodes("AUTHORITY");
            oSRSTmp.GetRoot()->StripNodes("EXTENSION");

            oSRSTmp.exportToPrettyWkt(&pszWKGCS);
            if( oSRS.IsSameGeogCS(&oSRSTmp) )
            {
                oSRS.SetWellKnownGeogCS(pszWKGCSList[i]);
                oSRS.exportToWkt(&(pszTempProjection));
                SetProjection(pszTempProjection);
                CPLFree(pszTempProjection);
            }
        }
    }
#endif
}

int netCDFDataset::ProcessCFGeolocation( int nVarId )
{
    bool bAddGeoloc = false;
    char *pszTemp = NULL;

    if( NCDFGetAttr(cdfid, nVarId, "coordinates", &pszTemp) == CE_None )
    {
        // Get X and Y geolocation names from coordinates attribute.
        char **papszTokens = CSLTokenizeString2(pszTemp, " ", 0);
        if( CSLCount(papszTokens) >= 2 )
        {
            char szGeolocXName[NC_MAX_NAME + 1];
            char szGeolocYName[NC_MAX_NAME + 1];
            szGeolocXName[0] = '\0';
            szGeolocYName[0] = '\0';

            // Test that each variable is longitude/latitude.
            for( int i = 0; i < CSLCount(papszTokens); i++ )
            {
                if( NCDFIsVarLongitude(cdfid, -1, papszTokens[i]) )
                    snprintf(szGeolocXName, sizeof(szGeolocXName),
                             "%s",papszTokens[i]);
                else if( NCDFIsVarLatitude(cdfid, -1, papszTokens[i]) )
                    snprintf(szGeolocYName, sizeof(szGeolocYName),
                             "%s",papszTokens[i]);
            }
            // Add GEOLOCATION metadata.
            if( !EQUAL(szGeolocXName, "") && !EQUAL(szGeolocYName, "") )
            {
                bAddGeoloc = true;
                CPLDebug("GDAL_netCDF",
                         "using variables %s and %s for GEOLOCATION",
                         szGeolocXName, szGeolocYName);

                SetMetadataItem("SRS", SRS_WKT_WGS84, "GEOLOCATION");

                CPLString osTMP;
                osTMP.Printf("NETCDF:\"%s\":%s",
                             osFilename.c_str(), szGeolocXName);
                SetMetadataItem("X_DATASET", osTMP, "GEOLOCATION");
                SetMetadataItem("X_BAND", "1" , "GEOLOCATION");
                osTMP.Printf("NETCDF:\"%s\":%s",
                             osFilename.c_str(), szGeolocYName);
                SetMetadataItem("Y_DATASET", osTMP, "GEOLOCATION");
                SetMetadataItem("Y_BAND", "1", "GEOLOCATION");

                SetMetadataItem("PIXEL_OFFSET", "0", "GEOLOCATION");
                SetMetadataItem("PIXEL_STEP", "1", "GEOLOCATION");

                SetMetadataItem("LINE_OFFSET", "0", "GEOLOCATION");
                SetMetadataItem("LINE_STEP", "1", "GEOLOCATION");
            }
            else
            {
                CPLDebug("GDAL_netCDF",
                         "coordinates attribute [%s] is unsupported", pszTemp);
            }
        }
        else
        {
            CPLDebug("GDAL_netCDF",
                     "coordinates attribute [%s] with %d element(s) is "
                     "unsupported",
                     pszTemp, CSLCount(papszTokens));
        }
        if( papszTokens ) CSLDestroy(papszTokens);
        CPLFree(pszTemp);
    }

    return bAddGeoloc;
}

CPLErr netCDFDataset::Set1DGeolocation( int nVarId, const char *szDimName )
{
    // Get values.
    char *pszVarValues = NULL;
    CPLErr eErr = NCDFGet1DVar(cdfid, nVarId, &pszVarValues);
    if( eErr != CE_None )
        return eErr;

    // Write metadata.
    char szTemp[ NC_MAX_NAME + 1 + 32 ] = {};
    snprintf(szTemp, sizeof(szTemp), "%s_VALUES", szDimName);
    SetMetadataItem(szTemp, pszVarValues, "GEOLOCATION2");

    CPLFree(pszVarValues);

    return CE_None;
}

double *netCDFDataset::Get1DGeolocation( CPL_UNUSED const char *szDimName,
                                         int &nVarLen )
{
    nVarLen = 0;

    // Get Y_VALUES as tokens.
    char **papszValues
        = NCDFTokenizeArray(GetMetadataItem("Y_VALUES", "GEOLOCATION2"));
    if( papszValues == NULL )
        return NULL;

    // Initialize and fill array.
    nVarLen = CSLCount(papszValues);
    double *pdfVarValues =
        static_cast<double *>(CPLCalloc(nVarLen, sizeof(double)));

    for( int i = 0, j = 0; i < nVarLen; i++ )
    {
        if( !bBottomUp ) j = nVarLen - 1 - i;
        else j = i;  // Invert latitude values.
        char *pszTemp = NULL;
        pdfVarValues[j] = CPLStrtod(papszValues[i], &pszTemp);
    }
    CSLDestroy(papszValues);

    return pdfVarValues;
}

/************************************************************************/
/*                          SetProjection()                           */
/************************************************************************/
CPLErr netCDFDataset::SetProjection( const char * pszNewProjection )
{
    CPLMutexHolderD(&hNCMutex);

    // TODO: Look if proj. already defined, like in geotiff.
    if( pszNewProjection == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "NULL projection.");
        return CE_Failure;
    }

    if( bSetProjection && (GetAccess() == GA_Update) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                  "netCDFDataset::SetProjection() should only be called once "
                  "in update mode!\npszNewProjection=\n%s",
                  pszNewProjection);
    }

    CPLDebug("GDAL_netCDF", "SetProjection, WKT = %s", pszNewProjection);

    if( !STARTS_WITH_CI(pszNewProjection, "GEOGCS")
        && !STARTS_WITH_CI(pszNewProjection, "PROJCS")
        && !EQUAL(pszNewProjection, "") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                  "Only OGC WKT GEOGCS and PROJCS Projections supported "
                  "for writing to NetCDF.  "
                  "%s not supported.",
                  pszNewProjection);

        return CE_Failure;
    }

    CPLFree(pszProjection);
    pszProjection = CPLStrdup(pszNewProjection);

    if( GetAccess() == GA_Update )
    {
        if( bSetGeoTransform && !bSetProjection )
        {
            bSetProjection = true;
            return AddProjectionVars();
        }
    }

    bSetProjection = true;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::SetGeoTransform ( double * padfTransform )
{
    CPLMutexHolderD(&hNCMutex);

    memcpy(adfGeoTransform, padfTransform, sizeof(double)*6);
    // bGeoTransformValid = TRUE;
    // bGeoTIFFInfoChanged = TRUE;

    CPLDebug("GDAL_netCDF",
              "SetGeoTransform(%f,%f,%f,%f,%f,%f)",
              padfTransform[0], padfTransform[1], padfTransform[2],
              padfTransform[3], padfTransform[4], padfTransform[5]);

    if( GetAccess() == GA_Update )
    {
        if( bSetProjection && !bSetGeoTransform )
        {
            bSetGeoTransform = true;
            return AddProjectionVars();
        }
    }

    bSetGeoTransform = true;

    return CE_None;
}

/************************************************************************/
/*                         NCDFWriteSRSVariable()                       */
/************************************************************************/

int NCDFWriteSRSVariable(int cdfid, OGRSpatialReference* poSRS,
                                char **ppszCFProjection, bool bWriteGDALTags)
{
    int status;
    int NCDFVarID = -1;
    char *pszCFProjection = NULL;

    *ppszCFProjection = NULL;

    if( poSRS->IsProjected() )
    {
        // Write CF-1.5 compliant Projected attributes.

        const OGR_SRSNode *poPROJCS = poSRS->GetAttrNode("PROJCS");
        const char *pszProjName = poSRS->GetAttrValue("PROJECTION");
        if( pszProjName == NULL )
            return -1;

        // Basic Projection info (grid_mapping and datum).
        for( int i = 0; poNetcdfSRS_PT[i].WKT_SRS != NULL; i++ )
        {
            if( EQUAL(poNetcdfSRS_PT[i].WKT_SRS, pszProjName) )
            {
                CPLDebug("GDAL_netCDF", "GDAL PROJECTION = %s , NCDF PROJECTION = %s",
                            poNetcdfSRS_PT[i].WKT_SRS,
                            poNetcdfSRS_PT[i].CF_SRS);
                pszCFProjection = CPLStrdup(poNetcdfSRS_PT[i].CF_SRS);
                CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                         cdfid, poNetcdfSRS_PT[i].CF_SRS, NC_CHAR);
                status = nc_def_var(cdfid, poNetcdfSRS_PT[i].CF_SRS, NC_CHAR, 0,
                                    NULL, &NCDFVarID);
                NCDF_ERR(status);
                break;
            }
        }
        if( pszCFProjection == NULL )
            return -1;

        status = nc_put_att_text(cdfid, NCDFVarID, CF_GRD_MAPPING_NAME,
                                 strlen(pszCFProjection), pszCFProjection);
        NCDF_ERR(status);

        // Various projection attributes.
        // PDS: keep in sync with SetProjection function
        NCDFWriteProjAttribs(poPROJCS, pszProjName, cdfid, NCDFVarID);

        if( EQUAL(pszProjName, SRS_PT_GEOSTATIONARY_SATELLITE) )
        {
            const char *pszPredefProj4 = poSRS->GetExtension(
                        poSRS->GetRoot()->GetValue(), "PROJ4", NULL);
            const char *pszSweepAxisAngle =
                (pszPredefProj4 != NULL && strstr(pszPredefProj4, "+sweep=x")) ? "x" : "y";
            status =
                nc_put_att_text(cdfid, NCDFVarID, CF_PP_SWEEP_ANGLE_AXIS,
                                strlen(pszSweepAxisAngle), pszSweepAxisAngle);
            NCDF_ERR(status);
        }
    }
    else
    {
        // Write CF-1.5 compliant Geographics attributes.
        // Note: WKT information will not be preserved (e.g. WGS84).

        pszCFProjection = CPLStrdup("crs");
        CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                 cdfid, pszCFProjection, NC_CHAR);
        status =
            nc_def_var(cdfid, pszCFProjection, NC_CHAR, 0, NULL, &NCDFVarID);
        NCDF_ERR(status);
        status = nc_put_att_text(cdfid, NCDFVarID, CF_GRD_MAPPING_NAME,
                                 strlen(CF_PT_LATITUDE_LONGITUDE),
                                 CF_PT_LATITUDE_LONGITUDE);
        NCDF_ERR(status);
    }

    status = nc_put_att_text(cdfid, NCDFVarID, CF_LNG_NAME,
                             strlen("CRS definition"), "CRS definition");
    NCDF_ERR(status);

    *ppszCFProjection = pszCFProjection;

    // Write CF-1.5 compliant common attributes.

    // DATUM information.
    double dfTemp = poSRS->GetPrimeMeridian();
    nc_put_att_double(cdfid, NCDFVarID, CF_PP_LONG_PRIME_MERIDIAN,
                      NC_DOUBLE, 1, &dfTemp);
    dfTemp = poSRS->GetSemiMajor();
    nc_put_att_double(cdfid, NCDFVarID, CF_PP_SEMI_MAJOR_AXIS,
                      NC_DOUBLE, 1, &dfTemp);
    dfTemp = poSRS->GetInvFlattening();
    nc_put_att_double(cdfid, NCDFVarID, CF_PP_INVERSE_FLATTENING,
                      NC_DOUBLE, 1, &dfTemp);

    if( bWriteGDALTags )
    {
        char *pszSpatialRef = NULL;
        poSRS->exportToWkt(&pszSpatialRef);
        status = nc_put_att_text(cdfid, NCDFVarID, NCDF_SPATIAL_REF,
                                 strlen(pszSpatialRef), pszSpatialRef);
        NCDF_ERR(status);
        CPLFree(pszSpatialRef);
    }

    return NCDFVarID;
}

/************************************************************************/
/*                   NCDFWriteLonLatVarsAttributes()                    */
/************************************************************************/

void NCDFWriteLonLatVarsAttributes(int cdfid, int nVarLonID, int nVarLatID)
{
    int status =
        nc_put_att_text(cdfid, nVarLatID, CF_STD_NAME,
                        strlen(CF_LATITUDE_STD_NAME), CF_LATITUDE_STD_NAME);
    NCDF_ERR(status);

    status =
        nc_put_att_text(cdfid, nVarLatID, CF_LNG_NAME,
                        strlen(CF_LATITUDE_LNG_NAME), CF_LATITUDE_LNG_NAME);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarLatID, CF_UNITS,
                             strlen(CF_DEGREES_NORTH), CF_DEGREES_NORTH);
    NCDF_ERR(status);

    status =
        nc_put_att_text(cdfid, nVarLonID, CF_STD_NAME,
                        strlen(CF_LONGITUDE_STD_NAME), CF_LONGITUDE_STD_NAME);
    NCDF_ERR(status);

    status =
        nc_put_att_text(cdfid, nVarLonID, CF_LNG_NAME,
                        strlen(CF_LONGITUDE_LNG_NAME), CF_LONGITUDE_LNG_NAME);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarLonID, CF_UNITS,
                             strlen(CF_DEGREES_EAST), CF_DEGREES_EAST);
    NCDF_ERR(status);
}

/************************************************************************/
/*                     NCDFWriteXYVarsAttributes()                      */
/************************************************************************/

void NCDFWriteXYVarsAttributes(int cdfid, int nVarXID, int nVarYID,
                               OGRSpatialReference *poSRS)
{
    int status;
    char *pszUnits = NULL;
    const char *pszUnitsToWrite = "";

    const double dfUnits = poSRS->GetLinearUnits(&pszUnits);
    if( fabs(dfUnits - 1.0) < 1e-15 || pszUnits == NULL ||
        EQUAL(pszUnits, "m") || EQUAL(pszUnits, "metre") )
    {
        pszUnitsToWrite = "m";
    }
    else if( fabs(dfUnits - 1000.0) < 1e-15 )
    {
        pszUnitsToWrite = "km";
    }
    else if( fabs(dfUnits - CPLAtof(SRS_UL_US_FOOT_CONV)) < 1e-15 ||
             EQUAL(pszUnits, SRS_UL_US_FOOT) ||
             EQUAL(pszUnits, "US survey foot") )
    {
        pszUnitsToWrite = "US_survey_foot";
    }

    status = nc_put_att_text(cdfid, nVarXID, CF_STD_NAME,
                             strlen(CF_PROJ_X_COORD), CF_PROJ_X_COORD);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarXID, CF_LNG_NAME,
                             strlen(CF_PROJ_X_COORD_LONG_NAME),
                             CF_PROJ_X_COORD_LONG_NAME);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarXID, CF_UNITS,
                             strlen(pszUnitsToWrite), pszUnitsToWrite);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarYID, CF_STD_NAME,
                             strlen(CF_PROJ_Y_COORD), CF_PROJ_Y_COORD);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarYID, CF_LNG_NAME,
                             strlen(CF_PROJ_Y_COORD_LONG_NAME),
                             CF_PROJ_Y_COORD_LONG_NAME);
    NCDF_ERR(status);

    status = nc_put_att_text(cdfid, nVarYID, CF_UNITS, strlen(pszUnitsToWrite),
                             pszUnitsToWrite);
    NCDF_ERR(status);
}

/************************************************************************/
/*                          AddProjectionVars()                         */
/************************************************************************/

CPLErr netCDFDataset::AddProjectionVars( GDALProgressFunc pfnProgress,
                                         void *pProgressData )
{
    const char *pszValue = NULL;
    CPLErr eErr = CE_None;

    bool bWriteGridMapping = false;
    bool bWriteLonLat = false;
    bool bHasGeoloc = false;
    bool bWriteGDALTags = false;
    bool bWriteGeoTransform = false;

    nc_type eLonLatType = NC_NAT;
    int nVarLonID = -1;
    int nVarLatID = -1;
    int nVarXID = -1;
    int nVarYID = -1;

    // For GEOLOCATION information.
    GDALDatasetH hDS_X = NULL;
    GDALRasterBandH hBand_X = NULL;
    GDALDatasetH hDS_Y = NULL;
    GDALRasterBandH hBand_Y = NULL;

    bAddedProjectionVars = true;

    char *pszWKT = (char *)pszProjection;
    OGRSpatialReference oSRS;
    oSRS.importFromWkt(&pszWKT);

    if( oSRS.IsProjected() )
        bIsProjected = true;
    else if( oSRS.IsGeographic() )
        bIsGeographic = true;

    CPLDebug("GDAL_netCDF",
             "SetProjection, WKT now = [%s]\nprojected: %d geographic: %d",
             pszProjection ? pszProjection : "(null)",
             static_cast<int>(bIsProjected),
             static_cast<int>(bIsGeographic));

    if( !bSetGeoTransform )
        CPLDebug("GDAL_netCDF", "netCDFDataset::AddProjectionVars() called, "
                 "but GeoTransform has not yet been defined!");

    if( !bSetProjection )
        CPLDebug("GDAL_netCDF", "netCDFDataset::AddProjectionVars() called, "
                 "but Projection has not yet been defined!");

    // Check GEOLOCATION information.
    char **papszGeolocationInfo = GetMetadata("GEOLOCATION");
    if( papszGeolocationInfo != NULL )
    {
        // Look for geolocation datasets.
        const char *pszDSName =
            CSLFetchNameValue(papszGeolocationInfo, "X_DATASET");
        if( pszDSName != NULL )
            hDS_X = GDALOpenShared(pszDSName, GA_ReadOnly);
        pszDSName = CSLFetchNameValue(papszGeolocationInfo, "Y_DATASET");
        if( pszDSName != NULL )
            hDS_Y = GDALOpenShared(pszDSName, GA_ReadOnly);

        if( hDS_X != NULL && hDS_Y != NULL )
        {
            int nBand = std::max(1, atoi(CSLFetchNameValueDef(
                                        papszGeolocationInfo, "X_BAND", "0")));
            hBand_X = GDALGetRasterBand(hDS_X, nBand);
            nBand = std::max(1, atoi(CSLFetchNameValueDef(papszGeolocationInfo,
                                                          "Y_BAND", "0")));
            hBand_Y = GDALGetRasterBand(hDS_Y, nBand);

            // If geoloc bands are found, do basic validation based on their
            // dimensions.
            if( hBand_X != NULL && hBand_Y != NULL )
            {
                int nXSize_XBand = GDALGetRasterXSize(hDS_X);
                int nYSize_XBand = GDALGetRasterYSize(hDS_X);
                int nXSize_YBand = GDALGetRasterXSize(hDS_Y);
                int nYSize_YBand = GDALGetRasterYSize(hDS_Y);

                // TODO 1D geolocation arrays not implemented.
                if( nYSize_XBand == 1 && nYSize_YBand == 1 )
                {
                    bHasGeoloc = false;
                    CPLDebug("GDAL_netCDF",
                              "1D GEOLOCATION arrays not supported yet");
                }
                // 2D bands must have same sizes as the raster bands.
                else if( nXSize_XBand != nRasterXSize ||
                         nYSize_XBand != nRasterYSize ||
                         nXSize_YBand != nRasterXSize ||
                         nYSize_YBand != nRasterYSize )
                {
                    bHasGeoloc = false;
                    CPLDebug("GDAL_netCDF",
                             "GEOLOCATION array sizes (%dx%d %dx%d) differ "
                             "from raster (%dx%d), not supported",
                             nXSize_XBand, nYSize_XBand, nXSize_YBand,
                             nYSize_YBand, nRasterXSize, nRasterYSize);
                }
                // 2D bands are only supported for projected SRS (see CF 5.6).
                else if( !bIsProjected )
                {
                    bHasGeoloc = false;
                    CPLDebug("GDAL_netCDF",
                             "2D GEOLOCATION arrays only supported for projected SRS");
                }
                else
                {
                    bHasGeoloc = true;
                    CPLDebug("GDAL_netCDF",
                             "dataset has GEOLOCATION information, will try to write it");
                }
            }
        }
    }

    // Process projection options.
    if( bIsProjected )
    {
        bool bIsCfProjection =
            NCDFIsCfProjection(oSRS.GetAttrValue("PROJECTION"));
        bWriteGridMapping = true;
        bWriteGDALTags = CPL_TO_BOOL(
            CSLFetchBoolean(papszCreationOptions, "WRITE_GDAL_TAGS", TRUE));
        // Force WRITE_GDAL_TAGS if is not a CF projection.
        if( !bWriteGDALTags && !bIsCfProjection )
            bWriteGDALTags = true;
        if( bWriteGDALTags )
            bWriteGeoTransform = true;

        // Write lon/lat: default is NO, except if has geolocation.
        // With IF_NEEDED: write if has geoloc or is not CF projection.
        pszValue = CSLFetchNameValue(papszCreationOptions, "WRITE_LONLAT");
        if( pszValue )
        {
            if( EQUAL(pszValue, "IF_NEEDED") )
            {
                bWriteLonLat = bHasGeoloc || !bIsCfProjection;
            }
            else
            {
                bWriteLonLat = CPLTestBool(pszValue);
            }
        }
        else
        {
            bWriteLonLat = bHasGeoloc;
        }

        // Save value of pszCFCoordinates for later.
        if( bWriteLonLat )
        {
            pszCFCoordinates = CPLStrdup(NCDF_LONLAT);
        }

        eLonLatType = NC_FLOAT;
        pszValue =
            CSLFetchNameValueDef(papszCreationOptions, "TYPE_LONLAT", "FLOAT");
        if( EQUAL(pszValue, "DOUBLE") )
            eLonLatType = NC_DOUBLE;
    }
    else
    {
        // Files without a Datum will not have a grid_mapping variable and
        // geographic information.
        bWriteGridMapping = bIsGeographic;

        bWriteGDALTags = CPL_TO_BOOL(CSLFetchBoolean(
            papszCreationOptions, "WRITE_GDAL_TAGS", bWriteGridMapping));
        if(bWriteGDALTags)
            bWriteGeoTransform = true;

        pszValue =
            CSLFetchNameValueDef(papszCreationOptions, "WRITE_LONLAT", "YES");
        if( EQUAL(pszValue, "IF_NEEDED") )
            bWriteLonLat = true;
        else
            bWriteLonLat = CPLTestBool(pszValue);
        //  Don't write lon/lat if no source geotransform.
        if( !bSetGeoTransform )
            bWriteLonLat = false;
        // If we don't write lon/lat, set dimnames to X/Y and write gdal tags.
        if( !bWriteLonLat )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "creating geographic file without lon/lat values!");
            if( bSetGeoTransform )
            {
                bWriteGDALTags = true;  // Not desirable if no geotransform.
                bWriteGeoTransform = true;
            }
        }

        eLonLatType = NC_DOUBLE;
        pszValue =
            CSLFetchNameValueDef(papszCreationOptions, "TYPE_LONLAT", "DOUBLE");
        if( EQUAL(pszValue, "FLOAT") )
            eLonLatType = NC_FLOAT;
    }

    // Make sure we write grid_mapping if we need to write GDAL tags.
    if( bWriteGDALTags ) bWriteGridMapping = true;

    // bottom-up value: new driver is bottom-up by default.
    // Override with WRITE_BOTTOMUP.
    bBottomUp = CPL_TO_BOOL(
        CSLFetchBoolean(papszCreationOptions, "WRITE_BOTTOMUP", TRUE));

    CPLDebug("GDAL_netCDF",
             "bIsProjected=%d bIsGeographic=%d bWriteGridMapping=%d "
             "bWriteGDALTags=%d bWriteLonLat=%d bBottomUp=%d bHasGeoloc=%d",
             static_cast<int>(bIsProjected),
             static_cast<int>(bIsGeographic),
             static_cast<int>(bWriteGridMapping),
             static_cast<int>(bWriteGDALTags),
             static_cast<int>(bWriteLonLat),
             static_cast<int>(bBottomUp),
             static_cast<int>(bHasGeoloc));

    // Exit if nothing to do.
    if( !bIsProjected && !bWriteLonLat )
        return CE_None;

    // Define dimension names.

    // Make sure we are in define mode.
    SetDefineMode(true);

    // Rename dimensions if lon/lat.
    if( !bIsProjected )
    {
        // Rename dims to lat/lon.
        papszDimName.Clear();  // If we add other dims one day, this has to change
        papszDimName.AddString(NCDF_DIMNAME_LAT);
        papszDimName.AddString(NCDF_DIMNAME_LON);

        int status = nc_rename_dim(cdfid, nYDimID, NCDF_DIMNAME_LAT);
        NCDF_ERR(status);
        status = nc_rename_dim(cdfid, nXDimID, NCDF_DIMNAME_LON);
        NCDF_ERR(status);
    }

    // Write projection attributes.
    if( bWriteGridMapping )
    {
        const int NCDFVarID = NCDFWriteSRSVariable(
            cdfid, &oSRS, &pszCFProjection, bWriteGDALTags);
        if( NCDFVarID < 0 )
            return CE_Failure;

        // Optional GDAL custom projection tags.
        if( bWriteGDALTags )
        {
            CPLString osGeoTransform;
            for( int i = 0; i < 6; i++ )
            {
                osGeoTransform += CPLSPrintf("%.16g ", adfGeoTransform[i]);
            }
            CPLDebug("GDAL_netCDF", "szGeoTransform = %s",
                     osGeoTransform.c_str());

            // if( strlen(pszProj4Defn) > 0 ) {
            //     nc_put_att_text(cdfid, NCDFVarID, "proj4",
            //                      strlen(pszProj4Defn), pszProj4Defn);
            // }

            // For now, write the geotransform for back-compat or else
            // the old (1.8.1) driver overrides the CF geotransform with
            // empty values from dfNN, dfSN, dfEE, dfWE;

            // TODO: fix this in 1.8 branch, and then remove this here.
            if( bWriteGeoTransform && bSetGeoTransform )
            {
                {
                    const int status = nc_put_att_text(
                        cdfid, NCDFVarID, NCDF_GEOTRANSFORM,
                        osGeoTransform.size(), osGeoTransform.c_str());
                    NCDF_ERR(status);
                }
            }
        }

        // Write projection variable to band variable.
        // Need to call later if there are no bands.
        AddGridMappingRef();
    }  // end if( bWriteGridMapping )

    pfnProgress(0.10, NULL, pProgressData);

    // Write CF Projection vars.

    // Write X/Y attributes.
    if( bIsProjected )
    {
        // X
        int anXDims[1];
        anXDims[0] = nXDimID;
        CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                 cdfid, CF_PROJ_X_VAR_NAME, NC_DOUBLE);
        int status = nc_def_var(cdfid, CF_PROJ_X_VAR_NAME, NC_DOUBLE,
                                1, anXDims, &nVarXID);
        NCDF_ERR(status);

        // Y
        int anYDims[1];
        anYDims[0] = nYDimID;
        CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                 cdfid, CF_PROJ_Y_VAR_NAME, NC_DOUBLE);
        status = nc_def_var(cdfid, CF_PROJ_Y_VAR_NAME, NC_DOUBLE,
                            1, anYDims, &nVarYID);
        NCDF_ERR(status);

        NCDFWriteXYVarsAttributes(cdfid, nVarXID, nVarYID, &oSRS);
    }

    // Write lat/lon attributes if needed.
    if( bWriteLonLat )
    {
        int *panLatDims = NULL;
        int *panLonDims = NULL;
        int nLatDims = -1;
        int nLonDims = -1;

        // Get information.
        if( bHasGeoloc )
        {
            // Geoloc
            nLatDims = 2;
            panLatDims = static_cast<int *>(CPLCalloc(nLatDims, sizeof(int)));
            panLatDims[0] = nYDimID;
            panLatDims[1] = nXDimID;
            nLonDims = 2;
            panLonDims = static_cast<int *>(CPLCalloc(nLonDims, sizeof(int)));
            panLonDims[0] = nYDimID;
            panLonDims[1] = nXDimID;
        }
        else if( bIsProjected )
        {
            // Projected
            nLatDims = 2;
            panLatDims = static_cast<int *>(CPLCalloc(nLatDims, sizeof(int)));
            panLatDims[0] = nYDimID;
            panLatDims[1] = nXDimID;
            nLonDims = 2;
            panLonDims = static_cast<int *>(CPLCalloc(nLonDims, sizeof(int)));
            panLonDims[0] = nYDimID;
            panLonDims[1] = nXDimID;
        }
        else
        {
            // Geographic
            nLatDims = 1;
            panLatDims = static_cast<int *>(CPLCalloc(nLatDims, sizeof(int)));
            panLatDims[0] = nYDimID;
            nLonDims = 1;
            panLonDims = static_cast<int *>(CPLCalloc(nLonDims, sizeof(int)));
            panLonDims[0] = nXDimID;
        }

        // Def vars and attributes.
        int status = nc_def_var(cdfid, CF_LATITUDE_VAR_NAME, eLonLatType,
                                nLatDims, panLatDims, &nVarLatID);
        CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d,%d,-,-) got id %d",
                 cdfid, CF_LATITUDE_VAR_NAME, eLonLatType, nLatDims, nVarLatID);
        NCDF_ERR(status);
        DefVarDeflate(nVarLatID, false);  // Don't set chunking.

        status = nc_def_var(cdfid, CF_LONGITUDE_VAR_NAME, eLonLatType,
                            nLonDims, panLonDims, &nVarLonID);
        CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d,%d,-,-) got id %d",
                 cdfid, CF_LONGITUDE_VAR_NAME, eLonLatType, nLatDims, nVarLonID);
        NCDF_ERR(status);
        DefVarDeflate(nVarLonID, false);  // Don't set chunking.

        NCDFWriteLonLatVarsAttributes(cdfid, nVarLonID, nVarLatID);

        CPLFree(panLatDims);
        CPLFree(panLonDims);
    }

    // Get projection values.

    double dfX0 = 0.0;
    double dfDX = 0.0;
    double dfY0 = 0.0;
    double dfDY = 0.0;
    double *padLonVal = NULL;
    double *padLatVal = NULL;

    if( bIsProjected )
    {
        OGRSpatialReference *poLatLonSRS = NULL;
        OGRCoordinateTransformation *poTransform = NULL;

        char *pszWKT2 = (char *)pszProjection;
        OGRSpatialReference oSRS2;
        oSRS2.importFromWkt(&pszWKT2);

        double *padYVal = NULL;
        double *padXVal = NULL;
        size_t startX[1];
        size_t countX[1];
        size_t startY[1];
        size_t countY[1];

        CPLDebug("GDAL_netCDF", "Getting (X,Y) values");

        padXVal =
            static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));
        padYVal =
            static_cast<double *>(CPLMalloc(nRasterYSize * sizeof(double)));

        // Get Y values.
        if( !bBottomUp )
            dfY0 = adfGeoTransform[3];
        else  // Invert latitude values.
            dfY0 = adfGeoTransform[3] + (adfGeoTransform[5] * nRasterYSize);
        dfDY = adfGeoTransform[5];

        for( int j = 0; j < nRasterYSize; j++ )
        {
            // The data point is centered inside the pixel.
            if( !bBottomUp )
                padYVal[j] = dfY0 + (j + 0.5) * dfDY;
            else  // Invert latitude values.
                padYVal[j] = dfY0 - (j + 0.5) * dfDY;
        }
        startX[0] = 0;
        countX[0] = nRasterXSize;

        // Get X values.
        dfX0 = adfGeoTransform[0];
        dfDX = adfGeoTransform[1];

        for( int i = 0; i < nRasterXSize; i++ )
        {
            // The data point is centered inside the pixel.
            padXVal[i] = dfX0 + (i + 0.5) * dfDX;
        }
        startY[0] = 0;
        countY[0] = nRasterYSize;

        // Write X/Y values.

        // Make sure we are in data mode.
        SetDefineMode(false);

        CPLDebug("GDAL_netCDF", "Writing X values");
        int status =
            nc_put_vara_double(cdfid, nVarXID, startX, countX, padXVal);
        NCDF_ERR(status);

        CPLDebug("GDAL_netCDF", "Writing Y values");
        status = nc_put_vara_double(cdfid, nVarYID, startY, countY, padYVal);
        NCDF_ERR(status);

        pfnProgress(0.20, NULL, pProgressData);

        // Write lon/lat arrays (CF coordinates) if requested.

        // Get OGR transform if GEOLOCATION is not available.
        if( bWriteLonLat && !bHasGeoloc )
        {
            poLatLonSRS = oSRS2.CloneGeogCS();
            if( poLatLonSRS != NULL )
                poTransform =
                    OGRCreateCoordinateTransformation(&oSRS2, poLatLonSRS);
            // If no OGR transform, then don't write CF lon/lat.
            if( poTransform == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to get Coordinate Transform");
                bWriteLonLat = false;
            }
        }

        if( bWriteLonLat )
        {
            if( !bHasGeoloc )
                CPLDebug("GDAL_netCDF", "Transforming (X,Y)->(lon,lat)");
            else
                CPLDebug("GDAL_netCDF",
                         "Writing (lon,lat) from GEOLOCATION arrays");

            bool bOK = true;
            double dfProgress = 0.2;

            size_t start[] = { 0, 0 };
            size_t count[] = { 1, (size_t)nRasterXSize };
            padLatVal =
                static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));
            padLonVal =
                static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));

            for( int j = 0; j < nRasterYSize && bOK && status == NC_NOERR; j++ )
            {
                start[0] = j;

                // Get values from geotransform.
                if( !bHasGeoloc )
                {
                    // Fill values to transform.
                    for( int i=0; i<nRasterXSize; i++ )
                    {
                        padLatVal[i] = padYVal[j];
                        padLonVal[i] = padXVal[i];
                    }

                    // Do the transform.
                    bOK = CPL_TO_BOOL(poTransform->Transform(
                        nRasterXSize, padLonVal, padLatVal, NULL));
                    if( !bOK )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                  "Unable to Transform (X,Y) to (lon,lat).");
                    }
                }
                // Get values from geoloc arrays.
                else
                {
                    eErr = GDALRasterIO(hBand_Y, GF_Read,
                                        0, j, nRasterXSize, 1,
                                        padLatVal, nRasterXSize, 1,
                                        GDT_Float64, 0, 0);
                    if( eErr == CE_None )
                    {
                        eErr = GDALRasterIO(hBand_X, GF_Read,
                                            0, j, nRasterXSize, 1,
                                            padLonVal, nRasterXSize, 1,
                                            GDT_Float64, 0, 0);
                    }

                    if( eErr == CE_None )
                    {
                        bOK = true;
                    }
                    else
                    {
                        bOK = false;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Unable to get scanline %d", j);
                    }
                }

                // Write data.
                if( bOK )
                {
                    status = nc_put_vara_double(cdfid, nVarLatID, start,
                                                count, padLatVal);
                    NCDF_ERR(status);
                    status = nc_put_vara_double(cdfid, nVarLonID, start,
                                                count, padLonVal);
                    NCDF_ERR(status);
                }

                if( (nRasterYSize / 10) >0 && (j % (nRasterYSize / 10) == 0) )
                {
                    dfProgress += 0.08;
                    pfnProgress(dfProgress, NULL, pProgressData);
                }
            }
        }

        if( poLatLonSRS != NULL ) delete poLatLonSRS;
        if( poTransform != NULL ) delete poTransform;

        CPLFree(padXVal);
        CPLFree(padYVal);
        CPLFree(padLonVal);
        CPLFree(padLatVal);
    }  // Projected

    // If not projected, assume geographic to catch grids without Datum.
    else if( bWriteLonLat )
    {
        // Get latitude values.
        if( !bBottomUp )
            dfY0 = adfGeoTransform[3];
        else  // Invert latitude values.
            dfY0 = adfGeoTransform[3] + (adfGeoTransform[5] * nRasterYSize);
        dfDY = adfGeoTransform[5];

        // Override lat values with the ones in GEOLOCATION/Y_VALUES.
        if( GetMetadataItem("Y_VALUES", "GEOLOCATION") != NULL )
        {
            int nTemp = 0;
            padLatVal = Get1DGeolocation("Y_VALUES", nTemp);
            // Make sure we got the correct amount, if not fallback to GT */
            // could add test fabs(fabs(padLatVal[0]) - fabs(dfY0)) <= 0.1))
            if( nTemp == nRasterYSize )
            {
                CPLDebug("GDAL_netCDF",
                         "Using Y_VALUES geolocation metadata for lat values");
            }
            else
            {
                CPLDebug("GDAL_netCDF",
                         "Got %d elements from Y_VALUES geolocation metadata, need %d",
                         nTemp, nRasterYSize);
                if( padLatVal )
                {
                    CPLFree(padLatVal);
                    padLatVal = NULL;
                }
            }
        }

        if( padLatVal == NULL )
        {
            padLatVal =
                static_cast<double *>(CPLMalloc(nRasterYSize * sizeof(double)));
            for( int i = 0; i < nRasterYSize; i++ )
            {
                // The data point is centered inside the pixel.
                if( !bBottomUp )
                    padLatVal[i] = dfY0 + (i + 0.5) * dfDY;
                else  // Invert latitude values.
                    padLatVal[i] = dfY0 - (i + 0.5) * dfDY;
            }
        }

        size_t startLat[1] = {0};
        size_t countLat[1] = {static_cast<size_t>(nRasterYSize)};

        // Get longitude values.
        dfX0 = adfGeoTransform[0];
        dfDX = adfGeoTransform[1];

        padLonVal =
            static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));
        for( int i = 0; i < nRasterXSize; i++ )
        {
            // The data point is centered inside the pixel.
            padLonVal[i] = dfX0 + (i + 0.5) * dfDX;
        }

        size_t startLon[1] = {0};
        size_t countLon[1] = {static_cast<size_t>(nRasterXSize)};

        // Write latitude and longitude values.

        // Make sure we are in data mode.
        SetDefineMode(false);

        // Write values.
        CPLDebug("GDAL_netCDF", "Writing lat values");

        int status =
            nc_put_vara_double(cdfid, nVarLatID, startLat, countLat, padLatVal);
        NCDF_ERR(status);

        CPLDebug("GDAL_netCDF", "Writing lon values");
        status =
            nc_put_vara_double(cdfid, nVarLonID, startLon, countLon, padLonVal);
        NCDF_ERR(status);

        CPLFree(padLatVal);
        CPLFree(padLonVal);
    }  // Not projected.

    if( hDS_X != NULL )
    {
        GDALClose(hDS_X);
    }
    if( hDS_Y != NULL )
    {
        GDALClose(hDS_Y);
    }

    pfnProgress(1.00, NULL, pProgressData);

    return CE_None;
}

// Write Projection variable to band variable.
// Moved from AddProjectionVars() for cases when bands are added after
// projection.
void netCDFDataset::AddGridMappingRef()
{
    bool bOldDefineMode = bDefineMode;

    if( (GetAccess() == GA_Update) &&
        (nBands >= 1) && (GetRasterBand(1)) &&
        pszCFProjection != NULL && !EQUAL(pszCFProjection, "") )
    {
        const int nVarId =
            static_cast<netCDFRasterBand *>(GetRasterBand(1))->nZId;
        bAddedGridMappingRef = true;

        // Make sure we are in define mode.
        SetDefineMode(true);
        int status = nc_put_att_text(cdfid, nVarId, CF_GRD_MAPPING,
                                     strlen(pszCFProjection), pszCFProjection);
        NCDF_ERR(status);
        if( pszCFCoordinates != NULL && !EQUAL(pszCFCoordinates, "") )
        {
            status =
                nc_put_att_text(cdfid, nVarId, CF_COORDINATES,
                                strlen(pszCFCoordinates), pszCFCoordinates);
            NCDF_ERR(status);
        }

        // Go back to previous define mode.
        SetDefineMode(bOldDefineMode);
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::GetGeoTransform( double *padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    if( bSetGeoTransform )
        return CE_None;

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                                rint()                                */
/************************************************************************/

#ifdef HAVE_CXX11
double netCDFDataset::rint( double dfX )
{
    return std::round(dfX);
}
#else
double netCDFDataset::rint( double dfX)
{
    // rint has undefined behavior for values of dfX that exceed in int max.
    if( dfX > 0 )
    {
        int nX = (int)(dfX + 0.5);
        if( nX % 2 )
        {
            double dfDiff = dfX - (double)nX;
            if( dfDiff == -0.5 )
                return double(nX - 1);
        }
        return double(nX);
    }
    else
    {
        int nX = (int)(dfX - 0.5);
        if( nX % 2 )
        {
            double dfDiff = dfX - (double)nX;
            if( dfDiff == 0.5 )
                return double(nX + 1);
        }
        return double(nX);
    }
}
#endif  // !HAVE_CXX11

/************************************************************************/
/*                        ReadAttributes()                              */
/************************************************************************/
CPLErr netCDFDataset::ReadAttributes( int cdfidIn, int var)

{
    char szVarName[NC_MAX_NAME + 1];
    int nbAttr;

    nc_inq_varnatts(cdfidIn, var, &nbAttr);
    if( var == NC_GLOBAL )
    {
        strcpy(szVarName, "NC_GLOBAL");
    }
    else
    {
        szVarName[0] = '\0';
        int status = nc_inq_varname(cdfidIn, var, szVarName);
        NCDF_ERR(status);
    }

    for( int l = 0; l < nbAttr; l++ )
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        int status = nc_inq_attname(cdfidIn, var, l, szAttrName);
        NCDF_ERR(status);
        char szMetaName[NC_MAX_NAME * 2 + 1 + 1];
        snprintf(szMetaName, sizeof(szMetaName), "%s#%s", szVarName,
                 szAttrName);

        char *pszMetaTemp = NULL;
        if( NCDFGetAttr(cdfidIn, var, szAttrName, &pszMetaTemp) == CE_None )
        {
            papszMetadata =
                CSLSetNameValue(papszMetadata, szMetaName, pszMetaTemp);
            CPLFree(pszMetaTemp);
            pszMetaTemp = NULL;
        }
        else
        {
            CPLDebug("GDAL_netCDF", "invalid global metadata %s", szMetaName);
        }
    }

    return CE_None;
}

/************************************************************************/
/*                netCDFDataset::CreateSubDatasetList()                 */
/************************************************************************/
void netCDFDataset::CreateSubDatasetList()
{
    char szName[NC_MAX_NAME + 1];
    char szVarStdName[NC_MAX_NAME + 1];
    int *ponDimIds = NULL;
    nc_type nAttype;
    size_t nAttlen;

    netCDFDataset *poDS = this;

    int nSub = 1;
    int nVarCount;
    nc_inq_nvars(cdfid, &nVarCount);

    for( int nVar = 0; nVar < nVarCount; nVar++ )
    {

        int nDims;
        nc_inq_varndims(cdfid, nVar, &nDims);

        if( nDims >= 2 )
        {
            ponDimIds = static_cast<int *>(CPLCalloc(nDims, sizeof(int)));
            nc_inq_vardimid(cdfid, nVar, ponDimIds);

            // Create Sub dataset list.
            CPLString osDim;
            for( int i = 0; i < nDims; i++ )
            {
                size_t nDimLen;
                nc_inq_dimlen(cdfid, ponDimIds[i], &nDimLen);
                osDim += CPLSPrintf("%dx", (int)nDimLen);
            }

            nc_type nVarType;
            nc_inq_vartype(cdfid, nVar, &nVarType);
            // Get rid of the last "x" character.
            osDim.resize(osDim.size() - 1);
            const char *pszType = "";
            switch( nVarType )
            {
            case NC_BYTE:
                pszType = "8-bit integer";
                break;
            case NC_CHAR:
                pszType = "8-bit character";
                break;
            case NC_SHORT:
                pszType = "16-bit integer";
                break;
            case NC_INT:
                pszType = "32-bit integer";
                break;
            case NC_FLOAT:
                pszType = "32-bit floating-point";
                break;
            case NC_DOUBLE:
                pszType = "64-bit floating-point";
                break;
#ifdef NETCDF_HAS_NC4
            case NC_UBYTE:
                pszType = "8-bit unsigned integer";
                break;
            case NC_USHORT:
                pszType = "16-bit unsigned integer";
                break;
            case NC_UINT:
                pszType = "32-bit unsigned integer";
                break;
            case NC_INT64:
                pszType = "64-bit integer";
                break;
            case NC_UINT64:
                pszType = "64-bit unsigned integer";
                break;
#endif
            default:
                break;
            }
            szName[0] = '\0';
            int status = nc_inq_varname(cdfid, nVar, szName);
            NCDF_ERR(status);
            nAttlen = 0;
            nc_inq_att(cdfid, nVar, CF_STD_NAME, &nAttype, &nAttlen);
            if( nAttlen < sizeof(szVarStdName) &&
                nc_get_att_text(cdfid, nVar, CF_STD_NAME,
                                szVarStdName) == NC_NOERR )
            {
                szVarStdName[nAttlen] = '\0';
            }
            else
            {
                snprintf(szVarStdName, sizeof(szVarStdName), "%s", szName);
            }

            char szTemp[NC_MAX_NAME + 1];
            snprintf(szTemp, sizeof(szTemp), "SUBDATASET_%d_NAME", nSub);

            poDS->papszSubDatasets =
                CSLSetNameValue(poDS->papszSubDatasets, szTemp,
                                CPLSPrintf("NETCDF:\"%s\":%s",
                                           poDS->osFilename.c_str(), szName));

            snprintf(szTemp, sizeof(szTemp), "SUBDATASET_%d_DESC", nSub++);

            poDS->papszSubDatasets =
                CSLSetNameValue(poDS->papszSubDatasets, szTemp,
                                CPLSPrintf("[%s] %s (%s)", osDim.c_str(),
                                           szVarStdName, pszType));

            CPLFree(ponDimIds);
        }
    }
}

/************************************************************************/
/*                              IdentifyFormat()                      */
/************************************************************************/

NetCDFFormatEnum netCDFDataset::IdentifyFormat( GDALOpenInfo *poOpenInfo,
#ifndef HAVE_HDF5
CPL_UNUSED
#endif
                                   bool bCheckExt = true )
{
    // Does this appear to be a netcdf file? If so, which format?
    // http://www.unidata.ucar.edu/software/netcdf/docs/faq.html#fv1_5

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
        return NCDF_FORMAT_UNKNOWN;
    if( poOpenInfo->nHeaderBytes < 4 )
        return NCDF_FORMAT_NONE;
    if( STARTS_WITH_CI((char*)poOpenInfo->pabyHeader, "CDF\001") )
    {
        // In case the netCDF driver is registered before the GMT driver,
        // avoid opening GMT files.
        if( GDALGetDriverByName("GMT") != NULL )
        {
            bool bFoundZ = false;
            bool bFoundDimension = false;
            for( int i = 0; i < poOpenInfo->nHeaderBytes - 11; i++ )
            {
                if( poOpenInfo->pabyHeader[i] == 1 &&
                    poOpenInfo->pabyHeader[i + 1] == 'z' &&
                    poOpenInfo->pabyHeader[i + 2] == 0 )
                    bFoundZ = true;
                else if( poOpenInfo->pabyHeader[i] == 9 &&
                         memcmp((const char *)poOpenInfo->pabyHeader + i + 1,
                                "dimension", 9) == 0 &&
                         poOpenInfo->pabyHeader[i + 10] == 0 )
                    bFoundDimension = true;
            }
            if( bFoundZ && bFoundDimension )
                return NCDF_FORMAT_UNKNOWN;
        }

        return NCDF_FORMAT_NC;
    }
    else if( STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "CDF\002") )
    {
        return NCDF_FORMAT_NC2;
    }
    else if( STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "\211HDF\r\n\032\n") )
    {
        // Requires netCDF-4/HDF5 support in libnetcdf (not just libnetcdf-v4).
        // If HDF5 is not supported in GDAL, this driver will try to open the
        // file Else, make sure this driver does not try to open HDF5 files If
        // user really wants to open with this driver, use NETCDF:file.h5
        // format.  This check should be relaxed, but there is no clear way to
        // make a difference.

        // Check for HDF5 support in GDAL.
#ifdef HAVE_HDF5
        if( bCheckExt )
        {
            // Check by default.
            const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
            if( !(EQUAL(pszExtension, "nc") || EQUAL(pszExtension, "cdf") ||
                  EQUAL(pszExtension, "nc2") || EQUAL(pszExtension, "nc4") ||
                  EQUAL(pszExtension, "nc3") || EQUAL(pszExtension, "grd")) )
                return NCDF_FORMAT_HDF5;
        }
#endif

        // Check for netcdf-4 support in libnetcdf.
#ifdef NETCDF_HAS_NC4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF5;
#endif
    }
    else if( STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "\016\003\023\001") )
    {
        // Requires HDF4 support in libnetcdf, but if HF4 is supported by GDAL
        // don't try to open.
        // If user really wants to open with this driver, use NETCDF:file.hdf
        // syntax.

        // Check for HDF4 support in GDAL.
#ifdef HAVE_HDF4
        if( bCheckExt )
        {
            // Check by default.
            // Always treat as HDF4 file.
            return NCDF_FORMAT_HDF4;
        }
#endif

        // Check for HDF4 support in libnetcdf.
#ifdef NETCDF_HAS_HDF4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF4;
#endif
    }

    return NCDF_FORMAT_NONE;
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int netCDFDataset::TestCapability(const char *pszCap)
{
    if( EQUAL(pszCap, ODsCCreateLayer) )
    {
        return eAccess == GA_Update && nBands == 0 &&
               (eMultipleLayerBehaviour != SINGLE_LAYER || nLayers == 0);
    }
    return FALSE;
}

/************************************************************************/
/*                            GetLayer()                                */
/************************************************************************/

OGRLayer *netCDFDataset::GetLayer(int nIdx)
{
    if( nIdx < 0 || nIdx >= nLayers )
        return NULL;
    return papoLayers[nIdx];
}

/************************************************************************/
/*                            ICreateLayer()                            */
/************************************************************************/

OGRLayer *netCDFDataset::ICreateLayer( const char *pszName,
                                       OGRSpatialReference *poSpatialRef,
                                       OGRwkbGeometryType eGType,
                                       char **papszOptions )
{
    int nLayerCDFId = cdfid;
    if( !TestCapability(ODsCCreateLayer) )
        return NULL;

    CPLString osNetCDFLayerName(pszName);
    const netCDFWriterConfigLayer *poLayerConfig = NULL;
    if( oWriterConfig.m_bIsValid )
    {
        std::map<CPLString, netCDFWriterConfigLayer>::const_iterator
            oLayerIter = oWriterConfig.m_oLayers.find(pszName);
        if( oLayerIter != oWriterConfig.m_oLayers.end() )
        {
            poLayerConfig = &(oLayerIter->second);
            osNetCDFLayerName = poLayerConfig->m_osNetCDFName;
        }
    }

    netCDFDataset *poLayerDataset = NULL;
    if( eMultipleLayerBehaviour == SEPARATE_FILES )
    {
        char **papszDatasetOptions = NULL;
        papszDatasetOptions = CSLSetNameValue(
            papszDatasetOptions, "CONFIG_FILE",
            CSLFetchNameValue(papszCreationOptions, "CONFIG_FILE"));
        papszDatasetOptions =
            CSLSetNameValue(papszDatasetOptions, "FORMAT",
                            CSLFetchNameValue(papszCreationOptions, "FORMAT"));
        papszDatasetOptions = CSLSetNameValue(
            papszDatasetOptions, "WRITE_GDAL_TAGS",
            CSLFetchNameValue(papszCreationOptions, "WRITE_GDAL_TAGS"));
        CPLString osLayerFilename(
            CPLFormFilename(osFilename, osNetCDFLayerName, "nc"));
        CPLAcquireMutex(hNCMutex, 1000.0);
        poLayerDataset =
            CreateLL(osLayerFilename, 0, 0, 0, papszDatasetOptions);
        CPLReleaseMutex(hNCMutex);
        CSLDestroy(papszDatasetOptions);
        if( poLayerDataset == NULL )
            return NULL;

        nLayerCDFId = poLayerDataset->cdfid;
        NCDFAddGDALHistory(nLayerCDFId, osLayerFilename, "", "Create",
                           NCDF_CONVENTIONS_CF_V1_6);
    }
#ifdef NETCDF_HAS_NC4
    else if( eMultipleLayerBehaviour == SEPARATE_GROUPS )
    {
        SetDefineMode(true);

        nLayerCDFId = -1;
        int status = nc_def_grp(cdfid, osNetCDFLayerName, &nLayerCDFId);
        NCDF_ERR(status);
        if( status != NC_NOERR )
            return NULL;

        NCDFAddGDALHistory(nLayerCDFId, osFilename, "", "Create",
                           NCDF_CONVENTIONS_CF_V1_6);
    }
#endif

    // Make a clone to workaround a bug in released MapServer versions
    // that destroys the passed SRS instead of releasing it .
    OGRSpatialReference *poSRS = poSpatialRef;
    if( poSRS != NULL )
        poSRS = poSRS->Clone();
    netCDFLayer *poLayer =
        new netCDFLayer(poLayerDataset ? poLayerDataset : this, nLayerCDFId,
                        osNetCDFLayerName, eGType, poSRS);
    if( poSRS != NULL )
        poSRS->Release();

    // Fetch layer creation options coming from config file
    char **papszNewOptions = CSLDuplicate(papszOptions);
    if( oWriterConfig.m_bIsValid )
    {
        std::map<CPLString, CPLString>::const_iterator oIter;
        for( oIter = oWriterConfig.m_oLayerCreationOptions.begin();
             oIter != oWriterConfig.m_oLayerCreationOptions.end(); ++oIter )
        {
            papszNewOptions =
                CSLSetNameValue(papszNewOptions, oIter->first, oIter->second);
        }
        if( poLayerConfig != NULL )
        {
            for( oIter = poLayerConfig->m_oLayerCreationOptions.begin();
                 oIter != poLayerConfig->m_oLayerCreationOptions.end(); ++oIter )
            {
                papszNewOptions = CSLSetNameValue(papszNewOptions, oIter->first,
                                                  oIter->second);
            }
        }
    }

    const bool bRet = poLayer->Create(papszNewOptions, poLayerConfig);
    CSLDestroy(papszNewOptions);

    if( !bRet )
    {
        delete poLayer;
        return NULL;
    }

    if( poLayerDataset != NULL )
        apoVectorDatasets.push_back(poLayerDataset);

    papoLayers = static_cast<netCDFLayer **>(
        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(netCDFLayer *)));
    papoLayers[nLayers++] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                           CloneAttributes()                          */
/************************************************************************/

bool netCDFDataset::CloneAttributes(int old_cdfid, int new_cdfid, int nSrcVarId,
                                    int nDstVarId)
{
    int nAttCount = -1;
    int status = nc_inq_varnatts(old_cdfid, nSrcVarId, &nAttCount);
    NCDF_ERR(status);

    for( int i = 0; i < nAttCount; i++ )
    {
        char szName[NC_MAX_NAME + 1];
        szName[0] = 0;
        status = nc_inq_attname(old_cdfid, nSrcVarId, i, szName);
        NCDF_ERR(status);

        status =
            nc_copy_att(old_cdfid, nSrcVarId, szName, new_cdfid, nDstVarId);
        NCDF_ERR(status);
        if( status != NC_NOERR )
            return false;
    }

    return true;
}

/************************************************************************/
/*                          CloneVariableContent()                      */
/************************************************************************/

bool netCDFDataset::CloneVariableContent(int old_cdfid, int new_cdfid,
                                         int nSrcVarId, int nDstVarId)
{
    int nVarDimCount = -1;
    int status = nc_inq_varndims(old_cdfid, nSrcVarId, &nVarDimCount);
    NCDF_ERR(status);
    int anDimIds[] = { -1, 1 };
    status = nc_inq_vardimid(old_cdfid, nSrcVarId, anDimIds);
    NCDF_ERR(status);
    nc_type nc_datatype = NC_NAT;
    status = nc_inq_vartype(old_cdfid, nSrcVarId, &nc_datatype);
    NCDF_ERR(status);
    size_t nTypeSize = 0;
    switch( nc_datatype )
    {
    case NC_BYTE:
    case NC_CHAR:
        nTypeSize = 1;
        break;
    case NC_SHORT:
        nTypeSize = 2;
        break;
    case NC_INT:
        nTypeSize = 4;
        break;
    case NC_FLOAT:
        nTypeSize = 4;
        break;
    case NC_DOUBLE:
        nTypeSize = 8;
        break;
#ifdef NETCDF_HAS_NC4
    case NC_UBYTE:
        nTypeSize = 1;
        break;
    case NC_USHORT:
        nTypeSize = 2;
        break;
    case NC_UINT:
        nTypeSize = 4;
        break;
    case NC_INT64:
    case NC_UINT64:
        nTypeSize = 8;
        break;
    case NC_STRING:
        nTypeSize = sizeof(char *);
        break;
#endif
    default:
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type: %d",
                 nc_datatype);
        return false;
    }
    }

    size_t nElems = 1;
    size_t anStart[NC_MAX_DIMS];
    size_t anCount[NC_MAX_DIMS];
    size_t nRecords = 1;
    for( int i = 0; i < nVarDimCount; i++)
    {
        anStart[i] = 0;
        if( i == 0 )
        {
            anCount[i] = 1;
            status = nc_inq_dimlen(old_cdfid, anDimIds[i], &nRecords);
            NCDF_ERR(status);
        }
        else
        {
            anCount[i] = 0;
            status = nc_inq_dimlen(old_cdfid, anDimIds[i], &anCount[i]);
            NCDF_ERR(status);
            nElems *= anCount[i];
        }
    }

    void *pBuffer = VSI_MALLOC2_VERBOSE(nElems, nTypeSize);
    if( pBuffer == NULL )
        return false;

    for(size_t iRecord = 0; iRecord < nRecords; iRecord++ )
    {
        anStart[0] = iRecord;

        switch( nc_datatype )
        {
        case NC_BYTE:
            status = nc_get_vara_schar(old_cdfid, nSrcVarId, anStart, anCount,
                                       static_cast<signed char *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_schar(new_cdfid, nDstVarId, anStart, anCount,
                                      static_cast<signed char *>(pBuffer));
            break;
        case NC_CHAR:
            status = nc_get_vara_text(old_cdfid, nSrcVarId, anStart, anCount,
                                      static_cast<char *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_text(new_cdfid, nDstVarId, anStart, anCount,
                                     static_cast<char *>(pBuffer));
            break;
        case NC_SHORT:
            status = nc_get_vara_short(old_cdfid, nSrcVarId, anStart, anCount,
                                       static_cast<short *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_short(new_cdfid, nDstVarId, anStart, anCount,
                                      static_cast<short *>(pBuffer));
            break;
        case NC_INT:
            status = nc_get_vara_int(old_cdfid, nSrcVarId, anStart, anCount,
                                     static_cast<int *>(pBuffer));
            if( !status )
                status = nc_put_vara_int(new_cdfid, nDstVarId, anStart, anCount,
                                         static_cast<int *>(pBuffer));
            break;
        case NC_FLOAT:
            status = nc_get_vara_float(old_cdfid, nSrcVarId, anStart, anCount,
                                       static_cast<float *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_float(new_cdfid, nDstVarId, anStart, anCount,
                                      static_cast<float *>(pBuffer));
            break;
        case NC_DOUBLE:
            status = nc_get_vara_double(old_cdfid, nSrcVarId, anStart, anCount,
                                        static_cast<double *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_double(new_cdfid, nDstVarId, anStart, anCount,
                                       static_cast<double *>(pBuffer));
            break;
#ifdef NETCDF_HAS_NC4
        case NC_STRING:
            status = nc_get_vara_string(old_cdfid, nSrcVarId, anStart, anCount,
                                        static_cast<char **>(pBuffer));
            if( !status )
            {
                status =
                    nc_put_vara_string(new_cdfid, nDstVarId, anStart, anCount,
                                       static_cast<const char **>(pBuffer));
                nc_free_string(nElems, static_cast<char **>(pBuffer));
            }
            break;

        case NC_UBYTE:
            status = nc_get_vara_uchar(old_cdfid, nSrcVarId, anStart, anCount,
                                       static_cast<unsigned char *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_uchar(new_cdfid, nDstVarId, anStart, anCount,
                                      static_cast<unsigned char *>(pBuffer));
            break;
        case NC_USHORT:
            status = nc_get_vara_ushort(old_cdfid, nSrcVarId, anStart, anCount,
                                        static_cast<unsigned short *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_ushort(new_cdfid, nDstVarId, anStart, anCount,
                                       static_cast<unsigned short *>(pBuffer));
            break;
        case NC_UINT:
            status = nc_get_vara_uint(old_cdfid, nSrcVarId, anStart, anCount,
                                      static_cast<unsigned int *>(pBuffer));
            if( !status )
                status =
                    nc_put_vara_uint(new_cdfid, nDstVarId, anStart, anCount,
                                     static_cast<unsigned int *>(pBuffer));
            break;
        case NC_INT64:
            status =
                nc_get_vara_longlong(old_cdfid, nSrcVarId, anStart, anCount,
                                     static_cast<long long *>(pBuffer));
            if(!status)
                status =
                    nc_put_vara_longlong(new_cdfid, nDstVarId, anStart, anCount,
                                         static_cast<long long *>(pBuffer));
            break;
        case NC_UINT64:
            status = nc_get_vara_ulonglong(
                old_cdfid, nSrcVarId, anStart, anCount,
                static_cast<unsigned long long *>(pBuffer));
            if( !status )
                status = nc_put_vara_ulonglong(
                    new_cdfid, nDstVarId, anStart, anCount,
                    static_cast<unsigned long long *>(pBuffer));
            break;
#endif
        default:
            status = NC_EBADTYPE;
        }

        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            VSIFree(pBuffer);
            return false;
        }
    }

    VSIFree(pBuffer);
    return true;
}

/************************************************************************/
/*                         NCDFIsUnlimitedDim()                         */
/************************************************************************/

bool NCDFIsUnlimitedDim(bool
#ifdef NETCDF_HAS_NC4
                            bIsNC4
#endif
                            , int cdfid, int nDimId)
{
#ifdef NETCDF_HAS_NC4
    if( bIsNC4 )
    {
        int nUnlimitedDims = 0;
        nc_inq_unlimdims(cdfid, &nUnlimitedDims, NULL);
        bool bFound = false;
        if( nUnlimitedDims )
        {
            int *panUnlimitedDimIds =
                static_cast<int *>(CPLMalloc(sizeof(int) * nUnlimitedDims));
            nc_inq_unlimdims(cdfid, NULL, panUnlimitedDimIds);
            for(int i = 0; i < nUnlimitedDims; i++)
            {
                if( panUnlimitedDimIds[i] == nDimId )
                {
                    bFound = true;
                    break;
                }
            }
            CPLFree(panUnlimitedDimIds);
        }
        return bFound;
    }
    else
#endif
    {
        int nUnlimitedDimId = -1;
        nc_inq(cdfid, NULL, NULL, NULL, &nUnlimitedDimId);
        return nDimId == nUnlimitedDimId;
    }
}

/************************************************************************/
/*                              CloneGrp()                              */
/************************************************************************/

bool netCDFDataset::CloneGrp(int nOldGrpId, int nNewGrpId,
                             bool bIsNC4, int nLayerId,
                             int nDimIdToGrow, size_t nNewSize)
{
    // Clone dimensions
    int nDimCount = -1;
    int status = nc_inq_ndims(nOldGrpId, &nDimCount);
    NCDF_ERR(status)
    int *panDimIds = static_cast<int *>(CPLMalloc(sizeof(int) * nDimCount));
    int nUnlimiDimID = -1;
    status = nc_inq_unlimdim(nOldGrpId, &nUnlimiDimID);
    NCDF_ERR(status);
#ifdef NETCDF_HAS_NC4
    if( bIsNC4 )
    {
        // In NC4, the dimension ids of a group are not necessarily in
        // [0,nDimCount-1] range
        int nDimCount2 = -1;
        status = nc_inq_dimids(nOldGrpId, &nDimCount2, panDimIds, FALSE);
        NCDF_ERR(status);
        CPLAssert(nDimCount == nDimCount2);
    }
    else
#endif
    {
        for( int i = 0; i < nDimCount; i++ )
            panDimIds[i] = i;
    }
    for( int i = 0; i < nDimCount; i++ )
    {
        char szDimName[NC_MAX_NAME + 1];
        szDimName[0] = 0;
        size_t nLen = 0;
        const int nDimId = panDimIds[i];
        status = nc_inq_dim(nOldGrpId, nDimId, szDimName, &nLen);
        NCDF_ERR(status);
        if( NCDFIsUnlimitedDim(bIsNC4, nOldGrpId, nDimId) )
            nLen = NC_UNLIMITED;
        else if( nDimId == nDimIdToGrow && nOldGrpId == nLayerId )
            nLen = nNewSize;
        int nNewDimId = -1;
        status = nc_def_dim(nNewGrpId, szDimName, nLen, &nNewDimId);
        NCDF_ERR(status);
        CPLAssert(nDimId == nNewDimId);
        if( status != NC_NOERR )
        {
            CPLFree(panDimIds);
            return false;
        }
    }
    CPLFree(panDimIds);

    // Clone main attributes
    if( !CloneAttributes(nOldGrpId, nNewGrpId, NC_GLOBAL, NC_GLOBAL) )
    {
        return false;
    }

    // Clone variable definitions
    int nVarCount = -1;
    status = nc_inq_nvars(nOldGrpId, &nVarCount);
    NCDF_ERR(status);

    for( int i=0; i < nVarCount; i++ )
    {
        char szVarName[NC_MAX_NAME + 1];
        szVarName[0] = 0;
        status = nc_inq_varname(nOldGrpId, i, szVarName);
        NCDF_ERR(status);
        nc_type nc_datatype = NC_NAT;
        status = nc_inq_vartype(nOldGrpId, i, &nc_datatype);
        NCDF_ERR(status);
        int nVarDimCount = -1;
        status = nc_inq_varndims(nOldGrpId, i, &nVarDimCount);
        NCDF_ERR(status);
        int anDimIds[NC_MAX_DIMS];
        status = nc_inq_vardimid(nOldGrpId, i, anDimIds);
        NCDF_ERR(status);
        int nNewVarId = -1;
        status = nc_def_var(nNewGrpId, szVarName, nc_datatype,
                            nVarDimCount, anDimIds, &nNewVarId);
        NCDF_ERR(status);
        CPLAssert(i == nNewVarId);
        if( status != NC_NOERR )
        {
            return false;
        }

        if( !CloneAttributes(nOldGrpId, nNewGrpId, i, i) )
        {
            return false;
        }
    }

    status = nc_enddef(nNewGrpId);
    NCDF_ERR(status);
    if( status != NC_NOERR )
    {
        return false;
    }

    // Clone variable content
    for( int i = 0; i < nVarCount; i++ )
    {
        if( !CloneVariableContent(nOldGrpId, nNewGrpId, i, i) )
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                              GrowDim()                               */
/************************************************************************/

bool netCDFDataset::GrowDim(int nLayerId, int nDimIdToGrow, size_t nNewSize)
{
    int nCreationMode;
    // Set nCreationMode based on eFormat.
    switch( eFormat )
    {
#ifdef NETCDF_HAS_NC2
    case NCDF_FORMAT_NC2:
        nCreationMode = NC_CLOBBER | NC_64BIT_OFFSET;
        break;
#endif
#ifdef NETCDF_HAS_NC4
    case NCDF_FORMAT_NC4:
        nCreationMode = NC_CLOBBER | NC_NETCDF4;
        break;
    case NCDF_FORMAT_NC4C:
        nCreationMode = NC_CLOBBER | NC_NETCDF4 | NC_CLASSIC_MODEL;
        break;
#endif
    case NCDF_FORMAT_NC:
    default:
        nCreationMode = NC_CLOBBER;
        break;
    }

    int new_cdfid = -1;
    CPLString osTmpFilename(osFilename + ".tmp");
    int status = nc_create(osTmpFilename, nCreationMode, &new_cdfid);
    NCDF_ERR(status)
    if( status != NC_NOERR )
        return false;

    if( !CloneGrp(cdfid, new_cdfid,
                  eFormat == NCDF_FORMAT_NC4,
                  nLayerId, nDimIdToGrow, nNewSize) )
    {
        nc_close(new_cdfid);
        return false;
    }

#ifdef NETCDF_HAS_NC4
    int nGroupCount = 0;
    std::vector<CPLString> oListGrpName;
    if( eFormat == NCDF_FORMAT_NC4 &&
        nc_inq_grps(cdfid, &nGroupCount, NULL) == NC_NOERR &&
        nGroupCount > 0 )
    {
        int *panGroupIds =
            static_cast<int *>(CPLMalloc(sizeof(int) * nGroupCount));
        status = nc_inq_grps(cdfid, NULL, panGroupIds);
        NCDF_ERR(status)
        for(int i = 0; i < nGroupCount; i++)
        {
            char szGroupName[NC_MAX_NAME + 1];
            szGroupName[0] = 0;
            nc_inq_grpname(panGroupIds[i], szGroupName);
            int nNewGrpId = -1;
            status = nc_def_grp(new_cdfid, szGroupName, &nNewGrpId);
            NCDF_ERR(status)
            if( status != NC_NOERR )
            {
                CPLFree(panGroupIds);
                nc_close(new_cdfid);
                return false;
            }
            if( !CloneGrp(panGroupIds[i], nNewGrpId,
                          eFormat == NCDF_FORMAT_NC4,
                          nLayerId, nDimIdToGrow, nNewSize) )
            {
                CPLFree(panGroupIds);
                nc_close(new_cdfid);
                return false;
            }
        }
        CPLFree(panGroupIds);

        for(int i = 0; i < nLayers; i++)
        {
            char szGroupName[NC_MAX_NAME + 1];
            szGroupName[0] = 0;
            status = nc_inq_grpname(papoLayers[i]->GetCDFID(), szGroupName);
            NCDF_ERR(status)
            oListGrpName.push_back(szGroupName);
        }
    }
#endif

    nc_close(cdfid);
    cdfid = -1;
    nc_close(new_cdfid);

    CPLString osOriFilename(osFilename + ".ori");
    if( VSIRename(osFilename, osOriFilename) != 0 ||
        VSIRename(osTmpFilename, osFilename) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Renaming of files failed");
        return false;
    }
    VSIUnlink(osOriFilename);

    status = nc_open(osFilename, NC_WRITE, &cdfid);
    NCDF_ERR(status);
    if( status != NC_NOERR )
        return false;
    bDefineMode = false;

#ifdef NETCDF_HAS_NC4
    if( !oListGrpName.empty() )
    {
        for(int i = 0; i < nLayers; i++)
        {
            int nNewLayerCDFID = -1;
            status =
                nc_inq_ncid(cdfid, oListGrpName[i].c_str(), &nNewLayerCDFID);
            NCDF_ERR(status);
            papoLayers[i]->SetCDFID(nNewLayerCDFID);
        }
    }
    else
#endif
    {
        for(int i = 0; i < nLayers; i++)
        {
            papoLayers[i]->SetCDFID(cdfid);
        }
    }

    return true;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int netCDFDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
    {
        return TRUE;
    }
    const NetCDFFormatEnum eTmpFormat = IdentifyFormat(poOpenInfo);
    if( NCDF_FORMAT_NC == eTmpFormat ||
        NCDF_FORMAT_NC2 == eTmpFormat ||
        NCDF_FORMAT_NC4 == eTmpFormat ||
        NCDF_FORMAT_NC4C == eTmpFormat )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *netCDFDataset::Open( GDALOpenInfo *poOpenInfo )

{
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "\n=====\nOpen(), filename=[%s]",
             poOpenInfo->pszFilename);
#endif

    // Does this appear to be a netcdf file?
    NetCDFFormatEnum eTmpFormat = NCDF_FORMAT_NONE;
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
    {
        eTmpFormat = IdentifyFormat(poOpenInfo);
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "identified format %d", eTmpFormat);
#endif
        // Note: not calling Identify() directly, because we want the file type.
        // Only support NCDF_FORMAT* formats.
        if( !(NCDF_FORMAT_NC == eTmpFormat ||
              NCDF_FORMAT_NC2 == eTmpFormat ||
              NCDF_FORMAT_NC4 == eTmpFormat ||
              NCDF_FORMAT_NC4C == eTmpFormat) )
            return NULL;
    }

    CPLMutexHolderD(&hNCMutex);

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with
                                // GDALDataset own mutex.
    netCDFDataset *poDS = new netCDFDataset();
    poDS->papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
    CPLAcquireMutex(hNCMutex, 1000.0);

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Check if filename start with NETCDF: tag.
    bool bTreatAsSubdataset = false;
    CPLString osSubdatasetName;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
    {
        char **papszName =
            CSLTokenizeString2(poOpenInfo->pszFilename,
                               ":", CSLT_HONOURSTRINGS|CSLT_PRESERVEESCAPES);

        // Check for drive name in windows NETCDF:"D:\...
        if( CSLCount(papszName) == 4 &&
            strlen(papszName[1]) == 1 &&
            (papszName[2][0] == '/' || papszName[2][0] == '\\') )
        {
            poDS->osFilename = papszName[1];
            poDS->osFilename += ':';
            poDS->osFilename += papszName[2];
            osSubdatasetName = papszName[3];
            bTreatAsSubdataset = true;
            CSLDestroy(papszName);
        }
        else if( CSLCount(papszName) == 3 )
        {
            poDS->osFilename = papszName[1];
            osSubdatasetName = papszName[2];
            bTreatAsSubdataset = true;
            CSLDestroy(papszName);
        }
        else if( CSLCount(papszName) == 2 )
        {
            poDS->osFilename = papszName[1];
            osSubdatasetName = "";
            bTreatAsSubdataset = false;
            CSLDestroy(papszName);
        }
        else
        {
            CSLDestroy(papszName);
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                        // deadlock with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to parse NETCDF: prefix string into expected 2, 3 or 4 fields.");
            return NULL;
        }
        // Identify Format from real file, with bCheckExt=FALSE.
        GDALOpenInfo *poOpenInfo2 =
            new GDALOpenInfo(poDS->osFilename.c_str(), GA_ReadOnly);
        poDS->eFormat = IdentifyFormat(poOpenInfo2, FALSE);
        delete poOpenInfo2;
        if( NCDF_FORMAT_NONE == poDS->eFormat ||
            NCDF_FORMAT_UNKNOWN == poDS->eFormat )
        {
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                        // deadlock with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return NULL;
        }
    }
    else
    {
        poDS->osFilename = poOpenInfo->pszFilename;
        bTreatAsSubdataset = false;
        poDS->eFormat = eTmpFormat;
    }

    // Try opening the dataset.
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "calling nc_open(%s)", poDS->osFilename.c_str());
#endif
    int cdfid;
    const int nMode = ((poOpenInfo->nOpenFlags & (GDAL_OF_UPDATE | GDAL_OF_VECTOR)) ==
                (GDAL_OF_UPDATE | GDAL_OF_VECTOR)) ? NC_WRITE : NC_NOWRITE;
    if( nc_open(poDS->osFilename, nMode, &cdfid) != NC_NOERR )
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "error opening");
#endif
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "got cdfid=%d", cdfid);
#endif

    // Is this a real netCDF file?
    int ndims;
    int ngatts;
    int nvars;
    int unlimdimid;
    int status = nc_inq(cdfid, &ndims, &nvars, &ngatts, &unlimdimid);
    if( status != NC_NOERR )
    {
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    // Get file type from netcdf.
    int nTmpFormat = NCDF_FORMAT_NONE;
    status = nc_inq_format(cdfid, &nTmpFormat);
    if( status != NC_NOERR )
    {
        NCDF_ERR(status);
    }
    else
    {
        CPLDebug("GDAL_netCDF",
                 "driver detected file type=%d, libnetcdf detected type=%d",
                 poDS->eFormat, nTmpFormat);
        if( static_cast<NetCDFFormatEnum>(nTmpFormat) != poDS->eFormat )
        {
            // Warn if file detection conflicts with that from libnetcdf
            // except for NC4C, which we have no way of detecting initially.
            if( nTmpFormat != NCDF_FORMAT_NC4C )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "NetCDF driver detected file type=%d, but libnetcdf detected type=%d",
                         poDS->eFormat, nTmpFormat);
            }
            CPLDebug("GDAL_netCDF", "setting file type to %d, was %d",
                     nTmpFormat, poDS->eFormat);
            poDS->eFormat = static_cast<NetCDFFormatEnum>(nTmpFormat);
        }
    }

    // Confirm the requested access is supported.
    if( poOpenInfo->eAccess == GA_Update &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The NETCDF driver does not support update access to existing"
                 " datasets.");
        nc_close(cdfid);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    // Does the request variable exist?
    if( bTreatAsSubdataset )
    {
        int var;
        status = nc_inq_varid(cdfid, osSubdatasetName, &var);
        if( status != NC_NOERR )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s is a netCDF file, but %s is not a variable.",
                     poOpenInfo->pszFilename, osSubdatasetName.c_str());

            nc_close(cdfid);
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                        // deadlock with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return NULL;
        }
    }

    if( ndims < 2 && (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "%s is a netCDF file, but without any dimensions >= 2.",
                 poOpenInfo->pszFilename);

        nc_close(cdfid);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    CPLDebug("GDAL_netCDF", "dim_count = %d", ndims);

    char szConventions[NC_MAX_NAME + 1];
    szConventions[0] = '\0';
    nc_type nAttype = NC_NAT;
    size_t nAttlen = 0;
    nc_inq_att(cdfid, NC_GLOBAL, "Conventions", &nAttype, &nAttlen);
    if( nAttlen >= sizeof(szConventions) ||
        (status = nc_get_att_text(cdfid, NC_GLOBAL, "Conventions",
                                  szConventions)) != NC_NOERR )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No UNIDATA NC_GLOBAL:Conventions attribute");
        // Note that 'Conventions' is always capital 'C' in CF spec.
    }
    else
    {
        szConventions[nAttlen] = '\0';
    }

    // Create band information objects.
    CPLDebug("GDAL_netCDF", "var_count = %d", nvars);

    // Create a corresponding GDALDataset.
    // Create Netcdf Subdataset if filename as NETCDF tag.
    poDS->cdfid = cdfid;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->bDefineMode = false;

    poDS->ReadAttributes(cdfid, NC_GLOBAL);

    int nCount = 0;
    int nVarID = -1;
    int nIgnoredVars = 0;
    char *pszTemp = NULL;

    // In vector only mode, if the file is NC4, if the main group has no
    // variables but there are sub-groups, then iterate over the subgroups to
    // check if there are vector layers.
#ifdef NETCDF_HAS_NC4
    int nGroupCount = 0;
    int *panGroupIds = NULL;
    if( nvars == 0 &&
        poDS->eFormat == NCDF_FORMAT_NC4 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 )
    {
        nc_inq_grps(cdfid, &nGroupCount, NULL);
    }
    if( nGroupCount > 0 )
    {
        panGroupIds = static_cast<int *>(CPLMalloc(sizeof(int) * nGroupCount));
        nc_inq_grps(cdfid, NULL, panGroupIds);
    }
    for( int iGrp = 0; iGrp < ((nGroupCount) ? nGroupCount : 1); iGrp ++ )
    {
        char szGroupName[NC_MAX_NAME + 1];
        szGroupName[0] = 0;
        int iGrpId = (nGroupCount) ? panGroupIds[iGrp] : cdfid;
        if( nGroupCount )
        {
            nc_inq_grpname(iGrpId, szGroupName);
            ndims = 0;
            ngatts = 0;
            nvars = 0;
            unlimdimid = -1;
            status = nc_inq(iGrpId, &ndims, &nvars, &ngatts, &unlimdimid);
            NCDF_ERR(status);

            CSLDestroy(poDS->papszMetadata);
            poDS->papszMetadata = NULL;
            poDS->ReadAttributes(cdfid, NC_GLOBAL);
            poDS->ReadAttributes(iGrpId, NC_GLOBAL);
        }
        else
        {
            snprintf(szGroupName, sizeof(szGroupName), "%s",
                     CPLGetBasename(poDS->osFilename));
        }
        poDS->cdfid = iGrpId;
#else
    char szGroupName[NC_MAX_NAME + 1];
    snprintf(szGroupName, sizeof(szGroupName),
             "%s", CPLGetBasename(poDS->osFilename));
#endif

    // Identify variables that we should ignore as Raster Bands.
    // Variables that are identified in other variable's "coordinate" and
    // "bounds" attribute should not be treated as Raster Bands.
    // See CF sections 5.2, 5.6 and 7.1.
    char **papszIgnoreVars = NULL;

        for( int j = 0; j < nvars; j++ )
        {
            if( NCDFGetAttr(poDS->cdfid, j, "coordinates", &pszTemp) == CE_None )
            {
                char **papszTokens = CSLTokenizeString2(pszTemp, " ", 0);
                for( int i = 0; i<CSLCount(papszTokens); i++ )
                {
                    papszIgnoreVars =
                        CSLAddString(papszIgnoreVars, papszTokens[i]);
                }
                if( papszTokens ) CSLDestroy(papszTokens);
                CPLFree(pszTemp);
             }
            if( NCDFGetAttr(poDS->cdfid, j, "bounds", &pszTemp) == CE_None &&
                pszTemp != NULL )
            {
                if( !EQUAL(pszTemp, "") )
                    papszIgnoreVars = CSLAddString(papszIgnoreVars, pszTemp);
                CPLFree(pszTemp);
            }
        }

        // Filter variables (valid 2D raster bands and vector fields).
        nIgnoredVars = 0;
        std::vector<int> anPotentialVectorVarID;
        // oMapDimIdToCount[x] = number of times dim x is the first dimension of
        // potential vector variables
        std::map<int, int> oMapDimIdToCount;
        int nVarXId = -1;
        int nVarYId = -1;
        int nVarZId = -1;
        bool bIsVectorOnly = true;
        int nProfileDimId = -1;
        int nParentIndexVarID = -1;

        for( int j = 0; j < nvars; j++ )
        {
            int ndimsForVar = -1;
            char szTemp[NC_MAX_NAME + 1];
            nc_inq_varndims(poDS->cdfid, j, &ndimsForVar);
            // Should we ignore this variable?
            szTemp[0] = '\0';
            status = nc_inq_varname(poDS->cdfid, j, szTemp);
            if( status != NC_NOERR )
                continue;

            nc_type atttype = NC_NAT;
            size_t attlen = 0;

            if( ndimsForVar == 1 &&
                (NCDFIsVarLongitude(poDS->cdfid, -1, szTemp) ||
                NCDFIsVarProjectionX(poDS->cdfid, -1, szTemp)) )
            {
                nVarXId = j;
            }
            else if( ndimsForVar == 1 &&
                     (NCDFIsVarLatitude(poDS->cdfid, -1, szTemp) ||
                      NCDFIsVarProjectionY(poDS->cdfid, -1, szTemp)) )
            {
                nVarYId = j;
            }
            else if( ndimsForVar == 1 &&
                     NCDFIsVarVerticalCoord(poDS->cdfid, -1, szTemp) )
            {
                nVarZId = j;
            }
            else if( CSLFindString(papszIgnoreVars, szTemp) != -1 )
            {
                nIgnoredVars++;
                CPLDebug("GDAL_netCDF", "variable #%d [%s] was ignored", j,
                         szTemp);
            }
            // Only accept 2+D vars.
            else if( ndimsForVar >= 2 )
            {

                // Identify variables that might be vector variables
                if( ndimsForVar == 2 )
                {
                    int anDimIds[2] = { -1, -1 };
                    nc_inq_vardimid(poDS->cdfid, j, anDimIds);
                    char szDimNameX[NC_MAX_NAME + 1];
                    char szDimNameY[NC_MAX_NAME + 1];
                    szDimNameX[0] = '\0';
                    szDimNameY[0] = '\0';
                    if( nc_inq_dimname(poDS->cdfid, anDimIds[0], szDimNameY) ==
                           NC_NOERR &&
                       nc_inq_dimname(poDS->cdfid, anDimIds[1], szDimNameX) ==
                           NC_NOERR &&
                       NCDFIsVarLongitude(poDS->cdfid, -1, szDimNameX) ==
                           false &&
                       NCDFIsVarProjectionX(poDS->cdfid, -1, szDimNameX) ==
                           false &&
                       NCDFIsVarLatitude(poDS->cdfid, -1, szDimNameY) ==
                           false &&
                       NCDFIsVarProjectionY(poDS->cdfid, -1, szDimNameY) ==
                           false )
                    {
                        anPotentialVectorVarID.push_back(j);
                        oMapDimIdToCount[anDimIds[0]]++;
                    }
                    else
                    {
                        bIsVectorOnly = false;
                    }
                }
                else
                {
                    bIsVectorOnly = false;
                }
                if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 )
                {
                    nVarID = j;
                    nCount++;
                }
            }
            else if( ndimsForVar == 1 )
            {
                if( nc_inq_att(poDS->cdfid, j, "instance_dimension", &atttype,
                               &attlen) == NC_NOERR &&
                    atttype == NC_CHAR && attlen < NC_MAX_NAME )
                {
                    char szInstanceDimension[NC_MAX_NAME + 1];
                    if( nc_get_att_text(poDS->cdfid, j, "instance_dimension",
                                        szInstanceDimension) == NC_NOERR )
                    {
                        szInstanceDimension[attlen] = 0;
                        for(int idim = 0; idim < ndims; idim++)
                        {
                            char szDimName[NC_MAX_NAME + 1];
                            szDimName[0] = 0;
                            status =
                                nc_inq_dimname(poDS->cdfid, idim, szDimName);
                            NCDF_ERR(status);
                        if( strcmp(szInstanceDimension, szDimName) == 0 )
                            {
                                nParentIndexVarID = j;
                                nProfileDimId = idim;
                                break;
                            }
                        }
                        if( nProfileDimId < 0 )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Attribute instance_dimension='%s' refers "
                                     "to a non existing dimension",
                                     szInstanceDimension);
                        }
                    }
                }
                if( j != nParentIndexVarID )
                {
                    anPotentialVectorVarID.push_back(j);
                    int nDimId = -1;
                    nc_inq_vardimid(poDS->cdfid, j, &nDimId);
                    oMapDimIdToCount[nDimId]++;
                }
            }
        }

        CSLDestroy(papszIgnoreVars);

        CPLString osFeatureType(CSLFetchNameValueDef(
            poDS->papszMetadata, "NC_GLOBAL#featureType", ""));

        // If we are opened in raster-only mode and that there are only 1D or 2D
        // variables and that the 2D variables have no X/Y dim, and all
        // variables refer to the same main dimension (or 2 dimensions for
        // featureType=profile), then it is a pure vector dataset
        if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 &&
            bIsVectorOnly && nCount > 0 &&
            !anPotentialVectorVarID.empty() &&
            (oMapDimIdToCount.size() == 1 ||
             (EQUAL(osFeatureType, "profile") && oMapDimIdToCount.size() == 2 &&
              nProfileDimId >= 0)) )
        {
            anPotentialVectorVarID.resize(0);
            nCount = 0;
        }

        if( !anPotentialVectorVarID.empty() &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 )
        {
            // Take the dimension that is referenced the most times
            if( !(oMapDimIdToCount.size() == 1 ||
              (EQUAL(osFeatureType, "profile") && oMapDimIdToCount.size() == 2 && nProfileDimId >= 0)) )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The dataset has several variables that could be identified "
                         "as vector fields, but not all share the same primary dimension. "
                         "Consequently they will be ignored.");
            }
            else
            {
                OGRwkbGeometryType eGType = wkbUnknown;
                CPLString osLayerName = CSLFetchNameValueDef(
                poDS->papszMetadata, "NC_GLOBAL#ogr_layer_name", szGroupName);
                poDS->papszMetadata = CSLSetNameValue(
                    poDS->papszMetadata, "NC_GLOBAL#ogr_layer_name", NULL);

                if( EQUAL(osFeatureType, "point") ||
                    EQUAL(osFeatureType, "profile") )
                {
                    poDS->papszMetadata = CSLSetNameValue(
                        poDS->papszMetadata, "NC_GLOBAL#featureType", NULL);
                    eGType = wkbPoint;
                }

                const char *pszLayerType = CSLFetchNameValue(
                    poDS->papszMetadata, "NC_GLOBAL#ogr_layer_type");
                if( pszLayerType != NULL )
                {
                    eGType = OGRFromOGCGeomType(pszLayerType);
                    poDS->papszMetadata = CSLSetNameValue(
                        poDS->papszMetadata, "NC_GLOBAL#ogr_layer_type", NULL);
                }

                CPLString osGeometryField = CSLFetchNameValueDef(
                    poDS->papszMetadata, "NC_GLOBAL#ogr_geometry_field", "");
                poDS->papszMetadata = CSLSetNameValue(
                    poDS->papszMetadata, "NC_GLOBAL#ogr_geometry_field", NULL);

                int nFirstVarId = -1;
                int nVectorDim = oMapDimIdToCount.rbegin()->first;
                if( EQUAL(osFeatureType, "profile") &&
                    oMapDimIdToCount.size() == 2 )
                {
                    if( nVectorDim == nProfileDimId )
                        nVectorDim = oMapDimIdToCount.begin()->first;
                }
                else
                {
                    nProfileDimId = -1;
                }
                for( size_t j = 0; j < anPotentialVectorVarID.size(); j++ )
                {
                    int anDimIds[2] = { -1, -1 };
                    nc_inq_vardimid(poDS->cdfid, anPotentialVectorVarID[j],
                                    anDimIds);
                    if( nVectorDim == anDimIds[0] )
                    {
                        nFirstVarId = anPotentialVectorVarID[j];
                        break;
                    }
                }

                // In case where coordinates are explicitly specified for one of
                // the field/variable,
                // use them in priority over the ones that might have been
                // identified above.
                char *pszCoordinates = NULL;
                if( NCDFGetAttr(poDS->cdfid, nFirstVarId, "coordinates",
                                &pszCoordinates) == CE_None )
                {
                    char **papszTokens =
                        CSLTokenizeString2(pszCoordinates, " ", 0);
                    for(int i = 0;
                        papszTokens != NULL && papszTokens[i] != NULL; i++)
                    {
                        if( NCDFIsVarLongitude(poDS->cdfid, -1,
                                               papszTokens[i]) ||
                            NCDFIsVarProjectionX(poDS->cdfid, -1,
                                                 papszTokens[i]) )
                        {
                            nVarXId = -1;
                            CPL_IGNORE_RET_VAL(nc_inq_varid(
                                poDS->cdfid, papszTokens[i], &nVarXId));
                        }
                        else if( NCDFIsVarLatitude(poDS->cdfid, -1,
                                                   papszTokens[i]) ||
                                 NCDFIsVarProjectionY(poDS->cdfid, -1,
                                                      papszTokens[i]) )
                        {
                            nVarYId = -1;
                            CPL_IGNORE_RET_VAL(nc_inq_varid(
                                poDS->cdfid, papszTokens[i], &nVarYId));
                        }
                        else if( NCDFIsVarVerticalCoord(poDS->cdfid, -1,
                                                       papszTokens[i]))
                        {
                            nVarZId = -1;
                            CPL_IGNORE_RET_VAL(nc_inq_varid(
                                poDS->cdfid, papszTokens[i], &nVarZId));
                        }
                    }
                    CSLDestroy(papszTokens);
                }
                CPLFree(pszCoordinates);

                // Check that the X,Y,Z vars share 1D and share the same
                // dimension as attribute variables.
                if( nVarXId >= 0 && nVarYId >= 0 )
                {
                    int nVarDimCount = -1;
                    int nVarDimId = -1;
                    if( nc_inq_varndims(poDS->cdfid, nVarXId, &nVarDimCount) != NC_NOERR ||
                        nVarDimCount != 1 ||
                        nc_inq_vardimid(poDS->cdfid, nVarXId, &nVarDimId) != NC_NOERR ||
                        nVarDimId != ((nProfileDimId >= 0) ? nProfileDimId : nVectorDim) ||
                        nc_inq_varndims(poDS->cdfid, nVarYId, &nVarDimCount) != NC_NOERR ||
                        nVarDimCount != 1 ||
                    nc_inq_vardimid(poDS->cdfid, nVarYId, &nVarDimId) != NC_NOERR ||
                    nVarDimId != ((nProfileDimId >= 0) ? nProfileDimId : nVectorDim) )
                    {
                        nVarXId = nVarYId = -1;
                    }
                    else if( nVarZId >= 0 &&
                             (nc_inq_varndims(poDS->cdfid, nVarZId,
                                              &nVarDimCount) != NC_NOERR ||
                              nVarDimCount != 1 ||
                              nc_inq_vardimid(poDS->cdfid, nVarZId,
                                              &nVarDimId) != NC_NOERR ||
                              nVarDimId != nVectorDim) )
                    {
                        nVarZId = -1;
                    }
                }

                if( eGType == wkbUnknown && nVarXId >= 0 && nVarYId >= 0 )
                {
                    eGType = wkbPoint;
                }
                if( eGType == wkbPoint && nVarXId >= 0 && nVarYId >= 0 &&
                    nVarZId >= 0 )
                {
                    eGType = wkbPoint25D;
                }
                if( eGType == wkbUnknown && osGeometryField.empty() )
                {
                    eGType = wkbNone;
                }

                // Read projection info
                char **papszMetadataBackup = CSLDuplicate(poDS->papszMetadata);
                poDS->ReadAttributes(poDS->cdfid, nFirstVarId);
                poDS->SetProjectionFromVar(nFirstVarId, true);

                char szVarName[NC_MAX_NAME + 1];
                szVarName[0] = '\0';
                CPL_IGNORE_RET_VAL(
                    nc_inq_varname(poDS->cdfid, nFirstVarId, szVarName));
                char szTemp[NC_MAX_NAME + 1];
                snprintf(szTemp, sizeof(szTemp),
                         "%s#%s", szVarName, CF_GRD_MAPPING);
                CPLString osGridMapping =
                    CSLFetchNameValueDef(poDS->papszMetadata, szTemp, "");

                CSLDestroy(poDS->papszMetadata);
                poDS->papszMetadata = papszMetadataBackup;

                OGRSpatialReference *poSRS = NULL;
                if( poDS->pszProjection != NULL )
                {
                    poSRS = new OGRSpatialReference();
                    char *pszWKT = poDS->pszProjection;
                    if( poSRS->importFromWkt(&pszWKT) != OGRERR_NONE )
                    {
                        delete poSRS;
                        poSRS = NULL;
                    }
                    CPLFree(poDS->pszProjection);
                    poDS->pszProjection = NULL;
                }
                // Reset if there's a 2D raster
                poDS->bSetProjection = false;
                poDS->bSetGeoTransform = false;

            if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 )
                {
                    // Strip out uninteresting metadata.
                    poDS->papszMetadata = CSLSetNameValue(
                        poDS->papszMetadata, "NC_GLOBAL#Conventions", NULL);
                    poDS->papszMetadata = CSLSetNameValue(
                        poDS->papszMetadata, "NC_GLOBAL#GDAL", NULL);
                    poDS->papszMetadata = CSLSetNameValue(
                        poDS->papszMetadata, "NC_GLOBAL#history", NULL);
                }

                netCDFLayer *poLayer = new netCDFLayer(
                    poDS, poDS->cdfid, osLayerName, eGType, poSRS);
                if( poSRS != NULL )
                    poSRS->Release();
                poLayer->SetRecordDimID(nVectorDim);
                if( wkbFlatten(eGType) == wkbPoint && nVarXId >= 0 &&
                    nVarYId >= 0 )
                {
                    poLayer->SetXYZVars(nVarXId, nVarYId, nVarZId);
                }
                else if( !osGeometryField.empty() )
                {
                    poLayer->SetWKTGeometryField(osGeometryField);
                }
                if( !osGridMapping.empty() )
                {
                    poLayer->SetGridMapping(osGridMapping);
                }
                poLayer->SetProfile(nProfileDimId, nParentIndexVarID);
                poDS->papoLayers = static_cast<netCDFLayer **>(
                    CPLRealloc(poDS->papoLayers,
                               (poDS->nLayers + 1) * sizeof(netCDFLayer *)));
                poDS->papoLayers[poDS->nLayers++] = poLayer;

                for( size_t j = 0; j < anPotentialVectorVarID.size(); j++ )
                {
                    int anDimIds[2] = { -1, -1 };
                    nc_inq_vardimid(poDS->cdfid, anPotentialVectorVarID[j],
                                    anDimIds);
                    if( anDimIds[0] == nVectorDim ||
                        (nProfileDimId >= 0 && anDimIds[0] == nProfileDimId) )
                    {
#ifdef NCDF_DEBUG
                        char szTemp2[NC_MAX_NAME + 1] = {};
                        CPL_IGNORE_RET_VAL(nc_inq_varname(
                            poDS->cdfid, anPotentialVectorVarID[j], szTemp2));
                        CPLDebug("GDAL_netCDF", "Variable %s is a vector field",
                                 szTemp2);
#endif
                        poLayer->AddField(anPotentialVectorVarID[j]);
                    }
                }
            }
        }

#ifdef NETCDF_HAS_NC4
    }  // End for group.
    CPLFree(panGroupIds);
    poDS->cdfid = cdfid;
#endif

    // Case where there is no raster variable
    if( nCount == 0 && !bTreatAsSubdataset )
    {
        poDS->SetMetadata(poDS->papszMetadata);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        poDS->TryLoadXML();
        // If the dataset has been opened in raster mode only, exit
        if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
        {
            delete poDS;
            poDS = NULL;
        }
        // Otherwise if the dataset is opened in vector mode, that there is
        // no vector layer and we are in read-only, exit too.
        else if( poDS->nLayers == 0 &&
                 (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                 poOpenInfo->eAccess == GA_ReadOnly )
        {
            delete poDS;
            poDS = NULL;
        }
        CPLAcquireMutex(hNCMutex, 1000.0);
        return poDS;
    }

    // We have more than one variable with 2 dimensions in the
    // file, then treat this as a subdataset container dataset.
    if( (nCount > 1) && !bTreatAsSubdataset )
    {
        poDS->CreateSubDatasetList();
        poDS->SetMetadata(poDS->papszMetadata);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        poDS->TryLoadXML();
        CPLAcquireMutex(hNCMutex, 1000.0);
        return poDS;
    }

    // If we are not treating things as a subdataset, then capture
    // the name of the single available variable as the subdataset.
    if( !bTreatAsSubdataset )
    {
        char szVarName[NC_MAX_NAME + 1];
        szVarName[0] = '\0';
        status = nc_inq_varname(cdfid, nVarID, szVarName);
        NCDF_ERR(status);
        osSubdatasetName = szVarName;
    }

    // We have ignored at least one variable, so we should report them
    // as subdatasets for reference.
    if( nIgnoredVars > 0 && !bTreatAsSubdataset )
    {
        CPLDebug("GDAL_netCDF",
                 "As %d variables were ignored, creating subdataset list "
                 "for reference. Variable #%d [%s] is the main variable",
                 nIgnoredVars, nVarID, osSubdatasetName.c_str());
        poDS->CreateSubDatasetList();
    }

    // Open the NETCDF subdataset NETCDF:"filename":subdataset.
    int var = -1;
    nc_inq_varid(cdfid, osSubdatasetName, &var);
    int nd = 0;
    nc_inq_varndims(cdfid, var, &nd);

    int *paDimIds = static_cast<int *>(CPLCalloc(nd, sizeof(int)));

    // X, Y, Z position in array
    int *panBandDimPos = static_cast<int *>(CPLCalloc(nd, sizeof(int)));

    nc_inq_vardimid(cdfid, var, paDimIds);

    // Check if somebody tried to pass a variable with less than 2D.
    if( nd < 2 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Variable has %d dimension(s) - not supported.", nd);
        CPLFree(paDimIds);
        CPLFree(panBandDimPos);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    // CF-1 Convention
    //
    // Dimensions to appear in the relative order T, then Z, then Y,
    // then X  to the file. All other dimensions should, whenever
    // possible, be placed to the left of the spatiotemporal
    // dimensions.

    // Verify that dimensions are in the {T,Z,Y,X} or {T,Z,Y,X} order
    // Ideally we should detect for other ordering and act accordingly
    // Only done if file has Conventions=CF-* and only prints warning
    // To disable set GDAL_NETCDF_VERIFY_DIMS=NO and to use only
    // attributes (not varnames) set GDAL_NETCDF_VERIFY_DIMS=STRICT
    const bool bCheckDims =
        CPLTestBool(CPLGetConfigOption("GDAL_NETCDF_VERIFY_DIMS", "YES")) &&
        STARTS_WITH_CI(szConventions, "CF");

    if( bCheckDims )
    {
        char szDimName1[NC_MAX_NAME + 1] = {};
        char szDimName2[NC_MAX_NAME + 1] = {};
        status = nc_inq_dimname(cdfid, paDimIds[nd - 1], szDimName1);
        NCDF_ERR(status);
        status = nc_inq_dimname(cdfid, paDimIds[nd - 2], szDimName2);
        NCDF_ERR(status);
        if( NCDFIsVarLongitude(cdfid, -1, szDimName1) == false &&
            NCDFIsVarProjectionX(cdfid, -1, szDimName1) == false )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "dimension #%d (%s) is not a Longitude/X dimension.",
                     nd - 1, szDimName1);
        }
        if( NCDFIsVarLatitude(cdfid, -1, szDimName2) == false &&
            NCDFIsVarProjectionY(cdfid, -1, szDimName2) == false )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "dimension #%d (%s) is not a Latitude/Y dimension.",
                     nd - 2, szDimName2);
        }
        if( nd >= 3 )
        {
            char szDimName3[NC_MAX_NAME + 1] = {};
            status = nc_inq_dimname(cdfid, paDimIds[nd - 3], szDimName3);
            NCDF_ERR(status);
            if( nd >= 4 )
            {
                char szDimName4[NC_MAX_NAME + 1] = {};
                status = nc_inq_dimname(cdfid, paDimIds[nd - 4], szDimName4);
                NCDF_ERR(status);
                if( NCDFIsVarVerticalCoord( cdfid, -1, szDimName3) == false )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "dimension #%d (%s) is not a Time dimension.",
                             nd - 3, szDimName3);
                }
                if( NCDFIsVarTimeCoord(cdfid, -1, szDimName4) == false )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "dimension #%d (%s) is not a Time dimension.",
                             nd - 4, szDimName4);
                }
            }
            else
            {
                if( NCDFIsVarVerticalCoord(cdfid, -1, szDimName3) == false &&
                    NCDFIsVarTimeCoord(cdfid, -1, szDimName3) == false )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "dimension #%d (%s) is not a "
                             "Time or Vertical dimension.",
                             nd - 3, szDimName3);
                }
            }
        }
    }

    // Get X dimensions information.
    size_t xdim;
    poDS->nXDimID = paDimIds[nd - 1];
    nc_inq_dimlen(cdfid, poDS->nXDimID, &xdim);

    // Get Y dimension information.
    size_t ydim;
    poDS->nYDimID = paDimIds[nd - 2];
    nc_inq_dimlen(cdfid, poDS->nYDimID, &ydim);

    if( xdim > INT_MAX || ydim > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid raster dimensions: " CPL_FRMT_GUIB "x" CPL_FRMT_GUIB,
                 static_cast<GUIntBig>(xdim),
                 static_cast<GUIntBig>(ydim));
        CPLFree(paDimIds);
        CPLFree(panBandDimPos);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    poDS->nRasterXSize = static_cast<int>(xdim);
    poDS->nRasterYSize = static_cast<int>(ydim);

    unsigned int k = 0;
    for( int j = 0; j < nd; j++ )
    {
        if( paDimIds[j] == poDS->nXDimID )
        {
            panBandDimPos[0] = j;  // Save Position of XDim
            k++;
        }
        if( paDimIds[j] == poDS->nYDimID )
        {
            panBandDimPos[1] = j;  // Save Position of YDim
            k++;
        }
    }
    // X and Y Dimension Ids were not found!
    if( k != 2 )
    {
        CPLFree(paDimIds);
        CPLFree(panBandDimPos);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    // Read Metadata for this variable.

    // Should disable as is also done at band level, except driver needs the
    // variables as metadata (e.g. projection).
    poDS->ReadAttributes(cdfid, var);

    // Read Metadata for each dimension.
    for( int j = 0; j < ndims; j++ )
    {
        char szTemp[NC_MAX_NAME + 1];
        status = nc_inq_dimname(cdfid, j, szTemp);
        NCDF_ERR(status);
        poDS->papszDimName.AddString(szTemp);
        int nDimID;
        status = nc_inq_varid(cdfid, poDS->papszDimName[j], &nDimID);
        if( status == NC_NOERR )
        {
            poDS->ReadAttributes(cdfid, nDimID);
        }
    }

    // Set projection info.
    poDS->SetProjectionFromVar(var, false);

    // Override bottom-up with GDAL_NETCDF_BOTTOMUP config option.
    const char *pszValue = CPLGetConfigOption("GDAL_NETCDF_BOTTOMUP", NULL);
    if( pszValue )
    {
        poDS->bBottomUp = CPLTestBool(pszValue);
        CPLDebug("GDAL_netCDF",
                 "set bBottomUp=%d because GDAL_NETCDF_BOTTOMUP=%s",
                 static_cast<int>(poDS->bBottomUp), pszValue);
    }

    // Save non-spatial dimension info.

    int *panBandZLev = NULL;
    int nDim = 2;
    size_t lev_count;
    size_t nTotLevCount = 1;
    nc_type nType = NC_NAT;

    CPLString osExtraDimNames;

    if( nd > 2 )
    {
        nDim = 2;
        panBandZLev = static_cast<int *>(CPLCalloc(nd - 2, sizeof(int)));

        osExtraDimNames = "{";

        char szDimName[NC_MAX_NAME + 1] = {};

        for( int j = 0; j < nd; j++ )
        {
            if( (paDimIds[j] != poDS->nXDimID) &&
                (paDimIds[j] != poDS->nYDimID) )
            {
                nc_inq_dimlen(cdfid, paDimIds[j], &lev_count);
                nTotLevCount *= lev_count;
                panBandZLev[nDim - 2] = static_cast<int>(lev_count);
                panBandDimPos[nDim++] = j;  // Save Position of ZDim
                // Save non-spatial dimension names.
                if( nc_inq_dimname(cdfid, paDimIds[j], szDimName)
                    == NC_NOERR )
                {
                    osExtraDimNames += szDimName;
                    if( j < nd-3 )
                    {
                        osExtraDimNames += ",";
                    }
                    nc_inq_varid(cdfid, szDimName, &nVarID);
                    nc_inq_vartype(cdfid, nVarID, &nType);
                    char szExtraDimDef[NC_MAX_NAME + 1];
                    snprintf(szExtraDimDef, sizeof(szExtraDimDef), "{%ld,%d}",
                             (long)lev_count, nType);
                    char szTemp[NC_MAX_NAME + 32 + 1];
                    snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_DEF",
                             szDimName);
                    poDS->papszMetadata = CSLSetNameValue(
                        poDS->papszMetadata, szTemp, szExtraDimDef);
                    if( NCDFGet1DVar(cdfid, nVarID, &pszTemp) == CE_None )
                    {
                        snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_VALUES",
                                 szDimName);
                        poDS->papszMetadata = CSLSetNameValue(
                            poDS->papszMetadata, szTemp, pszTemp);
                        CPLFree(pszTemp);
                    }
                }
            }
        }
        osExtraDimNames += "}";
        poDS->papszMetadata = CSLSetNameValue(
            poDS->papszMetadata, "NETCDF_DIM_EXTRA", osExtraDimNames);
    }

    // Store Metadata.
    poDS->SetMetadata(poDS->papszMetadata);

    // Create bands.

    // Arbitrary threshold.
    int nMaxBandCount =
        atoi(CPLGetConfigOption("GDAL_MAX_BAND_COUNT", "32768"));
    if( nMaxBandCount <= 0 )
        nMaxBandCount = 32768;
    if( nTotLevCount > static_cast<unsigned int>(nMaxBandCount) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Limiting number of bands to %d instead of %u",
                 nMaxBandCount,
                 static_cast<unsigned int>(nTotLevCount));
        nTotLevCount = static_cast<unsigned int>(nMaxBandCount);
    }
    for( unsigned int lev = 0; lev < nTotLevCount ; lev++ )
    {
        netCDFRasterBand *poBand =
            new netCDFRasterBand(poDS, var, nDim, lev, panBandZLev,
                                 panBandDimPos, paDimIds, lev + 1);
        poDS->SetBand(lev + 1, poBand);
    }

    CPLFree(paDimIds);
    CPLFree(panBandDimPos);
    if( panBandZLev )
        CPLFree(panBandZLev);
    // Handle angular geographic coordinates here

    // Initialize any PAM information.
    if( bTreatAsSubdataset )
    {
        poDS->SetPhysicalFilename(poDS->osFilename);
        poDS->SetSubdatasetName(osSubdatasetName);
    }

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with
                                // GDALDataset own mutex.
    poDS->TryLoadXML();

    if( bTreatAsSubdataset )
        poDS->oOvManager.Initialize(poDS, ":::VIRTUAL:::");
    else
        poDS->oOvManager.Initialize(poDS, poDS->osFilename);

    CPLAcquireMutex(hNCMutex, 1000.0);

    return poDS;
}

/************************************************************************/
/*                            CopyMetadata()                            */
/*                                                                      */
/*      Create a copy of metadata for NC_GLOBAL or a variable           */
/************************************************************************/

static void CopyMetadata( void *poDS, int fpImage, int CDFVarID,
                          const char *pszPrefix, bool bIsBand )
{
    char **papszFieldData = NULL;

    // Remove the following band meta but set them later from band data.
    const char *papszIgnoreBand[] = { CF_ADD_OFFSET, CF_SCALE_FACTOR,
                                      "valid_range", "_Unsigned",
                                      _FillValue, "coordinates",
                                      NULL };
    const char *papszIgnoreGlobal[] = { "NETCDF_DIM_EXTRA", NULL };

    char **papszMetadata = NULL;
    if( CDFVarID == NC_GLOBAL )
    {
        papszMetadata = GDALGetMetadata((GDALDataset *)poDS, "");
    }
    else
    {
        papszMetadata = GDALGetMetadata((GDALRasterBandH)poDS, NULL);
    }

    const int nItems = CSLCount(papszMetadata);

    for( int k = 0; k < nItems; k++ )
    {
        const char *pszField = CSLGetField(papszMetadata, k);
        if( papszFieldData ) CSLDestroy(papszFieldData);
        papszFieldData = CSLTokenizeString2(pszField, "=", CSLT_HONOURSTRINGS);
        if( papszFieldData[1] != NULL )
        {
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF", "copy metadata [%s]=[%s]",
                     papszFieldData[0], papszFieldData[1]);
#endif

            CPLString osMetaName(papszFieldData[0]);
            CPLString osMetaValue(papszFieldData[1]);

            // Check for items that match pszPrefix if applicable.
            if( pszPrefix != NULL && !EQUAL(pszPrefix, "") )
            {
                // Remove prefix.
                if( EQUALN(osMetaName, pszPrefix, strlen(pszPrefix)) )
                {
                    osMetaName = osMetaName.substr(strlen(pszPrefix));
                }
                // Only copy items that match prefix.
                else
                {
                    continue;
                }
            }

            // Fix various issues with metadata translation.
            if( CDFVarID == NC_GLOBAL )
            {
                // Do not copy items in papszIgnoreGlobal and NETCDF_DIM_*.
                if( (CSLFindString(papszIgnoreGlobal, osMetaName) != -1) ||
                    (STARTS_WITH(osMetaName, "NETCDF_DIM_")) )
                    continue;
                // Remove NC_GLOBAL prefix for netcdf global Metadata.
                else if( STARTS_WITH(osMetaName, "NC_GLOBAL#") )
                {
                    osMetaName = osMetaName.substr(strlen("NC_GLOBAL#"));
                }
                // GDAL Metadata renamed as GDAL-[meta].
                else if( strstr(osMetaName, "#") == NULL )
                {
                    osMetaName = "GDAL_" + osMetaName;
                }
                // Keep time, lev and depth information for safe-keeping.
                // Time and vertical coordinate handling need improvements.
                /*
                else if( STARTS_WITH(szMetaName, "time#") )
                {
                    szMetaName[4] = '-';
                }
                else if( STARTS_WITH(szMetaName, "lev#") )
                {
                    szMetaName[3] = '-';
                }
                else if( STARTS_WITH(szMetaName, "depth#") )
                {
                    szMetaName[5] = '-';
                }
                */
                // Only copy data without # (previously all data was copied).
                if( strstr(osMetaName, "#") != NULL )
                    continue;
                // netCDF attributes do not like the '#' character.
                // for( unsigned int h=0; h < strlen(szMetaName) -1 ; h++ ) {
                //     if( szMetaName[h] == '#') szMetaName[h] = '-';
                // }
            }
            else
            {
                // Do not copy varname, stats, NETCDF_DIM_*, nodata
                // and items in papszIgnoreBand.
                if( STARTS_WITH(osMetaName, "NETCDF_VARNAME") ||
                    STARTS_WITH(osMetaName, "STATISTICS_") ||
                    STARTS_WITH(osMetaName, "NETCDF_DIM_") ||
                    STARTS_WITH(osMetaName, "missing_value") ||
                    STARTS_WITH(osMetaName, "_FillValue") ||
                    CSLFindString(papszIgnoreBand, osMetaName) != -1 )
                    continue;
            }

#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF", "copy name=[%s] value=[%s]",
                     osMetaName.c_str(), osMetaValue.c_str());
#endif
            if( NCDFPutAttr(fpImage, CDFVarID, osMetaName,
                            osMetaValue) != CE_None )
                CPLDebug("GDAL_netCDF", "NCDFPutAttr(%d, %d, %s, %s) failed",
                         fpImage, CDFVarID,
                         osMetaName.c_str(), osMetaValue.c_str());
        }
    }

    if( papszFieldData ) CSLDestroy(papszFieldData);

    // Set add_offset and scale_factor here if present.
    if( CDFVarID != NC_GLOBAL && bIsBand )
    {

        GDALRasterBandH poRB = poDS;
        int bGotAddOffset = FALSE;
        const double dfAddOffset = GDALGetRasterOffset(poRB, &bGotAddOffset);
        int bGotScale = FALSE;
        const double dfScale = GDALGetRasterScale(poRB, &bGotScale);

        if( bGotAddOffset && dfAddOffset != 0.0 )
            GDALSetRasterOffset(poRB, dfAddOffset);
        if( bGotScale && dfScale != 1.0 )
            GDALSetRasterScale(poRB, dfScale);
    }
}

/************************************************************************/
/*                            CreateLL()                                */
/*                                                                      */
/*      Shared functionality between netCDFDataset::Create() and        */
/*      netCDF::CreateCopy() for creating netcdf file based on a set of */
/*      options and a configuration.                                    */
/************************************************************************/

netCDFDataset *
netCDFDataset::CreateLL( const char *pszFilename,
                         int nXSize, int nYSize, int nBands,
                         char **papszOptions )
{
    if( !((nXSize == 0 && nYSize == 0 && nBands == 0) ||
          (nXSize > 0 && nYSize > 0 && nBands > 0)) )
    {
        return NULL;
    }

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with
                                // GDALDataset own mutex.
    netCDFDataset *poDS = new netCDFDataset();
    CPLAcquireMutex(hNCMutex, 1000.0);

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->osFilename = pszFilename;

    // From gtiff driver, is this ok?
    /*
    poDS->nBlockXSize = nXSize;
    poDS->nBlockYSize = 1;
    poDS->nBlocksPerBand =
        ((nYSize + poDS->nBlockYSize - 1) / poDS->nBlockYSize)
        * ((nXSize + poDS->nBlockXSize - 1) / poDS->nBlockXSize);
        */

    // process options.
    poDS->papszCreationOptions = CSLDuplicate(papszOptions);
    poDS->ProcessCreationOptions();

    if( poDS->eMultipleLayerBehaviour == SEPARATE_FILES )
    {
        VSIStatBuf sStat;
        if( VSIStat(pszFilename, &sStat) == 0 )
        {
            if( !VSI_ISDIR(sStat.st_mode) )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "%s is an existing file, but not a directory",
                         pszFilename);
                CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                            // deadlock with GDALDataset own
                                            // mutex.
                delete poDS;
                CPLAcquireMutex(hNCMutex, 1000.0);
                return NULL;
            }
        }
        else if( VSIMkdir(pszFilename, 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s directory",
                     pszFilename);
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                        // deadlock with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return NULL;
        }

        return poDS;
    }

    // Create the dataset.
    int status = nc_create(pszFilename, poDS->nCreateMode, &(poDS->cdfid));

    // Put into define mode.
    poDS->SetDefineMode(true);

    if( status != NC_NOERR )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to create netCDF file %s (Error code %d): %s .",
                 pszFilename, status, nc_strerror(status));
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return NULL;
    }

    // Define dimensions.
    if( nXSize > 0 && nYSize > 0 )
    {
        poDS->papszDimName.AddString(NCDF_DIMNAME_X);
        status =
            nc_def_dim(poDS->cdfid, NCDF_DIMNAME_X, nXSize, &(poDS->nXDimID));
        NCDF_ERR(status);
        CPLDebug("GDAL_netCDF", "status nc_def_dim(%d, %s, %d, -) got id %d",
                 poDS->cdfid, NCDF_DIMNAME_X, nXSize, poDS->nXDimID);

        poDS->papszDimName.AddString(NCDF_DIMNAME_Y);
        status =
            nc_def_dim(poDS->cdfid, NCDF_DIMNAME_Y, nYSize, &(poDS->nYDimID));
        NCDF_ERR(status);
        CPLDebug("GDAL_netCDF", "status nc_def_dim(%d, %s, %d, -) got id %d",
                 poDS->cdfid, NCDF_DIMNAME_Y, nYSize, poDS->nYDimID);
    }

    return poDS;
}

/************************************************************************/
/*                            Create()                                  */
/************************************************************************/

GDALDataset *
netCDFDataset::Create( const char *pszFilename,
                       int nXSize, int nYSize, int nBands,
                       GDALDataType eType,
                       char **papszOptions )
{
    CPLDebug("GDAL_netCDF",
              "\n=====\nnetCDFDataset::Create(%s, ...)",
              pszFilename);

    CPLMutexHolderD(&hNCMutex);

    netCDFDataset *poDS = netCDFDataset::CreateLL(pszFilename,
                                                  nXSize, nYSize, nBands,
                                                  papszOptions);
    if( !poDS )
        return NULL;

    // Should we write signed or unsigned byte?
    // TODO should this only be done in Create()
    poDS->bSignedData = true;
    const char *pszValue = CSLFetchNameValueDef(papszOptions, "PIXELTYPE", "");
    if( eType == GDT_Byte && !EQUAL(pszValue, "SIGNEDBYTE") )
        poDS->bSignedData = false;

    // Add Conventions, GDAL info and history.
    if( poDS->cdfid >= 0 )
    {
        NCDFAddGDALHistory(poDS->cdfid, pszFilename, "", "Create",
                           (nBands == 0) ? NCDF_CONVENTIONS_CF_V1_6
                                         : NCDF_CONVENTIONS_CF_V1_5);
    }

    // Define bands.
    for( int iBand = 1; iBand <= nBands; iBand++ )
    {
        poDS->SetBand(
            iBand, new netCDFRasterBand(poDS, eType, iBand, poDS->bSignedData));
    }

    CPLDebug("GDAL_netCDF", "netCDFDataset::Create(%s, ...) done", pszFilename);
    // Return same dataset.
    return poDS;
}

template <class T>
static CPLErr NCDFCopyBand( GDALRasterBand *poSrcBand, GDALRasterBand *poDstBand,
                            int nXSize, int nYSize,
                            GDALProgressFunc pfnProgress, void *pProgressData )
{
    GDALDataType eDT = poSrcBand->GetRasterDataType();
    CPLErr eErr = CE_None;
    T *patScanline = static_cast<T *>(CPLMalloc(nXSize * sizeof(T)));

    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        eErr = poSrcBand->RasterIO(GF_Read, 0, iLine, nXSize, 1, patScanline,
                                   nXSize, 1, eDT, 0, 0, NULL);
        if( eErr != CE_None )
        {
            CPLDebug(
                "GDAL_netCDF",
                "NCDFCopyBand(), poSrcBand->RasterIO() returned error code %d",
                eErr);
        }
        else
        {
            eErr = poDstBand->RasterIO(GF_Write, 0, iLine, nXSize, 1,
                                       patScanline, nXSize, 1, eDT, 0, 0, NULL);
            if( eErr != CE_None )
                CPLDebug("GDAL_netCDF",
                         "NCDFCopyBand(), poDstBand->RasterIO() returned error code %d",
                         eErr);
        }

        if( nYSize > 10 && (iLine % (nYSize / 10) == 1) )
        {
            if( !pfnProgress(1.0 * iLine / nYSize, NULL, pProgressData) )
            {
                eErr = CE_Failure;
                CPLError(CE_Failure, CPLE_UserInterrupt,
                         "User terminated CreateCopy()");
            }
        }
    }

    CPLFree(patScanline);

    pfnProgress(1.0, NULL, pProgressData);

    return eErr;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset*
netCDFDataset::CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                           CPL_UNUSED int bStrict, char **papszOptions,
                           GDALProgressFunc pfnProgress, void *pProgressData )
{
    CPLMutexHolderD(&hNCMutex);

    CPLDebug("GDAL_netCDF", "\n=====\nnetCDFDataset::CreateCopy(%s, ...)",
             pszFilename);

    const int nBands = poSrcDS->GetRasterCount();
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const char *pszWKT = poSrcDS->GetProjectionRef();

    // Check input bands for errors.
    if( nBands == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "NetCDF driver does not support "
                 "source dataset with zero band.");
        return NULL;
    }

    GDALDataType eDT;
    GDALRasterBand *poSrcBand = NULL;
    for( int iBand = 1; iBand <= nBands; iBand++ )
    {
        poSrcBand = poSrcDS->GetRasterBand(iBand);
        eDT = poSrcBand->GetRasterDataType();
        if( eDT == GDT_Unknown || GDALDataTypeIsComplex(eDT) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "NetCDF driver does not support source dataset with band "
                     "of complex type.");
            return NULL;
        }
    }

    if( !pfnProgress(0.0, NULL, pProgressData) )
        return NULL;

    // Same as in Create().
    netCDFDataset *poDS = netCDFDataset::CreateLL(pszFilename,
                                                   nXSize, nYSize, nBands,
                                                   papszOptions);
    if( !poDS )
        return NULL;

    // Copy global metadata.
    // Add Conventions, GDAL info and history.
    CopyMetadata((void *)poSrcDS, poDS->cdfid, NC_GLOBAL, NULL, false);
    NCDFAddGDALHistory(poDS->cdfid, pszFilename,
                       poSrcDS->GetMetadataItem("NC_GLOBAL#history", ""),
                       "CreateCopy");

    pfnProgress(0.1, NULL, pProgressData);

    // Check for extra dimensions.
    int nDim = 2;
    char **papszExtraDimNames =
        NCDFTokenizeArray(poSrcDS->GetMetadataItem("NETCDF_DIM_EXTRA", ""));
    char **papszExtraDimValues = NULL;

    if( papszExtraDimNames != NULL && CSLCount(papszExtraDimNames) > 0 )
    {
        size_t nDimSizeTot = 1;
        // first make sure dimensions lengths compatible with band count
        // for( int i=0; i<CSLCount(papszExtraDimNames ); i++ ) {
        for( int i = CSLCount(papszExtraDimNames) - 1; i >= 0; i-- )
        {
            char szTemp[NC_MAX_NAME + 32 + 1];
            snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_DEF",
                     papszExtraDimNames[i]);
            papszExtraDimValues =
                NCDFTokenizeArray(poSrcDS->GetMetadataItem(szTemp, ""));
            const size_t nDimSize = atol(papszExtraDimValues[0]);
            CSLDestroy(papszExtraDimValues);
            nDimSizeTot *= nDimSize;
        }
        if( nDimSizeTot == (size_t)nBands )
        {
            nDim = 2 + CSLCount(papszExtraDimNames);
        }
        else
        {
            // if nBands != #bands computed raise a warning
            // just issue a debug message, because it was probably intentional
            CPLDebug("GDAL_netCDF",
                     "Warning: Number of bands (%d) is not compatible with dimensions "
                     "(total=%ld names=%s)", nBands, (long)nDimSizeTot,
                     poSrcDS->GetMetadataItem("NETCDF_DIM_EXTRA", ""));
            CSLDestroy(papszExtraDimNames);
            papszExtraDimNames = NULL;
        }
    }

    int *panDimIds = static_cast<int *>(CPLCalloc(nDim, sizeof(int)));
    int *panBandDimPos = static_cast<int *>(CPLCalloc(nDim, sizeof(int)));

    nc_type nVarType;
    int status = NC_NOERR;
    int *panBandZLev = NULL;
    int *panDimVarIds = NULL;

    if( nDim > 2 )
    {
        panBandZLev = static_cast<int *>(CPLCalloc(nDim - 2, sizeof(int)));
        panDimVarIds = static_cast<int *>(CPLCalloc(nDim - 2, sizeof(int)));

        // Define all dims.
        for( int i = CSLCount(papszExtraDimNames) - 1; i >= 0; i-- )
        {
            poDS->papszDimName.AddString(papszExtraDimNames[i]);
            char szTemp[NC_MAX_NAME + 32 + 1];
            snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_DEF",
                     papszExtraDimNames[i]);
            papszExtraDimValues =
                NCDFTokenizeArray(poSrcDS->GetMetadataItem(szTemp, ""));
            const int nDimSize = atoi(papszExtraDimValues[0]);
            // nc_type is an enum in netcdf-3, needs casting.
            nVarType = static_cast<nc_type>(atol(papszExtraDimValues[1]));
            CSLDestroy(papszExtraDimValues);
            panBandZLev[i] = nDimSize;
            panBandDimPos[i + 2] = i;  // Save Position of ZDim.

            // Define dim.
            status = nc_def_dim(poDS->cdfid, papszExtraDimNames[i], nDimSize,
                                &(panDimIds[i]));
            NCDF_ERR(status);

            // Define dim var.
            int anDim[1] = {panDimIds[i]};
            status = nc_def_var(poDS->cdfid, papszExtraDimNames[i], nVarType, 1,
                                anDim, &(panDimVarIds[i]));
            NCDF_ERR(status);

            // Add dim metadata, using global var# items.
            snprintf(szTemp, sizeof(szTemp), "%s#", papszExtraDimNames[i]);
            CopyMetadata(poSrcDS, poDS->cdfid, panDimVarIds[i], szTemp, false);
        }
    }

    // Copy GeoTransform and Projection.

    // Copy geolocation info.
    if( poSrcDS->GetMetadata("GEOLOCATION") != NULL )
        poDS->SetMetadata(poSrcDS->GetMetadata("GEOLOCATION"), "GEOLOCATION");

    // Copy geotransform.
    bool bGotGeoTransform = false;
    double adfGeoTransform[6];
    CPLErr eErr = poSrcDS->GetGeoTransform(adfGeoTransform);
    if( eErr == CE_None )
    {
        poDS->SetGeoTransform(adfGeoTransform);
        // Disable AddProjectionVars() from being called.
        bGotGeoTransform = true;
        poDS->bSetGeoTransform = false;
    }

    // Copy projection.
    void *pScaledProgress = NULL;
    if( pszWKT )
    {
        poDS->SetProjection(pszWKT);
        // Now we can call AddProjectionVars() directly.
        poDS->bSetGeoTransform = bGotGeoTransform;
        pScaledProgress =
            GDALCreateScaledProgress(0.1, 0.25, pfnProgress, pProgressData);
        poDS->AddProjectionVars(GDALScaledProgress, pScaledProgress);
        // Save X,Y dim positions.
        panDimIds[nDim - 1] = poDS->nXDimID;
        panBandDimPos[0] = nDim - 1;
        panDimIds[nDim - 2] = poDS->nYDimID;
        panBandDimPos[1] = nDim - 2;
        GDALDestroyScaledProgress(pScaledProgress);
    }

    // Write extra dim values - after projection for optimization.
    if( nDim > 2 )
    {
        // Make sure we are in data mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(false);
        for( int i = CSLCount(papszExtraDimNames) - 1; i >= 0; i-- )
        {
            char szTemp[NC_MAX_NAME + 32 + 1];
            snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_VALUES",
                     papszExtraDimNames[i]);
            if( poSrcDS->GetMetadataItem(szTemp) != NULL )
            {
                NCDFPut1DVar(poDS->cdfid, panDimVarIds[i],
                             poSrcDS->GetMetadataItem(szTemp));
            }
        }
    }

    pfnProgress(0.25, NULL, pProgressData);

    // Define Bands.
    netCDFRasterBand *poBand = NULL;
    int nBandID = -1;

    for( int iBand = 1; iBand <= nBands; iBand++ )
    {
        CPLDebug("GDAL_netCDF", "creating band # %d/%d nDim = %d",
                 iBand, nBands, nDim);

        poSrcBand = poSrcDS->GetRasterBand(iBand);
        eDT = poSrcBand->GetRasterDataType();

        // Get var name from NETCDF_VARNAME.
        const char *tmpMetadata = poSrcBand->GetMetadataItem("NETCDF_VARNAME");
        char szBandName[NC_MAX_NAME + 1];
        if( tmpMetadata != NULL )
        {
            if( nBands > 1 && papszExtraDimNames == NULL )
                snprintf(szBandName, sizeof(szBandName),
                         "%s%d", tmpMetadata, iBand);
            else
                snprintf(szBandName, sizeof(szBandName), "%s", tmpMetadata);
        }
        else
        {
            szBandName[0] = '\0';
}

        // Get long_name from <var>#long_name.
        char szLongName[NC_MAX_NAME + 1];
        snprintf(szLongName, sizeof(szLongName), "%s#%s",
                 poSrcBand->GetMetadataItem("NETCDF_VARNAME"), CF_LNG_NAME);
        tmpMetadata = poSrcDS->GetMetadataItem(szLongName);
        if( tmpMetadata != NULL)
            snprintf(szLongName, sizeof(szLongName), "%s", tmpMetadata);
        else
            szLongName[0] = '\0';

        bool bSignedData = true;
        if( eDT == GDT_Byte )
        {
            // GDAL defaults to unsigned bytes, but check if metadata says its
            // signed, as NetCDF can support this for certain formats.
            bSignedData = false;
            tmpMetadata =
                poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            if( tmpMetadata && EQUAL(tmpMetadata, "SIGNEDBYTE") )
                bSignedData = true;
        }

        if( nDim > 2 )
            poBand = new netCDFRasterBand(
                poDS, eDT, iBand, bSignedData, szBandName, szLongName, nBandID,
                nDim, iBand - 1, panBandZLev, panBandDimPos, panDimIds);
        else
            poBand = new netCDFRasterBand(poDS, eDT, iBand, bSignedData,
                                          szBandName, szLongName);

        poDS->SetBand(iBand, poBand);

        // Set nodata value, if any.
        // poBand->SetNoDataValue(poSrcBand->GetNoDataValue(0));
        int bNoDataSet = FALSE;
        double dfNoDataValue = poSrcBand->GetNoDataValue(&bNoDataSet);
        if( bNoDataSet )
        {
            CPLDebug("GDAL_netCDF", "SetNoDataValue(%f) source", dfNoDataValue);
            poBand->SetNoDataValue(dfNoDataValue);
        }

        // Copy Metadata for band.
        CopyMetadata((void *)GDALGetRasterBand(poSrcDS, iBand),
                      poDS->cdfid, poBand->nZId);

        // If more than 2D pass the first band's netcdf var ID to subsequent
        // bands.
        if( nDim > 2 )
            nBandID = poBand->nZId;
    }

    // Write projection variable to band variable.
    poDS->AddGridMappingRef();

    pfnProgress(0.5, NULL, pProgressData);

    // Write bands.

    // Make sure we are in data mode.
    poDS->SetDefineMode(false);

    double dfTemp = 0.5;

    eErr = CE_None;

    for( int iBand = 1; iBand <= nBands && eErr == CE_None; iBand++ )
    {
        const double dfTemp2 = dfTemp + 0.4 / nBands;
        pScaledProgress = GDALCreateScaledProgress(dfTemp, dfTemp2, pfnProgress,
                                                   pProgressData);
        dfTemp = dfTemp2;

        CPLDebug("GDAL_netCDF", "copying band data # %d/%d ", iBand, nBands);

        poSrcBand = poSrcDS->GetRasterBand(iBand);
        eDT = poSrcBand->GetRasterDataType();

        GDALRasterBand *poDstBand = poDS->GetRasterBand(iBand);

        // Copy band data.
        if( eDT == GDT_Byte )
        {
            CPLDebug("GDAL_netCDF", "GByte Band#%d", iBand);
            eErr = NCDFCopyBand<GByte>(poSrcBand, poDstBand, nXSize, nYSize,
                                       GDALScaledProgress, pScaledProgress);
        }
        else if( (eDT == GDT_UInt16) || (eDT == GDT_Int16) )
        {
            CPLDebug("GDAL_netCDF", "GInt16 Band#%d", iBand);
            eErr = NCDFCopyBand<GInt16>(poSrcBand, poDstBand, nXSize, nYSize,
                                        GDALScaledProgress, pScaledProgress);
        }
        else if( (eDT == GDT_UInt32) || (eDT == GDT_Int32) )
        {
            CPLDebug("GDAL_netCDF", "GInt16 Band#%d", iBand);
            eErr = NCDFCopyBand<GInt32>(poSrcBand, poDstBand, nXSize, nYSize,
                                        GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_Float32 )
        {
            CPLDebug("GDAL_netCDF", "float Band#%d", iBand);
            eErr = NCDFCopyBand<float>(poSrcBand, poDstBand, nXSize, nYSize,
                                       GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_Float64 )
        {
            CPLDebug("GDAL_netCDF", "double Band#%d", iBand);
            eErr = NCDFCopyBand<double>(poSrcBand, poDstBand, nXSize, nYSize,
                                        GDALScaledProgress, pScaledProgress);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                      "The NetCDF driver does not support GDAL data type %d",
                      eDT);
        }

        GDALDestroyScaledProgress(pScaledProgress);
    }

    delete(poDS);

    CPLFree(panDimIds);
    CPLFree(panBandDimPos);
    CPLFree(panBandZLev);
    CPLFree(panDimVarIds);
    if( papszExtraDimNames )
        CSLDestroy(papszExtraDimNames);

    if( eErr != CE_None )
        return NULL;

    pfnProgress(0.95, NULL, pProgressData);

    // Re-open dataset so we can return it.
    poDS =
        reinterpret_cast<netCDFDataset *>(GDALOpen(pszFilename, GA_ReadOnly));

    // PAM cloning is disabled. See bug #4244.
    // if( poDS )
    //     poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    pfnProgress(1.0, NULL, pProgressData);

    return poDS;
}

// Note: some logic depends on bIsProjected and bIsGeoGraphic.
// May not be known when Create() is called, see AddProjectionVars().
void netCDFDataset::ProcessCreationOptions()
{
    const char *pszConfig =
        CSLFetchNameValue(papszCreationOptions, "CONFIG_FILE");
    if( pszConfig != NULL )
    {
        if( oWriterConfig.Parse(pszConfig) )
        {
            // Override dataset creation options from the config file
            std::map<CPLString, CPLString>::iterator oIter;
            for( oIter = oWriterConfig.m_oDatasetCreationOptions.begin();
                 oIter != oWriterConfig.m_oDatasetCreationOptions.end();
                 ++ oIter )
            {
                papszCreationOptions = CSLSetNameValue(
                    papszCreationOptions, oIter->first, oIter->second);
            }
        }
    }

    // File format.
    eFormat = NCDF_FORMAT_NC;
    const char *pszValue = CSLFetchNameValue(papszCreationOptions, "FORMAT");
    if( pszValue != NULL )
    {
        if( EQUAL(pszValue, "NC") )
        {
            eFormat = NCDF_FORMAT_NC;
        }
#ifdef NETCDF_HAS_NC2
        else if( EQUAL(pszValue, "NC2") )
        {
            eFormat = NCDF_FORMAT_NC2;
        }
#endif
#ifdef NETCDF_HAS_NC4
        else if( EQUAL(pszValue, "NC4") )
        {
            eFormat = NCDF_FORMAT_NC4;
        }
        else if( EQUAL(pszValue, "NC4C") )
        {
            eFormat = NCDF_FORMAT_NC4C;
        }
#endif
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "FORMAT=%s in not supported, using the default NC format.",
                     pszValue);
        }
    }

    // Compression only available for NC4.
#ifdef NETCDF_HAS_NC4

    // COMPRESS option.
    pszValue = CSLFetchNameValue(papszCreationOptions, "COMPRESS");
    if( pszValue != NULL )
    {
        if( EQUAL(pszValue, "NONE") )
        {
            eCompress = NCDF_COMPRESS_NONE;
        }
        else if( EQUAL(pszValue, "DEFLATE") )
        {
            eCompress = NCDF_COMPRESS_DEFLATE;
            if( !((eFormat == NCDF_FORMAT_NC4) ||
                  (eFormat == NCDF_FORMAT_NC4C)) )
            {
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "NOTICE: Format set to NC4C because compression is "
                         "set to DEFLATE.");
                eFormat = NCDF_FORMAT_NC4C;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                      "COMPRESS=%s is not supported.", pszValue);
        }
    }

    // ZLEVEL option.
    pszValue = CSLFetchNameValue(papszCreationOptions, "ZLEVEL");
    if( pszValue != NULL )
    {
        nZLevel = atoi(pszValue);
        if( !(nZLevel >= 1 && nZLevel <= 9) )
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "ZLEVEL=%s value not recognised, ignoring.", pszValue);
            nZLevel = NCDF_DEFLATE_LEVEL;
        }
    }

    // CHUNKING option.
    bChunking =
        CPL_TO_BOOL(CSLFetchBoolean(papszCreationOptions, "CHUNKING", TRUE));

#endif

    // MULTIPLE_LAYERS option.
    const char *pszMultipleLayerBehaviour =
        CSLFetchNameValueDef(papszCreationOptions, "MULTIPLE_LAYERS", "NO");
    if( EQUAL(pszMultipleLayerBehaviour, "NO") )
    {
        eMultipleLayerBehaviour = SINGLE_LAYER;
    }
    else if( EQUAL(pszMultipleLayerBehaviour, "SEPARATE_FILES") )
    {
        eMultipleLayerBehaviour = SEPARATE_FILES;
    }
#ifdef NETCDF_HAS_NC4
    else if( EQUAL(pszMultipleLayerBehaviour, "SEPARATE_GROUPS") )
    {
        if( eFormat == NCDF_FORMAT_NC4 )
        {
            eMultipleLayerBehaviour = SEPARATE_GROUPS;
        }
        else
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "MULTIPLE_LAYERS=%s is recognised only with FORMAT=NC4",
                     pszMultipleLayerBehaviour);
        }
    }
#endif
    else
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "MULTIPLE_LAYERS=%s not recognised",
                 pszMultipleLayerBehaviour);
    }

    // Set nCreateMode based on eFormat.
    switch( eFormat )
    {
#ifdef NETCDF_HAS_NC2
    case NCDF_FORMAT_NC2:
        nCreateMode = NC_CLOBBER | NC_64BIT_OFFSET;
        break;
#endif
#ifdef NETCDF_HAS_NC4
    case NCDF_FORMAT_NC4:
        nCreateMode = NC_CLOBBER | NC_NETCDF4;
        break;
    case NCDF_FORMAT_NC4C:
        nCreateMode = NC_CLOBBER | NC_NETCDF4 | NC_CLASSIC_MODEL;
        break;
#endif
    case NCDF_FORMAT_NC:
    default:
        nCreateMode = NC_CLOBBER;
        break;
    }

    CPLDebug("GDAL_netCDF", "file options: format=%d compress=%d zlevel=%d",
             eFormat, eCompress, nZLevel);
}

int netCDFDataset::DefVarDeflate(
#ifdef NETCDF_HAS_NC4
    int nVarId, bool bChunkingArg
#else
    int /* nVarId */, bool /* bChunkingArg */
#endif
    )
{
#ifdef NETCDF_HAS_NC4
    if( eCompress == NCDF_COMPRESS_DEFLATE )
    {
        // Must set chunk size to avoid huge performance hit (set
        // bChunkingArg=TRUE)
        // perhaps another solution it to change the chunk cache?
        // http://www.unidata.ucar.edu/software/netcdf/docs/netcdf.html#Chunk-Cache
        // TODO: make sure this is okay.
        CPLDebug("GDAL_netCDF", "DefVarDeflate(%d, %d) nZlevel=%d", nVarId,
                 static_cast<int>(bChunkingArg), nZLevel);

        int status = nc_def_var_deflate(cdfid, nVarId, 1, 1, nZLevel);
        NCDF_ERR(status);

        if( status == NC_NOERR && bChunkingArg && bChunking )
        {
            // set chunking to be 1 for all dims, except X dim
            // size_t chunksize[] = { 1, (size_t)nRasterXSize };
            size_t chunksize[MAX_NC_DIMS];
            int nd;
            nc_inq_varndims(cdfid, nVarId, &nd);
            chunksize[0] = (size_t)1;
            chunksize[1] = (size_t)1;
            for( int i = 2; i < nd; i++) chunksize[i] = (size_t)1;
            chunksize[nd - 1] = (size_t)nRasterXSize;

            CPLDebug("GDAL_netCDF",
                     "DefVarDeflate() chunksize={%ld, %ld} chunkX=%ld nd=%d",
                     (long)chunksize[0], (long)chunksize[1],
                     (long)chunksize[nd - 1], nd);
#ifdef NCDF_DEBUG
            for( int i = 0; i < nd; i++ )
                CPLDebug("GDAL_netCDF", "DefVarDeflate() chunk[%d]=%ld",
                         i, chunksize[i]);
#endif

            status = nc_def_var_chunking(cdfid, nVarId, NC_CHUNKED, chunksize);
            NCDF_ERR(status);
        }
        else
        {
            CPLDebug("GDAL_netCDF", "chunksize not set");
        }
        return status;
    }
#endif
    return NC_NOERR;
}

/************************************************************************/
/*                           NCDFUnloadDriver()                         */
/************************************************************************/

static void NCDFUnloadDriver(CPL_UNUSED GDALDriver *poDriver)
{
    if( hNCMutex != NULL )
        CPLDestroyMutex(hNCMutex);
    hNCMutex = NULL;
}

/************************************************************************/
/*                          GDALRegister_netCDF()                       */
/************************************************************************/

void GDALRegister_netCDF()

{
    if( !GDAL_CHECK_VERSION("netCDF driver") )
        return;

    if( GDALGetDriverByName("netCDF") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    // Set the driver details.
    poDriver->SetDescription("netCDF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Network Common Data Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_netcdf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "nc");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='NC'>"
"     <Value>NC</Value>"
#ifdef NETCDF_HAS_NC2
"     <Value>NC2</Value>"
#endif
#ifdef NETCDF_HAS_NC4
"     <Value>NC4</Value>"
"     <Value>NC4C</Value>"
#endif
"   </Option>"
#ifdef NETCDF_HAS_NC4
"   <Option name='COMPRESS' type='string-select' default='NONE'>"
"     <Value>NONE</Value>"
"     <Value>DEFLATE</Value>"
"   </Option>"
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='1'/>"
#endif
"   <Option name='WRITE_BOTTOMUP' type='boolean' default='YES'>"
"   </Option>"
"   <Option name='WRITE_GDAL_TAGS' type='boolean' default='YES'>"
"   </Option>"
"   <Option name='WRITE_LONLAT' type='string-select'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"   </Option>"
"   <Option name='TYPE_LONLAT' type='string-select'>"
"     <Value>float</Value>"
"     <Value>double</Value>"
"   </Option>"
"   <Option name='PIXELTYPE' type='string-select' description='only used in Create()'>"
"       <Value>DEFAULT</Value>"
"       <Value>SIGNEDBYTE</Value>"
"   </Option>"
"   <Option name='CHUNKING' type='boolean' default='YES' description='define chunking when creating netcdf4 file'/>"
"   <Option name='MULTIPLE_LAYERS' type='string-select' description='Behaviour regarding multiple vector layer creation' default='NO'>"
"       <Value>NO</Value>"
"       <Value>SEPARATE_FILES</Value>"
#ifdef NETCDF_HAS_NC4
"       <Value>SEPARATE_GROUPS</Value>"
#endif
"   </Option>"
"   <Option name='CONFIG_FILE' type='string' description='Path to a XML configuration file (or content inlined)'/>"
"</CreationOptionList>"
                              );
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"   <Option name='RECORD_DIM_NAME' type='string' description='Name of the unlimited dimension' default='record'/>"
"   <Option name='STRING_DEFAULT_WIDTH' type='int' description='"
#ifdef NETCDF_HAS_NC4
"For non-NC4 format, "
#endif
"default width of strings. Default is 10 in autogrow mode, 80 otherwise.'/>"
"   <Option name='WKT_DEFAULT_WIDTH' type='int' description='"
#ifdef NETCDF_HAS_NC4
"For non-NC4 format, "
#endif
"default width of WKT strings. Default is 1000 in autogrow mode, 10000 otherwise.'/>"
"   <Option name='AUTOGROW_STRINGS' type='boolean' description='Whether to auto-grow non-bounded string fields of bidimensional char variable' default='YES'/>"
#ifdef NETCDF_HAS_NC4
"   <Option name='USE_STRING_IN_NC4' type='boolean' description='Whether to use NetCDF string type for strings in NC4 format. If NO, bidimensional char variable are used' default='YES'/>"
#if 0
"   <Option name='NCDUMP_COMPAT' type='boolean' description='When USE_STRING_IN_NC4=YEs, whether to use empty string instead of null string to avoid crashes with ncdump' default='NO'/>"
#endif
#endif
"   <Option name='FEATURE_TYPE' type='string-select' description='CF FeatureType' default='AUTO'>"
"       <Value>AUTO</Value>"
"       <Value>POINT</Value>"
"       <Value>PROFILE</Value>"
"   </Option>"
"   <Option name='PROFILE_DIM_NAME' type='string' description='Name of the profile dimension and variable' default='profile'/>"
"   <Option name='PROFILE_DIM_INIT_SIZE' type='string' description='Initial size of profile dimension (default 100), or UNLIMITED for NC4 files'/>"
"   <Option name='PROFILE_VARIABLES' type='string' description='Comma separated list of field names that must be indexed by the profile dimension'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='HONOUR_VALID_RANGE' type='boolean' "
    "description='Whether to set to nodata pixel values outside of the "
    "validity range' default='YES'/>"
"</OpenOptionList>" );


    // Make driver config and capabilities available.
    poDriver->SetMetadataItem("NETCDF_VERSION", nc_inq_libvers());
    poDriver->SetMetadataItem("NETCDF_CONVENTIONS", NCDF_CONVENTIONS_CF_V1_5);
#ifdef NETCDF_HAS_NC2
    poDriver->SetMetadataItem("NETCDF_HAS_NC2", "YES");
#endif
#ifdef NETCDF_HAS_NC4
    poDriver->SetMetadataItem("NETCDF_HAS_NC4", "YES");
#endif
#ifdef NETCDF_HAS_HDF4
    poDriver->SetMetadataItem("NETCDF_HAS_HDF4", "YES");
#endif
#ifdef HAVE_HDF4
    poDriver->SetMetadataItem("GDAL_HAS_HDF4", "YES");
#endif
#ifdef HAVE_HDF5
    poDriver->SetMetadataItem("GDAL_HAS_HDF5", "YES");
#endif

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime");

    // Set pfns and register driver.
    poDriver->pfnOpen = netCDFDataset::Open;
    poDriver->pfnCreateCopy = netCDFDataset::CreateCopy;
    poDriver->pfnCreate = netCDFDataset::Create;
    poDriver->pfnIdentify = netCDFDataset::Identify;
    poDriver->pfnUnloadDriver = NCDFUnloadDriver;

    GetGDALDriverManager()->RegisterDriver(poDriver);

#ifdef NETCDF_PLUGIN
    GDALRegister_GMT();
#endif
}

/************************************************************************/
/*                          New functions                               */
/************************************************************************/

/* Test for GDAL version string >= target */
static bool NCDFIsGDALVersionGTE(const char *pszVersion, int nTarget)
{

    // Valid strings are "GDAL 1.9dev, released 2011/01/18" and "GDAL 1.8.1 ".
    if( pszVersion == NULL || EQUAL(pszVersion, "") )
        return false;
    else if( !STARTS_WITH_CI(pszVersion, "GDAL ") )
        return false;
    // 2.0dev of 2011/12/29 has been later renamed as 1.10dev.
    else if( EQUAL("GDAL 2.0dev, released 2011/12/29", pszVersion) )
        return nTarget <= GDAL_COMPUTE_VERSION(1, 10, 0);
    else if( STARTS_WITH_CI(pszVersion, "GDAL 1.9dev") )
        return nTarget <= 1900;
    else if( STARTS_WITH_CI(pszVersion, "GDAL 1.8dev") )
        return nTarget <= 1800;

    char **papszTokens = CSLTokenizeString2(pszVersion + 5, ".", 0);

    int nVersions[] = {0, 0, 0, 0};
    for( int iToken = 0;
         papszTokens && iToken < 4 && papszTokens[iToken];
         iToken++ )
    {
        nVersions[iToken] = atoi(papszTokens[iToken]);
    }

    int nVersion = 0;
    if( nVersions[0] > 1 || nVersions[1] >= 10 )
        nVersion =
            GDAL_COMPUTE_VERSION(nVersions[0], nVersions[1], nVersions[2]);
    else
        nVersion = nVersions[0] * 1000 + nVersions[1] * 100 +
                   nVersions[2] * 10 + nVersions[3];

    CSLDestroy(papszTokens);
    return nTarget <= nVersion;
}

// Add Conventions, GDAL version and history.
static void NCDFAddGDALHistory( int fpImage,
                                const char *pszFilename,
                                const char *pszOldHist,
                                const char *pszFunctionName,
                                const char *pszCFVersion )
{
    int status = nc_put_att_text(fpImage, NC_GLOBAL, "Conventions",
                                 strlen(pszCFVersion), pszCFVersion);
    NCDF_ERR(status);

    const char *pszNCDF_GDAL = GDALVersionInfo("--version");
    status = nc_put_att_text(fpImage, NC_GLOBAL, "GDAL",
                             strlen(pszNCDF_GDAL), pszNCDF_GDAL);
    NCDF_ERR(status);

    // Add history.
    CPLString osTmp;
#ifdef GDAL_SET_CMD_LINE_DEFINED_TMP
    if( !EQUAL(GDALGetCmdLine(), "") )
        osTmp = GDALGetCmdLine();
    else
        osTmp = CPLSPrintf("GDAL %s( %s, ... )", pszFunctionName, pszFilename);
#else
    osTmp = CPLSPrintf("GDAL %s( %s, ... )", pszFunctionName, pszFilename);
#endif

    NCDFAddHistory(fpImage, osTmp.c_str(), pszOldHist);
}

// Code taken from cdo and libcdi, used for writing the history attribute.

// void cdoDefHistory(int fileID, char *histstring)
static void NCDFAddHistory(int fpImage, const char *pszAddHist,
                           const char *pszOldHist)
{
    // Check pszOldHist - as if there was no previous history, it will be
    // a null pointer - if so set as empty.
    if( NULL == pszOldHist )
    {
        pszOldHist = "";
    }

    char strtime[32];
    strtime[0] = '\0';

    time_t tp = time(NULL);
    if( tp != -1 )
    {
        struct tm *ltime = localtime(&tp);
        (void)strftime(strtime, sizeof(strtime), "%a %b %d %H:%M:%S %Y: ",
                       ltime);
    }

    // status = nc_get_att_text(fpImage, NC_GLOBAL,
    //                           "history", pszOldHist);
    // printf("status: %d pszOldHist: [%s]\n",status,pszOldHist);

    size_t nNewHistSize =
        strlen(pszOldHist) + strlen(strtime) + strlen(pszAddHist) + 1 + 1;
    char *pszNewHist =
        static_cast<char *>(CPLMalloc(nNewHistSize * sizeof(char)));

    strcpy(pszNewHist, strtime);
    strcat(pszNewHist, pszAddHist);

    // int disableHistory = FALSE;
    // if( !disableHistory )
    {
        if( !EQUAL(pszOldHist, "") )
            strcat(pszNewHist, "\n");
        strcat(pszNewHist, pszOldHist);
    }

    const int status = nc_put_att_text(fpImage, NC_GLOBAL, "history",
                                       strlen(pszNewHist), pszNewHist);
    NCDF_ERR(status);

    CPLFree(pszNewHist);
}

static bool NCDFIsCfProjection( const char *pszProjection )
{
    // Find the appropriate mapping.
    for( int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != NULL; iMap++ )
    {
#ifdef NCDF_DEBUG
      CPLDebug("GDAL_netCDF", "now at %d, proj=%s",
               iMap, poNetcdfSRS_PT[iMap].WKT_SRS);
#endif
      if( EQUAL(pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS) )
        {
            return poNetcdfSRS_PT[iMap].mappings != NULL;
        }
    }
    return false;
}

/* Write any needed projection attributes *
 * poPROJCS: ptr to proj crd system
 * pszProjection: name of projection system in GDAL WKT
 * fpImage: open NetCDF file in writing mode
 * NCDFVarID: NetCDF Var Id of proj system we're writing in to
 *
 * The function first looks for the oNetcdfSRS_PP mapping object
 * that corresponds to the input projection name. If none is found
 * the generic mapping is used.  In the case of specific mappings,
 * the driver looks for each attribute listed in the mapping object
 * and then looks up the value within the OGR_SRSNode. In the case
 * of the generic mapping, the lookup is reversed (projection params,
 * then mapping).  For more generic code, GDAL->NETCDF
 * mappings and the associated value are saved in std::map objects.
 */

// NOTE: modifications by ET to combine the specific and generic mappings.

static void NCDFWriteProjAttribs( const OGR_SRSNode *poPROJCS,
                                  const char *pszProjection,
                                  const int fpImage, const int NCDFVarID )
{
    const oNetcdfSRS_PP *poMap = NULL;
    int nMapIndex = -1;

    // Find the appropriate mapping.
    for( int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != NULL; iMap++ )
    {
        if( EQUAL(pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS) )
        {
            nMapIndex = iMap;
            poMap = poNetcdfSRS_PT[iMap].mappings;
            break;
        }
    }

    // ET TODO if projection name is not found, should we do something special?
    if( nMapIndex == -1 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "projection name %s not found in the lookup tables!",
                 pszProjection);
    }
    // If no mapping was found or assigned, set the generic one.
    if( !poMap )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "projection name %s in not part of the CF standard, "
                 "will not be supported by CF!",
                 pszProjection);
        poMap = poGenericMappings;
    }

    // Initialize local map objects.

    // Attribute <GDAL,NCDF> and Value <NCDF,value> mappings
    std::map<std::string, std::string> oAttMap;
    for( int iMap = 0; poMap[iMap].WKT_ATT != NULL; iMap++ )
    {
        oAttMap[poMap[iMap].WKT_ATT] = poMap[iMap].CF_ATT;
    }

    const char *pszParamVal = NULL;
    std::map<std::string, double> oValMap;
    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poNode = poPROJCS->GetChild(iChild);
        if( !EQUAL(poNode->GetValue(), "PARAMETER") ||
            poNode->GetChildCount() != 2 )
            continue;
        const char *pszParamStr = poNode->GetChild(0)->GetValue();
        pszParamVal = poNode->GetChild(1)->GetValue();

        oValMap[pszParamStr] = CPLAtof(pszParamVal);
    }

    double dfValue = 0.0;
    const std::string *posGDALAtt;
    std::map<std::string, std::string>::iterator oAttIter;
    std::map<std::string, double>::iterator oValIter, oValIter2;

    // Results to write.
    std::vector<std::pair<std::string, double> > oOutList;

    // Lookup mappings and fill output vector.
    if(poMap != poGenericMappings)
    {
        // Specific mapping, loop over mapping values.
        for( oAttIter = oAttMap.begin(); oAttIter != oAttMap.end(); ++oAttIter )
        {
            posGDALAtt = &(oAttIter->first);
            const std::string *posNCDFAtt = &(oAttIter->second);
            oValIter = oValMap.find(*posGDALAtt);

            if( oValIter != oValMap.end() )
            {
                dfValue = oValIter->second;
                bool bWriteVal = true;

                // special case for PS (Polar Stereographic) grid.
                // See comments in netcdfdataset.h for this projection.
                if( EQUAL(SRS_PP_LATITUDE_OF_ORIGIN, posGDALAtt->c_str()) &&
                    EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
                {
                    double dfLatPole = 0.0;
                    if( dfValue > 0.0 ) dfLatPole = 90.0;
                    else dfLatPole = -90.0;
                    oOutList.push_back(std::make_pair(
                        std::string(CF_PP_LAT_PROJ_ORIGIN), dfLatPole));
                }

                // special case for LCC-1SP
                //   See comments in netcdfdataset.h for this projection.
                else if( EQUAL(SRS_PP_SCALE_FACTOR, posGDALAtt->c_str()) &&
                         EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
                {
                    // Default is to not write as it is not CF-1.
                    bWriteVal = false;
                    // Test if there is no standard_parallel1.
                    if( oValMap.find(std::string(CF_PP_STD_PARALLEL_1)) == oValMap.end() )
                    {
                        // If scale factor != 1.0, write value for GDAL, but
                        // this is not supported by CF-1.
                        if( !CPLIsEqual(dfValue, 1.0) )
                        {
                            CPLError(
                                CE_Failure, CPLE_NotSupported,
                                "NetCDF driver export of LCC-1SP with scale "
                                "factor != 1.0 and no standard_parallel1 is "
                                "not CF-1 (bug #3324).  Use the 2SP variant "
                                "which is supported by CF.");
                            bWriteVal = true;
                        }
                        // Else copy standard_parallel1 from
                        // latitude_of_origin, because scale_factor=1.0.
                        else
                        {
                            oValIter2 = oValMap.find(
                                std::string(SRS_PP_LATITUDE_OF_ORIGIN));
                            if( oValIter2 != oValMap.end() )
                            {
                                oOutList.push_back(std::make_pair(
                                    std::string(CF_PP_STD_PARALLEL_1),
                                    oValIter2->second));
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_NotSupported,
                                         "NetCDF driver export of LCC-1SP with "
                                         "no standard_parallel1 "
                                         "and no latitude_of_origin is not "
                                         "supported (bug #3324).");
                            }
                        }
                    }
                }
                if( bWriteVal )
                    oOutList.push_back(std::make_pair(*posNCDFAtt, dfValue));
            }
#ifdef NCDF_DEBUG
            else
            {
                CPLDebug("GDAL_netCDF", "NOT FOUND!");
            }
#endif
        }
    }
    else
    {
        // Generic mapping, loop over projected values.
        for( oValIter = oValMap.begin(); oValIter != oValMap.end(); ++oValIter )
        {
            posGDALAtt = &(oValIter->first);
            dfValue = oValIter->second;

            oAttIter = oAttMap.find(*posGDALAtt);

            if( oAttIter != oAttMap.end() )
            {
                oOutList.push_back(std::make_pair(oAttIter->second, dfValue));
            }
            /* for SRS_PP_SCALE_FACTOR write 2 mappings */
            else if( EQUAL(posGDALAtt->c_str(), SRS_PP_SCALE_FACTOR) )
            {
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_SCALE_FACTOR_MERIDIAN), dfValue));
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_SCALE_FACTOR_ORIGIN), dfValue));
            }
            /* if not found insert the GDAL name */
            else
            {
                oOutList.push_back(std::make_pair(*posGDALAtt, dfValue));
            }
        }
    }

    /* Write all the values that were found */
    // std::vector< std::pair<std::string, double> >::reverse_iterator it;
    // for( it = oOutList.rbegin();  it != oOutList.rend(); it++ ) {
    std::vector< std::pair<std::string, double> >::iterator it;
    double dfStdP[2];
    bool bFoundStdP1 = false;
    bool bFoundStdP2 = false;
    for( it = oOutList.begin(); it != oOutList.end(); ++it )
    {
        pszParamVal = (it->first).c_str();
        dfValue = it->second;
        /* Handle the STD_PARALLEL attrib */
        if( EQUAL(pszParamVal, CF_PP_STD_PARALLEL_1) )
        {
            bFoundStdP1 = true;
            dfStdP[0] = dfValue;
        }
        else if( EQUAL(pszParamVal, CF_PP_STD_PARALLEL_2) )
        {
            bFoundStdP2 = true;
            dfStdP[1] = dfValue;
        }
        else
        {
            nc_put_att_double(fpImage, NCDFVarID, pszParamVal,
                              NC_DOUBLE, 1, &dfValue);
        }
    }
    /* Now write the STD_PARALLEL attrib */
    if( bFoundStdP1 )
    {
        /* one value or equal values */
        if( !bFoundStdP2 || dfStdP[0] == dfStdP[1] )
        {
            nc_put_att_double(fpImage, NCDFVarID, CF_PP_STD_PARALLEL,
                              NC_DOUBLE, 1, &dfStdP[0]);
        }
        else
        {
            // Two values.
            nc_put_att_double(fpImage, NCDFVarID, CF_PP_STD_PARALLEL,
                              NC_DOUBLE, 2, dfStdP);
        }
    }
}

static CPLErr NCDFSafeStrcat(char **ppszDest, const char *pszSrc,
                             size_t *nDestSize)
{
    /* Reallocate the data string until the content fits */
    while( *nDestSize < (strlen(*ppszDest) + strlen(pszSrc) + 1) )
    {
        (*nDestSize) *= 2;
        *ppszDest = static_cast<char *>(
            CPLRealloc(reinterpret_cast<void *>(*ppszDest), *nDestSize));
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "NCDFSafeStrcat() resized str from %ld to %ld",
                 (*nDestSize) / 2, *nDestSize);
#endif
    }
    strcat(*ppszDest, pszSrc);

    return CE_None;
}

/* helper function for NCDFGetAttr() */
/* sets pdfValue to first value returned */
/* and if bSetPszValue=True sets pszValue with all attribute values */
/* pszValue is the responsibility of the caller and must be freed */
static CPLErr NCDFGetAttr1( int nCdfId, int nVarId, const char *pszAttrName,
                            double *pdfValue, char **pszValue,
                            bool bSetPszValue )
{
    nc_type nAttrType = NC_NAT;
    size_t nAttrLen = 0;

    int status = nc_inq_att(nCdfId, nVarId, pszAttrName, &nAttrType, &nAttrLen);
    if( status != NC_NOERR )
        return CE_Failure;

#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "NCDFGetAttr1(%s) len=%ld type=%d", pszAttrName,
             nAttrLen, nAttrType);
#endif

    /* Allocate guaranteed minimum size (use 10 or 20 if not a string) */
    size_t nAttrValueSize = nAttrLen + 1;
    if( nAttrType != NC_CHAR && nAttrValueSize < 10 )
        nAttrValueSize = 10;
    if( nAttrType == NC_DOUBLE && nAttrValueSize < 20 )
        nAttrValueSize = 20;
#ifdef NETCDF_HAS_NC4
    if( nAttrType == NC_INT64 && nAttrValueSize < 20 )
        nAttrValueSize = 22;
#endif
    char *pszAttrValue =
        static_cast<char *>(CPLCalloc(nAttrValueSize, sizeof(char)));
    *pszAttrValue = '\0';

    if( nAttrLen > 1 && nAttrType != NC_CHAR )
        NCDFSafeStrcat(&pszAttrValue, "{", &nAttrValueSize);

    double dfValue = 0.0;
    size_t m;
    char szTemp[256];

    switch( nAttrType )
    {
    case NC_CHAR:
        nc_get_att_text(nCdfId, nVarId, pszAttrName, pszAttrValue);
        pszAttrValue[nAttrLen] = '\0';
        dfValue = 0.0;
        break;
    case NC_BYTE:
    {
        signed char *pscTemp = static_cast<signed char *>(
            CPLCalloc(nAttrLen, sizeof(signed char)));
        nc_get_att_schar(nCdfId, nVarId, pszAttrName, pscTemp);
        dfValue = static_cast<double>(pscTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            snprintf(szTemp, sizeof(szTemp), "%d,", pscTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), "%d", pscTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(pscTemp);
        break;
    }
    case NC_SHORT:
    {
        short *psTemp =
            static_cast<short *>(CPLCalloc(nAttrLen, sizeof(short)));
        nc_get_att_short(nCdfId, nVarId, pszAttrName, psTemp);
        dfValue = static_cast<double>(psTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            snprintf(szTemp, sizeof(szTemp), "%d,", psTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), "%d", psTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(psTemp);
        break;
    }
    case NC_INT:
    {
        int *pnTemp = static_cast<int *>(CPLCalloc(nAttrLen, sizeof(int)));
        nc_get_att_int(nCdfId, nVarId, pszAttrName, pnTemp);
        dfValue = static_cast<double>(pnTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            snprintf(szTemp, sizeof(szTemp), "%d,", pnTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), "%d", pnTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(pnTemp);
        break;
    }
    case NC_FLOAT:
    {
        float *pfTemp =
            static_cast<float *>(CPLCalloc(nAttrLen, sizeof(float)));
        nc_get_att_float(nCdfId, nVarId, pszAttrName, pfTemp);
        dfValue = static_cast<double>(pfTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%.8g,", pfTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%.8g", pfTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(pfTemp);
        break;
    }
    case NC_DOUBLE:
    {
        double *pdfTemp =
            static_cast<double *>(CPLCalloc(nAttrLen, sizeof(double)));
        nc_get_att_double(nCdfId, nVarId, pszAttrName, pdfTemp);
        dfValue = pdfTemp[0];
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%.16g,", pdfTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%.16g", pdfTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(pdfTemp);
        break;
    }
#ifdef NETCDF_HAS_NC4
    case NC_STRING:
    {
        char **ppszTemp =
            static_cast<char **>(CPLCalloc(nAttrLen, sizeof(char *)));
        nc_get_att_string(nCdfId, nVarId, pszAttrName, ppszTemp);
        dfValue = 0.0;
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            NCDFSafeStrcat(&pszAttrValue, ppszTemp[m], &nAttrValueSize);
            NCDFSafeStrcat(&pszAttrValue, ",", &nAttrValueSize);
        }
        NCDFSafeStrcat(&pszAttrValue, ppszTemp[m], &nAttrValueSize);
        nc_free_string(nAttrLen, ppszTemp);
        CPLFree(ppszTemp);
        break;
    }
    case NC_UBYTE:
    {
        unsigned char *pucTemp = static_cast<unsigned char *>(
            CPLCalloc(nAttrLen, sizeof(unsigned char)));
        nc_get_att_uchar(nCdfId, nVarId, pszAttrName, pucTemp);
        dfValue = static_cast<double>(pucTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%u,", pucTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%u", pucTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(pucTemp);
        break;
    }
    case NC_USHORT:
    {
        unsigned short *pusTemp;
        pusTemp = static_cast<unsigned short *>(
            CPLCalloc(nAttrLen, sizeof(unsigned short)));
        nc_get_att_ushort(nCdfId, nVarId, pszAttrName, pusTemp);
        dfValue = static_cast<double>(pusTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%u,", pusTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%u", pusTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(pusTemp);
        break;
    }
    case NC_UINT:
    {
        unsigned int *punTemp =
            static_cast<unsigned int *>(CPLCalloc(nAttrLen, sizeof(int)));
        nc_get_att_uint(nCdfId, nVarId, pszAttrName, punTemp);
        dfValue = static_cast<double>(punTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%u,", punTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%u", punTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(punTemp);
        break;
    }
    case NC_INT64:
    {
        GIntBig *panTemp =
            static_cast<GIntBig *>(CPLCalloc(nAttrLen, sizeof(GIntBig)));
        nc_get_att_longlong(nCdfId, nVarId, pszAttrName, panTemp);
        dfValue = static_cast<double>(panTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), CPL_FRMT_GIB ",", panTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), CPL_FRMT_GIB, panTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(panTemp);
        break;
    }
    case NC_UINT64:
    {
        GUIntBig *panTemp =
            static_cast<GUIntBig *>(CPLCalloc(nAttrLen, sizeof(GUIntBig)));
        nc_get_att_ulonglong(nCdfId, nVarId, pszAttrName, panTemp);
        dfValue = static_cast<double>(panTemp[0]);
        for( m = 0; m < nAttrLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), CPL_FRMT_GUIB ",", panTemp[m]);
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), CPL_FRMT_GUIB, panTemp[m]);
        NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
        CPLFree(panTemp);
        break;
    }
#endif
    default:
        CPLDebug("GDAL_netCDF",
                 "NCDFGetAttr unsupported type %d for attribute %s",
                  nAttrType, pszAttrName);
        break;
    }

    if( nAttrLen > 1 && nAttrType!= NC_CHAR )
        NCDFSafeStrcat(&pszAttrValue, "}", &nAttrValueSize);

    /* set return values */
    if( bSetPszValue ) *pszValue = pszAttrValue;
    else CPLFree(pszAttrValue);
    if( pdfValue ) *pdfValue = dfValue;

    return CE_None;
}

/* sets pdfValue to first value found */
CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName,
                    double *pdfValue )
{
    return NCDFGetAttr1(nCdfId, nVarId, pszAttrName, pdfValue, NULL, false);
}

/* pszValue is the responsibility of the caller and must be freed */
CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName,
                    char **pszValue )
{
    return NCDFGetAttr1(nCdfId, nVarId, pszAttrName, NULL, pszValue, true);
}

/* By default write NC_CHAR, but detect for int/float/double and */
/* NC4 string arrays */
static CPLErr NCDFPutAttr( int nCdfId, int nVarId, const char *pszAttrName,
                           const char *pszValue )
{
    int status = 0;
    char *pszTemp = NULL;

    /* get the attribute values as tokens */
    char **papszValues = NCDFTokenizeArray(pszValue);
    if( papszValues == NULL )
        return CE_Failure;

    size_t nAttrLen = CSLCount(papszValues);

    /* first detect type */
    nc_type nAttrType = NC_CHAR;
    nc_type nTmpAttrType = NC_CHAR;
    for( size_t i = 0; i < nAttrLen; i++ )
    {
        nTmpAttrType = NC_CHAR;
        bool bFoundType = false;
        errno = 0;
        int nValue = static_cast<int>(strtol(papszValues[i], &pszTemp, 10));
        /* test for int */
        /* TODO test for Byte and short - can this be done safely? */
        if( errno == 0 && papszValues[i] != pszTemp && *pszTemp == 0 )
        {
            char szTemp[256];
            CPLsnprintf(szTemp, sizeof(szTemp), "%d", nValue);
            if( EQUAL(szTemp, papszValues[i]) )
            {
                bFoundType = true;
                nTmpAttrType = NC_INT;
            }
#ifdef NETCDF_HAS_NC4
            else
            {
                unsigned int unValue = static_cast<unsigned int>(
                    strtoul(papszValues[i], &pszTemp, 10));
                CPLsnprintf(szTemp, sizeof(szTemp), "%u", unValue);
                if( EQUAL(szTemp, papszValues[i]) )
                {
                    bFoundType = true;
                    nTmpAttrType = NC_UINT;
                }
            }
#endif
        }
        if( !bFoundType )
        {
            /* test for double */
            errno = 0;
            double dfValue = CPLStrtod(papszValues[i], &pszTemp);
            if( (errno == 0) && (papszValues[i] != pszTemp) && (*pszTemp == 0) )
            {
                // Test for float instead of double.
                // strtof() is C89, which is not available in MSVC.
                // See if we loose precision if we cast to float and write to char*.
                float fValue = float(dfValue);
                char szTemp[256];
                CPLsnprintf(szTemp, sizeof(szTemp), "%.8g", fValue);
                if( EQUAL(szTemp, papszValues[i]) )
                    nTmpAttrType = NC_FLOAT;
                else
                    nTmpAttrType = NC_DOUBLE;
            }
        }
        if( (nTmpAttrType <= NC_DOUBLE && nAttrType <= NC_DOUBLE
             && nTmpAttrType > nAttrType)
#ifdef NETCDF_HAS_NC4
             || (nTmpAttrType == NC_UINT && nAttrType < NC_FLOAT)
             || (nTmpAttrType >= NC_FLOAT && nAttrType == NC_UINT)
#endif
            )
            nAttrType = nTmpAttrType;
    }

    /* now write the data */
    if( nAttrType == NC_CHAR )
    {
#ifdef NETCDF_HAS_NC4
        int nTmpFormat = 0;
        if( nAttrLen > 1 )
        {
            status = nc_inq_format(nCdfId, &nTmpFormat);
            NCDF_ERR(status);
        }
        if( nAttrLen > 1 && nTmpFormat == NCDF_FORMAT_NC4 )
            status = nc_put_att_string(nCdfId, nVarId, pszAttrName, nAttrLen,
                                       const_cast<const char **>(papszValues));
        else
#endif
            status = nc_put_att_text(nCdfId, nVarId, pszAttrName,
                                     strlen(pszValue), pszValue);
        NCDF_ERR(status);
    }
    else
    {
        switch( nAttrType )
        {
        case NC_INT:
        {
            int *pnTemp =
                static_cast<int *>(CPLCalloc(nAttrLen, sizeof(int)));
            for( size_t i = 0; i < nAttrLen; i++ )
            {
                pnTemp[i] =
                    static_cast<int>(strtol(papszValues[i], &pszTemp, 10));
            }
            status = nc_put_att_int(nCdfId, nVarId, pszAttrName, NC_INT,
                                    nAttrLen, pnTemp);
            NCDF_ERR(status);
            CPLFree(pnTemp);
            break;
        }
#ifdef NETCDF_HAS_NC4
        case NC_UINT:
        {
            unsigned int *punTemp = static_cast<unsigned int *>(
                CPLCalloc(nAttrLen, sizeof(unsigned int)));
            for( size_t i = 0; i < nAttrLen; i++ )
            {
                punTemp[i] = static_cast<unsigned int>(
                    strtol(papszValues[i], &pszTemp, 10));
            }
            status = nc_put_att_uint(nCdfId, nVarId, pszAttrName, NC_UINT,
                                     nAttrLen, punTemp);
            NCDF_ERR(status);
            CPLFree(punTemp);
            break;
        }
#endif
        case NC_FLOAT:
        {
            float *pfTemp =
                static_cast<float *>(CPLCalloc(nAttrLen, sizeof(float)));
            for( size_t i = 0; i < nAttrLen; i++ )
            {
                pfTemp[i] =
                    static_cast<float>(CPLStrtod(papszValues[i], &pszTemp));
            }
            status = nc_put_att_float(nCdfId, nVarId, pszAttrName, NC_FLOAT,
                                      nAttrLen, pfTemp);
            NCDF_ERR(status);
            CPLFree(pfTemp);
            break;
        }
        case NC_DOUBLE:
        {
            double *pdfTemp =
                static_cast<double *>(CPLCalloc(nAttrLen, sizeof(double)));
            for( size_t i = 0; i < nAttrLen; i++ )
            {
                pdfTemp[i] = CPLStrtod(papszValues[i], &pszTemp);
            }
            status = nc_put_att_double(nCdfId, nVarId, pszAttrName, NC_DOUBLE,
                                       nAttrLen, pdfTemp);
            NCDF_ERR(status);
            CPLFree(pdfTemp);
            break;
        }
        default:
            if( papszValues ) CSLDestroy(papszValues);
            return CE_Failure;
            break;
        }
    }

    if( papszValues ) CSLDestroy(papszValues);

    return CE_None;
}

static CPLErr NCDFGet1DVar( int nCdfId, int nVarId, char **pszValue )
{
    /* get var information */
    int nVarDimId = -1;
    int status = nc_inq_varndims(nCdfId, nVarId, &nVarDimId);
    if( status != NC_NOERR || nVarDimId != 1 )
        return CE_Failure;

    status = nc_inq_vardimid(nCdfId, nVarId, &nVarDimId);
    if( status != NC_NOERR )
        return CE_Failure;

    nc_type nVarType = NC_NAT;
    status = nc_inq_vartype(nCdfId, nVarId, &nVarType);
    if( status != NC_NOERR )
        return CE_Failure;

    size_t nVarLen = 0;
    status = nc_inq_dimlen(nCdfId, nVarDimId, &nVarLen);
    if( status != NC_NOERR )
        return CE_Failure;

    size_t start[1] = {0};
    size_t count[1] = {nVarLen};

    /* Allocate guaranteed minimum size */
    size_t nVarValueSize = NCDF_MAX_STR_LEN;
    char *pszVarValue =
        static_cast<char *>(CPLCalloc(nVarValueSize, sizeof(char)));
    *pszVarValue = '\0';

    if( nVarLen == 0 )
    {
        /* set return values */
        *pszValue = pszVarValue;

        return CE_None;
    }

    if( nVarLen > 1 && nVarType != NC_CHAR )
        NCDFSafeStrcat(&pszVarValue, "{", &nVarValueSize);

    switch (nVarType)
    {
    case NC_CHAR:
        nc_get_vara_text(nCdfId, nVarId, start, count, pszVarValue);
        pszVarValue[nVarLen] = '\0';
        break;
    case NC_BYTE:
    {
        signed char *pscTemp = static_cast<signed char *>(
            CPLCalloc(nVarLen, sizeof(signed char)));
        nc_get_vara_schar(nCdfId, nVarId, start, count, pscTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            snprintf(szTemp, sizeof(szTemp), "%d,", pscTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), "%d", pscTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pscTemp);
        break;
    }
    case NC_SHORT:
    {
        short *psTemp = static_cast<short *>(CPLCalloc(nVarLen, sizeof(short)));
        nc_get_vara_short(nCdfId, nVarId, start, count, psTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            snprintf(szTemp, sizeof(szTemp), "%d,", psTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), "%d", psTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(psTemp);
        break;
    }
    case NC_INT:
    {
        int *pnTemp = static_cast<int *>(CPLCalloc(nVarLen, sizeof(int)));
        nc_get_vara_int(nCdfId, nVarId, start, count, pnTemp);
        char szTemp[256];
        size_t m = 0;
        for(; m < nVarLen - 1; m++)
        {
            snprintf(szTemp, sizeof(szTemp), "%d,", pnTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), "%d", pnTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pnTemp);
        break;
    }
    case NC_FLOAT:
    {
        float *pfTemp = static_cast<float *>(CPLCalloc(nVarLen, sizeof(float)));
        nc_get_vara_float(nCdfId, nVarId, start, count, pfTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%.8g,", pfTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%.8g", pfTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pfTemp);
        break;
    }
    case NC_DOUBLE:
    {
        double *pdfTemp =
            static_cast<double *>(CPLCalloc(nVarLen, sizeof(double)));
        nc_get_vara_double(nCdfId, nVarId, start, count, pdfTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%.16g,", pdfTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%.16g", pdfTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pdfTemp);
        break;
    }
#ifdef NETCDF_HAS_NC4
    case NC_STRING:
    {
        char **ppszTemp =
            static_cast<char **>(CPLCalloc(nVarLen, sizeof(char *)));
        nc_get_vara_string(nCdfId, nVarId, start, count, ppszTemp);
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            NCDFSafeStrcat(&pszVarValue, ppszTemp[m], &nVarValueSize);
            NCDFSafeStrcat(&pszVarValue, ",", &nVarValueSize);
        }
        NCDFSafeStrcat(&pszVarValue, ppszTemp[m], &nVarValueSize);
        nc_free_string(nVarLen, ppszTemp);
        CPLFree(ppszTemp);
        break;
    }
    case NC_UBYTE:
    {
        unsigned char *pucTemp;
        pucTemp = static_cast<unsigned char *>(
            CPLCalloc(nVarLen, sizeof(unsigned char)));
        nc_get_vara_uchar(nCdfId, nVarId, start, count, pucTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%u,", pucTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%u", pucTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pucTemp);
        break;
    }
    case NC_USHORT:
    {
        unsigned short *pusTemp;
        pusTemp = static_cast<unsigned short *>(
            CPLCalloc(nVarLen, sizeof(unsigned short)));
        nc_get_vara_ushort(nCdfId, nVarId, start, count, pusTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%u,", pusTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%u", pusTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pusTemp);
        break;
    }
    case NC_UINT:
    {
        unsigned int *punTemp;
        punTemp = static_cast<unsigned int *>(
            CPLCalloc(nVarLen, sizeof(unsigned int)));
        nc_get_vara_uint(nCdfId, nVarId, start, count, punTemp);
        char szTemp[256];
        size_t m = 0;
        for( ; m < nVarLen - 1; m++ )
        {
            CPLsnprintf(szTemp, sizeof(szTemp), "%u,", punTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        CPLsnprintf(szTemp, sizeof(szTemp), "%u", punTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(punTemp);
        break;
    }
    case NC_INT64:
    {
        long long *pnTemp = static_cast<long long *>(
            CPLCalloc(nVarLen, sizeof(long long)));
        nc_get_vara_longlong(nCdfId, nVarId, start, count, pnTemp);
        char szTemp[256];
        size_t m = 0;
        for(; m < nVarLen - 1; m++)
        {
            snprintf(szTemp, sizeof(szTemp), CPL_FRMT_GIB ",", pnTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), CPL_FRMT_GIB, pnTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pnTemp);
        break;
    }
    case NC_UINT64:
    {
        unsigned long long *pnTemp = static_cast<unsigned long long *>(
            CPLCalloc(nVarLen, sizeof(unsigned long long)));
        nc_get_vara_ulonglong(nCdfId, nVarId, start, count, pnTemp);
        char szTemp[256];
        size_t m = 0;
        for(; m < nVarLen - 1; m++)
        {
            snprintf(szTemp, sizeof(szTemp), CPL_FRMT_GUIB ",", pnTemp[m]);
            NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        }
        snprintf(szTemp, sizeof(szTemp), CPL_FRMT_GUIB, pnTemp[m]);
        NCDFSafeStrcat(&pszVarValue, szTemp, &nVarValueSize);
        CPLFree(pnTemp);
        break;
    }
#endif
    default:
        CPLDebug("GDAL_netCDF", "NCDFGetVar1D unsupported type %d", nVarType);
        CPLFree(pszVarValue);
        pszVarValue = NULL;
        break;
    }

    if( pszVarValue != NULL && nVarLen > 1 && nVarType != NC_CHAR )
        NCDFSafeStrcat(&pszVarValue, "}", &nVarValueSize);

    /* set return values */
    *pszValue = pszVarValue;

    return CE_None;
}

static CPLErr NCDFPut1DVar( int nCdfId, int nVarId, const char *pszValue )
{
    if( EQUAL(pszValue, "") )
        return CE_Failure;

    /* get var information */
    int nVarDimId = -1;
    int status = nc_inq_varndims(nCdfId, nVarId, &nVarDimId);
    if( status != NC_NOERR || nVarDimId != 1)
        return CE_Failure;

    status = nc_inq_vardimid(nCdfId, nVarId, &nVarDimId);
    if( status != NC_NOERR )
        return CE_Failure;

    nc_type nVarType = NC_CHAR;
    status = nc_inq_vartype(nCdfId, nVarId, &nVarType);
    if( status != NC_NOERR )
        return CE_Failure;

    size_t nVarLen = 0;
    status = nc_inq_dimlen(nCdfId, nVarDimId, &nVarLen);
    if( status != NC_NOERR )
        return CE_Failure;

    size_t start[1] = {0};
    size_t count[1] = {nVarLen};

    /* get the values as tokens */
    char **papszValues = NCDFTokenizeArray(pszValue);
    if( papszValues == NULL )
        return CE_Failure;

    nVarLen = CSLCount(papszValues);

    /* now write the data */
    if( nVarType == NC_CHAR )
    {
        status = nc_put_vara_text(nCdfId, nVarId, start, count, pszValue);
        NCDF_ERR(status);
    }
    else
    {
        switch( nVarType )
        {
        case NC_BYTE:
        {
            signed char *pscTemp = static_cast<signed char *>(
                CPLCalloc(nVarLen, sizeof(signed char)));
            for( size_t i = 0; i < nVarLen; i++ )
            {
                char *pszTemp = NULL;
                pscTemp[i] = static_cast<signed char>(
                    strtol(papszValues[i], &pszTemp, 10));
            }
            status = nc_put_vara_schar(nCdfId, nVarId, start, count, pscTemp);
            NCDF_ERR(status);
            CPLFree(pscTemp);
            break;
        }
        case NC_SHORT:
        {
            short *psTemp =
                static_cast<short *>(CPLCalloc(nVarLen, sizeof(short)));
            for( size_t i = 0; i < nVarLen; i++ )
            {
                char *pszTemp = NULL;
                psTemp[i] =
                    static_cast<short>(strtol(papszValues[i], &pszTemp, 10));
            }
            status = nc_put_vara_short(nCdfId, nVarId, start, count, psTemp);
            NCDF_ERR(status);
            CPLFree(psTemp);
            break;
        }
        case NC_INT:
        {
            int *pnTemp = static_cast<int *>(CPLCalloc(nVarLen, sizeof(int)));
            for( size_t i = 0; i < nVarLen; i++ )
            {
                char *pszTemp = NULL;
                pnTemp[i] =
                    static_cast<int>(strtol(papszValues[i], &pszTemp, 10));
            }
            status = nc_put_vara_int(nCdfId, nVarId, start, count, pnTemp);
            NCDF_ERR(status);
            CPLFree(pnTemp);
            break;
        }
        case NC_FLOAT:
        {
            float *pfTemp =
                static_cast<float *>(CPLCalloc(nVarLen, sizeof(float)));
            for(size_t i = 0; i < nVarLen; i++)
            {
                char *pszTemp = NULL;
                pfTemp[i] =
                    static_cast<float>(CPLStrtod(papszValues[i], &pszTemp));
            }
            status = nc_put_vara_float(nCdfId, nVarId, start, count, pfTemp);
            NCDF_ERR(status);
            CPLFree(pfTemp);
            break;
        }
        case NC_DOUBLE:
        {
            double *pdfTemp =
                static_cast<double *>(CPLCalloc(nVarLen, sizeof(double)));
            for( size_t i = 0; i < nVarLen; i++ )
            {
                char *pszTemp = NULL;
                pdfTemp[i] = CPLStrtod(papszValues[i], &pszTemp);
            }
            status = nc_put_vara_double(nCdfId, nVarId, start, count, pdfTemp);
            NCDF_ERR(status);
            CPLFree(pdfTemp);
            break;
        }
        default:
#ifdef NETCDF_HAS_NC4
            int nTmpFormat = 0;
            status = nc_inq_format(nCdfId, &nTmpFormat);
            NCDF_ERR(status);
            if( nTmpFormat == NCDF_FORMAT_NC4 )
            {
                switch ( nVarType )
                {
                case NC_STRING:
                {
                    status = nc_put_vara_string(nCdfId, nVarId, start, count,
                                                (const char **)papszValues);
                    NCDF_ERR(status);
                    break;
                }
                case NC_UBYTE:
                {
                    unsigned char *pucTemp = static_cast<unsigned char *>(
                        CPLCalloc(nVarLen, sizeof(unsigned char)));
                    for( size_t i = 0; i < nVarLen; i++ )
                    {
                        char *pszTemp = NULL;
                        pucTemp[i] = static_cast<unsigned char>(
                            strtoul(papszValues[i], &pszTemp, 10));
                    }
                    status = nc_put_vara_uchar(nCdfId, nVarId, start, count,
                                               pucTemp);
                    NCDF_ERR(status);
                    CPLFree(pucTemp);
                    break;
                }
                case NC_USHORT:
                {
                    unsigned short *pusTemp =
                        static_cast<unsigned short *>(
                            CPLCalloc(nVarLen, sizeof(unsigned short)));
                    for( size_t i = 0; i < nVarLen; i++ )
                    {
                        char *pszTemp = NULL;
                        pusTemp[i] = static_cast<unsigned short>(
                            strtoul(papszValues[i], &pszTemp, 10));
                    }
                    status = nc_put_vara_ushort(nCdfId, nVarId, start, count,
                                                pusTemp);
                    NCDF_ERR(status);
                    CPLFree(pusTemp);
                    break;
                }
                case NC_UINT:
                {
                    unsigned int *punTemp = static_cast<unsigned int *>(
                        CPLCalloc(nVarLen, sizeof(unsigned int)));
                    for( size_t i = 0; i < nVarLen; i++ )
                    {
                        char *pszTemp = NULL;
                        punTemp[i] = static_cast<unsigned int>(
                            strtoul(papszValues[i], &pszTemp, 10));
                    }
                    status =
                        nc_put_vara_uint(nCdfId, nVarId, start, count, punTemp);
                    NCDF_ERR(status);
                    CPLFree(punTemp);
                    break;
                }
                default:
#endif
                    if( papszValues )
                        CSLDestroy(papszValues);
                    return CE_Failure;
                    break;
#ifdef NETCDF_HAS_NC4
                }
            }
#endif
        }
    }

    if( papszValues )
        CSLDestroy(papszValues);

    return CE_None;
}

/************************************************************************/
/*                           GetDefaultNoDataValue()                    */
/************************************************************************/

double NCDFGetDefaultNoDataValue( int nVarType )

{
    double dfNoData = 0.0;

    switch( nVarType )
    {
    case NC_BYTE:
#ifdef NETCDF_HAS_NC4
    case NC_UBYTE:
#endif
        // Don't do default fill-values for bytes, too risky.
        dfNoData = 0.0;
        break;
    case NC_CHAR:
        dfNoData = NC_FILL_CHAR;
        break;
    case NC_SHORT:
        dfNoData = NC_FILL_SHORT;
        break;
    case NC_INT:
        dfNoData = NC_FILL_INT;
        break;
    case NC_FLOAT:
        dfNoData = NC_FILL_FLOAT;
        break;
    case NC_DOUBLE:
        dfNoData = NC_FILL_DOUBLE;
        break;
#ifdef NETCDF_HAS_NC4
    case NC_USHORT:
        dfNoData = NC_FILL_USHORT;
        break;
    case NC_UINT:
        dfNoData = NC_FILL_UINT;
        break;
#endif
    default:
        dfNoData = 0.0;
        break;
    }

    return dfNoData;
}

static int NCDFDoesVarContainAttribVal( int nCdfId,
                                        const char *const *papszAttribNames,
                                        const char *const *papszAttribValues,
                                        int nVarId,
                                        const char *pszVarName,
                                        bool bStrict = true )
{
    if( nVarId == -1 && pszVarName != NULL )
        nc_inq_varid(nCdfId, pszVarName, &nVarId);

    if( nVarId == -1 ) return -1;

    bool bFound = false;
    for( int i = 0; !bFound && i < CSLCount(papszAttribNames); i++ )
    {
        char *pszTemp = NULL;
        if( NCDFGetAttr(nCdfId, nVarId, papszAttribNames[i], &pszTemp) ==
               CE_None &&
           pszTemp != NULL )
        {
            if( bStrict )
            {
                if( EQUAL(pszTemp, papszAttribValues[i]) )
                    bFound = true;
            }
            else
            {
                if( EQUALN(pszTemp, papszAttribValues[i],
                           strlen(papszAttribValues[i])) )
                    bFound = true;
            }
            CPLFree(pszTemp);
        }
    }
    return bFound;
}

static int NCDFDoesVarContainAttribVal2( int nCdfId,
                                         const char *papszAttribName,
                                         const char *const *papszAttribValues,
                                         int nVarId,
                                         const char *pszVarName,
                                         int bStrict = true )
{
    if( nVarId == -1 && pszVarName != NULL )
        nc_inq_varid(nCdfId, pszVarName, &nVarId);

    if( nVarId == -1 ) return -1;

    bool bFound = false;
    char *pszTemp = NULL;
    if( NCDFGetAttr(nCdfId, nVarId, papszAttribName, &pszTemp) != CE_None ||
        pszTemp == NULL )
        return FALSE;

    for( int i = 0; !bFound && i < CSLCount(papszAttribValues); i++ )
    {
        if( bStrict )
        {
            if( EQUAL(pszTemp, papszAttribValues[i]) )
                bFound = true;
        }
        else
        {
            if( EQUALN(pszTemp, papszAttribValues[i],
                       strlen(papszAttribValues[i])) )
                bFound = true;
        }
    }

    CPLFree(pszTemp);

    return bFound;
}

static bool NCDFEqual( const char *papszName, const char *const *papszValues )
{
    if( papszName == NULL || EQUAL(papszName, "") )
        return false;

    for( int i = 0; i < CSLCount(papszValues); ++i )
    {
        if( EQUAL(papszName, papszValues[i]) )
            return true;
    }

    return false;
}

// Test that a variable is longitude/latitude coordinate,
// following CF 4.1 and 4.2.
static bool NCDFIsVarLongitude( int nCdfId, int nVarId,
                                const char *pszVarName )
{
    // Check for matching attributes.
    int bVal = NCDFDoesVarContainAttribVal(nCdfId,
                                           papszCFLongitudeAttribNames,
                                           papszCFLongitudeAttribValues,
                                           nVarId, pszVarName);
    // If not found using attributes then check using var name
    // unless GDAL_NETCDF_VERIFY_DIMS=STRICT.
    if( bVal == -1 )
    {
        if( !EQUAL(CPLGetConfigOption("GDAL_NETCDF_VERIFY_DIMS", "YES"),
                   "STRICT") )
            bVal = NCDFEqual(pszVarName, papszCFLongitudeVarNames);
        else
            bVal = FALSE;
    }
    else if ( bVal )
    {
        // Check that the units is not 'm'. See #6759
        char *pszTemp = NULL;
        if( NCDFGetAttr(nCdfId, nVarId, "units", &pszTemp) == CE_None &&
            pszTemp != NULL )
        {
            if( EQUAL(pszTemp, "m") )
                bVal = false;
            CPLFree(pszTemp);
        }
    }

    return CPL_TO_BOOL(bVal);
}

static bool NCDFIsVarLatitude( int nCdfId, int nVarId, const char *pszVarName )
{
    int bVal = NCDFDoesVarContainAttribVal(nCdfId,
                                           papszCFLatitudeAttribNames,
                                           papszCFLatitudeAttribValues,
                                           nVarId, pszVarName);
    if( bVal == -1 )
    {
        if( !EQUAL(CPLGetConfigOption("GDAL_NETCDF_VERIFY_DIMS", "YES"),
                   "STRICT") )
            bVal = NCDFEqual(pszVarName, papszCFLatitudeVarNames);
        else
            bVal = FALSE;
    }
    else if ( bVal )
    {
        // Check that the units is not 'm'. See #6759
        char *pszTemp = NULL;
        if( NCDFGetAttr(nCdfId, nVarId, "units", &pszTemp) == CE_None &&
            pszTemp != NULL )
        {
            if( EQUAL(pszTemp, "m") )
                bVal = false;
            CPLFree(pszTemp);
        }
    }

    return CPL_TO_BOOL(bVal);
}

static bool NCDFIsVarProjectionX( int nCdfId, int nVarId,
                                  const char * pszVarName )
{
    int bVal = NCDFDoesVarContainAttribVal(nCdfId,
                                           papszCFProjectionXAttribNames,
                                           papszCFProjectionXAttribValues,
                                           nVarId, pszVarName);
    if( bVal == -1 )
    {
        if( !EQUAL(CPLGetConfigOption("GDAL_NETCDF_VERIFY_DIMS", "YES"),
                   "STRICT") )
            bVal = NCDFEqual(pszVarName, papszCFProjectionXVarNames);
        else
            bVal = FALSE;
    }
    return CPL_TO_BOOL(bVal);
}

static bool NCDFIsVarProjectionY( int nCdfId, int nVarId,
                                  const char *pszVarName )
{
    int bVal = NCDFDoesVarContainAttribVal(nCdfId,
                                           papszCFProjectionYAttribNames,
                                           papszCFProjectionYAttribValues,
                                           nVarId, pszVarName);
    if( bVal == -1 )
    {
        if( !EQUAL(CPLGetConfigOption("GDAL_NETCDF_VERIFY_DIMS", "YES"),
                   "STRICT") )
            bVal = NCDFEqual(pszVarName, papszCFProjectionYVarNames);
        else
            bVal = FALSE;
    }
    return CPL_TO_BOOL(bVal);
}

/* test that a variable is a vertical coordinate, following CF 4.3 */
static bool NCDFIsVarVerticalCoord( int nCdfId, int nVarId,
                                    const char *pszVarName )
{
    /* check for matching attributes */
    if( NCDFDoesVarContainAttribVal(nCdfId,
                                    papszCFVerticalAttribNames,
                                    papszCFVerticalAttribValues,
                                    nVarId, pszVarName) )
        return true;
    /* check for matching units */
    else if( NCDFDoesVarContainAttribVal2(nCdfId,
                                          CF_UNITS,
                                          papszCFVerticalUnitsValues,
                                          nVarId, pszVarName) )
        return true;
    /* check for matching standard name */
    else if( NCDFDoesVarContainAttribVal2(nCdfId,
                                          CF_STD_NAME,
                                          papszCFVerticalStandardNameValues,
                                          nVarId, pszVarName) )
        return true;
    else
        return false;
}

/* test that a variable is a time coordinate, following CF 4.4 */
static bool NCDFIsVarTimeCoord( int nCdfId, int nVarId,
                                const char *pszVarName )
{
    /* check for matching attributes */
    if( NCDFDoesVarContainAttribVal(nCdfId,
                                    papszCFTimeAttribNames,
                                    papszCFTimeAttribValues,
                                    nVarId, pszVarName) )
        return true;
    /* check for matching units */
    else if( NCDFDoesVarContainAttribVal2(nCdfId,
                                          CF_UNITS,
                                          papszCFTimeUnitsValues,
                                          nVarId, pszVarName, false) )
        return true;
    else
        return false;
}

// Parse a string, and return as a string list.
// If it an array of the form {a,b}, then tokenize it.
// Otherwise, return a copy.
static char **NCDFTokenizeArray( const char *pszValue )
{
    if( pszValue == NULL || EQUAL(pszValue, "") )
        return NULL;

    char **papszValues = NULL;
    const int nLen = static_cast<int>(strlen(pszValue));

    if( pszValue[0] == '{' && nLen > 2 && pszValue[nLen - 1] == '}' )
    {
        char *pszTemp = static_cast<char *>(CPLMalloc((nLen - 2) + 1));
        strncpy(pszTemp, pszValue + 1, nLen - 2);
        pszTemp[nLen - 2] = '\0';
        papszValues = CSLTokenizeString2(pszTemp, ",", CSLT_ALLOWEMPTYTOKENS);
        CPLFree(pszTemp);
    }
    else
    {
        papszValues = reinterpret_cast<char **>(CPLCalloc(2, sizeof(char *)));
        papszValues[0] = CPLStrdup(pszValue);
        papszValues[1] = NULL;
    }

    return papszValues;
}
