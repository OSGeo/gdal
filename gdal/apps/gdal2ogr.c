/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Create an OGR datasource from the values of a GDAL dataset
 *           May be useful to test gdal_grid and generate its input OGR file
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()
{
    int iDriver;
    int nDriverCount;

    printf( "Usage: gdal2ogr [--help-general] [-f format_name]\n"
            "                [-b band_number] [-l dest_layer_name]\n"
            "                [-t type]\n"
            "                gdal_datasource_src_name ogr_datasource_dst_name\n"
            "\n"
            " -f format_name: output file format name, possible values are:\n");

    nDriverCount = OGRGetDriverCount();
    for( iDriver = 0; iDriver <nDriverCount; iDriver++ )
    {
        OGRSFDriverH hDriver = OGRGetDriver(iDriver);

        if( OGR_Dr_TestCapability(hDriver, ODrCCreateDataSource ) )
            printf( "     -f \"%s\"\n", OGR_Dr_GetName(hDriver) );
    }

    printf( " -b band_number: band number of the GDAL datasource (1 by default)\n"
            " -l dest_layer_name : name of the layer created in the OGR datasource\n"
            "                      (basename of the OGR datasource by default)\n"
            " -t type: one of POINT, POINT25D (default), POLYGON\n"
            "\n"
            "Create an OGR datasource from the values of a GDAL dataset.\n\n");

    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char* argv[])
{
    const char     *pszFormat = "ESRI Shapefile";
    char           *pszLayerName = NULL;
    const char     *pszSrcFilename = NULL, *pszDstFilename = NULL;
    int             iBand = 1;
    GDALDatasetH    hDS;
    GDALRasterBandH hBand;
    int             nXSize, nYSize;
    int             i, j;
    FILE           *fOut = NULL;
    double         *padfBuffer;
    double          adfGeotransform[6];
    OGRSFDriverH    hOGRDriver;
    OGRDataSourceH  hOGRDS;
    OGRLayerH       hOGRLayer;
    OGRwkbGeometryType eType = wkbPoint25D;
    int             xStep = 1, yStep = 1;

    OGRRegisterAll();
    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if ( EQUAL(argv[i], "-b") && i < argc - 1)
            iBand = atoi(argv[++i]);
        else if ( EQUAL(argv[i], "-f") && i < argc - 1)
            pszFormat = argv[++i];
        else if ( EQUAL(argv[i], "-l") && i < argc - 1)
            pszLayerName = CPLStrdup(argv[++i]);
        else if ( EQUAL(argv[i], "-t") && i < argc - 1)
        {
            i++;
            if (EQUAL(argv[i], "POLYGON"))
                eType = wkbPolygon;
            else if (EQUAL(argv[i], "POINT"))
                eType = wkbPoint;
            else if (EQUAL(argv[i], "POINT25D"))
                eType = wkbPoint25D;
            else
            {
                fprintf(stderr, "unhandled geometry type : %s\n", argv[i]);
            }
        }
        else if ( EQUAL(argv[i], "-step") && i < argc - 1)
            xStep = yStep = atoi(argv[++i]);
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
/*      Open GDAL source dataset                                        */
/* -------------------------------------------------------------------- */
    hDS = GDALOpen(pszSrcFilename, GA_ReadOnly);
    if (hDS == NULL)
    {
        fprintf(stderr, "Can't open %s\n", pszSrcFilename);
        exit(1);
    }

    hBand = GDALGetRasterBand(hDS, iBand);
    if (hBand == NULL)
    {
        fprintf(stderr, "Can't get band %d\n", iBand);
        exit(1);
    }

    if (GDALGetGeoTransform(hDS, adfGeotransform) != CE_None)
    {
        fprintf(stderr, "Can't get geotransform\n");
        exit(1);
    }

    nXSize = GDALGetRasterXSize(hDS);
    nYSize = GDALGetRasterYSize(hDS);

/* -------------------------------------------------------------------- */
/*     Create OGR destination dataset                                   */
/* -------------------------------------------------------------------- */
    /* Special case for CSV : we generate the appropriate VRT file in the same time */
    if (EQUAL(pszFormat, "CSV") && EQUAL(CPLGetExtension(pszDstFilename), "CSV"))
    {
        FILE* fOutCSVT;
        FILE* fOutVRT;
        char* pszDstFilenameCSVT;
        char* pszDstFilenameVRT;

        fOut = fopen(pszDstFilename, "wt");
        if (fOut == NULL)
        {
            fprintf(stderr, "Can't open %s for writing\n", pszDstFilename);
            exit(1);
        }
        fprintf(fOut, "x,y,z\n");

        pszDstFilenameCSVT = CPLMalloc(strlen(pszDstFilename) + 2);
        strcpy(pszDstFilenameCSVT, pszDstFilename);
        strcat(pszDstFilenameCSVT, "t");
        fOutCSVT = fopen(pszDstFilenameCSVT, "wt");
        if (fOutCSVT == NULL)
        {
            fprintf(stderr, "Can't open %s for writing\n", pszDstFilenameCSVT);
            exit(1);
        }
        CPLFree(pszDstFilenameCSVT);
        fprintf(fOutCSVT, "Real,Real,Real\n");
        fclose(fOutCSVT);
        fOutCSVT = NULL;

        pszDstFilenameVRT = CPLStrdup(pszDstFilename);
        strcpy(pszDstFilenameVRT + strlen(pszDstFilename) - 3, "vrt");
        fOutVRT = fopen(pszDstFilenameVRT, "wt");
        if (fOutVRT == NULL)
        {
            fprintf(stderr, "Can't open %s for writing\n", pszDstFilenameVRT);
            exit(1);
        }
        CPLFree(pszDstFilenameVRT);
        fprintf(fOutVRT, "<OGRVRTDataSource>\n");
        fprintf(fOutVRT, "  <OGRVRTLayer name=\"%s\">\n", CPLGetBasename(pszDstFilename));
        fprintf(fOutVRT, "    <SrcDataSource>%s</SrcDataSource> \n", pszDstFilename);
        fprintf(fOutVRT, "    <GeometryType>wkbPoint</GeometryType>\n");
        fprintf(fOutVRT, "    <GeometryField encoding=\"PointFromColumns\" x=\"x\" y=\"y\" z=\"z\"/>\n");
        fprintf(fOutVRT, "  </OGRVRTLayer>\n");
        fprintf(fOutVRT, "</OGRVRTDataSource>\n");
        fclose(fOutVRT);
        fOutVRT = NULL;
    }
    else
    {
        OGRSpatialReferenceH hSRS = NULL;
        const char* pszWkt;

        hOGRDriver = OGRGetDriverByName(pszFormat);
        if (hOGRDriver == NULL)
        {
            fprintf(stderr, "Can't find OGR driver %s\n", pszFormat);
            exit(1);
        }

        hOGRDS = OGR_Dr_CreateDataSource(hOGRDriver, pszDstFilename, NULL);
        if (hOGRDS == NULL)
        {
            fprintf(stderr, "Can't create OGR datasource %s\n", pszDstFilename);
            exit(1);
        }

        pszWkt = GDALGetProjectionRef(hDS);
        if (pszWkt && pszWkt[0])
        {
            hSRS = OSRNewSpatialReference(pszWkt);
        }

        if (pszLayerName == NULL)
            pszLayerName = CPLStrdup(CPLGetBasename(pszDstFilename));

        hOGRLayer = OGR_DS_CreateLayer( hOGRDS, pszLayerName,
                                        hSRS, eType, NULL);

        if (hSRS)
            OSRDestroySpatialReference(hSRS);

        if (hOGRLayer == NULL)
        {
            fprintf(stderr, "Can't create layer %s\n", pszLayerName);
            exit(1);
        }

        if (eType != wkbPoint25D)
        {
            OGRFieldDefnH hFieldDefn =  OGR_Fld_Create( "z", OFTReal );
            OGR_L_CreateField(hOGRLayer, hFieldDefn, 0);
            OGR_Fld_Destroy( hFieldDefn );
        }
    }


    padfBuffer = (double*)CPLMalloc(nXSize * sizeof(double));

#define GET_X(j, i) adfGeotransform[0] + (j) * adfGeotransform[1] + (i) * adfGeotransform[2]
#define GET_Y(j, i) adfGeotransform[3] + (j) * adfGeotransform[4] + (i) * adfGeotransform[5]
#define GET_XY(j, i) GET_X(j, i), GET_Y(j, i)

/* -------------------------------------------------------------------- */
/*     "Translate" the source dataset                                   */
/* -------------------------------------------------------------------- */
    for(i=0;i<nYSize;i+=yStep)
    {
        GDALRasterIO( hBand, GF_Read, 0, i, nXSize, 1,
                      padfBuffer, nXSize, 1, GDT_Float64, 0, 0);
        for(j=0;j<nXSize;j+=xStep)
        {
            if (fOut)
            {
                fprintf(fOut, "%f,%f,%f\n",
                        GET_XY(j + .5, i + .5), padfBuffer[j]);
            }
            else
            {
                OGRFeatureH hFeature = OGR_F_Create(OGR_L_GetLayerDefn(hOGRLayer));
                OGRGeometryH hGeometry = OGR_G_CreateGeometry(eType);
                if (eType == wkbPoint25D)
                {
                    OGR_G_SetPoint(hGeometry, 0, GET_XY(j + .5, i + .5),
                                   padfBuffer[j]);
                }
                else if (eType == wkbPoint)
                {
                    OGR_G_SetPoint_2D(hGeometry, 0, GET_XY(j + .5, i + .5));
                    OGR_F_SetFieldDouble(hFeature, 0, padfBuffer[j]);
                }
                else
                {
                    OGRGeometryH hLinearRing = OGR_G_CreateGeometry(wkbLinearRing);
                    OGR_G_SetPoint_2D(hLinearRing, 0, GET_XY(j + 0, i + 0));
                    OGR_G_SetPoint_2D(hLinearRing, 1, GET_XY(j + 1, i + 0));
                    OGR_G_SetPoint_2D(hLinearRing, 2, GET_XY(j + 1, i + 1));
                    OGR_G_SetPoint_2D(hLinearRing, 3, GET_XY(j + 0, i + 1));
                    OGR_G_SetPoint_2D(hLinearRing, 4, GET_XY(j + 0, i + 0));
                    OGR_G_AddGeometryDirectly(hGeometry, hLinearRing);
                    OGR_F_SetFieldDouble(hFeature, 0, padfBuffer[j]);
                }
                OGR_F_SetGeometryDirectly(hFeature, hGeometry);
                OGR_L_CreateFeature(hOGRLayer, hFeature);
                OGR_F_Destroy(hFeature);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*     Cleanup                                                          */
/* -------------------------------------------------------------------- */
    if (fOut)
        fclose(fOut);
    else
        OGR_DS_Destroy(hOGRDS);
    GDALClose(hDS);

    CPLFree(padfBuffer);
    CPLFree(pszLayerName);

    GDALDumpOpenDatasets( stderr );
    GDALDestroyDriverManager();
    OGRCleanupAll();
    CSLDestroy( argv );

    return 0;
}
