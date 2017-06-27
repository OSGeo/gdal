/******************************************************************************
 *
 * Project:  GDAL algorithms
 * Purpose:  Apply vertical shift grid
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdalwarper.h"
#include "vrtdataset.h"
#include "ogr_spatialref.h"

#ifdef PROJ_STATIC
#include "proj_api.h"
#endif

#include <limits>

CPL_CVSID("$Id$")

/************************************************************************/
/*                        GDALApplyVSGDataset                           */
/************************************************************************/

class GDALApplyVSGDataset: public GDALDataset
{
        friend class GDALApplyVSGRasterBand;

        GDALDataset* m_poSrcDataset;
        GDALDataset* m_poReprojectedGrid;
        bool         m_bInverse;
        double       m_dfSrcUnitToMeter;
        double       m_dfDstUnitToMeter;

    public:
        GDALApplyVSGDataset( GDALDataset* poSrcDataset,
                             GDALDataset* poReprojectedGrid,
                             GDALDataType eDT,
                             bool bInverse,
                             double dfSrcUnitToMeter,
                             double dfDstUnitToMeter,
                             int nBlockSize );
        virtual ~GDALApplyVSGDataset();

        virtual int        CloseDependentDatasets() override;

        virtual CPLErr GetGeoTransform(double* padfGeoTransform) override;
        virtual const char* GetProjectionRef() override;

        bool    IsInitOK();
};

/************************************************************************/
/*                       GDALApplyVSGRasterBand                         */
/************************************************************************/

class GDALApplyVSGRasterBand: public GDALRasterBand
{
        friend class GDALApplyVSGDataset;

        float       *m_pafSrcData;
        float       *m_pafGridData;

    public:
        GDALApplyVSGRasterBand( GDALDataType eDT,
                                int nBlockSize );
        virtual ~GDALApplyVSGRasterBand();

        virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pData ) override;
        virtual double GetNoDataValue( int* pbSuccess ) override;
};

/************************************************************************/
/*                        GDALApplyVSGDataset()                         */
/************************************************************************/

GDALApplyVSGDataset::GDALApplyVSGDataset( GDALDataset* poSrcDataset,
                                          GDALDataset* poReprojectedGrid,
                                          GDALDataType eDT,
                                          bool bInverse,
                                          double dfSrcUnitToMeter,
                                          double dfDstUnitToMeter,
                                          int nBlockSize ) :
    m_poSrcDataset(poSrcDataset),
    m_poReprojectedGrid(poReprojectedGrid),
    m_bInverse(bInverse),
    m_dfSrcUnitToMeter(dfSrcUnitToMeter),
    m_dfDstUnitToMeter(dfDstUnitToMeter)
{
    m_poSrcDataset->Reference();
    m_poReprojectedGrid->Reference();

    nRasterXSize = poSrcDataset->GetRasterXSize();
    nRasterYSize = poSrcDataset->GetRasterYSize();
    SetBand( 1, new GDALApplyVSGRasterBand( eDT, nBlockSize ) );
}

/************************************************************************/
/*                       ~GDALApplyVSGDataset()                         */
/************************************************************************/

GDALApplyVSGDataset::~GDALApplyVSGDataset()
{
    CloseDependentDatasets();
}

/************************************************************************/
/*                     CloseDependentDatasets()                         */
/************************************************************************/

int GDALApplyVSGDataset::CloseDependentDatasets()
{
    bool bRet = false;
    if( m_poSrcDataset != NULL )
    {
        if( m_poSrcDataset->ReleaseRef() )
        {
            bRet = true;
        }
        m_poSrcDataset = NULL;
    }
    if( m_poReprojectedGrid != NULL )
    {
        if( m_poReprojectedGrid->ReleaseRef() )
        {
            bRet = true;
        }
        m_poReprojectedGrid = NULL;
    }
    return bRet;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALApplyVSGDataset::GetGeoTransform(double* padfGeoTransform)
{
    return m_poSrcDataset->GetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* GDALApplyVSGDataset::GetProjectionRef()
{
    return m_poSrcDataset->GetProjectionRef();
}

/************************************************************************/
/*                             IsInitOK()                               */
/************************************************************************/

bool GDALApplyVSGDataset::IsInitOK()
{
    GDALApplyVSGRasterBand* poBand =
        reinterpret_cast<GDALApplyVSGRasterBand*>(GetRasterBand(1));
    return poBand->m_pafSrcData != NULL && poBand->m_pafGridData != NULL;
}

/************************************************************************/
/*                       GDALApplyVSGRasterBand()                       */
/************************************************************************/

GDALApplyVSGRasterBand::GDALApplyVSGRasterBand( GDALDataType eDT,
                                                int nBlockSize )
{
    eDataType = eDT;
    nBlockXSize = nBlockSize;
    nBlockYSize = nBlockSize;
    m_pafSrcData = static_cast<float*>(
        VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize, sizeof(float)));
    m_pafGridData = static_cast<float*>(
        VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize, sizeof(float)));
}

/************************************************************************/
/*                      ~GDALApplyVSGRasterBand()                       */
/************************************************************************/

GDALApplyVSGRasterBand::~GDALApplyVSGRasterBand()
{
    VSIFree(m_pafSrcData);
    VSIFree(m_pafGridData);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALApplyVSGRasterBand::GetNoDataValue( int* pbSuccess )
{
    GDALApplyVSGDataset* poGDS = reinterpret_cast<GDALApplyVSGDataset*>(poDS);
    return poGDS->m_poSrcDataset->GetRasterBand(1)->GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                              IReadBlock()                            */
/************************************************************************/

CPLErr GDALApplyVSGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                           void * pData )
{
    GDALApplyVSGDataset* poGDS = reinterpret_cast<GDALApplyVSGDataset*>(poDS);

    const int nXOff = nBlockXOff * nBlockXSize;
    const int nReqXSize = ( nXOff > nRasterXSize - nBlockXSize ) ?
                                    nRasterXSize - nXOff : nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqYSize = ( nYOff > nRasterYSize - nBlockYSize ) ?
                                    nRasterYSize - nYOff : nBlockYSize;

    CPLErr eErr =
        poGDS->m_poSrcDataset->GetRasterBand(1)->RasterIO(GF_Read,
                                                    nXOff, nYOff,
                                                    nReqXSize, nReqYSize,
                                                    m_pafSrcData,
                                                    nReqXSize, nReqYSize,
                                                    GDT_Float32,
                                                    sizeof(float),
                                                    nBlockXSize * sizeof(float),
                                                    NULL);
    if( eErr == CE_None )
        eErr =  poGDS->m_poReprojectedGrid->GetRasterBand(1)->RasterIO(GF_Read,
                                                    nXOff, nYOff,
                                                    nReqXSize, nReqYSize,
                                                    m_pafGridData,
                                                    nReqXSize, nReqYSize,
                                                    GDT_Float32,
                                                    sizeof(float),
                                                    nBlockXSize * sizeof(float),
                                                    NULL);
    if( eErr == CE_None )
    {
        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        int bHasNoData = FALSE;
        float fNoDataValue = static_cast<float>(GetNoDataValue(&bHasNoData));
        for( int iY = 0; iY < nReqYSize; iY++ )
        {
            for( int iX = 0; iX < nReqXSize; iX ++ )
            {
                const float fSrcVal = m_pafSrcData[iY * nBlockXSize + iX];
                const float fGridVal = m_pafGridData[iY * nBlockXSize + iX];
                if( bHasNoData && fSrcVal == fNoDataValue )
                {
                }
                else if( CPLIsInf(fGridVal) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Missing vertical grid value at source (%d,%d)",
                             nXOff + iX, nYOff + iY);
                    return CE_Failure;
                }
                else if( poGDS->m_bInverse )
                {
                    m_pafSrcData[iY * nBlockXSize + iX] = static_cast<float>(
                        (fSrcVal * poGDS->m_dfSrcUnitToMeter - fGridVal) / 
                                                poGDS->m_dfDstUnitToMeter);
                }
                else
                {
                    m_pafSrcData[iY * nBlockXSize + iX] = static_cast<float>(
                        (fSrcVal * poGDS->m_dfSrcUnitToMeter + fGridVal) / 
                                                poGDS->m_dfDstUnitToMeter);
                }
            }
            GDALCopyWords( m_pafSrcData + iY * nBlockXSize,
                                GDT_Float32, sizeof(float),
                           static_cast<GByte*>(pData) + iY * nBlockXSize *
                                nDTSize, eDataType, nDTSize,
                           nReqXSize );
        }
    }

    return eErr;
}

/************************************************************************/
/*                      GDALApplyVerticalShiftGrid()                    */
/************************************************************************/

/** Apply a vertical shift grid to a source (DEM typically) dataset.
 * 
 * hGridDataset will typically use WGS84 as horizontal datum (but this is
 * not a requirement) and its values are the values to add to go from geoid
 * elevations to WGS84 ellipsoidal heights.
 * 
 * hGridDataset will be on-the-fly reprojected and resampled to the projection
 * and resolution of hSrcDataset, using bilinear resampling by default.
 * 
 * Both hSrcDataset and hGridDataset must be single band datasets, and have
 * a valid geotransform and projection.
 *
 * On success, a reference will be taken on hSrcDataset and hGridDataset.
 * Reference counting semantics on the source and grid datasets should be
 * honoured. That is, don't just GDALClose() it, unless it was opened with
 * GDALOpenShared(), but rather use GDALReleaseDataset() if wanting to
 * immediately release the reference(s) and make the returned dataset the
 * owner of them.
 *
 * Valid use cases:
 * 
 * \code
 * hSrcDataset = GDALOpen(...)
 * hGridDataset = GDALOpen(...)
 * hDstDataset = GDALApplyVerticalShiftGrid(hSrcDataset, hGridDataset, ...)
 * GDALReleaseDataset(hSrcDataset);
 * GDALReleaseDataset(hGridDataset);
 * if( hDstDataset )
 * {
 *     // Do things with hDstDataset 
 *     GDALClose(hDstDataset) // will close hSrcDataset and hGridDataset
 * }
 * \endcode

 *
 * @param hSrcDataset source (DEM) dataset. Must not be NULL.
 * @param hGridDataset vertical grid shift dataset. Must not be NULL.
 * @param bInverse if set to FALSE, hGridDataset values will be added to
 *                 hSrcDataset. If set to TRUE, they will be subtracted.
 * @param dfSrcUnitToMeter the factor to convert values from hSrcDataset to
 *                         meters (1.0 if source values are in meter).
 * @param dfDstUnitToMeter the factor to convert shifted values from meter
 *                          (1.0 if output values must be in meter).
 * @param papszOptions list of options, or NULL. Supported options are:
 * <ul>
 * <li>RESAMPLING=NEAREST/BILINEAR/CUBIC. Defaults to BILINEAR.</li>
 * <li>MAX_ERROR=val. Maximum error measured in input pixels that is allowed in
 * approximating the transformation (0.0 for exact calculations). Defaults
 * to 0.125</li>
 * <li>DATATYPE=Byte/UInt16/Int16/Float32/Float64. Output data type. If not
 * specified will be the same as the one of hSrcDataset.
 * <li>ERROR_ON_MISSING_VERT_SHIFT=YES/NO. Whether a missing/nodata value in
 * hGridDataset should cause I/O requests to fail. Default is NO (in which case
 * 0 will be used)
 * <li>SRC_SRS=srs_def. Override projection on hSrcDataset;
 * </ul>
 *
 * @return a new dataset corresponding to hSrcDataset adjusted with
 * hGridDataset, or NULL. If not NULL, it must be closed with GDALClose().
 *
 * @since GDAL 2.2
 */
GDALDatasetH GDALApplyVerticalShiftGrid( GDALDatasetH hSrcDataset,
                                         GDALDatasetH hGridDataset,
                                         int bInverse,
                                         double dfSrcUnitToMeter,
                                         double dfDstUnitToMeter,
                                         const char* const* papszOptions )
{
    VALIDATE_POINTER1( hSrcDataset, "GDALApplyVerticalShiftGrid", NULL );
    VALIDATE_POINTER1( hGridDataset, "GDALApplyVerticalShiftGrid", NULL );

    double adfSrcGT[6];
    if( GDALGetGeoTransform(hSrcDataset, adfSrcGT) != CE_None )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset has no geotransform.");
        return NULL;
    }
    const char* pszSrcProjection = CSLFetchNameValueDef(papszOptions,
                                            "SRC_SRS",
                                            GDALGetProjectionRef(hSrcDataset));
    if( pszSrcProjection == NULL || pszSrcProjection[0] == '\0' )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset has no projection.");
        return NULL;
    }
    if(  GDALGetRasterCount(hSrcDataset) != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only single band source dataset is supported.");
        return NULL;
    }

    double adfGridGT[6];
    if( GDALGetGeoTransform(hGridDataset, adfGridGT) != CE_None )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Grid dataset has no geotransform.");
        return NULL;
    }
    const char* pszGridProjection = GDALGetProjectionRef(hGridDataset);
    if( pszGridProjection == NULL || pszGridProjection[0] == '\0' )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Grid dataset has no projection.");
        return NULL;
    }
    if(  GDALGetRasterCount(hGridDataset) != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only single band grid dataset is supported.");
        return NULL;
    }

    GDALDataType eDT = GDALGetRasterDataType(GDALGetRasterBand(hSrcDataset,1));
    const char* pszDataType = CSLFetchNameValue(papszOptions, "DATATYPE");
    if( pszDataType )
        eDT = GDALGetDataTypeByName(pszDataType);
    if( eDT == GDT_Unknown )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid DATATYPE=%s", pszDataType);
        return NULL;
    }

    const int nSrcXSize = GDALGetRasterXSize(hSrcDataset);
    const int nSrcYSize = GDALGetRasterYSize(hSrcDataset);

    OGRSpatialReference oSRS;
    CPLString osSrcProjection(pszSrcProjection);
    oSRS.SetFromUserInput(osSrcProjection);
    if( oSRS.IsCompound() )
    {
        OGR_SRSNode* poNode = oSRS.GetRoot()->GetChild(1);
        if( poNode != NULL )
        {
            char* pszWKT = NULL;
            poNode->exportToWkt(&pszWKT);
            osSrcProjection = pszWKT;
            CPLFree(pszWKT);
        }
    }

    void* hTransform = GDALCreateGenImgProjTransformer3( pszGridProjection,
                                                         adfGridGT,
                                                         osSrcProjection,
                                                         adfSrcGT );
    if( hTransform == NULL )
        return NULL;
    GDALWarpOptions* psWO = GDALCreateWarpOptions();
    psWO->hSrcDS = hGridDataset;
    psWO->eResampleAlg = GRA_Bilinear;
    const char* pszResampling = CSLFetchNameValue(papszOptions, "RESAMPLING");
    if( pszResampling )
    {
        if( EQUAL(pszResampling, "NEAREST") )
            psWO->eResampleAlg = GRA_NearestNeighbour;
        else if( EQUAL(pszResampling, "BILINEAR") )
            psWO->eResampleAlg = GRA_Bilinear;
        else if( EQUAL(pszResampling, "CUBIC") )
            psWO->eResampleAlg = GRA_Cubic;
    }
    psWO->eWorkingDataType = GDT_Float32;
    int bHasNoData = FALSE;
    const double dfSrcNoData = GDALGetRasterNoDataValue(
        GDALGetRasterBand(hGridDataset, 1), &bHasNoData );
    if( bHasNoData )
    {
        psWO->padfSrcNoDataReal =
                static_cast<double*>(CPLMalloc(sizeof(double)));
        psWO->padfSrcNoDataReal[0] = dfSrcNoData;
    }

    psWO->padfDstNoDataReal = static_cast<double*>(CPLMalloc(sizeof(double)));
    const bool bErrorOnMissingShift = CPLFetchBool( papszOptions,
                                              "ERROR_ON_MISSING_VERT_SHIFT",
                                              false );
    psWO->padfDstNoDataReal[0] = 
        (bErrorOnMissingShift) ? -std::numeric_limits<float>::infinity() : 0.0;
    psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                                 "INIT_DEST",
                                                 "NO_DATA");

    psWO->pfnTransformer = GDALGenImgProjTransform;
    psWO->pTransformerArg = hTransform;
    const double dfMaxError = CPLAtof(CSLFetchNameValueDef(papszOptions,
                                                           "MAX_ERROR",
                                                           "0.125"));
    if( dfMaxError > 0.0 )
    {
        psWO->pTransformerArg =
            GDALCreateApproxTransformer( psWO->pfnTransformer,
                                         psWO->pTransformerArg,
                                         dfMaxError );
        psWO->pfnTransformer = GDALApproxTransform;
        GDALApproxTransformerOwnsSubtransformer(psWO->pTransformerArg, TRUE);
    }
    psWO->nBandCount = 1;
    psWO->panSrcBands = static_cast<int *>(CPLMalloc(sizeof(int)));
    psWO->panSrcBands[0] = 1;
    psWO->panDstBands = static_cast<int *>(CPLMalloc(sizeof(int)));
    psWO->panDstBands[0] = 1;

    VRTWarpedDataset* poReprojectedGrid =
                new VRTWarpedDataset(nSrcXSize, nSrcYSize);
    // This takes a reference on hGridDataset
    CPLErr eErr = poReprojectedGrid->Initialize(psWO);
    CPLAssert(eErr == CE_None);
    CPL_IGNORE_RET_VAL(eErr);
    GDALDestroyWarpOptions(psWO);
    poReprojectedGrid->SetGeoTransform(adfSrcGT);
    poReprojectedGrid->AddBand(GDT_Float32, NULL);

    GDALApplyVSGDataset* poOutDS = new GDALApplyVSGDataset(
        reinterpret_cast<GDALDataset*>(hSrcDataset),
        poReprojectedGrid,
        eDT,
        CPL_TO_BOOL(bInverse),
        dfSrcUnitToMeter,
        dfDstUnitToMeter,
        // Undocumented option. For testing only
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "256")) );

    poReprojectedGrid->ReleaseRef();

    if( !poOutDS->IsInitOK() )
    {
        delete poOutDS;
        return NULL;
    }
    poOutDS->SetDescription( GDALGetDescription( hSrcDataset ) );
    return reinterpret_cast<GDALDatasetH>(poOutDS);
}

/************************************************************************/
/*                          my_proj4_logger()                           */
/************************************************************************/

#if defined(PROJ_STATIC) && PJ_VERSION <= 493
static void my_proj4_logger(void * user_data, int /*level*/, const char * msg)
{
    CPLString* posMsg = static_cast<CPLString*>(user_data);
    *posMsg += msg;
}
#endif

/************************************************************************/
/*                           GetProj4Filename()                         */
/************************************************************************/

static CPLString GetProj4Filename(const char* pszFilename)
{
    CPLString osFilename;

    /* or fixed path: /name, ./name or ../name */
    if ( !CPLIsFilenameRelative(pszFilename) || *pszFilename == '.' )
    {
        return pszFilename;
    }

#if defined(PROJ_STATIC) && PJ_VERSION > 493
    osFilename.resize(2048);
    projCtx ctx = pj_ctx_alloc();
    if( pj_find_file(ctx, pszFilename, &osFilename[0], osFilename.size()) )
    {
        osFilename.resize( strlen(osFilename) );
    }
    else
    {
        osFilename.clear();
    }
    pj_ctx_free(ctx);
#else
    // Transpose some of the proj.4 pj_open_lib() logic...

    /* check if ~/name */
    char* pszSysname;
    if (*pszFilename == '~' &&
        (pszFilename[1] == '/' || pszFilename[1] == '\\') )
    {
        if ((pszSysname = getenv("HOME")) != NULL)
        {
            osFilename = CPLFormFilename(pszSysname, pszFilename + 1, NULL);
        }
        return osFilename;
    }

    /* or is environment PROJ_LIB defined */
    else if ((pszSysname = getenv("PROJ_LIB")) != NULL)
    {
        osFilename = CPLFormFilename(pszSysname, pszFilename, NULL);
        VSIStatBufL sStat;
        if( VSIStatL(osFilename, &sStat) == 0 )
            return osFilename;
        osFilename.clear();
    }


#if defined(PROJ_STATIC) && PJ_VERSION >= 490
    // Super messy. proj.4 up to 4.9.3 had no public API to return the full
    // path to a resource file, so we rely on the fact that it emits a log
    // message with it...
    // Basically this is needed in the case where the file is in the
    // resource installation directory of proj.4, which we have no way to
    // know otherwise.
    CPLString osMsg;
    projCtx ctx = pj_ctx_alloc();
    pj_ctx_set_app_data(ctx, &osMsg);
    pj_ctx_set_debug(ctx, PJ_LOG_DEBUG_MAJOR);
    pj_ctx_set_logger(ctx, my_proj4_logger);
    PAFile f = pj_open_lib(ctx, pszFilename, "rb");
    if( f )
    {
        pj_ctx_fclose(ctx, f);
        size_t nPos = osMsg.find("fopen(");
        if( nPos != std::string::npos )
        {
            osFilename = osMsg.substr(nPos + strlen("fopen("));
            nPos = osFilename.find(")");
            if( nPos != std::string::npos )
                osFilename = osFilename.substr(0, nPos);
        }
    }
    pj_ctx_free(ctx);
#endif
#endif
    return osFilename;
}


/************************************************************************/
/*                       GDALOpenVerticalShiftGrid()                    */
/************************************************************************/

/** Load proj.4 geoidgrids as GDAL dataset
 *
 * @param pszProj4Geoidgrids Value of proj.4 geoidgrids parameter.
 * @param pbError If not NULL, the pointed value will be set to TRUE if an
 *                error occurred.
 *
 * @return a dataset. If not NULL, it must be closed with GDALClose().
 *
 * @since GDAL 2.2
 */
GDALDatasetH GDALOpenVerticalShiftGrid( const char* pszProj4Geoidgrids,
                                        int* pbError )
{
    char** papszGrids = CSLTokenizeString2( pszProj4Geoidgrids, ",", 0);
    const int nGridCount = CSLCount(papszGrids);
    if( nGridCount == 1 )
    {
        CSLDestroy(papszGrids);

        bool bMissingOk = false;
        if( *pszProj4Geoidgrids == '@' )
        {
            pszProj4Geoidgrids ++;
            bMissingOk = true;
        }
        const CPLString osFilename(GetProj4Filename(pszProj4Geoidgrids));
        const char* const papszOpenOptions[] =
            { "@SHIFT_ORIGIN_IN_MINUS_180_PLUS_180=YES", NULL };
        GDALDatasetH hDS = GDALOpenEx(osFilename, 0, NULL, papszOpenOptions, NULL);
        if( hDS == NULL )
        {
            CPLDebug("GDAL", "Cannot find file corresponding to %s",
                     pszProj4Geoidgrids);
        }
        if( pbError )
            *pbError = (!bMissingOk && hDS == NULL);
        return hDS;
    }

    CPLStringList aosFilenames;
    for( int i = nGridCount - 1; i >= 0; i-- )
    {
        const char* pszName = papszGrids[i];
        bool bMissingOk = false;
        if( *pszName == '@' )
        {
            pszName ++;
            bMissingOk = true;
        }
        const CPLString osFilename(GetProj4Filename(pszName));
        VSIStatBufL sStat;
        if( osFilename.empty() || VSIStatL(osFilename, &sStat) != 0 )
        {
            CPLDebug("GDAL", "Cannot find file corresponding to %s",
                     pszName);
            if( !bMissingOk )
            {
                if( pbError )
                    *pbError = true;
                CSLDestroy(papszGrids);
                return NULL;
            }
        }
        else
        {
            aosFilenames.AddString(osFilename);
        }
    }

    CSLDestroy(papszGrids);

    if( aosFilenames.empty() )
    {
        if( pbError )
            *pbError = false;
        return NULL;
    }

    char** papszArgv = NULL;
    papszArgv = CSLAddString(papszArgv, "-resolution");
    papszArgv = CSLAddString(papszArgv, "highest");
    papszArgv = CSLAddString(papszArgv, "-vrtnodata");
    papszArgv = CSLAddString(papszArgv, "-inf");
    papszArgv = CSLAddString(papszArgv, "-oo");
    papszArgv = CSLAddString(papszArgv, "@SHIFT_ORIGIN_IN_MINUS_180_PLUS_180=YES");
    GDALBuildVRTOptions* psOptions = GDALBuildVRTOptionsNew(papszArgv, NULL);
    CSLDestroy(papszArgv);
    GDALDatasetH hDS =
        GDALBuildVRT( "", aosFilenames.size(), NULL, aosFilenames.List(),
                     psOptions, NULL );
    GDALBuildVRTOptionsFree( psOptions );
    if( pbError )
        *pbError = hDS != NULL;
    return hDS;
}
