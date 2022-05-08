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
 * Copyright (c) 2021, CLS
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

#include <cassert>
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
#include <set>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Must be included after standard includes, otherwise VS2015 fails when
// including <ctime>
#include "netcdfdataset.h"
#include "netcdfsg.h"
#include "netcdfuffd.h"

#ifdef HAVE_NETCDF_MEM
#include "netcdf_mem.h"
#endif

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_json.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_progress.h"
#include "cpl_time.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

#define ROTATED_POLE_VAR_NAME "rotated_pole"

// Detect netCDF 4.8
#ifdef NC_ENCZARR
#define NETCDF_USES_UTF8
#endif

CPL_CVSID("$Id$")

// Internal function declarations.

static bool NCDFIsGDALVersionGTE(const char *pszVersion, int nTarget);

static void NCDFAddGDALHistory(
    int fpImage,
    const char *pszFilename,
    bool bWriteGDALVersion,
    bool bWriteGDALHistory,
    const char *pszOldHist,
    const char *pszFunctionName,
    const char *pszCFVersion = GDAL_DEFAULT_NCDF_CONVENTIONS );

static void NCDFAddHistory( int fpImage, const char *pszAddHist,
                            const char *pszOldHist );

static bool NCDFIsCfProjection( const char *pszProjection );

static std::vector<std::pair<std::string, double> > NCDFGetProjAttribs(
                                  const OGR_SRSNode *poPROJCS,
                                  const char *pszProjection);

static CPLErr NCDFSafeStrcat( char **ppszDest, const char *pszSrc,
                              size_t *nDestSize );

// Var / attribute helper functions.
static CPLErr NCDFPutAttr( int nCdfId, int nVarId,
                           const char *pszAttrName, const char *pszValue );

// Replace this where used.
static CPLErr NCDFGet1DVar( int nCdfId, int nVarId, char **pszValue );
static CPLErr NCDFPut1DVar( int nCdfId, int nVarId, const char *pszValue );

static double NCDFGetDefaultNoDataValue( int nCdfId, int nVarId, int nVarType, bool& bGotNoData );
#ifdef NETCDF_HAS_NC4
static int64_t NCDFGetDefaultNoDataValueAsInt64( int nCdfId, int nVarId, bool& bGotNoData );
static uint64_t NCDFGetDefaultNoDataValueAsUInt64( int nCdfId, int nVarId, bool& bGotNoData );
#endif

// Replace this where used.
static char **NCDFTokenizeArray( const char *pszValue );
static void CopyMetadata( GDALDataset* poSrcDS,
                          GDALRasterBand* poSrcBand,
                          GDALRasterBand* poDstBand,
                          int fpImage, int CDFVarID,
                          const char *pszMatchPrefix=nullptr );

// NetCDF-4 groups helper functions.
// They all work also for NetCDF-3 files which are considered as
// NetCDF-4 file with only one group.
static CPLErr NCDFOpenSubDataset( int nCdfId, const char *pszSubdatasetName,
                                  int *pnGroupId, int *pnVarId );
static CPLErr NCDFGetVisibleDims( int nGroupId, int *pnDims,
                                  int **ppanDimIds );
static CPLErr NCDFGetSubGroups( int nGroupId, int *pnSubGroups,
                                int **ppanSubGroupIds );
static CPLErr NCDFGetGroupFullName( int nGroupId, char **ppszFullName,
                                    bool bNC3Compat=true );
static CPLErr NCDFGetVarFullName( int nGroupId, int nVarId,
                                  char **ppszFullName, bool bNC3Compat=true );
static CPLErr NCDFGetRootGroup( int nStartGroupId, int *pnRootGroupId );

static CPLErr NCDFResolveVarFullName( int nStartGroupId, const char *pszVar,
                                      char **ppszFullName,
                                      bool bMandatory=false );
static CPLErr NCDFResolveAttInt( int nStartGroupId, int nStartVarId,
                                 const char *pszAtt, int *pnAtt,
                                 bool bMandatory=false );
static CPLErr NCDFGetCoordAndBoundVarFullNames( int nCdfId,
                                                char ***ppapszVars );

// Uncomment this for more debug output.
// #define NCDF_DEBUG 1

CPLMutex *hNCMutex = nullptr;

/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand final: public GDALPamRasterBand
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
    bool        m_bNoDataSet = false;
    double      m_dfNoDataValue = 0;
    bool        m_bNoDataSetAsInt64 = false;
    int64_t     m_nNodataValueInt64 = 0;
    bool        m_bNoDataSetAsUInt64 = false;
    uint64_t    m_nNodataValueUInt64 = 0;
    bool        bValidRangeValid = false;
    double      adfValidRange[2]{0,0};
    bool        m_bHaveScale = false;
    bool        m_bHaveOffset = false;
    double      m_dfScale = 1;
    double      m_dfOffset = 0;
    CPLString   m_osUnitType{};
    bool        bSignedData;
    bool        bCheckLongitude;

    CPLErr          CreateBandMetadata( const int *paDimIds,
                                        const int* panExtraDimGroupIds,
                                        const int* panExtraDimVarIds );
    template <class T> void CheckData ( void *pImage, void *pImageNC,
                                        size_t nTmpBlockXSize,
                                        size_t nTmpBlockYSize,
                                        bool bCheckIsNan=false ) ;
    template <class T> void CheckDataCpx ( void *pImage, void *pImageNC,
                                        size_t nTmpBlockXSize,
                                        size_t nTmpBlockYSize,
                                        bool bCheckIsNan=false );
    void            SetBlockSize();

    bool            FetchNetcdfChunk( size_t xstart,
                                      size_t ystart,
                                      void* pImage );

    void            SetNoDataValueNoUpdate(double dfNoData);
    void            SetNoDataValueNoUpdate(int64_t nNoData);
    void            SetNoDataValueNoUpdate(uint64_t nNoData);

    void            SetOffsetNoUpdate(double dfVal);
    void            SetScaleNoUpdate(double dfVal);
    void            SetUnitTypeNoUpdate(const char* pszNewValue);

  protected:
    CPLXMLNode *SerializeToXML( const char *pszUnused ) override;

  public:

    struct CONSTRUCTOR_OPEN {};
    struct CONSTRUCTOR_CREATE {};

    netCDFRasterBand( const CONSTRUCTOR_OPEN&,
                      netCDFDataset *poDS,
                      int nGroupId,
                      int nZId,
                      int nZDim,
                      int nLevel,
                      const int *panBandZLen,
                      const int *panBandPos,
                      const int *paDimIds,
                      int nBand,
                      const int *panExtraDimGroupIds,
                      const int *panExtraDimVarIds );
    netCDFRasterBand( const CONSTRUCTOR_CREATE&,
                      netCDFDataset *poDS,
                      GDALDataType eType,
                      int nBand,
                      bool bSigned=true,
                      const char *pszBandName=nullptr,
                      const char *pszLongName=nullptr,
                      int nZId=-1,
                      int nZDim=2,
                      int nLevel=0,
                      const int *panBandZLev=nullptr,
                      const int *panBandZPos=nullptr,
                      const int *paDimIds=nullptr );
    virtual ~netCDFRasterBand();

    virtual double GetNoDataValue( int * ) override;
    virtual int64_t GetNoDataValueAsInt64( int *pbSuccess = nullptr ) override;
    virtual uint64_t GetNoDataValueAsUInt64( int *pbSuccess = nullptr ) override;
    virtual CPLErr SetNoDataValue( double ) override;
    virtual CPLErr SetNoDataValueAsInt64( int64_t nNoData ) override;
    virtual CPLErr SetNoDataValueAsUInt64( uint64_t nNoData ) override;
    // virtual CPLErr DeleteNoDataValue();
    virtual double GetOffset( int * ) override;
    virtual CPLErr SetOffset( double ) override;
    virtual double GetScale( int * ) override;
    virtual CPLErr SetScale( double ) override;
    virtual const char *GetUnitType() override;
    virtual CPLErr SetUnitType( const char * ) override;
    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    virtual CPLErr SetMetadataItem( const char* pszName, const char* pszValue, const char* pszDomain = "" ) override;
    virtual CPLErr SetMetadata( char** papszMD, const char* pszDomain = "" ) override;
};

/************************************************************************/
/*                          netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::netCDFRasterBand( const netCDFRasterBand::CONSTRUCTOR_OPEN&,
                                    netCDFDataset *poNCDFDS,
                                    int nGroupId,
                                    int nZIdIn,
                                    int nZDimIn,
                                    int nLevelIn,
                                    const int *panBandZLevIn,
                                    const int *panBandZPosIn,
                                    const int *paDimIds,
                                    int nBandIn,
                                    const int *panExtraDimGroupIds,
                                    const int *panExtraDimVarIds ) :
    nc_datatype(NC_NAT),
    cdfid(nGroupId),
    nZId(nZIdIn),
    nZDim(nZDimIn),
    nLevel(nLevelIn),
    nBandXPos(panBandZPosIn[0]),
    nBandYPos(nZDim == 1 ? -1 : panBandZPosIn[1]),
    panBandZPos(nullptr),
    panBandZLev(nullptr),
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
    if( nc_inq_var(cdfid, nZId, nullptr, &nc_datatype, nullptr, nullptr,
                   nullptr) != NC_NOERR )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error in nc_var_inq() on 'z'.");
        return;
    }

#ifdef NETCDF_HAS_NC4
    if (NCDFIsUserDefinedType(cdfid, nc_datatype))
    {
        //First enquire and check that the number of fields is 2
        size_t nfields, compoundsize;
        if( nc_inq_compound(cdfid, nc_datatype, nullptr, &compoundsize, &nfields) != NC_NOERR)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error in nc_inq_compound() on 'z'.");
            return;
        }

        if (nfields != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unsupported data type encountered in nc_inq_compound() on 'z'.");
            return;
        }

        //Now check that that two types are the same in the struct.
        nc_type field_type1, field_type2;
        int field_dims1, field_dims2;
        if ( nc_inq_compound_field(cdfid, nc_datatype, 0, nullptr, nullptr, &field_type1, &field_dims1, nullptr) != NC_NOERR)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error in querying Field 1 in nc_inq_compound_field() on 'z'.");
            return;
        }

        if ( nc_inq_compound_field(cdfid, nc_datatype, 0, nullptr, nullptr, &field_type2, &field_dims2, nullptr) != NC_NOERR)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error in querying Field 2 in nc_inq_compound_field() on 'z'.");
            return;
        }

        if ((field_type1 != field_type2) || (field_dims1 != field_dims2) || (field_dims1 != 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error in interpreting compound data type on 'z'.");
            return;
        }

        if (field_type1 == NC_SHORT)
            eDataType = GDT_CInt16;
        else if (field_type1 == NC_INT)
            eDataType = GDT_CInt32;
        else if (field_type1 == NC_FLOAT)
            eDataType = GDT_CFloat32;
        else if (field_type1 == NC_DOUBLE)
            eDataType = GDT_CFloat64;
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Unsupported netCDF compound data type encountered.");
            return;
        }
    }
    else
#endif
    {
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
        else if( nc_datatype == NC_INT64 )
            eDataType = GDT_Int64;
        else if( nc_datatype == NC_UINT64 )
            eDataType = GDT_UInt64;
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
    }

    // Find and set No Data for this variable.
    nc_type atttype = NC_NAT;
    size_t attlen = 0;
    const char *pszNoValueName = nullptr;

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
#ifdef NETCDF_HAS_NC4
    int64_t nNoDataAsInt64 = 0;
    bool bGotNoDataAsInt64 = false;
    uint64_t nNoDataAsUInt64 = 0;
    bool bGotNoDataAsUInt64 = false;
#endif
    if( status == NC_NOERR )
    {
#ifdef NETCDF_HAS_NC4
        nc_type nAttrType = NC_NAT;
        size_t nAttrLen = 0;
        status = nc_inq_att(cdfid, nZId, pszNoValueName, &nAttrType, &nAttrLen);
        if( status == NC_NOERR && nAttrLen == 1 && nAttrType == NC_INT64 )
        {
            long long v;
            nc_get_att_longlong(cdfid, nZId, pszNoValueName, &v);
            bGotNoData = true;
            bGotNoDataAsInt64 = true;
            nNoDataAsInt64 = static_cast<int64_t>(v);
        }
        else if( status == NC_NOERR && nAttrLen == 1 && nAttrType == NC_UINT64 )
        {
            unsigned long long v;
            nc_get_att_ulonglong(cdfid, nZId, pszNoValueName, &v);
            bGotNoData = true;
            bGotNoDataAsUInt64 = true;
            nNoDataAsUInt64 = static_cast<uint64_t>(v);
        }
        else
#endif
        if( NCDFGetAttr(cdfid, nZId, pszNoValueName, &dfNoData) == CE_None )
        {
            bGotNoData = true;
        }
    }

    // If NoData was not found, use the default value, but for non-Byte types
    // as it is not recommended:
    // https://www.unidata.ucar.edu/software/netcdf/docs/attribute_conventions.html
    nc_type vartype = NC_NAT;
    if( !bGotNoData )
    {
        nc_inq_vartype(cdfid, nZId, &vartype);
#ifdef NETCDF_HAS_NC4
        if( vartype == NC_INT64 )
        {
            nNoDataAsInt64 = NCDFGetDefaultNoDataValueAsInt64(cdfid, nZId, bGotNoData);
            bGotNoDataAsInt64 = bGotNoData;
        }
        else if( vartype == NC_UINT64 )
        {
            nNoDataAsUInt64 = NCDFGetDefaultNoDataValueAsUInt64(cdfid, nZId, bGotNoData);
            bGotNoDataAsUInt64 = bGotNoData;
        }
        else
#endif
        if( vartype != NC_CHAR &&
            vartype != NC_BYTE
#ifdef NETCDF_HAS_NC4
            && vartype != NC_UBYTE
#endif
        )
        {
            dfNoData = NCDFGetDefaultNoDataValue(cdfid, nZId, vartype, bGotNoData);
            if( bGotNoData )
            {
                CPLDebug("GDAL_netCDF",
                        "did not get nodata value for variable #%d, using default %f",
                        nZId, dfNoData);
            }
        }
    }

    // Look for valid_range or valid_min/valid_max.

    // First look for valid_range.
    if( CPLFetchBool(poNCDFDS->GetOpenOptions(), "HONOUR_VALID_RANGE", true) )
    {
        char *pszValidRange = nullptr;
        if( NCDFGetAttr(cdfid, nZId, "valid_range", &pszValidRange) == CE_None &&
            pszValidRange[0] == '{' && pszValidRange[strlen(pszValidRange)-1] == '}' )
        {
            const std::string osValidRange = std::string(
                pszValidRange).substr(1, strlen(pszValidRange)-2);
            const CPLStringList aosValidRange(
                CSLTokenizeString2(osValidRange.c_str(), ",", 0));
            if( aosValidRange.size() == 2 &&
                CPLGetValueType(aosValidRange[0]) != CPL_VALUE_STRING &&
                CPLGetValueType(aosValidRange[1]) != CPL_VALUE_STRING )
            {
                bValidRangeValid = true;
                adfValidRange[0] = CPLAtof(aosValidRange[0]);
                adfValidRange[1] = CPLAtof(aosValidRange[1]);
            }
        }
        CPLFree(pszValidRange);

        // If not found look for valid_min and valid_max.
        if( !bValidRangeValid )
        {
            double dfMin = 0;
            double dfMax = 0;
            if( NCDFGetAttr(cdfid, nZId, "valid_min", &dfMin) == CE_None &&
                NCDFGetAttr(cdfid, nZId, "valid_max", &dfMax) == CE_None )
            {
                adfValidRange[0] = dfMin;
                adfValidRange[1] = dfMax;
                bValidRangeValid = true;
            }
        }

        if (bValidRangeValid && adfValidRange[0] > adfValidRange[1])
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "netCDFDataset::valid_range: min > max:\n"
                "  min: %lf\n  max: %lf\n", adfValidRange[0], adfValidRange[1]);
            bValidRangeValid = false;
            adfValidRange[0] = 0.0;
            adfValidRange[1] = 0.0;
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

        // For NC4 format NC_BYTE is (normally) signed, NC_UBYTE is unsigned.
        // But in case a NC3 file was converted automatically and has hints
        // that it is unsigned, take them into account
        if( poNCDFDS->eFormat == NCDF_FORMAT_NC4 )
        {
            bSignedData = true;
        }

        // Fix nodata value as it was stored signed.
        if( !bSignedData && dfNoData < 0 )
        {
            dfNoData += 256;
        }

        // If we got valid_range, test for signed/unsigned range.
        // http://www.unidata.ucar.edu/software/netcdf/docs/netcdf/Attribute-Conventions.html
        if( bValidRangeValid )
        {
            // If we got valid_range={0,255}, treat as unsigned.
            if( adfValidRange[0] == 0 && adfValidRange[1] == 255 )
            {
                bSignedData = false;
                // Fix nodata value as it was stored signed.
                if( dfNoData < 0 )
                {
                    dfNoData += 256;
                }

                // Reset valid_range.
                bValidRangeValid = false;
            }
            // If we got valid_range={-128,127}, treat as signed.
            else if( adfValidRange[0] == -128 && adfValidRange[1] == 127 )
            {
                bSignedData = true;
                // Reset valid_range.
                bValidRangeValid = false;
            }
        }
        // Else test for _Unsigned.
        // http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html
        else
        {
            char *pszTemp = nullptr;
            if( NCDFGetAttr(cdfid, nZId, "_Unsigned", &pszTemp) == CE_None )
            {
                if( EQUAL(pszTemp, "true") )
                    bSignedData = false;
                else if( EQUAL(pszTemp, "false") )
                    bSignedData = true;
                CPLFree(pszTemp);
            }

            // Fix nodata value as it was stored signed.
            if( !bSignedData && dfNoData < 0 )
            {
                dfNoData += 256;
            }
        }

        if( bSignedData )
        {
            // set PIXELTYPE=SIGNEDBYTE
            // See http://trac.osgeo.org/gdal/wiki/rfc14_imagestructure
            GDALPamRasterBand::SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
        }
    }

#ifdef NETCDF_HAS_NC4
    if( nc_datatype == NC_UBYTE ||
        nc_datatype == NC_USHORT ||
        nc_datatype == NC_UINT ||
        nc_datatype == NC_UINT64 )
        bSignedData = false;
#endif

    CPLDebug("GDAL_netCDF", "netcdf type=%d gdal type=%d signedByte=%d",
             nc_datatype, eDataType, static_cast<int>(bSignedData));

    if( bGotNoData )
    {
        // Set nodata value.
#ifdef NETCDF_HAS_NC4
        if( bGotNoDataAsInt64 )
        {
            if( eDataType == GDT_Int64 )
            {
                SetNoDataValueNoUpdate(nNoDataAsInt64);
            }
            else if (eDataType == GDT_UInt64 &&
                     nNoDataAsInt64 >= 0 )
            {
                SetNoDataValueNoUpdate(static_cast<uint64_t>(nNoDataAsInt64));
            }
            else
            {
                SetNoDataValueNoUpdate(static_cast<double>(nNoDataAsInt64));
            }
        }
        else if( bGotNoDataAsUInt64 )
        {
            if( eDataType == GDT_UInt64 )
            {
                SetNoDataValueNoUpdate(nNoDataAsUInt64);
            }
            else if (eDataType == GDT_Int64 &&
                     nNoDataAsUInt64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) )
            {
                SetNoDataValueNoUpdate(static_cast<int64_t>(nNoDataAsUInt64));
            }
            else
            {
                SetNoDataValueNoUpdate(static_cast<double>(nNoDataAsUInt64));
            }
        }
        else
#endif
        {
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF", "SetNoDataValue(%f) read", dfNoData);
#endif
            if( eDataType == GDT_Int64 &&
                dfNoData >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                dfNoData <= static_cast<double>(std::numeric_limits<int64_t>::max()) &&
                dfNoData == static_cast<double>(static_cast<int64_t>(dfNoData)) )
            {
                SetNoDataValueNoUpdate(static_cast<int64_t>(dfNoData));
            }
            else if( eDataType == GDT_UInt64 &&
                     dfNoData >= static_cast<double>(std::numeric_limits<uint64_t>::min()) &&
                     dfNoData <= static_cast<double>(std::numeric_limits<uint64_t>::max()) &&
                     dfNoData == static_cast<double>(static_cast<uint64_t>(dfNoData)) )
            {
                SetNoDataValueNoUpdate(static_cast<uint64_t>(dfNoData));
            }
            else
            {
                SetNoDataValueNoUpdate(dfNoData);
            }
        }
    }

    // Create Band Metadata.
    CreateBandMetadata(paDimIds, panExtraDimGroupIds, panExtraDimVarIds);

    // Attempt to fetch the scale_factor and add_offset attributes for the
    // variable and set them.  If these values are not available, set
    // offset to 0 and scale to 1.
    if( nc_inq_attid (cdfid, nZId, CF_ADD_OFFSET, nullptr) == NC_NOERR )
    {
        double dfOffset = 0;
        status = nc_get_att_double(cdfid, nZId, CF_ADD_OFFSET, &dfOffset);
        CPLDebug("GDAL_netCDF", "got add_offset=%.16g, status=%d",
                 dfOffset, status);
        SetOffsetNoUpdate(dfOffset);
    }

    if( nc_inq_attid(cdfid, nZId, CF_SCALE_FACTOR, nullptr) == NC_NOERR )
    {
        double dfScale = 1;
        status = nc_get_att_double(cdfid, nZId, CF_SCALE_FACTOR, &dfScale);
        CPLDebug("GDAL_netCDF", "got scale_factor=%.16g, status=%d",
                 dfScale, status);
        SetScaleNoUpdate(dfScale);
    }

    // Should we check for longitude values > 360?
    bCheckLongitude =
        CPLTestBool(CPLGetConfigOption("GDAL_NETCDF_CENTERLONG_180", "YES")) &&
        NCDFIsVarLongitude(cdfid, nZId, nullptr);

    // Attempt to fetch the units attribute for the variable and set it.
    SetUnitTypeNoUpdate(GetMetadataItem(CF_UNITS));

    SetBlockSize();
}

void netCDFRasterBand::SetBlockSize()
{
    // Check for variable chunking (netcdf-4 only).
    // GDAL block size should be set to hdf5 chunk size.
#ifdef NETCDF_HAS_NC4
    int nTmpFormat = 0;
    int status = nc_inq_format(cdfid, &nTmpFormat);
    NetCDFFormatEnum eTmpFormat = static_cast<NetCDFFormatEnum>(nTmpFormat);
    if( (status == NC_NOERR) &&
        (eTmpFormat == NCDF_FORMAT_NC4 || eTmpFormat == NCDF_FORMAT_NC4C) )
    {
        size_t chunksize[MAX_NC_DIMS] = {};
        // Check for chunksize and set it as the blocksize (optimizes read).
        status = nc_inq_var_chunking(cdfid, nZId, &nTmpFormat, chunksize);
        if( (status == NC_NOERR) && (nTmpFormat == NC_CHUNKED) )
        {
            nBlockXSize = (int)chunksize[nZDim - 1];
            if( nZDim >= 2 )
                nBlockYSize = (int)chunksize[nZDim - 2];
            else
                nBlockYSize = 1;
        }
    }
#endif

    // Deal with bottom-up datasets and nBlockYSize != 1.
    auto poGDS = static_cast<netCDFDataset*>(poDS);
    if( poGDS->bBottomUp && nBlockYSize != 1 && poGDS->poChunkCache == nullptr )
    {
        if( poGDS->eAccess == GA_ReadOnly )
        {
            // Try to cache 1 or 2 'rows' of netCDF chunks along the whole
            // width of the raster
            size_t nChunks = static_cast<size_t>(DIV_ROUND_UP(nRasterXSize, nBlockXSize));
            if( (nRasterYSize % nBlockYSize) != 0 )
                nChunks *= 2;
            const size_t nChunkSize = static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType))
                * nBlockXSize * nBlockYSize;
            constexpr size_t MAX_CACHE_SIZE = 100 * 1024 * 1024;
            nChunks = std::min(nChunks, MAX_CACHE_SIZE / nChunkSize);
            if( nChunks )
            {
                poGDS->poChunkCache.reset(new netCDFDataset::ChunkCacheType(nChunks));
            }
        }
        else
        {
            nBlockYSize = 1;
        }
    }
}

// Constructor in create mode.
// If nZId and following variables are not passed, the band will have 2
// dimensions.
// TODO: Get metadata, missing val from band #1 if nZDim > 2.
netCDFRasterBand::netCDFRasterBand( const netCDFRasterBand::CONSTRUCTOR_CREATE&,
                                    netCDFDataset *poNCDFDS,
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
    panBandZPos(nullptr),
    panBandZLev(nullptr),
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
    if( nZDim > 2 && paDimIds != nullptr )
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
        case GDT_Int64:
            if( poNCDFDS->eFormat == NCDF_FORMAT_NC4 )
            {
                nc_datatype = NC_INT64;
            }
            else
            {
                if( nBand == 1 )
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Unsupported GDAL datatype %s, treat as NC_DOUBLE.", "Int64");
                nc_datatype = NC_DOUBLE;
                eDataType = GDT_Float64;
            }
            break;
        case GDT_UInt64:
            if( poNCDFDS->eFormat == NCDF_FORMAT_NC4 )
            {
                nc_datatype = NC_UINT64;
            }
            else
            {
                if( nBand == 1 )
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Unsupported GDAL datatype %s, treat as NC_DOUBLE.", "UInt64");
                nc_datatype = NC_DOUBLE;
                eDataType = GDT_Float64;
            }
            break;
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
        const char *pszTemp = nullptr;
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
        if( nZDim > 2 && paDimIds != nullptr )
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
            if( nc_datatype == NC_BYTE &&
                poNCDFDS->eFormat != NCDF_FORMAT_NC4 )
            {
                CPLDebug("GDAL_netCDF",
                         "adding valid_range attributes for Byte Band");
                short l_adfValidRange[2] = { 0, 0 };
                int status;
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
            GDALPamRasterBand::SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
    }

    if( nc_datatype != NC_BYTE &&
        nc_datatype != NC_CHAR
#ifdef NETCDF_HAS_NC4
        && nc_datatype != NC_UBYTE
#endif
        )
    {
        // Set default nodata.
        bool bIgnored = false;
        double dfNoData = NCDFGetDefaultNoDataValue(cdfid, nZId, nc_datatype, bIgnored);
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "SetNoDataValue(%f) default", dfNoData);
#endif
        netCDFRasterBand::SetNoDataValue(dfNoData);
    }

    SetBlockSize();
}

/************************************************************************/
/*                         ~netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::~netCDFRasterBand()
{
    netCDFRasterBand::FlushCache(true);
    CPLFree(panBandZPos);
    CPLFree(panBandZLev);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr netCDFRasterBand::SetMetadataItem( const char* pszName,
                                          const char* pszValue,
                                          const char* pszDomain )
{
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                  "netCDFRasterBand::SetMetadataItem() can only be "
                  "called in update mode");
        return CE_Failure;
    }

    if( (pszDomain == nullptr || pszDomain[0] == '\0') && pszValue != nullptr )
    {
        // Same logic as in CopyMetadata()

        const char * const papszIgnoreBand[] = { CF_ADD_OFFSET, CF_SCALE_FACTOR,
                                          "valid_range", "_Unsigned",
                                          _FillValue, "coordinates",
                                          nullptr };
        // Do not copy varname, stats, NETCDF_DIM_*, nodata
        // and items in papszIgnoreBand.
        if( STARTS_WITH(pszName, "NETCDF_VARNAME") ||
            STARTS_WITH(pszName, "STATISTICS_") ||
            STARTS_WITH(pszName, "NETCDF_DIM_") ||
            STARTS_WITH(pszName, "missing_value") ||
            STARTS_WITH(pszName, "_FillValue") ||
            CSLFindString(papszIgnoreBand, pszName) != -1 )
        {
            // do nothing
        }
        else
        {
            cpl::down_cast<netCDFDataset*>(poDS)->SetDefineMode(true);

            if( !NCDFPutAttr(cdfid, nZId, pszName, pszValue) )
                return CE_Failure;
        }
    }

    return GDALPamRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                          SetMetadata()                               */
/************************************************************************/

CPLErr netCDFRasterBand::SetMetadata( char** papszMD, const char* pszDomain )
{
    if( pszDomain == nullptr || pszDomain[0] == '\0' )
    {
        // We don't handle metadata item removal for now
        for( const char* const*  papszIter = papszMD; papszIter && *papszIter; ++papszIter )
        {
            char* pszName = nullptr;
            const char* pszValue = CPLParseNameValue(*papszIter, &pszName);
            if( pszName && pszValue )
                SetMetadataItem(pszName, pszValue);
            CPLFree(pszName);
        }
    }
    return GDALPamRasterBand::SetMetadata(papszMD, pszDomain);
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/
double netCDFRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = static_cast<int>(m_bHaveOffset);

    return m_dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/
CPLErr netCDFRasterBand::SetOffset( double dfNewOffset )
{
    CPLMutexHolderD(&hNCMutex);

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // Make sure we are in define mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        const int status = nc_put_att_double(cdfid, nZId, CF_ADD_OFFSET,
                                             NC_DOUBLE, 1, &dfNewOffset);

        NCDF_ERR(status);
        if( status == NC_NOERR )
        {
            SetOffsetNoUpdate(dfNewOffset);
            return CE_None;
        }

        return CE_Failure;
    }

    SetOffsetNoUpdate(dfNewOffset);
    return CE_None;
}

/************************************************************************/
/*                         SetOffsetNoUpdate()                          */
/************************************************************************/
void netCDFRasterBand::SetOffsetNoUpdate( double dfVal )
{
    m_dfOffset = dfVal;
    m_bHaveOffset = true;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/
double netCDFRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = static_cast<int>(m_bHaveScale);

    return m_dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/
CPLErr netCDFRasterBand::SetScale( double dfNewScale )
{
    CPLMutexHolderD(&hNCMutex);

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // Make sure we are in define mode.
        static_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        const int status = nc_put_att_double(cdfid, nZId, CF_SCALE_FACTOR,
                                             NC_DOUBLE, 1, &dfNewScale);

        NCDF_ERR(status);
        if( status == NC_NOERR )
        {
            SetScaleNoUpdate(dfNewScale);
            return CE_None;
        }

        return CE_Failure;
    }

    SetScaleNoUpdate(dfNewScale);
    return CE_None;
}

/************************************************************************/
/*                         SetScaleNoUpdate()                           */
/************************************************************************/
void netCDFRasterBand::SetScaleNoUpdate( double dfVal )
{
    m_dfScale = dfVal;
    m_bHaveScale = true;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *netCDFRasterBand::GetUnitType()

{
    if( !m_osUnitType.empty() )
        return m_osUnitType;

    return GDALRasterBand::GetUnitType();
}

/************************************************************************/
/*                           SetUnitType()                              */
/************************************************************************/

CPLErr netCDFRasterBand::SetUnitType( const char *pszNewValue )

{
    CPLMutexHolderD(&hNCMutex);

    const std::string osUnitType = (pszNewValue != nullptr ? pszNewValue : "");

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
            {
                SetUnitTypeNoUpdate(pszNewValue);
                return CE_None;
            }

            return CE_Failure;
        }
    }

    SetUnitTypeNoUpdate(pszNewValue);

    return CE_None;
}

/************************************************************************/
/*                       SetUnitTypeNoUpdate()                          */
/************************************************************************/

void netCDFRasterBand::SetUnitTypeNoUpdate( const char *pszNewValue )
{
    m_osUnitType = (pszNewValue != nullptr ? pszNewValue : "");
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double netCDFRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( m_bNoDataSetAsInt64 )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return GDALGetNoDataValueCastToDouble(m_nNodataValueInt64);
    }

    if( m_bNoDataSetAsUInt64 )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return GDALGetNoDataValueCastToDouble(m_nNodataValueUInt64);
    }

    if( m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                        GetNoDataValueAsInt64()                       */
/************************************************************************/

int64_t netCDFRasterBand::GetNoDataValueAsInt64( int *pbSuccess )

{
    if( m_bNoDataSetAsInt64 )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_nNodataValueInt64;
    }

    return GDALPamRasterBand::GetNoDataValueAsInt64(pbSuccess);
}

/************************************************************************/
/*                        GetNoDataValueAsUInt64()                      */
/************************************************************************/

uint64_t netCDFRasterBand::GetNoDataValueAsUInt64( int *pbSuccess )

{
    if( m_bNoDataSetAsUInt64 )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_nNodataValueUInt64;
    }

    return GDALPamRasterBand::GetNoDataValueAsUInt64(pbSuccess);
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr netCDFRasterBand::SetNoDataValue( double dfNoData )

{
    CPLMutexHolderD(&hNCMutex);

    // If already set to new value, don't do anything.
    if( m_bNoDataSet && CPLIsEqual(dfNoData, m_dfNoDataValue) )
        return CE_None;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // netcdf-4 does not allow to set _FillValue after leaving define mode,
        // but it is ok if variable has not been written to, so only print debug.
        // See bug #4484.
        if( m_bNoDataSet &&
            !reinterpret_cast<netCDFDataset *>(poDS)->GetDefineMode() )
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to %.18g (previously set to %.18g) "
                     "but file is no longer in define mode (id #%d, band #%d)",
                     dfNoData, m_dfNoDataValue, cdfid, nBand);
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
            SetNoDataValueNoUpdate(dfNoData);
            return CE_None;
        }

        return CE_Failure;
    }

    SetNoDataValueNoUpdate(dfNoData);
    return CE_None;
}

/************************************************************************/
/*                       SetNoDataValueNoUpdate()                       */
/************************************************************************/

void netCDFRasterBand::SetNoDataValueNoUpdate(double dfNoData)
{
    m_dfNoDataValue = dfNoData;
    m_bNoDataSet = true;
    m_bNoDataSetAsInt64 = false;
    m_bNoDataSetAsUInt64 = false;
}

/************************************************************************/
/*                        SetNoDataValueAsInt64()                       */
/************************************************************************/

CPLErr netCDFRasterBand::SetNoDataValueAsInt64( int64_t nNoData )

{
    CPLMutexHolderD(&hNCMutex);

    // If already set to new value, don't do anything.
    if( m_bNoDataSetAsInt64 && nNoData == m_nNodataValueInt64 )
        return CE_None;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // netcdf-4 does not allow to set _FillValue after leaving define mode,
        // but it is ok if variable has not been written to, so only print debug.
        // See bug #4484.
        if( m_bNoDataSetAsInt64 &&
            !reinterpret_cast<netCDFDataset *>(poDS)->GetDefineMode() )
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to " CPL_FRMT_GIB " (previously set to " CPL_FRMT_GIB ") "
                     "but file is no longer in define mode (id #%d, band #%d)",
                     static_cast<GIntBig>(nNoData), static_cast<GIntBig>(m_nNodataValueInt64), cdfid, nBand);
        }
#ifdef NCDF_DEBUG
        else
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to " CPL_FRMT_GIB " (id #%d, band #%d)",
                     static_cast<GIntBig>(nNoData), cdfid, nBand);
        }
#endif
        // Make sure we are in define mode.
        reinterpret_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        int status;
#ifdef NETCDF_HAS_NC4
        if( eDataType == GDT_Int64 &&
                 reinterpret_cast<netCDFDataset *>(poDS)->eFormat ==
                 NCDF_FORMAT_NC4 )
        {
            long long tmp = static_cast<long long>(nNoData);
            status = nc_put_att_longlong (cdfid, nZId, _FillValue,
                                          nc_datatype, 1, &tmp);
        }
        else
#endif
        {
            double dfNoData = static_cast<double>(nNoData);
            status = nc_put_att_double(cdfid, nZId, _FillValue,
                                       nc_datatype, 1, &dfNoData);
        }

        NCDF_ERR(status);

        // Update status if write worked.
        if( status == NC_NOERR )
        {
            SetNoDataValueNoUpdate(nNoData);
            return CE_None;
        }

        return CE_Failure;
    }

    SetNoDataValueNoUpdate(nNoData);
    return CE_None;
}

/************************************************************************/
/*                       SetNoDataValueNoUpdate()                       */
/************************************************************************/

void netCDFRasterBand::SetNoDataValueNoUpdate(int64_t nNoData)
{
    m_nNodataValueInt64 = nNoData;
    m_bNoDataSet = false;
    m_bNoDataSetAsInt64 = true;
    m_bNoDataSetAsUInt64 = false;
}

/************************************************************************/
/*                        SetNoDataValueAsUInt64()                      */
/************************************************************************/

CPLErr netCDFRasterBand::SetNoDataValueAsUInt64( uint64_t nNoData )

{
    CPLMutexHolderD(&hNCMutex);

    // If already set to new value, don't do anything.
    if( m_bNoDataSetAsUInt64 && nNoData == m_nNodataValueUInt64 )
        return CE_None;

    // Write value if in update mode.
    if( poDS->GetAccess() == GA_Update )
    {
        // netcdf-4 does not allow to set _FillValue after leaving define mode,
        // but it is ok if variable has not been written to, so only print debug.
        // See bug #4484.
        if( m_bNoDataSetAsUInt64 &&
            !reinterpret_cast<netCDFDataset *>(poDS)->GetDefineMode() )
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to " CPL_FRMT_GUIB " (previously set to " CPL_FRMT_GUIB ") "
                     "but file is no longer in define mode (id #%d, band #%d)",
                     static_cast<GUIntBig>(nNoData), static_cast<GUIntBig>(m_nNodataValueUInt64), cdfid, nBand);
        }
#ifdef NCDF_DEBUG
        else
        {
            CPLDebug("GDAL_netCDF",
                     "Setting NoDataValue to " CPL_FRMT_GUIB " (id #%d, band #%d)",
                     static_cast<GUIntBig>(nNoData), cdfid, nBand);
        }
#endif
        // Make sure we are in define mode.
        reinterpret_cast<netCDFDataset *>(poDS)->SetDefineMode(true);

        int status;
#ifdef NETCDF_HAS_NC4
        if( eDataType == GDT_UInt64 &&
                 reinterpret_cast<netCDFDataset *>(poDS)->eFormat ==
                 NCDF_FORMAT_NC4 )
        {
            unsigned long long tmp = static_cast<long long>(nNoData);
            status = nc_put_att_ulonglong (cdfid, nZId, _FillValue,
                                           nc_datatype, 1, &tmp);
        }
        else
#endif
        {
            double dfNoData = static_cast<double>(nNoData);
            status = nc_put_att_double(cdfid, nZId, _FillValue,
                                       nc_datatype, 1, &dfNoData);
        }

        NCDF_ERR(status);

        // Update status if write worked.
        if( status == NC_NOERR )
        {
            SetNoDataValueNoUpdate(nNoData);
            return CE_None;
        }

        return CE_Failure;
    }

    SetNoDataValueNoUpdate(nNoData);
    return CE_None;
}

/************************************************************************/
/*                       SetNoDataValueNoUpdate()                       */
/************************************************************************/

void netCDFRasterBand::SetNoDataValueNoUpdate(uint64_t nNoData)
{
    m_nNodataValueUInt64 = nNoData;
    m_bNoDataSet = false;
    m_bNoDataSetAsInt64 = false;
    m_bNoDataSetAsUInt64 = true;
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
    if( psPam == nullptr )
        return nullptr;

    // Setup root node and attributes.
    CPLXMLNode *psTree = CPLCreateXMLNode(nullptr, CXT_Element, "PAMRasterBand");

    if( GetBand() > 0 )
    {
        CPLString oFmt;
        CPLSetXMLValue(psTree, "#band", oFmt.Printf("%d", GetBand()));
    }

    // Histograms.
    if( psPam->psSavedHistograms != nullptr )
        CPLAddXMLChild(psTree, CPLCloneXMLTree(psPam->psSavedHistograms));

    // Metadata (statistics only).
    GDALMultiDomainMetadata oMDMDStats;
    const char *papszMDStats[] = { "STATISTICS_MINIMUM", "STATISTICS_MAXIMUM",
                                   "STATISTICS_MEAN", "STATISTICS_STDDEV",
                                   nullptr };
    for( int i = 0; i < CSLCount(papszMDStats); i++ )
    {
        if( GetMetadataItem(papszMDStats[i]) != nullptr )
            oMDMDStats.SetMetadataItem(papszMDStats[i],
                                       GetMetadataItem(papszMDStats[i]));
    }
    CPLXMLNode *psMD = oMDMDStats.Serialize();

    if( psMD != nullptr )
    {
        if( psMD->psChild == nullptr )
            CPLDestroyXMLNode(psMD);
        else
            CPLAddXMLChild(psTree, psMD);
    }

    // We don't want to return anything if we had no metadata to attach.
    if( psTree->psChild == nullptr || psTree->psChild->psNext == nullptr )
    {
        CPLDestroyXMLNode(psTree);
        psTree = nullptr;
    }

    return psTree;
}

/************************************************************************/
/*               Get1DVariableIndexedByDimension()                      */
/************************************************************************/

static int Get1DVariableIndexedByDimension( int cdfid, int nDimId,
                                            const char* pszDimName,
                                            bool bVerboseError,
                                            int *pnGroupID )
{
    *pnGroupID = -1;
    int nVarID = -1;
    // First try to find a variable whose name is identical to the dimension
    // name, and check that it is indeed indexed by this dimension
    if( NCDFResolveVar(cdfid, pszDimName, pnGroupID, &nVarID) == CE_None )
    {
        int nDimCountOfVariable = 0;
        nc_inq_varndims(*pnGroupID, nVarID, &nDimCountOfVariable);
        if( nDimCountOfVariable == 1 )
        {
            int nDimIdOfVariable = -1;
            nc_inq_vardimid(*pnGroupID, nVarID, &nDimIdOfVariable);
            if( nDimIdOfVariable == nDimId )
            {
                return nVarID;
            }
        }
    }

    // Otherwise iterate over the variables to find potential candidates
    // TODO: should be modified to search also in other groups using the same
    //       logic than in NCDFResolveVar(), but maybe not needed if it's a
    //       very rare case? and I think this is not CF compliant.
    int nvars = 0;
    CPL_IGNORE_RET_VAL( nc_inq(cdfid, nullptr, &nvars, nullptr, nullptr) );

    int nCountCandidateVars = 0;
    int nCandidateVarID = -1;
    for( int k = 0; k < nvars; k++ )
    {
        int nDimCountOfVariable = 0;
        nc_inq_varndims(cdfid, k, &nDimCountOfVariable);
        if( nDimCountOfVariable == 1 )
        {
            int nDimIdOfVariable = -1;
            nc_inq_vardimid(cdfid, k, &nDimIdOfVariable);
            if( nDimIdOfVariable == nDimId )
            {
                nCountCandidateVars ++;
                nCandidateVarID = k;
            }
        }
    }
    if( nCountCandidateVars > 1 )
    {
        if( bVerboseError )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                 "Several 1D variables are indexed by dimension %s",
                 pszDimName);
        }
        *pnGroupID = -1;
        return -1;
    }
    else if( nCandidateVarID < 0 )
    {
        if( bVerboseError )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                 "No 1D variable is indexed by dimension %s",
                 pszDimName);
        }
    }
    *pnGroupID = cdfid;
    return nCandidateVarID;
}

/************************************************************************/
/*                         CreateBandMetadata()                         */
/************************************************************************/

CPLErr netCDFRasterBand::CreateBandMetadata( const int *paDimIds,
                                             const int* panExtraDimGroupIds,
                                             const int* panExtraDimVarIds )

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

    GDALPamRasterBand::SetMetadataItem("NETCDF_VARNAME", szVarName);
    int Sum = 1;
    if( nd == 3 )
    {
        Sum *= panBandZLev[0];
    }

    // Loop over non-spatial dimensions.
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

        char szMetaName[NC_MAX_NAME + 1 + 32];
        snprintf(szMetaName, sizeof(szMetaName), "NETCDF_DIM_%s", szVarName);

        int nGroupID = panExtraDimGroupIds[i];
        int nVarID = panExtraDimVarIds[i];
        if( nVarID < 0 )
        {
            GDALPamRasterBand::SetMetadataItem(szMetaName, CPLSPrintf("%d", result + 1));
        }
        else
        {
            // TODO: Make sure all the status checks make sense.

            nc_type nVarType = NC_NAT;
            /* status = */ nc_inq_vartype(nGroupID, nVarID, &nVarType);

            int nDims = 0;
            /* status = */ nc_inq_varndims(nGroupID, nVarID, &nDims);

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
                        /* status = */ nc_get_vara_schar(nGroupID, nVarID,
                                                    start,
                                                    count, &cData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", cData);
                        break;
                    case NC_SHORT:
                        short sData;
                        /* status = */ nc_get_vara_short(nGroupID, nVarID,
                                                    start,
                                                    count, &sData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", sData);
                        break;
                    case NC_INT:
                    {
                        int nData;
                        /* status = */ nc_get_vara_int(nGroupID, nVarID,
                                                start,
                                                count, &nData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), "%d", nData);
                        break;
                    }
                    case NC_FLOAT:
                        float fData;
                        /* status = */ nc_get_vara_float(nGroupID, nVarID,
                                                    start,
                                                    count, &fData);
                        CPLsnprintf(szMetaTemp, sizeof(szMetaTemp),
                                    "%.8g", fData);
                        break;
                    case NC_DOUBLE:
                        double dfData;
                        /* status = */ nc_get_vara_double(nGroupID, nVarID,
                                                    start,
                                                    count, &dfData);
                        CPLsnprintf(szMetaTemp, sizeof(szMetaTemp),
                                    "%.16g", dfData);
                        break;
    #ifdef NETCDF_HAS_NC4
                    case NC_UBYTE:
                        unsigned char ucData;
                        /* status = */ nc_get_vara_uchar(nGroupID, nVarID,
                                                    start,
                                                    count, &ucData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), "%u", ucData);
                        break;
                    case NC_USHORT:
                        unsigned short usData;
                        /* status = */ nc_get_vara_ushort(nGroupID, nVarID,
                                                    start,
                                                    count, &usData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), "%u", usData);
                        break;
                    case NC_UINT:
                    {
                        unsigned int unData;
                        /* status = */ nc_get_vara_uint(nGroupID, nVarID,
                                                    start,
                                                    count, &unData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), "%u", unData);
                        break;
                    }
                    case NC_INT64:
                    {
                        long long nData;
                        /* status = */ nc_get_vara_longlong(nGroupID, nVarID,
                                                    start,
                                                    count, &nData);
                        snprintf(szMetaTemp, sizeof(szMetaTemp), CPL_FRMT_GIB, nData);
                        break;
                    }
                    case NC_UINT64:
                    {
                        unsigned long long unData;
                        /* status = */ nc_get_vara_ulonglong(nGroupID, nVarID,
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

            GDALPamRasterBand::SetMetadataItem(szMetaName, szMetaTemp);
        }

        // Avoid int32 overflow. Perhaps something more sensible to do here ?
        if( result > 0 && Sum > INT_MAX / result )
            break;
        if( Taken > INT_MAX - result * Sum )
            break;

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

        char *pszMetaValue = nullptr;
        if( NCDFGetAttr(cdfid, nZId, szMetaName, &pszMetaValue) == CE_None )
        {
            GDALPamRasterBand::SetMetadataItem(szMetaName, pszMetaValue);
        }
        else
        {
            CPLDebug("GDAL_netCDF", "invalid Band metadata %s", szMetaName);
        }

        if( pszMetaValue )
        {
            CPLFree(pszMetaValue);
            pszMetaValue = nullptr;
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
    CPLAssert(pImage != nullptr && pImageNC != nullptr);

    // If this block is not a full block (in the x axis), we need to re-arrange
    // the data this is because partial blocks are not arranged the same way in
    // netcdf and gdal.
    if( nTmpBlockXSize != static_cast<size_t>(nBlockXSize) )
    {
        T *ptrWrite = static_cast<T*>(pImage);
        T *ptrRead = static_cast<T*>(pImageNC);
        for( size_t j = 0;
             j < nTmpBlockYSize;
             j++, ptrWrite += nBlockXSize, ptrRead += nTmpBlockXSize)
        {
            memmove(ptrWrite, ptrRead, nTmpBlockXSize * sizeof(T));
        }
    }

    // Is valid data checking needed or requested?
    if( bValidRangeValid ||
        bCheckIsNan )
    {
        T *ptrImage = static_cast<T*>(pImage);
        for( size_t j = 0; j < nTmpBlockYSize; j++ )
        {
            // k moves along the gdal block, skipping the out-of-range pixels.
            size_t k = j * nBlockXSize;
            for( size_t i = 0; i < nTmpBlockXSize; i++, k++ )
            {
                // Check for nodata and nan.
                if( CPLIsEqual((double) ptrImage[k], m_dfNoDataValue) )
                    continue;
                if( bCheckIsNan && CPLIsNan((double) ptrImage[k]) )
                {
                    ptrImage[k] = (T)m_dfNoDataValue;
                    continue;
                }
                // Check for valid_range.
                if( bValidRangeValid )
                {
                    if( ((adfValidRange[0] != m_dfNoDataValue) &&
                        (ptrImage[k] < (T)adfValidRange[0]))
                        ||
                        ((adfValidRange[1] != m_dfNoDataValue) &&
                        (ptrImage[k] > (T)adfValidRange[1])) )
                    {
                        ptrImage[k] = (T)m_dfNoDataValue;
                    }
                }
            }
        }
    }

    // If minimum longitude is > 180, subtract 360 from all.
    // If not, disable checking for further calls (check just once).
    // Only check first and last block elements since lon must be monotonic.
    const bool bIsSigned = std::numeric_limits<T>::is_signed;
    if( bCheckLongitude && bIsSigned &&
        !CPLIsEqual((double)((T *)pImage)[0], m_dfNoDataValue) &&
        !CPLIsEqual((double)((T *)pImage)[nTmpBlockXSize - 1], m_dfNoDataValue) &&
        std::min(((T *)pImage)[0], ((T *)pImage)[nTmpBlockXSize - 1]) > 180.0 )
    {
        T *ptrImage = static_cast<T*>(pImage);
        for( size_t j = 0; j < nTmpBlockYSize; j++ )
        {
            size_t k = j * nBlockXSize;
            for( size_t i = 0; i < nTmpBlockXSize; i++, k++ )
            {
                if( !CPLIsEqual((double)ptrImage[k], m_dfNoDataValue) )
                    ptrImage[k] = static_cast<T>(ptrImage[k] - 360);
            }
        }
    }
    else
    {
        bCheckLongitude = false;
    }
}

/************************************************************************/
/*                             CheckDataCpx()                              */
/************************************************************************/
template <class T>
void netCDFRasterBand::CheckDataCpx( void *pImage, void *pImageNC,
                                  size_t nTmpBlockXSize, size_t nTmpBlockYSize,
                                  bool bCheckIsNan )
{
    CPLAssert(pImage != nullptr && pImageNC != nullptr);

    // If this block is not a full block (in the x axis), we need to re-arrange
    // the data this is because partial blocks are not arranged the same way in
    // netcdf and gdal.
    if( nTmpBlockXSize != static_cast<size_t>(nBlockXSize) )
    {
        T *ptrWrite = static_cast<T*>(pImage);
        T *ptrRead = static_cast<T*>(pImageNC);
        for( size_t j = 0;
             j < nTmpBlockYSize;
             j++, ptrWrite += (2*nBlockXSize), ptrRead += (2*nTmpBlockXSize))
        {
            memmove(ptrWrite, ptrRead, nTmpBlockXSize * sizeof(T) * 2);
        }
    }

    // Is valid data checking needed or requested?
    if( bValidRangeValid ||
        bCheckIsNan )
    {
        T *ptrImage = static_cast<T*>(pImage);
        for( size_t j = 0; j < nTmpBlockYSize; j++ )
        {
            // k moves along the gdal block, skipping the out-of-range pixels.
            size_t k = 2 * j * nBlockXSize;
            for( size_t i = 0; i < (2 * nTmpBlockXSize); i++, k++ )
            {
                // Check for nodata and nan.
                if( CPLIsEqual((double) ptrImage[k], m_dfNoDataValue) )
                    continue;
                if( bCheckIsNan && CPLIsNan((double) ptrImage[k]) )
                {
                    ptrImage[k] = (T)m_dfNoDataValue;
                    continue;
                }
                // Check for valid_range.
                if( bValidRangeValid )
                {
                    if( ((adfValidRange[0] != m_dfNoDataValue) &&
                        (ptrImage[k] < (T)adfValidRange[0])) ||
                        ((adfValidRange[1] != m_dfNoDataValue) &&
                        (ptrImage[k] > (T)adfValidRange[1])) )
                    {
                        ptrImage[k] = (T)m_dfNoDataValue;
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                         FetchNetcdfChunk()                           */
/************************************************************************/

bool netCDFRasterBand::FetchNetcdfChunk( size_t xstart,
                                         size_t ystart,
                                         void* pImage )
{
    size_t start[MAX_NC_DIMS] = {};
    size_t edge[MAX_NC_DIMS] = {};

    start[nBandXPos] = xstart;
    edge[nBandXPos] = nBlockXSize;
    if( (start[nBandXPos] + edge[nBandXPos]) > (size_t)nRasterXSize )
        edge[nBandXPos] = nRasterXSize - start[nBandXPos];
    if( nBandYPos >= 0 )
    {
        start[nBandYPos] = ystart;
        edge[nBandYPos] = nBlockYSize;
        if( (start[nBandYPos] + edge[nBandYPos]) > (size_t)nRasterYSize )
            edge[nBandYPos] = nRasterYSize - start[nBandYPos];
    }
    const size_t nYChunkSize = nBandYPos < 0 ? 1 : edge[nBandYPos];

#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "start={%ld,%ld} edge={%ld,%ld} bBottomUp=%d",
                start[nBandXPos], nBandYPos < 0 ? 0 : start[nBandYPos],
                edge[nBandXPos],  nYChunkSize,
                ((netCDFDataset *)poDS)->bBottomUp);
#endif

    int nd = 0;
    nc_inq_varndims(cdfid, nZId, &nd);
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
            + ((nBlockXSize * nBlockYSize - edge[nBandXPos] * nYChunkSize)
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
                                       nYChunkSize, false);
        }
        else
        {
            status = nc_get_vara_uchar(cdfid, nZId, start, edge,
                                       static_cast<unsigned char *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<unsigned char>(pImage, pImageNC, edge[nBandXPos],
                                         nYChunkSize, false);
        }
    }
    else if( eDataType == GDT_Int16 )
    {
        status = nc_get_vara_short(cdfid, nZId, start, edge,
                                   static_cast<short *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<short>(pImage, pImageNC, edge[nBandXPos], nYChunkSize,
                             false);
    }
    else if( eDataType == GDT_Int32 )
    {
#if SIZEOF_UNSIGNED_LONG == 4
            status = nc_get_vara_long(cdfid, nZId, start, edge,
                                      static_cast<long *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<long>(pImage, pImageNC, edge[nBandXPos],
                                nYChunkSize, false);
#else
            status = nc_get_vara_int(cdfid, nZId, start, edge,
                                     static_cast<int *>(pImageNC));
            if( status == NC_NOERR )
                CheckData<int>(pImage, pImageNC, edge[nBandXPos],
                               nYChunkSize, false);
#endif
    }
    else if( eDataType == GDT_Float32 )
    {
        status = nc_get_vara_float(cdfid, nZId, start, edge,
                                   static_cast<float *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<float>(pImage, pImageNC, edge[nBandXPos], nYChunkSize,
                             true);
    }
    else if( eDataType == GDT_Float64 )
    {
        status = nc_get_vara_double(cdfid, nZId, start, edge,
                                    static_cast<double *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<double>(pImage, pImageNC, edge[nBandXPos],
                              nYChunkSize, true);
    }
#ifdef NETCDF_HAS_NC4
    else if( eDataType == GDT_UInt16 )
    {
        status = nc_get_vara_ushort(cdfid, nZId, start, edge,
                                    static_cast<unsigned short *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<unsigned short>(pImage, pImageNC, edge[nBandXPos],
                                      nYChunkSize, false);
    }
    else if( eDataType == GDT_UInt32 )
    {
        status = nc_get_vara_uint(cdfid, nZId, start, edge,
                                  static_cast<unsigned int *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<unsigned int>(pImage, pImageNC, edge[nBandXPos],
                                    nYChunkSize, false);
    }
    else if( eDataType == GDT_Int64 )
    {
        status = nc_get_vara_longlong(cdfid, nZId, start, edge,
                                      static_cast<long long *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<std::int64_t>(pImage, pImageNC, edge[nBandXPos],
                                    nYChunkSize, false);
    }
    else if( eDataType == GDT_UInt64 )
    {
        status = nc_get_vara_ulonglong(cdfid, nZId, start, edge,
                                       static_cast<unsigned long long *>(pImageNC));
        if( status == NC_NOERR )
            CheckData<std::uint64_t>(pImage, pImageNC, edge[nBandXPos],
                                     nYChunkSize, false);
    }
    else if ( eDataType == GDT_CInt16 )
    {
        status = nc_get_vara(cdfid, nZId, start, edge,
                              pImageNC);
        if ( status == NC_NOERR )
            CheckDataCpx<short>(pImage, pImageNC, edge[nBandXPos],
                    nYChunkSize, false);
    }
    else if ( eDataType == GDT_CInt32 )
    {
        status = nc_get_vara(cdfid, nZId, start, edge,
                              pImageNC);
        if (status == NC_NOERR)
            CheckDataCpx<int>(pImage, pImageNC, edge[nBandXPos],
                            nYChunkSize, false);
    }
    else if ( eDataType == GDT_CFloat32 )
    {
        status = nc_get_vara(cdfid, nZId, start, edge,
                              pImageNC);
        if (status == NC_NOERR)
            CheckDataCpx<float>(pImage, pImageNC, edge[nBandXPos],
                            nYChunkSize, false);
    }
    else if ( eDataType == GDT_CFloat64 )
    {
        status = nc_get_vara(cdfid, nZId, start, edge,
                              pImageNC);
        if (status == NC_NOERR)
            CheckDataCpx<double>(pImage, pImageNC, edge[nBandXPos],
                            nYChunkSize, false);
    }


#endif
    else
        status = NC_EBADTYPE;

    if( status != NC_NOERR )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "netCDF chunk fetch failed: #%d (%s)", status,
                 nc_strerror(status));
        return false;
    }
    return true;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr netCDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void *pImage )

{
    CPLMutexHolderD(&hNCMutex);

    // Locate X, Y and Z position in the array.

    size_t xstart = nBlockXOff * nBlockXSize;
    size_t ystart = 0;

    // Check y order.
    if( nBandYPos >= 0 )
    {
        auto poGDS = static_cast<netCDFDataset *>(poDS);
        if( poGDS->bBottomUp )
        {
            if( nBlockYSize == 1 )
            {
                ystart = nRasterYSize - 1 - nBlockYOff;
            }
            else
            {
                // in GDAL space
                ystart = nBlockYOff * nBlockYSize;
                const size_t yend = std::min(ystart + nBlockYSize - 1,
                                             static_cast<size_t>(nRasterYSize - 1));
                // in netCDF space
                const size_t nFirstChunkLine = nRasterYSize - 1 - yend;
                const size_t nLastChunkLine = nRasterYSize - 1 - ystart;
                const size_t nFirstChunkBlock = nFirstChunkLine / nBlockYSize;
                const size_t nLastChunkBlock = nLastChunkLine / nBlockYSize;

                const auto firstKey = netCDFDataset::ChunkKey(
                    nBlockXOff, nFirstChunkBlock, nBand);
                const auto secondKey = netCDFDataset::ChunkKey(
                    nBlockXOff, nLastChunkBlock, nBand);

                // Retrieve data from the one or 2 needed netCDF chunks
                std::shared_ptr<std::vector<GByte>> firstChunk;
                std::shared_ptr<std::vector<GByte>> secondChunk;
                if( poGDS->poChunkCache )
                {
                    poGDS->poChunkCache->tryGet(firstKey, firstChunk);
                    if( firstKey != secondKey )
                        poGDS->poChunkCache->tryGet(secondKey, secondChunk);
                }
                const size_t nChunkLineSize = static_cast<size_t>(
                    GDALGetDataTypeSizeBytes(eDataType)) * nBlockXSize;
                const size_t nChunkSize = nChunkLineSize * nBlockYSize;
                if( !firstChunk )
                {
                    firstChunk.reset(new std::vector<GByte>(nChunkSize));
                    if( !FetchNetcdfChunk( xstart,
                                           nFirstChunkBlock * nBlockYSize,
                                           firstChunk.get()->data() ) )
                        return CE_Failure;
                    if( poGDS->poChunkCache )
                        poGDS->poChunkCache->insert(firstKey, firstChunk);
                }
                if( !secondChunk && firstKey != secondKey )
                {
                    secondChunk.reset(new std::vector<GByte>(nChunkSize));
                    if( !FetchNetcdfChunk( xstart,
                                           nLastChunkBlock * nBlockYSize,
                                           secondChunk.get()->data() ) )
                        return CE_Failure;
                    if( poGDS->poChunkCache )
                        poGDS->poChunkCache->insert(secondKey, secondChunk);
                }

                // Assemble netCDF chunks into GDAL block
                GByte* pabyImage = static_cast<GByte*>(pImage);
                const size_t nFirstChunkBlockLine = nFirstChunkBlock * nBlockYSize;
                const size_t nLastChunkBlockLine = nLastChunkBlock * nBlockYSize;
                for( size_t iLine = ystart; iLine <= yend; iLine ++ )
                {
                    const size_t nLineFromBottom = nRasterYSize - 1 - iLine;
                    const size_t nChunkY = nLineFromBottom / nBlockYSize;
                    if( nChunkY == nFirstChunkBlock )
                    {
                        memcpy(pabyImage + nChunkLineSize * (iLine - ystart),
                               firstChunk.get()->data() +
                                    (nLineFromBottom - nFirstChunkBlockLine) * nChunkLineSize,
                               nChunkLineSize);
                    }
                    else
                    {
                        CPLAssert(nChunkY == nLastChunkBlock);
                        assert(secondChunk);
                        memcpy(pabyImage + nChunkLineSize * (iLine - ystart),
                               secondChunk.get()->data() +
                                    (nLineFromBottom - nLastChunkBlockLine) * nChunkLineSize,
                               nChunkLineSize);
                    }
                }
                return CE_None;
            }
        }
        else
        {
            ystart = nBlockYOff * nBlockYSize;
        }
    }

    return FetchNetcdfChunk( xstart, ystart, pImage ) ? CE_None : CE_Failure;
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
    start[nBandXPos] = nBlockXOff * nBlockXSize;

    // check y order.
    if( static_cast<netCDFDataset *>(poDS)->bBottomUp )
    {
        if( nBlockYSize == 1 )
        {
            start[nBandYPos] = nRasterYSize - 1 - nBlockYOff;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "nBlockYSize = %d, only 1 supported when "
                     "writing bottom-up dataset",
                     nBlockYSize);
            return CE_Failure;
        }
    }
    else
    {
        start[nBandYPos] = nBlockYOff * nBlockYSize;  // y
    }

    size_t edge[MAX_NC_DIMS] = {};

    edge[nBandXPos] = nBlockXSize;
    if( (start[nBandXPos] + edge[nBandXPos]) > (size_t)nRasterXSize )
        edge[nBandXPos] = nRasterXSize - start[nBandXPos];
    edge[nBandYPos] = nBlockYSize;
    if( (start[nBandYPos] + edge[nBandYPos]) > (size_t)nRasterYSize )
        edge[nBandYPos] = nRasterYSize - start[nBandYPos];

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
    else if( eDataType == GDT_UInt64 &&
             static_cast<netCDFDataset *>(poDS)->eFormat == NCDF_FORMAT_NC4 )
    {
        status = nc_put_vara_ulonglong(cdfid, nZId, start, edge,
                                       static_cast<unsigned long long *>(pImage));
    }
    else if( eDataType == GDT_Int64 &&
             static_cast<netCDFDataset *>(poDS)->eFormat == NCDF_FORMAT_NC4 )
    {
        status = nc_put_vara_longlong(cdfid, nZId, start, edge,
                                      static_cast<long long *>(pImage));
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
#ifdef ENABLE_NCDUMP
    bFileToDestroyAtClosing(false),
#endif
    cdfid(-1),
    nSubDatasets(0),
    papszSubDatasets(nullptr),
    papszMetadata(nullptr),
    bBottomUp(true),
    eFormat(NCDF_FORMAT_NONE),
    bIsGdalFile(false),
    bIsGdalCfFile(false),
    pszCFProjection(nullptr),
    pszCFCoordinates(nullptr),
    nCFVersion(1.6),
    bSGSupport(false),
    eMultipleLayerBehavior(SINGLE_LAYER),
    logCount(0),
    vcdf(cdfid),
    GeometryScribe(vcdf, this->generateLogName()),
    FieldScribe(vcdf, this->generateLogName()),
    bufManager(CPLGetUsablePhysicalRAM() / 5),

    // projection/GT.
    m_pszProjection(CPLStrdup("")),
    nXDimID(-1),
    nYDimID(-1),
    bIsProjected(false),
    bIsGeographic(false),  // Can be not projected, and also not geographic

    // State vars.
    bDefineMode(true),
    bAddedGridMappingRef(false),

    // Create vars.
    papszCreationOptions(nullptr),
    eCompress(NCDF_COMPRESS_NONE),
    nZLevel(NCDF_DEFLATE_LEVEL),
#ifdef NETCDF_HAS_NC4
    bChunking(false),
#endif
    nCreateMode(NC_CLOBBER),
    bSignedData(true)
{
    // Projection/GT.
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;

    // Set buffers
    bufManager.addBuffer(&(GeometryScribe.getMemBuffer()));
    bufManager.addBuffer(&(FieldScribe.getMemBuffer()));
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

    // Write data related to geotransform
    if( GetAccess() == GA_Update &&
        !m_bAddedProjectionVarsData &&
        (m_bHasProjection || m_bHasGeoTransform) )
    {
        // Ensure projection is written if GeoTransform OR Projection are missing.
        if( !m_bAddedProjectionVarsDefs )
        {
            AddProjectionVars( true, nullptr, nullptr );
        }
        AddProjectionVars( false, nullptr, nullptr );
    }

    netCDFDataset::FlushCache(true);
    SGCommitPendingTransaction();

    for(size_t i = 0; i < apoVectorDatasets.size(); i++)
        delete apoVectorDatasets[i];

    // Make sure projection variable is written to band variable.
    if( GetAccess() == GA_Update && !bAddedGridMappingRef )
        AddGridMappingRef();

    CSLDestroy(papszMetadata);
    CSLDestroy(papszSubDatasets);
    CSLDestroy(papszCreationOptions);

    CPLFree(m_pszProjection);
    CPLFree(pszCFProjection);

    if( cdfid > 0 )
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "calling nc_close( %d)", cdfid);
#endif
        int status = nc_close(cdfid);
#ifdef ENABLE_UFFD
        NETCDF_UFFD_UNMAP(pCtx);
#endif
        NCDF_ERR(status);
    }

    if( fpVSIMEM )
        VSIFCloseL(fpVSIMEM);

#ifdef ENABLE_NCDUMP
    if( bFileToDestroyAtClosing )
        VSIUnlink( osFilename );
#endif
}

/************************************************************************/
/*                            SetDefineMode()                           */
/************************************************************************/
bool netCDFDataset::SetDefineMode( bool bNewDefineMode )
{
    // Do nothing if already in new define mode
    // or if dataset is in read-only mode or if dataset is true NC4 dataset.
    if( bDefineMode == bNewDefineMode || GetAccess() == GA_ReadOnly || eFormat == NCDF_FORMAT_NC4 )
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
    char** papszDomains = BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(), TRUE,
                                   "SUBDATASETS", nullptr);
    for( const auto& kv: m_oMapDomainToJSon )
        papszDomains = CSLAddString(papszDomains, ("json:" + kv.first).c_str());
    return papszDomains;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/
char **netCDFDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "SUBDATASETS") )
        return papszSubDatasets;

    if( pszDomain != nullptr && STARTS_WITH(pszDomain, "json:") )
    {
        auto iter = m_oMapDomainToJSon.find(pszDomain + strlen("json:"));
        if( iter != m_oMapDomainToJSon.end() )
            return iter->second.List();
    }

    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr netCDFDataset::SetMetadataItem( const char* pszName,
                                          const char* pszValue,
                                          const char* pszDomain )
{
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                  "netCDFDataset::SetMetadataItem() can only be "
                  "called in update mode");
        return CE_Failure;
    }

    if( (pszDomain == nullptr || pszDomain[0] == '\0') && pszValue != nullptr )
    {
        std::string osName(pszName);

        // Same logic as in CopyMetadata()
        if( STARTS_WITH(osName.c_str(), "NC_GLOBAL#") )
            osName = osName.substr(strlen("NC_GLOBAL#"));
        else if( strchr(osName.c_str(), '#') == nullptr )
            osName = "GDAL_" + osName;

        if( STARTS_WITH(osName.c_str(), "NETCDF_DIM_") ||
            strchr(osName.c_str(), '#') != nullptr )
        {
            // do nothing
        }
        else
        {
            SetDefineMode(true);

            if( !NCDFPutAttr(cdfid, NC_GLOBAL, osName.c_str(), pszValue) )
                return CE_Failure;
        }
    }

    return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                          SetMetadata()                               */
/************************************************************************/

CPLErr netCDFDataset::SetMetadata( char** papszMD, const char* pszDomain )
{
    if( pszDomain == nullptr || pszDomain[0] == '\0' )
    {
        // We don't handle metadata item removal for now
        for( const char* const*  papszIter = papszMD; papszIter && *papszIter; ++papszIter )
        {
            char* pszName = nullptr;
            const char* pszValue = CPLParseNameValue(*papszIter, &pszName);
            if( pszName && pszValue )
                SetMetadataItem(pszName, pszValue);
            CPLFree(pszName);
        }
    }
    return GDALPamDataset::SetMetadata(papszMD, pszDomain);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *netCDFDataset::_GetProjectionRef()
{
    if( m_bHasProjection )
        return m_pszProjection;

    return GDALPamDataset::_GetProjectionRef();
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *netCDFDataset::SerializeToXML( const char *pszUnused )

{
    // Overridden from GDALPamDataset to add only band histogram
    // and statistics. See bug #4244.

    if( psPam == nullptr )
        return nullptr;

    // Setup root node and attributes.
    CPLXMLNode *psDSTree = CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset");

    // Process bands.
    for( int iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        netCDFRasterBand *poBand =
            static_cast<netCDFRasterBand *>(GetRasterBand(iBand + 1));

        if( poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        CPLXMLNode *psBandTree = poBand->SerializeToXML(pszUnused);

        if( psBandTree != nullptr )
            CPLAddXMLChild(psDSTree, psBandTree);
    }

    // We don't want to return anything if we had no metadata to attach.
    if( psDSTree->psChild == nullptr )
    {
        CPLDestroyXMLNode(psDSTree);
        psDSTree = nullptr;
    }

    return psDSTree;
}

/************************************************************************/
/*                           FetchCopyParam()                            */
/************************************************************************/

double netCDFDataset::FetchCopyParam( const char *pszGridMappingValue,
                                     const char *pszParam, double dfDefault,
                                     bool *pbFound )

{
    char *pszTemp = CPLStrdup(CPLSPrintf("%s#%s",
                                         pszGridMappingValue, pszParam));
    const char *pszValue = CSLFetchNameValue(papszMetadata, pszTemp);
    CPLFree(pszTemp);

    if( pbFound )
    {
        *pbFound = (pszValue != nullptr);
    }

    if( pszValue )
    {
        return CPLAtofM(pszValue);
    }

    return dfDefault;
}

/************************************************************************/
/*                           FetchStandardParallels()                   */
/************************************************************************/

std::vector<std::string> netCDFDataset::FetchStandardParallels( const char *pszGridMappingValue )
{
    // cf-1.0 tags
    const char *pszValue = FetchAttr(pszGridMappingValue, CF_PP_STD_PARALLEL);

    std::vector<std::string> ret;
    if( pszValue != nullptr )
    {
        CPLStringList aosValues;
        if( pszValue[0] != '{' && CPLString(pszValue).Trim().find(' ') != std::string::npos )
        {
            // Some files like ftp://data.knmi.nl/download/KNW-NetCDF-3D/1.0/noversion/2013/11/14/KNW-1.0_H37-ERA_NL_20131114.nc
            // do not use standard formatting for arrays, but just space
            // separated syntax
            aosValues = CSLTokenizeString2(pszValue, " ", 0);
        }
        else
        {
            aosValues = NCDFTokenizeArray(pszValue);
        }
        for( int i = 0; i < aosValues.size(); i++ )
        {
            ret.push_back(aosValues[i]);
        }
    }
    // Try gdal tags.
    else
    {
        pszValue = FetchAttr(pszGridMappingValue, CF_PP_STD_PARALLEL_1);

        if( pszValue != nullptr )
            ret.push_back(pszValue);

        pszValue = FetchAttr(pszGridMappingValue, CF_PP_STD_PARALLEL_2);

        if( pszValue != nullptr )
            ret.push_back(pszValue);
    }

    return ret;
}

/************************************************************************/
/*                           FetchAttr()                                */
/************************************************************************/

const char *netCDFDataset::FetchAttr( const char *pszVarFullName,
                                      const char *pszAttr )

{
    char *pszKey = CPLStrdup(CPLSPrintf("%s#%s", pszVarFullName, pszAttr));
    const char *pszValue = CSLFetchNameValue(papszMetadata, pszKey);
    CPLFree(pszKey);
    return pszValue;
}

const char *netCDFDataset::FetchAttr( int nGroupId, int nVarId,
                                      const char *pszAttr )

{
    char *pszVarFullName = nullptr;
    NCDFGetVarFullName(nGroupId, nVarId, &pszVarFullName);
    const char *pszValue = FetchAttr(pszVarFullName, pszAttr);
    CPLFree(pszVarFullName);
    return pszValue;
}

/************************************************************************/
/*                       IsDifferenceBelow()                            */
/************************************************************************/

static bool IsDifferenceBelow(double dfA, double dfB, double dfError)
{
    const double dfAbsDiff = fabs(dfA - dfB);
    return dfAbsDiff <= dfError;
}

/************************************************************************/
/*                      SetProjectionFromVar()                          */
/************************************************************************/
void netCDFDataset::SetProjectionFromVar( int nGroupId, int nVarId,
                                          bool bReadSRSOnly, const char * pszGivenGM, std::string* returnProjStr,
                                          nccfdriver::SGeometry_Reader* sg)
{
    bool bGotGeogCS = false;
    bool bGotCfSRS = false;
    bool bGotCfWktSRS = false;
    bool bGotGdalSRS = false;
    bool bGotCfGT = false;
    bool bGotGdalGT = false;

    // These values from CF metadata.
    OGRSpatialReference oSRS;
    char szDimNameX[NC_MAX_NAME + 1];
    // char szDimNameY[NC_MAX_NAME + 1];
    size_t xdim = nRasterXSize;
    size_t ydim = nRasterYSize;

    // These values from GDAL metadata.
    const char *pszWKT = nullptr;
    const char *pszGeoTransform = nullptr;

    netCDFDataset *poDS = this;  // Perhaps this should be removed for clarity.

    CPLDebug("GDAL_netCDF", "\n=====\nSetProjectionFromVar( %d, %d)",
             nGroupId, nVarId);

    // Get x/y range information.

    // Temp variables to use in SetGeoTransform() and SetProjection().
    double adfTempGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    // Look for grid_mapping metadata.
    const char *pszValue = pszGivenGM;
    CPLString osTmpGridMapping; // let is in this outer scope as pszValue may point to it
    if( pszValue == nullptr )
    {
        pszValue = FetchAttr(nGroupId, nVarId, CF_GRD_MAPPING);
        if( pszValue && strchr(pszValue, ':') && strchr(pszValue, ' ') )
        {
            // Expanded form of grid_mapping
            // e.g. "crsOSGB: x y crsWGS84: lat lon"
            // Pickup the grid_mapping whose coordinates are dimensions of the
            // variable
            CPLStringList aosTokens(CSLTokenizeString2(pszValue, " ", 0));
            if( (aosTokens.size() % 3) == 0 )
            {
                for( int i = 0; i < aosTokens.size() / 3; i++ )
                {
                    if( CSLFindString(poDS->papszDimName, aosTokens[3*i+1]) >= 0 &&
                        CSLFindString(poDS->papszDimName, aosTokens[3*i+2]) >= 0 )
                    {
                        osTmpGridMapping = aosTokens[3*i];
                        if( !osTmpGridMapping.empty() && osTmpGridMapping.back() == ':' )
                        {
                            osTmpGridMapping.resize(osTmpGridMapping.size()-1);
                        }
                        pszValue = osTmpGridMapping.c_str();
                        break;
                    }
                }
            }
        }
    }
    char *pszGridMappingValue = CPLStrdup(pszValue ? pszValue : "");

    if( !EQUAL(pszGridMappingValue, "") )
    {
        // Read grid_mapping metadata.
        int nProjGroupID = -1;
        int nProjVarID = -1;
        if( NCDFResolveVar(nGroupId, pszGridMappingValue,
                           &nProjGroupID, &nProjVarID) == CE_None )
        {
            poDS->ReadAttributes(nProjGroupID, nProjVarID);

            // Look for GDAL spatial_ref and GeoTransform within grid_mapping.
            CPLFree(pszGridMappingValue);
            pszGridMappingValue = nullptr;
            NCDFGetVarFullName(nProjGroupID, nProjVarID, &pszGridMappingValue);
            if( pszGridMappingValue )
            {
                CPLDebug("GDAL_netCDF", "got grid_mapping %s",
                         pszGridMappingValue);
                pszWKT = FetchAttr(pszGridMappingValue, NCDF_SPATIAL_REF);
                if ( !pszWKT ) {
                    pszWKT = FetchAttr(pszGridMappingValue, NCDF_CRS_WKT);
                } else {
                    bGotGdalSRS = true;
                    CPLDebug("GDAL_netCDF", "setting WKT from GDAL");
                }
                if( pszWKT )
                {
                    if (!bGotGdalSRS) {
                        bGotCfWktSRS = true;
                        CPLDebug("GDAL_netCDF", "setting WKT from CF");
                    }
                    if(returnProjStr != nullptr)
                    {
                        (*returnProjStr) = std::string(pszWKT);
                    }
                    else
                    {
                        m_bAddedProjectionVarsDefs = true;
                        m_bAddedProjectionVarsData = true;
                        SetProjectionNoUpdate(pszWKT);
                    }
                    pszGeoTransform = FetchAttr(pszGridMappingValue,
                                                NCDF_GEOTRANSFORM);
                }
            }
            else
            {
                pszGridMappingValue = CPLStrdup("");
            }
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
    pszValue = FetchAttr("NC_GLOBAL", "GDAL");

    if( pszValue && NCDFIsGDALVersionGTE(pszValue, 1900))
    {
        bIsGdalFile = true;
        bIsGdalCfFile = true;
    }
    else if( pszWKT != nullptr && pszGeoTransform != nullptr )
    {
        bIsGdalFile = true;
        bIsGdalCfFile = false;
    }

    // Set default bottom-up default value.
    // Y axis dimension and absence of GT can modify this value.
    // Override with Config option GDAL_NETCDF_BOTTOMUP.

    // New driver is bottom-up by default.
    if( (bIsGdalFile && !bIsGdalCfFile) || bSwitchedXY )
        poDS->bBottomUp = false;
    else
        poDS->bBottomUp = true;

    CPLDebug("GDAL_netCDF", "bIsGdalFile=%d bIsGdalCfFile=%d bSwitchedXY=%d bBottomUp=%d",
             static_cast<int>(bIsGdalFile), static_cast<int>(bIsGdalCfFile),
             static_cast<int>(bSwitchedXY),
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

    bool bRotatedPole = false;

    if( !pszWKT && !EQUAL(pszGridMappingValue, "") )
    {
        pszValue = FetchAttr(pszGridMappingValue, CF_GRD_MAPPING_NAME);

        // Some files such as http://www.ecad.eu/download/ensembles/data/Grid_0.44deg_rot/tg_0.44deg_rot_v16.0.nc.gz
        // lack an explicit projection_var:grid_mapping_name attribute
        if( pszValue == nullptr &&
            FetchAttr(pszGridMappingValue, CF_PP_GRID_NORTH_POLE_LONGITUDE) != nullptr )
        {
            pszValue = CF_PT_ROTATED_LATITUDE_LONGITUDE;
        }

        if( pszValue != nullptr )
        {
            // Check for datum/spheroid information.
            double dfEarthRadius = poDS->FetchCopyParam(
                pszGridMappingValue, CF_PP_EARTH_RADIUS, -1.0);

            const double dfLonPrimeMeridian = poDS->FetchCopyParam(
                pszGridMappingValue, CF_PP_LONG_PRIME_MERIDIAN, 0.0);

            const char *pszPMName = FetchAttr(pszGridMappingValue,
                                              CF_PRIME_MERIDIAN_NAME);

            // Should try to find PM name from its value if not Greenwich.
            if( pszPMName == nullptr && !CPLIsEqual(dfLonPrimeMeridian, 0.0) )
                pszPMName = "unknown";

            double dfInverseFlattening = poDS->FetchCopyParam(
                pszGridMappingValue, CF_PP_INVERSE_FLATTENING, -1.0);

            double dfSemiMajorAxis = poDS->FetchCopyParam(
                pszGridMappingValue, CF_PP_SEMI_MAJOR_AXIS, -1.0);

            const double dfSemiMinorAxis = poDS->FetchCopyParam(
                pszGridMappingValue, CF_PP_SEMI_MINOR_AXIS, -1.0);

            // See if semi-major exists if radius doesn't.
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = dfSemiMajorAxis;

            // If still no radius, check old tag.
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_EARTH_RADIUS_OLD, -1.0);

            const char* pszEllipsoidName = FetchAttr(pszGridMappingValue,
                                                     CF_REFERENCE_ELLIPSOID_NAME);

            const char* pszDatumName = FetchAttr(pszGridMappingValue,
                                                 CF_HORIZONTAL_DATUM_NAME);

            const char* pszGeogName = FetchAttr(pszGridMappingValue,
                                                CF_GEOGRAPHIC_CRS_NAME);
            if( pszGeogName == nullptr )
                pszGeogName = "unknown";

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
                        oSRS.SetGeogCS(pszGeogName,
                                        pszDatumName,
                                        pszEllipsoidName ? pszEllipsoidName : "Sphere",
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

                        oSRS.SetGeogCS(pszGeogName,
                                        pszDatumName,
                                        pszEllipsoidName ? pszEllipsoidName : "Spheroid",
                                        dfEarthRadius, dfInverseFlattening,
                                        pszPMName, dfLonPrimeMeridian);
                        bGotGeogCS = true;
                    }
                }
                else
                {
                    oSRS.SetGeogCS(pszGeogName,
                                    pszDatumName,
                                    pszEllipsoidName ? pszEllipsoidName : "Spheroid",
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
            if( EQUAL(pszValue, CF_PT_TM) )
            {
                const double dfScale = poDS->FetchCopyParam(pszGridMappingValue,
                                              CF_PP_SCALE_FACTOR_MERIDIAN, 1.0);

                const double dfCenterLon = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                const double dfCenterLat = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

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
            if( EQUAL(pszValue, CF_PT_AEA) )
            {
                const double dfCenterLon =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                const double dfFalseEasting =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                         CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                         CF_PP_FALSE_NORTHING, 0.0);

                const auto aosStdParallels =
                    FetchStandardParallels(pszGridMappingValue);

                double dfStdP1 = 0;
                double dfStdP2 = 0;
                if( aosStdParallels.size() == 1 )
                {
                    // TODO CF-1 standard says it allows AEA to be encoded
                    // with only 1 standard parallel.  How should this
                    // actually map to a 2StdP OGC WKT version?
                    CPLError(
                        CE_Warning, CPLE_NotSupported,
                        "NetCDF driver import of AEA-1SP is not tested, "
                        "using identical std. parallels.");
                    dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
                    dfStdP2 = dfStdP1;
                }
                else if( aosStdParallels.size() == 2 )
                {
                    dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
                    dfStdP2 = CPLAtofM(aosStdParallels[1].c_str());
                }
                // Old default.
                else
                {
                    dfStdP1 =
                        poDS->FetchCopyParam(pszGridMappingValue,
                                             CF_PP_STD_PARALLEL_1, 0.0);

                    dfStdP2 =
                        poDS->FetchCopyParam(pszGridMappingValue,
                                             CF_PP_STD_PARALLEL_2, 0.0);
                }

                const double dfCenterLat =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0);

                bGotCfSRS = true;
                oSRS.SetACEA(dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                             dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Cylindrical Equal Area
            else if( EQUAL(pszValue, CF_PT_CEA) || EQUAL(pszValue, CF_PT_LCEA) )
            {
                const auto aosStdParallels =
                    FetchStandardParallels(pszGridMappingValue);

                double dfStdP1 = 0;
                if( !aosStdParallels.empty() )
                {
                    dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
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

                const double dfCentralMeridian = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetCEA(dfStdP1, dfCentralMeridian,
                             dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // lambert_azimuthal_equal_area.
            else if( EQUAL(pszValue, CF_PT_LAEA) )
            {
                const double dfCenterLon = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                const double dfCenterLat = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetLAEA(dfCenterLat, dfCenterLon,
                              dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                if( oSRS.GetAttrValue("DATUM") != nullptr &&
                    EQUAL(oSRS.GetAttrValue("DATUM"), "WGS_1984") )
                {
                    oSRS.SetProjCS("LAEA (WGS84)");
                }
            }

            // Azimuthal Equidistant.
            else if( EQUAL(pszValue, CF_PT_AE) )
            {
                const double dfCenterLon = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                const double dfCenterLat = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetAE( dfCenterLat, dfCenterLon,
                            dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Lambert conformal conic.
            else if( EQUAL(pszValue, CF_PT_LCC) )
            {
                const double dfCenterLon = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_LONG_CENTRAL_MERIDIAN, 0.0);

                const double dfCenterLat = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                const auto aosStdParallels = FetchStandardParallels(pszGridMappingValue);

                // 2SP variant.
                if( aosStdParallels.size() == 2 )
                {
                    const double dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
                    const double dfStdP2 = CPLAtofM(aosStdParallels[1].c_str());
                    oSRS.SetLCC(dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                                dfFalseEasting, dfFalseNorthing);
                }
                // 1SP variant (with standard_parallel or center lon).
                // See comments in netcdfdataset.h for this projection.
                else
                {
                    double dfScale = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_SCALE_FACTOR_ORIGIN, -1.0);

                    // CF definition, without scale factor.
                    if( CPLIsEqual(dfScale, -1.0) )
                    {
                        double dfStdP1;
                        // With standard_parallel.
                        if( aosStdParallels.size() == 1 )
                            dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());
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

                // If there is a standard_parallel, know it is Mercator 2SP.
                const auto aosStdParallels = FetchStandardParallels(pszGridMappingValue);

                if( !aosStdParallels.empty() )
                {
                    // CF-1 Mercator 2SP always has lat centered at equator.
                    const double dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());

                    const double dfCenterLat = 0.0;

                    const double dfCenterLon = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_LON_PROJ_ORIGIN, 0.0);

                    const double dfFalseEasting = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_FALSE_EASTING, 0.0);

                    const double dfFalseNorthing = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                    oSRS.SetMercator2SP(dfStdP1, dfCenterLat, dfCenterLon,
                                        dfFalseEasting, dfFalseNorthing);
                }
                else
                {
                    const double dfCenterLon = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_LON_PROJ_ORIGIN, 0.0);

                    const double dfCenterLat = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_LAT_PROJ_ORIGIN, 0.0);

                    const double dfScale = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

                    const double dfFalseEasting = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_FALSE_EASTING, 0.0);

                    const double dfFalseNorthing = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                    oSRS.SetMercator(dfCenterLat, dfCenterLon, dfScale,
                                     dfFalseEasting, dfFalseNorthing);
                }

                bGotCfSRS = true;

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Orthographic.
            else if( EQUAL (pszValue, CF_PT_ORTHOGRAPHIC) )
            {
                const double dfCenterLon = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                const double dfCenterLat = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;

                oSRS.SetOrthographic(dfCenterLat, dfCenterLon,
                                      dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Polar Stereographic.
            else if( EQUAL(pszValue, CF_PT_POLAR_STEREO) )
            {
                const auto aosStdParallels = FetchStandardParallels(pszGridMappingValue);

                const double dfCenterLon = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_VERT_LONG_FROM_POLE, 0.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                // CF allows the use of standard_parallel (lat_ts) OR
                // scale_factor (k0), make sure we have standard_parallel, using
                // Snyder eq. 22-7 with k=1 and lat=standard_parallel.
                if( !aosStdParallels.empty() )
                {
                    const double dfStdP1 = CPLAtofM(aosStdParallels[0].c_str());

                    // Polar Stereographic Variant B with latitude of standard parallel
                    oSRS.SetPS(dfStdP1, dfCenterLon, 1.0,
                                dfFalseEasting, dfFalseNorthing);
                }
                else
                {
                    // Fetch latitude_of_projection_origin (+90/-90).
                    double dfLatProjOrigin = poDS->FetchCopyParam(
                        pszGridMappingValue, CF_PP_LAT_PROJ_ORIGIN, 0.0);
                    if( !CPLIsEqual(dfLatProjOrigin, 90.0) &&
                        !CPLIsEqual(dfLatProjOrigin, -90.0) )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Polar Stereographic must have a %s "
                                 "parameter equal to +90 or -90.",
                                 CF_PP_LAT_PROJ_ORIGIN);
                        dfLatProjOrigin = 90.0;
                    }

                    const double dfScale = poDS->FetchCopyParam(pszGridMappingValue,
                                              CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

                    // Polar Stereographic Variant A with scale factor at natural
                    // origin and latitude of origin = +/- 90
                    oSRS.SetPS(dfLatProjOrigin, dfCenterLon, dfScale,
                                dfFalseEasting, dfFalseNorthing);
                }

                bGotCfSRS = true;

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Stereographic.
            else if( EQUAL(pszValue, CF_PT_STEREO) )
            {
                const double dfCenterLon = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                const double dfCenterLat = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LAT_PROJ_ORIGIN, 0.0);

                const double dfScale = poDS->FetchCopyParam(pszGridMappingValue,
                                              CF_PP_SCALE_FACTOR_ORIGIN, 1.0);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetStereographic(dfCenterLat, dfCenterLon, dfScale,
                                      dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");
            }

            // Geostationary.
            else if( EQUAL(pszValue, CF_PT_GEOS) )
            {
                const double dfCenterLon = poDS->FetchCopyParam(pszGridMappingValue,
                                                  CF_PP_LON_PROJ_ORIGIN, 0.0);

                const double dfSatelliteHeight =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                        CF_PP_PERSPECTIVE_POINT_HEIGHT, 35785831.0);

                const char *pszSweepAxisAngle =
                    poDS->FetchAttr(pszGridMappingValue, CF_PP_SWEEP_ANGLE_AXIS);

                const double dfFalseEasting = poDS->FetchCopyParam(pszGridMappingValue,
                                                     CF_PP_FALSE_EASTING, 0.0);

                const double dfFalseNorthing = poDS->FetchCopyParam(
                    pszGridMappingValue, CF_PP_FALSE_NORTHING, 0.0);

                bGotCfSRS = true;
                oSRS.SetGEOS(dfCenterLon, dfSatelliteHeight,
                              dfFalseEasting, dfFalseNorthing);

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS("WGS84");

                if( pszSweepAxisAngle != nullptr && EQUAL(pszSweepAxisAngle, "x") )
                {
                    char *pszProj4 = nullptr;
                    oSRS.exportToProj4(&pszProj4);
                    CPLString osProj4 = pszProj4;
                    osProj4 += " +sweep=x";
                    oSRS.SetExtension(oSRS.GetRoot()->GetValue(),
                                      "PROJ4", osProj4);
                    CPLFree(pszProj4);
                }
            }

            else if( EQUAL(pszValue, CF_PT_ROTATED_LATITUDE_LONGITUDE) )
            {
                const double dfGridNorthPoleLong =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                        CF_PP_GRID_NORTH_POLE_LONGITUDE,0.0);
                const double dfGridNorthPoleLat =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                        CF_PP_GRID_NORTH_POLE_LATITUDE,0.0);
                const double dfNorthPoleGridLong =
                    poDS->FetchCopyParam(pszGridMappingValue,
                                        CF_PP_NORTH_POLE_GRID_LONGITUDE,0.0);

                oSRS.SetDerivedGeogCRSWithPoleRotationNetCDFCFConvention(
                                                           "Rotated_pole",
                                                           dfGridNorthPoleLat,
                                                           dfGridNorthPoleLong,
                                                           dfNorthPoleGridLong);
                bRotatedPole = true;
            }

            if( oSRS.IsProjected() )
            {
                const char* pszProjectedCRSName = FetchAttr(pszGridMappingValue,
                                                            CF_PROJECTED_CRS_NAME);
                if( pszProjectedCRSName )
                    oSRS.SetProjCS(pszProjectedCRSName);
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
    else
    {
        // Dataset from https://github.com/OSGeo/gdal/issues/4075 has a "crs"
        // attribute hold on the variable of interest that contains a PROJ.4 string
        pszValue = FetchAttr(nGroupId, nVarId, "crs");
        if( pszValue &&
            (strstr(pszValue, "+proj=") != nullptr ||
             strstr(pszValue, "GEOGCS") != nullptr ||
             strstr(pszValue, "PROJCS") != nullptr ||
             strstr(pszValue, "EPSG:") != nullptr ) &&
            oSRS.SetFromUserInput(pszValue) == OGRERR_NONE )
        {
            bGotCfSRS = true;
        }
    }
    // Read projection coordinates.

    int nGroupDimXID = -1;
    int nVarDimXID = -1;
    int nGroupDimYID = -1;
    int nVarDimYID = -1;
    if(sg != nullptr)
    {
       nGroupDimXID = sg->get_ncID();
       nGroupDimYID = sg->get_ncID();
       nVarDimXID = sg->getNodeCoordVars()[0];
       nVarDimYID = sg->getNodeCoordVars()[1];
    }

    if( !bReadSRSOnly )
    {
        NCDFResolveVar(nGroupId, poDS->papszDimName[nXDimID],
                       &nGroupDimXID, &nVarDimXID);
        NCDFResolveVar(nGroupId, poDS->papszDimName[nYDimID],
                       &nGroupDimYID, &nVarDimYID);
        // TODO: if above resolving fails we should also search for coordinate
        // variables without same name than dimension using the same resolving
        // logic. This should handle for example NASA Ocean Color L2 products.

        const bool bIgnoreXYAxisNameChecks =
            CPLTestBool(
                CSLFetchNameValueDef(papszOpenOptions, "IGNORE_XY_AXIS_NAME_CHECKS",
                    CPLGetConfigOption("GDAL_NETCDF_IGNORE_XY_AXIS_NAME_CHECKS", "NO"))) ||
            // Dataset from https://github.com/OSGeo/gdal/issues/4075 has a res and transform attributes
            (FetchAttr(nGroupId, nVarId, "res") != nullptr &&
             FetchAttr(nGroupId, nVarId, "transform") != nullptr) ||
            FetchAttr(nGroupId, NC_GLOBAL, "GMT_version") != nullptr;

        // Check that they are 1D or 2D variables
        if( nVarDimXID >= 0 )
        {
            int ndims = -1;
            nc_inq_varndims(nGroupId, nVarDimXID, &ndims);
            if( ndims == 0 || ndims > 2 )
                nVarDimXID = -1;
            else if( !bIgnoreXYAxisNameChecks )
            {
                if( !NCDFIsVarLongitude(nGroupId, nVarDimXID, nullptr) &&
                    !NCDFIsVarProjectionX(nGroupId, nVarDimXID, nullptr) &&
                    // In case of inversion of X/Y
                    !NCDFIsVarLatitude(nGroupId, nVarDimXID, nullptr) &&
                    !NCDFIsVarProjectionY(nGroupId, nVarDimXID, nullptr) )
                {
                    CPLDebug("netCDF",
                             "Georeferencing ignored due to non-specific "
                             "enough X axis name. "
                             "Set GDAL_NETCDF_IGNORE_XY_AXIS_NAME_CHECKS=YES "
                             "as configuration option to bypass this check");
                    nVarDimXID = -1;
                }
            }
        }

        if( nVarDimYID >= 0 )
        {
            int ndims = -1;
            nc_inq_varndims(nGroupId, nVarDimYID, &ndims);
            if( ndims == 0 || ndims > 2 )
                nVarDimYID = -1;
            else if( !bIgnoreXYAxisNameChecks )
            {
                if( !NCDFIsVarLatitude(nGroupId, nVarDimYID, nullptr) &&
                    !NCDFIsVarProjectionY(nGroupId, nVarDimYID, nullptr) &&
                    // In case of inversion of X/Y
                    !NCDFIsVarLongitude(nGroupId, nVarDimYID, nullptr) &&
                    !NCDFIsVarProjectionX(nGroupId, nVarDimYID, nullptr) )
                {
                    CPLDebug("netCDF",
                             "Georeferencing ignored due to non-specific "
                             "enough Y axis name. "
                             "Set GDAL_NETCDF_IGNORE_XY_AXIS_NAME_CHECKS=YES "
                             "as configuration option to bypass this check");
                    nVarDimYID = -1;
                }
            }
        }

        if( (nVarDimXID >= 0 && xdim == 1) || (nVarDimXID >= 0 && ydim == 1) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "1-pixel width/height files not supported, "
                     "xdim: %ld ydim: %ld",
                     static_cast<long>(xdim), static_cast<long>(ydim));
            nVarDimXID = -1;
            nVarDimYID = -1;
        }
    }

    // Set Projection from CF.
    if( ( bGotGeogCS || bGotCfSRS ) )
    {
        if( (nVarDimXID != -1) && (nVarDimYID != -1) &&
            xdim > 0 && ydim > 0 )
        {
            // Set SRS Units.

            // Check units for x and y.
            if( oSRS.IsProjected() )
            {
                const char *pszUnitsX = FetchAttr(nGroupDimXID, nVarDimXID,
                                                  "units" );
                const char *pszUnitsY = FetchAttr(nGroupDimYID, nVarDimYID,
                                                  "units" );

                const char *pszUnits = nullptr;

                // TODO: What to do if units are not equal in X and Y.
                if( (pszUnitsX != nullptr) && (pszUnitsY != nullptr) &&
                     EQUAL(pszUnitsX, pszUnitsY) )
                    pszUnits = pszUnitsX;

                // Add units to PROJCS.
                if( pszUnits != nullptr && !EQUAL(pszUnits, "") )
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
            else if( oSRS.IsGeographic() && !bRotatedPole )
            {
                oSRS.SetAngularUnits(CF_UNITS_D, CPLAtof(SRS_UA_DEGREE_CONV));
                oSRS.SetAuthority("GEOGCS|UNIT", "EPSG", 9122);
            }
        }

        // Set projection.
        char *pszTempProjection = nullptr;
        oSRS.exportToWkt(&pszTempProjection);
        if( pszTempProjection )
        {
            CPLDebug("GDAL_netCDF", "setting WKT from CF");
            if(returnProjStr != nullptr)
            {
                (*returnProjStr) = std::string(pszTempProjection);
            }
            else
            {
                m_bAddedProjectionVarsDefs = true;
                m_bAddedProjectionVarsData = true;
                SetProjectionNoUpdate(pszTempProjection);
            }
        }
        CPLFree(pszTempProjection);
    }

    if( !bReadSRSOnly && (nVarDimXID != -1) && (nVarDimYID != -1) &&
        xdim > 0 && ydim > 0 )
    {
        double* pdfXCoord = static_cast<double *>(CPLCalloc(xdim, sizeof(double)));
        double* pdfYCoord = static_cast<double *>(CPLCalloc(ydim, sizeof(double)));

        size_t start[2] = { 0, 0 };
        size_t edge[2] = { xdim, 0 };
        int status = nc_get_vara_double(nGroupDimXID, nVarDimXID,
                                        start, edge, pdfXCoord);
        NCDF_ERR(status);

        edge[0] = ydim;
        status = nc_get_vara_double(nGroupDimYID, nVarDimYID,
                                    start, edge, pdfYCoord);
        NCDF_ERR(status);

        nc_type nc_var_dimx_datatype = NC_NAT;
        status = nc_inq_vartype(nGroupDimXID, nVarDimXID, &nc_var_dimx_datatype);
        NCDF_ERR(status);

        nc_type nc_var_dimy_datatype = NC_NAT;
        status = nc_inq_vartype(nGroupDimYID, nVarDimYID, &nc_var_dimy_datatype);
        NCDF_ERR(status);

        if( !poDS->bSwitchedXY )
        {
            // Convert ]180,540] longitude values to ]-180,0].
            if( NCDFIsVarLongitude(nGroupDimXID, nVarDimXID, nullptr) &&
                CPLTestBool(CPLGetConfigOption("GDAL_NETCDF_CENTERLONG_180",
                                            "YES")) )
            {
                // If minimum longitude is > 180, subtract 360 from all.
                // Add a check on the maximum X value too, since NCDFIsVarLongitude()
                // is not very specific by default (see https://github.com/OSGeo/gdal/issues/1440)
                if( std::min(pdfXCoord[0], pdfXCoord[xdim - 1]) > 180.0 &&
                    std::max(pdfXCoord[0], pdfXCoord[xdim - 1]) <= 540 )
                {
                    CPLDebug("GDAL_netCDF",
                             "Offsetting longitudes from ]180,540] to ]-180,180]. "
                             "Can be disabled with GDAL_NETCDF_CENTERLONG_180=NO");
                    for( size_t i = 0; i < xdim; i++ )
                            pdfXCoord[i] -= 360;
                }
            }
        }

        // Is pixel spacing uniform across the map?

        // Check Longitude.

        bool bLonSpacingOK = false;
        if( xdim == 2 )
        {
            bLonSpacingOK = true;
        }
        else
        {
            bool bWestIsLeft = (pdfXCoord[0] < pdfXCoord[xdim - 1]);

            // fix longitudes if longitudes should increase from
            // west to east, but west > east
            if (NCDFIsVarLongitude(nGroupDimXID, nVarDimXID, nullptr) &&
                !bWestIsLeft)
            {
                size_t ndecreases = 0;

                // there is lon wrap if longitudes increase
                // with one single decrease
                for( size_t i = 1; i < xdim; i++ )
                {
                    if (pdfXCoord[i] < pdfXCoord[i - 1])
                        ndecreases++;
                }

                if (ndecreases == 1)
                {
                    CPLDebug("GDAL_netCDF", "longitude wrap detected");
                    for( size_t i = 0; i < xdim; i++ )
                    {
                        if (pdfXCoord[i] > pdfXCoord[xdim - 1])
                            pdfXCoord[i] -= 360;
                    }
                }
            }

            const double dfSpacingBegin = pdfXCoord[1] - pdfXCoord[0];
            const double dfSpacingMiddle =
                    pdfXCoord[xdim / 2 + 1] - pdfXCoord[xdim / 2];
            const double dfSpacingLast =
                    pdfXCoord[xdim - 1] - pdfXCoord[xdim - 2];

            CPLDebug("GDAL_netCDF",
                     "xdim: %ld dfSpacingBegin: %f dfSpacingMiddle: %f "
                     "dfSpacingLast: %f",
                     static_cast<long>(xdim),
                     dfSpacingBegin, dfSpacingMiddle, dfSpacingLast);
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF",
                     "xcoords: %f %f %f %f %f %f",
                     pdfXCoord[0], pdfXCoord[1],
                     pdfXCoord[xdim / 2], pdfXCoord[(xdim / 2) + 1],
                     pdfXCoord[xdim - 2], pdfXCoord[xdim - 1]);
#endif

            // ftp://ftp.cdc.noaa.gov/Datasets/NARR/Dailies/monolevel/vwnd.10m.2015.nc
            // requires a 0.02% tolerance, so let's settle for 0.05%

            // For float variables, increase to 0.2% (as seen in https://github.com/OSGeo/gdal/issues/3663)
            const double dfEpsRel =
                nc_var_dimx_datatype == NC_FLOAT ? 0.002 : 0.0005;

            const double dfEps = dfEpsRel * std::max(fabs(dfSpacingBegin),
                        std::max(fabs(dfSpacingMiddle), fabs(dfSpacingLast)));
            if( IsDifferenceBelow(dfSpacingBegin, dfSpacingLast, dfEps) &&
                IsDifferenceBelow(dfSpacingBegin, dfSpacingMiddle, dfEps) &&
                IsDifferenceBelow(dfSpacingMiddle, dfSpacingLast, dfEps) )
            {
                bLonSpacingOK = true;
            }
            else if( CPLTestBool(CPLGetConfigOption(
                "GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK", "NO")) )
            {
                bLonSpacingOK = true;
                CPLDebug("GDAL_netCDF",
                     "Longitude/X is not equally spaced, but will be considered "
                     "as such because of GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK");
            }
        }

        if( bLonSpacingOK == false )
        {
            CPLDebug("GDAL_netCDF",
                     "%s",
                     "Longitude/X is not equally spaced (with a 0.05% tolerance). "
                     "You may set the "
                     "GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK configuration "
                     "option to YES to ignore this check");
        }

        // Check Latitude.
        bool bLatSpacingOK = false;

        if( ydim == 2 )
        {
            bLatSpacingOK = true;
        }
        else
        {
            const double dfSpacingBegin = pdfYCoord[1] - pdfYCoord[0];
            const double dfSpacingMiddle =
                pdfYCoord[ydim / 2 + 1] - pdfYCoord[ydim / 2];

            const double dfSpacingLast =
                pdfYCoord[ydim - 1] - pdfYCoord[ydim - 2];

            CPLDebug("GDAL_netCDF",
                     "ydim: %ld dfSpacingBegin: %f dfSpacingMiddle: %f dfSpacingLast: %f",
                     (long)ydim, dfSpacingBegin, dfSpacingMiddle, dfSpacingLast);
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF",
                     "ycoords: %f %f %f %f %f %f",
                     pdfYCoord[0], pdfYCoord[1], pdfYCoord[ydim / 2], pdfYCoord[(ydim / 2) + 1],
                     pdfYCoord[ydim - 2], pdfYCoord[ydim - 1]);
#endif

            const double dfEpsRel =
                nc_var_dimy_datatype == NC_FLOAT ? 0.002 : 0.0005;

            const double dfEps = dfEpsRel * std::max(fabs(dfSpacingBegin),
                        std::max(fabs(dfSpacingMiddle), fabs(dfSpacingLast)));
            if( IsDifferenceBelow(dfSpacingBegin, dfSpacingLast, dfEps) &&
                IsDifferenceBelow(dfSpacingBegin, dfSpacingMiddle, dfEps) &&
                IsDifferenceBelow(dfSpacingMiddle, dfSpacingLast, dfEps) )
            {
                bLatSpacingOK = true;
            }
            else if( CPLTestBool(CPLGetConfigOption(
                "GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK", "NO")) )
            {
                bLatSpacingOK = true;
                CPLDebug("GDAL_netCDF",
                     "Latitude/Y is not equally spaced, but will be considered "
                     "as such because of GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK");
            }
            else if( !oSRS.IsProjected() &&
                     fabs(dfSpacingBegin - dfSpacingLast) <= 0.1 &&
                     fabs(dfSpacingBegin - dfSpacingMiddle) <= 0.1 &&
                     fabs(dfSpacingMiddle - dfSpacingLast) <= 0.1 )
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
                Set1DGeolocation(nGroupDimYID, nVarDimYID, "Y");
            }

            if( bLatSpacingOK == false )
            {
                CPLDebug("GDAL_netCDF",
                     "%s",
                     "Latitude/Y is not equally spaced (with a 0.05% tolerance). "
                     "You may set the "
                     "GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK configuration "
                     "option to YES to ignore this check");
            }
        }

        if( bLonSpacingOK && bLatSpacingOK )
        {
            // We have gridded data so we can set the Georeferencing info.

            // Enable GeoTransform.

            // In the following "actual_range" and "node_offset"
            // are attributes used by netCDF files created by GMT.
            // If we find them we know how to proceed. Else, use
            // the original algorithm.
            bGotCfGT = true;

            int node_offset = 0;
            NCDFResolveAttInt(nGroupId, NC_GLOBAL, "node_offset", &node_offset);

            double adfActualRange[2] = { 0.0, 0.0 };
            double xMinMax[2] = { 0.0, 0.0 };
            double yMinMax[2] = { 0.0, 0.0 };

            if( !nc_get_att_double(nGroupDimXID, nVarDimXID,
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

            if( !nc_get_att_double(nGroupDimYID, nVarDimYID,
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

            double dfCoordOffset = 0.0;
            double dfCoordScale = 1.0;
            if( !nc_get_att_double(nGroupId, nVarDimXID,
                                   CF_ADD_OFFSET, &dfCoordOffset) &&
                !nc_get_att_double(nGroupId, nVarDimXID,
                                   CF_SCALE_FACTOR, &dfCoordScale) )
            {
                xMinMax[0] = dfCoordOffset + xMinMax[0] * dfCoordScale;
                xMinMax[1] = dfCoordOffset + xMinMax[1] * dfCoordScale;
            }

            if ( !nc_get_att_double(nGroupId, nVarDimYID,
                                    CF_ADD_OFFSET, &dfCoordOffset) &&
                 !nc_get_att_double(nGroupId, nVarDimYID,
                                    CF_SCALE_FACTOR, &dfCoordScale) )
            {
                yMinMax[0] = dfCoordOffset + yMinMax[0] * dfCoordScale;
                yMinMax[1] = dfCoordOffset + yMinMax[1] * dfCoordScale;
            }

            // Check for reverse order of y-coordinate.
            if( !bSwitchedXY )
            {
                poDS->bBottomUp = (yMinMax[0] <= yMinMax[1]);
                CPLDebug("GDAL_netCDF", "set bBottomUp = %d from Y axis",
                        static_cast<int>(poDS->bBottomUp));
                if( !poDS->bBottomUp )
                {
                    std::swap(yMinMax[0], yMinMax[1]);
                }
            }

            // Geostationary satellites can specify units in (micro)radians
            // So we check if they do, and if so convert to linear units (meters)
            const char *pszProjName = oSRS.GetAttrValue( "PROJECTION" );
            if( pszProjName != nullptr )
            {
                if( EQUAL( pszProjName, SRS_PT_GEOSTATIONARY_SATELLITE ) )
                {
                    double satelliteHeight = oSRS.GetProjParm(SRS_PP_SATELLITE_HEIGHT, 1.0);
                    size_t nAttlen = 0;
                    char szUnits[NC_MAX_NAME+1];
                    szUnits[0] = '\0';
                    nc_type nAttype=NC_NAT;
                    nc_inq_att(nGroupId, nVarDimXID, "units", &nAttype, &nAttlen);
                    if( nAttlen < sizeof(szUnits) &&
                        nc_get_att_text( nGroupId, nVarDimXID, "units",
                        szUnits ) == NC_NOERR )
                    {
                        szUnits[nAttlen] = '\0';
                        if( EQUAL( szUnits, "microradian" ) )
                        {
                            xMinMax[0] = xMinMax[0] * satelliteHeight * 0.000001;
                            xMinMax[1] = xMinMax[1] * satelliteHeight * 0.000001;
                        }
                        else if( EQUAL( szUnits, "rad" ) || EQUAL( szUnits, "radian" ) )
                        {
                            xMinMax[0] = xMinMax[0] * satelliteHeight;
                            xMinMax[1] = xMinMax[1] * satelliteHeight;
                        }
                    }
                    szUnits[0] = '\0';
                    nc_inq_att(nGroupId, nVarDimYID, "units", &nAttype, &nAttlen);
                    if( nAttlen < sizeof(szUnits) &&
                       nc_get_att_text( nGroupId, nVarDimYID, "units",
                       szUnits ) == NC_NOERR )
                    {
                        szUnits[nAttlen] = '\0';
                        if( EQUAL( szUnits, "microradian" ) )
                        {
                            yMinMax[0] = yMinMax[0] * satelliteHeight * 0.000001;
                            yMinMax[1] = yMinMax[1] * satelliteHeight * 0.000001;
                        }
                        else if( EQUAL( szUnits, "rad" ) || EQUAL( szUnits, "radian" ) )
                        {
                            yMinMax[0] = yMinMax[0] * satelliteHeight;
                            yMinMax[1] = yMinMax[1] * satelliteHeight;
                        }
                    }
                }
            }


            adfTempGeoTransform[0] = xMinMax[0];
            adfTempGeoTransform[1] = (xMinMax[1] - xMinMax[0]) /
                                     (poDS->nRasterXSize + (node_offset - 1));
            adfTempGeoTransform[2] = 0;
            if( bSwitchedXY )
            {
                adfTempGeoTransform[3] = yMinMax[0];
                adfTempGeoTransform[4] = 0;
                adfTempGeoTransform[5] = (yMinMax[1] - yMinMax[0]) /
                                        (poDS->nRasterYSize + (node_offset - 1));
            }
            else
            {
                adfTempGeoTransform[3] = yMinMax[1];
                adfTempGeoTransform[4] = 0;
                adfTempGeoTransform[5] = (yMinMax[0] - yMinMax[1]) /
                                        (poDS->nRasterYSize + (node_offset - 1));
            }

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

    // Process custom GeoTransform GDAL value.
    if( !EQUAL(pszGridMappingValue, "") && !bGotCfGT )
    {
        // TODO: Read the GT values and detect for conflict with CF.
        // This could resolve the GT precision loss issue.

        if( pszGeoTransform != nullptr )
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
        }
        else
        {
            // Look for corner array values.
            // CPLDebug("GDAL_netCDF",
            //           "looking for geotransform corners");
            bool bGotNN = false;
            double dfNN = FetchCopyParam(pszGridMappingValue,
                                        "Northernmost_Northing", 0, &bGotNN);

            bool bGotSN = false;
            double dfSN = FetchCopyParam(pszGridMappingValue,
                                        "Southernmost_Northing", 0, &bGotSN);

            bool bGotEE = false;
            double dfEE = FetchCopyParam(pszGridMappingValue,
                                        "Easternmost_Easting", 0, &bGotEE);

            bool bGotWE = false;
            double dfWE = FetchCopyParam(pszGridMappingValue,
                                        "Westernmost_Easting", 0, &bGotWE);

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
    }

    if ( !pszWKT && !bGotCfSRS ) {
        // Some netCDF files have a srid attribute (#6613) like
        // urn:ogc:def:crs:EPSG::6931
        const char *pszSRID = FetchAttr(pszGridMappingValue, "srid");
        if( pszSRID != nullptr )
        {
            oSRS.Clear();
            if( oSRS.SetFromUserInput(pszSRID, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) == OGRERR_NONE )
            {
                char *pszWKTExport = nullptr;
                CPLDebug("GDAL_netCDF", "Got SRS from %s", pszSRID);
                oSRS.exportToWkt(&pszWKTExport);
                if(returnProjStr != nullptr)
                {
                    (*returnProjStr) = std::string(pszWKTExport);
                }
                else
                {
                    m_bAddedProjectionVarsDefs = true;
                    m_bAddedProjectionVarsData = true;
                    SetProjectionNoUpdate(pszWKTExport);
                }
                CPLFree(pszWKTExport);
            }
        }
    }

    CPLFree(pszGridMappingValue);

    if( bReadSRSOnly )
        return;

    // Process geolocation arrays from CF "coordinates" attribute.
    if( ProcessCFGeolocation(nGroupId, nVarId) )
    {
        if( !oSRS.IsProjected() && !bSwitchedXY )
        {
            bGotCfGT = false;
        }
    }

    // Set GeoTransform if we got a complete one - after projection has been set
    if( bGotCfGT || bGotGdalGT )
    {
        m_bAddedProjectionVarsDefs = true;
        m_bAddedProjectionVarsData = true;
        SetGeoTransformNoUpdate(adfTempGeoTransform);
    }

    // Debugging reports.
    CPLDebug("GDAL_netCDF",
             "bGotGeogCS=%d bGotCfSRS=%d bGotCfGT=%d bGotCfWktSRS=%d "
             "bGotGdalSRS=%d bGotGdalGT=%d",
             static_cast<int>(bGotGeogCS), static_cast<int>(bGotCfSRS),
             static_cast<int>(bGotCfGT), static_cast<int>(bGotCfWktSRS),
             static_cast<int>(bGotGdalSRS), static_cast<int>(bGotGdalGT));

    if( !bGotCfGT && !bGotGdalGT )
        CPLDebug("GDAL_netCDF", "did not get geotransform from CF nor GDAL!");

    if( !bGotGeogCS && !bGotCfSRS && !bGotGdalSRS && !bGotCfGT && !bGotCfWktSRS)
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

void netCDFDataset::SetProjectionFromVar( int nGroupId, int nVarId,
                                          bool bReadSRSOnly )
{
    SetProjectionFromVar(nGroupId, nVarId, bReadSRSOnly, nullptr, nullptr, nullptr);
}

int netCDFDataset::ProcessCFGeolocation( int nGroupId, int nVarId )
{
    bool bAddGeoloc = false;
    char *pszTemp = nullptr;

    if( NCDFGetAttr(nGroupId, nVarId, "coordinates", &pszTemp) == CE_None )
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
                if( NCDFIsVarLongitude(nGroupId, -1, papszTokens[i]) )
                {
                    int nOtherGroupId = -1;
                    int nOtherVarId = -1;
                    // Check that the variable actually exists
                    // Needed on Sentinel-3 products
                    if( NCDFResolveVar(nGroupId, papszTokens[i],
                                       &nOtherGroupId, &nOtherVarId) == CE_None )
                    {
                        snprintf(szGeolocXName, sizeof(szGeolocXName),
                                 "%s",papszTokens[i]);
                    }
                }
                else if( NCDFIsVarLatitude(nGroupId, -1, papszTokens[i]) )
                {
                    int nOtherGroupId = -1;
                    int nOtherVarId = -1;
                    // Check that the variable actually exists
                    // Needed on Sentinel-3 products
                    if( NCDFResolveVar(nGroupId, papszTokens[i],
                                       &nOtherGroupId, &nOtherVarId) == CE_None )
                    {
                        snprintf(szGeolocYName, sizeof(szGeolocYName),
                                 "%s",papszTokens[i]);
                    }
                }
            }
            // Add GEOLOCATION metadata.
            if( !EQUAL(szGeolocXName, "") && !EQUAL(szGeolocYName, "") )
            {
                char *pszGeolocXFullName = nullptr;
                char *pszGeolocYFullName = nullptr;
                if( NCDFResolveVarFullName(nGroupId, szGeolocXName,
                                           &pszGeolocXFullName) == CE_None &&
                    NCDFResolveVarFullName(nGroupId, szGeolocYName,
                                           &pszGeolocYFullName) == CE_None )
                {
                    if( bSwitchedXY )
                    {
                        std::swap(pszGeolocXFullName, pszGeolocYFullName);
                        GDALPamDataset::SetMetadataItem("SWAP_XY", "YES", "GEOLOCATION");
                    }

                    bAddGeoloc = true;
                    CPLDebug("GDAL_netCDF",
                             "using variables %s and %s for GEOLOCATION",
                             pszGeolocXFullName, pszGeolocYFullName);

                    GDALPamDataset::SetMetadataItem("SRS", SRS_WKT_WGS84_LAT_LONG, "GEOLOCATION");

                    CPLString osTMP;
                    osTMP.Printf("NETCDF:\"%s\":%s",
                                 osFilename.c_str(), pszGeolocXFullName);

                    GDALPamDataset::SetMetadataItem("X_DATASET", osTMP, "GEOLOCATION");
                    GDALPamDataset::SetMetadataItem("X_BAND", "1" , "GEOLOCATION");
                    osTMP.Printf("NETCDF:\"%s\":%s",
                                 osFilename.c_str(), pszGeolocYFullName);

                    GDALPamDataset::SetMetadataItem("Y_DATASET", osTMP, "GEOLOCATION");
                    GDALPamDataset::SetMetadataItem("Y_BAND", "1", "GEOLOCATION");

                    GDALPamDataset::SetMetadataItem("PIXEL_OFFSET", "0", "GEOLOCATION");
                    GDALPamDataset::SetMetadataItem("PIXEL_STEP", "1", "GEOLOCATION");

                    GDALPamDataset::SetMetadataItem("LINE_OFFSET", "0", "GEOLOCATION");
                    GDALPamDataset::SetMetadataItem("LINE_STEP", "1", "GEOLOCATION");

                    GDALPamDataset::SetMetadataItem("GEOREFERENCING_CONVENTION", "PIXEL_CENTER", "GEOLOCATION");
                }
                else
                {
                    CPLDebug("GDAL_netCDF", "cannot resolve location of "
                             "lat/lon variables specified by the coordinates "
                             "attribute [%s]", pszTemp);
                }
                CPLFree(pszGeolocXFullName);
                CPLFree(pszGeolocYFullName);
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
    }

    CPLFree(pszTemp);

    return bAddGeoloc;
}

CPLErr netCDFDataset::Set1DGeolocation( int nGroupId, int nVarId,
                                        const char *szDimName )
{
    // Get values.
    char *pszVarValues = nullptr;
    CPLErr eErr = NCDFGet1DVar(nGroupId, nVarId, &pszVarValues);
    if( eErr != CE_None )
        return eErr;

    // Write metadata.
    char szTemp[ NC_MAX_NAME + 1 + 32 ] = {};
    snprintf(szTemp, sizeof(szTemp), "%s_VALUES", szDimName);
    GDALPamDataset::SetMetadataItem(szTemp, pszVarValues, "GEOLOCATION2");

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
    if( papszValues == nullptr )
        return nullptr;

    // Initialize and fill array.
    nVarLen = CSLCount(papszValues);
    double *pdfVarValues =
        static_cast<double *>(CPLCalloc(nVarLen, sizeof(double)));

    for( int i = 0, j = 0; i < nVarLen; i++ )
    {
        if( !bBottomUp ) j = nVarLen - 1 - i;
        else j = i;  // Invert latitude values.
        char *pszTemp = nullptr;
        pdfVarValues[j] = CPLStrtod(papszValues[i], &pszTemp);
    }
    CSLDestroy(papszValues);

    return pdfVarValues;
}

/************************************************************************/
/*                        SetProjectionNoUpdate()                       */
/************************************************************************/

void netCDFDataset::SetProjectionNoUpdate( const char * pszNewProjection )
{
    CPLFree(m_pszProjection);
    m_pszProjection = CPLStrdup(pszNewProjection);
    m_bHasProjection = true;
}

/************************************************************************/
/*                          _SetProjection()                            */
/************************************************************************/

CPLErr netCDFDataset::_SetProjection( const char * pszNewProjection )
{
    CPLMutexHolderD(&hNCMutex);

    if( GetAccess() != GA_Update || m_bHasProjection )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                  "netCDFDataset::_SetProjection() should only be called once "
                  "in update mode!\npszNewProjection=\n%s",
                  pszNewProjection);
        return CE_Failure;
    }

    CPLDebug("GDAL_netCDF", "SetProjection, WKT = %s", pszNewProjection);

    if( !STARTS_WITH_CI(pszNewProjection, "GEOGCS")
        && !STARTS_WITH_CI(pszNewProjection, "PROJCS")
        && !STARTS_WITH_CI(pszNewProjection, "GEOGCRS")
        && !EQUAL(pszNewProjection, "") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                  "Only OGC WKT GEOGCS and PROJCS Projections supported "
                  "for writing to NetCDF.  "
                  "%s not supported.",
                  pszNewProjection);

        return CE_Failure;
    }

    if( m_bHasGeoTransform )
    {
        SetProjectionNoUpdate(pszNewProjection);

        // For NC4/NC4C, writing both projection variables and data,
        // followed by redefining nodata value, cancels the projection
        // info from the Band variable, so for now only write the
        // variable definitions, and write data at the end.
        // See https://trac.osgeo.org/gdal/ticket/7245
        return AddProjectionVars(true, nullptr, nullptr);
    }

    SetProjectionNoUpdate(pszNewProjection);

    return CE_None;
}

/************************************************************************/
/*                     SetGeoTransformNoUpdate()                        */
/************************************************************************/

void netCDFDataset::SetGeoTransformNoUpdate( double * padfTransform )
{
    memcpy(m_adfGeoTransform, padfTransform, sizeof(double)*6);
    m_bHasGeoTransform = true;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::SetGeoTransform ( double * padfTransform )
{
    CPLMutexHolderD(&hNCMutex);

    if( GetAccess() != GA_Update || m_bHasGeoTransform )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "netCDFDataset::SetGeoTransform() should only be called once "
                 "in update mode!");
        return CE_Failure;
    }

    CPLDebug("GDAL_netCDF",
              "SetGeoTransform(%f,%f,%f,%f,%f,%f)",
              padfTransform[0], padfTransform[1], padfTransform[2],
              padfTransform[3], padfTransform[4], padfTransform[5]);

    if( m_bHasProjection )
    {
        SetGeoTransformNoUpdate(padfTransform);

        // For NC4/NC4C, writing both projection variables and data,
        // followed by redefining nodata value, cancels the projection
        // info from the Band variable, so for now only write the
        // variable definitions, and write data at the end.
        // See https://trac.osgeo.org/gdal/ticket/7245
        return AddProjectionVars(true, nullptr, nullptr);
    }

    SetGeoTransformNoUpdate(padfTransform);
    return CE_None;
}

/************************************************************************/
/*                         NCDFWriteSRSVariable()                       */
/************************************************************************/

int NCDFWriteSRSVariable(int cdfid, const OGRSpatialReference* poSRS,
                                char **ppszCFProjection, bool bWriteGDALTags, const std::string& srsVarName)
{
    char *pszCFProjection = nullptr;
    bool bWriteWkt = true;

    struct Value
    {
        std::string key{};
        std::string valueStr{};
        size_t      doubleCount = 0;
        double      doubles[2] = {0, 0};
    };

    std::vector<Value> oParams;

    const auto addParamString = [&oParams](const char* key, const char* value)
    {
        Value v;
        v.key = key;
        v.valueStr = value;
        oParams.push_back(v);
    };

    const auto addParamDouble = [&oParams](const char* key, double value)
    {
        Value v;
        v.key = key;
        v.doubleCount = 1;
        v.doubles[0] = value;
        oParams.push_back(v);
    };

    const auto addParam2Double = [&oParams](const char* key, double value1, double value2)
    {
        Value v;
        v.key = key;
        v.doubleCount = 2;
        v.doubles[0] = value1;
        v.doubles[1] = value2;
        oParams.push_back(v);
    };

    *ppszCFProjection = nullptr;

    if( poSRS->IsProjected() )
    {
        // Write CF-1.5 compliant Projected attributes.

        const OGR_SRSNode *poPROJCS = poSRS->GetAttrNode("PROJCS");
        if( poPROJCS == nullptr )
            return -1;
        const char *pszProjName = poSRS->GetAttrValue("PROJECTION");
        if( pszProjName == nullptr )
            return -1;

        // Basic Projection info (grid_mapping and datum).
        for( int i = 0; poNetcdfSRS_PT[i].WKT_SRS != nullptr; i++ )
        {
            if( EQUAL(poNetcdfSRS_PT[i].WKT_SRS, pszProjName) )
            {
                CPLDebug("GDAL_netCDF", "GDAL PROJECTION = %s , NCDF PROJECTION = %s",
                            poNetcdfSRS_PT[i].WKT_SRS,
                            poNetcdfSRS_PT[i].CF_SRS);
                pszCFProjection = CPLStrdup(poNetcdfSRS_PT[i].CF_SRS);
                CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                         cdfid, poNetcdfSRS_PT[i].CF_SRS, NC_CHAR);
                break;
            }
        }
        if( pszCFProjection == nullptr )
            return -1;

        addParamString(CF_GRD_MAPPING_NAME, pszCFProjection);

        // Various projection attributes.
        // PDS: keep in sync with SetProjection function
        auto oOutList = NCDFGetProjAttribs(poPROJCS, pszProjName);

        /* Write all the values that were found */
        double dfStdP[2] = {0, 0};
        bool bFoundStdP1 = false;
        bool bFoundStdP2 = false;
        for( const auto& it: oOutList )
        {
            const char* pszParamVal = it.first.c_str();
            double dfValue = it.second;
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
                addParamDouble(pszParamVal, dfValue);
            }
        }
        /* Now write the STD_PARALLEL attrib */
        if( bFoundStdP1 )
        {
            /* one value  */
            if( !bFoundStdP2 )
            {
                addParamDouble(CF_PP_STD_PARALLEL, dfStdP[0]);
            }
            else
            {
                // Two values.
                addParam2Double(CF_PP_STD_PARALLEL, dfStdP[0], dfStdP[1]);
            }
        }

        if( EQUAL(pszProjName, SRS_PT_GEOSTATIONARY_SATELLITE) )
        {
            const char *pszPredefProj4 = poSRS->GetExtension(
                        poSRS->GetRoot()->GetValue(), "PROJ4", nullptr);
            const char *pszSweepAxisAngle =
                (pszPredefProj4 != nullptr && strstr(pszPredefProj4, "+sweep=x")) ? "x" : "y";
            addParamString(CF_PP_SWEEP_ANGLE_AXIS, pszSweepAxisAngle);
        }
    }
    else if( poSRS->IsDerivedGeographic() )
    {
        const OGR_SRSNode *poConversion = poSRS->GetAttrNode("DERIVINGCONVERSION");
        if( poConversion == nullptr )
            return -1;
        const char *pszMethod = poSRS->GetAttrValue("METHOD");
        if( pszMethod == nullptr )
            return -1;

        std::map<std::string, double> oValMap;
        for( int iChild = 0; iChild < poConversion->GetChildCount(); iChild++ )
        {
            const OGR_SRSNode *poNode = poConversion->GetChild(iChild);
            if( !EQUAL(poNode->GetValue(), "PARAMETER") ||
                poNode->GetChildCount() <= 2 )
                continue;
            const char *pszParamStr = poNode->GetChild(0)->GetValue();
            const char *pszParamVal = poNode->GetChild(1)->GetValue();
            oValMap[pszParamStr] = CPLAtof(pszParamVal);
        }

        if( EQUAL(pszMethod, "PROJ ob_tran o_proj=longlat") )
        {
            // Not enough interoperable to be written as WKT
            bWriteWkt = false;

            const double dfLon0 = oValMap["lon_0"];
            const double dfLonp = oValMap["o_lon_p"];
            const double dfLatp = oValMap["o_lat_p"];

            pszCFProjection = CPLStrdup(ROTATED_POLE_VAR_NAME);
            addParamString(CF_GRD_MAPPING_NAME, CF_PT_ROTATED_LATITUDE_LONGITUDE);
            addParamDouble(CF_PP_GRID_NORTH_POLE_LONGITUDE, dfLon0 - 180);
            addParamDouble(CF_PP_GRID_NORTH_POLE_LATITUDE, dfLatp);
            addParamDouble(CF_PP_NORTH_POLE_GRID_LONGITUDE, dfLonp);
        }
        else if( EQUAL(pszMethod, "Pole rotation (netCDF CF convention)") )
        {
            // Not enough interoperable to be written as WKT
            bWriteWkt = false;

            const double dfGridNorthPoleLat = oValMap["Grid north pole latitude (netCDF CF convention)"];
            const double dfGridNorthPoleLong = oValMap["Grid north pole longitude (netCDF CF convention)"];
            const double dfNorthPoleGridLong = oValMap["North pole grid longitude (netCDF CF convention)"];

            pszCFProjection = CPLStrdup(ROTATED_POLE_VAR_NAME);
            addParamString(CF_GRD_MAPPING_NAME, CF_PT_ROTATED_LATITUDE_LONGITUDE);
            addParamDouble(CF_PP_GRID_NORTH_POLE_LONGITUDE, dfGridNorthPoleLong);
            addParamDouble(CF_PP_GRID_NORTH_POLE_LATITUDE, dfGridNorthPoleLat);
            addParamDouble(CF_PP_NORTH_POLE_GRID_LONGITUDE, dfNorthPoleGridLong);
        }
        else if( EQUAL(pszMethod, "Pole rotation (GRIB convention)") )
        {
            // Not enough interoperable to be written as WKT
            bWriteWkt = false;

            const double dfLatSouthernPole = oValMap["Latitude of the southern pole (GRIB convention)"];
            const double dfLonSouthernPole = oValMap["Longitude of the southern pole (GRIB convention)"];
            const double dfAxisRotation = oValMap["Axis rotation (GRIB convention)"];

            const double dfLon0 = dfLonSouthernPole;
            const double dfLonp = dfAxisRotation == 0 ? 0 : -dfAxisRotation;
            const double dfLatp = dfLatSouthernPole == 0 ? 0 : -dfLatSouthernPole;

            pszCFProjection = CPLStrdup(ROTATED_POLE_VAR_NAME);
            addParamString(CF_GRD_MAPPING_NAME, CF_PT_ROTATED_LATITUDE_LONGITUDE);
            addParamDouble(CF_PP_GRID_NORTH_POLE_LONGITUDE, dfLon0 - 180);
            addParamDouble(CF_PP_GRID_NORTH_POLE_LATITUDE, dfLatp);
            addParamDouble(CF_PP_NORTH_POLE_GRID_LONGITUDE, dfLonp);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported method for DerivedGeographicCRS: %s",
                     pszMethod);
            return -1;
        }
    }
    else
    {
        // Write CF-1.5 compliant Geographics attributes.
        // Note: WKT information will not be preserved (e.g. WGS84).
        pszCFProjection = CPLStrdup("crs");
        addParamString(CF_GRD_MAPPING_NAME, CF_PT_LATITUDE_LONGITUDE);
    }

    addParamString(CF_LNG_NAME, "CRS definition");


    // Write CF-1.5 compliant common attributes.

    // DATUM information.
    addParamDouble(CF_PP_LONG_PRIME_MERIDIAN, poSRS->GetPrimeMeridian());
    addParamDouble(CF_PP_SEMI_MAJOR_AXIS, poSRS->GetSemiMajor());
    addParamDouble(CF_PP_INVERSE_FLATTENING, poSRS->GetInvFlattening());

    if( bWriteWkt )
    {
        char *pszSpatialRef = nullptr;
        poSRS->exportToWkt(&pszSpatialRef);
        if( pszSpatialRef && pszSpatialRef[0] )
        {
            if ( bWriteGDALTags ) {
                // SPATIAL_REF is deprecated. Will be removed in GDAL 4.
                addParamString(NCDF_SPATIAL_REF, pszSpatialRef);
            }
            addParamString(NCDF_CRS_WKT, pszSpatialRef);
        }
        CPLFree(pszSpatialRef);
    }

    int NCDFVarID;
    std::map<std::string, size_t> oMapAttNameToIdx;
    std::string varNameRadix(pszCFProjection);
    int nCounter = 2;
    while( true )
    {
        NCDFVarID = -1;
        nc_inq_varid(cdfid, pszCFProjection, &NCDFVarID);
        if( NCDFVarID < 0 )
            break;
        if( oMapAttNameToIdx.empty() )
        {
            for( size_t i = 0; i < oParams.size(); ++i )
            {
                oMapAttNameToIdx[oParams[i].key] = i;
            }
        }
        int nbAttr = 0;
        NCDF_ERR(nc_inq_varnatts(cdfid, NCDFVarID, &nbAttr));
        bool bSame = nbAttr == static_cast<int>(oParams.size());
        for( int i = 0; bSame && (i < nbAttr); i++ )
        {
            char szAttrName[NC_MAX_NAME + 1];
            szAttrName[0] = 0;
            NCDF_ERR(nc_inq_attname(cdfid, NCDFVarID, i, szAttrName));
            auto oIter = oMapAttNameToIdx.find(szAttrName);
            if( oIter == oMapAttNameToIdx.end() )
            {
                bSame = false;
                break;
            }
            const auto& oParam(oParams[oIter->second]);

            nc_type atttype = NC_NAT;
            size_t attlen = 0;
            NCDF_ERR(nc_inq_att(cdfid, NCDFVarID, szAttrName, &atttype, &attlen));
            if( atttype != NC_CHAR && atttype != NC_DOUBLE )
            {
                bSame = false;
                break;
            }
            if( atttype == NC_CHAR )
            {
                if( oParam.doubleCount != 0 )
                {
                    bSame = false;
                    break;
                }
                std::string val;
                val.resize(attlen);
                nc_get_att_text(cdfid, NCDFVarID, szAttrName, &val[0]);
                if( val != oParam.valueStr )
                {
                    bSame = false;
                    break;
                }
            }
            else
            {
                if( oParam.doubleCount != attlen )
                {
                    bSame = false;
                    break;
                }
                double vals[2];
                nc_get_att_double(cdfid, NCDFVarID, szAttrName, vals);
                if( vals[0] != oParam.doubles[0] ||
                    (attlen == 2 && vals[1] != oParam.doubles[1]) )
                {
                    bSame = false;
                    break;
                }
            }
        }
        if( bSame )
        {
            *ppszCFProjection = pszCFProjection;
            return NCDFVarID;
        }
        CPLFree(pszCFProjection);
        pszCFProjection = CPLStrdup(CPLSPrintf("%s_%d", varNameRadix.c_str(), nCounter));
        nCounter ++;
    }

    *ppszCFProjection = pszCFProjection;

    const char* pszVarName;

    if(srsVarName != "")
    {
        pszVarName = srsVarName.c_str();
    }
    else
    {
        pszVarName = pszCFProjection;
    }

    int status =
        nc_def_var(cdfid, pszVarName, NC_CHAR, 0, nullptr, &NCDFVarID);
    NCDF_ERR(status);
    for( const auto& it: oParams )
    {
        if( it.doubleCount == 0 )
        {
            status = nc_put_att_text(cdfid, NCDFVarID,
                                     it.key.c_str(),
                                     it.valueStr.size(),
                                     it.valueStr.c_str());
        }
        else
        {
            status = nc_put_att_double(cdfid, NCDFVarID,
                                       it.key.c_str(),
                                       NC_DOUBLE, it.doubleCount,
                                       it.doubles);
        }
        NCDF_ERR(status);
    }

    return NCDFVarID;
}

/************************************************************************/
/*                   NCDFWriteLonLatVarsAttributes()                    */
/************************************************************************/

void NCDFWriteLonLatVarsAttributes(nccfdriver::netCDFVID & vcdf, int nVarLonID, int nVarLatID)
{

    try
    {
        vcdf.nc_put_vatt_text(nVarLatID, CF_STD_NAME, CF_LATITUDE_STD_NAME);
        vcdf.nc_put_vatt_text(nVarLatID, CF_LNG_NAME, CF_LATITUDE_LNG_NAME);
        vcdf.nc_put_vatt_text(nVarLatID, CF_UNITS, CF_DEGREES_NORTH);
        vcdf.nc_put_vatt_text(nVarLonID, CF_STD_NAME, CF_LONGITUDE_STD_NAME);
        vcdf.nc_put_vatt_text(nVarLonID, CF_LNG_NAME, CF_LONGITUDE_LNG_NAME);
        vcdf.nc_put_vatt_text(nVarLonID, CF_UNITS, CF_DEGREES_EAST);
    }
    catch(nccfdriver::SG_Exception& e)
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s", e.get_err_msg());
    }
}

/************************************************************************/
/*                   NCDFWriteRLonRLatVarsAttributes()                    */
/************************************************************************/

void NCDFWriteRLonRLatVarsAttributes(nccfdriver::netCDFVID & vcdf,
                                     int nVarRLonID, int nVarRLatID)
{
    try
    {
        vcdf.nc_put_vatt_text(nVarRLatID, CF_STD_NAME, "grid_latitude");
        vcdf.nc_put_vatt_text(nVarRLatID, CF_LNG_NAME, "latitude in rotated pole grid");
        vcdf.nc_put_vatt_text(nVarRLatID, CF_UNITS, "degrees");
        vcdf.nc_put_vatt_text(nVarRLatID, CF_AXIS, "Y");

        vcdf.nc_put_vatt_text(nVarRLonID, CF_STD_NAME, "grid_longitude");
        vcdf.nc_put_vatt_text(nVarRLonID, CF_LNG_NAME, "longitude in rotated pole grid");
        vcdf.nc_put_vatt_text(nVarRLonID, CF_UNITS, "degrees");
        vcdf.nc_put_vatt_text(nVarRLonID, CF_AXIS, "X");
    }
    catch(nccfdriver::SG_Exception& e)
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s", e.get_err_msg());
    }
}

/************************************************************************/
/*                        NCDFGetProjectedCFUnit()                      */
/************************************************************************/

const char* NCDFGetProjectedCFUnit(const OGRSpatialReference *poSRS)
{
    const char *pszUnits = nullptr;
    const char *pszUnitsToWrite = "";

    const double dfUnits = poSRS->GetLinearUnits(&pszUnits);
    if( fabs(dfUnits - 1.0) < 1e-15 || pszUnits == nullptr ||
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

    return pszUnitsToWrite;
}

/************************************************************************/
/*                     NCDFWriteXYVarsAttributes()                      */
/************************************************************************/

void NCDFWriteXYVarsAttributes(nccfdriver::netCDFVID& vcdf, int nVarXID, int nVarYID,
                               OGRSpatialReference *poSRS)
{
    const char *pszUnitsToWrite = NCDFGetProjectedCFUnit(poSRS);

    try
    {
        vcdf.nc_put_vatt_text(nVarXID, CF_STD_NAME, CF_PROJ_X_COORD);
        vcdf.nc_put_vatt_text(nVarXID, CF_LNG_NAME, CF_PROJ_X_COORD_LONG_NAME);
        vcdf.nc_put_vatt_text(nVarXID, CF_UNITS, pszUnitsToWrite);
        vcdf.nc_put_vatt_text(nVarYID, CF_STD_NAME, CF_PROJ_Y_COORD);
        vcdf.nc_put_vatt_text(nVarYID, CF_LNG_NAME, CF_PROJ_Y_COORD_LONG_NAME);
        vcdf.nc_put_vatt_text(nVarYID, CF_UNITS, pszUnitsToWrite);
    }
    catch(nccfdriver::SG_Exception& e)
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s", e.get_err_msg());
    }
}

/************************************************************************/
/*                          AddProjectionVars()                         */
/************************************************************************/

CPLErr netCDFDataset::AddProjectionVars( bool bDefsOnly,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData )
{
    if(nCFVersion >= 1.8)
        return CE_None; // do nothing

    bool bWriteGridMapping = false;
    bool bWriteLonLat = false;
    bool bHasGeoloc = false;
    bool bWriteGDALTags = false;
    bool bWriteGeoTransform = false;

    // For GEOLOCATION information.
    GDALDatasetH hDS_X = nullptr;
    GDALRasterBandH hBand_X = nullptr;
    GDALDatasetH hDS_Y = nullptr;
    GDALRasterBandH hBand_Y = nullptr;

    OGRSpatialReference oSRS;
    if( m_pszProjection )
    {
        oSRS.importFromWkt(m_pszProjection);

        if( oSRS.IsProjected() )
            bIsProjected = true;
        else if( oSRS.IsGeographic() )
            bIsGeographic = true;
    }

    if( bDefsOnly )
    {
        CPLDebug("GDAL_netCDF",
                "SetProjection, WKT now = [%s]\nprojected: %d geographic: %d",
                m_pszProjection,
                static_cast<int>(bIsProjected),
                static_cast<int>(bIsGeographic));

        if( !m_bHasGeoTransform )
            CPLDebug("GDAL_netCDF", "netCDFDataset::AddProjectionVars() called, "
                    "but GeoTransform has not yet been defined!");

        if( !m_bHasProjection )
            CPLDebug("GDAL_netCDF", "netCDFDataset::AddProjectionVars() called, "
                    "but Projection has not yet been defined!");
    }

    // Check GEOLOCATION information.
    char **papszGeolocationInfo = netCDFDataset::GetMetadata("GEOLOCATION");
    if( papszGeolocationInfo != nullptr )
    {
        // Look for geolocation datasets.
        const char *pszDSName =
            CSLFetchNameValue(papszGeolocationInfo, "X_DATASET");
        if( pszDSName != nullptr )
            hDS_X = GDALOpenShared(pszDSName, GA_ReadOnly);
        pszDSName = CSLFetchNameValue(papszGeolocationInfo, "Y_DATASET");
        if( pszDSName != nullptr )
            hDS_Y = GDALOpenShared(pszDSName, GA_ReadOnly);

        if( hDS_X != nullptr && hDS_Y != nullptr )
        {
            int nBand = std::max(1, atoi(CSLFetchNameValueDef(
                                        papszGeolocationInfo, "X_BAND", "0")));
            hBand_X = GDALGetRasterBand(hDS_X, nBand);
            nBand = std::max(1, atoi(CSLFetchNameValueDef(papszGeolocationInfo,
                                                          "Y_BAND", "0")));
            hBand_Y = GDALGetRasterBand(hDS_Y, nBand);

            // If geoloc bands are found, do basic validation based on their
            // dimensions.
            if( hBand_X != nullptr && hBand_Y != nullptr )
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
        const char* pszValue = CSLFetchNameValue(papszCreationOptions, "WRITE_LONLAT");
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
            pszCFCoordinates = NCDF_LONLAT;
        }
    }
    else
    {
        // Files without a Datum will not have a grid_mapping variable and
        // geographic information.
        bWriteGridMapping = bIsGeographic;

        if( bHasGeoloc )
        {
            bWriteLonLat = true;
        }
        else
        {
            bWriteGDALTags = CPL_TO_BOOL(CSLFetchBoolean(
                papszCreationOptions, "WRITE_GDAL_TAGS", bWriteGridMapping));
            if(bWriteGDALTags)
                bWriteGeoTransform = true;

            const char* pszValue =
                CSLFetchNameValueDef(papszCreationOptions, "WRITE_LONLAT", "YES");
            if( EQUAL(pszValue, "IF_NEEDED") )
                bWriteLonLat = true;
            else
                bWriteLonLat = CPLTestBool(pszValue);
            //  Don't write lon/lat if no source geotransform.
            if( !m_bHasGeoTransform )
                bWriteLonLat = false;
            // If we don't write lon/lat, set dimnames to X/Y and write gdal tags.
            if( !bWriteLonLat )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "creating geographic file without lon/lat values!");
                if( m_bHasGeoTransform )
                {
                    bWriteGDALTags = true;  // Not desirable if no geotransform.
                    bWriteGeoTransform = true;
                }
            }
        }
    }

    // Make sure we write grid_mapping if we need to write GDAL tags.
    if( bWriteGDALTags ) bWriteGridMapping = true;

    // bottom-up value: new driver is bottom-up by default.
    // Override with WRITE_BOTTOMUP.
    bBottomUp = CPL_TO_BOOL(
        CSLFetchBoolean(papszCreationOptions, "WRITE_BOTTOMUP", TRUE ));

    if( bDefsOnly )
    {
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
    }

    // Exit if nothing to do.
    if( !bIsProjected && !bWriteLonLat )
        return CE_None;

    // Define dimension names.

    if( bDefsOnly )
    {
        int nVarLonID = -1;
        int nVarLatID = -1;
        int nVarXID = -1;
        int nVarYID = -1;

        m_bAddedProjectionVarsDefs = true;

        // Make sure we are in define mode.
        SetDefineMode(true);

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
                    osGeoTransform += CPLSPrintf("%.16g ", m_adfGeoTransform[i]);
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
                if( bWriteGeoTransform && m_bHasGeoTransform )
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

        // Write CF Projection vars.

        const bool bIsRotatedPole = pszCFProjection != nullptr &&
                                    EQUAL(pszCFProjection, ROTATED_POLE_VAR_NAME);
        if( bIsRotatedPole )
        {
            // Rename dims to rlat/rlon.
            papszDimName.Clear();  // If we add other dims one day, this has to change
            papszDimName.AddString(NCDF_DIMNAME_RLAT);
            papszDimName.AddString(NCDF_DIMNAME_RLON);

            int status = nc_rename_dim(cdfid, nYDimID, NCDF_DIMNAME_RLAT);
            NCDF_ERR(status);
            status = nc_rename_dim(cdfid, nXDimID, NCDF_DIMNAME_RLON);
            NCDF_ERR(status);
        }
        // Rename dimensions if lon/lat.
        else if( !bIsProjected && !bHasGeoloc )
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

        // Write X/Y attributes.
        else /* if( bIsProjected || bHasGeoloc ) */
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

            if( bIsProjected )
            {
                NCDFWriteXYVarsAttributes(this->vcdf, nVarXID, nVarYID, &oSRS);
            }
            else
            {
                CPLAssert(bHasGeoloc);
                vcdf.nc_put_vatt_text(nVarXID, CF_AXIS, CF_SG_X_AXIS);
                vcdf.nc_put_vatt_text(nVarXID, CF_LNG_NAME, "x-coordinate in Cartesian system");
                vcdf.nc_put_vatt_text(nVarXID, CF_UNITS, "m");
                vcdf.nc_put_vatt_text(nVarYID, CF_AXIS, CF_SG_Y_AXIS);
                vcdf.nc_put_vatt_text(nVarYID, CF_LNG_NAME, "y-coordinate in Cartesian system");
                vcdf.nc_put_vatt_text(nVarYID, CF_UNITS, "m");

                pszCFCoordinates = NCDF_LONLAT;
            }
        }

        // Write lat/lon attributes if needed.
        if( bWriteLonLat )
        {
            int *panLatDims = nullptr;
            int *panLonDims = nullptr;
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

            nc_type eLonLatType = NC_NAT;
            if( bIsProjected )
            {
                eLonLatType = NC_FLOAT;
                const char* pszValue =
                    CSLFetchNameValueDef(papszCreationOptions, "TYPE_LONLAT", "FLOAT");
                if( EQUAL(pszValue, "DOUBLE") )
                    eLonLatType = NC_DOUBLE;
            }
            else
            {
                eLonLatType = NC_DOUBLE;
                const char* pszValue =
                    CSLFetchNameValueDef(papszCreationOptions, "TYPE_LONLAT", "DOUBLE");
                if( EQUAL(pszValue, "FLOAT") )
                    eLonLatType = NC_FLOAT;
            }

            // Def vars and attributes.
            {
                const char* pszVarName = bIsRotatedPole ?
                                    NCDF_DIMNAME_RLAT : CF_LATITUDE_VAR_NAME;
                int status = nc_def_var(cdfid, pszVarName, eLonLatType,
                                        nLatDims, panLatDims, &nVarLatID);
                CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d,%d,-,-) got id %d",
                        cdfid, pszVarName, eLonLatType, nLatDims, nVarLatID);
                NCDF_ERR(status);
                DefVarDeflate(nVarLatID, false);  // Don't set chunking.
            }

            {
                const char* pszVarName = bIsRotatedPole ?
                                    NCDF_DIMNAME_RLON : CF_LONGITUDE_VAR_NAME;
                int status = nc_def_var(cdfid, pszVarName, eLonLatType,
                                        nLonDims, panLonDims, &nVarLonID);
                CPLDebug("GDAL_netCDF", "nc_def_var(%d,%s,%d,%d,-,-) got id %d",
                        cdfid, pszVarName, eLonLatType, nLatDims, nVarLonID);
                NCDF_ERR(status);
                DefVarDeflate(nVarLonID, false);  // Don't set chunking.
            }

            if( bIsRotatedPole )
                NCDFWriteRLonRLatVarsAttributes(this->vcdf, nVarLonID, nVarLatID);
            else
                NCDFWriteLonLatVarsAttributes(this->vcdf, nVarLonID, nVarLatID);

            CPLFree(panLatDims);
            CPLFree(panLonDims);
        }
    }

    if( !bDefsOnly )
    {
        m_bAddedProjectionVarsData = true;

        int nVarXID = -1;
        int nVarYID = -1;

        nc_inq_varid(cdfid, CF_PROJ_X_VAR_NAME, &nVarXID);
        nc_inq_varid(cdfid, CF_PROJ_Y_VAR_NAME, &nVarYID);

        int nVarLonID = -1;
        int nVarLatID = -1;

        const bool bIsRotatedPole = pszCFProjection != nullptr &&
                                    EQUAL(pszCFProjection, ROTATED_POLE_VAR_NAME);
        nc_inq_varid(cdfid,
                     bIsRotatedPole ? NCDF_DIMNAME_RLON : CF_LONGITUDE_VAR_NAME,
                     &nVarLonID);
        nc_inq_varid(cdfid,
                     bIsRotatedPole ? NCDF_DIMNAME_RLAT : CF_LATITUDE_VAR_NAME,
                     &nVarLatID);

        // Get projection values.

        double *padLonVal = nullptr;
        double *padLatVal = nullptr;

        if( bIsProjected )
        {
            OGRSpatialReference *poLatLonSRS = nullptr;
            OGRCoordinateTransformation *poTransform = nullptr;

            OGRSpatialReference oSRS2;
            oSRS2.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            oSRS2.importFromWkt(m_pszProjection);

            size_t startX[1];
            size_t countX[1];
            size_t startY[1];
            size_t countY[1];

            CPLDebug("GDAL_netCDF", "Getting (X,Y) values");

            double* padXVal =
                static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));
            double* padYVal =
                static_cast<double *>(CPLMalloc(nRasterYSize * sizeof(double)));

            // Get Y values.
            const double dfY0 = ( !bBottomUp ) ?
                m_adfGeoTransform[3]:
                // Invert latitude values.
                m_adfGeoTransform[3] + (m_adfGeoTransform[5] * nRasterYSize);
            const double dfDY = m_adfGeoTransform[5];

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
            const double dfX0 = m_adfGeoTransform[0];
            const double dfDX = m_adfGeoTransform[1];

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

            if( pfnProgress )
                pfnProgress(0.20, nullptr, pProgressData);

            // Write lon/lat arrays (CF coordinates) if requested.

            // Get OGR transform if GEOLOCATION is not available.
            if( bWriteLonLat && !bHasGeoloc )
            {
                poLatLonSRS = oSRS2.CloneGeogCS();
                if( poLatLonSRS != nullptr )
                {
                    poLatLonSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    poTransform =
                        OGRCreateCoordinateTransformation(&oSRS2, poLatLonSRS);
                }
                // If no OGR transform, then don't write CF lon/lat.
                if( poTransform == nullptr )
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
                            nRasterXSize, padLonVal, padLatVal, nullptr));
                        if( !bOK )
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                    "Unable to Transform (X,Y) to (lon,lat).");
                        }
                    }
                    // Get values from geoloc arrays.
                    else
                    {
                        CPLErr eErr = GDALRasterIO(hBand_Y, GF_Read,
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

                    if( pfnProgress &&
                        (nRasterYSize / 10) >0 && (j % (nRasterYSize / 10) == 0) )
                    {
                        dfProgress += 0.08;
                        pfnProgress(dfProgress, nullptr, pProgressData);
                    }
                }
            }

            if( poLatLonSRS != nullptr ) delete poLatLonSRS;
            if( poTransform != nullptr ) delete poTransform;

            CPLFree(padXVal);
            CPLFree(padYVal);
        }  // Projected

        // If not projected/geographic and has geoloc
        else if( !bIsGeographic && bHasGeoloc )
        {
            // Use https://cfconventions.org/Data/cf-conventions/cf-conventions-1.9/cf-conventions.html#_two_dimensional_latitude_longitude_coordinate_variables

            bool bOK = true;
            double dfProgress = 0.2;

            // Make sure we are in data mode.
            SetDefineMode(false);

            size_t startX[1];
            size_t countX[1];
            size_t startY[1];
            size_t countY[1];
            startX[0] = 0;
            countX[0] = nRasterXSize;

            startY[0] = 0;
            countY[0] = nRasterYSize;

            std::vector<double> adfXVal(nRasterXSize);
            for(int i = 0; i < nRasterXSize; i++)
                adfXVal[i] = i;

            std::vector<double> adfYVal(nRasterYSize);
            for(int i = 0; i < nRasterYSize; i++)
                adfYVal[i] = bBottomUp ? nRasterYSize - 1 - i: i;

            CPLDebug("GDAL_netCDF", "Writing X values");
            int status =
                nc_put_vara_double(cdfid, nVarXID, startX, countX, adfXVal.data());
            NCDF_ERR(status);

            CPLDebug("GDAL_netCDF", "Writing Y values");
            status = nc_put_vara_double(cdfid, nVarYID, startY, countY, adfYVal.data());
            NCDF_ERR(status);

            if( pfnProgress )
                pfnProgress(0.20, nullptr, pProgressData);

            size_t start[] = { 0, 0 };
            size_t count[] = { 1, (size_t)nRasterXSize };
            padLatVal =
                static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));
            padLonVal =
                static_cast<double *>(CPLMalloc(nRasterXSize * sizeof(double)));

            for( int j = 0; j < nRasterYSize && bOK && status == NC_NOERR; j++ )
            {
                start[0] = j;

                CPLErr eErr = GDALRasterIO(hBand_Y, GF_Read,
                                    0, bBottomUp ? nRasterYSize - 1 - j : j,
                                    nRasterXSize, 1,
                                    padLatVal, nRasterXSize, 1,
                                    GDT_Float64, 0, 0);
                if( eErr == CE_None )
                {
                    eErr = GDALRasterIO(hBand_X, GF_Read,
                                        0, bBottomUp ? nRasterYSize - 1 - j : j,
                                        nRasterXSize, 1,
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

                if( pfnProgress &&
                    (nRasterYSize / 10) >0 && (j % (nRasterYSize / 10) == 0) )
                {
                    dfProgress += 0.08;
                    pfnProgress(dfProgress, nullptr, pProgressData);
                }
            }
        }

        // If not projected, assume geographic to catch grids without Datum.
        else if( bWriteLonLat )
        {
            // Get latitude values.
            const double dfY0 = ( !bBottomUp ) ?
                m_adfGeoTransform[3] :
                // Invert latitude values.
                m_adfGeoTransform[3] + (m_adfGeoTransform[5] * nRasterYSize);
            const double dfDY = m_adfGeoTransform[5];

            // Override lat values with the ones in GEOLOCATION/Y_VALUES.
            if( netCDFDataset::GetMetadataItem("Y_VALUES", "GEOLOCATION") != nullptr )
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
                        padLatVal = nullptr;
                    }
                }
            }

            if( padLatVal == nullptr )
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
            const double dfX0 = m_adfGeoTransform[0];
            const double dfDX = m_adfGeoTransform[1];

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

        }  // Not projected.

        CPLFree(padLatVal);
        CPLFree(padLonVal);

        if( pfnProgress )
            pfnProgress(1.00, nullptr, pProgressData);
    }

    if( hDS_X != nullptr )
    {
        GDALClose(hDS_X);
    }
    if( hDS_Y != nullptr )
    {
        GDALClose(hDS_Y);
    }

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
        ((pszCFCoordinates != nullptr && !EQUAL(pszCFCoordinates, "")) ||
         (pszCFProjection != nullptr && !EQUAL(pszCFProjection, ""))) )
    {
        bAddedGridMappingRef = true;

        // Make sure we are in define mode.
        SetDefineMode(true);

        for( int i = 1; i <= nBands; i++ )
        {
            const int nVarId =
                static_cast<netCDFRasterBand *>(GetRasterBand(i))->nZId;

            if( pszCFProjection != nullptr && !EQUAL(pszCFProjection, "") )
            {
                int status = nc_put_att_text(cdfid, nVarId, CF_GRD_MAPPING,
                                            strlen(pszCFProjection), pszCFProjection);
                NCDF_ERR(status);
            }
            if( pszCFCoordinates != nullptr && !EQUAL(pszCFCoordinates, "") )
            {
                int status =
                    nc_put_att_text(cdfid, nVarId, CF_COORDINATES,
                                    strlen(pszCFCoordinates), pszCFCoordinates);
                NCDF_ERR(status);
            }
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
    memcpy(padfTransform, m_adfGeoTransform, sizeof(double) * 6);
    if( m_bHasGeoTransform )
        return CE_None;

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                                rint()                                */
/************************************************************************/

double netCDFDataset::rint( double dfX )
{
    return std::round(dfX);
}

/************************************************************************/
/*                          NCDFReadIsoMetadata()                       */
/************************************************************************/

#ifdef NETCDF_HAS_NC4

static void NCDFReadMetadataAsJson(int cdfid, CPLJSONObject& obj)
{
    int nbAttr = 0;
    NCDF_ERR(nc_inq_varnatts(cdfid, NC_GLOBAL, &nbAttr));

    std::map<std::string, CPLJSONArray> oMapNameToArray;
    for( int l = 0; l < nbAttr; l++ )
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        NCDF_ERR(nc_inq_attname(cdfid, NC_GLOBAL, l, szAttrName));

        char *pszMetaValue = nullptr;
        if( NCDFGetAttr(cdfid, NC_GLOBAL, szAttrName, &pszMetaValue) == CE_None )
        {
            nc_type nAttrType = NC_NAT;
            size_t nAttrLen = 0;

            NCDF_ERR(nc_inq_att(cdfid, NC_GLOBAL, szAttrName, &nAttrType, &nAttrLen));

            std::string osAttrName(szAttrName);
            const auto sharpPos = osAttrName.find('#');
            if( sharpPos == std::string:: npos )
            {
                if( nAttrType == NC_DOUBLE || nAttrType == NC_FLOAT )
                    obj.Add(osAttrName, CPLAtof(pszMetaValue));
                else
                    obj.Add(osAttrName, pszMetaValue);
            }
            else
            {
                osAttrName.resize(sharpPos);
                auto iter = oMapNameToArray.find(osAttrName);
                if( iter == oMapNameToArray.end() )
                {
                    CPLJSONArray array;
                    obj.Add(osAttrName, array);
                    oMapNameToArray[osAttrName] = array;
                    array.Add(pszMetaValue);
                }
                else
                {
                    iter->second.Add(pszMetaValue);
                }
            }
            CPLFree(pszMetaValue);
            pszMetaValue = nullptr;
        }
    }

    int nSubGroups = 0;
    int *panSubGroupIds = nullptr;
    NCDFGetSubGroups(cdfid, &nSubGroups, &panSubGroupIds);
    oMapNameToArray.clear();
    for( int i = 0; i < nSubGroups; i++ )
    {
        CPLJSONObject subObj;
        NCDFReadMetadataAsJson(panSubGroupIds[i], subObj);

        std::string osGroupName;
        osGroupName.resize(NC_MAX_NAME);
        NCDF_ERR(nc_inq_grpname(panSubGroupIds[i], &osGroupName[0]));
        osGroupName.resize(strlen(osGroupName.data()));
        const auto sharpPos = osGroupName.find('#');
        if( sharpPos == std::string:: npos )
        {
            obj.Add(osGroupName, subObj);
        }
        else
        {
            osGroupName.resize(sharpPos);
            auto iter = oMapNameToArray.find(osGroupName);
            if( iter == oMapNameToArray.end() )
            {
                CPLJSONArray array;
                obj.Add(osGroupName, array);
                oMapNameToArray[osGroupName] = array;
                array.Add(subObj);
            }
            else
            {
                iter->second.Add(subObj);
            }
        }
    }
    CPLFree(panSubGroupIds);
}

std::string NCDFReadMetadataAsJson(int cdfid)
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();
    NCDFReadMetadataAsJson(cdfid, oRoot);
    return oDoc.SaveAsString();
}

#endif

/************************************************************************/
/*                        ReadAttributes()                              */
/************************************************************************/
CPLErr netCDFDataset::ReadAttributes( int cdfidIn, int var)

{
    char *pszVarFullName = nullptr;
    ERR_RET(NCDFGetVarFullName(cdfidIn, var, &pszVarFullName));
#ifdef NETCDF_HAS_NC4
    // For metadata in Sentinel 5
    if( STARTS_WITH(pszVarFullName, "/METADATA/") )
    {
        for( const char* key : { "ISO_METADATA", "ESA_METADATA", "EOP_METADATA",
                                 "QA_STATISTICS", "GRANULE_DESCRIPTION", "ALGORITHM_SETTINGS" } )
        {
            if( var == NC_GLOBAL &&
                strcmp(pszVarFullName, CPLSPrintf("/METADATA/%s/NC_GLOBAL", key)) == 0 )
            {
                CPLFree(pszVarFullName);
                CPLStringList aosList;
                aosList.AddString(CPLString(NCDFReadMetadataAsJson(cdfidIn)).replaceAll("\\/", '/'));
                m_oMapDomainToJSon[key] = std::move(aosList);
                return CE_None;
            }
        }
    }
    if( STARTS_WITH(pszVarFullName, "/PRODUCT/SUPPORT_DATA/") )
    {
        CPLFree(pszVarFullName);
        CPLStringList aosList;
        aosList.AddString(CPLString(NCDFReadMetadataAsJson(cdfidIn)).replaceAll("\\/", '/'));
        m_oMapDomainToJSon["SUPPORT_DATA"] = std::move(aosList);
        return CE_None;
    }
#endif

    size_t nMetaNameSize = sizeof(char) * (strlen(pszVarFullName) + 1
                                           + NC_MAX_NAME + 1);
    char *pszMetaName = static_cast<char *>(CPLMalloc(nMetaNameSize));

    int nbAttr = 0;
    NCDF_ERR(nc_inq_varnatts(cdfidIn, var, &nbAttr));

    for( int l = 0; l < nbAttr; l++ )
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        NCDF_ERR(nc_inq_attname(cdfidIn, var, l, szAttrName));
        snprintf(pszMetaName, nMetaNameSize,
                 "%s#%s", pszVarFullName, szAttrName);

        char *pszMetaTemp = nullptr;
        if( NCDFGetAttr(cdfidIn, var, szAttrName, &pszMetaTemp) == CE_None )
        {
            papszMetadata =
                CSLSetNameValue(papszMetadata, pszMetaName, pszMetaTemp);
            CPLFree(pszMetaTemp);
            pszMetaTemp = nullptr;
        }
        else
        {
            CPLDebug("GDAL_netCDF", "invalid metadata %s", pszMetaName);
        }
    }

    CPLFree(pszVarFullName);
    CPLFree(pszMetaName);

    if( var == NC_GLOBAL )
    {
        // Recurse on sub-groups.
        int nSubGroups = 0;
        int *panSubGroupIds = nullptr;
        NCDFGetSubGroups(cdfidIn, &nSubGroups, &panSubGroupIds);
        for( int i = 0; i < nSubGroups; i++ )
        {
            ReadAttributes(panSubGroupIds[i], var);
        }
        CPLFree(panSubGroupIds);
    }

    return CE_None;
}

/************************************************************************/
/*                netCDFDataset::CreateSubDatasetList()                 */
/************************************************************************/
void netCDFDataset::CreateSubDatasetList( int nGroupId )
{
    char szVarStdName[NC_MAX_NAME + 1];
    int *ponDimIds = nullptr;
    nc_type nAttype;
    size_t nAttlen;

    netCDFDataset *poDS = this;

    int nVarCount;
    nc_inq_nvars(nGroupId, &nVarCount);

    for( int nVar = 0; nVar < nVarCount; nVar++ )
    {

        int nDims;
        nc_inq_varndims(nGroupId, nVar, &nDims);

        if( nDims >= 2 )
        {
            ponDimIds = static_cast<int *>(CPLCalloc(nDims, sizeof(int)));
            nc_inq_vardimid(nGroupId, nVar, ponDimIds);

            // Create Sub dataset list.
            CPLString osDim;
            for( int i = 0; i < nDims; i++ )
            {
                size_t nDimLen;
                nc_inq_dimlen(nGroupId, ponDimIds[i], &nDimLen);
                osDim += CPLSPrintf("%dx", (int)nDimLen);
            }
            CPLFree(ponDimIds);

            nc_type nVarType;
            nc_inq_vartype(nGroupId, nVar, &nVarType);
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

            char *pszName = nullptr;
            if( NCDFGetVarFullName(nGroupId, nVar, &pszName) != CE_None )
                continue;

            nSubDatasets++;

            nAttlen = 0;
            nc_inq_att(nGroupId, nVar, CF_STD_NAME, &nAttype, &nAttlen);
            if( nAttlen < sizeof(szVarStdName) &&
                nc_get_att_text(nGroupId, nVar, CF_STD_NAME,
                                szVarStdName) == NC_NOERR )
            {
                szVarStdName[nAttlen] = '\0';
            }
            else
            {
                snprintf(szVarStdName, sizeof(szVarStdName), "%s", pszName);
            }

            char szTemp[NC_MAX_NAME + 1];
            snprintf(szTemp, sizeof(szTemp), "SUBDATASET_%d_NAME", nSubDatasets);

            poDS->papszSubDatasets =
                CSLSetNameValue(poDS->papszSubDatasets, szTemp,
                                CPLSPrintf("NETCDF:\"%s\":%s",
                                           poDS->osFilename.c_str(), pszName));
            CPLFree(pszName);

            snprintf(szTemp, sizeof(szTemp), "SUBDATASET_%d_DESC", nSubDatasets);

            poDS->papszSubDatasets =
                CSLSetNameValue(poDS->papszSubDatasets, szTemp,
                                CPLSPrintf("[%s] %s (%s)", osDim.c_str(),
                                           szVarStdName, pszType));
        }
    }

    // Recurse on sub groups.
    int nSubGroups = 0;
    int *panSubGroupIds = nullptr;
    NCDFGetSubGroups(nGroupId, &nSubGroups, &panSubGroupIds);
    for( int i = 0; i < nSubGroups; i++ )
    {
        CreateSubDatasetList(panSubGroupIds[i]);
    }
    CPLFree(panSubGroupIds);
}

/************************************************************************/
/*                              IdentifyFormat()                      */
/************************************************************************/

NetCDFFormatEnum netCDFDataset::IdentifyFormat( GDALOpenInfo *poOpenInfo,
                                                bool bCheckExt )
{
    // Does this appear to be a netcdf file? If so, which format?
    // http://www.unidata.ucar.edu/software/netcdf/docs/faq.html#fv1_5

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
        return NCDF_FORMAT_UNKNOWN;
    if( poOpenInfo->nHeaderBytes < 4 )
        return NCDF_FORMAT_NONE;
    const char* pszHeader =
                reinterpret_cast<const char*>(poOpenInfo->pabyHeader);

#ifdef ENABLE_NCDUMP
    if( poOpenInfo->fpL != nullptr &&
        STARTS_WITH(pszHeader, "netcdf ") &&
        strstr(pszHeader, "dimensions:") &&
        strstr(pszHeader, "variables:") )
    {
#ifdef NETCDF_HAS_NC4
        if( strstr(pszHeader, "// NC4C") )
            return NCDF_FORMAT_NC4C;
        else if( strstr(pszHeader, "// NC4") )
            return NCDF_FORMAT_NC4;
        else
#endif // NETCDF_HAS_NC4
            return NCDF_FORMAT_NC;
    }
#endif // ENABLE_NCDUMP

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // We don't necessarily want to catch bugs in libnetcdf ...
    if( CPLGetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", nullptr) )
    {
        return NCDF_FORMAT_NONE;
    }
#endif

    constexpr char achHDF5Signature[] = "\211HDF\r\n\032\n";

    if( STARTS_WITH_CI(pszHeader, "CDF\001") )
    {
        // In case the netCDF driver is registered before the GMT driver,
        // avoid opening GMT files.
        if( GDALGetDriverByName("GMT") != nullptr )
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
    else if( STARTS_WITH_CI(pszHeader, "CDF\002") )
    {
        return NCDF_FORMAT_NC2;
    }
    else if( STARTS_WITH_CI(pszHeader, achHDF5Signature) ||
             (poOpenInfo->nHeaderBytes > 512 + 8 &&
              memcmp(pszHeader + 512, achHDF5Signature, 8) == 0) )
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
                  EQUAL(pszExtension, "nc3") || EQUAL(pszExtension, "grd") ||
                  EQUAL(pszExtension, "gmac") ) )
            {
                if( GDALGetDriverByName("HDF5") != nullptr )
                {
                    return NCDF_FORMAT_HDF5;
                }
            }
        }
#endif

        // Check for netcdf-4 support in libnetcdf.
#ifdef NETCDF_HAS_NC4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF5;
#endif
    }
    else if( STARTS_WITH_CI(pszHeader, "\016\003\023\001") )
    {
        // Requires HDF4 support in libnetcdf, but if HF4 is supported by GDAL
        // don't try to open.
        // If user really wants to open with this driver, use NETCDF:file.hdf
        // syntax.

        // Check for HDF4 support in GDAL.
#ifdef HAVE_HDF4
        if( bCheckExt && GDALGetDriverByName("HDF4") != nullptr )
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

    // The HDF5 signature of netCDF 4 files can be at offsets 512, 1024, 2048,
    // etc.
    const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
    if( poOpenInfo->fpL != nullptr &&
        (!bCheckExt ||
         EQUAL(pszExtension, "nc") || EQUAL(pszExtension, "cdf") ||
         EQUAL(pszExtension, "nc4")) )
    {
        vsi_l_offset nOffset = 512;
        for(int i = 0; i < 64; i++)
        {
            GByte abyBuf[8];
            if( VSIFSeekL(poOpenInfo->fpL, nOffset, SEEK_SET) != 0 ||
                VSIFReadL(abyBuf, 1, 8, poOpenInfo->fpL) != 8 )
            {
                break;
            }
            if( memcmp(abyBuf, achHDF5Signature, 8) == 0 )
            {
#ifdef NETCDF_HAS_NC4
                return NCDF_FORMAT_NC4;
#else
                return NCDF_FORMAT_HDF5;
#endif
            }
            nOffset *= 2;
        }
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
               (eMultipleLayerBehavior != SINGLE_LAYER || this->GetLayerCount() == 0 || bSGSupport);
    }
    return FALSE;
}

/************************************************************************/
/*                            GetLayer()                                */
/************************************************************************/

OGRLayer *netCDFDataset::GetLayer(int nIdx)
{
    if( nIdx < 0 || nIdx >= this->GetLayerCount() )
        return nullptr;
    return papoLayers[nIdx].get();
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
        return nullptr;

    CPLString osNetCDFLayerName(pszName);
    const netCDFWriterConfigLayer *poLayerConfig = nullptr;
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

    netCDFDataset *poLayerDataset = nullptr;
    if( eMultipleLayerBehavior == SEPARATE_FILES )
    {
        char **papszDatasetOptions = nullptr;
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
        if( poLayerDataset == nullptr )
            return nullptr;

        nLayerCDFId = poLayerDataset->cdfid;
        NCDFAddGDALHistory(nLayerCDFId, osLayerFilename,
                           bWriteGDALVersion,
                           bWriteGDALHistory,
                           "", "Create",
                           NCDF_CONVENTIONS_CF_V1_6);
    }
#ifdef NETCDF_HAS_NC4
    else if( eMultipleLayerBehavior == SEPARATE_GROUPS )
    {
        SetDefineMode(true);

        nLayerCDFId = -1;
        int status = nc_def_grp(cdfid, osNetCDFLayerName, &nLayerCDFId);
        NCDF_ERR(status);
        if( status != NC_NOERR )
            return nullptr;

        NCDFAddGDALHistory(nLayerCDFId, osFilename,
                           bWriteGDALVersion,
                           bWriteGDALHistory,
                           "", "Create",
                           NCDF_CONVENTIONS_CF_V1_6);
    }
#endif

    // Make a clone to workaround a bug in released MapServer versions
    // that destroys the passed SRS instead of releasing it .
    OGRSpatialReference *poSRS = poSpatialRef;
    if( poSRS != nullptr )
    {
        poSRS = poSRS->Clone();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    std::shared_ptr<netCDFLayer> poLayer(new netCDFLayer(poLayerDataset ? poLayerDataset : this, nLayerCDFId,
                        osNetCDFLayerName, eGType, poSRS));
    if( poSRS != nullptr )
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
        if( poLayerConfig != nullptr )
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
        return nullptr;
    }

    if( poLayerDataset != nullptr )
        apoVectorDatasets.push_back(poLayerDataset);

    papoLayers.push_back(poLayer);
    return poLayer.get();
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

    /* Workaround in some cases a netCDF bug: https://github.com/Unidata/netcdf-c/pull/1442 */
    if( nRecords > 0 && nRecords < 10*1000*1000 / (nElems * nTypeSize) )
    {
        nElems *= nRecords;
        anCount[0] = nRecords;
        nRecords = 1;
    }

    void *pBuffer = VSI_MALLOC2_VERBOSE(nElems, nTypeSize);
    if( pBuffer == nullptr )
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
        nc_inq_unlimdims(cdfid, &nUnlimitedDims, nullptr);
        bool bFound = false;
        if( nUnlimitedDims )
        {
            int *panUnlimitedDimIds =
                static_cast<int *>(CPLMalloc(sizeof(int) * nUnlimitedDims));
            nc_inq_unlimdims(cdfid, nullptr, panUnlimitedDimIds);
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
        nc_inq(cdfid, nullptr, nullptr, nullptr, &nUnlimitedDimId);
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
    NCDF_ERR(status);
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
    CPLString osFilenameForNCCreate(osTmpFilename);
#if defined(WIN32) && !defined(NETCDF_USES_UTF8)
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        char* pszTemp = CPLRecode( osFilenameForNCCreate, CPL_ENC_UTF8, "CP_ACP" );
        osFilenameForNCCreate = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    int status = nc_create(osFilenameForNCCreate, nCreationMode, &new_cdfid);
    NCDF_ERR(status);
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
        nc_inq_grps(cdfid, &nGroupCount, nullptr) == NC_NOERR &&
        nGroupCount > 0 )
    {
        int *panGroupIds =
            static_cast<int *>(CPLMalloc(sizeof(int) * nGroupCount));
        status = nc_inq_grps(cdfid, nullptr, panGroupIds);
        NCDF_ERR(status);
        for(int i = 0; i < nGroupCount; i++)
        {
            char szGroupName[NC_MAX_NAME + 1];
            szGroupName[0] = 0;
            nc_inq_grpname(panGroupIds[i], szGroupName);
            int nNewGrpId = -1;
            status = nc_def_grp(new_cdfid, szGroupName, &nNewGrpId);
            NCDF_ERR(status);
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

        for(int i = 0; i < this->GetLayerCount(); i++)
        {
            auto poLayer = dynamic_cast<netCDFLayer*>(papoLayers[i].get());
            if( poLayer )
            {
                char szGroupName[NC_MAX_NAME + 1];
                szGroupName[0] = 0;
                status = nc_inq_grpname(poLayer->GetCDFID(), szGroupName);
                NCDF_ERR(status);
                oListGrpName.push_back(szGroupName);
            }
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

    CPLString osFilenameForNCOpen(osFilename);
#if defined(WIN32) && !defined(NETCDF_USES_UTF8)
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        char* pszTemp = CPLRecode( osFilenameForNCOpen, CPL_ENC_UTF8, "CP_ACP" );
        osFilenameForNCOpen = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    status = nc_open(osFilenameForNCOpen, NC_WRITE, &cdfid);
    NCDF_ERR(status);
    if( status != NC_NOERR )
        return false;
    bDefineMode = false;

#ifdef NETCDF_HAS_NC4
    if( !oListGrpName.empty() )
    {
        for(int i = 0; i < this->GetLayerCount(); i++)
        {
            auto poLayer = dynamic_cast<netCDFLayer*>(papoLayers[i].get());
            if( poLayer )
            {
                int nNewLayerCDFID = -1;
                status =
                    nc_inq_ncid(cdfid, oListGrpName[i].c_str(), &nNewLayerCDFID);
                NCDF_ERR(status);
                poLayer->SetCDFID(nNewLayerCDFID);
            }
        }
    }
    else
#endif
    {
        for(int i = 0; i < this->GetLayerCount(); i++)
        {
            auto poLayer = dynamic_cast<netCDFLayer*>(papoLayers[i].get());
            if( poLayer )
                poLayer->SetCDFID(cdfid);
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
    const NetCDFFormatEnum eTmpFormat = IdentifyFormat(poOpenInfo,
                                                       /* bCheckExt = */ true);
    if( NCDF_FORMAT_NC == eTmpFormat ||
        NCDF_FORMAT_NC2 == eTmpFormat ||
        NCDF_FORMAT_NC4 == eTmpFormat ||
        NCDF_FORMAT_NC4C == eTmpFormat )
        return TRUE;

    return FALSE;
}

#ifdef ENABLE_NCDUMP

/************************************************************************/
/*                      netCDFDatasetCreateTempFile()                   */
/************************************************************************/

/* Create a netCDF file from a text dump (format of ncdump) */
/* Mostly to easy fuzzing of the driver, while still generating valid */
/* netCDF files. */
/* Note: not all data types are supported ! */
bool netCDFDatasetCreateTempFile( NetCDFFormatEnum eFormat,
                                         const char* pszTmpFilename,
                                         VSILFILE* fpSrc )
{
    CPL_IGNORE_RET_VAL(eFormat);
    int nCreateMode = NC_CLOBBER;
#ifdef NETCDF_HAS_NC4
    if( eFormat == NCDF_FORMAT_NC4 )
        nCreateMode |= NC_NETCDF4;
    else if( eFormat == NCDF_FORMAT_NC4C )
        nCreateMode |= NC_NETCDF4 | NC_CLASSIC_MODEL;
#endif
    int nCdfId = -1;
    int status = nc_create(pszTmpFilename, nCreateMode, &nCdfId);
    if( status != NC_NOERR )
    {
        return false;
    }
    VSIFSeekL( fpSrc, 0, SEEK_SET );
    const char* pszLine;
    constexpr int SECTION_NONE = 0;
    constexpr int SECTION_DIMENSIONS = 1;
    constexpr int SECTION_VARIABLES = 2;
    constexpr int SECTION_DATA = 3;
    int nActiveSection = SECTION_NONE;
    std::map<CPLString, int> oMapDimToId;
    std::map<int, int> oMapDimIdToDimLen;
    std::map<CPLString, int> oMapVarToId;
    std::map<int, std::vector<int> > oMapVarIdToVectorOfDimId;
    std::map<int, int> oMapVarIdToType;
    std::set<CPLString> oSetAttrDefined;
    oMapVarToId[""] = -1;
    size_t nTotalVarSize = 0;
    while( (pszLine = CPLReadLineL(fpSrc)) != nullptr )
    {
        if( STARTS_WITH(pszLine, "dimensions:") &&
            nActiveSection == SECTION_NONE )
        {
            nActiveSection = SECTION_DIMENSIONS;
        }
        else if( STARTS_WITH(pszLine, "variables:") &&
            nActiveSection == SECTION_DIMENSIONS )
        {
            nActiveSection = SECTION_VARIABLES;
        }
        else if( STARTS_WITH(pszLine, "data:") &&
            nActiveSection == SECTION_VARIABLES )
        {
            nActiveSection = SECTION_DATA;
            status = nc_enddef(nCdfId);
            if(status != NC_NOERR )
            {
                CPLDebug("netCDF", "nc_enddef() failed: %s",
                         nc_strerror(status));
            }
        }
        else if( nActiveSection == SECTION_DIMENSIONS )
        {
            char** papszTokens = CSLTokenizeString2(pszLine, " \t=;", 0);
            if( CSLCount(papszTokens) == 2 )
            {
                const char* pszDimName = papszTokens[0];
                bool bValidName = true;
                if( STARTS_WITH(pszDimName, "_nc4_non_coord_") )
                {
                    // This is an internal netcdf prefix. Using it may
                    // cause memory leaks.
                    bValidName = false;
                }
                if( !bValidName )
                {
                    CPLDebug("netCDF",
                             "nc_def_dim(%s) failed: invalid name found",
                             pszDimName);
                    CSLDestroy(papszTokens);
                    continue;
                }

                bool bIsASCII = true;
                for( int i = 0; pszDimName[i] != '\0'; i++ )
                {
                    if( reinterpret_cast<const unsigned char*>(pszDimName)[i] > 127 )
                    {
                        bIsASCII = false;
                        break;
                    }
                }
                if( !bIsASCII )
                {
                    // Workaround https://github.com/Unidata/netcdf-c/pull/450
                    CPLDebug("netCDF",
                             "nc_def_dim(%s) failed: rejected because "
                             "of non-ASCII characters", pszDimName);
                    CSLDestroy(papszTokens);
                    continue;
                }
                int nDimSize = EQUAL(papszTokens[1], "UNLIMITED") ?
                                        NC_UNLIMITED : atoi(papszTokens[1]);
                if( nDimSize >= 1000 )
                    nDimSize = 1000; // to avoid very long processing
                if( nDimSize >= 0 )
                {
                    int nDimId = -1;
                    status = nc_def_dim(nCdfId, pszDimName, nDimSize, &nDimId);
                    if( status != NC_NOERR )
                    {
                        CPLDebug("netCDF", "nc_def_dim(%s, %d) failed: %s",
                                 pszDimName, nDimSize, nc_strerror(status));
                    }
                    else
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("netCDF", "nc_def_dim(%s, %d) (%s) succeeded",
                                 pszDimName, nDimSize, pszLine);
#endif
                        oMapDimToId[pszDimName] = nDimId;
                        oMapDimIdToDimLen[nDimId] = nDimSize;
                    }
                }
            }
            CSLDestroy(papszTokens);
        }
        else if( nActiveSection == SECTION_VARIABLES )
        {
            while( *pszLine == ' ' || *pszLine == '\t' )
                pszLine ++;
            const char* pszColumn = strchr(pszLine, ':');
            const char* pszEqual = strchr(pszLine, '=' );
            if( pszColumn == nullptr )
            {
                char** papszTokens = CSLTokenizeString2(pszLine, " \t=(),;", 0);
                if( CSLCount(papszTokens) >= 2 )
                {
                    const char* pszVarName = papszTokens[1];
                    bool bValidName = true;
                    if( STARTS_WITH(pszVarName, "_nc4_non_coord_") )
                    {
                        // This is an internal netcdf prefix. Using it may
                        // cause memory leaks.
                        bValidName = false;
                    }
                    for( int i = 0; pszVarName[i]; i++ )
                    {
                        if( !((pszVarName[i] >= 'a' && pszVarName[i] <= 'z') ||
                              (pszVarName[i] >= 'A' && pszVarName[i] <= 'Z') ||
                              (pszVarName[i] >= '0' && pszVarName[i] <= '9') ||
                              pszVarName[i] == '_') )
                        {
                            bValidName = false;
                        }
                    }
                    if( !bValidName )
                    {
                        CPLDebug("netCDF",
                                 "nc_def_var(%s) failed: illegal character found",
                                 pszVarName);
                        CSLDestroy(papszTokens);
                        continue;
                    }
                    if( oMapVarToId.find(pszVarName) != oMapVarToId.end() )
                    {
                        CPLDebug("netCDF",
                                 "nc_def_var(%s) failed: already defined",
                                 pszVarName);
                        CSLDestroy(papszTokens);
                        continue;
                    }
                    const char* pszVarType = papszTokens[0];
                    int nc_datatype = NC_BYTE;
                    size_t nDataTypeSize = 1;
                    if( EQUAL(pszVarType, "char") )
                    {
                        nc_datatype = NC_CHAR;
                        nDataTypeSize = 1;
                    }
                    else if( EQUAL(pszVarType, "byte") )
                    {
                        nc_datatype = NC_BYTE;
                        nDataTypeSize = 1;
                    }
                    else if( EQUAL(pszVarType, "short") )
                    {
                        nc_datatype = NC_SHORT;
                        nDataTypeSize = 2;
                    }
                    else if( EQUAL(pszVarType, "int") )
                    {
                        nc_datatype = NC_INT;
                        nDataTypeSize = 4;
                    }
                    else if( EQUAL(pszVarType, "float") )
                    {
                        nc_datatype = NC_FLOAT;
                        nDataTypeSize = 4;
                    }
                    else if( EQUAL(pszVarType, "double") )
                    {
                        nc_datatype = NC_DOUBLE;
                        nDataTypeSize = 8;
                    }
#ifdef NETCDF_HAS_NC4
                    else if( EQUAL(pszVarType, "ubyte") )
                    {
                        nc_datatype = NC_UBYTE;
                        nDataTypeSize = 1;
                    }
                    else if( EQUAL(pszVarType, "ushort") )
                    {
                        nc_datatype = NC_USHORT;
                        nDataTypeSize = 2;
                    }
                    else if( EQUAL(pszVarType, "uint") )
                    {
                        nc_datatype = NC_UINT;
                        nDataTypeSize = 4;
                    }
                    else if( EQUAL(pszVarType, "int64") )
                    {
                        nc_datatype = NC_INT64;
                        nDataTypeSize = 8;
                    }
                    else if( EQUAL(pszVarType, "uint64") )
                    {
                        nc_datatype = NC_UINT64;
                        nDataTypeSize = 8;
                    }
#endif

                    int nDims = CSLCount(papszTokens) - 2;
                    if( nDims >= 32 )
                    {
                        // The number of dimensions in a netCDFv4 file is
                        // limited by #define H5S_MAX_RANK    32
                        // but libnetcdf doesn't check that...
                        CPLDebug("netCDF",
                                 "nc_def_var(%s) failed: too many dimensions",
                                 pszVarName);
                        CSLDestroy(papszTokens);
                        continue;
                    }
                    std::vector<int> aoDimIds;
                    bool bFailed = false;
                    size_t nSize = 1;
                    for( int i = 0; i < nDims; i++ )
                    {
                        const char* pszDimName = papszTokens[2+i];
                        if( oMapDimToId.find(pszDimName) == oMapDimToId.end() )
                        {
                            bFailed = true;
                            break;
                        }
                        const int nDimId = oMapDimToId[pszDimName];
                        aoDimIds.push_back(nDimId);

                        const size_t nDimSize = oMapDimIdToDimLen[nDimId];
                        if( nDimSize != 0 )
                        {
                            if (nSize > std::numeric_limits<size_t>::max() / nDimSize )
                            {
                                bFailed = true;
                                break;
                            }
                            else
                            {
                                nSize *= nDimSize;
                            }
                        }
                    }
                    if( bFailed )
                    {
                        CPLDebug("netCDF",
                                 "nc_def_var(%s) failed: unknown dimension(s)",
                                 pszVarName);
                        CSLDestroy(papszTokens);
                        continue;
                    }
                    if( nSize > 100U * 1024 * 1024 / nDataTypeSize )
                    {
                        CPLDebug("netCDF",
                                 "nc_def_var(%s) failed: too large data",
                                 pszVarName);
                        CSLDestroy(papszTokens);
                        continue;
                    }
                    if( nTotalVarSize > std::numeric_limits<size_t>::max() - nSize ||
                        nTotalVarSize + nSize > 100 * 1024 * 1024 )
                    {
                        CPLDebug("netCDF",
                                 "nc_def_var(%s) failed: too large data",
                                 pszVarName);
                        CSLDestroy(papszTokens);
                        continue;
                    }
                    nTotalVarSize += nSize;

                    int nVarId = -1;
                    status =
                        nc_def_var(nCdfId, pszVarName, nc_datatype, nDims,
                                   (nDims) ? &aoDimIds[0] : nullptr, &nVarId);
                    if( status != NC_NOERR )
                    {
                        CPLDebug("netCDF", "nc_def_var(%s) failed: %s",
                                 pszVarName, nc_strerror(status));
                    }
                    else
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("netCDF", "nc_def_var(%s) (%s) succeeded",
                                 pszVarName, pszLine);
#endif
                        oMapVarToId[pszVarName] = nVarId;
                        oMapVarIdToType[nVarId] = nc_datatype;
                        oMapVarIdToVectorOfDimId[nVarId] = aoDimIds;
                    }
                }
                CSLDestroy(papszTokens);
            }
            else if( pszEqual != nullptr && pszEqual - pszColumn > 0 )
            {
                CPLString osVarName( pszLine, pszColumn - pszLine );
                CPLString osAttrName( pszColumn + 1, pszEqual - pszColumn - 1);
                osAttrName.Trim();
                if( oMapVarToId.find(osVarName) == oMapVarToId.end() )
                {
                    CPLDebug("netCDF",
                             "nc_put_att(%s:%s) failed: "
                             "no corresponding variable",
                             osVarName.c_str(), osAttrName.c_str());
                    continue;
                }
                bool bValidName = true;
                for( size_t i = 0; i < osAttrName.size(); i++ )
                {
                    if( !((osAttrName[i] >= 'a' && osAttrName[i] <= 'z') ||
                            (osAttrName[i] >= 'A' && osAttrName[i] <= 'Z') ||
                            (osAttrName[i] >= '0' && osAttrName[i] <= '9') ||
                            osAttrName[i] == '_') )
                    {
                        bValidName = false;
                    }
                }
                if( !bValidName )
                {
                    CPLDebug("netCDF",
                             "nc_put_att(%s:%s) failed: illegal character found",
                             osVarName.c_str(), osAttrName.c_str());
                    continue;
                }
                if( oSetAttrDefined.find(osVarName + ":" + osAttrName) !=
                        oSetAttrDefined.end() )
                {
                    CPLDebug("netCDF",
                             "nc_put_att(%s:%s) failed: already defined",
                             osVarName.c_str(), osAttrName.c_str());
                    continue;
                }

                const int nVarId = oMapVarToId[osVarName];
                const char* pszValue = pszEqual + 1;
                while( *pszValue == ' ' )
                    pszValue ++;

                status = NC_EBADTYPE;
                if( *pszValue == '"' )
                {
                    // For _FillValue, the attribute type should match
                    // the variable type. Leaks memory with NC4 otherwise
                    if( osAttrName == "_FillValue" )
                    {
                        CPLDebug("netCDF", "nc_put_att_(%s:%s) failed: %s",
                                osVarName.c_str(), osAttrName.c_str(),
                                nc_strerror(status));
                        continue;
                    }

                    // Unquote and unescape string value
                    CPLString osVal(pszValue + 1);
                    while( !osVal.empty() )
                    {
                        if( osVal.back() == ';' ||
                            osVal.back() == ' ')
                        {
                            osVal.resize(osVal.size()-1);
                        }
                        else if( osVal.back() == '"' )
                        {
                            osVal.resize(osVal.size()-1);
                            break;
                        }
                        else
                        {
                            break;
                        }
                    }
                    osVal.replaceAll("\\\"", '"');
                    status = nc_put_att_text(nCdfId, nVarId, osAttrName,
                                             osVal.size(), osVal.c_str());
                }
                else
                {
                    CPLString osVal(pszValue);
                    while( !osVal.empty() )
                    {
                        if( osVal.back() == ';' ||
                            osVal.back() == ' ')
                        {
                            osVal.resize(osVal.size()-1);
                        }
                        else
                        {
                            break;
                        }
                    }
                    int nc_datatype = -1;
                    if( !osVal.empty() && osVal.back() == 'b' )
                    {
                        nc_datatype = NC_BYTE;
                        osVal.resize(osVal.size()-1);
                    }
                    else if( !osVal.empty() && osVal.back() == 's' )
                    {
                        nc_datatype = NC_SHORT;
                        osVal.resize(osVal.size()-1);
                    }
                    if( CPLGetValueType(osVal) == CPL_VALUE_INTEGER )
                    {
                        if( nc_datatype < 0 )
                            nc_datatype = NC_INT;
                    }
                    else if( CPLGetValueType(osVal) == CPL_VALUE_REAL )
                    {
                        nc_datatype = NC_DOUBLE;
                    }
                    else
                    {
                        nc_datatype = -1;
                    }

                    // For _FillValue, check that the attribute type matches
                    // the variable type. Leaks memory with NC4 otherwise
                    if( osAttrName == "_FillValue" )
                    {
                        if( nVarId < 0 ||
                            nc_datatype != oMapVarIdToType[nVarId] )
                        {
                            nc_datatype = -1;
                        }
                    }

                    if( nc_datatype == NC_BYTE )
                    {
                        signed char chVal =
                            static_cast<signed char>(atoi(osVal));
                        status = nc_put_att_schar(
                            nCdfId, nVarId, osAttrName, NC_BYTE, 1, &chVal);
                    }
                    else if( nc_datatype == NC_SHORT )
                    {
                        short nVal =
                            static_cast<short>(atoi(osVal));
                        status = nc_put_att_short(
                            nCdfId, nVarId, osAttrName, NC_SHORT, 1, &nVal);
                    }
                    else if( nc_datatype == NC_INT )
                    {
                        int nVal =
                            static_cast<int>(atoi(osVal));
                        status = nc_put_att_int(
                            nCdfId, nVarId, osAttrName, NC_INT, 1, &nVal);
                    }
                    else if( nc_datatype == NC_DOUBLE )
                    {
                        double dfVal = CPLAtof(osVal);
                        status = nc_put_att_double(
                            nCdfId, nVarId, osAttrName, NC_DOUBLE, 1, &dfVal);
                    }

                }
                if( status != NC_NOERR )
                {
                    CPLDebug("netCDF", "nc_put_att_(%s:%s) failed: %s",
                             osVarName.c_str(), osAttrName.c_str(),
                             nc_strerror(status));
                }
                else
                {
                    oSetAttrDefined.insert(osVarName + ":" + osAttrName);
#ifdef DEBUG_VERBOSE
                    CPLDebug("netCDF", "nc_put_att_(%s:%s) (%s) succeeded",
                             osVarName.c_str(), osAttrName.c_str(),
                             pszLine);
#endif
                }
            }
        }
        else if( nActiveSection == SECTION_DATA )
        {
            while( *pszLine == ' ' || *pszLine == '\t' )
                pszLine ++;
            const char* pszEqual = strchr(pszLine, '=');
            if( pszEqual )
            {
                CPLString osVarName( pszLine, pszEqual - pszLine );
                osVarName.Trim();
                if( oMapVarToId.find(osVarName) == oMapVarToId.end() )
                    continue;
                const int nVarId = oMapVarToId[osVarName];
                CPLString osAccVal(pszEqual + 1);
                osAccVal.Trim();
                while( osAccVal.empty() || osAccVal.back() != ';' )
                {
                    pszLine = CPLReadLineL(fpSrc);
                    if( pszLine == nullptr )
                        break;
                    CPLString osVal(pszLine);
                    osVal.Trim();
                    osAccVal += osVal;
                }
                if( pszLine == nullptr )
                    break;
                osAccVal.resize( osAccVal.size() - 1 );

                const std::vector<int> aoDimIds =
                                    oMapVarIdToVectorOfDimId[nVarId];
                size_t nSize = 1;
                std::vector<size_t> aoStart, aoEdge;
                aoStart.resize( aoDimIds.size() );
                aoEdge.resize( aoDimIds.size() );
                for( size_t i = 0; i < aoDimIds.size(); ++i )
                {
                    const size_t nDimSize = oMapDimIdToDimLen[aoDimIds[i]];
                    if( nDimSize != 0 &&
                        nSize > std::numeric_limits<size_t>::max() / nDimSize )
                    {
                        nSize = 0;
                    }
                    else
                    {
                        nSize *= nDimSize;
                    }
                    aoStart[i] = 0;
                    aoEdge[i] = nDimSize;
                }

                status = NC_EBADTYPE;
                if( nSize == 0 )
                {
                    // Might happen with a unlimited dimension
                }
                else if( oMapVarIdToType[nVarId] == NC_DOUBLE )
                {
                    if( !aoStart.empty() )
                    {
                        char** papszTokens = CSLTokenizeString2(
                                                    osAccVal, " ,;", 0);
                        size_t nTokens = CSLCount(papszTokens);
                        if( nTokens >= nSize )
                        {
                            double* padfVals = static_cast<double*>(
                                VSI_CALLOC_VERBOSE(nSize, sizeof(double)));
                            if( padfVals )
                            {
                                for(size_t i=0; i<nSize; i++)
                                {
                                    padfVals[i] = CPLAtof(papszTokens[i]);
                                }
                                status = nc_put_vara_double(
                                                    nCdfId, nVarId, &aoStart[0],
                                                    &aoEdge[0], padfVals );
                                VSIFree(padfVals);
                            }
                        }
                        CSLDestroy(papszTokens);
                    }
                }
                else if( oMapVarIdToType[nVarId] == NC_BYTE )
                {
                    if( !aoStart.empty() )
                    {
                        char** papszTokens = CSLTokenizeString2(
                                                    osAccVal, " ,;", 0);
                        size_t nTokens = CSLCount(papszTokens);
                        if( nTokens >= nSize )
                        {
                            signed char* panVals = static_cast<signed char*>(
                                VSI_CALLOC_VERBOSE(nSize, sizeof(signed char)));
                            if( panVals )
                            {
                                for(size_t i=0; i<nSize; i++)
                                {
                                    panVals[i] = static_cast<signed char>(
                                                        atoi(papszTokens[i]));
                                }
                                status = nc_put_vara_schar(
                                                nCdfId, nVarId, &aoStart[0],
                                                &aoEdge[0], panVals );
                                VSIFree(panVals);
                            }
                        }
                        CSLDestroy(papszTokens);
                    }
                }
                else if( oMapVarIdToType[nVarId] == NC_CHAR )
                {
                    if( aoStart.size() == 2 )
                    {
                        std::vector<CPLString> aoStrings;
                        bool bInString = false;
                        CPLString osCurString;
                        for( size_t i = 0; i < osAccVal.size() ; )
                        {
                            if( !bInString )
                            {
                                if( osAccVal[i] == '"' )
                                {
                                    bInString = true;
                                    osCurString.clear();
                                }
                                i++;
                            }
                            else if( osAccVal[i] == '\\' &&
                                i + 1 < osAccVal.size() &&
                                     osAccVal[i+1] == '"' )
                            {
                                osCurString += '"';
                                i += 2;
                            }
                            else if( osAccVal[i] == '"' )
                            {
                                aoStrings.push_back(osCurString);
                                osCurString.clear();
                                bInString = false;
                                i ++;
                            }
                            else
                            {
                                osCurString += osAccVal[i];
                                i ++;
                            }
                        }
                        const size_t nRecords = oMapDimIdToDimLen[aoDimIds[0]];
                        const size_t nWidth = oMapDimIdToDimLen[aoDimIds[1]];
                        size_t nIters = aoStrings.size();
                        if( nIters > nRecords )
                            nIters = nRecords;
                        for(size_t i=0; i< nIters; i++)
                        {
                            size_t anIndex[2];
                            anIndex[0] = i;
                            anIndex[1] = 0;
                            size_t anCount[2];
                            anCount[0] = 1;
                            anCount[1] = aoStrings[i].size();
                            if( anCount[1] > nWidth )
                                anCount[1] = nWidth;
                            status = nc_put_vara_text(
                                nCdfId, nVarId, anIndex, anCount,
                                aoStrings[i].c_str());
                            if( status != NC_NOERR )
                                break;
                        }
                    }
                }
                if( status != NC_NOERR )
                {
                    CPLDebug("netCDF", "nc_put_var_(%s) failed: %s",
                             osVarName.c_str(), nc_strerror(status));
                }
            }
        }
    }

    nc_close(nCdfId);
    return true;
}

#endif // ENABLE_NCDUMP

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
        eTmpFormat = IdentifyFormat(poOpenInfo, /* bCheckExt = */ true);
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "identified format %d", eTmpFormat);
#endif
        // Note: not calling Identify() directly, because we want the file type.
        // Only support NCDF_FORMAT* formats.
        if( !(NCDF_FORMAT_NC == eTmpFormat ||
              NCDF_FORMAT_NC2 == eTmpFormat ||
              NCDF_FORMAT_NC4 == eTmpFormat ||
              NCDF_FORMAT_NC4C == eTmpFormat) )
            return nullptr;
    }
    else
    {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        // We don't necessarily want to catch bugs in libnetcdf ...
        if( CPLGetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", nullptr) )
        {
            return nullptr;
        }
#endif
    }

#ifdef NETCDF_HAS_NC4
    if( poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER )
    {
        return OpenMultiDim(poOpenInfo);
    }
#endif

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

#ifdef ENABLE_NCDUMP
    const char* pszHeader =
                reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
    if( poOpenInfo->fpL != nullptr &&
        STARTS_WITH(pszHeader, "netcdf ") &&
        strstr(pszHeader, "dimensions:") &&
        strstr(pszHeader, "variables:") )
    {
        // By default create a temporary file that will be destroyed,
        // unless NETCDF_TMP_FILE is defined. Can be useful to see which
        // netCDF file has been generated from a potential fuzzed input.
        poDS->osFilename = CPLGetConfigOption("NETCDF_TMP_FILE", "");
        if( poDS->osFilename.empty() )
        {
            poDS->bFileToDestroyAtClosing = true;
            poDS->osFilename = CPLGenerateTempFilename("netcdf_tmp");
        }
        if( !netCDFDatasetCreateTempFile( eTmpFormat,
                                          poDS->osFilename,
                                          poOpenInfo->fpL ) )
        {
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                        // with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return nullptr;
        }
        bTreatAsSubdataset = false;
        poDS->eFormat = eTmpFormat;
    }
    else
#endif

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
    {
        char **papszName =
            CSLTokenizeString2(poOpenInfo->pszFilename,
                               ":", CSLT_HONOURSTRINGS|CSLT_PRESERVEESCAPES);

        if( CSLCount(papszName) >= 3 &&
                ((strlen(papszName[1]) == 1 && /* D:\\bla */
                    (papszName[2][0] == '/' || papszName[2][0] == '\\')) ||
                 EQUAL(papszName[1], "http") ||
                 EQUAL(papszName[1], "https") ||
                 EQUAL(papszName[1], "/vsicurl/http") ||
                 EQUAL(papszName[1], "/vsicurl/https")) )
        {
            const int nCountBefore = CSLCount(papszName);
            CPLString osTmp = papszName[1];
            osTmp += ':';
            osTmp += papszName[2];
            CPLFree(papszName[1]);
            CPLFree(papszName[2]);
            papszName[1] = CPLStrdup(osTmp);
            memmove(papszName + 2, papszName + 3, (nCountBefore - 2) * sizeof(char*));
        }

        if( CSLCount(papszName) == 3 )
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
            return nullptr;
        }

        if( !STARTS_WITH(poDS->osFilename, "http://") &&
            !STARTS_WITH(poDS->osFilename, "https://") )
        {
            // Identify Format from real file, with bCheckExt=FALSE.
            GDALOpenInfo *poOpenInfo2 =
                new GDALOpenInfo(poDS->osFilename.c_str(), GA_ReadOnly);
            poDS->eFormat = IdentifyFormat(poOpenInfo2, /* bCheckExt = */ false);
            delete poOpenInfo2;
            if( NCDF_FORMAT_NONE == poDS->eFormat ||
                NCDF_FORMAT_UNKNOWN == poDS->eFormat )
            {
                CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                            // deadlock with GDALDataset own mutex.
                delete poDS;
                CPLAcquireMutex(hNCMutex, 1000.0);
                return nullptr;
            }
        }
    }
    else
    {
        poDS->osFilename = poOpenInfo->pszFilename;
        bTreatAsSubdataset = false;
        poDS->eFormat = eTmpFormat;
    }

    // Try opening the dataset.
#if defined(NCDF_DEBUG) && defined(ENABLE_UFFD)
    CPLDebug("GDAL_netCDF", "calling nc_open_mem(%s)", poDS->osFilename.c_str());
#elseif defined(NCDF_DEBUG) && !defined(ENABLE_UFFD)
    CPLDebug("GDAL_netCDF", "calling nc_open(%s)", poDS->osFilename.c_str());
#endif
    int cdfid = -1;
    const int nMode = ((poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0) ? NC_WRITE : NC_NOWRITE;
    CPLString osFilenameForNCOpen(poDS->osFilename);
#if defined(WIN32) && !defined(NETCDF_USES_UTF8)
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        char* pszTemp = CPLRecode( osFilenameForNCOpen, CPL_ENC_UTF8, "CP_ACP" );
        osFilenameForNCOpen = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    int status2 = -1;

#ifdef ENABLE_UFFD
    cpl_uffd_context * pCtx = nullptr;
#endif

#ifdef HAVE_NETCDF_MEM
    if( STARTS_WITH(osFilenameForNCOpen, "/vsimem/") &&
        poOpenInfo->eAccess == GA_ReadOnly )
    {
        vsi_l_offset nLength = 0;
        poDS->fpVSIMEM = VSIFOpenL(osFilenameForNCOpen, "rb");
        if( poDS->fpVSIMEM )
        {
            // We assume that the file will not be modified. If it is, then
            // pabyBuffer might become invalid.
            GByte* pabyBuffer = VSIGetMemFileBuffer(osFilenameForNCOpen,
                                                    &nLength, false);
            if( pabyBuffer )
            {
                status2 = nc_open_mem(CPLGetFilename(osFilenameForNCOpen),
                                    nMode, static_cast<size_t>(nLength), pabyBuffer,
                                    &cdfid);
            }
        }
    }
    else
#endif
    {
#ifdef ENABLE_UFFD
        bool bVsiFile = !strncmp(osFilenameForNCOpen, "/vsi", strlen("/vsi"));
        bool bReadOnly = (poOpenInfo->eAccess == GA_ReadOnly);
        void * pVma = nullptr;
        uint64_t nVmaSize = 0;

        if ( bVsiFile && bReadOnly && CPLIsUserFaultMappingSupported() )
            pCtx = CPLCreateUserFaultMapping(osFilenameForNCOpen, &pVma, &nVmaSize);
        if (pCtx != nullptr && pVma != nullptr && nVmaSize > 0)
        {
            // netCDF code, at least for netCDF 4.7.0, is confused by filenames like
            // /vsicurl/http[s]://example.com/foo.nc, so just pass the final part
            status2 = nc_open_mem(CPLGetFilename(osFilenameForNCOpen), nMode, static_cast<size_t>(nVmaSize), pVma, &cdfid);
        }
        else
            status2 = nc_open(osFilenameForNCOpen, nMode, &cdfid);
#else
        status2 = nc_open(osFilenameForNCOpen, nMode, &cdfid);
#endif
    }
    if( status2 != NC_NOERR )
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "error opening");
#endif
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
    }
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "got cdfid=%d", cdfid);
#endif

#if defined(ENABLE_NCDUMP) && !defined(WIN32)
    // Try to destroy the temporary file right now on Unix
    if( poDS->bFileToDestroyAtClosing )
    {
        if( VSIUnlink( poDS->osFilename ) == 0 )
        {
            poDS->bFileToDestroyAtClosing = false;
        }
    }
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
        return nullptr;
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
            if( nTmpFormat != NCDF_FORMAT_NC4C &&
                !STARTS_WITH(poDS->osFilename, "http://") &&
                !STARTS_WITH(poDS->osFilename, "https://") )
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

    // Does the request variable exist?
    if( bTreatAsSubdataset )
    {
        int dummy;
        if( NCDFOpenSubDataset(cdfid, osSubdatasetName.c_str(), &dummy, &dummy)
            != CE_None )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s is a netCDF file, but %s is not a variable.",
                     poOpenInfo->pszFilename, osSubdatasetName.c_str());

            nc_close(cdfid);
#ifdef ENABLE_UFFD
            NETCDF_UFFD_UNMAP(pCtx);
#endif
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                        // deadlock with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return nullptr;
        }
    }

    // Figure out whether or not the listed dataset has support for simple geometries (CF-1.8)
    poDS->nCFVersion = nccfdriver::getCFVersion(cdfid);
    if(poDS->nCFVersion >= 1.8)
    {
        poDS->bSGSupport = true;
        poDS->DetectAndFillSGLayers(cdfid);
        poDS->vcdf.enableFullVirtualMode();
    }
    else
    {
         poDS->bSGSupport = false;
    }

    char szConventions[NC_MAX_NAME + 1];
    szConventions[0] = '\0';
    nc_type nAttype = NC_NAT;
    size_t nAttlen = 0;
    nc_inq_att(cdfid, NC_GLOBAL, "Conventions", &nAttype, &nAttlen);
    if( nAttlen >= sizeof(szConventions) ||
        nc_get_att_text(cdfid, NC_GLOBAL, "Conventions",
                                  szConventions) != NC_NOERR )
    {
        CPLDebug("GDAL_netCDF",
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
#ifdef ENABLE_UFFD
    poDS->pCtx = pCtx;
#endif
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->bDefineMode = false;

    poDS->ReadAttributes(cdfid, NC_GLOBAL);

    // Identify coordinate and boundary variables that we should
    // ignore as Raster Bands.
    char **papszIgnoreVars = nullptr;
    NCDFGetCoordAndBoundVarFullNames(cdfid, &papszIgnoreVars);
    // Filter variables to keep only valid 2+D raster bands and vector fields.
    int nRasterVars = 0;
    int nIgnoredVars = 0;
    int nGroupID = -1;
    int nVarID = -1;

    std::map<std::array<int, 3>, std::vector<std::pair<int, int>>> oMap2DDimsToGroupAndVar;
#ifdef NETCDF_HAS_NC4
    if( (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
        STARTS_WITH(CSLFetchNameValueDef(poDS->papszMetadata, "NC_GLOBAL#mission_name", ""), "Sentinel 3") &&
        EQUAL(CSLFetchNameValueDef(poDS->papszMetadata, "NC_GLOBAL#altimeter_sensor_name", ""), "SRAL") &&
        EQUAL(CSLFetchNameValueDef(poDS->papszMetadata, "NC_GLOBAL#radiometer_sensor_name", ""), "MWR") )
    {
        if( poDS->eAccess == GA_Update )
        {
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                        // with GDALDataset own mutex.
            delete poDS;
            return nullptr;
        }
        poDS->ProcessSentinel3_SRAL_MWR();
    }
    else
#endif
    {
        poDS->FilterVars(cdfid, (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0,
                        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0,
                        papszIgnoreVars, &nRasterVars, &nGroupID, &nVarID,
                        &nIgnoredVars, oMap2DDimsToGroupAndVar);
    }
    CSLDestroy(papszIgnoreVars);

    // Case where there is no raster variable
    if( nRasterVars == 0 && !bTreatAsSubdataset )
    {
        poDS->GDALPamDataset::SetMetadata(poDS->papszMetadata);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        poDS->TryLoadXML();
        // If the dataset has been opened in raster mode only, exit
        if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
        {
            delete poDS;
            poDS = nullptr;
        }
        // Otherwise if the dataset is opened in vector mode, that there is
        // no vector layer and we are in read-only, exit too.
        else if( poDS->GetLayerCount() == 0 &&
                 (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                 poOpenInfo->eAccess == GA_ReadOnly )
        {
            delete poDS;
            poDS = nullptr;
        }
        CPLAcquireMutex(hNCMutex, 1000.0);
        return poDS;
    }

    // We have more than one variable with 2 dimensions in the
    // file, then treat this as a subdataset container dataset.
    bool bSeveralVariablesAsBands = false;
    if( (nRasterVars > 1) && !bTreatAsSubdataset )
    {
        if( CPLFetchBool(poOpenInfo->papszOpenOptions, "VARIABLES_AS_BANDS", false)
            && oMap2DDimsToGroupAndVar.size() == 1 )
        {
            std::tie(nGroupID, nVarID) = oMap2DDimsToGroupAndVar.begin()->second.front();
            bSeveralVariablesAsBands = true;
        }
        else
        {
            poDS->CreateSubDatasetList(cdfid);
            poDS->GDALPamDataset::SetMetadata(poDS->papszMetadata);
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                        // with GDALDataset own mutex.
            poDS->TryLoadXML();
            CPLAcquireMutex(hNCMutex, 1000.0);
            return poDS;
        }
    }

    // If we are not treating things as a subdataset, then capture
    // the name of the single available variable as the subdataset.
    if( !bTreatAsSubdataset )
    {
        char *pszVarName = nullptr;
        NCDF_ERR(NCDFGetVarFullName(nGroupID, nVarID, &pszVarName));
        osSubdatasetName = (pszVarName != nullptr ? pszVarName : "");
        CPLFree(pszVarName);
    }

    // We have ignored at least one variable, so we should report them
    // as subdatasets for reference.
    if( nIgnoredVars > 0 && !bTreatAsSubdataset )
    {
        CPLDebug("GDAL_netCDF",
                 "As %d variables were ignored, creating subdataset list "
                 "for reference. Variable #%d [%s] is the main variable",
                 nIgnoredVars, nVarID, osSubdatasetName.c_str());
        poDS->CreateSubDatasetList(cdfid);
    }

    // Open the NETCDF subdataset NETCDF:"filename":subdataset.
    int var = -1;
    NCDFOpenSubDataset(cdfid, osSubdatasetName.c_str(), &nGroupID, &var);
    // Now we can forget the root cdfid and only use the selected group.
    cdfid = nGroupID;
    int nd = 0;
    nc_inq_varndims(cdfid, var, &nd);

    int *paDimIds = static_cast<int *>(CPLCalloc(nd, sizeof(int)));

    // X, Y, Z position in array
    int *panBandDimPos = static_cast<int *>(CPLCalloc(nd, sizeof(int)));

    nc_inq_vardimid(cdfid, var, paDimIds);

    // Check if somebody tried to pass a variable with less than 1D.
    if( nd < 1 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Variable has %d dimension(s) - not supported.", nd);
        CPLFree(paDimIds);
        CPLFree(panBandDimPos);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
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

    if( nd >= 2 && bCheckDims )
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
        if( (NCDFIsVarLongitude(cdfid, -1, szDimName2) ||
             NCDFIsVarProjectionX(cdfid, -1, szDimName2)) &&
            (NCDFIsVarLatitude(cdfid, -1, szDimName1) ||
             NCDFIsVarProjectionY(cdfid, -1, szDimName1)) )
        {
            poDS->bSwitchedXY = true;
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
    if( nd >= 2 )
    {
        poDS->nYDimID = paDimIds[nd - 2];
        nc_inq_dimlen(cdfid, poDS->nYDimID, &ydim);
    }
    else
    {
        poDS->nYDimID = -1;
        ydim = 1;
    }

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
        return nullptr;
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
    if( (nd >= 2 && k != 2) || (nd == 1 && k != 1) )
    {
        CPLFree(paDimIds);
        CPLFree(panBandDimPos);
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
    }

    // Read Metadata for this variable.

    // Should disable as is also done at band level, except driver needs the
    // variables as metadata (e.g. projection).
    poDS->ReadAttributes(cdfid, var);

    // Read Metadata for each dimension.
    int *panDimIds = nullptr;
    NCDFGetVisibleDims(cdfid, &ndims, &panDimIds);
    // With NetCDF-4 groups panDimIds is not always [0..dim_count-1] like
    // in NetCDF-3 because we see only the dimensions of the selected group
    // and its parents.
    // poDS->papszDimName is indexed by dim IDs, so it must contains all IDs
    // [0..max(panDimIds)], but they are not all useful so we fill names
    // of useless dims with empty string.
    if( panDimIds )
    {
        int nMaxDimId = -1;
        for( int i = 0; i < ndims; i++ )
        {
            nMaxDimId = std::max(nMaxDimId, panDimIds[i]);
        }
        for( int j = 0; j <= nMaxDimId; j++ ){
            // Is j dim used?
            int i;
            for( i = 0; i < ndims; i++ )
            {
                if( panDimIds[i] == j )
                    break;
            }
            if( i < ndims )
            {
                // Useful dim.
                char szTemp[NC_MAX_NAME + 1] = {};
                status = nc_inq_dimname(cdfid, panDimIds[i], szTemp);
                if( status != NC_NOERR )
                {
                    CPLFree(paDimIds);
                    CPLFree(panBandDimPos);
                    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                                // deadlock with GDALDataset own
                                                // mutex.
                    delete poDS;
                    CPLAcquireMutex(hNCMutex, 1000.0);
                    return nullptr;
                }
                poDS->papszDimName.AddString(szTemp);
                int nDimGroupId = -1;
                int nDimVarId = -1;
                if( NCDFResolveVar(cdfid, poDS->papszDimName[j],
                                &nDimGroupId, &nDimVarId) == CE_None )
                {
                    poDS->ReadAttributes(nDimGroupId, nDimVarId);
                }
            }
            else
            {
                // Useless dim.
                poDS->papszDimName.AddString("");
            }
        }
        CPLFree(panDimIds);
    }

    // Set projection info.
    if( nd > 1)
    {
        poDS->SetProjectionFromVar(cdfid, var, false);
    }

    // Override bottom-up with GDAL_NETCDF_BOTTOMUP config option.
    const char *pszValue = CPLGetConfigOption("GDAL_NETCDF_BOTTOMUP", nullptr);
    if( pszValue )
    {
        poDS->bBottomUp = CPLTestBool(pszValue);
        CPLDebug("GDAL_netCDF",
                 "set bBottomUp=%d because GDAL_NETCDF_BOTTOMUP=%s",
                 static_cast<int>(poDS->bBottomUp), pszValue);
    }

    // Save non-spatial dimension info.

    int *panBandZLev = nullptr;
    int nDim = (nd >= 2) ? 2 : 1;
    size_t lev_count;
    size_t nTotLevCount = 1;
    nc_type nType = NC_NAT;

    CPLString osExtraDimNames;
    int anExtraDimVarIds[NC_MAX_NAME] = { -1 };
    int anExtraDimGroupIds[NC_MAX_NAME] = { -1 };

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
                panBandDimPos[nDim] = j;  // Save Position of ZDim
                // Save non-spatial dimension names.
                if( nc_inq_dimname(cdfid, paDimIds[j], szDimName)
                    == NC_NOERR )
                {
                    osExtraDimNames += szDimName;
                    if( j < nd-3 )
                    {
                        osExtraDimNames += ",";
                    }

                    int nIdxGroupID = -1;
                    int nIdxVarID = Get1DVariableIndexedByDimension(cdfid,
                                                            paDimIds[j],
                                                            szDimName,
                                                            true,
                                                            &nIdxGroupID);
                    anExtraDimGroupIds[nDim-2] = nIdxGroupID;
                    anExtraDimVarIds[nDim-2] = nIdxVarID;

                    if( nIdxVarID >= 0 )
                    {
                        nc_inq_vartype(nIdxGroupID, nIdxVarID, &nType);
                        char szExtraDimDef[NC_MAX_NAME + 1];
                        snprintf(szExtraDimDef, sizeof(szExtraDimDef), "{%ld,%d}",
                                (long)lev_count, nType);
                        char szTemp[NC_MAX_NAME + 32 + 1];
                        snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_DEF",
                                szDimName);
                        poDS->papszMetadata = CSLSetNameValue(
                            poDS->papszMetadata, szTemp, szExtraDimDef);
                        char *pszTemp = nullptr;
                        if( NCDFGet1DVar(nIdxGroupID, nIdxVarID, &pszTemp) == CE_None )
                        {
                            snprintf(szTemp, sizeof(szTemp), "NETCDF_DIM_%s_VALUES",
                                     szDimName);
                            poDS->papszMetadata = CSLSetNameValue(
                                poDS->papszMetadata, szTemp, pszTemp);
                            CPLFree(pszTemp);
                        }
                    }
                }
                else
                {
                    anExtraDimGroupIds[nDim-2] = -1;
                    anExtraDimVarIds[nDim-2] = -1;
                }

                nDim ++;
            }
        }
        osExtraDimNames += "}";
        poDS->papszMetadata = CSLSetNameValue(
            poDS->papszMetadata, "NETCDF_DIM_EXTRA", osExtraDimNames);
    }

    // Store Metadata.
    poDS->GDALPamDataset::SetMetadata(poDS->papszMetadata);

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
    if( poDS->nRasterXSize == 0 || poDS->nRasterYSize == 0 )
    {
        poDS->nRasterXSize = 0;
        poDS->nRasterYSize = 0;
        nTotLevCount = 0;
        if( poDS->GetLayerCount() == 0 )
        {
            CPLFree(paDimIds);
            CPLFree(panBandDimPos);
            CPLFree(panBandZLev);
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                        // with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return nullptr;
        }
    }
    if( bSeveralVariablesAsBands )
    {
        const auto& listVariables = oMap2DDimsToGroupAndVar.begin()->second;
        for( int iBand = 0; iBand < static_cast<int>(listVariables.size()); ++iBand )
        {
            int bandVarGroupId = listVariables[iBand].first;
            int bandVarId = listVariables[iBand].second;
            netCDFRasterBand *poBand =
                new netCDFRasterBand(netCDFRasterBand::CONSTRUCTOR_OPEN(),
                                     poDS, bandVarGroupId, bandVarId, nDim, 0, nullptr,
                                     panBandDimPos, paDimIds, iBand + 1,
                                     anExtraDimGroupIds, anExtraDimVarIds);
            poDS->SetBand(iBand + 1, poBand);
        }
    }
    else
    {
        for( unsigned int lev = 0; lev < nTotLevCount ; lev++ )
        {
            netCDFRasterBand *poBand =
                new netCDFRasterBand(netCDFRasterBand::CONSTRUCTOR_OPEN(),
                                     poDS, cdfid, var, nDim, lev, panBandZLev,
                                     panBandDimPos, paDimIds, lev + 1,
                                     anExtraDimGroupIds, anExtraDimVarIds);
            poDS->SetBand(lev + 1, poBand);
        }
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

static void CopyMetadata( GDALDataset* poSrcDS,
                          GDALRasterBand* poSrcBand,
                          GDALRasterBand* poDstBand,
                          int nCdfId, int CDFVarID,
                          const char *pszPrefix )
{
    char **papszFieldData = nullptr;

    // Remove the following band meta but set them later from band data.
    const char * const papszIgnoreBand[] = { CF_ADD_OFFSET, CF_SCALE_FACTOR,
                                      "valid_range", "_Unsigned",
                                      _FillValue, "coordinates",
                                      nullptr };
    const char * const papszIgnoreGlobal[] = { "NETCDF_DIM_EXTRA", nullptr };

    char **papszMetadata = nullptr;
    if( poSrcDS )
    {
        papszMetadata = poSrcDS->GetMetadata();
    }
    else if( poSrcBand )
    {
        papszMetadata = poSrcBand->GetMetadata();
    }

    const int nItems = CSLCount(papszMetadata);

    for( int k = 0; k < nItems; k++ )
    {
        const char *pszField = CSLGetField(papszMetadata, k);
        if( papszFieldData ) CSLDestroy(papszFieldData);
        papszFieldData = CSLTokenizeString2(pszField, "=", CSLT_HONOURSTRINGS);
        if( papszFieldData[1] != nullptr )
        {
#ifdef NCDF_DEBUG
            CPLDebug("GDAL_netCDF", "copy metadata [%s]=[%s]",
                     papszFieldData[0], papszFieldData[1]);
#endif

            CPLString osMetaName(papszFieldData[0]);
            CPLString osMetaValue(papszFieldData[1]);

            // Check for items that match pszPrefix if applicable.
            if( pszPrefix != nullptr && !EQUAL(pszPrefix, "") )
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
                else if( strstr(osMetaName, "#") == nullptr )
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
                if( strstr(osMetaName, "#") != nullptr )
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
            if( NCDFPutAttr(nCdfId, CDFVarID, osMetaName,
                            osMetaValue) != CE_None )
                CPLDebug("GDAL_netCDF", "NCDFPutAttr(%d, %d, %s, %s) failed",
                         nCdfId, CDFVarID,
                         osMetaName.c_str(), osMetaValue.c_str());
        }
    }

    if( papszFieldData ) CSLDestroy(papszFieldData);

    // Set add_offset and scale_factor here if present.
    if( poSrcBand && poDstBand )
    {

        int bGotAddOffset = FALSE;
        const double dfAddOffset = poSrcBand->GetOffset(&bGotAddOffset);
        int bGotScale = FALSE;
        const double dfScale = poSrcBand->GetScale(&bGotScale);

        if( bGotAddOffset && dfAddOffset != 0.0 )
            poDstBand->SetOffset(dfAddOffset);
        if( bGotScale && dfScale != 1.0 )
            poDstBand->SetScale(dfScale);
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
                         int nXSize, int nYSize, int nBandsIn,
                         char **papszOptions )
{
    if( !((nXSize == 0 && nYSize == 0 && nBandsIn == 0) ||
          (nXSize > 0 && nYSize > 0 && nBandsIn > 0)) )
    {
        return nullptr;
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

    if( poDS->eMultipleLayerBehavior == SEPARATE_FILES )
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
                return nullptr;
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
            return nullptr;
        }

        return poDS;
    }

    // Create the dataset.
    CPLString osFilenameForNCCreate(pszFilename);
#if defined(WIN32) && !defined(NETCDF_USES_UTF8)
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        char* pszTemp = CPLRecode( osFilenameForNCCreate, CPL_ENC_UTF8, "CP_ACP" );
        osFilenameForNCCreate = pszTemp;
        CPLFree(pszTemp);
    }
#endif

    int status = nc_create(osFilenameForNCCreate, poDS->nCreateMode, &(poDS->cdfid));

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
        return nullptr;
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
                       int nXSize, int nYSize, int nBandsIn,
                       GDALDataType eType,
                       char **papszOptions )
{
    CPLDebug("GDAL_netCDF",
              "\n=====\nnetCDFDataset::Create(%s, ...)",
              pszFilename);

    const char * legacyCreationOp = CSLFetchNameValueDef(papszOptions, "GEOMETRY_ENCODING", "CF_1.8");
    std::string legacyCreationOp_s = std::string(legacyCreationOp);

    // Check legacy creation op FIRST

    bool legacyCreateMode = false;

    if (nXSize != 0 || nYSize != 0 || nBandsIn != 0 )
    {
        legacyCreateMode = true;
    }
    else if (legacyCreationOp_s == "CF_1.8")
    {
        legacyCreateMode = false;
    }

    else if(legacyCreationOp_s == "WKT")
    {
        legacyCreateMode = true;
    }

    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Dataset creation option GEOMETRY_ENCODING=%s is not supported.", legacyCreationOp_s.c_str());
        return nullptr;
    }

    CPLMutexHolderD(&hNCMutex);

    CPLStringList aosOptions(CSLDuplicate(papszOptions));
#ifdef NETCDF_HAS_NC4
    if( aosOptions.FetchNameValue("FORMAT") == nullptr &&
        (eType == GDT_UInt16 || eType == GDT_UInt32 ||
         eType == GDT_UInt64 || eType == GDT_Int64) )
    {
        CPLDebug("netCDF", "Selecting FORMAT=NC4 due to data type");
        aosOptions.SetNameValue("FORMAT", "NC4");
    }
#endif
    netCDFDataset *poDS = netCDFDataset::CreateLL(pszFilename,
                                                  nXSize, nYSize, nBandsIn,
                                                  aosOptions.List());

    if( !poDS )
        return nullptr;

    if (!legacyCreateMode)
    {
        poDS->bSGSupport = true;
        poDS->vcdf.enableFullVirtualMode();
    }

    else
    {
        poDS->bSGSupport = false;
    }

    // Should we write signed or unsigned byte?
    // TODO should this only be done in Create()
    poDS->bSignedData = true;
    const char *pszValue = CSLFetchNameValueDef(papszOptions, "PIXELTYPE", "");
    if( eType == GDT_Byte && !EQUAL(pszValue, "SIGNEDBYTE") )
        poDS->bSignedData = false;

    // Add Conventions, GDAL info and history.
    if( poDS->cdfid >= 0 )
    {
        const char * CF_Vector_Conv = poDS->bSGSupport ? NCDF_CONVENTIONS_CF_V1_8 : NCDF_CONVENTIONS_CF_V1_6;
        poDS->bWriteGDALVersion = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "WRITE_GDAL_VERSION", "YES"));
        poDS->bWriteGDALHistory = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "WRITE_GDAL_HISTORY", "YES"));
        NCDFAddGDALHistory(poDS->cdfid, pszFilename,
                           poDS->bWriteGDALVersion,
                           poDS->bWriteGDALHistory,
                           "", "Create",
                           (nBandsIn == 0) ? CF_Vector_Conv
                                         : GDAL_DEFAULT_NCDF_CONVENTIONS);
    }

    // Define bands.
    for( int iBand = 1; iBand <= nBandsIn; iBand++ )
    {
        poDS->SetBand(
            iBand, new netCDFRasterBand(netCDFRasterBand::CONSTRUCTOR_CREATE(),
                                        poDS, eType, iBand, poDS->bSignedData));
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
                                   nXSize, 1, eDT, 0, 0, nullptr);
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
                                       patScanline, nXSize, 1, eDT, 0, 0, nullptr);
            if( eErr != CE_None )
                CPLDebug("GDAL_netCDF",
                         "NCDFCopyBand(), poDstBand->RasterIO() returned error code %d",
                         eErr);
        }

        if( nYSize > 10 && (iLine % (nYSize / 10) == 1) )
        {
            if( !pfnProgress(1.0 * iLine / nYSize, nullptr, pProgressData) )
            {
                eErr = CE_Failure;
                CPLError(CE_Failure, CPLE_UserInterrupt,
                         "User terminated CreateCopy()");
            }
        }
    }

    CPLFree(patScanline);

    pfnProgress(1.0, nullptr, pProgressData);

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

    if( poSrcDS->GetRootGroup() )
    {
        auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("netCDF"));
        if( poDrv )
        {
            return poDrv->DefaultCreateCopy(pszFilename, poSrcDS, bStrict,
                                     papszOptions, pfnProgress, pProgressData);
        }
    }

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
        return nullptr;
    }

    GDALDataType eDT = GDT_Unknown;
    GDALRasterBand *poSrcBand = nullptr;
    for( int iBand = 1; iBand <= nBands; iBand++ )
    {
        poSrcBand = poSrcDS->GetRasterBand(iBand);
        eDT = poSrcBand->GetRasterDataType();
        if( eDT == GDT_Unknown || GDALDataTypeIsComplex(eDT) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "NetCDF driver does not support source dataset with band "
                     "of complex type.");
            return nullptr;
        }
    }

    if( !pfnProgress(0.0, nullptr, pProgressData) )
        return nullptr;

    // Same as in Create().
    CPLStringList aosOptions(CSLDuplicate(papszOptions));
#ifdef NETCDF_HAS_NC4
    if( aosOptions.FetchNameValue("FORMAT") == nullptr &&
        (eDT == GDT_UInt16 || eDT == GDT_UInt32 ||
         eDT == GDT_UInt64 || eDT == GDT_Int64) )
    {
        CPLDebug("netCDF", "Selecting FORMAT=NC4 due to data type");
        aosOptions.SetNameValue("FORMAT", "NC4");
    }
#endif
    netCDFDataset *poDS = netCDFDataset::CreateLL(pszFilename,
                                                   nXSize, nYSize, nBands,
                                                   aosOptions.List());
    if( !poDS )
        return nullptr;

    // Copy global metadata.
    // Add Conventions, GDAL info and history.
    CopyMetadata(poSrcDS, nullptr, nullptr, poDS->cdfid, NC_GLOBAL, nullptr);
    const bool bWriteGDALVersion = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_GDAL_VERSION", "YES"));
    const bool bWriteGDALHistory = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_GDAL_HISTORY", "YES"));
    NCDFAddGDALHistory(poDS->cdfid, pszFilename,
                       bWriteGDALVersion,
                       bWriteGDALHistory,
                       poSrcDS->GetMetadataItem("NC_GLOBAL#history"),
                       "CreateCopy",
                       poSrcDS->GetMetadataItem("NC_GLOBAL#Conventions"));

    pfnProgress(0.1, nullptr, pProgressData);

    // Check for extra dimensions.
    int nDim = 2;
    char **papszExtraDimNames =
        NCDFTokenizeArray(poSrcDS->GetMetadataItem("NETCDF_DIM_EXTRA", ""));
    char **papszExtraDimValues = nullptr;

    if( papszExtraDimNames != nullptr && CSLCount(papszExtraDimNames) > 0 )
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
            papszExtraDimNames = nullptr;
        }
    }

    int *panDimIds = static_cast<int *>(CPLCalloc(nDim, sizeof(int)));
    int *panBandDimPos = static_cast<int *>(CPLCalloc(nDim, sizeof(int)));

    nc_type nVarType;
    int *panBandZLev = nullptr;
    int *panDimVarIds = nullptr;

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
            const int nDimSize = papszExtraDimValues && papszExtraDimValues[0] ?
                atoi(papszExtraDimValues[0]) : 0;
            // nc_type is an enum in netcdf-3, needs casting.
            nVarType = static_cast<nc_type>(
                papszExtraDimValues && papszExtraDimValues[0] &&
                papszExtraDimValues[1] ? atol(papszExtraDimValues[1]) : 0);
            CSLDestroy(papszExtraDimValues);
            panBandZLev[i] = nDimSize;
            panBandDimPos[i + 2] = i;  // Save Position of ZDim.

            // Define dim.
            int status = nc_def_dim(poDS->cdfid, papszExtraDimNames[i], nDimSize,
                                &(panDimIds[i]));
            NCDF_ERR(status);

            // Define dim var.
            int anDim[1] = {panDimIds[i]};
            status = nc_def_var(poDS->cdfid, papszExtraDimNames[i], nVarType, 1,
                                anDim, &(panDimVarIds[i]));
            NCDF_ERR(status);

            // Add dim metadata, using global var# items.
            snprintf(szTemp, sizeof(szTemp), "%s#", papszExtraDimNames[i]);
            CopyMetadata(poSrcDS, nullptr, nullptr, poDS->cdfid, panDimVarIds[i], szTemp);
        }
    }

    // Copy GeoTransform and Projection.

    // Copy geolocation info.
    char** papszGeolocationInfo = poSrcDS->GetMetadata("GEOLOCATION");
    if( papszGeolocationInfo != nullptr )
        poDS->GDALPamDataset::SetMetadata(papszGeolocationInfo, "GEOLOCATION");

    // Copy geotransform.
    bool bGotGeoTransform = false;
    double adfGeoTransform[6];
    CPLErr eErr = poSrcDS->GetGeoTransform(adfGeoTransform);
    if( eErr == CE_None )
    {
        poDS->SetGeoTransform(adfGeoTransform);
        // Disable AddProjectionVars() from being called.
        bGotGeoTransform = true;
        poDS->m_bHasGeoTransform = false;
    }

    // Copy projection.
    void *pScaledProgress = nullptr;
    if( bGotGeoTransform || (pszWKT && pszWKT[0] != 0) )
    {
        poDS->SetProjection(pszWKT ? pszWKT : "");

        // Now we can call AddProjectionVars() directly.
        poDS->m_bHasGeoTransform = bGotGeoTransform;
        poDS->AddProjectionVars(true, nullptr, nullptr);
        pScaledProgress =
            GDALCreateScaledProgress(0.1, 0.25, pfnProgress, pProgressData);
        poDS->AddProjectionVars(false, GDALScaledProgress, pScaledProgress);
        // Save X,Y dim positions.
        panDimIds[nDim - 1] = poDS->nXDimID;
        panBandDimPos[0] = nDim - 1;
        panDimIds[nDim - 2] = poDS->nYDimID;
        panBandDimPos[1] = nDim - 2;
        GDALDestroyScaledProgress(pScaledProgress);
    }
    else
    {
        poDS->bBottomUp = CPL_TO_BOOL(
            CSLFetchBoolean(papszOptions, "WRITE_BOTTOMUP", TRUE));
        if( papszGeolocationInfo )
        {
            poDS->AddProjectionVars(true, nullptr, nullptr);
            poDS->AddProjectionVars(false, nullptr, nullptr);
        }
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
            if( poSrcDS->GetMetadataItem(szTemp) != nullptr )
            {
                NCDFPut1DVar(poDS->cdfid, panDimVarIds[i],
                             poSrcDS->GetMetadataItem(szTemp));
            }
        }
    }

    pfnProgress(0.25, nullptr, pProgressData);

    // Define Bands.
    netCDFRasterBand *poBand = nullptr;
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
        if( tmpMetadata != nullptr )
        {
            if( nBands > 1 && papszExtraDimNames == nullptr )
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
        if( tmpMetadata != nullptr)
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
                netCDFRasterBand::CONSTRUCTOR_CREATE(),
                poDS, eDT, iBand, bSignedData, szBandName, szLongName, nBandID,
                nDim, iBand - 1, panBandZLev, panBandDimPos, panDimIds);
        else
            poBand = new netCDFRasterBand(netCDFRasterBand::CONSTRUCTOR_CREATE(),
                                          poDS, eDT, iBand, bSignedData,
                                          szBandName, szLongName);

        poDS->SetBand(iBand, poBand);

        // Set nodata value, if any.
        GDALCopyNoDataValue(poBand, poSrcBand);

        // Copy Metadata for band.
        CopyMetadata(nullptr,
                     poSrcDS->GetRasterBand(iBand),
                     poBand,
                     poDS->cdfid, poBand->nZId);

        // If more than 2D pass the first band's netcdf var ID to subsequent
        // bands.
        if( nDim > 2 )
            nBandID = poBand->nZId;
    }

    // Write projection variable to band variable.
    poDS->AddGridMappingRef();

    pfnProgress(0.5, nullptr, pProgressData);

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
        else if( eDT == GDT_UInt16 )
        {
            CPLDebug("GDAL_netCDF", "GUInt16 Band#%d", iBand);
            eErr = NCDFCopyBand<GInt16>(poSrcBand, poDstBand, nXSize, nYSize,
                                        GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_Int16 )
        {
            CPLDebug("GDAL_netCDF", "GInt16 Band#%d", iBand);
            eErr = NCDFCopyBand<GUInt16>(poSrcBand, poDstBand, nXSize, nYSize,
                                         GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_UInt32 )
        {
            CPLDebug("GDAL_netCDF", "GUInt32 Band#%d", iBand);
            eErr = NCDFCopyBand<GUInt32>(poSrcBand, poDstBand, nXSize, nYSize,
                                         GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_Int32 )
        {
            CPLDebug("GDAL_netCDF", "GInt32 Band#%d", iBand);
            eErr = NCDFCopyBand<GInt32>(poSrcBand, poDstBand, nXSize, nYSize,
                                         GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_UInt64 )
        {
            CPLDebug("GDAL_netCDF", "GUInt64 Band#%d", iBand);
            eErr = NCDFCopyBand<std::uint64_t>(poSrcBand, poDstBand, nXSize, nYSize,
                                               GDALScaledProgress, pScaledProgress);
        }
        else if( eDT == GDT_Int64 )
        {
            CPLDebug("GDAL_netCDF", "GInt64 Band#%d", iBand);
            eErr = NCDFCopyBand<std::int64_t>(poSrcBand, poDstBand, nXSize, nYSize,
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
        return nullptr;

    pfnProgress(0.95, nullptr, pProgressData);

    // Re-open dataset so we can return it.
    CPLStringList aosOpenOptions;
    aosOpenOptions.AddString("VARIABLES_AS_BANDS=YES");
    GDALOpenInfo oOpenInfo(pszFilename, GA_Update);
    oOpenInfo.nOpenFlags = GDAL_OF_RASTER | GDAL_OF_UPDATE;
    oOpenInfo.papszOpenOptions = aosOpenOptions.List();
    auto poRetDS = Open(&oOpenInfo);

    // PAM cloning is disabled. See bug #4244.
    // if( poDS )
    //     poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    pfnProgress(1.0, nullptr, pProgressData);

    return poRetDS;
}

// Note: some logic depends on bIsProjected and bIsGeoGraphic.
// May not be known when Create() is called, see AddProjectionVars().
void netCDFDataset::ProcessCreationOptions()
{
    const char *pszConfig =
        CSLFetchNameValue(papszCreationOptions, "CONFIG_FILE");
    if( pszConfig != nullptr )
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
    if( pszValue != nullptr )
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
    if( pszValue != nullptr )
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
    if( pszValue != nullptr )
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
    const char *pszMultipleLayerBehavior =
        CSLFetchNameValueDef(papszCreationOptions, "MULTIPLE_LAYERS", "NO");
    const char *pszGeometryEnc =
        CSLFetchNameValueDef(papszCreationOptions, "GEOMETRY_ENCODING", "CF_1.8");
    if( EQUAL(pszMultipleLayerBehavior, "NO") || EQUAL(pszGeometryEnc, "CF_1.8"))
    {
        eMultipleLayerBehavior = SINGLE_LAYER;
    }
    else if( EQUAL(pszMultipleLayerBehavior, "SEPARATE_FILES") )
    {
        eMultipleLayerBehavior = SEPARATE_FILES;
    }
#ifdef NETCDF_HAS_NC4
    else if( EQUAL(pszMultipleLayerBehavior, "SEPARATE_GROUPS") )
    {
        if( eFormat == NCDF_FORMAT_NC4 )
        {
            eMultipleLayerBehavior = SEPARATE_GROUPS;
        }
        else
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "MULTIPLE_LAYERS=%s is recognised only with FORMAT=NC4",
                     pszMultipleLayerBehavior);
        }
    }
#endif
    else
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "MULTIPLE_LAYERS=%s not recognised",
                 pszMultipleLayerBehavior);
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

            // Config options just for testing purposes
            const char* pszBlockXSize = CPLGetConfigOption("BLOCKXSIZE", nullptr);
            if( pszBlockXSize )
                chunksize[nd - 1] = (size_t)atoi(pszBlockXSize);

            const char* pszBlockYSize = CPLGetConfigOption("BLOCKYSIZE", nullptr);
            if( nd >= 2 && pszBlockYSize )
                chunksize[nd - 2] = (size_t)atoi(pszBlockYSize);

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
    if( hNCMutex != nullptr )
        CPLDestroyMutex(hNCMutex);
    hNCMutex = nullptr;
}

/************************************************************************/
/*                          GDALRegister_netCDF()                       */
/************************************************************************/

void GDALRegister_netCDF()

{
    if( !GDAL_CHECK_VERSION("netCDF driver") )
        return;

    if( GDALGetDriverByName("netCDF") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    // Set the driver details.
    poDriver->SetDescription("netCDF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Network Common Data Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/netcdf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "nc");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
#ifdef NETCDF_HAS_NC4
                              "Byte UInt16 Int16 UInt32 Int32 Int64 UInt64 "
#else
                              "Byte Int16 Int32 "
#endif
                              "Float32 Float64 "
                              "CInt16 CInt32 CFloat32 CFloat64" );
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
"   <Option name='COMPRESS' type='string-select' scope='raster' default='NONE'>"
"     <Value>NONE</Value>"
"     <Value>DEFLATE</Value>"
"   </Option>"
"   <Option name='ZLEVEL' type='int' scope='raster' description='DEFLATE compression level 1-9' default='1'/>"
#endif
"   <Option name='WRITE_BOTTOMUP' type='boolean' scope='raster' default='YES'>"
"   </Option>"
"   <Option name='WRITE_GDAL_TAGS' type='boolean' default='YES'>"
"   </Option>"
"   <Option name='WRITE_LONLAT' type='string-select' scope='raster'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"   </Option>"
"   <Option name='TYPE_LONLAT' type='string-select' scope='raster'>"
"     <Value>float</Value>"
"     <Value>double</Value>"
"   </Option>"
"   <Option name='PIXELTYPE' type='string-select' scope='raster' description='only used in Create()'>"
"       <Value>DEFAULT</Value>"
"       <Value>SIGNEDBYTE</Value>"
"   </Option>"
"   <Option name='CHUNKING' type='boolean' scope='raster' default='YES' description='define chunking when creating netcdf4 file'/>"
"   <Option name='MULTIPLE_LAYERS' type='string-select' scope='vector' description='Behaviour regarding multiple vector layer creation' default='NO'>"
"       <Value>NO</Value>"
"       <Value>SEPARATE_FILES</Value>"
#ifdef NETCDF_HAS_NC4
"       <Value>SEPARATE_GROUPS</Value>"
#endif
"   </Option>"
"   <Option name='GEOMETRY_ENCODING' type='string' scope='vector' default='CF_1.8' description='Specifies the type of geometry encoding when creating a netCDF dataset'>"
"       <Value>WKT</Value>"
"       <Value>CF_1.8</Value>"
"   </Option>"
"   <Option name='CONFIG_FILE' type='string' scope='vector' description='Path to a XML configuration file (or content inlined)'/>"
"   <Option name='WRITE_GDAL_VERSION' type='boolean' default='YES'/>"
"   <Option name='WRITE_GDAL_HISTORY' type='boolean' default='YES'/>"
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
"   <Option name='BUFFER_SIZE' type='int' default='' description='Specifies the soft limit of buffer translation in bytes. Minimum size is 4096. Does not apply to datasets with CF version less than 1.8.'/>"
"   <Option name='GROUPLESS_WRITE_BACK' type='boolean' default='NO' description='Enables or disables array building write back for CF-1.8.'/>"
"   <Option name='PROFILE_DIM_NAME' type='string' description='Name of the profile dimension and variable' default='profile'/>"
"   <Option name='PROFILE_DIM_INIT_SIZE' type='string' description='Initial size of profile dimension (default 100), or UNLIMITED for NC4 files'/>"
"   <Option name='PROFILE_VARIABLES' type='string' description='Comma separated list of field names that must be indexed by the profile dimension'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='HONOUR_VALID_RANGE' type='boolean' scope='raster' "
    "description='Whether to set to nodata pixel values outside of the "
    "validity range' default='YES'/>"
"   <Option name='IGNORE_XY_AXIS_NAME_CHECKS' type='boolean' scope='raster' "
    "description='Whether X/Y dimensions should be always considered as "
    "geospatial axis, even if the lack conventional attributes confirming it.'"
    " default='NO'/>"
"   <Option name='VARIABLES_AS_BANDS' type='boolean' scope='raster' "
    "description='Whether 2D variables that share the same indexing dimensions "
    "should be exposed as several bands of a same dataset instead of several "
    "subdatasets.' default='NO'/>"
"</OpenOptionList>" );


    // Make driver config and capabilities available.
    poDriver->SetMetadataItem("NETCDF_VERSION", nc_inq_libvers());
    poDriver->SetMetadataItem("NETCDF_CONVENTIONS", GDAL_DEFAULT_NCDF_CONVENTIONS);
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
#ifdef HAVE_NETCDF_MEM
    poDriver->SetMetadataItem("NETCDF_HAS_NETCDF_MEM", "YES");
#endif

#ifdef ENABLE_NCDUMP
    poDriver->SetMetadataItem("ENABLE_NCDUMP", "YES");
#endif

#ifdef ENABLE_UFFD
    if( CPLIsUserFaultMappingSupported() )
    {
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    }
#endif

#ifdef NETCDF_HAS_NC4
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );

    poDriver->SetMetadataItem(GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST,
"<MultiDimDatasetCreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='NC4'>"
"     <Value>NC</Value>"
#ifdef NETCDF_HAS_NC2
"     <Value>NC2</Value>"
#endif
"     <Value>NC4</Value>"
"     <Value>NC4C</Value>"
"   </Option>"
"   <Option name='CONVENTIONS' type='string' default='CF-1.6' description='Value of the Conventions attribute'/>"
"</MultiDimDatasetCreationOptionList>" );

    poDriver->SetMetadataItem(GDAL_DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST,
"<MultiDimDimensionCreationOptionList>"
"   <Option name='UNLIMITED' type='boolean' description='Whether the dimension should be unlimited' default='false'/>"
"</MultiDimDimensionCreationOptionList>" );

    poDriver->SetMetadataItem(GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST,
"<MultiDimArrayCreationOptionList>"
"   <Option name='BLOCKSIZE' type='int' description='Block size in pixels'/>"
"   <Option name='COMPRESS' type='string-select' default='NONE'>"
"     <Value>NONE</Value>"
"     <Value>DEFLATE</Value>"
"   </Option>"
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='1'/>"
"   <Option name='NC_TYPE' type='string-select' default='netCDF data type'>"
"     <Value>AUTO</Value>"
"     <Value>NC_BYTE</Value>"
"     <Value>NC_INT64</Value>"
"     <Value>NC_UINT64</Value>"
"   </Option>"
"</MultiDimArrayCreationOptionList>" );

    poDriver->SetMetadataItem(GDAL_DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST,
"<MultiDimAttributeCreationOptionList>"
"   <Option name='NC_TYPE' type='string-select' default='netCDF data type'>"
"     <Value>AUTO</Value>"
"     <Value>NC_BYTE</Value>"
"     <Value>NC_CHAR</Value>"
"     <Value>NC_INT64</Value>"
"     <Value>NC_UINT64</Value>"
"   </Option>"
"</MultiDimAttributeCreationOptionList>" );
#endif

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime");

    // Set pfns and register driver.
    poDriver->pfnOpen = netCDFDataset::Open;
    poDriver->pfnCreateCopy = netCDFDataset::CreateCopy;
    poDriver->pfnCreate = netCDFDataset::Create;
#ifdef NETCDF_HAS_NC4
    poDriver->pfnCreateMultiDimensional = netCDFDataset::CreateMultiDimensional;
#endif
    poDriver->pfnIdentify = netCDFDataset::Identify;
    poDriver->pfnUnloadDriver = NCDFUnloadDriver;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                          New functions                               */
/************************************************************************/

/* Test for GDAL version string >= target */
static bool NCDFIsGDALVersionGTE(const char *pszVersion, int nTarget)
{

    // Valid strings are "GDAL 1.9dev, released 2011/01/18" and "GDAL 1.8.1 ".
    if( pszVersion == nullptr || EQUAL(pszVersion, "") )
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
        if( nVersions[iToken] < 0 )
            nVersions[iToken] = 0;
        else if( nVersions[iToken] > 99 )
            nVersions[iToken] = 99;
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
                                bool bWriteGDALVersion,
                                bool bWriteGDALHistory,
                                const char *pszOldHist,
                                const char *pszFunctionName,
                                const char *pszCFVersion )
{
    if( pszCFVersion == nullptr )
    {
        pszCFVersion = GDAL_DEFAULT_NCDF_CONVENTIONS;
    }
    int status = nc_put_att_text(fpImage, NC_GLOBAL, "Conventions",
                                 strlen(pszCFVersion), pszCFVersion);
    NCDF_ERR(status);

    if( bWriteGDALVersion )
    {
        const char *pszNCDF_GDAL = GDALVersionInfo("--version");
        status = nc_put_att_text(fpImage, NC_GLOBAL, "GDAL",
                                 strlen(pszNCDF_GDAL), pszNCDF_GDAL);
        NCDF_ERR(status);
    }

    if( bWriteGDALHistory )
    {
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
    else if( pszOldHist != nullptr )
    {
        status = nc_put_att_text(fpImage, NC_GLOBAL, "history",
                                 strlen(pszOldHist), pszOldHist);
        NCDF_ERR(status);
    }
}

// Code taken from cdo and libcdi, used for writing the history attribute.

// void cdoDefHistory(int fileID, char *histstring)
static void NCDFAddHistory(int fpImage, const char *pszAddHist,
                           const char *pszOldHist)
{
    // Check pszOldHist - as if there was no previous history, it will be
    // a null pointer - if so set as empty.
    if( nullptr == pszOldHist )
    {
        pszOldHist = "";
    }

    char strtime[32];
    strtime[0] = '\0';

    time_t tp = time(nullptr);
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
    for( int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != nullptr; iMap++ )
    {
#ifdef NCDF_DEBUG
      CPLDebug("GDAL_netCDF", "now at %d, proj=%s",
               iMap, poNetcdfSRS_PT[iMap].WKT_SRS);
#endif
      if( EQUAL(pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS) )
        {
            return poNetcdfSRS_PT[iMap].mappings != nullptr;
        }
    }
    return false;
}

/* Write any needed projection attributes *
 * poPROJCS: ptr to proj crd system
 * pszProjection: name of projection system in GDAL WKT
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

static std::vector<std::pair<std::string, double> >
            NCDFGetProjAttribs( const OGR_SRSNode *poPROJCS,
                                  const char *pszProjection )
{
    const oNetcdfSRS_PP *poMap = nullptr;
    int nMapIndex = -1;

    // Find the appropriate mapping.
    for( int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != nullptr; iMap++ )
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
    for( int iMap = 0; poMap[iMap].WKT_ATT != nullptr; iMap++ )
    {
        oAttMap[poMap[iMap].WKT_ATT] = poMap[iMap].CF_ATT;
    }

    const char *pszParamVal = nullptr;
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

    const std::string *posGDALAtt;
    std::map<std::string, std::string>::iterator oAttIter;
    std::map<std::string, double>::iterator oValIter, oValIter2;

    // Results to write.
    std::vector<std::pair<std::string, double> > oOutList;

    // Lookup mappings and fill output vector.
    if(poMap != poGenericMappings)
    {
        // special case for PS (Polar Stereographic) grid.
        if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
        {
            const double dfLat = oValMap[SRS_PP_LATITUDE_OF_ORIGIN];

            auto oScaleFactorIter = oValMap.find(SRS_PP_SCALE_FACTOR);
            if( oScaleFactorIter != oValMap.end() )
            {
                // Polar Stereographic (variant A)
                const double dfScaleFactor = oScaleFactorIter->second;
                // dfLat should be +/- 90
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_LAT_PROJ_ORIGIN), dfLat));
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_SCALE_FACTOR_ORIGIN), dfScaleFactor));
            }
            else
            {
                // Polar Stereographic (variant B)
                const double dfLatPole = (dfLat > 0) ? 90.0 : -90.0;
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_LAT_PROJ_ORIGIN), dfLatPole));
                oOutList.push_back(std::make_pair(
                    std::string(CF_PP_STD_PARALLEL), dfLat));
            }
        }

        // Specific mapping, loop over mapping values.
        for( oAttIter = oAttMap.begin(); oAttIter != oAttMap.end(); ++oAttIter )
        {
            posGDALAtt = &(oAttIter->first);
            const std::string *posNCDFAtt = &(oAttIter->second);
            oValIter = oValMap.find(*posGDALAtt);

            if( oValIter != oValMap.end() )
            {
                double dfValue = oValIter->second;
                bool bWriteVal = true;

                // special case for LCC-1SP
                //   See comments in netcdfdataset.h for this projection.
                if( EQUAL(SRS_PP_SCALE_FACTOR, posGDALAtt->c_str()) &&
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
            double dfValue = oValIter->second;

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

    return oOutList;
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
    if( nAttrLen == 0 && nAttrType != NC_CHAR )
        return CE_Failure;

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
        CPL_IGNORE_RET_VAL(
            nc_get_att_text(nCdfId, nVarId, pszAttrName, pszAttrValue));
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
            NCDFSafeStrcat(&pszAttrValue, ppszTemp[m] ? ppszTemp[m] : "{NULL}", &nAttrValueSize);
            NCDFSafeStrcat(&pszAttrValue, ",", &nAttrValueSize);
        }
        NCDFSafeStrcat(&pszAttrValue, ppszTemp[m] ? ppszTemp[m] : "{NULL}", &nAttrValueSize);
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
    return NCDFGetAttr1(nCdfId, nVarId, pszAttrName, pdfValue, nullptr, false);
}

/* pszValue is the responsibility of the caller and must be freed */
CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName,
                    char **pszValue )
{
    return NCDFGetAttr1(nCdfId, nVarId, pszAttrName, nullptr, pszValue, true);
}

/* By default write NC_CHAR, but detect for int/float/double and */
/* NC4 string arrays */
static CPLErr NCDFPutAttr( int nCdfId, int nVarId, const char *pszAttrName,
                           const char *pszValue )
{
    int status = 0;
    char *pszTemp = nullptr;

    /* get the attribute values as tokens */
    char **papszValues = NCDFTokenizeArray(pszValue);
    if( papszValues == nullptr )
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

#ifdef DEBUG
    if( EQUAL(pszAttrName, "DEBUG_EMPTY_DOUBLE_ATTR" ) )
    {
        nAttrType = NC_DOUBLE;
        nAttrLen = 0;
    }
#endif

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
        pszVarValue = nullptr;
        break;
    }

    if( pszVarValue != nullptr && nVarLen > 1 && nVarType != NC_CHAR )
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
    if( papszValues == nullptr )
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
                char *pszTemp = nullptr;
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
                char *pszTemp = nullptr;
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
                char *pszTemp = nullptr;
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
                char *pszTemp = nullptr;
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
                char *pszTemp = nullptr;
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
                        char *pszTemp = nullptr;
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
                        char *pszTemp = nullptr;
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
                        char *pszTemp = nullptr;
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

double NCDFGetDefaultNoDataValue( int nCdfId, int nVarId, int nVarType, bool& bGotNoData )

{
    int nNoFill = 0;
    double dfNoData = 0.0;

    switch( nVarType )
    {
    case NC_CHAR:
    case NC_BYTE:
#ifdef NETCDF_HAS_NC4
    case NC_UBYTE:
#endif
        // Don't do default fill-values for bytes, too risky.
        // This function should not be called in those cases.
        CPLAssert(false);
        break;
    case NC_SHORT:
    {
        short nFillVal = 0;
        if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &nFillVal ) == NC_NOERR )
        {
            if( !nNoFill )
            {
                bGotNoData = true;
                dfNoData = nFillVal;
            }
        }
        else
            dfNoData = NC_FILL_SHORT;
        break;
    }
    case NC_INT:
    {
        int nFillVal = 0;
        if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &nFillVal ) == NC_NOERR )
        {
            if( !nNoFill )
            {
                bGotNoData = true;
                dfNoData = nFillVal;
            }
        }
        else
            dfNoData = NC_FILL_INT;
        break;
    }
    case NC_FLOAT:
    {
        float fFillVal = 0;
        if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &fFillVal ) == NC_NOERR )
        {
            if( !nNoFill )
            {
                bGotNoData = true;
                dfNoData = fFillVal;
            }
        }
        else
            dfNoData = NC_FILL_FLOAT;
        break;
    }
    case NC_DOUBLE:
    {
        if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &dfNoData ) == NC_NOERR )
        {
            if( !nNoFill )
            {
                bGotNoData = true;
            }
        }
        else
            dfNoData = NC_FILL_DOUBLE;
        break;
    }
#ifdef NETCDF_HAS_NC4
    case NC_USHORT:
    {
        unsigned short nFillVal = 0;
        if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &nFillVal ) == NC_NOERR )
        {
            if( !nNoFill )
            {
                bGotNoData = true;
                dfNoData = nFillVal;
            }
        }
        else
            dfNoData = NC_FILL_USHORT;
        break;
    }
    case NC_UINT:
    {
        unsigned int nFillVal = 0;
        if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &nFillVal ) == NC_NOERR )
        {
            if( !nNoFill )
            {
                bGotNoData = true;
                dfNoData = nFillVal;
            }
        }
        else
            dfNoData = NC_FILL_UINT;
        break;
    }
#endif
    default:
        dfNoData = 0.0;
        break;
    }

    return dfNoData;
}

#ifdef NETCDF_HAS_NC4

/************************************************************************/
/*                      NCDFGetDefaultNoDataValueAsInt64()              */
/************************************************************************/

static int64_t NCDFGetDefaultNoDataValueAsInt64( int nCdfId, int nVarId, bool& bGotNoData )

{
    int nNoFill = 0;
    long long nFillVal = 0;
    if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &nFillVal ) == NC_NOERR )
    {
        if( !nNoFill )
        {
            bGotNoData = true;
            return static_cast<int64_t>(nFillVal);
        }
    }
    else
        return static_cast<int64_t>(NC_FILL_INT64);
    return 0;
}

/************************************************************************/
/*                     NCDFGetDefaultNoDataValueAsUInt64()              */
/************************************************************************/

static uint64_t NCDFGetDefaultNoDataValueAsUInt64( int nCdfId, int nVarId, bool& bGotNoData )

{
    int nNoFill = 0;
    unsigned long long nFillVal = 0;
    if( nc_inq_var_fill( nCdfId, nVarId, &nNoFill, &nFillVal ) == NC_NOERR )
    {
        if( !nNoFill )
        {
            bGotNoData = true;
            return static_cast<uint64_t>(nFillVal);
        }
    }
    else
        return static_cast<uint64_t>(NC_FILL_UINT64);
    return 0;
}

#endif

static int NCDFDoesVarContainAttribVal( int nCdfId,
                                        const char *const *papszAttribNames,
                                        const char *const *papszAttribValues,
                                        int nVarId,
                                        const char *pszVarName,
                                        bool bStrict = true )
{
    if( nVarId == -1 && pszVarName != nullptr )
        NCDFResolveVar(nCdfId, pszVarName, &nCdfId, &nVarId);

    if( nVarId == -1 ) return -1;

    bool bFound = false;
    for( int i = 0; !bFound && papszAttribNames != nullptr &&
                    papszAttribNames[i] != nullptr; i++ )
    {
        char *pszTemp = nullptr;
        if( NCDFGetAttr(nCdfId, nVarId, papszAttribNames[i], &pszTemp) ==
               CE_None &&
           pszTemp != nullptr )
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
    if( nVarId == -1 && pszVarName != nullptr )
        NCDFResolveVar(nCdfId, pszVarName, &nCdfId, &nVarId);

    if( nVarId == -1 ) return -1;

    bool bFound = false;
    char *pszTemp = nullptr;
    if( NCDFGetAttr(nCdfId, nVarId, papszAttribName, &pszTemp) != CE_None ||
        pszTemp == nullptr )
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
    if( papszName == nullptr || EQUAL(papszName, "") )
        return false;

    for( int i = 0; papszValues && papszValues[i]; ++i )
    {
        if( EQUAL(papszName, papszValues[i]) )
            return true;
    }

    return false;
}

// Test that a variable is longitude/latitude coordinate,
// following CF 4.1 and 4.2.
bool NCDFIsVarLongitude( int nCdfId, int nVarId,
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
        // Check that the units is not 'm' or '1'. See #6759
        char *pszTemp = nullptr;
        if( NCDFGetAttr(nCdfId, nVarId, "units", &pszTemp) == CE_None &&
            pszTemp != nullptr )
        {
            if( EQUAL(pszTemp, "m") || EQUAL(pszTemp, "1") )
                bVal = false;
            CPLFree(pszTemp);
        }
    }

    return CPL_TO_BOOL(bVal);
}

bool NCDFIsVarLatitude( int nCdfId, int nVarId, const char *pszVarName )
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
        // Check that the units is not 'm' or '1'. See #6759
        char *pszTemp = nullptr;
        if( NCDFGetAttr(nCdfId, nVarId, "units", &pszTemp) == CE_None &&
            pszTemp != nullptr )
        {
            if( EQUAL(pszTemp, "m") || EQUAL(pszTemp, "1") )
                bVal = false;
            CPLFree(pszTemp);
        }
    }

    return CPL_TO_BOOL(bVal);
}

bool NCDFIsVarProjectionX( int nCdfId, int nVarId,
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
    else if ( bVal )
    {
        // Check that the units is not '1'
        char *pszTemp = nullptr;
        if( NCDFGetAttr(nCdfId, nVarId, "units", &pszTemp) == CE_None &&
            pszTemp != nullptr )
        {
            if( EQUAL(pszTemp, "1") )
                bVal = false;
            CPLFree(pszTemp);
        }
    }

    return CPL_TO_BOOL(bVal);
}

bool NCDFIsVarProjectionY( int nCdfId, int nVarId,
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
    else if ( bVal )
    {
        // Check that the units is not '1'
        char *pszTemp = nullptr;
        if( NCDFGetAttr(nCdfId, nVarId, "units", &pszTemp) == CE_None &&
            pszTemp != nullptr )
        {
            if( EQUAL(pszTemp, "1") )
                bVal = false;
            CPLFree(pszTemp);
        }
    }

    return CPL_TO_BOOL(bVal);
}

/* test that a variable is a vertical coordinate, following CF 4.3 */
bool NCDFIsVarVerticalCoord( int nCdfId, int nVarId,
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
bool NCDFIsVarTimeCoord( int nCdfId, int nVarId,
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
    if( pszValue == nullptr || EQUAL(pszValue, "") )
        return nullptr;

    char **papszValues = nullptr;
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
        papszValues[1] = nullptr;
    }

    return papszValues;
}

// Open a NetCDF subdataset from full path /group1/group2/.../groupn/var.
// Leading slash is optional.
static CPLErr NCDFOpenSubDataset( int nCdfId, const char *pszSubdatasetName,
                                  int *pnGroupId, int *pnVarId )
{
    *pnGroupId = -1;
    *pnVarId = -1;

    // Open group.
    char *pszGroupFullName = CPLStrdup(CPLGetPath(pszSubdatasetName));
    // Add a leading slash if needed.
    if( pszGroupFullName[0] != '/' )
    {
        char *old = pszGroupFullName;
        pszGroupFullName = CPLStrdup(CPLSPrintf("/%s", pszGroupFullName));
        CPLFree(old);
    }
    // Detect root group.
    if( EQUAL(pszGroupFullName, "/") )
    {
        *pnGroupId = nCdfId;
        CPLFree(pszGroupFullName);
    }
#ifdef NETCDF_HAS_NC4
    else
    {
        int status = nc_inq_grp_full_ncid(nCdfId, pszGroupFullName, pnGroupId);
        CPLFree(pszGroupFullName);
        NCDF_ERR_RET(status);
    }
#endif

    // Open var.
    const char *pszVarName = CPLGetFilename(pszSubdatasetName);
    NCDF_ERR_RET(nc_inq_varid(*pnGroupId, pszVarName, pnVarId));

    return CE_None;
}

// Get all dimensions visible from a given NetCDF (or group) ID and any of
// its parents.
static CPLErr NCDFGetVisibleDims( int nGroupId, int *pnDims,
                                  int **ppanDimIds )
{
    int nDims = 0;
    int *panDimIds = nullptr;
#ifdef NETCDF_HAS_NC4
    NCDF_ERR_RET(nc_inq_dimids(nGroupId, &nDims, nullptr, true));
#else
    NCDF_ERR_RET(nc_inq_ndims(nGroupId, &nDims));
#endif

    panDimIds = static_cast<int *>(CPLMalloc(nDims * sizeof(int)));

#ifdef NETCDF_HAS_NC4
    int status = nc_inq_dimids(nGroupId, nullptr, panDimIds, true);
    if( status != NC_NOERR )
        CPLFree(panDimIds);
    NCDF_ERR_RET(status);
#else
    for( int i = 0; i < nDims; i++ )
    {
        panDimIds[i] = i;
    }
#endif

    *pnDims = nDims;
    *ppanDimIds = panDimIds;

    return CE_None;
}

// Get direct sub-groups IDs of a given NetCDF (or group) ID.
// Consider only direct children, does not get children of children.
static CPLErr NCDFGetSubGroups( int nGroupId, int *pnSubGroups,
                                  int **ppanSubGroupIds )
{
    *pnSubGroups = 0;
    *ppanSubGroupIds = nullptr;

#ifdef NETCDF_HAS_NC4
    int nSubGroups;
    NCDF_ERR_RET(nc_inq_grps(nGroupId, &nSubGroups, nullptr));
    int *panSubGroupIds = static_cast<int *>(CPLMalloc(nSubGroups
                                                       * sizeof(int)));
    NCDF_ERR_RET(nc_inq_grps(nGroupId, nullptr, panSubGroupIds));
    *pnSubGroups = nSubGroups;
    *ppanSubGroupIds = panSubGroupIds;
#endif

    return CE_None;
}

// Get the full name of a given NetCDF (or group) ID
// (e.g. /group1/group2/.../groupn).
// bNC3Compat remove the leading slash for top-level variables for
// backward compatibility (top-level variables are the ones in the root group).
static CPLErr NCDFGetGroupFullName( int nGroupId, char **ppszFullName,
                                    bool bNC3Compat )
{
    *ppszFullName = nullptr;

#ifdef NETCDF_HAS_NC4
    size_t nFullNameLen;
    NCDF_ERR_RET(nc_inq_grpname_len(nGroupId, &nFullNameLen));
    *ppszFullName = static_cast<char *>(CPLMalloc((nFullNameLen + 1)
                                                  * sizeof(char)));
    int status = nc_inq_grpname_full(nGroupId, &nFullNameLen, *ppszFullName);
    if( status != NC_NOERR )
    {
        CPLFree(*ppszFullName);
        *ppszFullName = nullptr;
        NCDF_ERR_RET(status);
    }
#else
    *ppszFullName = CPLStrdup("/");
#endif

    if( bNC3Compat && EQUAL(*ppszFullName, "/") )
        (*ppszFullName)[0] = '\0';

    return CE_None;
}

CPLString NCDFGetGroupFullName(int nGroupId)
{
    char* pszFullname = nullptr;
    NCDFGetGroupFullName(nGroupId, &pszFullname, false);
    CPLString osRet(pszFullname ? pszFullname : "");
    CPLFree(pszFullname);
    return osRet;
}

// Get the full name of a given NetCDF variable ID
// (e.g. /group1/group2/.../groupn/var).
// Handle also NC_GLOBAL as nVarId.
// bNC3Compat remove the leading slash for top-level variables for
// backward compatibility (top-level variables are the ones in the root group).
static CPLErr NCDFGetVarFullName( int nGroupId, int nVarId,
                                  char **ppszFullName, bool bNC3Compat )
{
    *ppszFullName = nullptr;
    char *pszGroupFullName = nullptr;
    ERR_RET(NCDFGetGroupFullName(nGroupId, &pszGroupFullName, bNC3Compat));
    char szVarName[NC_MAX_NAME + 1];
    if( nVarId == NC_GLOBAL )
    {
        strcpy(szVarName, "NC_GLOBAL");
    }
    else
    {
        int status = nc_inq_varname(nGroupId, nVarId, szVarName);
        if( status != NC_NOERR )
        {
            CPLFree(pszGroupFullName);
            NCDF_ERR_RET(status);
        }
    }
    const char *pszSep = "/";
    if( EQUAL(pszGroupFullName, "/") || EQUAL(pszGroupFullName, "") )
        pszSep = "";
    *ppszFullName = CPLStrdup(CPLSPrintf("%s%s%s", pszGroupFullName, pszSep,
                                         szVarName));
    CPLFree(pszGroupFullName);
    return CE_None;
}

// Get the NetCDF root group ID of a given group ID.
static CPLErr NCDFGetRootGroup( int nStartGroupId, int *pnRootGroupId )
{
    *pnRootGroupId = -1;
#ifdef NETCDF_HAS_NC4
    // Recurse on parent group.
    int nParentGroupId;
    int status = nc_inq_grp_parent(nStartGroupId, &nParentGroupId);
    if( status == NC_NOERR )
        return NCDFGetRootGroup(nParentGroupId, pnRootGroupId);
    else if( status != NC_ENOGRP )
        NCDF_ERR_RET(status);
    else // No more parent group.
#endif
    {
        *pnRootGroupId = nStartGroupId;
    }

    return CE_None;
}

// Implementation of NCDFResolveVar/Att.
static CPLErr NCDFResolveElem( int nStartGroupId,
                               const char *pszVar, const char *pszAtt,
                               int *pnGroupId, int *pnId, bool bMandatory )
{
    if( !pszVar && !pszAtt )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "pszVar and pszAtt NCDFResolveElem() args are both null.");
        return CE_Failure;
    }

#ifdef NETCDF_HAS_NC4
    enum {NCRM_PARENT, NCRM_WIDTH_WISE} eNCResolveMode = NCRM_PARENT;
#endif

    std::queue<int> aoQueueGroupIdsToVisit;
    aoQueueGroupIdsToVisit.push(nStartGroupId);

    while( !aoQueueGroupIdsToVisit.empty() )
    {
        // Get the first group of the FIFO queue.
        *pnGroupId = aoQueueGroupIdsToVisit.front();
        aoQueueGroupIdsToVisit.pop();

        // Look if this group contains the searched element.
        int status;
        if( pszVar )
            status = nc_inq_varid(*pnGroupId, pszVar, pnId);
        else // pszAtt != nullptr.
            status = nc_inq_attid(*pnGroupId, NC_GLOBAL, pszAtt, pnId);

        if( status == NC_NOERR )
        {
            return CE_None;
        }
        else if( (pszVar && status != NC_ENOTVAR) ||
                 (pszAtt && status != NC_ENOTATT) )
        {
            NCDF_ERR(status);
        }
#ifdef NETCDF_HAS_NC4
        // Element not found, in NC4 case we must search in other groups
        // following the CF logic.

        // The first resolve mode consists to search on parent groups.
        if( eNCResolveMode == NCRM_PARENT )
        {
            int nParentGroupId = -1;
            int status2 = nc_inq_grp_parent(*pnGroupId, &nParentGroupId);
            if( status2 == NC_NOERR )
                aoQueueGroupIdsToVisit.push(nParentGroupId);
            else if( status2 != NC_ENOGRP )
                NCDF_ERR(status2);
            if( pszVar )
                // When resolving a variable, if there is no more
                // parent group then we switch to width-wise search mode
                // starting from the latest found parent group.
                eNCResolveMode = NCRM_WIDTH_WISE;
        }

        // The second resolve mode is a width-wise search.
        if( eNCResolveMode == NCRM_WIDTH_WISE )
        {
            // Enqueue all direct sub-groups.
            int nSubGroups = 0;
            int *panSubGroupIds = nullptr;
            NCDFGetSubGroups(*pnGroupId, &nSubGroups, &panSubGroupIds);
            for( int i = 0; i < nSubGroups; i++ )
                aoQueueGroupIdsToVisit.push(panSubGroupIds[i]);
            CPLFree(panSubGroupIds);
        }
#endif
    }

    if( bMandatory )
    {
        char *pszStartGroupFullName = nullptr;
        NCDFGetGroupFullName(nStartGroupId, &pszStartGroupFullName);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot resolve mandatory %s %s from group %s",
                 (pszVar ? pszVar : pszAtt),
                 (pszVar ? "variable" : "attribute"),
                 (pszStartGroupFullName ? pszStartGroupFullName : ""));
        CPLFree(pszStartGroupFullName);
    }

    *pnGroupId = -1;
    *pnId = -1;
    return CE_Failure;
}

// Resolve a variable name from a given starting group following the CF logic:
// - if var name is an absolute path then directly open it
// - first search in the starting group and its parent groups
// - then if there is no more parent group we switch to a width-wise search
//   mode starting from the latest found parent group.
// The full CF logic is described here:
// https://github.com/diwg/cf2/blob/master/group/cf2-group.adoc#scope
// If bMandatory then print an error if resolving fails.
// TODO: implement support of relative paths.
// TODO: to follow strictly the CF logic, when searching for a coordinate
//       variable, we must stop the parent search mode once the corresponding
//       dimension is found and start the width-wise search from this group.
// TODO: to follow strictly the CF logic, when searching in width-wise mode
//       we should skip every groups already visited during the parent
//       search mode (but revisiting them should have no impact so we could
//       let as it is if it is simpler...)
// TODO: CF specifies that the width-wise search order is "left-to-right" so
//       maybe we must sort sibling groups alphabetically? but maybe not
//       necessary if nc_inq_grps() already sort them?
CPLErr NCDFResolveVar( int nStartGroupId, const char *pszVar,
                              int *pnGroupId, int *pnVarId,
                              bool bMandatory )
{
    *pnGroupId = -1;
    *pnVarId = -1;
    int nGroupId = nStartGroupId, nVarId;
    if( pszVar[0] == '/' )
    {
        // This is an absolute path: we can open the var directly.
        int nRootGroupId;
        ERR_RET(NCDFGetRootGroup(nStartGroupId, &nRootGroupId));
        ERR_RET(NCDFOpenSubDataset(nRootGroupId, pszVar, &nGroupId, &nVarId));
    }
    else
    {
        // We have to search the variable following the CF logic.
        ERR_RET(NCDFResolveElem(nStartGroupId, pszVar, nullptr,
                                &nGroupId, &nVarId, bMandatory));
    }
    *pnGroupId = nGroupId;
    *pnVarId = nVarId;
    return CE_None;
}

// Like NCDFResolveVar but returns directly the var full name.
static CPLErr NCDFResolveVarFullName( int nStartGroupId, const char *pszVar,
                                      char **ppszFullName,
                                      bool bMandatory )
{
    *ppszFullName = nullptr;
    int nGroupId, nVarId;
    ERR_RET(NCDFResolveVar(nStartGroupId, pszVar, &nGroupId, &nVarId,
                           bMandatory));
    return NCDFGetVarFullName(nGroupId, nVarId, ppszFullName);
}

// Like NCDFResolveVar but resolves an attribute instead a variable and
// returns its integer value.
// Only GLOBAL attributes are supported for the moment.
static CPLErr NCDFResolveAttInt( int nStartGroupId, int nStartVarId,
                                 const char *pszAtt, int *pnAtt,
                                 bool bMandatory )
{
    int nGroupId = nStartGroupId, nAttId = nStartVarId;
    ERR_RET(NCDFResolveElem(nStartGroupId, nullptr, pszAtt,
                            &nGroupId, &nAttId, bMandatory));
    NCDF_ERR_RET(nc_get_att_int(nGroupId, NC_GLOBAL, pszAtt, pnAtt));
    return CE_None;
}

// Filter variables to keep only valid 2+D raster bands and vector fields in
// a given a NetCDF (or group) ID and its sub-groups.
// Coordinate or boundary variables are ignored.
// It also creates corresponding vector layers.
CPLErr netCDFDataset::FilterVars( int nCdfId, bool bKeepRasters,
                                  bool bKeepVectors, char **papszIgnoreVars,
                                  int *pnRasterVars,
                                  int *pnGroupId, int *pnVarId,
                                  int *pnIgnoredVars,
                                  std::map<std::array<int, 3>, std::vector<std::pair<int, int>>>& oMap2DDimsToGroupAndVar)
{
    int nVars = 0;
    int nRasterVars = 0;
    NCDF_ERR(nc_inq(nCdfId, nullptr, &nVars, nullptr, nullptr));

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

    for( int v = 0; v < nVars; v++ )
    {
        int nVarDims;
        NCDF_ERR_RET(nc_inq_varndims(nCdfId, v, &nVarDims));
        // Should we ignore this variable?
        char szTemp[NC_MAX_NAME + 1];
        szTemp[0] = '\0';
        NCDF_ERR_RET(nc_inq_varname(nCdfId, v, szTemp));

        if( strstr(szTemp, "_node_coordinates") || strstr(szTemp, "_node_count") )
        {
            // Ignore CF-1.8 Simple Geometries helper variables
            continue;
        }

        if( nVarDims == 1 && (NCDFIsVarLongitude(nCdfId, -1, szTemp) ||
                              NCDFIsVarProjectionX(nCdfId, -1, szTemp)) )
        {
            nVarXId = v;
        }
        else if( nVarDims == 1 && (NCDFIsVarLatitude(nCdfId, -1, szTemp) ||
                                   NCDFIsVarProjectionY(nCdfId, -1, szTemp)) )
        {
            nVarYId = v;
        }
        else if( nVarDims == 1 && NCDFIsVarVerticalCoord(nCdfId, -1, szTemp) )
        {
            nVarZId = v;
        }
        else
        {
            char *pszVarFullName = nullptr;
            CPLErr eErr = NCDFGetVarFullName(nCdfId, v, &pszVarFullName);
            if( eErr != CE_None ) {
                CPLFree(pszVarFullName);
                continue;
            }
            bool bIgnoreVar = (CSLFindString(papszIgnoreVars, pszVarFullName)
                               != -1);
            CPLFree(pszVarFullName);
            if( bIgnoreVar )
            {
                (*pnIgnoredVars)++;
                CPLDebug("GDAL_netCDF", "variable #%d [%s] was ignored", v,
                        szTemp);
            }
            // Only accept 2+D vars.
            else if( nVarDims >= 2 )
            {
                bool bRasterCandidate = true;
                // Identify variables that might be vector variables
                if( nVarDims == 2 )
                {
                    int anDimIds[2] = { -1, -1 };
                    nc_inq_vardimid(nCdfId, v, anDimIds);

                    nc_type vartype = NC_NAT;
                    nc_inq_vartype(nCdfId, v, &vartype);

                    char szDimNameFirst[NC_MAX_NAME + 1];
                    char szDimNameSecond[NC_MAX_NAME + 1];
                    szDimNameFirst[0] = '\0';
                    szDimNameSecond[0] = '\0';
                    if( vartype == NC_CHAR &&
                        nc_inq_dimname(nCdfId, anDimIds[0], szDimNameFirst) ==
                            NC_NOERR &&
                        nc_inq_dimname(nCdfId, anDimIds[1], szDimNameSecond) ==
                            NC_NOERR &&
                        !NCDFIsVarLongitude(nCdfId, -1, szDimNameSecond) &&
                        !NCDFIsVarProjectionX(nCdfId, -1, szDimNameSecond) &&
                        !NCDFIsVarLatitude(nCdfId, -1, szDimNameFirst) &&
                        !NCDFIsVarProjectionY(nCdfId, -1, szDimNameFirst) )
                    {
                        anPotentialVectorVarID.push_back(v);
                        oMapDimIdToCount[anDimIds[0]]++;
                        if( strstr( szDimNameSecond, "_max_width") )
                        {
                            bRasterCandidate = false;
                        }
                        else
                        {
                            std::array<int, 3> oKey{anDimIds[0], anDimIds[1], vartype};
                            oMap2DDimsToGroupAndVar[oKey].emplace_back(
                                std::pair<int, int>(nCdfId, v));
                        }
                    }
                    else
                    {
                        std::array<int, 3> oKey{anDimIds[0], anDimIds[1], vartype};
                        oMap2DDimsToGroupAndVar[oKey].emplace_back(
                            std::pair<int, int>(nCdfId, v));
                        bIsVectorOnly = false;
                    }
                }
                else
                {
                    bIsVectorOnly = false;
                }
                if( bKeepRasters && bRasterCandidate )
                {
                    *pnGroupId = nCdfId;
                    *pnVarId = v;
                    nRasterVars++;
                }
            }
            else if( nVarDims == 1 )
            {
                nc_type atttype = NC_NAT;
                size_t attlen = 0;
                if( nc_inq_att(nCdfId, v, "instance_dimension", &atttype,
                               &attlen) == NC_NOERR &&
                    atttype == NC_CHAR && attlen < NC_MAX_NAME )
                {
                    char szInstanceDimension[NC_MAX_NAME + 1];
                    if( nc_get_att_text(nCdfId, v, "instance_dimension",
                                        szInstanceDimension) == NC_NOERR )
                    {
                        szInstanceDimension[attlen] = 0;
                        int status = nc_inq_dimid(nCdfId, szInstanceDimension,
                                                  &nProfileDimId);
                        if( status == NC_NOERR )
                            nParentIndexVarID = v;
                        else
                            nProfileDimId = -1;
                        if( status == NC_EBADDIM )
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Attribute instance_dimension='%s' refers "
                                     "to a non existing dimension",
                                     szInstanceDimension);
                        else
                            NCDF_ERR(status);
                    }
                }
                if( v != nParentIndexVarID )
                {
                    anPotentialVectorVarID.push_back(v);
                    int nDimId = -1;
                    nc_inq_vardimid(nCdfId, v, &nDimId);
                    oMapDimIdToCount[nDimId]++;
                }
            }
        }
    }

    // If we are opened in raster-only mode and that there are only 1D or 2D
    // variables and that the 2D variables have no X/Y dim, and all
    // variables refer to the same main dimension (or 2 dimensions for
    // featureType=profile), then it is a pure vector dataset
    CPLString osFeatureType(CSLFetchNameValueDef(papszMetadata,
                                                 "NC_GLOBAL#featureType", ""));
    if( bKeepRasters && !bKeepVectors &&
        bIsVectorOnly && nRasterVars > 0 &&
        !anPotentialVectorVarID.empty() &&
        (oMapDimIdToCount.size() == 1 || (EQUAL(osFeatureType, "profile") &&
                                          oMapDimIdToCount.size() == 2 &&
                                          nProfileDimId >= 0)) )
    {
        anPotentialVectorVarID.resize(0);
    }
    else
    {
        *pnRasterVars += nRasterVars;
    }

    if( !anPotentialVectorVarID.empty() && bKeepVectors && nccfdriver::getCFVersion(nCdfId) <= 1.6)
    {
        // Take the dimension that is referenced the most times.
        if( !(oMapDimIdToCount.size() == 1 ||
            (EQUAL(osFeatureType, "profile") &&
             oMapDimIdToCount.size() == 2 && nProfileDimId >= 0)) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "The dataset has several variables that could be "
                    "identified as vector fields, but not all share the same "
                    "primary dimension. Consequently they will be ignored.");
        }
        else
        {
            CreateGrpVectorLayers(nCdfId, osFeatureType,
                                  anPotentialVectorVarID, oMapDimIdToCount,
                                  nVarXId, nVarYId, nVarZId, nProfileDimId,
                                  nParentIndexVarID, bKeepRasters);
        }
    }

    // Recurse on sub-groups.
    int nSubGroups = 0;
    int *panSubGroupIds = nullptr;
    NCDFGetSubGroups(nCdfId, &nSubGroups, &panSubGroupIds);
    for( int i = 0; i < nSubGroups; i++ )
    {
        FilterVars(panSubGroupIds[i], bKeepRasters, bKeepVectors,
                   papszIgnoreVars, pnRasterVars, pnGroupId, pnVarId,
                   pnIgnoredVars, oMap2DDimsToGroupAndVar);
    }
    CPLFree(panSubGroupIds);

    return CE_None;
}

// Create vector layers from given potentially identified vector variables
// resulting from the scanning of a NetCDF (or group) ID.
CPLErr netCDFDataset::CreateGrpVectorLayers( int nCdfId,
                                             CPLString osFeatureType,
                                             std::vector<int> anPotentialVectorVarID,
                                             std::map<int, int> oMapDimIdToCount,
                                             int nVarXId, int nVarYId, int nVarZId,
                                             int nProfileDimId,
                                             int nParentIndexVarID,
                                             bool bKeepRasters )
{
    char *pszGroupName = nullptr;
    NCDFGetGroupFullName(nCdfId, &pszGroupName);
    if( pszGroupName == nullptr || pszGroupName[0] == '\0' )
    {
        CPLFree(pszGroupName);
        pszGroupName = CPLStrdup(CPLGetBasename(osFilename));
    }
    OGRwkbGeometryType eGType = wkbUnknown;
    CPLString osLayerName = CSLFetchNameValueDef(papszMetadata,
                                                 "NC_GLOBAL#ogr_layer_name",
                                                 pszGroupName);
    CPLFree(pszGroupName);
    papszMetadata = CSLSetNameValue(papszMetadata, "NC_GLOBAL#ogr_layer_name",
                                    nullptr);

    if( EQUAL(osFeatureType, "point") || EQUAL(osFeatureType, "profile") )
    {
        papszMetadata = CSLSetNameValue(papszMetadata, "NC_GLOBAL#featureType",
                                        nullptr);
        eGType = wkbPoint;
    }

    const char *pszLayerType = CSLFetchNameValue(papszMetadata,
                                                 "NC_GLOBAL#ogr_layer_type");
    if( pszLayerType != nullptr )
    {
        eGType = OGRFromOGCGeomType(pszLayerType);
        papszMetadata = CSLSetNameValue(papszMetadata,
                                        "NC_GLOBAL#ogr_layer_type", nullptr);
    }

    CPLString osGeometryField = CSLFetchNameValueDef(
        papszMetadata, "NC_GLOBAL#ogr_geometry_field", "");
    papszMetadata = CSLSetNameValue(
        papszMetadata, "NC_GLOBAL#ogr_geometry_field", nullptr);

    int nFirstVarId = -1;
    int nVectorDim = oMapDimIdToCount.rbegin()->first;
    if( EQUAL(osFeatureType, "profile") && oMapDimIdToCount.size() == 2 )
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
        nc_inq_vardimid(nCdfId, anPotentialVectorVarID[j], anDimIds);
        if( nVectorDim == anDimIds[0] )
        {
            nFirstVarId = anPotentialVectorVarID[j];
            break;
        }
    }

    // In case where coordinates are explicitly specified for one of the
    // field/variable, use them in priority over the ones that might have been
    // identified above.
    char *pszCoordinates = nullptr;
    if( NCDFGetAttr(nCdfId, nFirstVarId, "coordinates", &pszCoordinates)
        == CE_None )
    {
        char **papszTokens = CSLTokenizeString2(pszCoordinates, " ", 0);
        for(int i = 0;
            papszTokens != nullptr && papszTokens[i] != nullptr; i++)
        {
            if( NCDFIsVarLongitude(nCdfId, -1, papszTokens[i]) ||
                NCDFIsVarProjectionX(nCdfId, -1, papszTokens[i]) )
            {
                nVarXId = -1;
                CPL_IGNORE_RET_VAL(nc_inq_varid(nCdfId, papszTokens[i],
                                                &nVarXId));
            }
            else if( NCDFIsVarLatitude(nCdfId, -1, papszTokens[i]) ||
                    NCDFIsVarProjectionY(nCdfId, -1, papszTokens[i]) )
            {
                nVarYId = -1;
                CPL_IGNORE_RET_VAL(nc_inq_varid(nCdfId, papszTokens[i],
                                                &nVarYId));
            }
            else if( NCDFIsVarVerticalCoord(nCdfId, -1, papszTokens[i]))
            {
                nVarZId = -1;
                CPL_IGNORE_RET_VAL(nc_inq_varid(nCdfId, papszTokens[i],
                                                &nVarZId));
            }
        }
        CSLDestroy(papszTokens);
    }
    CPLFree(pszCoordinates);

    // Check that the X,Y,Z vars share 1D and share the same dimension as
    // attribute variables.
    if( nVarXId >= 0 && nVarYId >= 0 )
    {
        int nVarDimCount = -1;
        int nVarDimId = -1;
        if( nc_inq_varndims(nCdfId, nVarXId, &nVarDimCount) != NC_NOERR ||
            nVarDimCount != 1 ||
            nc_inq_vardimid(nCdfId, nVarXId, &nVarDimId) != NC_NOERR ||
            nVarDimId != ((nProfileDimId >= 0) ? nProfileDimId : nVectorDim) ||
            nc_inq_varndims(nCdfId, nVarYId, &nVarDimCount) != NC_NOERR ||
            nVarDimCount != 1 ||
            nc_inq_vardimid(nCdfId, nVarYId, &nVarDimId) != NC_NOERR ||
            nVarDimId != ((nProfileDimId >= 0) ? nProfileDimId : nVectorDim) )
        {
            nVarXId = nVarYId = -1;
        }
        else if( nVarZId >= 0 && (nc_inq_varndims(nCdfId, nVarZId,
                                                  &nVarDimCount) != NC_NOERR ||
                                  nVarDimCount != 1 ||
                                  nc_inq_vardimid(nCdfId, nVarZId,
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
    if( eGType == wkbPoint && nVarXId >= 0 && nVarYId >= 0 && nVarZId >= 0 )
    {
        eGType = wkbPoint25D;
    }
    if( eGType == wkbUnknown && osGeometryField.empty() )
    {
        eGType = wkbNone;
    }

    // Read projection info
    char **papszMetadataBackup = CSLDuplicate(papszMetadata);
    ReadAttributes(nCdfId, nFirstVarId);
    if(!this->bSGSupport)
        SetProjectionFromVar(nCdfId, nFirstVarId, true);
    const char *pszValue = FetchAttr(nCdfId, nFirstVarId, CF_GRD_MAPPING);
    char *pszGridMapping = (pszValue ? CPLStrdup(pszValue) : nullptr);
    CSLDestroy(papszMetadata);
    papszMetadata = papszMetadataBackup;

    OGRSpatialReference *poSRS = nullptr;
    if( m_pszProjection[0] )
    {
        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSRS->importFromWkt(m_pszProjection) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
        }
        CPLFree(m_pszProjection);
        m_pszProjection = CPLStrdup("");
    }
    // Reset if there's a 2D raster
    m_bHasProjection = false;
    m_bHasGeoTransform = false;

    if( !bKeepRasters )
    {
        // Strip out uninteresting metadata.
        papszMetadata = CSLSetNameValue(papszMetadata, "NC_GLOBAL#Conventions",
                                        nullptr);
        papszMetadata = CSLSetNameValue(papszMetadata, "NC_GLOBAL#GDAL",
                                        nullptr);
        papszMetadata = CSLSetNameValue(papszMetadata, "NC_GLOBAL#history",
                                        nullptr);
    }

    std::shared_ptr<netCDFLayer> poLayer(new netCDFLayer(this, nCdfId, osLayerName,
                                           eGType, poSRS));
    if( poSRS != nullptr )
        poSRS->Release();
    poLayer->SetRecordDimID(nVectorDim);
    if( wkbFlatten(eGType) == wkbPoint && nVarXId >= 0 && nVarYId >= 0 )
    {
        poLayer->SetXYZVars(nVarXId, nVarYId, nVarZId);
    }
    else if( !osGeometryField.empty() )
    {
        poLayer->SetWKTGeometryField(osGeometryField);
    }
    if( pszGridMapping != nullptr )
    {
        poLayer->SetGridMapping(pszGridMapping);
        CPLFree(pszGridMapping);
    }
    poLayer->SetProfile(nProfileDimId, nParentIndexVarID);

    for( size_t j = 0; j < anPotentialVectorVarID.size(); j++ )
    {
        int anDimIds[2] = { -1, -1 };
        nc_inq_vardimid(nCdfId, anPotentialVectorVarID[j], anDimIds);
        if( anDimIds[0] == nVectorDim || (nProfileDimId >= 0 &&
                                          anDimIds[0] == nProfileDimId) )
        {
#ifdef NCDF_DEBUG
            char szTemp2[NC_MAX_NAME + 1] = {};
            CPL_IGNORE_RET_VAL(nc_inq_varname(
                nCdfId, anPotentialVectorVarID[j], szTemp2));
            CPLDebug("GDAL_netCDF", "Variable %s is a vector field", szTemp2);
#endif
            poLayer->AddField(anPotentialVectorVarID[j]);
        }
    }

    if( poLayer->GetLayerDefn()->GetFieldCount() != 0 ||
        poLayer->GetGeomType() != wkbNone )
    {
        papoLayers.push_back(poLayer);
    }

    return CE_None;
}

// Get all coordinate and boundary variables full names referenced in
// a given a NetCDF (or group) ID and its sub-groups.
// These variables are identified in other variable's
// "coordinates" and "bounds" attribute.
// Searching coordinate and boundary variables may need to explore
// parents groups (or other groups in case of reference given in form of an
// absolute path).
// See CF sections 5.2, 5.6 and 7.1
static CPLErr NCDFGetCoordAndBoundVarFullNames( int nCdfId,
                                                char ***ppapszVars )
{
    int nVars = 0;
    NCDF_ERR(nc_inq( nCdfId, nullptr, &nVars, nullptr, nullptr));

    for( int v = 0; v < nVars; v++ )
    {
        char *pszTemp = nullptr;
        char **papszTokens = nullptr;
        if( NCDFGetAttr(nCdfId, v, "coordinates", &pszTemp) == CE_None )
            papszTokens = CSLTokenizeString2(pszTemp, " ", 0);
        CPLFree(pszTemp);
        pszTemp = nullptr;
        if( NCDFGetAttr(nCdfId, v, "bounds", &pszTemp) == CE_None &&
            pszTemp != nullptr && !EQUAL(pszTemp, "") )
            papszTokens = CSLAddString( papszTokens, pszTemp );
        CPLFree(pszTemp);
        for( int i = 0; papszTokens != nullptr && papszTokens[i] != nullptr; i++ )
        {
            char *pszVarFullName = nullptr;
            if( NCDFResolveVarFullName(nCdfId, papszTokens[i],
                                       &pszVarFullName) == CE_None )
                *ppapszVars = CSLAddString(*ppapszVars, pszVarFullName);
            CPLFree(pszVarFullName);
        }
        CSLDestroy(papszTokens);
    }

    // Recurse on sub-groups.
    int nSubGroups;
    int *panSubGroupIds = nullptr;
    NCDFGetSubGroups(nCdfId, &nSubGroups, &panSubGroupIds);
    for( int i = 0; i < nSubGroups; i++ )
    {
        NCDFGetCoordAndBoundVarFullNames(panSubGroupIds[i], ppapszVars);
    }
    CPLFree(panSubGroupIds);

    return CE_None;
}

//Check if give type is user defined
bool NCDFIsUserDefinedType(int ncid, int type)
{
    //Adapted from OPENDAP netcdf_handler
    //To circumvent use of NC_FIRSTUSERTYPEID
    //Which is not a part of netcdf 4.1.1 installed on RH
    //In all later version, type >= NC_FIRSTUSERTYPEID works
#if NETCDF_HAS_NC4
#  ifdef NC_FIRSTUSERTYPEID
    CPL_IGNORE_RET_VAL(ncid);
    return type >= NC_FIRSTUSERTYPEID;
#  else
    int ntypes;
    int typeids[NC_MAX_VARS];

    while( true )
    {
        int err = nc_inq_typeids(ncid, &ntypes, typeids);
        if (err != NC_NOERR)
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Could not get user defined type information");

        for (int i = 0; i < ntypes; ++i) {
            if (type == typeids[i])
                return true;
        }

        int nParentGroupId;
        int status = nc_inq_grp_parent(ncid, &nParentGroupId);
        if( status != NC_NOERR )
            break;
        ncid = nParentGroupId;
    }
    return false;
#  endif
#else
    return false;
#endif
}
