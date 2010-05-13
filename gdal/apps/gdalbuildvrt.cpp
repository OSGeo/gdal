/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to build VRT datasets from raster products or content of SHP tile index
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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

#include "gdal_proxy.h"
#include "cpl_string.h"
#include "vrt/gdal_vrt.h"

#ifdef OGR_ENABLED
#include "ogr_api.h"
#endif

CPL_CVSID("$Id$");

#define GEOTRSFRM_TOPLEFT_X            0
#define GEOTRSFRM_WE_RES               1
#define GEOTRSFRM_ROTATION_PARAM1      2
#define GEOTRSFRM_TOPLEFT_Y            3
#define GEOTRSFRM_ROTATION_PARAM2      4
#define GEOTRSFRM_NS_RES               5

typedef enum
{
    LOWEST_RESOLUTION,
    HIGHEST_RESOLUTION,
    AVERAGE_RESOLUTION,
    USER_RESOLUTION
} ResolutionStrategy;

typedef struct
{
    int    isFileOK;
    int    nRasterXSize;
    int    nRasterYSize;
    double adfGeoTransform[6];
    int    nBlockXSize;
    int    nBlockYSize;
    GDALDataType firstBandType;
    int*         panHasNoData;
    double*      padfNoDataValues;
} DatasetProperty;

typedef struct
{
    GDALColorInterp        colorInterpretation;
    GDALDataType           dataType;
    GDALColorTableH        colorTable;
    int                    bHasNoData;
    double                 noDataValue;
} BandProperty;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stdout, "%s", 
            "Usage: gdalbuildvrt [-tileindex field_name] [-resolution {highest|lowest|average|user}]\n"
            "                    [-tr xres yres] [-separate] [-allow_projection_difference] [-q]\n"
            "                    [-te xmin ymin xmax ymax] [-addalpha] [-hidenodata] \n"
            "                    [-srcnodata \"value [value...]\"] [-vrtnodata \"value [value...]\"] \n"
            "                    [-input_file_list my_liste.txt] output.vrt [gdalfile]*\n"
            "\n"
            "eg.\n"
            "  % gdalbuildvrt doq_index.vrt doq/*.tif\n"
            "  % gdalbuildvrt -input_file_list my_liste.txt doq_index.vrt\n"
            "\n"
            "NOTES:\n"
            "  o With -separate, each files goes into a separate band in the VRT band. Otherwise,\n"
            "    the files are considered as tiles of a larger mosaic.\n"
            "  o The default tile index field is 'location' unless otherwise specified by -tileindex.\n"
            "  o In case the resolution of all input files is not the same, the -resolution flag.\n"
            "    enable the user to control the way the output resolution is computed. average is the default.\n"
            "  o Input files may be any valid GDAL dataset or a GDAL raster tile index.\n"
            "  o For a GDAL raster tile index, all entries will be added to the VRT.\n"
            "  o If one GDAL dataset is made of several subdatasets and has 0 raster bands, its\n"
            "    datasets will be added to the VRT rather than the dataset itself.\n"
            "  o By default, only datasets of same projection and band characteristics may be added to the VRT.\n"
            );
    exit( 1 );
}

    
int  GetSrcDstWin(DatasetProperty* psDP,
                  double we_res, double ns_res,
                  double minX, double minY, double maxX, double maxY,
                  int* pnSrcXOff, int* pnSrcYOff, int* pnSrcXSize, int* pnSrcYSize,
                  int* pnDstXOff, int* pnDstYOff, int* pnDstXSize, int* pnDstYSize)
{
    /* Check that the destination bounding box intersects the source bounding box */
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
         psDP->nRasterXSize *
         psDP->adfGeoTransform[GEOTRSFRM_WE_RES] < minX )
         return FALSE;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] > maxX )
         return FALSE;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
         psDP->nRasterYSize *
         psDP->adfGeoTransform[GEOTRSFRM_NS_RES] > maxY )
         return FALSE;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] < minY )
         return FALSE;

    *pnSrcXSize = psDP->nRasterXSize;
    *pnSrcYSize = psDP->nRasterYSize;
    if ( psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] < minX )
    {
        *pnSrcXOff = (int)((minX - psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X]) /
            psDP->adfGeoTransform[GEOTRSFRM_WE_RES] + 0.5);
        *pnDstXOff = 0;
    }
    else
    {
        *pnSrcXOff = 0;
        *pnDstXOff = (int)
            (0.5 + (psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_X] - minX) / we_res);
    }
    if ( maxY < psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y])
    {
        *pnSrcYOff = (int)((psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] - maxY) /
            -psDP->adfGeoTransform[GEOTRSFRM_NS_RES] + 0.5);
        *pnDstYOff = 0;
    }
    else
    {
        *pnSrcYOff = 0;
        *pnDstYOff = (int)
            (0.5 + (maxY - psDP->adfGeoTransform[GEOTRSFRM_TOPLEFT_Y]) / -ns_res);
    }
    *pnDstXSize = (int)
        (0.5 + psDP->nRasterXSize *
         psDP->adfGeoTransform[GEOTRSFRM_WE_RES] / we_res);
    *pnDstYSize = (int)
        (0.5 + psDP->nRasterYSize *
         psDP->adfGeoTransform[GEOTRSFRM_NS_RES] / ns_res);
         
    return TRUE;
}

/************************************************************************/
/*                         GDALBuildVRT()                               */
/************************************************************************/

CPLErr GDALBuildVRT( const char* pszOutputFilename,
                     int* pnInputFiles, char*** pppszInputFilenames,
                     ResolutionStrategy resolutionStrategy,
                     double we_res, double ns_res,
                     double minX, double minY, double maxX, double maxY,
                     int bSeparate, int bAllowProjectionDifference,
                     int bAddAlpha, int bHideNoData,
                     const char* pszSrcNoData, const char* pszVRTNoData,
                     GDALProgressFunc pfnProgress, void * pProgressData)
{
    char* projectionRef = NULL;
    int nBands = 0;
    BandProperty* bandProperties = NULL;
    int i,j;
    int rasterXSize;
    int rasterYSize;
    int nCount = 0;
    int bFirst = TRUE;
    VRTDatasetH hVRTDS = NULL;
    CPLErr eErr = CE_None;
    int bHasGeoTransform = FALSE;
    
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;
        
    int bUserExtent = (minX != 0 || minY != 0 || maxX != 0 || maxY != 0);
    if (bUserExtent)
    {
        if (minX >= maxX || minY >= maxY )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid user extent");
            return CE_Failure;
        }
    }
        
    if (resolutionStrategy == USER_RESOLUTION)
    {
        if (we_res <= 0 || ns_res <= 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Invalid user resolution");
            return CE_Failure;
        }
        
        /* We work with negative north-south resolution in all the following code */
        ns_res = -ns_res;
    }
    else
    {
        we_res = ns_res = 0;
    }

    char** ppszInputFilenames = *pppszInputFilenames;
    int nInputFiles = *pnInputFiles;

    DatasetProperty* psDatasetProperties =
            (DatasetProperty*) CPLCalloc(nInputFiles, sizeof(DatasetProperty));
            
    
    int bAllowSrcNoData = TRUE;
    double* padfSrcNoData = NULL;
    int nSrcNoDataCount = 0;
    if (pszSrcNoData != NULL)
    {
        if (EQUAL(pszSrcNoData, "none"))
        {
            bAllowSrcNoData = FALSE;
        }
        else
        {
            char **papszTokens = CSLTokenizeString( pszSrcNoData );
            nSrcNoDataCount = CSLCount(papszTokens);
            padfSrcNoData = (double *) CPLMalloc(sizeof(double) * nSrcNoDataCount);
            for(i=0;i<nSrcNoDataCount;i++)
                padfSrcNoData[i] = CPLAtofM(papszTokens[i]);
            CSLDestroy(papszTokens);
        }
    }
    
    
    int bAllowVRTNoData = TRUE;
    double* padfVRTNoData = NULL;
    int nVRTNoDataCount = 0;
    if (pszVRTNoData != NULL)
    {
        if (EQUAL(pszVRTNoData, "none"))
        {
            bAllowVRTNoData = FALSE;
        }
        else
        {
            char **papszTokens = CSLTokenizeString( pszVRTNoData );
            nVRTNoDataCount = CSLCount(papszTokens);
            padfVRTNoData = (double *) CPLMalloc(sizeof(double) * nVRTNoDataCount);
            for(i=0;i<nVRTNoDataCount;i++)
                padfVRTNoData[i] = CPLAtofM(papszTokens[i]);
            CSLDestroy(papszTokens);
        }
    }

    int nRasterXSize = 0, nRasterYSize = 0;

    for(i=0;i<nInputFiles;i++)
    {
        const char* dsFileName = ppszInputFilenames[i];

        if (!pfnProgress( 1.0 * (i+1) / nInputFiles, NULL, pProgressData))
        {
            eErr = CE_Failure;
            goto end;
        }

        GDALDatasetH hDS = GDALOpen(ppszInputFilenames[i], GA_ReadOnly );
        psDatasetProperties[i].isFileOK = FALSE;

        if (hDS)
        {
            char** papszMetadata = GDALGetMetadata( hDS, "SUBDATASETS" );
            if( CSLCount(papszMetadata) > 0 && GDALGetRasterCount(hDS) == 0 )
            {
                psDatasetProperties =
                    (DatasetProperty*) CPLMalloc((nInputFiles+CSLCount(papszMetadata))*sizeof(DatasetProperty));

                ppszInputFilenames = (char**)CPLRealloc(ppszInputFilenames,
                                        sizeof(char*) * (nInputFiles+CSLCount(papszMetadata)));
                int count = 1;
                char subdatasetNameKey[256];
                sprintf(subdatasetNameKey, "SUBDATASET_%d_NAME", count);
                while(*papszMetadata != NULL)
                {
                    if (EQUALN(*papszMetadata, subdatasetNameKey, strlen(subdatasetNameKey)))
                    {
                        ppszInputFilenames[nInputFiles++] =
                                CPLStrdup(*papszMetadata+strlen(subdatasetNameKey)+1);
                        count++;
                        sprintf(subdatasetNameKey, "SUBDATASET_%d_NAME", count);
                    }
                    papszMetadata++;
                }
                GDALClose(hDS);
                continue;
            }

            const char* proj = GDALGetProjectionRef(hDS);
            int bGotGeoTransform = GDALGetGeoTransform(hDS, psDatasetProperties[i].adfGeoTransform) == CE_None;
            if (bSeparate)
            {
                if (bFirst)
                {
                    bHasGeoTransform = bGotGeoTransform;
                    if (!bHasGeoTransform)
                    {
                        if (bUserExtent)
                        {
                            CPLError(CE_Warning, CPLE_NotSupported,
                                "User extent ignored by gdalbuildvrt -separate with ungeoreferenced images.");
                        }
                        if (resolutionStrategy == USER_RESOLUTION)
                        {
                            CPLError(CE_Warning, CPLE_NotSupported,
                                "User resolution ignored by gdalbuildvrt -separate with ungeoreferenced images.");
                        }
                    }
                }
                else if (bHasGeoTransform != bGotGeoTransform)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt -separate cannot stack ungeoreferenced and georeferenced images. Skipping %s",
                            dsFileName);
                    GDALClose(hDS);
                    continue;
                }
                else if (!bHasGeoTransform &&
                         (nRasterXSize != GDALGetRasterXSize(hDS) ||
                          nRasterYSize != GDALGetRasterYSize(hDS)))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt -separate cannot stack ungeoreferenced images that have not the same dimensions. Skipping %s",
                            dsFileName);
                    GDALClose(hDS);
                    continue;
                }
            }
            else
            {
                if (!bGotGeoTransform)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt does not support ungeoreferenced image. Skipping %s",
                            dsFileName);
                    GDALClose(hDS);
                    continue;
                }
                bHasGeoTransform = TRUE;
            }

            if (bGotGeoTransform)
            {
                if (psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] != 0 ||
                    psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] != 0)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt does not support rotated geo transforms. Skipping %s",
                            dsFileName);
                    GDALClose(hDS);
                    continue;
                }
                if (psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_NS_RES] >= 0)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "gdalbuildvrt does not support positive NS resolution. Skipping %s",
                            dsFileName);
                    GDALClose(hDS);
                    continue;
                }
            }

            psDatasetProperties[i].nRasterXSize = GDALGetRasterXSize(hDS);
            psDatasetProperties[i].nRasterYSize = GDALGetRasterYSize(hDS);
            if (bFirst && bSeparate && !bGotGeoTransform)
            {
                nRasterXSize = GDALGetRasterXSize(hDS);
                nRasterYSize = GDALGetRasterYSize(hDS);
            }

            double product_minX = psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_TOPLEFT_X];
            double product_maxY = psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_TOPLEFT_Y];
            double product_maxX = product_minX +
                        GDALGetRasterXSize(hDS) *
                        psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_WE_RES];
            double product_minY = product_maxY +
                        GDALGetRasterYSize(hDS) *
                        psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_NS_RES];

            GDALGetBlockSize(GDALGetRasterBand( hDS, 1 ),
                             &psDatasetProperties[i].nBlockXSize,
                             &psDatasetProperties[i].nBlockYSize);

            int _nBands = GDALGetRasterCount(hDS);
            if (_nBands == 0)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Skipping %s as it has no bands", dsFileName);
                GDALClose(hDS);
                continue;
            }
            else if (_nBands > 1 && bSeparate)
            {
                CPLError(CE_Warning, CPLE_AppDefined, "%s has %d bands. Only the first one will "
                         "be taken into account in the -separate case",
                         dsFileName, _nBands);
                _nBands = 1;
            }

            /* For the -separate case */
            psDatasetProperties[i].firstBandType = GDALGetRasterDataType(GDALGetRasterBand(hDS, 1));

            psDatasetProperties[i].padfNoDataValues = (double*)CPLMalloc(sizeof(double) * _nBands);
            psDatasetProperties[i].panHasNoData = (int*)CPLMalloc(sizeof(int) * _nBands);
            for(j=0;j<_nBands;j++)
            {
                if (nSrcNoDataCount > 0)
                {
                    psDatasetProperties[i].panHasNoData[j] = TRUE;
                    if (j < nSrcNoDataCount)
                        psDatasetProperties[i].padfNoDataValues[j] = padfSrcNoData[j];
                    else
                        psDatasetProperties[i].padfNoDataValues[j] = padfSrcNoData[nSrcNoDataCount - 1];
                }
                else
                {
                    psDatasetProperties[i].padfNoDataValues[j]  =
                        GDALGetRasterNoDataValue(GDALGetRasterBand(hDS, j+1),
                                                &psDatasetProperties[i].panHasNoData[j]);
                }
            }

            if (bFirst)
            {
                if (proj)
                    projectionRef = CPLStrdup(proj);
                if (!bUserExtent)
                {
                    minX = product_minX;
                    minY = product_minY;
                    maxX = product_maxX;
                    maxY = product_maxY;
                }
                nBands = _nBands;

                if (!bSeparate)
                {
                    bandProperties = (BandProperty*)CPLMalloc(nBands*sizeof(BandProperty));
                    for(j=0;j<nBands;j++)
                    {
                        GDALRasterBandH hRasterBand = GDALGetRasterBand( hDS, j+1 );
                        bandProperties[j].colorInterpretation =
                                GDALGetRasterColorInterpretation(hRasterBand);
                        bandProperties[j].dataType = GDALGetRasterDataType(hRasterBand);
                        if (bandProperties[j].colorInterpretation == GCI_PaletteIndex)
                        {
                            bandProperties[j].colorTable =
                                    GDALGetRasterColorTable( hRasterBand );
                            if (bandProperties[j].colorTable)
                            {
                                bandProperties[j].colorTable =
                                        GDALCloneColorTable(bandProperties[j].colorTable);
                            }
                        }
                        else
                            bandProperties[j].colorTable = 0;
                            
                        if (nVRTNoDataCount > 0)
                        {
                            bandProperties[j].bHasNoData = TRUE;
                            if (j < nVRTNoDataCount)
                                bandProperties[j].noDataValue = padfVRTNoData[j];
                            else
                                bandProperties[j].noDataValue = padfVRTNoData[nVRTNoDataCount - 1];
                        }
                        else
                        {
                            bandProperties[j].noDataValue =
                                    GDALGetRasterNoDataValue(hRasterBand, &bandProperties[j].bHasNoData);
                        }
                    }
                }
            }
            else
            {
                if ((proj != NULL && projectionRef == NULL) ||
                    (proj == NULL && projectionRef != NULL) ||
                    (proj != NULL && projectionRef != NULL && EQUAL(proj, projectionRef) == FALSE))
                {
                    if (!bAllowProjectionDifference)
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "gdalbuildvrt does not support heterogenous projection. Skipping %s",
                                 dsFileName);
                        GDALClose(hDS);
                        continue;
                    }
                }
                if (!bSeparate)
                {
                    if (nBands != _nBands)
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "gdalbuildvrt does not support heterogenous band numbers. Skipping %s",
                                dsFileName);
                        GDALClose(hDS);
                        continue;
                    }
                    for(j=0;j<nBands;j++)
                    {
                        GDALRasterBandH hRasterBand = GDALGetRasterBand( hDS, j+1 );
                        if (bandProperties[j].colorInterpretation != GDALGetRasterColorInterpretation(hRasterBand) ||
                            bandProperties[j].dataType != GDALGetRasterDataType(hRasterBand))
                        {
                            CPLError(CE_Warning, CPLE_NotSupported,
                                     "gdalbuildvrt does not support heterogenous band characteristics. Skipping %s",
                                     dsFileName);
                            GDALClose(hDS);
                        }
                        if (bandProperties[j].colorTable)
                        {
                            GDALColorTableH colorTable = GDALGetRasterColorTable( hRasterBand );
                            if (colorTable == NULL ||
                                GDALGetColorEntryCount(colorTable) != GDALGetColorEntryCount(bandProperties[j].colorTable))
                            {
                                CPLError(CE_Warning, CPLE_NotSupported,
                                         "gdalbuildvrt does not support heterogenous band characteristics. Skipping %s",
                                        dsFileName);
                                GDALClose(hDS);
                                break;
                            }
                            /* We should check that the palette are the same too ! */
                        }
                    }
                    if (j != nBands)
                        continue;
                }
                if (!bUserExtent)
                {
                    if (product_minX < minX) minX = product_minX;
                    if (product_minY < minY) minY = product_minY;
                    if (product_maxX > maxX) maxX = product_maxX;
                    if (product_maxY > maxY) maxY = product_maxY;
                }
            }
            
            if (resolutionStrategy == AVERAGE_RESOLUTION)
            {
                we_res += psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_WE_RES];
                ns_res += psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_NS_RES];
            }
            else if (resolutionStrategy != USER_RESOLUTION)
            {
                if (bFirst)
                {
                    we_res = psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_WE_RES];
                    ns_res = psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_NS_RES];
                }
                else if (resolutionStrategy == HIGHEST_RESOLUTION)
                {
                    we_res = MIN(we_res, psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_WE_RES]);
                    /* Yes : as ns_res is negative, the highest resolution is the max value */
                    ns_res = MAX(ns_res, psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_NS_RES]);
                }
                else
                {
                    we_res = MAX(we_res, psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_WE_RES]);
                    /* Yes : as ns_res is negative, the lowest resolution is the min value */
                    ns_res = MIN(ns_res, psDatasetProperties[i].adfGeoTransform[GEOTRSFRM_NS_RES]);
                }
            }

            psDatasetProperties[i].isFileOK = 1;
            nCount ++;
            bFirst = FALSE;
            GDALClose(hDS);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined, 
                     "Warning : can't open %s. Skipping it", dsFileName);
        }
    }
    
    *pppszInputFilenames = ppszInputFilenames;
    *pnInputFiles = nInputFiles;
    
    if (nCount == 0)
        goto end;
    
    if (!bHasGeoTransform)
    {
        rasterXSize = nRasterXSize;
        rasterYSize = nRasterYSize;
    }
    else
    {
        if (resolutionStrategy == AVERAGE_RESOLUTION)
        {
            we_res /= nCount;
            ns_res /= nCount;
        }
        
        rasterXSize = (int)(0.5 + (maxX - minX) / we_res);
        rasterYSize = (int)(0.5 + (maxY - minY) / -ns_res);
    }
    
    if (rasterXSize == 0 || rasterYSize == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                  "Computed VRT dimension is invalid. You've probably specified unappropriate resolution.");
        goto end;
    }
    
    hVRTDS = VRTCreate(rasterXSize, rasterYSize);
    GDALSetDescription(hVRTDS, pszOutputFilename);
    
    if (projectionRef)
    {
        GDALSetProjection(hVRTDS, projectionRef);
    }

    if (bHasGeoTransform)
    {
        double adfGeoTransform[6];
        adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = minX;
        adfGeoTransform[GEOTRSFRM_WE_RES] = we_res;
        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] = 0;
        adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = maxY;
        adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] = 0;
        adfGeoTransform[GEOTRSFRM_NS_RES] = ns_res;
        GDALSetGeoTransform(hVRTDS, adfGeoTransform);
    }
    
    if (bSeparate)
    {
        int iBand = 1;
        for(i=0;i<nInputFiles;i++)
        {
            if (psDatasetProperties[i].isFileOK == 0)
                continue;
                
            int nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                nDstXOff, nDstYOff, nDstXSize, nDstYSize;
            if (bHasGeoTransform)
            {
                if ( ! GetSrcDstWin(&psDatasetProperties[i],
                            we_res, ns_res, minX, minY, maxX, maxY,
                            &nSrcXOff, &nSrcYOff, &nSrcXSize, &nSrcYSize,
                            &nDstXOff, &nDstYOff, &nDstXSize, &nDstYSize) )
                    continue;
            }
            else
            {
                nSrcXOff = nSrcYOff = nDstXOff = nDstYOff = 0;
                nSrcXSize = nDstXSize = nRasterXSize;
                nSrcYSize = nDstYSize = nRasterYSize;
            }

            const char* dsFileName = ppszInputFilenames[i];

            GDALAddBand(hVRTDS, psDatasetProperties[i].firstBandType, NULL);

            GDALProxyPoolDatasetH hProxyDS =
                GDALProxyPoolDatasetCreate(dsFileName,
                                            psDatasetProperties[i].nRasterXSize,
                                            psDatasetProperties[i].nRasterYSize,
                                            GA_ReadOnly, TRUE, projectionRef,
                                            psDatasetProperties[i].adfGeoTransform);
            GDALProxyPoolDatasetAddSrcBandDescription(hProxyDS,
                                                psDatasetProperties[i].firstBandType,
                                                psDatasetProperties[i].nBlockXSize,
                                                psDatasetProperties[i].nBlockYSize);

            VRTSourcedRasterBandH hVRTBand =
                    (VRTSourcedRasterBandH)GDALGetRasterBand(hVRTDS, iBand);

            if (bHideNoData)
                GDALSetMetadataItem(hVRTBand,"HideNoDataValue","1",NULL);

            if (bAllowSrcNoData && psDatasetProperties[i].panHasNoData[0])
            {
                GDALSetRasterNoDataValue(hVRTBand, psDatasetProperties[i].padfNoDataValues[0]);
                VRTAddComplexSource(hVRTBand, GDALGetRasterBand((GDALDatasetH)hProxyDS, 1),
                                nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                                nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                0, 1, psDatasetProperties[i].padfNoDataValues[0]);
            }
            else
                /* Place the raster band at the right position in the VRT */
                VRTAddSimpleSource(hVRTBand, GDALGetRasterBand((GDALDatasetH)hProxyDS, 1),
                                nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                                nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                "near", VRT_NODATA_UNSET);

            GDALDereferenceDataset(hProxyDS);

            iBand ++;
        }
    }
    else
    {
        for(j=0;j<nBands;j++)
        {
            GDALRasterBandH hBand;
            GDALAddBand(hVRTDS, bandProperties[j].dataType, NULL);
            hBand = GDALGetRasterBand(hVRTDS, j+1);
            GDALSetRasterColorInterpretation(hBand, bandProperties[j].colorInterpretation);
            if (bandProperties[j].colorInterpretation == GCI_PaletteIndex)
            {
                GDALSetRasterColorTable(hBand, bandProperties[j].colorTable);
            }
            if (bAllowVRTNoData && bandProperties[j].bHasNoData)
                GDALSetRasterNoDataValue(hBand, bandProperties[j].noDataValue);
            if ( bHideNoData )
                GDALSetMetadataItem(hBand,"HideNoDataValue","1",NULL);
        }
        
        if (bAddAlpha)
        {
            GDALRasterBandH hBand;
            GDALAddBand(hVRTDS, GDT_Byte, NULL);
            hBand = GDALGetRasterBand(hVRTDS, nBands + 1);
            GDALSetRasterColorInterpretation(hBand, GCI_AlphaBand);
        }
    
        for(i=0;i<nInputFiles;i++)
        {
            if (psDatasetProperties[i].isFileOK == 0)
                continue;
    
            int nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                nDstXOff, nDstYOff, nDstXSize, nDstYSize;
            if ( ! GetSrcDstWin(&psDatasetProperties[i],
                         we_res, ns_res, minX, minY, maxX, maxY,
                         &nSrcXOff, &nSrcYOff, &nSrcXSize, &nSrcYSize,
                         &nDstXOff, &nDstYOff, &nDstXSize, &nDstYSize) )
                continue;
                
            const char* dsFileName = ppszInputFilenames[i];
    
            GDALProxyPoolDatasetH hProxyDS =
                GDALProxyPoolDatasetCreate(dsFileName,
                                            psDatasetProperties[i].nRasterXSize,
                                            psDatasetProperties[i].nRasterYSize,
                                            GA_ReadOnly, TRUE, projectionRef,
                                            psDatasetProperties[i].adfGeoTransform);
    
            for(j=0;j<nBands;j++)
            {
                GDALProxyPoolDatasetAddSrcBandDescription(hProxyDS,
                                                bandProperties[j].dataType,
                                                psDatasetProperties[i].nBlockXSize,
                                                psDatasetProperties[i].nBlockYSize);
            }
    
            for(j=0;j<nBands;j++)
            {
                VRTSourcedRasterBandH hVRTBand =
                        (VRTSourcedRasterBandH)GDALGetRasterBand(hVRTDS, j + 1);
    
                /* Place the raster band at the right position in the VRT */
                if (bAllowSrcNoData && psDatasetProperties[i].panHasNoData[j])
                    VRTAddComplexSource(hVRTBand, GDALGetRasterBand((GDALDatasetH)hProxyDS, j + 1),
                                    nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                                    nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                    0, 1, psDatasetProperties[i].padfNoDataValues[j]);
                else
                    VRTAddSimpleSource(hVRTBand, GDALGetRasterBand((GDALDatasetH)hProxyDS, j + 1),
                                    nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                                    nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                    "near", VRT_NODATA_UNSET);
            }
            
            if (bAddAlpha)
            {
                VRTSourcedRasterBandH hVRTBand =
                        (VRTSourcedRasterBandH)GDALGetRasterBand(hVRTDS, nBands + 1);
                /* Little trick : we use an offset of 255 and a scaling of 0, so that in areas covered */
                /* by the source, the value of the alpha band will be 255, otherwise it will be 0 */
                VRTAddComplexSource(hVRTBand, GDALGetRasterBand((GDALDatasetH)hProxyDS, 1),
                                    nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                                    nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                    255, 0, VRT_NODATA_UNSET);
            }
            
            GDALDereferenceDataset(hProxyDS);
        }
    }
    GDALClose(hVRTDS);

end:
    for(i=0;i<nInputFiles;i++)
    {
        CPLFree(psDatasetProperties[i].padfNoDataValues);
        CPLFree(psDatasetProperties[i].panHasNoData);
    }
    CPLFree(psDatasetProperties);
    if (!bSeparate && bandProperties != NULL)
    {
        for(j=0;j<nBands;j++)
        {
            GDALDestroyColorTable(bandProperties[j].colorTable);
        }
    }
    CPLFree(bandProperties);
    CPLFree(projectionRef);
    CPLFree(padfSrcNoData);
    CPLFree(padfVRTNoData);

    return eErr;
}

/************************************************************************/
/*                        add_file_to_list()                            */
/************************************************************************/

static void add_file_to_list(const char* filename, const char* tile_index,
                             int* pnInputFiles, char*** pppszInputFilenames)
{
#ifndef OGR_ENABLED
    CPLError(CE_Failure, CPLE_AppDefined, "OGR support needed to read tileindex");
    *pnInputFiles = 0;
    *pppszInputFilenames = NULL;
#else
    OGRDataSourceH hDS;
    OGRLayerH      hLayer;
    OGRFeatureDefnH hFDefn;
    int j, ti_field;
    
    int nInputFiles = *pnInputFiles;
    char** ppszInputFilenames = *pppszInputFilenames;
    
    if (EQUAL(CPLGetExtension(filename), "SHP"))
    {
        OGRRegisterAll();
        
        /* Handle GDALTIndex Shapefile as a special case */
        hDS = OGROpen( filename, FALSE, NULL );
        if( hDS  == NULL )
        {
            fprintf( stderr, "Unable to open shapefile `%s'.\n", 
                    filename );
            exit(2);
        }
        
        hLayer = OGR_DS_GetLayer(hDS, 0);

        hFDefn = OGR_L_GetLayerDefn(hLayer);

        for( ti_field = 0; ti_field < OGR_FD_GetFieldCount(hFDefn); ti_field++ )
        {
            OGRFieldDefnH hFieldDefn = OGR_FD_GetFieldDefn( hFDefn, ti_field );
            const char* pszName = OGR_Fld_GetNameRef(hFieldDefn);

            if (strcmp(pszName, "LOCATION") == 0 && strcmp("LOCATION", tile_index) != 0 )
            {
                fprintf( stderr, "This shapefile seems to be a tile index of "
                                "OGR features and not GDAL products.\n");
            }
            if( strcmp(pszName, tile_index) == 0 )
                break;
        }
    
        if( ti_field == OGR_FD_GetFieldCount(hFDefn) )
        {
            fprintf( stderr, "Unable to find field `%s' in DBF file `%s'.\n", 
                    tile_index, filename );
            return;
        }
    
        /* Load in memory existing file names in SHP */
        int nTileIndexFiles = OGR_L_GetFeatureCount(hLayer, TRUE);
        if (nTileIndexFiles == 0)
        {
            fprintf( stderr, "Tile index %s is empty. Skipping it.\n", filename);
            return;
        }
        
        ppszInputFilenames = (char**)CPLRealloc(ppszInputFilenames,
                              sizeof(char*) * (nInputFiles+nTileIndexFiles));
        for(j=0;j<nTileIndexFiles;j++)
        {
            OGRFeatureH hFeat = OGR_L_GetNextFeature(hLayer);
            ppszInputFilenames[nInputFiles++] =
                    CPLStrdup(OGR_F_GetFieldAsString(hFeat, ti_field ));
            OGR_F_Destroy(hFeat);
        }

        OGR_DS_Destroy( hDS );
    }
    else
    {
        ppszInputFilenames = (char**)CPLRealloc(ppszInputFilenames,
                                                 sizeof(char*) * (nInputFiles+1));
        ppszInputFilenames[nInputFiles++] = CPLStrdup(filename);
    }

    *pnInputFiles = nInputFiles;
    *pppszInputFilenames = ppszInputFilenames;
#endif
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char *tile_index = "location";
    const char *resolution = NULL;
    int nInputFiles = 0;
    char ** ppszInputFilenames = NULL;
    const char * pszOutputFilename = NULL;
    int i, iArg;
    int bSeparate = FALSE;
    int bAllowProjectionDifference = FALSE;
    int bQuiet = FALSE;
    GDALProgressFunc pfnProgress = NULL;
    double we_res = 0, ns_res = 0;
    double xmin = 0, ymin = 0, xmax = 0, ymax = 0;
    int bAddAlpha = FALSE;
    int bForceOverwrite = FALSE;
    int bHideNoData = FALSE;
    const char* pszSrcNoData = NULL;
    const char* pszVRTNoData = NULL;

    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );
    if( nArgc < 1 )
        exit( -nArgc );

/* -------------------------------------------------------------------- */
/*      Parse commandline.                                              */
/* -------------------------------------------------------------------- */
    for( iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"-tileindex") &&
                 iArg + 1 < nArgc)
        {
            tile_index = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-resolution") &&
                 iArg + 1 < nArgc)
        {
            resolution = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-input_file_list") &&
                 iArg + 1 < nArgc)
        {
            const char* input_file_list = papszArgv[++iArg];
            FILE* f = VSIFOpen(input_file_list, "r");
            if (f)
            {
                while(1)
                {
                    const char* filename = CPLReadLine(f);
                    if (filename == NULL)
                        break;
                    add_file_to_list(filename, tile_index,
                                     &nInputFiles, &ppszInputFilenames);
                }
                VSIFClose(f);
            }
        }
        else if ( EQUAL(papszArgv[iArg],"-separate") )
        {
            bSeparate = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-allow_projection_difference") )
        {
            bAllowProjectionDifference = TRUE;
        }
        /* Alternate syntax for output file */
        else if( EQUAL(papszArgv[iArg],"-o")  &&
                 iArg + 1 < nArgc)
        {
            pszOutputFilename = papszArgv[++iArg];
        }
        else if ( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet") )
        {
            bQuiet = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-tr") && iArg + 2 < nArgc)
        {
            we_res = CPLAtofM(papszArgv[++iArg]);
            ns_res = CPLAtofM(papszArgv[++iArg]);
        }
        else if ( EQUAL(papszArgv[iArg],"-te") && iArg + 4 < nArgc)
        {
            xmin = CPLAtofM(papszArgv[++iArg]);
            ymin = CPLAtofM(papszArgv[++iArg]);
            xmax = CPLAtofM(papszArgv[++iArg]);
            ymax = CPLAtofM(papszArgv[++iArg]);
        }
        else if ( EQUAL(papszArgv[iArg],"-addalpha") )
        {
            bAddAlpha = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-hidenodata") )
        {
            bHideNoData = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-overwrite") )
        {
            bForceOverwrite = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-srcnodata") && iArg + 1 < nArgc)
        {
            pszSrcNoData = papszArgv[++iArg];
        }
        else if ( EQUAL(papszArgv[iArg],"-vrtnodata") && iArg + 1 < nArgc)
        {
            pszVRTNoData = papszArgv[++iArg];
        }
        else if ( papszArgv[iArg][0] == '-' )
        {
            printf("Unrecognized option : %s\n", papszArgv[iArg]);
            exit(-1);
        }
        else if( pszOutputFilename == NULL )
        {
            pszOutputFilename = papszArgv[iArg];
        }
        else
        {
            add_file_to_list(papszArgv[iArg], tile_index,
                             &nInputFiles, &ppszInputFilenames);
        }
    }

    if( pszOutputFilename == NULL || nInputFiles == 0 )
        Usage();

    if (!bQuiet)
        pfnProgress = GDALTermProgress;
       
    /* Avoid overwriting a non VRT dataset if the user did not put the */
    /* filenames in the right order */
    VSIStatBuf sBuf;
    if (!bForceOverwrite)
    {
        int bExists = (VSIStat(pszOutputFilename, &sBuf) == 0);
        if (bExists)
        {
            GDALDriverH hDriver = GDALIdentifyDriver( pszOutputFilename, NULL );
            if (hDriver && !EQUAL(GDALGetDriverShortName(hDriver), "VRT"))
            {
                fprintf(stderr,
                        "'%s' is an existing GDAL dataset managed by %s driver.\n"
                        "There is an high chance you did not put filenames in the right order.\n"
                        "If you want to overwrite %s, add -overwrite option to the command line.\n\n",
                        pszOutputFilename, GDALGetDriverShortName(hDriver), pszOutputFilename);
                Usage();
            }
        }
    }
    
    if (we_res != 0 && ns_res != 0 &&
        resolution != NULL && !EQUAL(resolution, "user"))
    {
        fprintf(stderr, "-tr option is not compatible with -resolution %s\n", resolution);
        Usage();
    }
    
    if (bAddAlpha && bSeparate)
    {
        fprintf(stderr, "-addalpha option is not compatible with -separate\n");
        Usage();
    }
        
    ResolutionStrategy eStrategy = AVERAGE_RESOLUTION;
    if ( resolution == NULL || EQUAL(resolution, "user") )
    {
        if ( we_res != 0 || ns_res != 0)
            eStrategy = USER_RESOLUTION;
        else if ( resolution != NULL && EQUAL(resolution, "user") )
        {
            fprintf(stderr, "-tr option must be used with -resolution user\n");
            Usage();
        }
    }
    else if ( EQUAL(resolution, "average") )
        eStrategy = AVERAGE_RESOLUTION;
    else if ( EQUAL(resolution, "highest") )
        eStrategy = HIGHEST_RESOLUTION;
    else if ( EQUAL(resolution, "lowest") )
        eStrategy = LOWEST_RESOLUTION;
    else
    {
        fprintf(stderr, "invalid value (%s) for -resolution\n", resolution);
        Usage();
    }
    
    /* If -srcnodata is specified, use it as the -vrtnodata if the latter is not */
    /* specified */
    if (pszSrcNoData != NULL && pszVRTNoData == NULL)
        pszVRTNoData = pszSrcNoData;

    GDALBuildVRT(pszOutputFilename, &nInputFiles, &ppszInputFilenames,
                 eStrategy, we_res, ns_res, xmin, ymin, xmax, ymax,
                 bSeparate, bAllowProjectionDifference, bAddAlpha, bHideNoData,
                 pszSrcNoData, pszVRTNoData, pfnProgress, NULL);
    
    for(i=0;i<nInputFiles;i++)
    {
        CPLFree(ppszInputFilenames[i]);
    }
    CPLFree(ppszInputFilenames);


    CSLDestroy( papszArgv );
    GDALDumpOpenDatasets( stderr );
    GDALDestroyDriverManager();

    return 0;
}
