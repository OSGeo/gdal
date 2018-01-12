/******************************************************************************
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for read support
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
 * Copyright (c) 2008-2017, Even Rouault <even dot rouault at spatialys dot com>
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
 ******************************************************************************
 *
 */

#include "cpl_port.h"
#include "gribdataset.h"

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "degrib/degrib/datasource.h"
#include "degrib/degrib/degrib2.h"
#include "degrib/degrib/filedatasource.h"
#include "degrib/degrib/inventory.h"
#include "degrib/degrib/memorydatasource.h"
#include "degrib/degrib/meta.h"
#include "degrib/degrib/metaname.h"
#include "degrib/degrib/myerror.h"
#include "degrib/degrib/type.h"
CPL_C_START
#include "degrib/g2clib/grib2.h"
#include "degrib/g2clib/pdstemplates.h"
CPL_C_END
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

static CPLMutex *hGRIBMutex = nullptr;

/************************************************************************/
/*                         ConvertUnitInText()                          */
/************************************************************************/

static CPLString ConvertUnitInText( bool bMetricUnits, const char *pszTxt )
{
    if( !bMetricUnits )
        return pszTxt;

    CPLString osRes(pszTxt);
    size_t iPos = osRes.find("[K]");
    if( iPos != std::string::npos )
        osRes = osRes.substr(0, iPos) + "[C]" + osRes.substr(iPos + 3);
    return osRes;
}

/************************************************************************/
/*                           GRIBRasterBand()                            */
/************************************************************************/

GRIBRasterBand::GRIBRasterBand( GRIBDataset *poDSIn, int nBandIn,
                                inventoryType *psInv ) :
    start(psInv->start),
    subgNum(psInv->subgNum),
    longFstLevel(CPLStrdup(psInv->longFstLevel)),
    m_Grib_Data(nullptr),
    m_Grib_MetaData(nullptr),
    nGribDataXSize(poDSIn->nRasterXSize),
    nGribDataYSize(poDSIn->nRasterYSize),
    m_nGribVersion(psInv->GribVersion),
    m_bHasLookedForNoData(false),
    m_dfNoData(0.0),
    m_bHasNoData(false)

{
    poDS = poDSIn;
    nBand = nBandIn;

    // Let user do -ot Float32 if needed for saving space, GRIB contains
    // Float64 (though not fully utilized most of the time).
    eDataType = GDT_Float64;

    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;

    const char *pszGribNormalizeUnits =
        CPLGetConfigOption("GRIB_NORMALIZE_UNITS", "YES");
    bool bMetricUnits = CPLTestBool(pszGribNormalizeUnits);

    SetMetadataItem("GRIB_UNIT",
                    ConvertUnitInText(bMetricUnits, psInv->unitName));
    SetMetadataItem("GRIB_COMMENT",
                    ConvertUnitInText(bMetricUnits, psInv->comment));
    SetMetadataItem("GRIB_ELEMENT", psInv->element);
    SetMetadataItem("GRIB_SHORT_NAME", psInv->shortFstLevel);
    SetMetadataItem("GRIB_REF_TIME",
                    CPLString().Printf("%12.0f sec UTC", psInv->refTime));
    SetMetadataItem("GRIB_VALID_TIME",
                    CPLString().Printf("%12.0f sec UTC", psInv->validTime));
    SetMetadataItem("GRIB_FORECAST_SECONDS",
                    CPLString().Printf("%.0f sec", psInv->foreSec));
}

/************************************************************************/
/*                          FindPDSTemplate()                           */
/*                                                                      */
/*      Scan the file for the PDS template info and represent it as     */
/*      metadata.                                                       */
/************************************************************************/

void GRIBRasterBand::FindPDSTemplate()

{
    GRIBDataset *poGDS = static_cast<GRIBDataset *>(poDS);

    // Read section 0
    GByte abySection0[16];
    VSIFSeekL(poGDS->fp, start, SEEK_SET);
    VSIFReadL(abySection0, 16, 1, poGDS->fp);
    GByte nDiscipline = abySection0[7 - 1]; 
    CPLString osDiscipline;
    osDiscipline = CPLString().Printf("%d", nDiscipline);
    static const char * const table00[] = {
        "Meteorological",
        "Hydrological",
        "Land Surface",
        "Space products",
        "Space products",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Oceanographic Products"
    };
    if( nDiscipline < CPL_ARRAYSIZE(table00) )
    {
        osDiscipline += CPLString("(") +
            CPLString(table00[nDiscipline]).replaceAll(' ','_') + ")";
    }

    SetMetadataItem("GRIB_DISCIPLINE", osDiscipline.c_str());

    GByte abyHead[5] = { 0 };
    VSIFReadL(abyHead, 5, 1, poGDS->fp);

    GUInt32 nSectSize = 0;
    if( abyHead[4] == 1 )
    {
        memcpy(&nSectSize, abyHead, 4);
        CPL_MSBPTR32(&nSectSize);
        if( nSectSize >= 21 &&
            nSectSize <= 100000  /* arbitrary upper limit */ )
        {
            GByte *pabyBody = static_cast<GByte *>(CPLMalloc(nSectSize));
            memcpy(pabyBody, abyHead, 5);
            VSIFReadL(pabyBody + 5, 1, nSectSize - 5, poGDS->fp);

            CPLString osIDS;
            unsigned short nCenter = static_cast<unsigned short>(
                                        pabyBody[6-1] * 256 + pabyBody[7-1]);
            if( nCenter != GRIB2MISSING_u1 && nCenter != GRIB2MISSING_u2 )
            {
                osIDS += "CENTER=";
                osIDS += CPLSPrintf("%d", nCenter);
                const char* pszCenter = centerLookup(nCenter);
                if( pszCenter )
                    osIDS += CPLString("(")+pszCenter+")";
            }

            unsigned short nSubCenter = static_cast<unsigned short>(
                                        pabyBody[8-1] * 256 + pabyBody[9-1]);
            if( nSubCenter != GRIB2MISSING_u2 )
            {
                if( !osIDS.empty() ) osIDS += " ";
                osIDS += "SUBCENTER=";
                osIDS += CPLSPrintf("%d", nSubCenter);
                const char* pszSubCenter = subCenterLookup(nCenter, nSubCenter);
                if( pszSubCenter )
                    osIDS += CPLString("(")+pszSubCenter+")";
            }

            if( !osIDS.empty() ) osIDS += " ";
            osIDS += "MASTER_TABLE=";
            osIDS += CPLSPrintf("%d", pabyBody[10-1]);
            osIDS += " ";
            osIDS += "LOCAL_TABLE=";
            osIDS += CPLSPrintf("%d", pabyBody[11-1]);
            osIDS += " ";
            osIDS += "SIGNF_REF_TIME=";
            unsigned nSignRefTime = pabyBody[12-1];
            osIDS += CPLSPrintf("%d", nSignRefTime);
            static const char * const table12[] = {
                "Analysis",
                "Start of Forecast",
                "Verifying time of forecast",
                "Observation time"
            };
            if( nSignRefTime < CPL_ARRAYSIZE(table12) )
            {
                osIDS += CPLString("(") +
                    CPLString(table12[nSignRefTime]).replaceAll(' ','_') + ")";
            }
            osIDS += " ";
            osIDS += "REF_TIME=";
            osIDS += CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                pabyBody[13-1] * 256 + pabyBody[14-1],
                                pabyBody[15-1],
                                pabyBody[16-1],
                                pabyBody[17-1],
                                pabyBody[18-1],
                                pabyBody[19-1]);
            osIDS += " ";
            osIDS += "PROD_STATUS=";
            unsigned nProdStatus = pabyBody[20-1];
            osIDS += CPLSPrintf("%d", nProdStatus);
            static const char * const table13[] = {
                "Operational",
                "Operational test",
                "Research",
                "Re-analysis",
                "TIGGE",
                "TIGGE test",
                "S2S operational",
                "S2S test",
                "UERRA",
                "UERRA test"
            };
            if( nProdStatus < CPL_ARRAYSIZE(table13) )
            {
                osIDS += CPLString("(") +
                    CPLString(table13[nProdStatus]).replaceAll(' ','_') + ")";
            }
            osIDS += " ";
            osIDS += "TYPE=";
            unsigned nType = pabyBody[21-1];
            osIDS += CPLSPrintf("%d", nType);
            static const char * const table14[] = { "Analysis",
                "Forecast",
                "Analysis and forecast",
                "Control forecast",
                "Perturbed forecast",
                "Control and perturbed forecast",
                "Processed satellite observations",
                "Processed radar observations",
                "Event Probability"
            };
            if( nType < CPL_ARRAYSIZE(table14) )
            {
                osIDS += CPLString("(") +
                    CPLString(table14[nType]).replaceAll(' ','_') + ")";
            }

            SetMetadataItem("GRIB_IDS", osIDS);

            CPLFree(pabyBody);
        }

        VSIFReadL(abyHead, 5, 1, poGDS->fp);
    }

    // Skip to section 4
    while( abyHead[4] != 4 )
    {
        memcpy(&nSectSize, abyHead, 4);
        CPL_MSBPTR32(&nSectSize);

        if( nSectSize < 5 ||
            VSIFSeekL(poGDS->fp, nSectSize - 5, SEEK_CUR) != 0 ||
            VSIFReadL(abyHead, 5, 1, poGDS->fp) != 1 )
            break;
    }

    // Collect section 4 octet information.  We read the file
    // ourselves since the GRIB API does not appear to preserve all
    // this for us.
    if( abyHead[4] == 4 )
    {
        memcpy(&nSectSize, abyHead, 4);
        CPL_MSBPTR32(&nSectSize);
        if( nSectSize >= 9 &&
            nSectSize <= 100000  /* arbitrary upper limit */ )
        {
            GByte *pabyBody = static_cast<GByte *>(CPLMalloc(nSectSize));
            memcpy(pabyBody, abyHead, 5);
            if( VSIFReadL(pabyBody + 5, 1, nSectSize - 5, poGDS->fp) !=
                    nSectSize - 5 )
            {
                CPLFree(pabyBody);
                return;
            }

            GUInt16 nCoordCount = 0;
            memcpy(&nCoordCount, pabyBody + 6-1, 2);
            CPL_MSBPTR16(&nCoordCount);

            GUInt16 nPDTN = 0;
            memcpy(&nPDTN, pabyBody + 8-1, 2);
            CPL_MSBPTR16(&nPDTN);

            SetMetadataItem("GRIB_PDS_PDTN", CPLString().Printf("%d", nPDTN));

            CPLString osOctet;
            const int nTemplateFoundByteCount =
                static_cast<int>(nSectSize - 9 - nCoordCount * 4);
            for( int i = 0; i < nTemplateFoundByteCount; i++ )
            {
                char szByte[10] = { '\0' };

                if( i == 0 )
                    snprintf(szByte, sizeof(szByte), "%d", pabyBody[i+9]);
                else
                    snprintf(szByte, sizeof(szByte), " %d", pabyBody[i+9]);
                osOctet += szByte;
            }

            SetMetadataItem("GRIB_PDS_TEMPLATE_NUMBERS", osOctet);

            g2int iofst = 0;
            g2int pdsnum = 0;
            g2int *pdstempl = nullptr;
            g2int mappdslen = 0;
            g2float *coordlist = nullptr;
            g2int numcoord = 0;
            if( getpdsindex(nPDTN) < 0 )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Template 4.%d is not recognized currently",
                         nPDTN);
            }
            else if ( g2_unpack4(pabyBody,nSectSize,&iofst,
                           &pdsnum,&pdstempl,&mappdslen,
                           &coordlist,&numcoord) == 0 )
            {
                gtemplate* mappds=extpdstemplate(pdsnum,pdstempl);
                if( mappds )
                {
                    int nTemplateByteCount = 0;
                    for( int i = 0; i < mappds->maplen; i++ )
                        nTemplateByteCount += abs(mappds->map[i]);
                    for( int i = 0; i < mappds->extlen; i++ )
                        nTemplateByteCount += abs(mappds->ext[i]);
                    if( nTemplateByteCount == nTemplateFoundByteCount )
                    {
                        CPLString osValues;
                        for(g2int i = 0; i < mappds->maplen+mappds->extlen; i++)
                        {
                            if( i > 0 )
                                osValues += " ";
                            const int nEltSize = (i < mappds->maplen) ?
                                mappds->map[i] :
                                mappds->ext[i - mappds->maplen];
                            if( nEltSize == 4 )
                                osValues += CPLSPrintf("%u",
                                            static_cast<GUInt32>(pdstempl[i]));
                            else
                                osValues += CPLSPrintf("%d", pdstempl[i]);
                        }
                        SetMetadataItem("GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES", osValues);
                    }
                    else
                    {
                        CPLDebug("GRIB",
                                 "Cannot expose GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES "
                                 "as we would expect %d bytes from the "
                                 "tables, but %d are available",
                                 nTemplateByteCount,
                                 nTemplateFoundByteCount);
                    }

                    free( mappds->ext );
                    free( mappds );
                }
            }
            free(pdstempl);
            free(coordlist);

            CPLFree(pabyBody);

            FindNoDataGrib2(false);
        }
    }
}

/************************************************************************/
/*                        FindNoDataGrib2()                             */
/************************************************************************/

void GRIBRasterBand::FindNoDataGrib2(bool bSeekToStart)
{
    // There is no easy way in the degrib API to retrieve the nodata value
    // without decompressing the data point section (which is slow), so
    // retrieve nodata value by parsing section 5 (Data Representation Section)
    // We also check section 6 to see if there is a bitmap
    GRIBDataset *poGDS = static_cast<GRIBDataset *>(poDS);
    CPLAssert( m_nGribVersion == 2 );

    if( m_bHasLookedForNoData )
        return;
    m_bHasLookedForNoData = true;

    if( bSeekToStart )
    {
        // Skip over section 0
        VSIFSeekL(poGDS->fp, start + 16, SEEK_SET);
    }

    GByte abyHead[5] = { 0 };
    VSIFReadL(abyHead, 5, 1, poGDS->fp);

    // Skip to section 5
    GUInt32 nSectSize = 0;
    while( abyHead[4] != 5 )
    {
        memcpy(&nSectSize, abyHead, 4);
        CPL_MSBPTR32(&nSectSize);

        if( nSectSize < 5 ||
            VSIFSeekL(poGDS->fp, nSectSize - 5, SEEK_CUR) != 0 ||
            VSIFReadL(abyHead, 5, 1, poGDS->fp) != 1 )
            break;
    }

    // See http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_sect5.shtml
    if( abyHead[4] == 5 )
    {
        memcpy(&nSectSize, abyHead, 4);
        CPL_MSBPTR32(&nSectSize);
        if( nSectSize >= 11 &&
            nSectSize <= 100000  /* arbitrary upper limit */ )
        {
            GByte *pabyBody = static_cast<GByte *>(CPLMalloc(nSectSize));
            memcpy(pabyBody, abyHead, 5);
            VSIFReadL(pabyBody + 5, 1, nSectSize - 5, poGDS->fp);

            GUInt16 nDRTN = 0;
            memcpy(&nDRTN, pabyBody + 10-1, 2);
            CPL_MSBPTR16(&nDRTN);

            GDALRasterBand::SetMetadataItem("DRS_DRTN",
                                            CPLSPrintf("%d", nDRTN),
                                            "GRIB");
            if( (nDRTN == GS5_SIMPLE ||
                 nDRTN == GS5_CMPLX ||
                 nDRTN == GS5_CMPLXSEC ||
                 nDRTN == GS5_JPEG2000 ||
                 nDRTN == GS5_PNG) && nSectSize >= 20 )
            {
                float fRef;
                memcpy(&fRef, pabyBody + 12 - 1, 4);
                CPL_MSBPTR32(&fRef);
                GDALRasterBand::SetMetadataItem("DRS_REF_VALUE",
                                                CPLSPrintf("%.10f", fRef),
                                                "GRIB");

                GUInt16 nBinaryScaleFactorUnsigned;
                memcpy(&nBinaryScaleFactorUnsigned, pabyBody + 16 - 1, 2);
                CPL_MSBPTR16(&nBinaryScaleFactorUnsigned);
                const int nBSF = (nBinaryScaleFactorUnsigned & 0x8000) ?
                    - static_cast<int>(nBinaryScaleFactorUnsigned & 0x7FFF) :
                    static_cast<int>(nBinaryScaleFactorUnsigned);
                GDALRasterBand::SetMetadataItem("DRS_BINARY_SCALE_FACTOR",
                    CPLSPrintf("%d", nBSF), "GRIB");

                GUInt16 nDecimalScaleFactorUnsigned;
                memcpy(&nDecimalScaleFactorUnsigned, pabyBody + 18 - 1, 2);
                CPL_MSBPTR16(&nDecimalScaleFactorUnsigned);
                const int nDSF = (nDecimalScaleFactorUnsigned & 0x8000) ?
                    - static_cast<int>(nDecimalScaleFactorUnsigned & 0x7FFF) :
                    static_cast<int>(nDecimalScaleFactorUnsigned);
                GDALRasterBand::SetMetadataItem("DRS_DECIMAL_SCALE_FACTOR",
                    CPLSPrintf("%d", nDSF), "GRIB");

                const int nBits = pabyBody[20-1];
                GDALRasterBand::SetMetadataItem("DRS_NBITS",
                    CPLSPrintf("%d", nBits), "GRIB");
            }

            // 2 = Grid Point Data - Complex Packing
            // 3 = Grid Point Data - Complex Packing and Spatial Differencing
            if( (nDRTN == GS5_CMPLX || nDRTN == GS5_CMPLXSEC) && nSectSize >= 31 )
            {
                const int nMiss = pabyBody[23-1];
                if( nMiss == 1 || nMiss == 2 )
                {
                    float fTemp;
                    memcpy(&fTemp, &pabyBody[24-1], 4);
                    CPL_MSBPTR32(&fTemp);
                    m_dfNoData = fTemp;
                    m_bHasNoData = true;

                    if( nMiss == 2 )
                    {
                        memcpy(&fTemp, &pabyBody[28-1], 4);
                        CPL_MSBPTR32(&fTemp);
                        double dfSecondaryNoData = fTemp;

                        // What TODO?
                        CPLDebug("GRIB",
                                 "Secondary missing value also set for band %d : %f",
                                 nBand, dfSecondaryNoData);
                    }
                }
            }

            if( nDRTN == GS5_CMPLXSEC && nSectSize >= 48 )
            {
                const int nOrder = pabyBody[48-1];
                GDALRasterBand::SetMetadataItem(
                    "DRS_SPATIAL_DIFFERENCING_ORDER",
                    CPLSPrintf("%d", nOrder), "GRIB");
            }

            CPLFree(pabyBody);
        }
        else if( nSectSize > 5 )
        {
            VSIFSeekL(poGDS->fp, nSectSize - 5, SEEK_CUR);
        }
    }

    if( !m_bHasNoData )
    {
        // Check bitmap section
        GByte abySection6[6] = { 0 };
        VSIFReadL(abySection6, 6, 1, poGDS->fp);
        // Is there a bitmap ?
        if( abySection6[4] == 6 && abySection6[5] == 0 )
        {
            m_dfNoData = 9999.0; // Same value as in metaparse.cpp:ParseGrid()
            m_bHasNoData = true;
        }
    }
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char *GRIBRasterBand::GetDescription() const
{
    if( longFstLevel == nullptr )
        return GDALPamRasterBand::GetDescription();

    return longFstLevel;
}

/************************************************************************/
/*                             LoadData()                               */
/************************************************************************/

CPLErr GRIBRasterBand::LoadData()

{
    if( !m_Grib_Data )
    {
        GRIBDataset *poGDS = static_cast<GRIBDataset *>(poDS);

        if (poGDS->bCacheOnlyOneBand)
        {
            // In "one-band-at-a-time" strategy, if the last recently used
            // band is not that one, uncache it. We could use a smarter strategy
            // based on a LRU, but that's a bit overkill for now.
            poGDS->poLastUsedBand->UncacheData();
            poGDS->nCachedBytes = 0;
        }
        else
        {
            // Once we have cached more than nCachedBytesThreshold bytes, we
            // will switch to "one-band-at-a-time" strategy, instead of caching
            // all bands that have been accessed.
            if (poGDS->nCachedBytes > poGDS->nCachedBytesThreshold)
            {
                CPLDebug("GRIB",
                         "Maximum band cache size reached for this dataset. "
                         "Caching only one band at a time from now");
                for(int i = 0; i < poGDS->nBands; i++)
                {
                    reinterpret_cast<GRIBRasterBand *>(
                        poGDS->GetRasterBand(i + 1))
                        ->UncacheData();
                }
                poGDS->nCachedBytes = 0;
                poGDS->bCacheOnlyOneBand = TRUE;
            }
        }

        FileDataSource grib_fp(poGDS->fp);

        // we don't seem to have any way to detect errors in this!
        ReadGribData(grib_fp, start, subgNum, &m_Grib_Data, &m_Grib_MetaData);
        if( !m_Grib_Data )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Out of memory.");
            if (m_Grib_MetaData != nullptr)
            {
                MetaFree(m_Grib_MetaData);
                delete m_Grib_MetaData;
                m_Grib_MetaData = nullptr;
            }
            return CE_Failure;
        }

        // Check the band matches the dataset as a whole, size wise. (#3246)
        nGribDataXSize = m_Grib_MetaData->gds.Nx;
        nGribDataYSize = m_Grib_MetaData->gds.Ny;
        if( nGribDataXSize <= 0 || nGribDataYSize <= 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Band %d of GRIB dataset is %dx%d.",
                     nBand,
                     nGribDataXSize, nGribDataYSize);
            MetaFree(m_Grib_MetaData);
            delete m_Grib_MetaData;
            m_Grib_MetaData = nullptr;
            return CE_Failure;
        }

        poGDS->nCachedBytes += static_cast<GIntBig>(nGribDataXSize) *
                               nGribDataYSize * sizeof(double);
        poGDS->poLastUsedBand = this;

        if( nGribDataXSize != nRasterXSize || nGribDataYSize != nRasterYSize )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Band %d of GRIB dataset is %dx%d, while the first band "
                     "and dataset is %dx%d.  Georeferencing of band %d may "
                     "be incorrect, and data access may be incomplete.",
                     nBand,
                     nGribDataXSize, nGribDataYSize,
                     nRasterXSize, nRasterYSize,
                     nBand);
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GRIBRasterBand::IReadBlock( int /* nBlockXOff */,
                                   int nBlockYOff,
                                   void *pImage )

{
    CPLErr eErr = LoadData();
    if (eErr != CE_None)
        return eErr;

    // The image as read is always upside down to our normal
    // orientation so we need to effectively flip it at this
    // point.  We also need to deal with bands that are a different
    // size than the dataset as a whole.

    if( nGribDataXSize == nRasterXSize && nGribDataYSize == nRasterYSize )
    {
        // Simple 1:1 case.
        memcpy(pImage,
               m_Grib_Data + static_cast<size_t>(nRasterXSize) * (nRasterYSize - nBlockYOff - 1),
               nRasterXSize * sizeof(double));

        return CE_None;
    }

    memset(pImage, 0, sizeof(double) * nRasterXSize);

    if( nBlockYOff >= nGribDataYSize )  // Off image?
        return CE_None;

    const int nCopyWords = std::min(nRasterXSize, nGribDataXSize);

    memcpy(pImage,
           m_Grib_Data + static_cast<size_t>(nGribDataXSize) * (nGribDataYSize - nBlockYOff - 1),
           nCopyWords * sizeof(double));

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GRIBRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( m_nGribVersion == 2 && !m_bHasLookedForNoData )
    {
        FindNoDataGrib2();
    }

    if( m_bHasLookedForNoData )
    {
        if( pbSuccess )
            *pbSuccess = m_bHasNoData;
        return m_dfNoData;
    }

    CPLErr eErr = LoadData();
    if (eErr != CE_None ||
        m_Grib_MetaData == nullptr ||
        m_Grib_MetaData->gridAttrib.f_miss == 0)
    {
        if (pbSuccess)
            *pbSuccess = FALSE;
        return 0;
    }

    if (m_Grib_MetaData->gridAttrib.f_miss == 2)
    {
        // What TODO?
        CPLDebug("GRIB", "Secondary missing value also set for band %d : %f",
                 nBand, m_Grib_MetaData->gridAttrib.missSec);
    }

    if (pbSuccess)
        *pbSuccess = TRUE;
    return m_Grib_MetaData->gridAttrib.missPri;
}

/************************************************************************/
/*                            ReadGribData()                            */
/************************************************************************/

void GRIBRasterBand::ReadGribData( DataSource &fp, sInt4 start, int subgNum,
                                   double **data, grib_MetaData **metaData)
{
    // Initialization, for calling the ReadGrib2Record function.
    sInt4 f_endMsg = 1;  // 1 if we read the last grid in a GRIB message, or we
                         // haven't read any messages.
    // int subgNum = 0; // The subgrid in the message that we are interested in.
    sChar f_unit = 2;       // None = 0, English = 1, Metric = 2
    double majEarth = 0.0;  // -radEarth if < 6000 ignore, otherwise use this
                            // to override the radEarth in the GRIB1 or GRIB2
                            // message.  Needed because NCEP uses 6371.2 but
                            // GRIB1 could only state 6367.47.
    double minEarth = 0.0;  // -minEarth if < 6000 ignore, otherwise use this
                            // to override the minEarth in the GRIB1 or GRIB2
                            // message.
    sChar f_SimpleVer = 4;  // Which version of the simple NDFD Weather table
                            // to use. (1 is 6/2003) (2 is 1/2004) (3 is
                            // 2/2004) (4 is 11/2004) (default 4)
    LatLon lwlf;            // Lower left corner (cookie slicing) -lwlf
    LatLon uprt;            // Upper right corner (cookie slicing) -uprt
    IS_dataType is;  // Un-parsed meta data for this GRIB2 message. As well
                     // as some memory used by the unpacker.

    lwlf.lat = -100;  // lat == -100 instructs the GRIB decoder that we don't
                      // want a subgrid

    IS_Init(&is);

    const char *pszGribNormalizeUnits =
        CPLGetConfigOption("GRIB_NORMALIZE_UNITS", "YES");
    if ( !CPLTestBool(pszGribNormalizeUnits) )
        f_unit = 0;  // Do not normalize units to metric.

    // Read GRIB message from file position "start".
    fp.DataSourceFseek(start, SEEK_SET);
    uInt4 grib_DataLen = 0;  // Size of Grib_Data.
    *metaData = new grib_MetaData();
    MetaInit(*metaData);
    const int simpWWA = 0; // seem to be unused in degrib
    ReadGrib2Record(fp, f_unit, data, &grib_DataLen, *metaData, &is, subgNum,
                    majEarth, minEarth, f_SimpleVer, simpWWA, &f_endMsg, &lwlf, &uprt);

    // No intention to show errors, just swallow it and free the memory.
    char *errMsg = errSprintf(nullptr);
    if( errMsg != nullptr )
        CPLDebug("GRIB", "%s", errMsg);
    free(errMsg);
    IS_Free(&is);
}

/************************************************************************/
/*                            UncacheData()                             */
/************************************************************************/

void GRIBRasterBand::UncacheData()
{
    if (m_Grib_Data)
        free(m_Grib_Data);
    m_Grib_Data = nullptr;
    if (m_Grib_MetaData)
    {
        MetaFree(m_Grib_MetaData);
        delete m_Grib_MetaData;
    }
    m_Grib_MetaData = nullptr;
}

/************************************************************************/
/*                           ~GRIBRasterBand()                          */
/************************************************************************/

GRIBRasterBand::~GRIBRasterBand()
{
    CPLFree(longFstLevel);
    UncacheData();
}

/************************************************************************/
/* ==================================================================== */
/*                              GRIBDataset                             */
/* ==================================================================== */
/************************************************************************/

GRIBDataset::GRIBDataset() :
    fp(nullptr),
    pszProjection(CPLStrdup("")),
    nCachedBytes(0),
    // Switch caching strategy once 100 MB threshold is reached.
    // Why 100 MB? --> Why not.
    nCachedBytesThreshold(
        static_cast<GIntBig>(atoi(CPLGetConfigOption("GRIB_CACHEMAX", "100")))
        * 1024 * 1024),
    bCacheOnlyOneBand(FALSE),
    poLastUsedBand(nullptr)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~GRIBDataset()                             */
/************************************************************************/

GRIBDataset::~GRIBDataset()

{
    FlushCache();
    if( fp != nullptr )
        VSIFCloseL(fp);

    CPLFree(pszProjection);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GRIBDataset::GetGeoTransform( double *padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GRIBDataset::GetProjectionRef() { return pszProjection; }

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int GRIBDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes < 8)
        return FALSE;

    const char *pasHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    // Does a part of what ReadSECT0(), but in a thread-safe way.
    for(int i = 0; i < poOpenInfo->nHeaderBytes - 3; i++)
    {
        if(STARTS_WITH_CI(pasHeader + i, "GRIB")
#ifdef ENABLE_TDLP
            || STARTS_WITH_CI(pasHeader + i, "TDLP")
#endif
        )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GRIBDataset::Open( GDALOpenInfo *poOpenInfo )

{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if( !Identify(poOpenInfo) )
        return nullptr;
#endif
    if( poOpenInfo->fpL == nullptr )
        return nullptr;

    // A fast "probe" on the header that is partially read in memory.
    char *buff = nullptr;
    uInt4 buffLen = 0;
    sInt4 sect0[SECT0LEN_WORD] = { 0 };
    uInt4 gribLen = 0;
    int version = 0;

    // grib is not thread safe, make sure not to cause problems
    // for other thread safe formats
    CPLMutexHolderD(&hGRIBMutex);
    MemoryDataSource mds(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes);
    if (ReadSECT0(mds, &buff, &buffLen, -1, sect0, &gribLen, &version) < 0) {
        free(buff);
        char *errMsg = errSprintf(nullptr);
        if( errMsg != nullptr && strstr(errMsg,"Ran out of file") == nullptr )
            CPLDebug("GRIB", "%s", errMsg);
        free(errMsg);
        return nullptr;
    }
    free(buff);

    // Confirm the requested access is supported.
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The GRIB driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    // Create a corresponding GDALDataset.
    GRIBDataset *poDS = new GRIBDataset();

    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    // Make an inventory of the GRIB file.
    // The inventory does not contain all the information needed for
    // creating the RasterBands (especially the x and y size), therefore
    // the first GRIB band is also read for some additional metadata.
    // The band-data that is read is stored into the first RasterBand,
    // simply so that the same portion of the file is not read twice.

    VSIFSeekL(poDS->fp, 0, SEEK_SET);

    FileDataSource grib_fp(poDS->fp);

    // Contains an GRIB2 message inventory of the file.
    gdal::grib::InventoryWrapper oInventories(grib_fp);

    if( oInventories.result() <= 0 )
    {
        char *errMsg = errSprintf(nullptr);
        if( errMsg != nullptr )
            CPLDebug("GRIB", "%s", errMsg);
        free(errMsg);

        CPLError(CE_Failure, CPLE_OpenFailed,
                 "%s is a grib file, "
                 "but no raster dataset was successfully identified.",
                 poOpenInfo->pszFilename);
        // Release hGRIBMutex otherwise we'll deadlock with GDALDataset own
        // hGRIBMutex.
        CPLReleaseMutex(hGRIBMutex);
        delete poDS;
        CPLAcquireMutex(hGRIBMutex, 1000.0);
        return nullptr;
    }

    // Create band objects.
    for (uInt4 i = 0; i < oInventories.length(); ++i)
    {
        inventoryType *psInv = oInventories.get(i);
        GRIBRasterBand *gribBand = nullptr;
        uInt4 bandNr = i + 1;

        // GRIB messages can be preceded by "garbage". GRIB2Inventory()
        // does not return the offset to the real start of the message
        GByte abyHeader[1024 + 1];
        VSIFSeekL( poDS->fp, psInv->start, SEEK_SET );
        size_t nRead = VSIFReadL( abyHeader, 1, sizeof(abyHeader)-1, poDS->fp );
        abyHeader[nRead] = 0;
        // Find the real offset of the fist message
        const char *pasHeader = reinterpret_cast<char *>(abyHeader);
        int nOffsetFirstMessage = 0;
        for(int j = 0; j < poOpenInfo->nHeaderBytes - 3; j++)
        {
            if(STARTS_WITH_CI(pasHeader + j, "GRIB")
#ifdef ENABLE_TDLP
               || STARTS_WITH_CI(pasHeader + j, "TDLP")
#endif
            )
            {
                nOffsetFirstMessage = j;
                break;
            }
        }
        psInv->start += nOffsetFirstMessage;

        if (bandNr == 1)
        {
            // Important: set DataSet extents before creating first RasterBand
            // in it.
            double *data = nullptr;
            grib_MetaData *metaData = nullptr;
            GRIBRasterBand::ReadGribData(grib_fp, 0,
                                         psInv->subgNum,
                                         &data, &metaData);
            if( data == nullptr || metaData == nullptr || metaData->gds.Nx < 1 ||
                metaData->gds.Ny < 1 )
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "%s is a grib file, "
                         "but no raster dataset was successfully identified.",
                         poOpenInfo->pszFilename);
                // Release hGRIBMutex otherwise we'll deadlock with GDALDataset
                // own hGRIBMutex.
                CPLReleaseMutex(hGRIBMutex);
                delete poDS;
                CPLAcquireMutex(hGRIBMutex, 1000.0);
                if (metaData != nullptr)
                {
                    MetaFree(metaData);
                    delete metaData;
                }
                if (data != nullptr)
                {
                    free(data);
                }
                return nullptr;
            }

            // Set the DataSet's x,y size, georeference and projection from
            // the first GRIB band.
            poDS->SetGribMetaData(metaData);
            gribBand = new GRIBRasterBand(poDS, bandNr, psInv);

            if( psInv->GribVersion == 2 )
                gribBand->FindPDSTemplate();

            gribBand->m_Grib_Data = data;
            gribBand->m_Grib_MetaData = metaData;
        }
        else
        {
            gribBand = new GRIBRasterBand(poDS, bandNr, psInv);
            if( CPLTestBool(CPLGetConfigOption("GRIB_PDS_ALL_BANDS", "ON")) )
            {
                if( psInv->GribVersion == 2 )
                    gribBand->FindPDSTemplate();
            }
        }
        poDS->SetBand(bandNr, gribBand);
    }

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);

    // Release hGRIBMutex otherwise we'll deadlock with GDALDataset own
    // hGRIBMutex.
    CPLReleaseMutex(hGRIBMutex);
    poDS->TryLoadXML();

    // Check for external overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename,
                                poOpenInfo->GetSiblingFiles());
    CPLAcquireMutex(hGRIBMutex, 1000.0);

    return poDS;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

void GRIBDataset::SetGribMetaData(grib_MetaData *meta)
{
    nRasterXSize = meta->gds.Nx;
    nRasterYSize = meta->gds.Ny;

    // Image projection.
    OGRSpatialReference oSRS;

    switch(meta->gds.projType)
    {
    case GS3_LATLON:
    case GS3_GAUSSIAN_LATLON:
        // No projection, only latlon system (geographic).
        break;
    case GS3_ROTATED_LATLON:
        CPLDebug("GRIB", "angleRotate=%f, southLat=%f, southLon=%f, poleLat=%f, poleLon=%f",
                 meta->gds.angleRotate,
                 meta->gds.southLat,
                 meta->gds.southLon,
                 meta->gds.poleLat,
                 meta->gds.poleLon);
        break;
    case GS3_MERCATOR:
        if( meta->gds.orientLon == 0.0 )
        {
            if( meta->gds.meshLat == 0.0 )
                oSRS.SetMercator(0.0, 0.0, 1.0, 0.0, 0.0);
            else
                oSRS.SetMercator2SP(meta->gds.meshLat, 0.0, 0.0, 0.0, 0.0);
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Orientation of the grid != 0 not supported");
            return;
        }
        break;
    case GS3_TRANSVERSE_MERCATOR:
        oSRS.SetTM(meta->gds.latitude_of_origin,
                   meta->gds.central_meridian,
                   std::abs(meta->gds.scaleLat1 - 0.9996) < 1e8 ?
                        0.9996 : meta->gds.scaleLat1,
                   meta->gds.x0,
                   meta->gds.y0);
        break;
    case GS3_POLAR:
        oSRS.SetPS(meta->gds.meshLat, meta->gds.orientLon, 1.0,
                   0.0, 0.0);
        break;
    case GS3_LAMBERT:
        oSRS.SetLCC(meta->gds.scaleLat1, meta->gds.scaleLat2, meta->gds.meshLat,
                    meta->gds.orientLon, 0.0, 0.0);
        break;
    case GS3_ALBERS_EQUAL_AREA:
        oSRS.SetACEA(meta->gds.scaleLat1, meta->gds.scaleLat2, meta->gds.meshLat,
                    meta->gds.orientLon, 0.0, 0.0); 
        break;

    case GS3_ORTHOGRAPHIC:

        // oSRS.SetOrthographic( 0.0, meta->gds.orientLon,
        //                       meta->gds.lon2, meta->gds.lat2);

        // oSRS.SetGEOS( meta->gds.orientLon, meta->gds.stretchFactor,
        //               meta->gds.lon2, meta->gds.lat2);

        // TODO: Hardcoded for now. How to parse the meta->gds section?
        oSRS.SetGEOS(0, 35785831, 0, 0);
        break;
    case GS3_LAMBERT_AZIMUTHAL:
        oSRS.SetLAEA(meta->gds.meshLat, meta->gds.orientLon, 0.0, 0.0);
        break;

    case GS3_EQUATOR_EQUIDIST:
        break;
    case GS3_AZIMUTH_RANGE:
        break;
    }

    const bool bHaveEarthModel =
        meta->gds.majEarth != 0.0 || meta->gds.minEarth != 0.0;
    // In meters.
    const double a = bHaveEarthModel ? meta->gds.majEarth * 1.0e3 : 6377563.396;
    const double b = bHaveEarthModel ? meta->gds.minEarth * 1.0e3 : 6356256.910;

    if (meta->gds.f_sphere)
    {
        oSRS.SetGeogCS("Coordinate System imported from GRIB file", nullptr,
                       "Sphere", a, 0.0);
    }
    else
    {
        const double fInv = a / (a - b);
            if( std::abs(a-6378137.0) < 0.01
             && std::abs(fInv-298.257223563) < 1e-9 ) // WGS84
        {
            if( meta->gds.projType == GS3_LATLON )
                oSRS.SetFromUserInput( SRS_WKT_WGS84 );
            else
            {
                oSRS.SetGeogCS("Coordinate System imported from GRIB file",
                               "WGS_1984",
                               "WGS 84", 6378137., 298.257223563);
            }
        }
        else if( std::abs(a-6378137.0) < 0.01
                && std::abs(fInv-298.257222101) < 1e-9 ) // GRS80
        {
            oSRS.SetGeogCS("Coordinate System imported from GRIB file", nullptr,
                           "GRS80", 6378137., 298.257222101);
        }
        else
        {
            oSRS.SetGeogCS("Coordinate System imported from GRIB file", nullptr,
                        "Spheroid imported from GRIB file", a, fInv);
        }
    }

    OGRSpatialReference oLL;  // Construct the "geographic" part of oSRS.
    oLL.CopyGeogCSFrom(&oSRS);

    double rMinX = 0.0;
    double rMaxY = 0.0;
    double rPixelSizeX = 0.0;
    double rPixelSizeY = 0.0;
    if (meta->gds.projType == GS3_ORTHOGRAPHIC)
    {
        // This is what should work, but it doesn't. Dx seems to have an
        // inverse relation with pixel size.
        // rMinX = -meta->gds.Dx * (meta->gds.Nx / 2);
        // rMaxY = meta->gds.Dy * (meta->gds.Ny / 2);
        // Hardcoded for now, assumption: GEOS projection, full disc (like MSG).
        const double geosExtentInMeters = 11137496.552;
        rMinX = -(geosExtentInMeters / 2);
        rMaxY = geosExtentInMeters / 2;
        rPixelSizeX = geosExtentInMeters / meta->gds.Nx;
        rPixelSizeY = geosExtentInMeters / meta->gds.Ny;
    }
    else if( meta->gds.projType == GS3_TRANSVERSE_MERCATOR )
    {
        rMinX = meta->gds.x1;
        rMaxY = meta->gds.y2;
        rPixelSizeX = meta->gds.Dx;
        rPixelSizeY = meta->gds.Dy;
    }
    else if( oSRS.IsProjected() )
    {
        // Longitude in degrees, to be transformed to meters (or degrees in
        // case of latlon).
        rMinX = meta->gds.lon1;
        // Latitude in degrees, to be transformed to meters.
        rMaxY = meta->gds.lat1;
        OGRCoordinateTransformation *poTransformLLtoSRS =
            OGRCreateCoordinateTransformation(&(oLL), &(oSRS));
        // Transform it to meters.
        if( (poTransformLLtoSRS != nullptr) &&
            poTransformLLtoSRS->Transform(1, &rMinX, &rMaxY) )
        {
            if (meta->gds.scan == GRIB2BIT_2)  // Y is minY, GDAL wants maxY.
            {
                // -1 because we GDAL needs the coordinates of the centre of
                // the pixel.
                rMaxY += (meta->gds.Ny - 1) * meta->gds.Dy;
            }
            rPixelSizeX = meta->gds.Dx;
            rPixelSizeY = meta->gds.Dy;
        }
        else
        {
            rMinX = 0.0;
            rMaxY = 0.0;

            rPixelSizeX = 1.0;
            rPixelSizeY = -1.0;

            oSRS.Clear();

            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unable to perform coordinate transformations, so the "
                     "correct projected geotransform could not be deduced "
                     "from the lat/long control points.  "
                     "Defaulting to ungeoreferenced.");
        }
        delete poTransformLLtoSRS;
    }
    else
    {
        // Longitude in degrees, to be transformed to meters (or degrees in
        // case of latlon).
        rMinX = meta->gds.lon1;
        // Latitude in degrees, to be transformed to meters.
        rMaxY = meta->gds.lat1;

        double rMinY = meta->gds.lat2;
        if (meta->gds.lat2 > rMaxY)
        {
            rMaxY = meta->gds.lat2;
            rMinY = meta->gds.lat1;
        }

        if( meta->gds.Nx == 1 )
            rPixelSizeX = meta->gds.Dx;
        else if (meta->gds.lon1 > meta->gds.lon2)
            rPixelSizeX = (360.0 - (meta->gds.lon1 - meta->gds.lon2)) /
                          (meta->gds.Nx - 1);
        else
            rPixelSizeX =
                (meta->gds.lon2 - meta->gds.lon1) / (meta->gds.Nx - 1);

        if( meta->gds.Ny == 1 )
            rPixelSizeY = meta->gds.Dy;
        else
            rPixelSizeY = (rMaxY - rMinY) / (meta->gds.Ny - 1);

        // Do some sanity checks for cases that can't be handled by the above
        // pixel size corrections. GRIB1 has a minimum precision of 0.001
        // for latitudes and longitudes, so we'll allow a bit higher than that.
        if (rPixelSizeX < 0 || fabs(rPixelSizeX - meta->gds.Dx) > 0.002)
            rPixelSizeX = meta->gds.Dx;

        if (rPixelSizeY < 0 || fabs(rPixelSizeY - meta->gds.Dy) > 0.002)
            rPixelSizeY = meta->gds.Dy;

        // Longitude origin of GRIB files is sometimes funny. Try to shift as close
        // as possible to the traditional [-180,180] longitude range
        // See https://trac.osgeo.org/gdal/ticket/7103
        if( ((rMinX >= 179 && rPixelSizeX * meta->gds.Nx > 10) || rMinX >= 180) &&
            CPLTestBool(CPLGetConfigOption("GRIB_ADJUST_LONGITUDE_RANGE", "YES")) )
        {
            CPLDebug("GRIB", "Adjusting longitude origin from %f to %f",
                     rMinX - rPixelSizeX / 2, rMinX - rPixelSizeX / 2 - 360 );
            rMinX -= 360;
        }
    }

    // http://gdal.org/gdal_datamodel.html :
    //   we need the top left corner of the top left pixel.
    //   At the moment we have the center of the pixel.
    rMinX -= rPixelSizeX / 2;
    rMaxY += rPixelSizeY / 2;

    adfGeoTransform[0] = rMinX;
    adfGeoTransform[3] = rMaxY;
    adfGeoTransform[1] = rPixelSizeX;
    adfGeoTransform[5] = -rPixelSizeY;

    if( meta->gds.projType == GS3_ROTATED_LATLON &&
        meta->gds.angleRotate == 0 )
    {
        oSRS.SetProjection( "Rotated_pole" );
        oSRS.SetExtension(
            "PROJCS", "PROJ4",
            CPLSPrintf("+proj=ob_tran +lon_0=%.18g +o_proj=longlat +o_lon_p=0 "
                       "+o_lat_p=%.18g +a=%.18g +b=%.18g +to_meter=0.0174532925199 +wktext",
                       meta->gds.southLon, -meta->gds.southLat, a, b));
    }

    CPLFree(pszProjection);
    pszProjection = nullptr;
    oSRS.exportToWkt(&pszProjection);
}

/************************************************************************/
/*                       GDALDeregister_GRIB()                          */
/************************************************************************/

static void GDALDeregister_GRIB( GDALDriver * )
{
    if( hGRIBMutex != nullptr )
    {
        CPLDestroyMutex(hGRIBMutex);
        hGRIBMutex = nullptr;
    }
}

/************************************************************************/
/*                          GDALGRIBDriver                              */
/************************************************************************/

class GDALGRIBDriver: public GDALDriver
{
    bool bHasFullInitMetadata;
    CPLStringList aosMetadata;

  public:
    GDALGRIBDriver();

    char** GetMetadata(const char* pszDomain) override;
    const char* GetMetadataItem(
        const char* pszName, const char* pszDomain) override;
    CPLErr SetMetadataItem(
        const char* pszName, const char* pszValue,
        const char* pszDomain) override;
};

/************************************************************************/
/*                          GDALGRIBDriver()                            */
/************************************************************************/

GDALGRIBDriver::GDALGRIBDriver() : bHasFullInitMetadata(false)
{
    aosMetadata.SetNameValue(GDAL_DCAP_RASTER, "YES");
    aosMetadata.SetNameValue(GDAL_DMD_LONGNAME, "GRIdded Binary (.grb, .grb2)");
    aosMetadata.SetNameValue(GDAL_DMD_HELPTOPIC, "frmt_grib.html");
    aosMetadata.SetNameValue(GDAL_DMD_EXTENSIONS, "grb grb2 grib2");
    aosMetadata.SetNameValue(GDAL_DCAP_VIRTUALIO, "YES");

    aosMetadata.SetNameValue( GDAL_DMD_CREATIONDATATYPES,
                            "Byte UInt16 Int16 UInt32 Int32 Float32 "
                            "Float64" );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** GDALGRIBDriver::GetMetadata(const char* pszDomain)
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        // Defer until necessary the setting of the CreationOptionList
        // to let a chance to JPEG2000 drivers to have been loaded.
        if( !bHasFullInitMetadata )
        {
            bHasFullInitMetadata = true;

            std::vector<CPLString> aosJ2KDrivers;
            for( size_t i = 0; i < CPL_ARRAYSIZE(apszJ2KDrivers); i++ )
            {
                if( GDALGetDriverByName(apszJ2KDrivers[i]) != nullptr )
                {
                    aosJ2KDrivers.push_back(apszJ2KDrivers[i]);
                }
            }

            CPLString osCreationOptionList(
"<CreationOptionList>"
"   <Option name='DATA_ENCODING' type='string-select' default='AUTO' "
    "description='How data is encoded internally'>"
"       <Value>AUTO</Value>"
"       <Value>SIMPLE_PACKING</Value>"
"       <Value>COMPLEX_PACKING</Value>"
"       <Value>IEEE_FLOATING_POINT</Value>");
            if( GDALGetDriverByName("PNG") != nullptr )
                osCreationOptionList +=
"       <Value>PNG</Value>";
            if( !aosJ2KDrivers.empty() )
                osCreationOptionList +=
"       <Value>JPEG2000</Value>";
            osCreationOptionList += 
"   </Option>"
"   <Option name='NBITS' type='int' default='0' "
    "description='Number of bits per value'/>"
"   <Option name='DECIMAL_SCALE_FACTOR' type='int' default='0' "
    "description='Value such that raw values are multiplied by "
    "10^DECIMAL_SCALE_FACTOR before integer encoding'/>"
"   <Option name='SPATIAL_DIFFERENCING_ORDER' type='int' default='0' "
    "description='Order of spatial difference' min='0' max='2'/>";
            if( !aosJ2KDrivers.empty() )
            {
                osCreationOptionList +=
"   <Option name='COMPRESSION_RATIO' type='int' default='1' min='1' max='100'"
    "description='N:1 target compression ratio for JPEG2000'/>"
"   <Option name='JPEG2000_DRIVER' type='string-select' "
    "description='Explicitly select a JPEG2000 driver'>";
                for( size_t i = 0; i < aosJ2KDrivers.size(); i++ )
                {
                    osCreationOptionList +=
"       <Value>" + aosJ2KDrivers[i] + "</Value>";
                }
                osCreationOptionList +=
"   </Option>";
            }
            osCreationOptionList +=
"   <Option name='DISCIPLINE' type='int' "
        "description='Discipline of the processed data'/>"
"   <Option name='IDS' type='string' "
        "description='String equivalent to the GRIB_IDS metadata item'/>"
"   <Option name='IDS_CENTER' type='int' "
        "description='Originating/generating center'/>"
"   <Option name='IDS_SUBCENTER' type='int' "
        "description='Originating/generating subcenter'/>"
"   <Option name='IDS_MASTER_TABLE' type='int' "
        "description='GRIB master tables version number'/>"
"   <Option name='IDS_SIGNF_REF_TIME' type='int' "
        "description='Significance of Reference Time'/>"
"   <Option name='IDS_REF_TIME' type='string' "
        "description='Reference time as YYYY-MM-DDTHH:MM:SSZ'/>"
"   <Option name='IDS_PROD_STATUS' type='int' "
        "description='Production Status of Processed data'/>"
"   <Option name='IDS_TYPE' type='int' "
        "description='Type of processed data'/>"
"   <Option name='PDS_PDTN' type='int' "
        "description='Product Definition Template Number'/>"
"   <Option name='PDS_TEMPLATE_NUMBERS' type='string' "
        "description='Product definition template raw numbers'/>"
"   <Option name='PDS_TEMPLATE_ASSEMBLED_VALUES' type='string' "
        "description='Product definition template assembled values'/>"
"   <Option name='INPUT_UNIT' type='string' "
        "description='Unit of input values. Only for temperatures. C or K'/>"
"   <Option name='BAND_*' type='string' "
    "description='Override options at band level'/>"
"</CreationOptionList>";

            aosMetadata.SetNameValue( GDAL_DMD_CREATIONOPTIONLIST,
                                      osCreationOptionList );
        }
        return aosMetadata.List();
    }
    return nullptr;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char* GDALGRIBDriver::GetMetadataItem(const char* pszName,
                                            const char* pszDomain)
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        // Defer until necessary the setting of the CreationOptionList
        // to let a chance to JPEG2000 drivers to have been loaded.
        if( !EQUAL( pszName, GDAL_DMD_CREATIONOPTIONLIST ) )
            return CSLFetchNameValue(aosMetadata, pszName);
    }
    return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALGRIBDriver::SetMetadataItem(
                const char* pszName, const char* pszValue,
                const char* pszDomain)
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        aosMetadata.SetNameValue(pszName, pszValue);
        return CE_None;
    }
    return GDALDriver::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                         GDALRegister_GRIB()                          */
/************************************************************************/

void GDALRegister_GRIB()

{
    if( GDALGetDriverByName("GRIB") != nullptr )
        return;

    GDALDriver *poDriver = new GDALGRIBDriver();

    poDriver->SetDescription("GRIB");

    poDriver->pfnOpen = GRIBDataset::Open;
    poDriver->pfnIdentify = GRIBDataset::Identify;
    poDriver->pfnCreateCopy = GRIBDataset::CreateCopy;
    poDriver->pfnUnloadDriver = GDALDeregister_GRIB;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
