/******************************************************************************
 * $Id$
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "kmlsuperoverlaydataset.h"

#include <cmath>   /* fabs */
#include <cstring> /* strdup */
#include <iostream>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <fstream>

#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "ogr_spatialref.h"
#include "../vrt/gdal_vrt.h"
#include "../vrt/vrtdataset.h"

using namespace std;

/************************************************************************/
/*                           GenerateTiles()                            */
/************************************************************************/
static void GenerateTiles(std::string filename,
                   CPL_UNUSED int zoom,
                   int rxsize,
                   int rysize,
                   CPL_UNUSED int ix,
                   CPL_UNUSED int iy,
                   int rx, int ry, int dxsize,
                   int dysize, int bands,
                   GDALDataset* poSrcDs,
                   GDALDriver* poOutputTileDriver,
                   GDALDriver* poMemDriver,
                   bool isJpegDriver)
{
    GDALDataset* poTmpDataset = NULL;
    GDALRasterBand* alphaBand = NULL;

    GByte* pafScanline = new GByte[dxsize];
    bool* hadnoData = new bool[dxsize];

    if (isJpegDriver && bands == 4)
        bands = 3;
   
    poTmpDataset = poMemDriver->Create("", dxsize, dysize, bands, GDT_Byte, NULL);
   
    if (!isJpegDriver)//Jpeg dataset only has one or three bands
    {
        if (bands < 4)//add transparency to files with one band or three bands
        {
            poTmpDataset->AddBand(GDT_Byte);
            alphaBand = poTmpDataset->GetRasterBand(poTmpDataset->GetRasterCount());
        }
    }

    const int rowOffset = rysize/dysize;
    const int loopCount = rysize/rowOffset;
    for (int row = 0; row < loopCount; row++)
    {
        if (!isJpegDriver)
        {
            for (int i = 0; i < dxsize; i++)
            {
                hadnoData[i] = false;
            }
        }

        for (int band = 1; band <= bands; band++)
        {
            GDALRasterBand* poBand = poSrcDs->GetRasterBand(band);
            int hasNoData = 0;
            const double noDataValue = poBand->GetNoDataValue(&hasNoData);
            const char* pixelType = poBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            const bool isSigned = ( pixelType && (strcmp(pixelType, "SIGNEDBYTE") == 0) );

            int yOffset = ry + row * rowOffset;
            CPLErr errTest =
                poBand->RasterIO( GF_Read, rx, yOffset, rxsize, rowOffset, pafScanline, dxsize, 1, GDT_Byte, 0, 0, NULL);

            const bool bReadFailed = ( errTest == CE_Failure );
            if ( bReadFailed )
            {
                hasNoData = 1;
            }


            //fill the true or false for hadnoData array if the source data has nodata value
            if (!isJpegDriver)
            {
                if (hasNoData == 1)
                {
                    for (int j = 0; j < dxsize; j++)
                    {
                        double v = pafScanline[j];
                        double tmpv = v;
                        if (isSigned)
                        {
                            tmpv -= 128;
                        }
                        if (tmpv == noDataValue || bReadFailed)
                        {
                            hadnoData[j] = true;
                        }
                    }
                }
            }

            if (!bReadFailed)
            {
                GDALRasterBand* poBandtmp = poTmpDataset->GetRasterBand(band);
                CPL_IGNORE_RET_VAL( poBandtmp->RasterIO(GF_Write, 0, row, dxsize, 1, pafScanline, dxsize, 1, GDT_Byte,
                                    0, 0, NULL) );
            }
        }

        //fill the values for alpha band
        if (!isJpegDriver)
        {
            if (alphaBand)
            {
                for (int i = 0; i < dxsize; i++)
                {
                    if (hadnoData[i])
                    {
                        pafScanline[i] = 0;
                    }
                    else
                    {
                        pafScanline[i] = 255;
                    }
                }

                CPL_IGNORE_RET_VAL( alphaBand->RasterIO(GF_Write, 0, row, dxsize, 1, pafScanline, dxsize, 1, GDT_Byte,
                                    0, 0, NULL) );
            }
        }
    }

    delete [] pafScanline;
    delete [] hadnoData;

    CPLString osOpenAfterCopy = CPLGetConfigOption("GDAL_OPEN_AFTER_COPY", "");
    CPLSetThreadLocalConfigOption("GDAL_OPEN_AFTER_COPY", "NO");
    /* to prevent CreateCopy() from calling QuietDelete() */
    char** papszOptions = CSLAddNameValue(NULL, "QUIET_DELETE_ON_CREATE_COPY", "NO");
    GDALDataset* outDs = poOutputTileDriver->CreateCopy(filename.c_str(), poTmpDataset, FALSE, papszOptions, NULL, NULL);
    CSLDestroy(papszOptions);
    CPLSetThreadLocalConfigOption("GDAL_OPEN_AFTER_COPY", osOpenAfterCopy.size() ? osOpenAfterCopy.c_str() : NULL);

    GDALClose(poTmpDataset);
    if (outDs)
        GDALClose(outDs);
}

/************************************************************************/
/*                          GenerateRootKml()                           */
/************************************************************************/

static
int  GenerateRootKml(const char* filename, 
                     const char* kmlfilename,
                     double north, 
                     double south, 
                     double east, 
                     double west, 
                     int tilesize,
                     const char* pszOverlayName,
                     const char* pszOverlayDescription)
{
    VSILFILE* fp = VSIFOpenL(filename, "wb");
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s",
                 filename);
        return FALSE;
    }
    int minlodpixels = tilesize/2;

    const char* tmpfilename = CPLGetBasename(kmlfilename);
    if( pszOverlayName == NULL )
        pszOverlayName = tmpfilename;

    // If we haven't writen any features yet, output the layer's schema
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
    VSIFPrintfL(fp, "\t<Document>\n");
    char* pszEncoded = CPLEscapeString(pszOverlayName, -1, CPLES_XML);
    VSIFPrintfL(fp, "\t\t<name>%s</name>\n", pszEncoded);
    CPLFree(pszEncoded);
    if( pszOverlayDescription == NULL )
    {
        VSIFPrintfL(fp, "\t\t<description></description>\n");
    }
    else
    {
        pszEncoded = CPLEscapeString(pszOverlayDescription, -1, CPLES_XML);
        VSIFPrintfL(fp, "\t\t<description>%s</description>\n", pszEncoded);
        CPLFree(pszEncoded);
    }
    VSIFPrintfL(fp, "\t\t<styleUrl>#hideChildrenStyle</styleUrl>\n");
    VSIFPrintfL(fp, "\t\t<Style id=\"hideChildrenStyle\">\n");
    VSIFPrintfL(fp, "\t\t\t<ListStyle id=\"hideChildren\">\n");
    VSIFPrintfL(fp, "\t\t\t\t<listItemType>checkHideChildren</listItemType>\n");
    VSIFPrintfL(fp, "\t\t\t</ListStyle>\n");
    VSIFPrintfL(fp, "\t\t</Style>\n");
    /*VSIFPrintfL(fp, "\t\t<Region>\n");
    VSIFPrintfL(fp, "\t\t\t<LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t<north>%f</north>\n", north);
    VSIFPrintfL(fp, "\t\t\t\t<south>%f</south>\n", south);
    VSIFPrintfL(fp, "\t\t\t\t<east>%f</east>\n", east);
    VSIFPrintfL(fp, "\t\t\t\t<west>%f</west>\n", west);
    VSIFPrintfL(fp, "\t\t\t</LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t</Region>\n");*/
    VSIFPrintfL(fp, "\t\t<NetworkLink>\n");
    VSIFPrintfL(fp, "\t\t\t<open>1</open>\n");
    VSIFPrintfL(fp, "\t\t\t<Region>\n");
    VSIFPrintfL(fp, "\t\t\t\t<LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t\t<north>%f</north>\n", north);
    VSIFPrintfL(fp, "\t\t\t\t\t<south>%f</south>\n", south);
    VSIFPrintfL(fp, "\t\t\t\t\t<east>%f</east>\n", east);
    VSIFPrintfL(fp, "\t\t\t\t\t<west>%f</west>\n", west);
    VSIFPrintfL(fp, "\t\t\t\t</LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t<Lod>\n");
    VSIFPrintfL(fp, "\t\t\t\t\t<minLodPixels>%d</minLodPixels>\n", minlodpixels);
    VSIFPrintfL(fp, "\t\t\t\t\t<maxLodPixels>-1</maxLodPixels>\n");
    VSIFPrintfL(fp, "\t\t\t\t</Lod>\n");
    VSIFPrintfL(fp, "\t\t\t</Region>\n");
    VSIFPrintfL(fp, "\t\t\t<Link>\n");
    VSIFPrintfL(fp, "\t\t\t\t<href>0/0/0.kml</href>\n");
    VSIFPrintfL(fp, "\t\t\t\t<viewRefreshMode>onRegion</viewRefreshMode>\n");
    VSIFPrintfL(fp, "\t\t\t</Link>\n");
    VSIFPrintfL(fp, "\t\t</NetworkLink>\n");
    VSIFPrintfL(fp, "\t</Document>\n");
    VSIFPrintfL(fp, "</kml>\n");

    VSIFCloseL(fp);
    return TRUE;
}

/************************************************************************/
/*                          GenerateChildKml()                          */
/************************************************************************/

static
int  GenerateChildKml(std::string filename, 
                      int zoom, int ix, int iy, 
                      double zoomxpixel, double zoomypixel, int dxsize, int dysize, 
                      double south, double west, int xsize, 
                      int ysize, int maxzoom, 
                      OGRCoordinateTransformation * poTransform,
                      std::string fileExt,
                      bool fixAntiMeridian,
                      const char* pszAltitude,
                      const char* pszAltitudeMode)
{
    double tnorth = south + zoomypixel *((iy + 1)*dysize);
    double tsouth = south + zoomypixel *(iy*dysize);
    double teast = west + zoomxpixel*((ix+1)*dxsize);
    double twest = west + zoomxpixel*ix*dxsize;

    double upperleftT = twest;
    double lowerleftT = twest;

    double rightbottomT = tsouth;
    double leftbottomT = tsouth;

    double lefttopT = tnorth;
    double righttopT = tnorth;

    double lowerrightT = teast;
    double upperrightT = teast;

    if (poTransform)
    {
        poTransform->Transform(1, &twest, &tsouth);
        poTransform->Transform(1, &teast, &tnorth);

        poTransform->Transform(1, &upperleftT, &lefttopT);
        poTransform->Transform(1, &upperrightT, &righttopT);
        poTransform->Transform(1, &lowerrightT, &rightbottomT);
        poTransform->Transform(1, &lowerleftT, &leftbottomT);
    }

    if ( fixAntiMeridian && teast < twest)
    {
        teast += 360;
        lowerrightT += 360;
        upperrightT += 360;
    }

    std::vector<int> xchildren;
    std::vector<int> ychildern;

    int minLodPixels = 128;
    if (zoom == 0)
    {
        minLodPixels = 1;
    }

    int maxLodPix = -1;
    if ( zoom < maxzoom )
    {
        double zareasize = pow(2.0, (maxzoom - zoom - 1))*dxsize;
        double zareasize1 = pow(2.0, (maxzoom - zoom - 1))*dysize;
        xchildren.push_back(ix*2);
        int tmp = ix*2 + 1;
        int tmp1 = (int)ceil(xsize / zareasize);
        if (tmp < tmp1)
        {
            xchildren.push_back(ix*2+1);
        }
        ychildern.push_back(iy*2);
        tmp = iy*2 + 1;
        tmp1 = (int)ceil(ysize / zareasize1);
        if (tmp < tmp1)
        {
            ychildern.push_back(iy*2+1);
        }     
        maxLodPix = 2048;
    }

    VSILFILE* fp = VSIFOpenL(filename.c_str(), "wb");
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s",
                 filename.c_str());
        return FALSE;
    }

    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n");
    VSIFPrintfL(fp, "\t<Document>\n");
    VSIFPrintfL(fp, "\t\t<name>%d/%d/%d.kml</name>\n", zoom, ix, iy);
    VSIFPrintfL(fp, "\t\t<styleUrl>#hideChildrenStyle</styleUrl>\n");
    VSIFPrintfL(fp, "\t\t<Style id=\"hideChildrenStyle\">\n");
    VSIFPrintfL(fp, "\t\t\t<ListStyle id=\"hideChildren\">\n");
    VSIFPrintfL(fp, "\t\t\t\t<listItemType>checkHideChildren</listItemType>\n");
    VSIFPrintfL(fp, "\t\t\t</ListStyle>\n");
    VSIFPrintfL(fp, "\t\t</Style>\n");
    VSIFPrintfL(fp, "\t\t<Region>\n");
    VSIFPrintfL(fp, "\t\t\t<LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t<north>%f</north>\n", tnorth);
    VSIFPrintfL(fp, "\t\t\t\t<south>%f</south>\n", tsouth);
    VSIFPrintfL(fp, "\t\t\t\t<east>%f</east>\n", teast);
    VSIFPrintfL(fp, "\t\t\t\t<west>%f</west>\n", twest);
    VSIFPrintfL(fp, "\t\t\t</LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t<Lod>\n");
    VSIFPrintfL(fp, "\t\t\t\t<minLodPixels>%d</minLodPixels>\n", minLodPixels);
    VSIFPrintfL(fp, "\t\t\t\t<maxLodPixels>%d</maxLodPixels>\n", maxLodPix);
    VSIFPrintfL(fp, "\t\t\t</Lod>\n");
    VSIFPrintfL(fp, "\t\t</Region>\n");
    VSIFPrintfL(fp, "\t\t<GroundOverlay>\n");
    VSIFPrintfL(fp, "\t\t\t<drawOrder>%d</drawOrder>\n", zoom);
    VSIFPrintfL(fp, "\t\t\t<Icon>\n");
    VSIFPrintfL(fp, "\t\t\t\t<href>%d%s</href>\n", iy, fileExt.c_str());
    VSIFPrintfL(fp, "\t\t\t</Icon>\n");

    if( pszAltitude != NULL )
    {
        VSIFPrintfL(fp, "\t\t\t<altitude>%s</altitude>\n", pszAltitude);
    }
    if( pszAltitudeMode != NULL &&
        (strcmp(pszAltitudeMode, "clampToGround") == 0 ||
         strcmp(pszAltitudeMode, "absolute") == 0) )
    {
        VSIFPrintfL(fp, "\t\t\t<altitudeMode>%s</altitudeMode>\n", pszAltitudeMode);
    }
    else if( pszAltitudeMode != NULL &&
        (strcmp(pszAltitudeMode, "relativeToSeaFloor") == 0 ||
         strcmp(pszAltitudeMode, "clampToSeaFloor") == 0) )
    {
        VSIFPrintfL(fp, "\t\t\t<gx:altitudeMode>%s</gx:altitudeMode>\n", pszAltitudeMode);
    }

    /* When possible, use <LatLonBox>. I've noticed otherwise that */
    /* if using <gx:LatLonQuad> with extents of the size of a country or */
    /* continent, the overlay is really bad placed in GoogleEarth */
    if( lowerleftT == upperleftT && lowerrightT == upperrightT &&
        leftbottomT == rightbottomT && righttopT == lefttopT )
    {
        VSIFPrintfL(fp, "\t\t\t<LatLonBox>\n");
        VSIFPrintfL(fp, "\t\t\t\t<north>%f</north>\n", tnorth);
        VSIFPrintfL(fp, "\t\t\t\t<south>%f</south>\n", tsouth);
        VSIFPrintfL(fp, "\t\t\t\t<east>%f</east>\n", teast);
        VSIFPrintfL(fp, "\t\t\t\t<west>%f</west>\n", twest);
        VSIFPrintfL(fp, "\t\t\t</LatLonBox>\n");
    }
    else
    {
        VSIFPrintfL(fp, "\t\t\t<gx:LatLonQuad>\n");
        VSIFPrintfL(fp, "\t\t\t\t<coordinates>\n");
        VSIFPrintfL(fp, "\t\t\t\t\t%f,%f,0\n", lowerleftT, leftbottomT);
        VSIFPrintfL(fp, "\t\t\t\t\t%f,%f,0\n", lowerrightT, rightbottomT);
        VSIFPrintfL(fp, "\t\t\t\t\t%f,%f,0\n", upperrightT, righttopT);
        VSIFPrintfL(fp, "\t\t\t\t\t%f,%f,0\n", upperleftT, lefttopT);
        VSIFPrintfL(fp, "\t\t\t\t</coordinates>\n");
        VSIFPrintfL(fp, "\t\t\t</gx:LatLonQuad>\n");
    }
    VSIFPrintfL(fp, "\t\t</GroundOverlay>\n");

    for (unsigned int i = 0; i < xchildren.size(); i++)
    {
        int cx = xchildren[i];
        for (unsigned int j = 0; j < ychildern.size(); j++)
        {
            int cy = ychildern[j];

            double cnorth = south + zoomypixel/2 *((cy + 1)*dysize);
            double csouth = south + zoomypixel/2 *(cy*dysize);
            double ceast = west + zoomxpixel/2*((cx+1)*dxsize);
            double cwest = west + zoomxpixel/2*cx*dxsize;

            if (poTransform)
            {
                poTransform->Transform(1, &cwest, &csouth);
                poTransform->Transform(1, &ceast, &cnorth);
            }

            if ( fixAntiMeridian && ceast < cwest )
            {
                ceast += 360;
            }

            VSIFPrintfL(fp, "\t\t<NetworkLink>\n");
            VSIFPrintfL(fp, "\t\t\t<name>%d/%d/%d%s</name>\n", zoom+1, cx, cy, fileExt.c_str());
            VSIFPrintfL(fp, "\t\t\t<Region>\n");
            VSIFPrintfL(fp, "\t\t\t\t<LatLonAltBox>\n");
            VSIFPrintfL(fp, "\t\t\t\t\t<north>%f</north>\n", cnorth);
            VSIFPrintfL(fp, "\t\t\t\t\t<south>%f</south>\n", csouth);
            VSIFPrintfL(fp, "\t\t\t\t\t<east>%f</east>\n", ceast);
            VSIFPrintfL(fp, "\t\t\t\t\t<west>%f</west>\n", cwest);
            VSIFPrintfL(fp, "\t\t\t\t</LatLonAltBox>\n");
            VSIFPrintfL(fp, "\t\t\t\t<Lod>\n");
            VSIFPrintfL(fp, "\t\t\t\t\t<minLodPixels>128</minLodPixels>\n");
            VSIFPrintfL(fp, "\t\t\t\t\t<maxLodPixels>-1</maxLodPixels>\n");
            VSIFPrintfL(fp, "\t\t\t\t</Lod>\n");
            VSIFPrintfL(fp, "\t\t\t</Region>\n");
            VSIFPrintfL(fp, "\t\t\t<Link>\n");
            VSIFPrintfL(fp, "\t\t\t\t<href>../../%d/%d/%d.kml</href>\n", zoom+1, cx, cy);
            VSIFPrintfL(fp, "\t\t\t\t<viewRefreshMode>onRegion</viewRefreshMode>\n");
            VSIFPrintfL(fp, "\t\t\t\t<viewFormat/>\n");
            VSIFPrintfL(fp, "\t\t\t</Link>\n");
            VSIFPrintfL(fp, "\t\t</NetworkLink>\n");
        }
    }

    VSIFPrintfL(fp, "\t</Document>\n");
    VSIFPrintfL(fp, "</kml>\n");
    VSIFCloseL(fp);
    
    return TRUE;
}

/************************************************************************/
/*                           CreateCopy()                               */
/************************************************************************/

class KmlSuperOverlayDummyDataset: public GDALDataset
{
    public:
        KmlSuperOverlayDummyDataset() {}
};

static
GDALDataset *KmlSuperOverlayCreateCopy( const char * pszFilename,
                                        GDALDataset *poSrcDS,
                                        CPL_UNUSED int bStrict,
                                        char ** papszOptions,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData)
{
    bool isKmz = false;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    int bands = poSrcDS->GetRasterCount();
    if (bands != 1 && bands != 3 && bands != 4)
        return NULL;
   
    //correct the file and get the directory
    char* output_dir = NULL;
    if (pszFilename == NULL)
    {
        output_dir = CPLGetCurrentDir();
        pszFilename = CPLFormFilename(output_dir, "doc", "kml");
    }
    else
    {
        const char* extension = CPLGetExtension(pszFilename);
        if (!EQUAL(extension,"kml") && !EQUAL(extension,"kmz"))
        {
            CPLError( CE_Failure, CPLE_None,
                      "File extension should be kml or kmz." );
            return NULL;
        }
        if (EQUAL(extension,"kmz"))
        {
            isKmz = true;
        }
    
        output_dir = CPLStrdup(CPLGetPath(pszFilename));
        if (strcmp(output_dir, "") == 0)
        {
            CPLFree(output_dir);
            output_dir = CPLGetCurrentDir();
        }
    }
    CPLString outDir = output_dir ? output_dir : "";
    CPLFree(output_dir);
    output_dir = NULL;

    VSILFILE* zipHandle = NULL;
    if (isKmz)
    {
        outDir = "/vsizip/";
        outDir += pszFilename;
        zipHandle = VSIFOpenL(outDir, "wb");
        if( zipHandle == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", pszFilename);
            return NULL;
        }
    }

    GDALDriver* poOutputTileDriver = NULL;
    bool isJpegDriver = true;

    const char* pszFormat = CSLFetchNameValueDef(papszOptions, "FORMAT", "JPEG");
    if (EQUAL(pszFormat, "PNG"))
    {
        isJpegDriver = false;
    }

    GDALDriver* poMemDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    poOutputTileDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);

    if( poMemDriver == NULL || poOutputTileDriver == NULL)
    {
        CPLError( CE_Failure, CPLE_None,
                  "Image export driver was not found.." );
        if( zipHandle != NULL )
        {
            VSIFCloseL(zipHandle);
            VSIUnlink(pszFilename);
        }
        return NULL;
    }

    int xsize = poSrcDS->GetRasterXSize();
    int ysize = poSrcDS->GetRasterYSize();

    double north = 0.0;
    double south = 0.0;
    double east = 0.0;
    double west = 0.0;

    double	adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None )
    {
        north = adfGeoTransform[3];
        south = adfGeoTransform[3] + adfGeoTransform[5]*ysize;
        east = adfGeoTransform[0] + adfGeoTransform[1]*xsize;
        west = adfGeoTransform[0];
    }

    OGRCoordinateTransformation * poTransform = NULL;
    if (poSrcDS->GetProjectionRef() != NULL)
    {
        OGRSpatialReference poDsUTM;
     
        char* projStr = (char*)poSrcDS->GetProjectionRef();
     
        if (poDsUTM.importFromWkt(&projStr) == OGRERR_NONE)
        {
            if (poDsUTM.IsProjected())
            {
                OGRSpatialReference poLatLong;
                poLatLong.SetWellKnownGeogCS( "WGS84" );
           
                poTransform = OGRCreateCoordinateTransformation( &poDsUTM, &poLatLong );
                if( poTransform != NULL )
                {
                    poTransform->Transform(1, &west, &south);
                    poTransform->Transform(1, &east, &north);
                }
            }
        }
    }

    bool fixAntiMeridian = CSLFetchBoolean( papszOptions, "FIX_ANTIMERIDIAN", FALSE );
    if ( fixAntiMeridian && east < west )
    {
        east += 360;
    }

    //Zoom levels of the pyramid.
    int maxzoom = 0;
    int tilexsize;
    int tileysize;
    // Let the longer side determine the max zoom level and x/y tilesizes.
    if ( xsize >= ysize )
    {
        double dtilexsize = xsize;
        while (dtilexsize > 400) //calculate x tile size
        {
            dtilexsize = dtilexsize/2;
            maxzoom ++;
        }
        tilexsize = (int)dtilexsize;
        tileysize = (int)( (double)(dtilexsize * ysize) / xsize );
    }
    else
    {
        double dtileysize = ysize;
        while (dtileysize > 400) //calculate y tile size
        {
            dtileysize = dtileysize/2;
            maxzoom ++;
        }

        tileysize = (int)dtileysize;
        tilexsize = (int)( (double)(dtileysize * xsize) / ysize );
    }

    std::vector<double> zoomxpixels;
    std::vector<double> zoomypixels;
    for (int zoom = 0; zoom < maxzoom + 1; zoom++)
    {
        zoomxpixels.push_back(adfGeoTransform[1] * pow(2.0, (maxzoom - zoom)));
        // zoomypixels.push_back(abs(adfGeoTransform[5]) * pow(2.0, (maxzoom - zoom)));
        zoomypixels.push_back(fabs(adfGeoTransform[5]) * pow(2.0, (maxzoom - zoom)));
    }

    std::string tmpFileName; 
    std::vector<std::string> fileVector;
    int nRet;

    const char* pszOverlayName = CSLFetchNameValue(papszOptions, "NAME");
    const char* pszOverlayDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");

    if (isKmz)
    {
        tmpFileName = CPLFormFilename(outDir, "doc.kml", NULL);
        nRet = GenerateRootKml(tmpFileName.c_str(), pszFilename,
                               north, south, east, west, (int)tilexsize,
                               pszOverlayName, pszOverlayDescription);
        fileVector.push_back(tmpFileName);
    }
    else
    {
        nRet = GenerateRootKml(pszFilename, pszFilename,
                               north, south, east, west, (int)tilexsize,
                               pszOverlayName, pszOverlayDescription);
    }
    
    if (nRet == FALSE)
    {
        OGRCoordinateTransformation::DestroyCT( poTransform );
        if( zipHandle != NULL )
        {
            VSIFCloseL(zipHandle);
            VSIUnlink(pszFilename);
        }
        return NULL;
    }

    const char* pszAltitude = CSLFetchNameValue(papszOptions, "ALTITUDE");
    const char* pszAltitudeMode = CSLFetchNameValue(papszOptions, "ALTITUDEMODE");
    if( pszAltitudeMode != NULL )
    {
        if( strcmp(pszAltitudeMode, "clampToGround") == 0 )
        {
            pszAltitudeMode = NULL;
            pszAltitude = NULL;
        }
        else if( strcmp(pszAltitudeMode, "absolute") == 0 )
        {
            if( pszAltitude == NULL )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Using ALTITUDE=0 as default value");
                pszAltitude = "0";
            }
        }
        else if( strcmp(pszAltitudeMode, "relativeToSeaFloor") == 0 )
        {
            /* nothing to do */
        }
        else if( strcmp(pszAltitudeMode, "clampToSeaFloor") == 0 )
        {
            pszAltitude = NULL;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Ignoring unhandled value of ALTITUDEMODE");
            pszAltitudeMode = NULL;
            pszAltitude = NULL;
        }
    }

    int zoom;
    int nTotalTiles = 0;
    int nTileCount = 0;

    for (zoom = maxzoom; zoom >= 0; --zoom)
    {
        int rmaxxsize = tilexsize * (1 << (maxzoom-zoom));
        int rmaxysize = tileysize * (1 << (maxzoom-zoom));

        int xloop = (int)xsize/rmaxxsize;
        int yloop = (int)ysize/rmaxysize;
        nTotalTiles += xloop * yloop;
    }

    for (zoom = maxzoom; zoom >= 0; --zoom)
    {
        int rmaxxsize = tilexsize * (1 << (maxzoom-zoom));
        int rmaxysize = tileysize * (1 << (maxzoom-zoom));

        int xloop = (int)xsize/rmaxxsize;
        int yloop = (int)ysize/rmaxysize;

        xloop = xloop>0 ? xloop : 1;
        yloop = yloop>0 ? yloop : 1;
        
        std::stringstream zoomStr;
        zoomStr << zoom;

        std::string zoomDir = outDir;
        zoomDir+= "/" + zoomStr.str();
        VSIMkdir(zoomDir.c_str(), 0775);

        for (int ix = 0; ix < xloop; ix++)
        {
            int rxsize = (int)(rmaxxsize);
            int rx = (int)(ix * rmaxxsize);
            int dxsize = (int)(rxsize/rmaxxsize * tilexsize);

            std::stringstream ixStr;
            ixStr << ix;

            zoomDir = outDir;
            zoomDir+= "/" + zoomStr.str();
            zoomDir+= "/" + ixStr.str();
            VSIMkdir(zoomDir.c_str(), 0775);

            for (int iy = 0; iy < yloop; iy++)
            {
                int rysize = (int)(rmaxysize);
                int ry = (int)(ysize - (iy * rmaxysize)) - rysize;
                int dysize = (int)(rysize/rmaxysize * tileysize);

                std::stringstream iyStr;
                iyStr << iy;

                std::string fileExt = ".jpg";
                if (isJpegDriver == false)
                {
                    fileExt = ".png";
                }
                std::string filename = zoomDir + "/" + iyStr.str() + fileExt;
                if (isKmz)
                {
                    fileVector.push_back(filename);
                }

                GenerateTiles(filename, zoom, rxsize, rysize, ix, iy, rx, ry, dxsize, 
                              dysize, bands, poSrcDS, poOutputTileDriver, poMemDriver, isJpegDriver);
                std::string childKmlfile = zoomDir + "/" + iyStr.str() + ".kml";
                if (isKmz)
                {
                    fileVector.push_back(childKmlfile);
                }

                double tmpSouth = adfGeoTransform[3] + adfGeoTransform[5]*ysize;
                double zoomxpix = zoomxpixels[zoom];
                double zoomypix = zoomypixels[zoom];
                if (zoomxpix == 0)
                {
                    zoomxpix = 1;
                }

                if (zoomypix == 0)
                {
                    zoomypix = 1;
                }

                GenerateChildKml(childKmlfile, zoom, ix, iy, zoomxpix, zoomypix, 
                                 dxsize, dysize, tmpSouth, adfGeoTransform[0],
                                 xsize, ysize, maxzoom, poTransform, fileExt, fixAntiMeridian,
                                 pszAltitude, pszAltitudeMode);

                nTileCount ++;
                pfnProgress(1.0 * nTileCount / nTotalTiles, "", pProgressData);
            }
        }
    }

    OGRCoordinateTransformation::DestroyCT( poTransform );
    poTransform = NULL;

    if( zipHandle != NULL )
    {
        VSIFCloseL(zipHandle);
    }

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    GDALDataset* poDS = KmlSuperOverlayReadDataset::Open(&oOpenInfo);
    if( poDS == NULL )
        poDS = new KmlSuperOverlayDummyDataset();
    return poDS;
}


/************************************************************************/
/*                            KMLRemoveSlash()                          */
/************************************************************************/

/* replace "a/b/../c" pattern by "a/c" */
static CPLString KMLRemoveSlash(const char* pszPathIn)
{
    char* pszPath = CPLStrdup(pszPathIn);

    while(TRUE)
    {
        char* pszSlashDotDot = strstr(pszPath, "/../");
        if (pszSlashDotDot == NULL || pszSlashDotDot == pszPath)
            break;
        char* pszSlashBefore = pszSlashDotDot-1;
        while(pszSlashBefore > pszPath && *pszSlashBefore != '/')
            pszSlashBefore --;
        if (pszSlashBefore == pszPath)
            break;
        memmove(pszSlashBefore + 1, pszSlashDotDot + 4,
                strlen(pszSlashDotDot + 4) + 1);
    }
    CPLString osRet = pszPath;
    CPLFree(pszPath);
    return osRet;
}

/************************************************************************/
/*                      KmlSuperOverlayReadDataset()                    */
/************************************************************************/

KmlSuperOverlayReadDataset::KmlSuperOverlayReadDataset()

{
    nFactor = 1;
    psRoot = NULL;
    psDocument = NULL;
    poDSIcon = NULL;
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;

    nOverviewCount = 0;
    papoOverviewDS = NULL;
    bIsOvr = FALSE;

    poParent = NULL;
    psFirstLink = NULL;
    psLastLink = NULL;
}

/************************************************************************/
/*                     ~KmlSuperOverlayReadDataset()                    */
/************************************************************************/

KmlSuperOverlayReadDataset::~KmlSuperOverlayReadDataset()

{
    if( psRoot != NULL )
        CPLDestroyXMLNode( psRoot );
    CloseDependentDatasets();
}

/************************************************************************/
/*                         CloseDependentDatasets()                     */
/************************************************************************/

int KmlSuperOverlayReadDataset::CloseDependentDatasets()
{
    int bRet = FALSE;
    if( poDSIcon != NULL )
    {
        CPLString osFilename(poDSIcon->GetDescription());
        delete poDSIcon;
        VSIUnlink(osFilename);
        poDSIcon = NULL;
        bRet = TRUE;
    }

    LinkedDataset* psCur = psFirstLink;
    psFirstLink = NULL;
    psLastLink = NULL;

    while( psCur != NULL )
    {
        LinkedDataset* psNext = psCur->psNext;
        if( psCur->poDS != NULL )
        {
            if( psCur->poDS->nRefCount == 1 )
                bRet = TRUE;
            GDALClose(psCur->poDS);
        }
        delete psCur;
        psCur = psNext;
    }

    if( nOverviewCount > 0 )
    {
        bRet = TRUE;
        for(int i = 0; i < nOverviewCount; i++)
            delete papoOverviewDS[i];
        CPLFree(papoOverviewDS);
        nOverviewCount = 0;
        papoOverviewDS = NULL;
    }

    return bRet;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *KmlSuperOverlayReadDataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr KmlSuperOverlayReadDataset::GetGeoTransform( double * padfGeoTransform )
{
    memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
    return CE_None;
}

/************************************************************************/
/*                        KmlSuperOverlayRasterBand()                   */
/************************************************************************/

KmlSuperOverlayRasterBand::KmlSuperOverlayRasterBand(KmlSuperOverlayReadDataset* poDS,
                                                     CPL_UNUSED int nBand)
{
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = GDT_Byte;
    nBlockXSize = 256;
    nBlockYSize = 256;
}

/************************************************************************/
/*                               IReadBlock()                           */
/************************************************************************/

CPLErr KmlSuperOverlayRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void *pData )
{
    int nXOff = nBlockXOff * nBlockXSize;
    int nYOff = nBlockYOff * nBlockYSize;
    int nXSize = nBlockXSize;
    int nYSize = nBlockYSize;
    if( nXOff + nXSize > nRasterXSize )
        nXSize = nRasterXSize - nXOff;
    if( nYOff + nYSize > nRasterYSize )
        nYSize = nRasterYSize - nYOff;
    
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO( GF_Read,
                      nXOff,
                      nYOff,
                      nXSize,
                      nYSize,
                      pData,
                      nXSize,
                      nYSize,
                      eDataType,
                      1,
                      nBlockXSize, &sExtraArg );
}

/************************************************************************/
/*                          GetColorInterpretation()                    */
/************************************************************************/

GDALColorInterp  KmlSuperOverlayRasterBand::GetColorInterpretation()
{
    return (GDALColorInterp)(GCI_RedBand + nBand - 1);
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr KmlSuperOverlayRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                             int nXOff, int nYOff, int nXSize, int nYSize,
                                             void * pData, int nBufXSize, int nBufYSize,
                                             GDALDataType eBufType,
                                             GSpacing nPixelSpace,
                                             GSpacing nLineSpace,
                                             GDALRasterIOExtraArg* psExtraArg )
{
    KmlSuperOverlayReadDataset* poGDS = (KmlSuperOverlayReadDataset* )poDS;

    return poGDS->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                            pData, nBufXSize, nBufYSize, eBufType,
                            1, &nBand,
                            nPixelSpace, nLineSpace, 0, psExtraArg );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int KmlSuperOverlayRasterBand::GetOverviewCount()
{
    KmlSuperOverlayReadDataset* poGDS = (KmlSuperOverlayReadDataset* )poDS;

    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand *KmlSuperOverlayRasterBand::GetOverview(int iOvr)
{
    KmlSuperOverlayReadDataset* poGDS = (KmlSuperOverlayReadDataset* )poDS;

    if( iOvr < 0 || iOvr >= poGDS->nOverviewCount )
        return NULL;

    return poGDS->papoOverviewDS[iOvr]->GetRasterBand(nBand);
}

/************************************************************************/
/*                     KmlSuperOverlayGetBoundingBox()                  */
/************************************************************************/

static
int KmlSuperOverlayGetBoundingBox(CPLXMLNode* psNode, double* adfExtents)
{
    CPLXMLNode* psBox = CPLGetXMLNode(psNode, "LatLonBox");
    if( psBox == NULL )
        psBox = CPLGetXMLNode(psNode, "LatLonAltBox");
    if( psBox == NULL )
        return FALSE;
    
    const char* pszNorth = CPLGetXMLValue(psBox, "north", NULL);
    const char* pszSouth = CPLGetXMLValue(psBox, "south", NULL);
    const char* pszEast = CPLGetXMLValue(psBox, "east", NULL);
    const char* pszWest = CPLGetXMLValue(psBox, "west", NULL);
    if( pszNorth == NULL || pszSouth == NULL || pszEast == NULL || pszWest == NULL )
        return FALSE;
    
    adfExtents[0] = CPLAtof(pszWest);
    adfExtents[1] = CPLAtof(pszSouth);
    adfExtents[2] = CPLAtof(pszEast);
    adfExtents[3] = CPLAtof(pszNorth);
    return TRUE;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

class SubImageDesc
{
    public:
        GDALDataset* poDS;
        double       adfExtents[4];
};

CPLErr KmlSuperOverlayReadDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if( eRWFlag == GF_Write )
        return CE_Failure;
    
    if( bIsOvr )
        return poParent->IRasterIO( eRWFlag,
                                    nXOff * (poParent->nFactor / nFactor),
                                    nYOff * (poParent->nFactor / nFactor),
                                    nXSize * (poParent->nFactor / nFactor),
                                    nYSize * (poParent->nFactor / nFactor),
                                    pData, nBufXSize, nBufYSize,
                                    eBufType, 
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
    
    double dfXOff = 1.0 * nXOff / nFactor;
    double dfYOff = 1.0 * nYOff / nFactor;
    double dfXSize = 1.0 * nXSize / nFactor;
    double dfYSize = 1.0 * nYSize / nFactor;

    int nIconCount = poDSIcon->GetRasterCount();

    if( nBufXSize > dfXSize || nBufYSize > dfYSize )
    {
        double dfRequestXMin = adfGeoTransform[0] + nXOff * adfGeoTransform[1];
        double dfRequestXMax = adfGeoTransform[0] + (nXOff + nXSize) * adfGeoTransform[1];
        double dfRequestYMin = adfGeoTransform[3] + (nYOff + nYSize) * adfGeoTransform[5];
        double dfRequestYMax = adfGeoTransform[3] + nYOff * adfGeoTransform[5];

        CPLXMLNode* psIter = psDocument->psChild;
        std::vector<SubImageDesc> aosImages;
        double dfXRes = adfGeoTransform[1] * nFactor;
        double dfYRes = -adfGeoTransform[5] * nFactor;
        double dfNewXRes = dfXRes;
        double dfNewYRes = dfYRes;

        while( psIter != NULL )
        {
            CPLXMLNode* psRegion = NULL;
            CPLXMLNode* psLink = NULL;
            double adfExtents[4];
            const char* pszHref = NULL;
            if( psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "NetworkLink") == 0 &&
                (psRegion = CPLGetXMLNode(psIter, "Region")) != NULL &&
                (psLink = CPLGetXMLNode(psIter, "Link")) != NULL &&
                KmlSuperOverlayGetBoundingBox(psRegion, adfExtents) &&
                (pszHref = CPLGetXMLValue(psLink, "href", NULL)) != NULL )
            {
                if( dfRequestXMin < adfExtents[2] && dfRequestXMax > adfExtents[0] &&
                    dfRequestYMin < adfExtents[3] && dfRequestYMax > adfExtents[1] )
                {
                    CPLString osSubFilename;
                    if( STARTS_WITH(pszHref, "http"))
                        osSubFilename = CPLSPrintf("/vsicurl_streaming/%s", pszHref);
                    else
                    {
                        const char* pszBaseFilename = osFilename.c_str();
                        if( EQUAL(CPLGetExtension(pszBaseFilename), "kmz") &&
                            !STARTS_WITH(pszBaseFilename, "/vsizip/") )
                        {
                            osSubFilename = "/vsizip/";
                            osSubFilename += CPLGetPath(pszBaseFilename);
                            osSubFilename += "/";
                            osSubFilename += pszHref;
                        }
                        else
                        {
                            osSubFilename = CPLFormFilename(CPLGetPath(pszBaseFilename), pszHref, NULL);
                        }
                        osSubFilename = KMLRemoveSlash(osSubFilename);
                    }

                    KmlSuperOverlayReadDataset* poSubImageDS = NULL;
                    if( EQUAL(CPLGetExtension(osSubFilename), "kml") )
                    {
                        KmlSuperOverlayReadDataset* poRoot = poParent ? poParent : this;
                        LinkedDataset* psLink = poRoot->oMapChildren[osSubFilename];
                        if( psLink == NULL )
                        {
                            if( poRoot->oMapChildren.size() == 64 )
                            {
                                psLink = poRoot->psLastLink;
                                CPLAssert(psLink);
                                poRoot->oMapChildren.erase(psLink->osSubFilename);
                                GDALClose(psLink->poDS);
                                if( psLink->psPrev != NULL )
                                {
                                    poRoot->psLastLink = psLink->psPrev;
                                    psLink->psPrev->psNext = NULL;
                                }
                                else
                                {
                                    CPLAssert(psLink == poRoot->psFirstLink);
                                    poRoot->psFirstLink = poRoot->psLastLink = NULL;
                                }
                            }
                            else
                                psLink = new LinkedDataset();

                            poRoot->oMapChildren[osSubFilename] = psLink;
                            poSubImageDS = (KmlSuperOverlayReadDataset*)
                                KmlSuperOverlayReadDataset::Open(osSubFilename, poRoot);
                            if( poSubImageDS )
                                poSubImageDS->MarkAsShared();
                            else
                                CPLDebug("KMLSuperOverlay", "Cannt open %s", osSubFilename.c_str());
                            psLink->osSubFilename = osSubFilename;
                            psLink->poDS = poSubImageDS;
                            psLink->psPrev = NULL;
                            psLink->psNext = poRoot->psFirstLink;
                            if( poRoot->psFirstLink != NULL )
                            {
                                CPLAssert(poRoot->psFirstLink->psPrev == NULL);
                                poRoot->psFirstLink->psPrev = psLink;
                            }
                            else
                                poRoot->psLastLink = psLink;
                            poRoot->psFirstLink = psLink;
                        }
                        else
                        {
                            poSubImageDS = psLink->poDS;
                            if( psLink != poRoot->psFirstLink )
                            {
                                if( psLink == poRoot->psLastLink )
                                {
                                    poRoot->psLastLink = psLink->psPrev;
                                    CPLAssert(poRoot->psLastLink != NULL );
                                    poRoot->psLastLink->psNext = NULL;
                                }
                                else
                                    psLink->psNext->psPrev = psLink->psPrev;
                                CPLAssert( psLink->psPrev != NULL );
                                psLink->psPrev->psNext = psLink->psNext;
                                psLink->psPrev = NULL;
                                poRoot->psFirstLink->psPrev = psLink;
                                psLink->psNext = poRoot->psFirstLink;
                                poRoot->psFirstLink = psLink;
                            }
                        }
                    }
                    if( poSubImageDS )
                    {
                        int nSubImageXSize = poSubImageDS->GetRasterXSize();
                        int nSubImageYSize = poSubImageDS->GetRasterYSize();
                        adfExtents[0] = poSubImageDS->adfGeoTransform[0];
                        adfExtents[1] = poSubImageDS->adfGeoTransform[3] + nSubImageYSize * poSubImageDS->adfGeoTransform[5];
                        adfExtents[2] = poSubImageDS->adfGeoTransform[0] + nSubImageXSize * poSubImageDS->adfGeoTransform[1];
                        adfExtents[3] = poSubImageDS->adfGeoTransform[3];

                        double dfSubXRes = (adfExtents[2] - adfExtents[0]) / nSubImageXSize;
                        double dfSubYRes = (adfExtents[3] - adfExtents[1]) / nSubImageYSize;

                        if( dfSubXRes < dfNewXRes ) dfNewXRes = dfSubXRes;
                        if( dfSubYRes < dfNewYRes ) dfNewYRes = dfSubYRes;

                        SubImageDesc oImageDesc;
                        oImageDesc.poDS = poSubImageDS;
                        poSubImageDS->Reference();
                        memcpy(oImageDesc.adfExtents, adfExtents, 4 * sizeof(double));
                        aosImages.push_back(oImageDesc);
                    }
                }
            }
            psIter = psIter->psNext;
        }

        if( dfNewXRes < dfXRes || dfNewYRes < dfYRes )
        {
            int i;
            double dfXFactor = dfXRes / dfNewXRes;
            double dfYFactor = dfYRes / dfNewYRes;
            VRTDataset* poVRTDS = new VRTDataset(
                (int)(nRasterXSize * dfXFactor + 0.5),
                (int)(nRasterYSize * dfYFactor + 0.5));

            for(int iBandIdx = 0; iBandIdx < 4; iBandIdx++ )
            {
                VRTAddBand( (VRTDatasetH) poVRTDS, GDT_Byte, NULL );

                int nBand = iBandIdx + 1;
                if( nBand <= nIconCount || (nIconCount == 1 && nBand != 4) )
                {
                    VRTAddSimpleSource( (VRTSourcedRasterBandH) poVRTDS->GetRasterBand(iBandIdx + 1),
                                        (GDALRasterBandH) poDSIcon->GetRasterBand(nBand <= nIconCount ? nBand : 1),
                                        0, 0,
                                        nRasterXSize,
                                        nRasterYSize,
                                        0, 0,
                                        poVRTDS->GetRasterXSize(),
                                        poVRTDS->GetRasterYSize(),
                                        NULL, VRT_NODATA_UNSET);
                }
                else
                {
                    VRTAddComplexSource( (VRTSourcedRasterBandH) poVRTDS->GetRasterBand(iBandIdx + 1),
                                        (GDALRasterBandH) poDSIcon->GetRasterBand(1),
                                        0, 0,
                                        nRasterXSize,
                                        nRasterYSize,
                                        0, 0,
                                        poVRTDS->GetRasterXSize(),
                                        poVRTDS->GetRasterYSize(),
                                        VRT_NODATA_UNSET, 0, 255);
                }
            }

            for(i=0; i < (int)aosImages.size(); i++)
            {
                int nDstXOff = (int)((aosImages[i].adfExtents[0] - adfGeoTransform[0]) / dfNewXRes + 0.5);
                int nDstYOff = (int)((adfGeoTransform[3] - aosImages[i].adfExtents[3]) / dfNewYRes + 0.5);
                int nDstXSize = (int)((aosImages[i].adfExtents[2] - aosImages[i].adfExtents[0]) / dfNewXRes + 0.5);
                int nDstYSize = (int)((aosImages[i].adfExtents[3] - aosImages[i].adfExtents[1]) / dfNewYRes + 0.5);

                int nSrcBandCount = aosImages[i].poDS->GetRasterCount();
                for(int iBandIdx = 0; iBandIdx < 4; iBandIdx++ )
                {
                    int nBand = iBandIdx + 1;
                    if( nBand <= nSrcBandCount || (nSrcBandCount == 1 && nBand != 4) )
                    {
                        VRTAddSimpleSource( (VRTSourcedRasterBandH) poVRTDS->GetRasterBand(iBandIdx + 1),
                                            (GDALRasterBandH) aosImages[i].poDS->GetRasterBand(nBand <= nSrcBandCount ? nBand : 1),
                                            0, 0,
                                            aosImages[i].poDS->GetRasterXSize(),
                                            aosImages[i].poDS->GetRasterYSize(),
                                            nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                            NULL, VRT_NODATA_UNSET);
                    }
                    else
                    {
                        VRTAddComplexSource( (VRTSourcedRasterBandH) poVRTDS->GetRasterBand(iBandIdx + 1),
                                            (GDALRasterBandH) aosImages[i].poDS->GetRasterBand(1),
                                            0, 0,
                                            aosImages[i].poDS->GetRasterXSize(),
                                            aosImages[i].poDS->GetRasterYSize(),
                                            nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                            VRT_NODATA_UNSET, 0, 255);
                    }
                }
            }

            int nReqXOff = (int)(dfXOff * dfXFactor + 0.5);
            int nReqYOff = (int)(dfYOff * dfYFactor + 0.5);
            int nReqXSize = (int)(dfXSize * dfXFactor + 0.5);
            int nReqYSize = (int)(dfYSize * dfYFactor + 0.5);
            if( nReqXOff + nReqXSize > poVRTDS->GetRasterXSize() )
                nReqXSize = poVRTDS->GetRasterXSize() - nReqXOff;
            if( nReqYOff + nReqYSize > poVRTDS->GetRasterYSize() )
                nReqYSize = poVRTDS->GetRasterYSize() - nReqYOff;

            CPLErr eErr = poVRTDS->RasterIO( eRWFlag,
                                             nReqXOff,
                                             nReqYOff,
                                             nReqXSize,
                                             nReqYSize,
                                             pData, nBufXSize, nBufYSize, eBufType,
                                             nBandCount, panBandMap,
                                             nPixelSpace, nLineSpace, nBandSpace,
                                             psExtraArg);

            for(i=0; i < (int)aosImages.size(); i++)
            {
                 aosImages[i].poDS->Dereference();
            }

            delete poVRTDS;

            return eErr;
        }
    }

    GDALProgressFunc  pfnProgressGlobal = psExtraArg->pfnProgress;
    void             *pProgressDataGlobal = psExtraArg->pProgressData;
    CPLErr eErr = CE_None;

    for(int iBandIdx = 0; iBandIdx < nBandCount && eErr == CE_None; iBandIdx++ )
    {
        int nBand = panBandMap[iBandIdx];

        if( (nIconCount > 1 || nBand == 4) && nBand > nIconCount )
        {
            GByte nVal = (nBand == 4) ? 255 : 0;
            for(int j = 0; j < nBufYSize; j ++ )
            {
                GDALCopyWords( &nVal, GDT_Byte, 0,
                            ((GByte*) pData) + j * nLineSpace + iBandIdx * nBandSpace, eBufType, nPixelSpace,
                            nBufXSize );
            }
            continue;
        }

        int nIconBand = (nIconCount == 1) ? 1 : nBand;

        int nReqXOff = (int)(dfXOff + 0.5);
        int nReqYOff = (int)(dfYOff + 0.5);
        int nReqXSize = (int)(dfXSize + 0.5);
        int nReqYSize = (int)(dfYSize + 0.5);
        if( nReqXOff + nReqXSize > poDSIcon->GetRasterXSize() )
            nReqXSize = poDSIcon->GetRasterXSize() - nReqXOff;
        if( nReqYOff + nReqYSize > poDSIcon->GetRasterYSize() )
            nReqYSize = poDSIcon->GetRasterYSize() - nReqYOff;

        psExtraArg->pfnProgress = GDALScaledProgress;
        psExtraArg->pProgressData = 
            GDALCreateScaledProgress( 1.0 * iBandIdx / nBandCount,
                                      1.0 * (iBandIdx + 1) / nBandCount,
                                      pfnProgressGlobal,
                                      pProgressDataGlobal );

        eErr = poDSIcon->GetRasterBand(nIconBand)->RasterIO( eRWFlag,
                                                      nReqXOff,
                                                      nReqYOff,
                                                      nReqXSize,
                                                      nReqYSize,
                                                      ((GByte*) pData) + nBandSpace * iBandIdx,
                                                      nBufXSize, nBufYSize, eBufType,
                                                      nPixelSpace, nLineSpace,
                                                      psExtraArg);

        GDALDestroyScaledProgress( psExtraArg->pProgressData );
    }

    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    return eErr;

}

/************************************************************************/
/*                    KmlSuperOverlayFindRegionStart()                  */
/************************************************************************/

static
int KmlSuperOverlayFindRegionStartInternal(CPLXMLNode* psNode,
                                   CPLXMLNode** ppsRegion,
                                   CPLXMLNode** ppsDocument,
                                   CPLXMLNode** ppsGroundOverlay,
                                   CPLXMLNode** ppsLink)
{
    CPLXMLNode* psRegion = NULL;
    CPLXMLNode* psLink = NULL;
    CPLXMLNode* psGroundOverlay = NULL;
    if( strcmp(psNode->pszValue, "NetworkLink") == 0 &&
        (psRegion = CPLGetXMLNode(psNode, "Region")) != NULL &&
        (psLink = CPLGetXMLNode(psNode, "Link")) != NULL )
    {
        *ppsRegion = psRegion;
        *ppsLink = psLink;
        return TRUE;
    }
    if( strcmp(psNode->pszValue, "Document") == 0 &&
        (psRegion = CPLGetXMLNode(psNode, "Region")) != NULL &&
        (psGroundOverlay = CPLGetXMLNode(psNode, "GroundOverlay")) != NULL )
    {
        *ppsDocument = psNode;
        *ppsRegion = psRegion;
        *ppsGroundOverlay = psGroundOverlay;
        return TRUE;
    }

    CPLXMLNode* psIter = psNode->psChild;
    while(psIter != NULL)
    {
        if( psIter->eType == CXT_Element )
        {
            if( KmlSuperOverlayFindRegionStartInternal(psIter, ppsRegion, ppsDocument,
                                               ppsGroundOverlay, ppsLink) )
                return TRUE;
        }

        psIter = psIter->psNext;
    }

    return FALSE;
}


static
int KmlSuperOverlayFindRegionStart(CPLXMLNode* psNode,
                                   CPLXMLNode** ppsRegion,
                                   CPLXMLNode** ppsDocument,
                                   CPLXMLNode** ppsGroundOverlay,
                                   CPLXMLNode** ppsLink)
{
    CPLXMLNode* psIter = psNode;
    while(psIter != NULL)
    {
        if( psIter->eType == CXT_Element )
        {
            if( KmlSuperOverlayFindRegionStartInternal(psIter, ppsRegion, ppsDocument,
                                                       ppsGroundOverlay, ppsLink) )
                return TRUE;
        }

        psIter = psIter->psNext;
    }

    return FALSE;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int KmlSuperOverlayReadDataset::Identify(GDALOpenInfo * poOpenInfo)

{
    const char* pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    if( EQUAL(pszExt, "kmz") )
        return -1;
    if( poOpenInfo->nHeaderBytes == 0 )
        return FALSE;
    if( !EQUAL(pszExt, "kml") ||
        strstr((const char*)poOpenInfo->pabyHeader, "<kml") == NULL )
        return FALSE;
    
    for( int i=0;i<2;i++ )
    {
        if( strstr((const char*)poOpenInfo->pabyHeader, "<NetworkLink>") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "<Region>") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "<Link>") != NULL )
            return TRUE;
        
        if( strstr((const char*)poOpenInfo->pabyHeader, "<Document>") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "<Region>") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "<GroundOverlay>") != NULL )
            return TRUE;

        if( i == 0 && !poOpenInfo->TryToIngest(1024*10) )
            return FALSE;
    }
    
    return -1;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *KmlSuperOverlayReadDataset::Open(GDALOpenInfo * poOpenInfo)

{
    if( Identify(poOpenInfo) == FALSE )
        return NULL;

    return Open(poOpenInfo->pszFilename);
}

/************************************************************************/
/*                         KmlSuperOverlayLoadIcon()                    */
/************************************************************************/

#define BUFFER_SIZE 20000000

static
GDALDataset* KmlSuperOverlayLoadIcon(const char* pszBaseFilename, const char* pszIcon)
{
    const char* pszExt = CPLGetExtension(pszIcon);
    if( !EQUAL(pszExt, "png") && !EQUAL(pszExt, "jpg") && !EQUAL(pszExt, "jpeg") )
    {
        return NULL;
    }

    CPLString osSubFilename;
    if( STARTS_WITH(pszIcon, "http"))
        osSubFilename = CPLSPrintf("/vsicurl_streaming/%s", pszIcon);
    else
    {
        osSubFilename = CPLFormFilename(CPLGetPath(pszBaseFilename), pszIcon, NULL);
        osSubFilename = KMLRemoveSlash(osSubFilename);
    }

    VSILFILE* fp = VSIFOpenL(osSubFilename, "rb");
    if( fp == NULL )
    {
        return NULL;
    }
    GByte* pabyBuffer = (GByte*) VSIMalloc(BUFFER_SIZE);
    if( pabyBuffer == NULL )
    {
        VSIFCloseL(fp);
        return NULL;
    }
    int nRead = (int)VSIFReadL(pabyBuffer, 1, BUFFER_SIZE, fp);
    VSIFCloseL(fp);
    if( nRead == BUFFER_SIZE )
    {
        CPLFree(pabyBuffer);
        return NULL;
    }

    static int nInc = 0;
    osSubFilename = CPLSPrintf("/vsimem/kmlsuperoverlay/%d_%p", nInc++, pszBaseFilename);
    VSIFCloseL(VSIFileFromMemBuffer( osSubFilename, pabyBuffer, nRead, TRUE) );

    GDALDataset* poDSIcon = (GDALDataset* )GDALOpen(osSubFilename, GA_ReadOnly);
    if( poDSIcon == NULL )
    {
        VSIUnlink(osSubFilename);
        return NULL;
    }

    return poDSIcon;
}


/************************************************************************/
/*                    KmlSuperOverlayComputeDepth()                     */
/************************************************************************/

static void KmlSuperOverlayComputeDepth(CPLString osFilename,
                                        CPLXMLNode* psDocument,
                                        int& nLevel)
{
    CPLXMLNode* psIter = psDocument->psChild;
    while(psIter != NULL)
    {
        const char* pszHref = NULL;
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "NetworkLink") == 0 &&
            CPLGetXMLNode(psIter, "Region") != NULL &&
            (pszHref = CPLGetXMLValue(psIter, "Link.href", NULL)) != NULL )
        {
            const char* pszExt = CPLGetExtension(pszHref);
            if( EQUAL(pszExt, "kml") )
            {
                CPLString osSubFilename;
                if( STARTS_WITH(pszHref, "http"))
                    osSubFilename = CPLSPrintf("/vsicurl_streaming/%s", pszHref);
                else
                {
                    osSubFilename = CPLFormFilename(CPLGetPath(osFilename), pszHref, NULL),
                    osSubFilename = KMLRemoveSlash(osSubFilename);
                }

                VSILFILE* fp = VSIFOpenL(osSubFilename, "rb");
                if( fp != NULL )
                {
                    char* pszBuffer = (char*) CPLMalloc(BUFFER_SIZE+1);
                    int nRead = (int)VSIFReadL(pszBuffer, 1, BUFFER_SIZE, fp);
                    pszBuffer[nRead] = '\0';
                    VSIFCloseL(fp);
                    if( nRead == BUFFER_SIZE )
                    {
                        CPLFree(pszBuffer);
                    }
                    else
                    {
                        CPLXMLNode* psNode = CPLParseXMLString(pszBuffer);
                        CPLFree(pszBuffer);
                        if( psNode != NULL )
                        {
                            CPLXMLNode* psRegion = NULL;
                            CPLXMLNode* psNewDocument = NULL;
                            CPLXMLNode* psGroundOverlay = NULL;
                            CPLXMLNode* psLink = NULL;
                            if( KmlSuperOverlayFindRegionStart(psNode, &psRegion,
                                            &psNewDocument, &psGroundOverlay, &psLink) &&
                                psNewDocument != NULL && nLevel < 20 )
                            {
                                nLevel ++;
                                KmlSuperOverlayComputeDepth(osSubFilename, psNewDocument, nLevel);
                            }
                            CPLDestroyXMLNode(psNode);
                            break;
                        }
                    }
                }
            }
        }
        psIter = psIter->psNext;
    }
}

/************************************************************************/
/*                    KmlSingleDocRasterDataset                         */
/************************************************************************/

class KmlSingleDocRasterRasterBand;

struct KmlSingleDocRasterTilesDesc
{
    int nMaxJ_i;     /* i index at which a tile with max j is realized */
    int nMaxJ_j;     /* j index at which a tile with max j is realized */
    int nMaxI_i;     /* i index at which a tile with max i is realized */
    int nMaxI_j;     /* j index at which a tile with max i is realized */
    char szExtJ[4];  /* extension of tile at which max j is realized */
    char szExtI[4];  /* extension of tile at which max i is realized */
};

class KmlSingleDocRasterDataset: public GDALDataset
{
        friend class KmlSingleDocRasterRasterBand;
        CPLString osDirname;
        CPLString osNominalExt;
        GDALDataset* poCurTileDS;
        double adfGlobalExtents[4];
        double adfGeoTransform[6];
        std::vector<KmlSingleDocRasterDataset*> apoOverviews;
        std::vector<KmlSingleDocRasterTilesDesc> aosDescs;
        int nLevel;
        int nTileSize;
        int bHasBuiltOverviews;
        int bLockOtherBands;

  protected:
    virtual int         CloseDependentDatasets();

    public:
                KmlSingleDocRasterDataset();
                ~KmlSingleDocRasterDataset();

        virtual CPLErr GetGeoTransform( double * padfGeoTransform )
        {
            memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
            return CE_None;
        }

        virtual const char *GetProjectionRef() { return SRS_WKT_WGS84; }

        void BuildOverviews();

        static GDALDataset* Open(const char* pszFilename,
                                 const CPLString& osFilename,
                                 CPLXMLNode* psNode);
};

/************************************************************************/
/*                    KmlSingleDocRasterRasterBand                      */
/************************************************************************/

class KmlSingleDocRasterRasterBand: public GDALRasterBand
{
    public:
        KmlSingleDocRasterRasterBand(KmlSingleDocRasterDataset* poDS,
                                     int nBand);

        virtual CPLErr IReadBlock( int, int, void * );
        virtual GDALColorInterp GetColorInterpretation();

        virtual int GetOverviewCount();
        virtual GDALRasterBand *GetOverview(int);
};

/************************************************************************/
/*                        KmlSingleDocRasterDataset()                   */
/************************************************************************/

KmlSingleDocRasterDataset::KmlSingleDocRasterDataset()
{
    poCurTileDS = NULL;
    nLevel = 0;
    nTileSize = 0;
    bHasBuiltOverviews = FALSE;
    bLockOtherBands = FALSE;
}

/************************************************************************/
/*                       ~KmlSingleDocRasterDataset()                   */
/************************************************************************/

KmlSingleDocRasterDataset::~KmlSingleDocRasterDataset()
{
    CloseDependentDatasets();
}

/************************************************************************/
/*                         CloseDependentDatasets()                     */
/************************************************************************/

int KmlSingleDocRasterDataset::CloseDependentDatasets()
{
    int bRet = FALSE;

    if( poCurTileDS != NULL )
    {
        bRet = TRUE;
        GDALClose((GDALDatasetH) poCurTileDS);
        poCurTileDS = NULL;
    }
    if( apoOverviews.size() > 0 )
    {
        bRet = TRUE;
        for(size_t i = 0; i < apoOverviews.size(); i++)
            delete apoOverviews[i];
        apoOverviews.resize(0);
    }

    return bRet;
}

/************************************************************************/
/*                     KmlSingleDocGetDimensions()                      */
/************************************************************************/

static int KmlSingleDocGetDimensions(const CPLString& osDirname,
                                     const KmlSingleDocRasterTilesDesc& oDesc,
                                     int nLevel,
                                     int nTileSize,
                                     int& nXSize,
                                     int& nYSize,
                                     int& nBands,
                                     int& bHasCT)
{
    const char* pszImageFilename = CPLFormFilename( osDirname,
            CPLSPrintf("kml_image_L%d_%d_%d", nLevel,
                    oDesc.nMaxJ_j,
                    oDesc.nMaxJ_i),
                    oDesc.szExtJ );
    GDALDataset* poImageDS = (GDALDataset*) GDALOpen(pszImageFilename, GA_ReadOnly);
    if( poImageDS == NULL )
    {
        return FALSE;
    }
    int nRightXSize;
    int nBottomYSize = poImageDS->GetRasterYSize();
    nBands = poImageDS->GetRasterCount();
    bHasCT = (nBands == 1 && poImageDS->GetRasterBand(1)->GetColorTable() != NULL);
    if( oDesc.nMaxJ_j == oDesc.nMaxI_j &&
        oDesc.nMaxJ_i == oDesc.nMaxI_i)
    {
        nRightXSize = poImageDS->GetRasterXSize();
    }
    else
    {
        GDALClose( (GDALDatasetH) poImageDS) ;
        pszImageFilename = CPLFormFilename( osDirname,
            CPLSPrintf("kml_image_L%d_%d_%d", nLevel,
                    oDesc.nMaxI_j,
                    oDesc.nMaxI_i),
                    oDesc.szExtI );
        poImageDS = (GDALDataset*) GDALOpen(pszImageFilename, GA_ReadOnly);
        if( poImageDS == NULL )
        {
            return FALSE;
        }
        nRightXSize = poImageDS->GetRasterXSize();
    }
    GDALClose( (GDALDatasetH) poImageDS) ;

    nXSize = nRightXSize + oDesc.nMaxI_i * nTileSize;
    nYSize = nBottomYSize + oDesc.nMaxJ_j * nTileSize;
    return (nXSize > 0 && nYSize > 0);
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

void KmlSingleDocRasterDataset::BuildOverviews()
{
    if( bHasBuiltOverviews )
        return;
    bHasBuiltOverviews = TRUE;

    for(int k = 2; k <= (int)aosDescs.size(); k++)
    {
        const KmlSingleDocRasterTilesDesc& oDesc = aosDescs[aosDescs.size()-k];
        int nXSize = 0, nYSize = 0, nTileBands = 0, bHasCT = FALSE;
        if( !KmlSingleDocGetDimensions(osDirname, oDesc, (int)aosDescs.size() - k  + 1,
                                       nTileSize,
                                       nXSize, nYSize, nTileBands, bHasCT) )
        {
            break;
        }

        KmlSingleDocRasterDataset* poOvrDS = new KmlSingleDocRasterDataset();
        poOvrDS->nRasterXSize = nXSize;
        poOvrDS->nRasterYSize = nYSize;
        poOvrDS->nLevel = (int)aosDescs.size() - k +  1;
        poOvrDS->nTileSize = nTileSize;
        poOvrDS->osDirname = osDirname;
        poOvrDS->osNominalExt = oDesc.szExtI;
        poOvrDS->adfGeoTransform[0] = adfGlobalExtents[0];
        poOvrDS->adfGeoTransform[1] = (adfGlobalExtents[2] - adfGlobalExtents[0]) / poOvrDS->nRasterXSize;
        poOvrDS->adfGeoTransform[2] = 0.0;
        poOvrDS->adfGeoTransform[3] = adfGlobalExtents[3];
        poOvrDS->adfGeoTransform[4] = 0.0;
        poOvrDS->adfGeoTransform[5] = -(adfGlobalExtents[3] - adfGlobalExtents[1]) / poOvrDS->nRasterXSize;
        for(int iBand = 1; iBand <= nBands; iBand ++ )
            poOvrDS->SetBand(iBand, new KmlSingleDocRasterRasterBand(poOvrDS, iBand));
        poOvrDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );

        apoOverviews.push_back(poOvrDS);
    }
}

/************************************************************************/
/*                      KmlSingleDocRasterRasterBand()                  */
/************************************************************************/

KmlSingleDocRasterRasterBand::KmlSingleDocRasterRasterBand(KmlSingleDocRasterDataset* poDS,
                                                           int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    nBlockXSize = poDS->nTileSize;
    nBlockYSize = poDS->nTileSize;
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                               IReadBlock()                           */
/************************************************************************/

CPLErr KmlSingleDocRasterRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                                 void * pImage )
{
    KmlSingleDocRasterDataset* poGDS = (KmlSingleDocRasterDataset*) poDS;
    const char* pszImageFilename = CPLFormFilename( poGDS->osDirname,
        CPLSPrintf("kml_image_L%d_%d_%d", poGDS->nLevel, nBlockYOff, nBlockXOff), poGDS->osNominalExt );
    if( poGDS->poCurTileDS == NULL || 
        strcmp(CPLGetFilename(poGDS->poCurTileDS->GetDescription()),
               CPLGetFilename(pszImageFilename)) != 0 )
    {
        if( poGDS->poCurTileDS != NULL ) GDALClose((GDALDatasetH) poGDS->poCurTileDS);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        poGDS->poCurTileDS = (GDALDataset*) GDALOpen(pszImageFilename, GA_ReadOnly);
        CPLPopErrorHandler();
    }
    GDALDataset* poImageDS = poGDS->poCurTileDS;
    if( poImageDS == NULL )
    {
        memset( pImage, 0, nBlockXSize * nBlockYSize );
        return CE_None;
    }
    int nXSize = poImageDS->GetRasterXSize();
    int nYSize = poImageDS->GetRasterYSize();

    int nReqXSize = nBlockXSize;
    if( nBlockXOff * nBlockXSize + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nBlockXOff * nBlockXSize;
    int nReqYSize = nBlockYSize;
    if( nBlockYOff * nBlockYSize + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nBlockYOff * nBlockYSize;

    if( nXSize != nReqXSize || nYSize != nReqYSize )
    {
        CPLDebug("KMLSUPEROVERLAY", "Tile %s, dimensions %dx%d, expected %dx%d",
                 pszImageFilename, nXSize, nYSize, nReqXSize, nReqYSize);
        return CE_Failure;
    }

    CPLErr eErr = CE_Failure;
    if( poImageDS->GetRasterCount() == 1 )
    {
        GDALColorTable* poColorTable = poImageDS->GetRasterBand(1)->GetColorTable();
        if( nBand == 4 && poColorTable == NULL )
        {
            /* Add fake alpha band */
            memset( pImage, 255, nBlockXSize * nBlockYSize );
            eErr = CE_None;
        }
        else
        {
            eErr = poImageDS->GetRasterBand(1)->RasterIO(GF_Read,
                                                    0, 0, nXSize, nYSize,
                                                    pImage,
                                                    nXSize, nYSize,
                                                    GDT_Byte, 1, nBlockXSize, NULL);

            /* Expand color table */
            if( eErr == CE_None && poColorTable != NULL )
            {
                int j, i;
                for(j = 0; j < nReqYSize; j++ )
                {
                    for(i = 0; i < nReqXSize; i++ )
                    {
                        GByte nVal = ((GByte*) pImage)[j * nBlockXSize + i];
                        const GDALColorEntry * poEntry = poColorTable->GetColorEntry(nVal);
                        if( poEntry != NULL )
                        {
                            if( nBand == 1 )
                                ((GByte*) pImage)[j * nBlockXSize + i] = poEntry->c1;
                            else if( nBand == 2 )
                                ((GByte*) pImage)[j * nBlockXSize + i] = poEntry->c2;
                            else if( nBand == 3 )
                                ((GByte*) pImage)[j * nBlockXSize + i] = poEntry->c3;
                            else
                                ((GByte*) pImage)[j * nBlockXSize + i] = poEntry->c4;
                        }
                    }
                }
            }
        }
    }
    else if( nBand <= poImageDS->GetRasterCount() )
    {
        eErr = poImageDS->GetRasterBand(nBand)->RasterIO(GF_Read,
                                                0, 0, nXSize, nYSize,
                                                pImage,
                                                nXSize, nYSize,
                                                GDT_Byte, 1, nBlockXSize, NULL);
    }
    else if( nBand == 4 && poImageDS->GetRasterCount() == 3 )
    {
        /* Add fake alpha band */
        memset( pImage, 255, nBlockXSize * nBlockYSize );
        eErr = CE_None;
    }

    /* Cache other bands */
    if( !poGDS->bLockOtherBands )
    {
        poGDS->bLockOtherBands = TRUE;
        for(int iBand = 1; iBand <= poGDS->nBands; iBand ++ )
        {
            if( iBand != nBand )
            {
                KmlSingleDocRasterRasterBand* poOtherBand =
                    (KmlSingleDocRasterRasterBand*)poGDS->GetRasterBand(iBand);
                GDALRasterBlock* poBlock = poOtherBand->
                                    GetLockedBlockRef(nBlockXOff, nBlockYOff);
                if( poBlock == NULL )
                    continue;
                poBlock->DropLock();
            }
        }
        poGDS->bLockOtherBands = FALSE;
    }

    return eErr;
}

/************************************************************************/
/*                          GetColorInterpretation()                    */
/************************************************************************/

GDALColorInterp  KmlSingleDocRasterRasterBand::GetColorInterpretation()
{
    return (GDALColorInterp)(GCI_RedBand + nBand - 1);
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int KmlSingleDocRasterRasterBand::GetOverviewCount()
{
    KmlSingleDocRasterDataset* poGDS = (KmlSingleDocRasterDataset*) poDS;
    poGDS->BuildOverviews();

    return (int)poGDS->apoOverviews.size();
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand *KmlSingleDocRasterRasterBand::GetOverview(int iOvr)
{
    KmlSingleDocRasterDataset* poGDS = (KmlSingleDocRasterDataset*) poDS;
    poGDS->BuildOverviews();

    if( iOvr < 0 || iOvr >= (int)poGDS->apoOverviews.size() )
        return NULL;

    return poGDS->apoOverviews[iOvr]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       KmlSingleDocCollectTiles()                     */
/************************************************************************/

static void KmlSingleDocCollectTiles(CPLXMLNode* psNode,
                                     std::vector<KmlSingleDocRasterTilesDesc>& aosDescs,
                                     CPLString& osURLBase)
{
    if( strcmp(psNode->pszValue, "href") == 0 )
    {
        int level, j, i;
        char szExt[4];
        const char* pszHref = CPLGetXMLValue(psNode, "", "");
        if( STARTS_WITH(pszHref, "http") )
        {
            osURLBase = CPLGetPath(pszHref);
        }
        if( sscanf(CPLGetFilename(pszHref), "kml_image_L%d_%d_%d.%3s",
                   &level, &j, &i, szExt) == 4 )
        {
            if( level > (int)aosDescs.size() )
            {
                KmlSingleDocRasterTilesDesc sDesc;
                while( level > (int)aosDescs.size() + 1 )
                {
                    sDesc.nMaxJ_i = -1;
                    sDesc.nMaxJ_j = -1;
                    sDesc.nMaxI_i = -1;
                    sDesc.nMaxI_j = -1;
                    strcpy(sDesc.szExtI, "");
                    strcpy(sDesc.szExtJ, "");
                    aosDescs.push_back(sDesc);
                }

                sDesc.nMaxJ_j = j;
                sDesc.nMaxJ_i = i;
                strcpy(sDesc.szExtJ, szExt);
                sDesc.nMaxI_j = j;
                sDesc.nMaxI_i = i;
                strcpy(sDesc.szExtI, szExt);
                aosDescs.push_back(sDesc);
            }
            else
            {
                /* 2010_USACE_JALBTCX_Louisiana_Mississippi_Lidar.kmz has not a lower-right tile */
                /* so the right most tile and the bottom most tile might be different */
                if( (j > aosDescs[level-1].nMaxJ_j) ||
                    (j == aosDescs[level-1].nMaxJ_j &&
                     i > aosDescs[level-1].nMaxJ_i) )
                {
                    aosDescs[level-1].nMaxJ_j = j;
                    aosDescs[level-1].nMaxJ_i = i;
                    strcpy(aosDescs[level-1].szExtJ, szExt);
                }
                if( i > aosDescs[level-1].nMaxI_i ||
                   (i == aosDescs[level-1].nMaxI_i &&
                    j > aosDescs[level-1].nMaxI_j) )
                {
                    aosDescs[level-1].nMaxI_j = j;
                    aosDescs[level-1].nMaxI_i = i;
                    strcpy(aosDescs[level-1].szExtI, szExt);
                }
            }
        }
    }
    else
    {
        CPLXMLNode* psIter = psNode->psChild;
        while(psIter != NULL)
        {
            if( psIter->eType == CXT_Element )
                KmlSingleDocCollectTiles(psIter, aosDescs, osURLBase);
            psIter = psIter->psNext;
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/* Read raster with a structure like http://opentopo.sdsc.edu/files/Haiti/NGA_Haiti_LiDAR2.kmz */
/* i.e. made of a doc.kml that list all tiles at all overview levels */
/* The tile name pattern is "kml_image_L{level}_{j}_{i}.{png|jpg}" */
GDALDataset* KmlSingleDocRasterDataset::Open(const char* pszFilename,
                                             const CPLString& osFilename,
                                             CPLXMLNode* psRoot)
{
    CPLXMLNode* psRootFolder = CPLGetXMLNode(psRoot, "=kml.Document.Folder");
    if( psRootFolder == NULL )
        return NULL;
    const char* pszRootFolderName =
        CPLGetXMLValue(psRootFolder, "name", "");
    if( strcmp(pszRootFolderName, "kml_image_L1_0_0") != 0 )
        return NULL;

    double adfGlobalExtents[4];
    CPLXMLNode* psRegion = CPLGetXMLNode(psRootFolder, "Region");
    if( psRegion == NULL )
        return NULL;
    if( !KmlSuperOverlayGetBoundingBox(psRegion, adfGlobalExtents) )
        return NULL;

    std::vector<KmlSingleDocRasterTilesDesc> aosDescs;
    CPLString osDirname = CPLGetPath(osFilename);
    KmlSingleDocCollectTiles(psRootFolder, aosDescs, osDirname);
    if( aosDescs.size() == 0 )
        return NULL;
    int k;
    for(k = 0; k < (int)aosDescs.size(); k++)
    {
        if( aosDescs[k].nMaxJ_i < 0 )
            return NULL;
    }

    const char* pszImageFilename = CPLFormFilename( osDirname,
            CPLSPrintf("kml_image_L%d_%d_%d", (int)aosDescs.size(), 0, 0), aosDescs[aosDescs.size()-1].szExtI);
    GDALDataset* poImageDS = (GDALDataset*) GDALOpen(pszImageFilename, GA_ReadOnly);
    if( poImageDS == NULL )
    {
        return NULL;
    }
    int nTileSize = poImageDS->GetRasterXSize();
    if( nTileSize != poImageDS->GetRasterYSize() )
    {
        nTileSize = 1024;
    }
    GDALClose( (GDALDatasetH) poImageDS) ;

    const KmlSingleDocRasterTilesDesc& oDesc = aosDescs[aosDescs.size()-1];
    int nXSize = 0, nYSize = 0, nBands = 0, bHasCT = FALSE;
    if( !KmlSingleDocGetDimensions(osDirname, oDesc, (int)aosDescs.size(), nTileSize,
                                   nXSize, nYSize, nBands, bHasCT) )
    {
        return NULL;
    }

    KmlSingleDocRasterDataset* poDS = new KmlSingleDocRasterDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nLevel = (int)aosDescs.size();
    poDS->nTileSize = nTileSize;
    poDS->osDirname = osDirname;
    poDS->osNominalExt = oDesc.szExtI;
    memcpy(poDS->adfGlobalExtents, adfGlobalExtents, 4 * sizeof(double));
    poDS->adfGeoTransform[0] = adfGlobalExtents[0];
    poDS->adfGeoTransform[1] = (adfGlobalExtents[2] - adfGlobalExtents[0]) / poDS->nRasterXSize;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = adfGlobalExtents[3];
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -(adfGlobalExtents[3] - adfGlobalExtents[1]) / poDS->nRasterYSize;
    if( nBands == 1 && bHasCT ) nBands = 4;
    for(int iBand = 1; iBand <= nBands; iBand ++ )
        poDS->SetBand(iBand, new KmlSingleDocRasterRasterBand(poDS, iBand));
    poDS->SetDescription(pszFilename);
    poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    poDS->aosDescs = aosDescs;

    return poDS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *KmlSuperOverlayReadDataset::Open(const char* pszFilename,
                                              KmlSuperOverlayReadDataset* poParent,
                                              int nRec)

{
    if( nRec == 2 )
        return NULL;
    CPLString osFilename(pszFilename);
    const char* pszExt = CPLGetExtension(pszFilename);
    if( EQUAL(pszExt, "kmz") )
    {
        if( !STARTS_WITH(pszFilename, "/vsizip/") )
            osFilename = CPLSPrintf("/vsizip/%s", pszFilename);
        char** papszFiles = CPLReadDir(osFilename);
        if( papszFiles == NULL )
            return NULL;
        char** papszIter = papszFiles;
        for(; *papszIter != NULL; papszIter ++)
        {
            pszExt = CPLGetExtension(*papszIter);
            if( EQUAL(pszExt, "kml") )
            {
                osFilename = CPLFormFilename(osFilename, *papszIter, NULL);
                osFilename = KMLRemoveSlash(osFilename);
                break;
            }
        }
        CSLDestroy(papszFiles);
    }
    VSILFILE* fp = VSIFOpenL(osFilename, "rb");
    if( fp == NULL )
        return NULL;
    char* pszBuffer = (char*) CPLMalloc(BUFFER_SIZE+1);
    int nRead = (int)VSIFReadL(pszBuffer, 1, BUFFER_SIZE, fp);
    pszBuffer[nRead] = '\0';
    VSIFCloseL(fp);
    if( nRead == BUFFER_SIZE )
    {
        CPLFree(pszBuffer);
        return NULL;
    }

    CPLXMLNode* psNode = CPLParseXMLString(pszBuffer);
    CPLFree(pszBuffer);
    if( psNode == NULL )
        return NULL;

    CPLXMLNode* psRegion = NULL;
    CPLXMLNode* psDocument = NULL;
    CPLXMLNode* psGroundOverlay = NULL;
    CPLXMLNode* psLink = NULL;
    if( !KmlSuperOverlayFindRegionStart(psNode, &psRegion,
                                        &psDocument, &psGroundOverlay, &psLink) )
    {
        GDALDataset* psDS = KmlSingleDocRasterDataset::Open(pszFilename,
                                                            osFilename,
                                                            psNode);
        CPLDestroyXMLNode(psNode);
        return psDS;
    }

    if( psLink != NULL )
    {
        const char* pszHref = CPLGetXMLValue(psLink, "href", NULL);
        if( pszHref == NULL || !EQUAL(CPLGetExtension(pszHref), "kml") )
        {
            CPLDestroyXMLNode(psNode);
            return NULL;
        }

        CPLString osSubFilename;
        if( STARTS_WITH(pszHref, "http"))
            osSubFilename = CPLSPrintf("/vsicurl_streaming/%s", pszHref);
        else
        {
            osSubFilename = CPLFormFilename(CPLGetPath(osFilename), pszHref, NULL);
            osSubFilename = KMLRemoveSlash(osSubFilename);
        }

        CPLString osOverlayName, osOverlayDescription;
        psDocument = CPLGetXMLNode(psNode, "=kml.Document");
        if( psDocument )
        {
            const char* pszOverlayName = CPLGetXMLValue(psDocument, "name", NULL);
            if( pszOverlayName != NULL &&
                strcmp(pszOverlayName, CPLGetBasename(pszFilename)) != 0 )
            {
                osOverlayName = pszOverlayName;
            }
            const char* pszOverlayDescription = CPLGetXMLValue(psDocument, "description", NULL);
            if( pszOverlayDescription != NULL )
            {
                osOverlayDescription = pszOverlayDescription;
            }
        }

        CPLDestroyXMLNode(psNode);

        // FIXME
        GDALDataset* poDS = Open(osSubFilename, poParent, nRec + 1);
        if( poDS != NULL )
        {
            poDS->SetDescription(pszFilename);

            if( osOverlayName.size() )
            {
                poDS->SetMetadataItem( "NAME", osOverlayName);
            }
            if( osOverlayDescription.size() )
            {
                poDS->SetMetadataItem( "DESCRIPTION", osOverlayDescription);
            }
        }

        return poDS;
    }

    CPLAssert(psDocument != NULL);
    CPLAssert(psGroundOverlay != NULL);
    CPLAssert(psRegion != NULL);

    double adfExtents[4];
    if( !KmlSuperOverlayGetBoundingBox(psGroundOverlay, adfExtents) )
    {
        CPLDestroyXMLNode(psNode);
        return NULL;
    }

    const char* pszIcon = CPLGetXMLValue(psGroundOverlay, "Icon.href", NULL);
    if( pszIcon == NULL )
    {
        CPLDestroyXMLNode(psNode);
        return NULL;
    }
    GDALDataset* poDSIcon = KmlSuperOverlayLoadIcon(pszFilename,  pszIcon);
    if( poDSIcon == NULL )
    {
        CPLDestroyXMLNode(psNode);
        return NULL;
    }

    int nFactor;
    if( poParent != NULL )
        nFactor = poParent->nFactor / 2;
    else
    {
        int nDepth = 0;
        KmlSuperOverlayComputeDepth(pszFilename, psDocument, nDepth);
        nFactor = 1 << nDepth;
    }

    KmlSuperOverlayReadDataset* poDS = new KmlSuperOverlayReadDataset();
    poDS->osFilename = pszFilename;
    poDS->psRoot = psNode;
    poDS->psDocument = psDocument;
    poDS->poDSIcon = poDSIcon;
    poDS->poParent = poParent;
    poDS->nFactor = nFactor;
    poDS->nRasterXSize = nFactor * poDSIcon->GetRasterXSize();
    poDS->nRasterYSize = nFactor * poDSIcon->GetRasterYSize();
    poDS->adfGeoTransform[0] = adfExtents[0];
    poDS->adfGeoTransform[1] = (adfExtents[2] - adfExtents[0]) / poDS->nRasterXSize;
    poDS->adfGeoTransform[3] = adfExtents[3];
    poDS->adfGeoTransform[5] = -(adfExtents[3] - adfExtents[1]) / poDS->nRasterYSize;
    poDS->nBands = 4;
    for(int i=0;i<4;i++)
        poDS->SetBand(i+1, new KmlSuperOverlayRasterBand(poDS, i+1));
    poDS->SetDescription(pszFilename);
    poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );

    while( poDS->poParent == NULL && nFactor > 1 )
    {
        nFactor /= 2;

        KmlSuperOverlayReadDataset* poOvrDS = new KmlSuperOverlayReadDataset();

        poDS->papoOverviewDS = (KmlSuperOverlayReadDataset**) CPLRealloc(
            poDS->papoOverviewDS, (poDS->nOverviewCount + 1) * sizeof(KmlSuperOverlayReadDataset*));
        poDS->papoOverviewDS[poDS->nOverviewCount ++] = poOvrDS;

        poOvrDS->bIsOvr = TRUE;
        poOvrDS->poParent = poDS;
        poOvrDS->nFactor = nFactor;
        poOvrDS->nRasterXSize = nFactor * poDSIcon->GetRasterXSize();
        poOvrDS->nRasterYSize = nFactor * poDSIcon->GetRasterYSize();
        poOvrDS->adfGeoTransform[0] = adfExtents[0];
        poOvrDS->adfGeoTransform[1] = (adfExtents[2] - adfExtents[0]) / poOvrDS->nRasterXSize;
        poOvrDS->adfGeoTransform[3] = adfExtents[3];
        poOvrDS->adfGeoTransform[5] = -(adfExtents[3] - adfExtents[1]) / poOvrDS->nRasterYSize;
        poOvrDS->nBands = 4;
        for(int i=0;i<4;i++)
            poOvrDS->SetBand(i+1, new KmlSuperOverlayRasterBand(poOvrDS, i+1));
        poOvrDS->SetDescription(pszFilename);
        poOvrDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }

    return poDS;
}

/************************************************************************/
/*                    KmlSuperOverlayDatasetDelete()                    */
/************************************************************************/

static CPLErr KmlSuperOverlayDatasetDelete(CPL_UNUSED const char* fileName)
{
    /* Null implementation, so that people can Delete("MEM:::") */
    return CE_None;
}

/************************************************************************/
/*                    GDALRegister_KMLSUPEROVERLAY()                    */
/************************************************************************/

void GDALRegister_KMLSUPEROVERLAY()
   
{
    GDALDriver	*poDriver;
   
    if( GDALGetDriverByName( "KMLSUPEROVERLAY" ) == NULL )
    {
        poDriver = new GDALDriver();
      
        poDriver->SetDescription( "KMLSUPEROVERLAY" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Kml Super Overlay" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='NAME' type='string' description='Overlay name'/>"
"   <Option name='DESCRIPTION' type='string' description='Overlay description'/>"
"   <Option name='ALTITUDE' type='float' description='Distance above the earth surface, in meters, interpreted according to the altitude mode'/>"
"   <Option name='ALTITUDEMODE' type='string-select' default='clampToGround' description='Specifies hows the altitude is interpreted'>"
"       <Value>clampToGround</Value>"
"       <Value>absolute</Value>"
"       <Value>relativeToSeaFloor</Value>"
"       <Value>clampToSeaFloor</Value>"
"   </Option>"
"   <Option name='FORMAT' type='string-select' default='JPEG' description='Format of the tiles'>"
"       <Value>PNG</Value>"
"       <Value>JPEG</Value>"
"   </Option>"
"   <Option name='FIX_ANTIMERIDIAN' type='boolean' description='Fix for images crossing the antimeridian causing errors in Google Earth' />"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = KmlSuperOverlayReadDataset::Identify;
        poDriver->pfnOpen = KmlSuperOverlayReadDataset::Open;
        poDriver->pfnCreateCopy = KmlSuperOverlayCreateCopy;
        poDriver->pfnDelete = KmlSuperOverlayDatasetDelete;
      
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
