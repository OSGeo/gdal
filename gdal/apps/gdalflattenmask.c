/******************************************************************************
 * $Id: gdalflattenmask.c $
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL mask flattening utility
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    printf( "Usage: gdalflattenmask [--help-general] [-of output_format] \n"
            "                       [-co \"NAME=VALUE\"]* [-set_alpha] [-a_nodata val] \n"
            "                       srcdatasetname dstdatasetname\n"
            "\n"
            "This utility is intended to produce a new file that merges regular data\n"
            "bands with the mask bands, for applications not being able to use the mask band concept.\n"
            "* If -set_alpha is not specified, this utility will use the mask band(s)\n"
            "  to create a new dataset with empty values where the mask has null values.\n"
            "* If -set_alpha is specified, a new alpha band is added to the destination\n"
            "  dataset with the content of the global dataset mask band.\n");
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char* argv[])
{
    const char* pszFormat = "GTiff";
    const char* pszSrcFilename = NULL;
    const char* pszDstFilename = NULL;
    int i;
    int nBands, nXSize, nYSize;
    GDALDriverH hDriver;
    GDALDatasetH hSrcDS;
    GDALDatasetH hDstDS;
    char** papszCreateOptions = NULL;
    int bSetNoData = FALSE;
    double dfDstNoData = 0;
    int bSetAlpha = FALSE;
    double adfGeoTransform[6];
    const char* pszProjectionRef;
    GByte* pabyMaskBuffer;
    char** papszMetadata;

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "-of") && i + 1 < argc)
        {
            pszFormat = argv[++i];
        }
        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }
        else if( EQUAL(argv[i],"-a_nodata") && i < argc - 1 )
        {
            bSetNoData = TRUE;
            dfDstNoData = atof(argv[++i]);
        }
        else if( EQUAL(argv[i], "-set_alpha"))
        {
            bSetAlpha = TRUE;
        }
        else if ( argv[i][0] == '-')
            Usage();
        else if( pszSrcFilename == NULL )
            pszSrcFilename = argv[i];
        else if(  pszDstFilename == NULL )
            pszDstFilename = argv[i];
        else
            Usage();
    }

    if( pszSrcFilename == NULL || pszDstFilename == NULL)
        Usage();

/* -------------------------------------------------------------------- */
/*      Open source dataset                                             */
/* -------------------------------------------------------------------- */
    hSrcDS = GDALOpen(pszSrcFilename, GA_ReadOnly);
    if (hSrcDS == NULL)
    {
        fprintf(stderr, "Can't open %s\n", pszSrcFilename);
        exit(1);
    }

    nBands = GDALGetRasterCount(hSrcDS);
    nXSize = GDALGetRasterXSize(hSrcDS);
    nYSize = GDALGetRasterYSize(hSrcDS);

    for(i = 0; i < nBands; i++)
    {
        GDALRasterBandH hSrcBand = GDALGetRasterBand(hSrcDS, i+1);
        GDALDataType  eDataType = GDALGetRasterDataType(hSrcBand);

        if (bSetAlpha)
        {
            if (nBands > 1 && (GDALGetMaskFlags(hSrcBand) & GMF_PER_DATASET) == 0)
            {
                fprintf(stderr, "When -set_alpha is specified, all source bands must "
                                "share the same mask band (PER_DATASET mask)\n");
                exit(1);
            }
            if (GDALGetRasterColorInterpretation(hSrcBand) == GCI_AlphaBand)
            {
                fprintf(stderr, "The source dataset has already an alpha band\n");
                exit(1);
            }
        }

        if (eDataType != GDT_Byte)
        {
            fprintf(stderr, "Only GDT_Byte type supported for source dataset\n");
            exit(1);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create destination dataset                                      */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName(pszFormat);
    if (hDriver == NULL)
    {
        fprintf(stderr, "Can't find driver %s\n", pszFormat);
        exit(1);
    }

    hDstDS = GDALCreate(hDriver,
                        pszDstFilename,
                        nXSize,
                        nYSize,
                        nBands + ((bSetAlpha) ? 1 : 0),
                        GDT_Byte,
                        papszCreateOptions);
    if (hDstDS == NULL)
    {
        fprintf(stderr, "Can't create %s\n", pszDstFilename);
        exit(1);
    }

/* -------------------------------------------------------------------- */
/*      Write geotransform, projection, color interpretations, no data  */
/*      values, color tables, metadata, etc. before the file is         */
/*       crystalized                                                    */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hSrcDS, adfGeoTransform ) == CE_None )
    {
        GDALSetGeoTransform( hDstDS, adfGeoTransform );
    }

    pszProjectionRef = GDALGetProjectionRef( hSrcDS );
    if (pszProjectionRef && pszProjectionRef[0])
    {
        GDALSetProjection( hDstDS, pszProjectionRef );
    }

    if (bSetAlpha)
    {
        GDALRasterBandH hDstAlphaBand = GDALGetRasterBand(hDstDS, nBands+1);
        GDALSetRasterColorInterpretation( hDstAlphaBand, GCI_AlphaBand);
    }

    papszMetadata = GDALGetMetadata(hSrcDS, NULL);
    GDALSetMetadata(hDstDS, papszMetadata, NULL);

    for(i = 0; i < nBands; i++)
    {
        GDALRasterBandH hSrcBand, hDstBand;
        GDALColorTableH hColorTable;
        GDALColorInterp eColorInterpretation;
        int bHasNoData;
        double dfNoDataValue;

        hSrcBand = GDALGetRasterBand(hSrcDS, i+1);
        hDstBand = GDALGetRasterBand(hDstDS, i+1);

        dfNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bHasNoData);
        if (!bHasNoData)
            dfNoDataValue = dfDstNoData;
        if (!bSetAlpha && (bHasNoData || bSetNoData))
            GDALSetRasterNoDataValue(hDstBand, dfNoDataValue);

        hColorTable = GDALGetRasterColorTable( hSrcBand );
        if (hColorTable)
        {
            GDALSetRasterColorTable(hDstBand, hColorTable);
        }

        papszMetadata = GDALGetMetadata(hSrcBand, NULL);
        GDALSetMetadata(hDstBand, papszMetadata, NULL);

        eColorInterpretation = GDALGetRasterColorInterpretation( hSrcBand );
        GDALSetRasterColorInterpretation( hDstBand, eColorInterpretation );
    }

/* -------------------------------------------------------------------- */
/*      Write the data values now                                       */
/* -------------------------------------------------------------------- */
    pabyMaskBuffer = (GByte*)CPLMalloc(nXSize);

    for(i = 0; i < nBands; i++)
    {
        GDALRasterBandH hSrcBand, hDstBand, hMaskBand;
        GDALDataType  eDataType;
        GByte* pabyBuffer;
        int iCol, iLine;
        int bHasNoData;
        double dfNoDataValue;
        int nMaskFlag;

        hSrcBand = GDALGetRasterBand(hSrcDS, i+1);
        hDstBand = GDALGetRasterBand(hDstDS, i+1);
        hMaskBand = GDALGetMaskBand(hSrcBand);
        nMaskFlag = GDALGetMaskFlags(hSrcBand);

        eDataType = GDALGetRasterDataType(hSrcBand);
        pabyBuffer = (GByte*)CPLMalloc(nXSize * GDALGetDataTypeSize(eDataType));
        dfNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bHasNoData);
        if (!bHasNoData)
            dfNoDataValue = dfDstNoData;

        for(iLine = 0; iLine < nYSize; iLine++)
        {
            GDALRasterIO( hSrcBand, GF_Read, 0, iLine, nXSize, 1,
                          pabyBuffer, nXSize, 1, eDataType, 0, 0);
            if (!bSetAlpha)
            {
                GDALRasterIO( hMaskBand, GF_Read, 0, iLine, nXSize, 1,
                              pabyMaskBuffer, nXSize, 1, GDT_Byte, 0, 0);
                switch (eDataType)
                {
                    case GDT_Byte:
                    {
                        for(iCol = 0; iCol < nXSize; iCol++)
                        {
                            /* If the mask is 1-bit and the value is 0,
                               or if the mask is 8-bit and the value < 128,
                               then replace the value of the pixel by the transparent value */
                            if (pabyMaskBuffer[iCol] == 0 ||
                                ((nMaskFlag & GMF_ALPHA) != 0 && pabyMaskBuffer[iCol] < 128))
                                pabyBuffer[iCol] = (GByte)dfNoDataValue;
                        }
                        break;
                    }

                    default:
                        CPLAssert(0);
                        break;
                }
            }

            GDALRasterIO( hDstBand, GF_Write, 0, iLine, nXSize, 1,
                          pabyBuffer, nXSize, 1, eDataType, 0, 0);
        }

        CPLFree(pabyBuffer);
    }

/* -------------------------------------------------------------------- */
/*      Create the alpha band if -set_alpha is specified                */
/* -------------------------------------------------------------------- */
    if (bSetAlpha)
    {
        GDALRasterBandH hSrcBand = GDALGetRasterBand(hSrcDS, 1);
        GDALRasterBandH hDstAlphaBand = GDALGetRasterBand(hDstDS, nBands+1);
        GDALRasterBandH hMaskBand = GDALGetMaskBand(hSrcBand);
        int nMaskFlag = GDALGetMaskFlags(hSrcBand);

        int iCol, iLine;
        for(iLine = 0; iLine < nYSize; iLine++)
        {
            GDALRasterIO( hMaskBand, GF_Read, 0, iLine, nXSize, 1,
                          pabyMaskBuffer, nXSize, 1, GDT_Byte, 0, 0);
            for(iCol = 0; iCol < nXSize; iCol++)
            {
                /* If the mask is 1-bit, expand 1 to 255 */
                if (pabyMaskBuffer[iCol] == 1 && (nMaskFlag & GMF_ALPHA) == 0)
                    pabyMaskBuffer[iCol] = 255;
            }
            GDALRasterIO( hDstAlphaBand, GF_Write, 0, iLine, nXSize, 1,
                          pabyMaskBuffer, nXSize, 1, GDT_Byte, 0, 0);
        }
    }

/************************************************************************/
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree(pabyMaskBuffer);

    GDALClose(hSrcDS);
    GDALClose(hDstDS);
    GDALDumpOpenDatasets( stderr );
    GDALDestroyDriverManager();
    CSLDestroy( argv );
    CSLDestroy( papszCreateOptions );

    return 0;
}
