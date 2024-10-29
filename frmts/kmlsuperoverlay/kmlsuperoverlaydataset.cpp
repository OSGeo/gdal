/******************************************************************************
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "kmlsuperoverlaydataset.h"

#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "../vrt/gdal_vrt.h"
#include "../vrt/vrtdataset.h"

/************************************************************************/
/*                           GenerateTiles()                            */
/************************************************************************/
static void GenerateTiles(const std::string &filename, CPL_UNUSED int zoom,
                          int rxsize, int rysize, CPL_UNUSED int ix,
                          CPL_UNUSED int iy, int rx, int ry, int dxsize,
                          int dysize, int bands, GDALDataset *poSrcDs,
                          GDALDriver *poOutputTileDriver,
                          GDALDriver *poMemDriver, bool isJpegDriver)
{
    GDALRasterBand *alphaBand = nullptr;

    std::vector<GByte> abyScanline(dxsize);
    std::vector<bool> hadnoData(dxsize);

    if (isJpegDriver && bands == 4)
        bands = 3;

    auto poTmpDataset = std::unique_ptr<GDALDataset>(
        poMemDriver->Create("", dxsize, dysize, bands, GDT_Byte, nullptr));

    if (!isJpegDriver)  // Jpeg dataset only has one or three bands
    {
        if (bands < 4)  // add transparency to files with one band or three
                        // bands
        {
            poTmpDataset->AddBand(GDT_Byte);
            alphaBand =
                poTmpDataset->GetRasterBand(poTmpDataset->GetRasterCount());
        }
    }

    const int rowOffset = rysize / dysize;
    const int loopCount = rysize / rowOffset;
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
            GDALRasterBand *poBand = poSrcDs->GetRasterBand(band);
            int hasNoData = 0;
            const double noDataValue = poBand->GetNoDataValue(&hasNoData);

            int yOffset = ry + row * rowOffset;
            CPLErr errTest = poBand->RasterIO(
                GF_Read, rx, yOffset, rxsize, rowOffset, abyScanline.data(),
                dxsize, 1, GDT_Byte, 0, 0, nullptr);

            const bool bReadFailed = (errTest == CE_Failure);
            if (bReadFailed)
            {
                hasNoData = 1;
            }

            // fill the true or false for hadnoData array if the source data has
            // nodata value
            if (!isJpegDriver)
            {
                if (hasNoData == 1)
                {
                    for (int j = 0; j < dxsize; j++)
                    {
                        double v = abyScanline[j];
                        if (v == noDataValue || bReadFailed)
                        {
                            hadnoData[j] = true;
                        }
                    }
                }
            }

            if (!bReadFailed)
            {
                GDALRasterBand *poBandtmp = poTmpDataset->GetRasterBand(band);
                CPL_IGNORE_RET_VAL(poBandtmp->RasterIO(
                    GF_Write, 0, row, dxsize, 1, abyScanline.data(), dxsize, 1,
                    GDT_Byte, 0, 0, nullptr));
            }
        }

        // fill the values for alpha band
        if (!isJpegDriver)
        {
            if (alphaBand)
            {
                for (int i = 0; i < dxsize; i++)
                {
                    if (hadnoData[i])
                    {
                        abyScanline[i] = 0;
                    }
                    else
                    {
                        abyScanline[i] = 255;
                    }
                }

                CPL_IGNORE_RET_VAL(alphaBand->RasterIO(
                    GF_Write, 0, row, dxsize, 1, abyScanline.data(), dxsize, 1,
                    GDT_Byte, 0, 0, nullptr));
            }
        }
    }

    CPLConfigOptionSetter oSetter("GDAL_OPEN_AFTER_COPY", "NO", false);
    /* to prevent CreateCopy() from calling QuietDelete() */
    const char *const apszOptions[] = {"@QUIET_DELETE_ON_CREATE_COPY=NO",
                                       nullptr};
    std::unique_ptr<GDALDataset>(
        poOutputTileDriver->CreateCopy(filename.c_str(), poTmpDataset.get(),
                                       FALSE, apszOptions, nullptr, nullptr));
}

/************************************************************************/
/*                          GenerateRootKml()                           */
/************************************************************************/

static int GenerateRootKml(const char *filename, const char *kmlfilename,
                           double north, double south, double east, double west,
                           int tilesize, const char *pszOverlayName,
                           const char *pszOverlayDescription)
{
    VSILFILE *fp = VSIFOpenL(filename, "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", filename);
        return FALSE;
    }
    int minlodpixels = tilesize / 2;

    const char *tmpfilename = CPLGetBasename(kmlfilename);
    if (pszOverlayName == nullptr)
        pszOverlayName = tmpfilename;

    // If we have not written any features yet, output the layer's schema.
    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
    VSIFPrintfL(fp, "\t<Document>\n");
    char *pszEncoded = CPLEscapeString(pszOverlayName, -1, CPLES_XML);
    VSIFPrintfL(fp, "\t\t<name>%s</name>\n", pszEncoded);
    CPLFree(pszEncoded);
    if (pszOverlayDescription == nullptr)
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
    VSIFPrintfL(fp, "\t\t\t\t\t<minLodPixels>%d</minLodPixels>\n",
                minlodpixels);
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

static int GenerateChildKml(
    const std::string &filename, int zoom, int ix, int iy, double zoomxpixel,
    double zoomypixel, int dxsize, int dysize, double south, double west,
    int xsize, int ysize, int maxzoom, OGRCoordinateTransformation *poTransform,
    const std::string &fileExt, bool fixAntiMeridian, const char *pszAltitude,
    const char *pszAltitudeMode,
    const std::vector<std::pair<std::pair<int, int>, bool>> &childTiles)
{
    double tnorth = south + zoomypixel * ((iy + 1) * dysize);
    double tsouth = south + zoomypixel * (iy * dysize);
    double teast = west + zoomxpixel * ((ix + 1) * dxsize);
    double twest = west + zoomxpixel * ix * dxsize;

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

    if (fixAntiMeridian && teast < twest)
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
    if (zoom < maxzoom)
    {
        double zareasize = pow(2.0, (maxzoom - zoom - 1)) * dxsize;
        double zareasize1 = pow(2.0, (maxzoom - zoom - 1)) * dysize;
        xchildren.push_back(ix * 2);
        int tmp = ix * 2 + 1;
        int tmp1 = static_cast<int>(ceil(xsize / zareasize));
        if (tmp < tmp1)
        {
            xchildren.push_back(ix * 2 + 1);
        }
        ychildern.push_back(iy * 2);
        tmp = iy * 2 + 1;
        tmp1 = static_cast<int>(ceil(ysize / zareasize1));
        if (tmp < tmp1)
        {
            ychildern.push_back(iy * 2 + 1);
        }
        maxLodPix = 2048;

        bool hasChildKML = false;
        for (const auto &kv : childTiles)
        {
            if (kv.second)
            {
                hasChildKML = true;
                break;
            }
        }
        if (!hasChildKML)
        {
            // no child KML files, so don't expire this one at any zoom.
            maxLodPix = -1;
        }
    }

    VSILFILE *fp = VSIFOpenL(filename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s",
                 filename.c_str());
        return FALSE;
    }

    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<kml xmlns=\"http://www.opengis.net/kml/2.2\" "
                    "xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n");
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

    if (pszAltitude != nullptr)
    {
        VSIFPrintfL(fp, "\t\t\t<altitude>%s</altitude>\n", pszAltitude);
    }
    if (pszAltitudeMode != nullptr &&
        (strcmp(pszAltitudeMode, "clampToGround") == 0 ||
         strcmp(pszAltitudeMode, "absolute") == 0))
    {
        VSIFPrintfL(fp, "\t\t\t<altitudeMode>%s</altitudeMode>\n",
                    pszAltitudeMode);
    }
    else if (pszAltitudeMode != nullptr &&
             (strcmp(pszAltitudeMode, "relativeToSeaFloor") == 0 ||
              strcmp(pszAltitudeMode, "clampToSeaFloor") == 0))
    {
        VSIFPrintfL(fp, "\t\t\t<gx:altitudeMode>%s</gx:altitudeMode>\n",
                    pszAltitudeMode);
    }

    /* When possible, use <LatLonBox>. I've noticed otherwise that */
    /* if using <gx:LatLonQuad> with extents of the size of a country or */
    /* continent, the overlay is really bad placed in GoogleEarth */
    if (lowerleftT == upperleftT && lowerrightT == upperrightT &&
        leftbottomT == rightbottomT && righttopT == lefttopT)
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

    for (const auto &kv : childTiles)
    {
        int cx = kv.first.first;
        int cy = kv.first.second;

        double cnorth = south + zoomypixel / 2 * ((cy + 1) * dysize);
        double csouth = south + zoomypixel / 2 * (cy * dysize);
        double ceast = west + zoomxpixel / 2 * ((cx + 1) * dxsize);
        double cwest = west + zoomxpixel / 2 * cx * dxsize;

        if (poTransform)
        {
            poTransform->Transform(1, &cwest, &csouth);
            poTransform->Transform(1, &ceast, &cnorth);
        }

        if (fixAntiMeridian && ceast < cwest)
        {
            ceast += 360;
        }

        VSIFPrintfL(fp, "\t\t<NetworkLink>\n");
        VSIFPrintfL(fp, "\t\t\t<name>%d/%d/%d%s</name>\n", zoom + 1, cx, cy,
                    fileExt.c_str());
        VSIFPrintfL(fp, "\t\t\t<Region>\n");
        VSIFPrintfL(fp, "\t\t\t\t<Lod>\n");
        VSIFPrintfL(fp, "\t\t\t\t\t<minLodPixels>128</minLodPixels>\n");
        VSIFPrintfL(fp, "\t\t\t\t\t<maxLodPixels>-1</maxLodPixels>\n");
        VSIFPrintfL(fp, "\t\t\t\t</Lod>\n");
        VSIFPrintfL(fp, "\t\t\t\t<LatLonAltBox>\n");
        VSIFPrintfL(fp, "\t\t\t\t\t<north>%f</north>\n", cnorth);
        VSIFPrintfL(fp, "\t\t\t\t\t<south>%f</south>\n", csouth);
        VSIFPrintfL(fp, "\t\t\t\t\t<east>%f</east>\n", ceast);
        VSIFPrintfL(fp, "\t\t\t\t\t<west>%f</west>\n", cwest);
        VSIFPrintfL(fp, "\t\t\t\t</LatLonAltBox>\n");
        VSIFPrintfL(fp, "\t\t\t</Region>\n");
        VSIFPrintfL(fp, "\t\t\t<Link>\n");
        VSIFPrintfL(fp, "\t\t\t\t<href>../../%d/%d/%d.kml</href>\n", zoom + 1,
                    cx, cy);
        VSIFPrintfL(fp,
                    "\t\t\t\t<viewRefreshMode>onRegion</viewRefreshMode>\n");
        VSIFPrintfL(fp, "\t\t\t\t<viewFormat/>\n");
        VSIFPrintfL(fp, "\t\t\t</Link>\n");
        VSIFPrintfL(fp, "\t\t</NetworkLink>\n");
    }

    VSIFPrintfL(fp, "\t</Document>\n");
    VSIFPrintfL(fp, "</kml>\n");
    VSIFCloseL(fp);

    return TRUE;
}

/************************************************************************/
/*                         DetectTransparency()                         */
/************************************************************************/
int KmlSuperOverlayReadDataset::DetectTransparency(int rxsize, int rysize,
                                                   int rx, int ry, int dxsize,
                                                   int dysize,
                                                   GDALDataset *poSrcDs)
{
    int bands = poSrcDs->GetRasterCount();
    int rowOffset = rysize / dysize;
    int loopCount = rysize / rowOffset;
    int hasNoData = 0;
    std::vector<GByte> abyScanline(dxsize);

    int flags = 0;
    for (int band = 1; band <= bands; band++)
    {
        GDALRasterBand *poBand = poSrcDs->GetRasterBand(band);
        int noDataValue = static_cast<int>(poBand->GetNoDataValue(&hasNoData));

        if (band < 4 && hasNoData)
        {
            for (int row = 0; row < loopCount; row++)
            {
                int yOffset = ry + row * rowOffset;
                CPL_IGNORE_RET_VAL(poBand->RasterIO(
                    GF_Read, rx, yOffset, rxsize, rowOffset, abyScanline.data(),
                    dxsize, 1, GDT_Byte, 0, 0, nullptr));
                for (int i = 0; i < dxsize; i++)
                {
                    if (abyScanline[i] == noDataValue)
                    {
                        flags |= KMLSO_ContainsTransparentPixels;
                    }
                    else
                    {
                        flags |= KMLSO_ContainsOpaquePixels;
                    }
                }
                // shortcut - if there are both types of pixels, flags is as
                // full as it is going to get.
                // so no point continuing, skip to the next band
                if ((flags & KMLSO_ContainsTransparentPixels) &&
                    (flags & KMLSO_ContainsOpaquePixels))
                {
                    break;
                }
            }
        }
        else if (band == 4)
        {
            for (int row = 0; row < loopCount; row++)
            {
                int yOffset = ry + row * rowOffset;
                CPL_IGNORE_RET_VAL(poBand->RasterIO(
                    GF_Read, rx, yOffset, rxsize, rowOffset, abyScanline.data(),
                    dxsize, 1, GDT_Byte, 0, 0, nullptr));
                for (int i = 0; i < dxsize; i++)
                {
                    if (abyScanline[i] == 255)
                    {
                        flags |= KMLSO_ContainsOpaquePixels;
                    }
                    else if (abyScanline[i] == 0)
                    {
                        flags |= KMLSO_ContainsTransparentPixels;
                    }
                    else
                    {
                        flags |= KMLSO_ContainsPartiallyTransparentPixels;
                    }
                }
            }
        }
    }
    return flags;
}

/************************************************************************/
/*                           CreateCopy()                               */
/************************************************************************/

class KmlSuperOverlayDummyDataset final : public GDALDataset
{
  public:
    KmlSuperOverlayDummyDataset() = default;
};

static GDALDataset *
KmlSuperOverlayCreateCopy(const char *pszFilename, GDALDataset *poSrcDS,
                          CPL_UNUSED int bStrict, char **papszOptions,
                          GDALProgressFunc pfnProgress, void *pProgressData)
{
    bool isKmz = false;

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    int bands = poSrcDS->GetRasterCount();
    if (bands != 1 && bands != 3 && bands != 4)
        return nullptr;

    // correct the file and get the directory
    char *output_dir = nullptr;
    if (pszFilename == nullptr)
    {
        output_dir = CPLGetCurrentDir();
        pszFilename = CPLFormFilename(output_dir, "doc", "kml");
    }
    else
    {
        const char *extension = CPLGetExtension(pszFilename);
        if (!EQUAL(extension, "kml") && !EQUAL(extension, "kmz"))
        {
            CPLError(CE_Failure, CPLE_None,
                     "File extension should be kml or kmz.");
            return nullptr;
        }
        if (EQUAL(extension, "kmz"))
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
    output_dir = nullptr;

    VSILFILE *zipHandle = nullptr;
    if (isKmz)
    {
        outDir = "/vsizip/";
        outDir += pszFilename;
        zipHandle = VSIFOpenL(outDir, "wb");
        if (zipHandle == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s",
                     pszFilename);
            return nullptr;
        }
    }

    GDALDriver *poOutputTileDriver = nullptr;
    GDALDriver *poJpegOutputTileDriver = nullptr;
    GDALDriver *poPngOutputTileDriver = nullptr;
    bool isAutoDriver = false;
    bool isJpegDriver = false;

    const char *pszFormat =
        CSLFetchNameValueDef(papszOptions, "FORMAT", "JPEG");
    if (EQUAL(pszFormat, "AUTO"))
    {
        isAutoDriver = true;
        poJpegOutputTileDriver =
            GetGDALDriverManager()->GetDriverByName("JPEG");
        poPngOutputTileDriver = GetGDALDriverManager()->GetDriverByName("PNG");
    }
    else
    {
        poOutputTileDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
        if (EQUAL(pszFormat, "JPEG"))
        {
            isJpegDriver = true;
        }
    }

    GDALDriver *poMemDriver = GetGDALDriverManager()->GetDriverByName("MEM");

    if (poMemDriver == nullptr ||
        (!isAutoDriver && poOutputTileDriver == nullptr) ||
        (isAutoDriver && (poJpegOutputTileDriver == nullptr ||
                          poPngOutputTileDriver == nullptr)))
    {
        CPLError(CE_Failure, CPLE_None, "Image export driver was not found..");
        if (zipHandle != nullptr)
        {
            VSIFCloseL(zipHandle);
            VSIUnlink(pszFilename);
        }
        return nullptr;
    }

    int xsize = poSrcDS->GetRasterXSize();
    int ysize = poSrcDS->GetRasterYSize();

    double north = 0.0;
    double south = 0.0;
    double east = 0.0;
    double west = 0.0;

    double adfGeoTransform[6];

    if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
    {
        north = adfGeoTransform[3];
        south = adfGeoTransform[3] + adfGeoTransform[5] * ysize;
        east = adfGeoTransform[0] + adfGeoTransform[1] * xsize;
        west = adfGeoTransform[0];
    }

    std::unique_ptr<OGRCoordinateTransformation> poTransform;
    const auto poSrcSRS = poSrcDS->GetSpatialRef();
    if (poSrcSRS && poSrcSRS->IsProjected())
    {
        OGRSpatialReference poLatLong;
        poLatLong.SetWellKnownGeogCS("WGS84");
        poLatLong.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        poTransform.reset(
            OGRCreateCoordinateTransformation(poSrcSRS, &poLatLong));
        if (poTransform != nullptr)
        {
            poTransform->Transform(1, &west, &south);
            poTransform->Transform(1, &east, &north);
        }
    }

    const bool fixAntiMeridian =
        CPLFetchBool(papszOptions, "FIX_ANTIMERIDIAN", false);
    if (fixAntiMeridian && east < west)
    {
        east += 360;
    }

    // Zoom levels of the pyramid.
    int maxzoom = 0;
    int tilexsize;
    int tileysize;
    // Let the longer side determine the max zoom level and x/y tilesizes.
    if (xsize >= ysize)
    {
        double dtilexsize = xsize;
        while (dtilexsize > 400)  // calculate x tile size
        {
            dtilexsize = dtilexsize / 2;
            maxzoom++;
        }
        tilexsize = static_cast<int>(dtilexsize);
        tileysize = static_cast<int>(dtilexsize * ysize / xsize);
    }
    else
    {
        double dtileysize = ysize;
        while (dtileysize > 400)  // calculate y tile size
        {
            dtileysize = dtileysize / 2;
            maxzoom++;
        }

        tileysize = static_cast<int>(dtileysize);
        tilexsize = static_cast<int>(dtileysize * xsize / ysize);
    }

    std::vector<double> zoomxpixels;
    std::vector<double> zoomypixels;
    for (int zoom = 0; zoom < maxzoom + 1; zoom++)
    {
        zoomxpixels.push_back(adfGeoTransform[1] * pow(2.0, (maxzoom - zoom)));
        // zoomypixels.push_back(abs(adfGeoTransform[5]) * pow(2.0, (maxzoom -
        // zoom)));
        zoomypixels.push_back(fabs(adfGeoTransform[5]) *
                              pow(2.0, (maxzoom - zoom)));
    }

    std::string tmpFileName;
    std::vector<std::string> fileVector;
    int nRet;

    const char *pszOverlayName = CSLFetchNameValue(papszOptions, "NAME");
    const char *pszOverlayDescription =
        CSLFetchNameValue(papszOptions, "DESCRIPTION");

    if (isKmz)
    {
        tmpFileName = CPLFormFilename(outDir, "doc.kml", nullptr);
        nRet = GenerateRootKml(tmpFileName.c_str(), pszFilename, north, south,
                               east, west, static_cast<int>(tilexsize),
                               pszOverlayName, pszOverlayDescription);
        fileVector.push_back(tmpFileName);
    }
    else
    {
        nRet = GenerateRootKml(pszFilename, pszFilename, north, south, east,
                               west, static_cast<int>(tilexsize),
                               pszOverlayName, pszOverlayDescription);
    }

    if (nRet == FALSE)
    {
        if (zipHandle != nullptr)
        {
            VSIFCloseL(zipHandle);
            VSIUnlink(pszFilename);
        }
        return nullptr;
    }

    const char *pszAltitude = CSLFetchNameValue(papszOptions, "ALTITUDE");
    const char *pszAltitudeMode =
        CSLFetchNameValue(papszOptions, "ALTITUDEMODE");
    if (pszAltitudeMode != nullptr)
    {
        if (strcmp(pszAltitudeMode, "clampToGround") == 0)
        {
            pszAltitudeMode = nullptr;
            pszAltitude = nullptr;
        }
        else if (strcmp(pszAltitudeMode, "absolute") == 0)
        {
            if (pszAltitude == nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Using ALTITUDE=0 as default value");
                pszAltitude = "0";
            }
        }
        else if (strcmp(pszAltitudeMode, "relativeToSeaFloor") == 0)
        {
            /* nothing to do */
        }
        else if (strcmp(pszAltitudeMode, "clampToSeaFloor") == 0)
        {
            pszAltitude = nullptr;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ignoring unhandled value of ALTITUDEMODE");
            pszAltitudeMode = nullptr;
            pszAltitude = nullptr;
        }
    }

    int zoom;
    int nTotalTiles = 0;
    int nTileCount = 0;

    for (zoom = maxzoom; zoom >= 0; --zoom)
    {
        const int rmaxxsize = tilexsize * (1 << (maxzoom - zoom));
        const int rmaxysize = tileysize * (1 << (maxzoom - zoom));

        const int xloop = std::max(1, static_cast<int>(xsize / rmaxxsize));
        const int yloop = std::max(1, static_cast<int>(ysize / rmaxysize));
        nTotalTiles += xloop * yloop;
    }

    // {(x, y): [((childx, childy), hasChildKML), ...], ...}
    std::map<std::pair<int, int>,
             std::vector<std::pair<std::pair<int, int>, bool>>>
        childTiles;
    std::map<std::pair<int, int>,
             std::vector<std::pair<std::pair<int, int>, bool>>>
        currentTiles;
    std::pair<int, int> childXYKey;
    std::pair<int, int> parentXYKey;
    for (zoom = maxzoom; zoom >= 0; --zoom)
    {
        const int rmaxxsize = tilexsize * (1 << (maxzoom - zoom));
        const int rmaxysize = tileysize * (1 << (maxzoom - zoom));

        const int xloop = std::max(1, static_cast<int>(xsize / rmaxxsize));
        const int yloop = std::max(1, static_cast<int>(ysize / rmaxysize));

        std::stringstream zoomStr;
        zoomStr << zoom;

        std::string zoomDir = outDir;
        zoomDir += "/" + zoomStr.str();
        VSIMkdir(zoomDir.c_str(), 0775);

        for (int ix = 0; ix < xloop; ix++)
        {
            int rxsize = static_cast<int>(rmaxxsize);
            int rx = static_cast<int>(ix * rmaxxsize);
            int dxsize = static_cast<int>(rxsize / rmaxxsize * tilexsize);

            std::stringstream ixStr;
            ixStr << ix;

            zoomDir = outDir;
            zoomDir += "/" + zoomStr.str();
            zoomDir += "/" + ixStr.str();
            VSIMkdir(zoomDir.c_str(), 0775);

            for (int iy = 0; iy < yloop; iy++)
            {
                int rysize = static_cast<int>(rmaxysize);
                int ry = static_cast<int>(ysize - (iy * rmaxysize)) - rysize;
                int dysize = static_cast<int>(rysize / rmaxysize * tileysize);

                std::stringstream iyStr;
                iyStr << iy;

                if (isAutoDriver)
                {
                    int flags = KmlSuperOverlayReadDataset::DetectTransparency(
                        rxsize, rysize, rx, ry, dxsize, dysize, poSrcDS);
                    if (flags & (KmlSuperOverlayReadDataset::
                                     KMLSO_ContainsPartiallyTransparentPixels |
                                 KmlSuperOverlayReadDataset::
                                     KMLSO_ContainsTransparentPixels))
                    {
                        if (!(flags &
                              (KmlSuperOverlayReadDataset::
                                   KMLSO_ContainsPartiallyTransparentPixels |
                               KmlSuperOverlayReadDataset::
                                   KMLSO_ContainsOpaquePixels)))
                        {
                            // don't bother creating empty tiles
                            continue;
                        }
                        poOutputTileDriver = poPngOutputTileDriver;
                        isJpegDriver = false;
                    }
                    else
                    {
                        poOutputTileDriver = poJpegOutputTileDriver;
                        isJpegDriver = true;
                    }
                }

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

                GenerateTiles(filename, zoom, rxsize, rysize, ix, iy, rx, ry,
                              dxsize, dysize, bands, poSrcDS,
                              poOutputTileDriver, poMemDriver, isJpegDriver);
                std::string childKmlfile = zoomDir + "/" + iyStr.str() + ".kml";
                if (isKmz)
                {
                    fileVector.push_back(childKmlfile);
                }

                double tmpSouth =
                    adfGeoTransform[3] + adfGeoTransform[5] * ysize;
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

                childXYKey = std::make_pair(ix, iy);
                parentXYKey = std::make_pair(ix / 2, iy / 2);

                // only create child KML if there are child tiles
                bool hasChildKML = !childTiles[childXYKey].empty();
                if (!currentTiles.count(parentXYKey))
                {
                    currentTiles[parentXYKey] =
                        std::vector<std::pair<std::pair<int, int>, bool>>();
                }
                currentTiles[parentXYKey].push_back(
                    std::make_pair(std::make_pair(ix, iy), hasChildKML));
                GenerateChildKml(childKmlfile, zoom, ix, iy, zoomxpix, zoomypix,
                                 dxsize, dysize, tmpSouth, adfGeoTransform[0],
                                 xsize, ysize, maxzoom, poTransform.get(),
                                 fileExt, fixAntiMeridian, pszAltitude,
                                 pszAltitudeMode, childTiles[childXYKey]);

                nTileCount++;
                pfnProgress(1.0 * nTileCount / nTotalTiles, "", pProgressData);
            }
        }
        childTiles = currentTiles;
        currentTiles.clear();
    }

    if (zipHandle != nullptr)
    {
        VSIFCloseL(zipHandle);
    }

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    auto poDS = std::unique_ptr<GDALDataset>(
        KmlSuperOverlayReadDataset::Open(&oOpenInfo));
    if (!poDS)
        poDS = std::make_unique<KmlSuperOverlayDummyDataset>();
    return poDS.release();
}

/************************************************************************/
/*                            KMLRemoveSlash()                          */
/************************************************************************/

/* replace "a/b/../c" pattern by "a/c" */
static CPLString KMLRemoveSlash(const char *pszPathIn)
{
    char *pszPath = CPLStrdup(pszPathIn);

    while (true)
    {
        char *pszSlashDotDot = strstr(pszPath, "/../");
        if (pszSlashDotDot == nullptr || pszSlashDotDot == pszPath)
            break;
        char *pszSlashBefore = pszSlashDotDot - 1;
        while (pszSlashBefore > pszPath && *pszSlashBefore != '/')
            pszSlashBefore--;
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
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                     ~KmlSuperOverlayReadDataset()                    */
/************************************************************************/

KmlSuperOverlayReadDataset::~KmlSuperOverlayReadDataset()

{
    if (psRoot != nullptr)
        CPLDestroyXMLNode(psRoot);
    KmlSuperOverlayReadDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                         CloseDependentDatasets()                     */
/************************************************************************/

int KmlSuperOverlayReadDataset::CloseDependentDatasets()
{
    int bRet = FALSE;
    if (poDSIcon)
    {
        CPLString l_osFilename(poDSIcon->GetDescription());
        poDSIcon.reset();
        VSIUnlink(l_osFilename);
        bRet = TRUE;
    }

    LinkedDataset *psCur = psFirstLink;
    psFirstLink = nullptr;
    psLastLink = nullptr;

    while (psCur != nullptr)
    {
        LinkedDataset *psNext = psCur->psNext;
        if (psCur->poDS != nullptr)
        {
            if (psCur->poDS->nRefCount == 1)
                bRet = TRUE;
            GDALClose(psCur->poDS);
        }
        delete psCur;
        psCur = psNext;
    }

    if (!m_apoOverviewDS.empty())
    {
        bRet = TRUE;
        m_apoOverviewDS.clear();
    }

    return bRet;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *KmlSuperOverlayReadDataset::GetSpatialRef() const

{
    return &m_oSRS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr KmlSuperOverlayReadDataset::GetGeoTransform(double *padfGeoTransform)
{
    memcpy(padfGeoTransform, adfGeoTransform.data(), 6 * sizeof(double));
    return CE_None;
}

/************************************************************************/
/*                        KmlSuperOverlayRasterBand()                   */
/************************************************************************/

KmlSuperOverlayRasterBand::KmlSuperOverlayRasterBand(
    KmlSuperOverlayReadDataset *poDSIn, int /* nBand*/)
{
    nRasterXSize = poDSIn->nRasterXSize;
    nRasterYSize = poDSIn->nRasterYSize;
    eDataType = GDT_Byte;
    nBlockXSize = 256;
    nBlockYSize = 256;
}

/************************************************************************/
/*                               IReadBlock()                           */
/************************************************************************/

CPLErr KmlSuperOverlayRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                             void *pData)
{
    int nXOff = nBlockXOff * nBlockXSize;
    int nYOff = nBlockYOff * nBlockYSize;
    int nXSize = nBlockXSize;
    int nYSize = nBlockYSize;
    if (nXOff + nXSize > nRasterXSize)
        nXSize = nRasterXSize - nXOff;
    if (nYOff + nYSize > nRasterYSize)
        nYSize = nRasterYSize - nYOff;

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize, pData, nXSize,
                     nYSize, eDataType, 1, nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                          GetColorInterpretation()                    */
/************************************************************************/

GDALColorInterp KmlSuperOverlayRasterBand::GetColorInterpretation()
{
    return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr KmlSuperOverlayRasterBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    KmlSuperOverlayReadDataset *poGDS =
        cpl::down_cast<KmlSuperOverlayReadDataset *>(poDS);

    return poGDS->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                            nBufXSize, nBufYSize, eBufType, 1, &nBand,
                            nPixelSpace, nLineSpace, 0, psExtraArg);
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int KmlSuperOverlayRasterBand::GetOverviewCount()
{
    KmlSuperOverlayReadDataset *poGDS =
        cpl::down_cast<KmlSuperOverlayReadDataset *>(poDS);

    return static_cast<int>(poGDS->m_apoOverviewDS.size());
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand *KmlSuperOverlayRasterBand::GetOverview(int iOvr)
{
    KmlSuperOverlayReadDataset *poGDS =
        cpl::down_cast<KmlSuperOverlayReadDataset *>(poDS);

    if (iOvr < 0 || iOvr >= static_cast<int>(poGDS->m_apoOverviewDS.size()))
        return nullptr;

    return poGDS->m_apoOverviewDS[iOvr]->GetRasterBand(nBand);
}

/************************************************************************/
/*                     KmlSuperOverlayGetBoundingBox()                  */
/************************************************************************/

static bool KmlSuperOverlayGetBoundingBox(const CPLXMLNode *psNode,
                                          std::array<double, 4> &adfExtents)
{
    const CPLXMLNode *psBox = CPLGetXMLNode(psNode, "LatLonBox");
    if (!psBox)
        psBox = CPLGetXMLNode(psNode, "LatLonAltBox");
    if (psBox)
    {
        const char *pszNorth = CPLGetXMLValue(psBox, "north", nullptr);
        const char *pszSouth = CPLGetXMLValue(psBox, "south", nullptr);
        const char *pszEast = CPLGetXMLValue(psBox, "east", nullptr);
        const char *pszWest = CPLGetXMLValue(psBox, "west", nullptr);
        if (pszNorth && pszSouth && pszEast && pszWest)
        {
            adfExtents[0] = CPLAtof(pszWest);
            adfExtents[1] = CPLAtof(pszSouth);
            adfExtents[2] = CPLAtof(pszEast);
            adfExtents[3] = CPLAtof(pszNorth);

            return true;
        }
    }
    else
    {
        const CPLXMLNode *psLatLonQuad = CPLGetXMLNode(psNode, "gx:LatLonQuad");
        if (psLatLonQuad)
        {
            const CPLStringList aosTuples(CSLTokenizeString2(
                CPLGetXMLValue(psLatLonQuad, "coordinates", ""), " \t\n\r", 0));
            if (aosTuples.size() == 4)
            {
                const CPLStringList aosLL(
                    CSLTokenizeString2(aosTuples[0], ",", 0));
                const CPLStringList aosLR(
                    CSLTokenizeString2(aosTuples[1], ",", 0));
                const CPLStringList aosUR(
                    CSLTokenizeString2(aosTuples[2], ",", 0));
                const CPLStringList aosUL(
                    CSLTokenizeString2(aosTuples[3], ",", 0));
                if (aosLL.size() >= 2 && aosLR.size() >= 2 &&
                    aosUR.size() >= 2 && aosUL.size() >= 2 &&
                    strcmp(aosLL[0], aosUL[0]) == 0 &&
                    strcmp(aosLL[1], aosLR[1]) == 0 &&
                    strcmp(aosLR[0], aosUR[0]) == 0 &&
                    strcmp(aosUR[1], aosUL[1]) == 0)
                {
                    adfExtents[0] = CPLAtof(aosLL[0]);
                    adfExtents[1] = CPLAtof(aosLL[1]);
                    adfExtents[2] = CPLAtof(aosUR[0]);
                    adfExtents[3] = CPLAtof(aosUR[1]);
                    return true;
                }
            }
        }
    }

    return false;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

class SubImageDesc
{
  public:
    GDALDataset *poDS = nullptr;
    std::array<double, 4> adfExtents = {0, 0, 0, 0};
};

CPLErr KmlSuperOverlayReadDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
    GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag == GF_Write)
        return CE_Failure;

    if (bIsOvr)
    {
        GDALRasterIOExtraArg sExtraArgs;
        GDALCopyRasterIOExtraArg(&sExtraArgs, psExtraArg);
        const int nOvrFactor = poParent->nFactor / nFactor;
        if (sExtraArgs.bFloatingPointWindowValidity)
        {
            sExtraArgs.dfXOff *= nOvrFactor;
            sExtraArgs.dfYOff *= nOvrFactor;
            sExtraArgs.dfXSize *= nOvrFactor;
            sExtraArgs.dfYSize *= nOvrFactor;
        }
        return poParent->IRasterIO(
            eRWFlag, nXOff * nOvrFactor, nYOff * nOvrFactor,
            nXSize * nOvrFactor, nYSize * nOvrFactor, pData, nBufXSize,
            nBufYSize, eBufType, nBandCount, panBandMap, nPixelSpace,
            nLineSpace, nBandSpace, &sExtraArgs);
    }

    double dfXOff = 1.0 * nXOff / nFactor;
    double dfYOff = 1.0 * nYOff / nFactor;
    double dfXSize = 1.0 * nXSize / nFactor;
    double dfYSize = 1.0 * nYSize / nFactor;

    int nIconCount = poDSIcon->GetRasterCount();

    if (nBufXSize > dfXSize || nBufYSize > dfYSize)
    {
        const double dfRequestXMin =
            adfGeoTransform[0] + nXOff * adfGeoTransform[1];
        const double dfRequestXMax =
            adfGeoTransform[0] + (nXOff + nXSize) * adfGeoTransform[1];
        const double dfRequestYMin =
            adfGeoTransform[3] + (nYOff + nYSize) * adfGeoTransform[5];
        const double dfRequestYMax =
            adfGeoTransform[3] + nYOff * adfGeoTransform[5];

        const CPLXMLNode *psIter = psDocument->psChild;
        std::vector<SubImageDesc> aoImages;
        const double dfXRes = adfGeoTransform[1] * nFactor;
        const double dfYRes = -adfGeoTransform[5] * nFactor;
        double dfNewXRes = dfXRes;
        double dfNewYRes = dfYRes;

        while (psIter != nullptr)
        {
            const CPLXMLNode *psRegion = nullptr;
            const CPLXMLNode *psLink = nullptr;
            std::array<double, 4> adfExtents = {0, 0, 0, 0};
            const char *pszHref = nullptr;
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "NetworkLink") == 0 &&
                (psRegion = CPLGetXMLNode(psIter, "Region")) != nullptr &&
                (psLink = CPLGetXMLNode(psIter, "Link")) != nullptr &&
                KmlSuperOverlayGetBoundingBox(psRegion, adfExtents) &&
                (pszHref = CPLGetXMLValue(psLink, "href", nullptr)) != nullptr)
            {
                if (dfRequestXMin < adfExtents[2] &&
                    dfRequestXMax > adfExtents[0] &&
                    dfRequestYMin < adfExtents[3] &&
                    dfRequestYMax > adfExtents[1])
                {
                    CPLString osSubFilename;
                    if (STARTS_WITH(pszHref, "http"))
                        osSubFilename =
                            CPLSPrintf("/vsicurl_streaming/%s", pszHref);
                    else
                    {
                        const char *pszBaseFilename = osFilename.c_str();
                        if (EQUAL(CPLGetExtension(pszBaseFilename), "kmz") &&
                            !STARTS_WITH(pszBaseFilename, "/vsizip/"))
                        {
                            osSubFilename = "/vsizip/";
                            osSubFilename += CPLGetPath(pszBaseFilename);
                            osSubFilename += "/";
                            osSubFilename += pszHref;
                        }
                        else
                        {
                            osSubFilename = CPLFormFilename(
                                CPLGetPath(pszBaseFilename), pszHref, nullptr);
                        }
                        osSubFilename = KMLRemoveSlash(osSubFilename);
                    }

                    KmlSuperOverlayReadDataset *poSubImageDS = nullptr;
                    if (EQUAL(CPLGetExtension(osSubFilename), "kml"))
                    {
                        KmlSuperOverlayReadDataset *poRoot =
                            poParent ? poParent : this;
                        LinkedDataset *psLinkDS =
                            poRoot->oMapChildren[osSubFilename];
                        if (psLinkDS == nullptr)
                        {
                            if (poRoot->oMapChildren.size() == 64)
                            {
                                psLinkDS = poRoot->psLastLink;
                                CPLAssert(psLinkDS);
                                poRoot->oMapChildren.erase(
                                    psLinkDS->osSubFilename);
                                GDALClose(psLinkDS->poDS);
                                if (psLinkDS->psPrev != nullptr)
                                {
                                    poRoot->psLastLink = psLinkDS->psPrev;
                                    psLinkDS->psPrev->psNext = nullptr;
                                }
                                else
                                {
                                    CPLAssert(psLinkDS == poRoot->psFirstLink);
                                    poRoot->psFirstLink = nullptr;
                                    poRoot->psLastLink = nullptr;
                                }
                            }
                            else
                                psLinkDS = new LinkedDataset();

                            poRoot->oMapChildren[osSubFilename] = psLinkDS;
                            poSubImageDS =
                                cpl::down_cast<KmlSuperOverlayReadDataset *>(
                                    KmlSuperOverlayReadDataset::Open(
                                        osSubFilename, poRoot));
                            if (poSubImageDS)
                                poSubImageDS->MarkAsShared();
                            else
                                CPLDebug("KMLSuperOverlay", "Cannot open %s",
                                         osSubFilename.c_str());
                            psLinkDS->osSubFilename = osSubFilename;
                            psLinkDS->poDS = poSubImageDS;
                            psLinkDS->psPrev = nullptr;
                            psLinkDS->psNext = poRoot->psFirstLink;
                            if (poRoot->psFirstLink != nullptr)
                            {
                                CPLAssert(poRoot->psFirstLink->psPrev ==
                                          nullptr);
                                poRoot->psFirstLink->psPrev = psLinkDS;
                            }
                            else
                                poRoot->psLastLink = psLinkDS;
                            poRoot->psFirstLink = psLinkDS;
                        }
                        else
                        {
                            poSubImageDS = psLinkDS->poDS;
                            if (psLinkDS != poRoot->psFirstLink)
                            {
                                if (psLinkDS == poRoot->psLastLink)
                                {
                                    poRoot->psLastLink = psLinkDS->psPrev;
                                    CPLAssert(poRoot->psLastLink != nullptr);
                                    poRoot->psLastLink->psNext = nullptr;
                                }
                                else
                                    psLinkDS->psNext->psPrev = psLinkDS->psPrev;
                                CPLAssert(psLinkDS->psPrev != nullptr);
                                psLinkDS->psPrev->psNext = psLinkDS->psNext;
                                psLinkDS->psPrev = nullptr;
                                poRoot->psFirstLink->psPrev = psLinkDS;
                                psLinkDS->psNext = poRoot->psFirstLink;
                                poRoot->psFirstLink = psLinkDS;
                            }
                        }
                    }
                    if (poSubImageDS)
                    {
                        int nSubImageXSize = poSubImageDS->GetRasterXSize();
                        int nSubImageYSize = poSubImageDS->GetRasterYSize();
                        adfExtents[0] = poSubImageDS->adfGeoTransform[0];
                        adfExtents[1] =
                            poSubImageDS->adfGeoTransform[3] +
                            nSubImageYSize * poSubImageDS->adfGeoTransform[5];
                        adfExtents[2] =
                            poSubImageDS->adfGeoTransform[0] +
                            nSubImageXSize * poSubImageDS->adfGeoTransform[1];
                        adfExtents[3] = poSubImageDS->adfGeoTransform[3];

                        double dfSubXRes =
                            (adfExtents[2] - adfExtents[0]) / nSubImageXSize;
                        double dfSubYRes =
                            (adfExtents[3] - adfExtents[1]) / nSubImageYSize;

                        if (dfSubXRes < dfNewXRes)
                            dfNewXRes = dfSubXRes;
                        if (dfSubYRes < dfNewYRes)
                            dfNewYRes = dfSubYRes;

                        SubImageDesc oImageDesc;
                        oImageDesc.poDS = poSubImageDS;
                        poSubImageDS->Reference();
                        oImageDesc.adfExtents = adfExtents;
                        aoImages.push_back(oImageDesc);
                    }
                    CPL_IGNORE_RET_VAL(osSubFilename);
                }
            }
            psIter = psIter->psNext;
        }

        if (dfNewXRes < dfXRes || dfNewYRes < dfYRes)
        {
            const double dfXFactor = dfXRes / dfNewXRes;
            const double dfYFactor = dfYRes / dfNewYRes;
            auto poVRTDS = std::make_unique<VRTDataset>(
                static_cast<int>(nRasterXSize * dfXFactor + 0.5),
                static_cast<int>(nRasterYSize * dfYFactor + 0.5));

            for (int iBandIdx = 0; iBandIdx < 4; iBandIdx++)
            {
                poVRTDS->AddBand(GDT_Byte, nullptr);

                auto poVRTBand = static_cast<VRTSourcedRasterBand *>(
                    poVRTDS->GetRasterBand(iBandIdx + 1));
                const int nBand = iBandIdx + 1;
                if (nBand <= nIconCount || (nIconCount == 1 && nBand != 4))
                {
                    poVRTBand->AddSimpleSource(
                        poDSIcon->GetRasterBand(nBand <= nIconCount ? nBand
                                                                    : 1),
                        0, 0, nRasterXSize, nRasterYSize, 0, 0,
                        poVRTDS->GetRasterXSize(), poVRTDS->GetRasterYSize(),
                        nullptr, VRT_NODATA_UNSET);
                }
                else
                {
                    poVRTBand->AddComplexSource(
                        poDSIcon->GetRasterBand(1), 0, 0, nRasterXSize,
                        nRasterYSize, 0, 0, poVRTDS->GetRasterXSize(),
                        poVRTDS->GetRasterYSize(), VRT_NODATA_UNSET, 0, 255);
                }
            }

            for (const auto &oImage : aoImages)
            {
                const int nDstXOff = static_cast<int>(
                    (oImage.adfExtents[0] - adfGeoTransform[0]) / dfNewXRes +
                    0.5);
                const int nDstYOff = static_cast<int>(
                    (adfGeoTransform[3] - oImage.adfExtents[3]) / dfNewYRes +
                    0.5);
                const int nDstXSize = static_cast<int>(
                    (oImage.adfExtents[2] - oImage.adfExtents[0]) / dfNewXRes +
                    0.5);
                const int nDstYSize = static_cast<int>(
                    (oImage.adfExtents[3] - oImage.adfExtents[1]) / dfNewYRes +
                    0.5);

                const int nSrcBandCount = oImage.poDS->GetRasterCount();
                for (int iBandIdx = 0; iBandIdx < 4; iBandIdx++)
                {
                    const int nBand = iBandIdx + 1;
                    auto poVRTBand = static_cast<VRTSourcedRasterBand *>(
                        poVRTDS->GetRasterBand(iBandIdx + 1));

                    if (nBand <= nSrcBandCount ||
                        (nSrcBandCount == 1 && nBand != 4))
                    {
                        poVRTBand->AddSimpleSource(
                            oImage.poDS->GetRasterBand(
                                nBand <= nSrcBandCount ? nBand : 1),
                            0, 0, oImage.poDS->GetRasterXSize(),
                            oImage.poDS->GetRasterYSize(), nDstXOff, nDstYOff,
                            nDstXSize, nDstYSize, nullptr, VRT_NODATA_UNSET);
                    }
                    else
                    {
                        poVRTBand->AddComplexSource(
                            oImage.poDS->GetRasterBand(1), 0, 0,
                            oImage.poDS->GetRasterXSize(),
                            oImage.poDS->GetRasterYSize(), nDstXOff, nDstYOff,
                            nDstXSize, nDstYSize, VRT_NODATA_UNSET, 0, 255);
                    }
                }
            }

            int nReqXOff = static_cast<int>(dfXOff * dfXFactor + 0.5);
            int nReqYOff = static_cast<int>(dfYOff * dfYFactor + 0.5);
            int nReqXSize = static_cast<int>(dfXSize * dfXFactor + 0.5);
            int nReqYSize = static_cast<int>(dfYSize * dfYFactor + 0.5);
            if (nReqXOff + nReqXSize > poVRTDS->GetRasterXSize())
                nReqXSize = poVRTDS->GetRasterXSize() - nReqXOff;
            if (nReqYOff + nReqYSize > poVRTDS->GetRasterYSize())
                nReqYSize = poVRTDS->GetRasterYSize() - nReqYOff;

            GDALRasterIOExtraArg sExtraArgs;
            INIT_RASTERIO_EXTRA_ARG(sExtraArgs);
            // cppcheck-suppress redundantAssignment
            sExtraArgs.eResampleAlg = psExtraArg->eResampleAlg;
            CPLErr eErr = poVRTDS->RasterIO(
                eRWFlag, nReqXOff, nReqYOff, nReqXSize, nReqYSize, pData,
                nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                nPixelSpace, nLineSpace, nBandSpace, &sExtraArgs);

            for (auto &oImage : aoImages)
            {
                oImage.poDS->Dereference();
            }

            return eErr;
        }
    }

    GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
    void *pProgressDataGlobal = psExtraArg->pProgressData;
    CPLErr eErr = CE_None;

    for (int iBandIdx = 0; iBandIdx < nBandCount && eErr == CE_None; iBandIdx++)
    {
        int nBand = panBandMap[iBandIdx];

        if ((nIconCount > 1 || nBand == 4) && nBand > nIconCount)
        {
            GByte nVal = (nBand == 4) ? 255 : 0;
            for (int j = 0; j < nBufYSize; j++)
            {
                GDALCopyWords(&nVal, GDT_Byte, 0,
                              static_cast<GByte *>(pData) + j * nLineSpace +
                                  iBandIdx * nBandSpace,
                              eBufType, static_cast<int>(nPixelSpace),
                              nBufXSize);
            }
            continue;
        }

        int nIconBand = (nIconCount == 1) ? 1 : nBand;

        int nReqXOff = static_cast<int>(dfXOff + 0.5);
        int nReqYOff = static_cast<int>(dfYOff + 0.5);
        int nReqXSize = static_cast<int>(dfXSize + 0.5);
        int nReqYSize = static_cast<int>(dfYSize + 0.5);
        if (nReqXOff + nReqXSize > poDSIcon->GetRasterXSize())
            nReqXSize = poDSIcon->GetRasterXSize() - nReqXOff;
        if (nReqYOff + nReqYSize > poDSIcon->GetRasterYSize())
            nReqYSize = poDSIcon->GetRasterYSize() - nReqYOff;

        GDALRasterIOExtraArg sExtraArgs;
        INIT_RASTERIO_EXTRA_ARG(sExtraArgs);
        // cppcheck-suppress redundantAssignment
        sExtraArgs.eResampleAlg = psExtraArg->eResampleAlg;
        sExtraArgs.pfnProgress = GDALScaledProgress;
        sExtraArgs.pProgressData = GDALCreateScaledProgress(
            1.0 * iBandIdx / nBandCount, 1.0 * (iBandIdx + 1) / nBandCount,
            pfnProgressGlobal, pProgressDataGlobal);

        eErr = poDSIcon->GetRasterBand(nIconBand)->RasterIO(
            eRWFlag, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
            static_cast<GByte *>(pData) + nBandSpace * iBandIdx, nBufXSize,
            nBufYSize, eBufType, nPixelSpace, nLineSpace, &sExtraArgs);

        GDALDestroyScaledProgress(sExtraArgs.pProgressData);
    }

    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    return eErr;
}

/************************************************************************/
/*                    KmlSuperOverlayFindRegionStart()                  */
/************************************************************************/

static int KmlSuperOverlayFindRegionStartInternal(CPLXMLNode *psNode,
                                                  CPLXMLNode **ppsRegion,
                                                  CPLXMLNode **ppsDocument,
                                                  CPLXMLNode **ppsGroundOverlay,
                                                  CPLXMLNode **ppsLink)
{
    CPLXMLNode *psRegion = nullptr;
    CPLXMLNode *psLink = nullptr;
    CPLXMLNode *psGroundOverlay = nullptr;
    if (strcmp(psNode->pszValue, "NetworkLink") == 0 &&
        (psRegion = CPLGetXMLNode(psNode, "Region")) != nullptr &&
        (psLink = CPLGetXMLNode(psNode, "Link")) != nullptr)
    {
        *ppsRegion = psRegion;
        *ppsLink = psLink;
        return TRUE;
    }
    if ((strcmp(psNode->pszValue, "Document") == 0 ||
         strcmp(psNode->pszValue, "Folder") == 0) &&
        (psRegion = CPLGetXMLNode(psNode, "Region")) != nullptr &&
        (psGroundOverlay = CPLGetXMLNode(psNode, "GroundOverlay")) != nullptr)
    {
        *ppsDocument = psNode;
        *ppsRegion = psRegion;
        *ppsGroundOverlay = psGroundOverlay;
        return TRUE;
    }

    CPLXMLNode *psIter = psNode->psChild;
    while (psIter != nullptr)
    {
        if (psIter->eType == CXT_Element)
        {
            if (KmlSuperOverlayFindRegionStartInternal(
                    psIter, ppsRegion, ppsDocument, ppsGroundOverlay, ppsLink))
                return TRUE;
        }

        psIter = psIter->psNext;
    }

    return FALSE;
}

static int KmlSuperOverlayFindRegionStart(CPLXMLNode *psNode,
                                          CPLXMLNode **ppsRegion,
                                          CPLXMLNode **ppsDocument,
                                          CPLXMLNode **ppsGroundOverlay,
                                          CPLXMLNode **ppsLink)
{
    CPLXMLNode *psIter = psNode;
    while (psIter != nullptr)
    {
        if (psIter->eType == CXT_Element)
        {
            if (KmlSuperOverlayFindRegionStartInternal(
                    psIter, ppsRegion, ppsDocument, ppsGroundOverlay, ppsLink))
                return TRUE;
        }

        psIter = psIter->psNext;
    }

    return FALSE;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int KmlSuperOverlayReadDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    const char *pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    if (EQUAL(pszExt, "kmz"))
        return -1;
    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;
    if (
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        !EQUAL(pszExt, "kml") ||
#endif
        strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
               "<kml") == nullptr)
        return FALSE;

    for (int i = 0; i < 2; i++)
    {
        // Leave below variable here as the TryToIngest() at end might
        // invalidate it
        const char *pszText =
            reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
        if (strstr(pszText, "<NetworkLink>") != nullptr &&
            strstr(pszText, "<Region>") != nullptr &&
            strstr(pszText, "<Link>") != nullptr)
            return TRUE;

        if (strstr(pszText, "<Document>") != nullptr &&
            strstr(pszText, "<Region>") != nullptr &&
            strstr(pszText, "<GroundOverlay>") != nullptr)
            return TRUE;

        if (strstr(pszText, "<GroundOverlay>") != nullptr &&
            strstr(pszText, "<Icon>") != nullptr &&
            strstr(pszText, "<href>") != nullptr &&
            (strstr(pszText, "<LatLonBox>") != nullptr ||
             strstr(pszText, "<gx:LatLonQuad>") != nullptr))
            return TRUE;

        if (i == 0 && !poOpenInfo->TryToIngest(1024 * 10))
            return FALSE;
    }

    return -1;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *KmlSuperOverlayReadDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (Identify(poOpenInfo) == FALSE)
        return nullptr;

    return Open(poOpenInfo->pszFilename);
}

/************************************************************************/
/*                         KmlSuperOverlayLoadIcon()                    */
/************************************************************************/

#define BUFFER_SIZE 20000000

static std::unique_ptr<GDALDataset>
KmlSuperOverlayLoadIcon(const char *pszBaseFilename, const char *pszIcon)
{
    const char *pszExt = CPLGetExtension(pszIcon);
    if (!EQUAL(pszExt, "png") && !EQUAL(pszExt, "jpg") &&
        !EQUAL(pszExt, "jpeg"))
    {
        return nullptr;
    }

    CPLString osSubFilename;
    if (STARTS_WITH(pszIcon, "http"))
        osSubFilename = CPLSPrintf("/vsicurl_streaming/%s", pszIcon);
    else
    {
        osSubFilename =
            CPLFormFilename(CPLGetPath(pszBaseFilename), pszIcon, nullptr);
        osSubFilename = KMLRemoveSlash(osSubFilename);
    }

    VSILFILE *fp = VSIFOpenL(osSubFilename, "rb");
    if (fp == nullptr)
    {
        return nullptr;
    }
    GByte *pabyBuffer = static_cast<GByte *>(VSIMalloc(BUFFER_SIZE));
    if (pabyBuffer == nullptr)
    {
        VSIFCloseL(fp);
        return nullptr;
    }
    const size_t nRead = VSIFReadL(pabyBuffer, 1, BUFFER_SIZE, fp);
    VSIFCloseL(fp);
    if (nRead == BUFFER_SIZE)
    {
        CPLFree(pabyBuffer);
        return nullptr;
    }

    osSubFilename = VSIMemGenerateHiddenFilename("kmlsuperoverlay");
    VSIFCloseL(VSIFileFromMemBuffer(osSubFilename, pabyBuffer, nRead, TRUE));

    auto poDSIcon = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        osSubFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
    if (!poDSIcon)
    {
        VSIUnlink(osSubFilename);
        return nullptr;
    }

    return poDSIcon;
}

/************************************************************************/
/*                    KmlSuperOverlayComputeDepth()                     */
/************************************************************************/

static bool KmlSuperOverlayComputeDepth(const std::string &osFilename,
                                        CPLXMLNode *psDocument, int &nLevel)
{
    CPLXMLNode *psIter = psDocument->psChild;
    while (psIter != nullptr)
    {
        const char *pszHref = nullptr;
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "NetworkLink") == 0 &&
            CPLGetXMLNode(psIter, "Region") != nullptr &&
            (pszHref = CPLGetXMLValue(psIter, "Link.href", nullptr)) != nullptr)
        {
            const char *pszExt = CPLGetExtension(pszHref);
            if (EQUAL(pszExt, "kml"))
            {
                CPLString osSubFilename;
                if (STARTS_WITH(pszHref, "http"))
                    osSubFilename =
                        CPLSPrintf("/vsicurl_streaming/%s", pszHref);
                else
                {
                    osSubFilename = CPLFormFilename(
                        CPLGetPath(osFilename.c_str()), pszHref, nullptr);
                    osSubFilename = KMLRemoveSlash(osSubFilename);
                }

                VSILFILE *fp = VSIFOpenL(osSubFilename, "rb");
                if (fp != nullptr)
                {
                    char *pszBuffer = static_cast<char *>(
                        VSI_MALLOC_VERBOSE(BUFFER_SIZE + 1));
                    if (pszBuffer == nullptr)
                    {
                        VSIFCloseL(fp);
                        return false;
                    }
                    const size_t nRead =
                        VSIFReadL(pszBuffer, 1, BUFFER_SIZE, fp);
                    pszBuffer[nRead] = '\0';
                    VSIFCloseL(fp);
                    if (nRead == BUFFER_SIZE)
                    {
                        CPLFree(pszBuffer);
                    }
                    else
                    {
                        CPLXMLNode *psNode = CPLParseXMLString(pszBuffer);
                        CPLFree(pszBuffer);
                        if (psNode != nullptr)
                        {
                            CPLXMLNode *psRegion = nullptr;
                            CPLXMLNode *psNewDocument = nullptr;
                            CPLXMLNode *psGroundOverlay = nullptr;
                            CPLXMLNode *psLink = nullptr;
                            if (KmlSuperOverlayFindRegionStart(
                                    psNode, &psRegion, &psNewDocument,
                                    &psGroundOverlay, &psLink) &&
                                psNewDocument != nullptr && nLevel < 20)
                            {
                                nLevel++;
                                if (!KmlSuperOverlayComputeDepth(
                                        osSubFilename, psNewDocument, nLevel))
                                {
                                    CPLDestroyXMLNode(psNode);
                                    return false;
                                }
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
    return true;
}

/************************************************************************/
/*                    KmlSingleDocRasterDataset                         */
/************************************************************************/

class KmlSingleDocRasterRasterBand;

struct KmlSingleDocRasterTilesDesc
{
    int nMaxJ_i;    /* i index at which a tile with max j is realized */
    int nMaxJ_j;    /* j index at which a tile with max j is realized */
    int nMaxI_i;    /* i index at which a tile with max i is realized */
    int nMaxI_j;    /* j index at which a tile with max i is realized */
    char szExtJ[4]; /* extension of tile at which max j is realized */
    char szExtI[4]; /* extension of tile at which max i is realized */
};

class KmlSingleDocRasterDataset final : public GDALDataset
{
    friend class KmlSingleDocRasterRasterBand;
    OGRSpatialReference m_oSRS{};
    CPLString osDirname{};
    CPLString osNominalExt{};
    std::unique_ptr<GDALDataset> poCurTileDS{};
    std::array<double, 4> adfGlobalExtents = {0, 0, 0, 0};
    std::array<double, 6> adfGeoTransform = {0, 0, 0, 0, 0, 0};
    std::vector<std::unique_ptr<KmlSingleDocRasterDataset>> m_apoOverviews{};
    std::vector<KmlSingleDocRasterTilesDesc> aosDescs{};
    int nLevel = 0;
    int nTileSize = 0;
    bool bHasBuiltOverviews = false;
    bool bLockOtherBands = false;

  protected:
    virtual int CloseDependentDatasets() override;

  public:
    KmlSingleDocRasterDataset();
    virtual ~KmlSingleDocRasterDataset();

    virtual CPLErr GetGeoTransform(double *padfGeoTransform) override
    {
        memcpy(padfGeoTransform, adfGeoTransform.data(), 6 * sizeof(double));
        return CE_None;
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return &m_oSRS;
    }

    void BuildOverviews();

    static GDALDataset *Open(const char *pszFilename,
                             const CPLString &osFilename, CPLXMLNode *psNode);
};

/************************************************************************/
/*                    KmlSingleDocRasterRasterBand                      */
/************************************************************************/

class KmlSingleDocRasterRasterBand final : public GDALRasterBand
{
  public:
    KmlSingleDocRasterRasterBand(KmlSingleDocRasterDataset *poDS, int nBand);

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;
};

/************************************************************************/
/*                        KmlSingleDocRasterDataset()                   */
/************************************************************************/

KmlSingleDocRasterDataset::KmlSingleDocRasterDataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);
}

/************************************************************************/
/*                       ~KmlSingleDocRasterDataset()                   */
/************************************************************************/

KmlSingleDocRasterDataset::~KmlSingleDocRasterDataset()
{
    KmlSingleDocRasterDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                         CloseDependentDatasets()                     */
/************************************************************************/

int KmlSingleDocRasterDataset::CloseDependentDatasets()
{
    int bRet = FALSE;

    if (poCurTileDS)
    {
        bRet = TRUE;
        poCurTileDS.reset();
    }
    if (!m_apoOverviews.empty())
    {
        bRet = TRUE;
        m_apoOverviews.clear();
    }

    return bRet;
}

/************************************************************************/
/*                     KmlSingleDocGetDimensions()                      */
/************************************************************************/

static bool KmlSingleDocGetDimensions(const CPLString &osDirname,
                                      const KmlSingleDocRasterTilesDesc &oDesc,
                                      int nLevel, int nTileSize, int &nXSize,
                                      int &nYSize, int &nBands, int &bHasCT)
{
    const char *pszImageFilename = CPLFormFilename(
        osDirname,
        CPLSPrintf("kml_image_L%d_%d_%d", nLevel, oDesc.nMaxJ_j, oDesc.nMaxJ_i),
        oDesc.szExtJ);
    auto poImageDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        pszImageFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
    if (!poImageDS)
    {
        return false;
    }
    int nRightXSize;
    int nBottomYSize = poImageDS->GetRasterYSize();
    nBands = poImageDS->GetRasterCount();
    bHasCT = (nBands == 1 &&
              poImageDS->GetRasterBand(1)->GetColorTable() != nullptr);
    if (oDesc.nMaxJ_j == oDesc.nMaxI_j && oDesc.nMaxJ_i == oDesc.nMaxI_i)
    {
        nRightXSize = poImageDS->GetRasterXSize();
    }
    else
    {
        pszImageFilename =
            CPLFormFilename(osDirname,
                            CPLSPrintf("kml_image_L%d_%d_%d", nLevel,
                                       oDesc.nMaxI_j, oDesc.nMaxI_i),
                            oDesc.szExtI);
        poImageDS.reset(GDALDataset::Open(
            pszImageFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
        if (!poImageDS)
        {
            return false;
        }
        nRightXSize = poImageDS->GetRasterXSize();
    }

    nXSize = nRightXSize + oDesc.nMaxI_i * nTileSize;
    nYSize = nBottomYSize + oDesc.nMaxJ_j * nTileSize;
    return (nXSize > 0 && nYSize > 0);
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

void KmlSingleDocRasterDataset::BuildOverviews()
{
    if (bHasBuiltOverviews)
        return;
    bHasBuiltOverviews = TRUE;

    for (int k = 2; k <= static_cast<int>(aosDescs.size()); k++)
    {
        const KmlSingleDocRasterTilesDesc &oDesc =
            aosDescs[aosDescs.size() - k];
        int nXSize = 0;
        int nYSize = 0;
        int nTileBands = 0;
        int bHasCT = FALSE;
        if (!KmlSingleDocGetDimensions(
                osDirname, oDesc, static_cast<int>(aosDescs.size()) - k + 1,
                nTileSize, nXSize, nYSize, nTileBands, bHasCT))
        {
            break;
        }

        auto poOvrDS = std::make_unique<KmlSingleDocRasterDataset>();
        poOvrDS->nRasterXSize = nXSize;
        poOvrDS->nRasterYSize = nYSize;
        poOvrDS->nLevel = static_cast<int>(aosDescs.size()) - k + 1;
        poOvrDS->nTileSize = nTileSize;
        poOvrDS->osDirname = osDirname;
        poOvrDS->osNominalExt = oDesc.szExtI;
        poOvrDS->adfGeoTransform[0] = adfGlobalExtents[0];
        poOvrDS->adfGeoTransform[1] =
            (adfGlobalExtents[2] - adfGlobalExtents[0]) / poOvrDS->nRasterXSize;
        poOvrDS->adfGeoTransform[2] = 0.0;
        poOvrDS->adfGeoTransform[3] = adfGlobalExtents[3];
        poOvrDS->adfGeoTransform[4] = 0.0;
        poOvrDS->adfGeoTransform[5] =
            -(adfGlobalExtents[3] - adfGlobalExtents[1]) /
            poOvrDS->nRasterXSize;
        for (int iBand = 1; iBand <= nBands; iBand++)
            poOvrDS->SetBand(iBand,
                             std::make_unique<KmlSingleDocRasterRasterBand>(
                                 poOvrDS.get(), iBand));
        poOvrDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

        m_apoOverviews.push_back(std::move(poOvrDS));
    }
}

/************************************************************************/
/*                      KmlSingleDocRasterRasterBand()                  */
/************************************************************************/

KmlSingleDocRasterRasterBand::KmlSingleDocRasterRasterBand(
    KmlSingleDocRasterDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nBlockXSize = poDSIn->nTileSize;
    nBlockYSize = poDSIn->nTileSize;
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                               IReadBlock()                           */
/************************************************************************/

CPLErr KmlSingleDocRasterRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                                void *pImage)
{
    KmlSingleDocRasterDataset *poGDS =
        cpl::down_cast<KmlSingleDocRasterDataset *>(poDS);
    const char *pszImageFilename =
        CPLFormFilename(poGDS->osDirname,
                        CPLSPrintf("kml_image_L%d_%d_%d", poGDS->nLevel,
                                   nBlockYOff, nBlockXOff),
                        poGDS->osNominalExt);
    if (poGDS->poCurTileDS == nullptr ||
        strcmp(CPLGetFilename(poGDS->poCurTileDS->GetDescription()),
               CPLGetFilename(pszImageFilename)) != 0)
    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        poGDS->poCurTileDS.reset(
            GDALDataset::Open(pszImageFilename, GDAL_OF_RASTER));
    }
    GDALDataset *poImageDS = poGDS->poCurTileDS.get();
    if (poImageDS == nullptr)
    {
        memset(pImage, 0, static_cast<size_t>(nBlockXSize) * nBlockYSize);
        return CE_None;
    }
    int nXSize = poImageDS->GetRasterXSize();
    int nYSize = poImageDS->GetRasterYSize();

    int nReqXSize = nBlockXSize;
    if (nBlockXOff * nBlockXSize + nReqXSize > nRasterXSize)
        nReqXSize = nRasterXSize - nBlockXOff * nBlockXSize;
    int nReqYSize = nBlockYSize;
    if (nBlockYOff * nBlockYSize + nReqYSize > nRasterYSize)
        nReqYSize = nRasterYSize - nBlockYOff * nBlockYSize;

    if (nXSize != nReqXSize || nYSize != nReqYSize)
    {
        CPLDebug("KMLSUPEROVERLAY", "Tile %s, dimensions %dx%d, expected %dx%d",
                 pszImageFilename, nXSize, nYSize, nReqXSize, nReqYSize);
        return CE_Failure;
    }

    CPLErr eErr = CE_Failure;
    if (poImageDS->GetRasterCount() == 1)
    {
        GDALColorTable *poColorTable =
            poImageDS->GetRasterBand(1)->GetColorTable();
        if (nBand == 4 && poColorTable == nullptr)
        {
            /* Add fake alpha band */
            memset(pImage, 255, static_cast<size_t>(nBlockXSize) * nBlockYSize);
            eErr = CE_None;
        }
        else
        {
            eErr = poImageDS->GetRasterBand(1)->RasterIO(
                GF_Read, 0, 0, nXSize, nYSize, pImage, nXSize, nYSize, GDT_Byte,
                1, nBlockXSize, nullptr);

            /* Expand color table */
            if (eErr == CE_None && poColorTable != nullptr)
            {
                GByte *pabyImage = static_cast<GByte *>(pImage);
                int j, i;
                for (j = 0; j < nReqYSize; j++)
                {
                    for (i = 0; i < nReqXSize; i++)
                    {
                        GByte nVal = pabyImage[j * nBlockXSize + i];
                        const GDALColorEntry *poEntry =
                            poColorTable->GetColorEntry(nVal);
                        if (poEntry != nullptr)
                        {
                            if (nBand == 1)
                                pabyImage[j * nBlockXSize + i] =
                                    static_cast<GByte>(poEntry->c1);
                            else if (nBand == 2)
                                pabyImage[j * nBlockXSize + i] =
                                    static_cast<GByte>(poEntry->c2);
                            else if (nBand == 3)
                                pabyImage[j * nBlockXSize + i] =
                                    static_cast<GByte>(poEntry->c3);
                            else
                                pabyImage[j * nBlockXSize + i] =
                                    static_cast<GByte>(poEntry->c4);
                        }
                    }
                }
            }
        }
    }
    else if (nBand <= poImageDS->GetRasterCount())
    {
        eErr = poImageDS->GetRasterBand(nBand)->RasterIO(
            GF_Read, 0, 0, nXSize, nYSize, pImage, nXSize, nYSize, GDT_Byte, 1,
            nBlockXSize, nullptr);
    }
    else if (nBand == 4 && poImageDS->GetRasterCount() == 3)
    {
        /* Add fake alpha band */
        memset(pImage, 255, static_cast<size_t>(nBlockXSize) * nBlockYSize);
        eErr = CE_None;
    }

    /* Cache other bands */
    if (!poGDS->bLockOtherBands)
    {
        poGDS->bLockOtherBands = TRUE;
        for (int iBand = 1; iBand <= poGDS->nBands; iBand++)
        {
            if (iBand != nBand)
            {
                KmlSingleDocRasterRasterBand *poOtherBand =
                    static_cast<KmlSingleDocRasterRasterBand *>(
                        poGDS->GetRasterBand(iBand));
                GDALRasterBlock *poBlock =
                    poOtherBand->GetLockedBlockRef(nBlockXOff, nBlockYOff);
                if (poBlock == nullptr)
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

GDALColorInterp KmlSingleDocRasterRasterBand::GetColorInterpretation()
{
    return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int KmlSingleDocRasterRasterBand::GetOverviewCount()
{
    KmlSingleDocRasterDataset *poGDS =
        cpl::down_cast<KmlSingleDocRasterDataset *>(poDS);
    poGDS->BuildOverviews();

    return static_cast<int>(poGDS->m_apoOverviews.size());
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

GDALRasterBand *KmlSingleDocRasterRasterBand::GetOverview(int iOvr)
{
    KmlSingleDocRasterDataset *poGDS =
        cpl::down_cast<KmlSingleDocRasterDataset *>(poDS);
    poGDS->BuildOverviews();

    if (iOvr < 0 || iOvr >= static_cast<int>(poGDS->m_apoOverviews.size()))
        return nullptr;

    return poGDS->m_apoOverviews[iOvr]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       KmlSingleDocCollectTiles()                     */
/************************************************************************/

static void
KmlSingleDocCollectTiles(CPLXMLNode *psNode,
                         std::vector<KmlSingleDocRasterTilesDesc> &aosDescs,
                         CPLString &osURLBase)
{
    if (strcmp(psNode->pszValue, "href") == 0)
    {
        int level, j, i;
        char szExt[4];
        const char *pszHref = CPLGetXMLValue(psNode, "", "");
        if (STARTS_WITH(pszHref, "http"))
        {
            osURLBase = CPLGetPath(pszHref);
        }
        if (sscanf(CPLGetFilename(pszHref), "kml_image_L%d_%d_%d.%3s", &level,
                   &j, &i, szExt) == 4)
        {
            if (level > static_cast<int>(aosDescs.size()))
            {
                KmlSingleDocRasterTilesDesc sDesc;
                while (level > static_cast<int>(aosDescs.size()) + 1)
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
                /* 2010_USACE_JALBTCX_Louisiana_Mississippi_Lidar.kmz has not a
                 * lower-right tile */
                /* so the right most tile and the bottom most tile might be
                 * different */
                if ((j > aosDescs[level - 1].nMaxJ_j) ||
                    (j == aosDescs[level - 1].nMaxJ_j &&
                     i > aosDescs[level - 1].nMaxJ_i))
                {
                    aosDescs[level - 1].nMaxJ_j = j;
                    aosDescs[level - 1].nMaxJ_i = i;
                    strcpy(aosDescs[level - 1].szExtJ, szExt);
                }
                if (i > aosDescs[level - 1].nMaxI_i ||
                    (i == aosDescs[level - 1].nMaxI_i &&
                     j > aosDescs[level - 1].nMaxI_j))
                {
                    aosDescs[level - 1].nMaxI_j = j;
                    aosDescs[level - 1].nMaxI_i = i;
                    strcpy(aosDescs[level - 1].szExtI, szExt);
                }
            }
        }
    }
    else
    {
        CPLXMLNode *psIter = psNode->psChild;
        while (psIter != nullptr)
        {
            if (psIter->eType == CXT_Element)
                KmlSingleDocCollectTiles(psIter, aosDescs, osURLBase);
            psIter = psIter->psNext;
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/* Read raster with a structure like
 * http://opentopo.sdsc.edu/files/Haiti/NGA_Haiti_LiDAR2.kmz */
/* i.e. made of a doc.kml that list all tiles at all overview levels */
/* The tile name pattern is "kml_image_L{level}_{j}_{i}.{png|jpg}" */
GDALDataset *KmlSingleDocRasterDataset::Open(const char *pszFilename,
                                             const CPLString &osFilename,
                                             CPLXMLNode *psRoot)
{
    CPLXMLNode *psRootFolder = CPLGetXMLNode(psRoot, "=kml.Document.Folder");
    if (psRootFolder == nullptr)
        return nullptr;
    const char *pszRootFolderName = CPLGetXMLValue(psRootFolder, "name", "");
    if (strcmp(pszRootFolderName, "kml_image_L1_0_0") != 0)
        return nullptr;

    std::array<double, 4> adfGlobalExtents = {0, 0, 0, 0};
    CPLXMLNode *psRegion = CPLGetXMLNode(psRootFolder, "Region");
    if (psRegion == nullptr)
        return nullptr;
    if (!KmlSuperOverlayGetBoundingBox(psRegion, adfGlobalExtents))
        return nullptr;

    std::vector<KmlSingleDocRasterTilesDesc> aosDescs;
    CPLString osDirname = CPLGetPath(osFilename);
    KmlSingleDocCollectTiles(psRootFolder, aosDescs, osDirname);
    if (aosDescs.empty())
        return nullptr;
    for (const auto &oDesc : aosDescs)
    {
        if (oDesc.nMaxJ_i < 0)
            return nullptr;
    }

    const char *pszImageFilename =
        CPLFormFilename(osDirname,
                        CPLSPrintf("kml_image_L%d_%d_%d",
                                   static_cast<int>(aosDescs.size()), 0, 0),
                        aosDescs.back().szExtI);
    auto poImageDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        pszImageFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
    if (poImageDS == nullptr)
    {
        return nullptr;
    }
    int nTileSize = poImageDS->GetRasterXSize();
    if (nTileSize != poImageDS->GetRasterYSize())
    {
        nTileSize = 1024;
    }

    const KmlSingleDocRasterTilesDesc &oDesc = aosDescs.back();
    int nXSize = 0;
    int nYSize = 0;
    int nBands = 0;
    int bHasCT = FALSE;
    if (!KmlSingleDocGetDimensions(osDirname, oDesc,
                                   static_cast<int>(aosDescs.size()), nTileSize,
                                   nXSize, nYSize, nBands, bHasCT))
    {
        return nullptr;
    }

    auto poDS = std::make_unique<KmlSingleDocRasterDataset>();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nLevel = static_cast<int>(aosDescs.size());
    poDS->nTileSize = nTileSize;
    poDS->osDirname = std::move(osDirname);
    poDS->osNominalExt = oDesc.szExtI;
    poDS->adfGlobalExtents = adfGlobalExtents;
    poDS->adfGeoTransform[0] = adfGlobalExtents[0];
    poDS->adfGeoTransform[1] =
        (adfGlobalExtents[2] - adfGlobalExtents[0]) / poDS->nRasterXSize;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = adfGlobalExtents[3];
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] =
        -(adfGlobalExtents[3] - adfGlobalExtents[1]) / poDS->nRasterYSize;
    if (nBands == 1 && bHasCT)
        nBands = 4;
    for (int iBand = 1; iBand <= nBands; iBand++)
        poDS->SetBand(iBand, std::make_unique<KmlSingleDocRasterRasterBand>(
                                 poDS.get(), iBand));
    poDS->SetDescription(pszFilename);
    poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    poDS->aosDescs = std::move(aosDescs);

    return poDS.release();
}

/************************************************************************/
/*                   KmlSingleOverlayRasterDataset                      */
/************************************************************************/

class KmlSingleOverlayRasterDataset final : public VRTDataset
{
  public:
    KmlSingleOverlayRasterDataset(int nXSize, int nYSize)
        : VRTDataset(nXSize, nYSize)
    {
    }

    static GDALDataset *Open(const char *pszFilename,
                             const CPLString &osFilename, CPLXMLNode *psRoot);
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/* Read raster with a structure like https://trac.osgeo.org/gdal/ticket/6712 */
/* i.e. made of a doc.kml that has a single GroundOverlay */
GDALDataset *KmlSingleOverlayRasterDataset::Open(const char *pszFilename,
                                                 const CPLString &osFilename,
                                                 CPLXMLNode *psRoot)
{
    CPLXMLNode *psGO = CPLGetXMLNode(psRoot, "=kml.GroundOverlay");
    if (psGO == nullptr)
    {
        // Otherwise look for kml.Document.Folder.GroundOverlay if there's
        // a single occurrence of Folder and GroundOverlay
        auto psDoc = CPLGetXMLNode(psRoot, "=kml.Document");
        if (psDoc == nullptr)
        {
            return nullptr;
        }
        CPLXMLNode *psFolder = nullptr;
        for (auto psIter = psDoc->psChild; psIter; psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "Folder") == 0)
            {
                if (psFolder == nullptr)
                    psFolder = psIter;
                else
                    return nullptr;
            }
        }

        // folder is not mandatory -- some kml have a structure
        // kml.Document.GroundOverlay
        CPLXMLNode *psParent = psFolder != nullptr ? psFolder : psDoc;
        for (auto psIter = psParent->psChild; psIter; psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "GroundOverlay") == 0)
            {
                if (psGO == nullptr)
                    psGO = psIter;
                else
                    return nullptr;
            }
        }
        if (psGO == nullptr)
        {
            return nullptr;
        }
    }

    const char *pszHref = CPLGetXMLValue(psGO, "Icon.href", nullptr);
    if (pszHref == nullptr)
        return nullptr;
    std::array<double, 4> adfExtents = {0, 0, 0, 0};
    if (!KmlSuperOverlayGetBoundingBox(psGO, adfExtents))
        return nullptr;
    const char *pszImageFilename =
        CPLFormFilename(CPLGetPath(osFilename), pszHref, nullptr);
    GDALDataset *poImageDS =
        GDALDataset::FromHandle(GDALOpenShared(pszImageFilename, GA_ReadOnly));
    if (poImageDS == nullptr)
        return nullptr;

    auto poDS = std::make_unique<KmlSingleOverlayRasterDataset>(
        poImageDS->GetRasterXSize(), poImageDS->GetRasterYSize());
    for (int i = 1; i <= poImageDS->GetRasterCount(); ++i)
    {
        poDS->AddBand(GDT_Byte, nullptr);

        auto poImageBand = poImageDS->GetRasterBand(i);
        auto poVRTBand =
            static_cast<VRTSourcedRasterBand *>(poDS->GetRasterBand(i));
        poVRTBand->AddSimpleSource(
            poImageBand, 0, 0, poImageDS->GetRasterXSize(),
            poImageDS->GetRasterYSize(), 0, 0, poImageDS->GetRasterXSize(),
            poImageDS->GetRasterYSize(), nullptr, VRT_NODATA_UNSET);

        poVRTBand->SetColorInterpretation(
            poImageBand->GetColorInterpretation());

        const auto poCT = poImageBand->GetColorTable();
        if (poCT)
            poVRTBand->SetColorTable(poCT);
    }
    poImageDS->Dereference();
    double adfGeoTransform[6] = {
        adfExtents[0],
        (adfExtents[2] - adfExtents[0]) / poImageDS->GetRasterXSize(),
        0,
        adfExtents[3],
        0,
        -(adfExtents[3] - adfExtents[1]) / poImageDS->GetRasterYSize()};
    poDS->SetGeoTransform(adfGeoTransform);
    poDS->SetProjection(SRS_WKT_WGS84_LAT_LONG);
    poDS->SetWritable(false);
    poDS->SetDescription(pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *
KmlSuperOverlayReadDataset::Open(const char *pszFilename,
                                 KmlSuperOverlayReadDataset *poParent, int nRec)

{
    if (nRec == 2)
        return nullptr;
    CPLString osFilename(pszFilename);
    const char *pszExt = CPLGetExtension(pszFilename);
    if (EQUAL(pszExt, "kmz"))
    {
        if (!STARTS_WITH(pszFilename, "/vsizip/"))
            osFilename = CPLSPrintf("/vsizip/%s", pszFilename);
        char **papszFiles = VSIReadDir(osFilename);
        if (papszFiles == nullptr)
            return nullptr;
        char **papszIter = papszFiles;
        for (; *papszIter != nullptr; papszIter++)
        {
            pszExt = CPLGetExtension(*papszIter);
            if (EQUAL(pszExt, "kml"))
            {
                osFilename = CPLFormFilename(osFilename, *papszIter, nullptr);
                osFilename = KMLRemoveSlash(osFilename);
                break;
            }
        }
        CSLDestroy(papszFiles);
    }
    VSILFILE *fp = VSIFOpenL(osFilename, "rb");
    if (fp == nullptr)
        return nullptr;
    char *pszBuffer = static_cast<char *>(VSI_MALLOC_VERBOSE(BUFFER_SIZE + 1));
    if (pszBuffer == nullptr)
    {
        VSIFCloseL(fp);
        return nullptr;
    }
    const size_t nRead = VSIFReadL(pszBuffer, 1, BUFFER_SIZE, fp);
    pszBuffer[nRead] = '\0';
    VSIFCloseL(fp);
    if (nRead == BUFFER_SIZE)
    {
        CPLFree(pszBuffer);
        return nullptr;
    }

    CPLXMLNode *psNode = CPLParseXMLString(pszBuffer);
    CPLFree(pszBuffer);
    if (psNode == nullptr)
        return nullptr;

    GDALDataset *psSingleDocDS =
        KmlSingleDocRasterDataset::Open(pszFilename, osFilename, psNode);
    if (psSingleDocDS != nullptr)
    {
        CPLDestroyXMLNode(psNode);
        return psSingleDocDS;
    }

    CPLXMLNode *psRegion = nullptr;
    CPLXMLNode *psDocument = nullptr;
    CPLXMLNode *psGroundOverlay = nullptr;
    CPLXMLNode *psLink = nullptr;
    if (!KmlSuperOverlayFindRegionStart(psNode, &psRegion, &psDocument,
                                        &psGroundOverlay, &psLink))
    {
        // If we didn't find a super overlay, this still could be a valid kml
        // containing a single overlay. Test for that now. (Note that we need to
        // test first for super overlay in order to avoid false positive matches
        // of super overlay datasets to single overlay datasets)
        GDALDataset *psSingleOverlayDS = KmlSingleOverlayRasterDataset::Open(
            pszFilename, osFilename, psNode);
        CPLDestroyXMLNode(psNode);
        return psSingleOverlayDS;
    }

    if (psLink != nullptr)
    {
        const char *pszHref = CPLGetXMLValue(psLink, "href", nullptr);
        if (pszHref == nullptr || !EQUAL(CPLGetExtension(pszHref), "kml"))
        {
            CPLDestroyXMLNode(psNode);
            return nullptr;
        }

        CPLString osSubFilename;
        if (STARTS_WITH(pszHref, "http"))
            osSubFilename = CPLSPrintf("/vsicurl_streaming/%s", pszHref);
        else
        {
            osSubFilename =
                CPLFormFilename(CPLGetPath(osFilename), pszHref, nullptr);
            osSubFilename = KMLRemoveSlash(osSubFilename);
        }

        CPLString osOverlayName, osOverlayDescription;
        psDocument = CPLGetXMLNode(psNode, "=kml.Document");
        if (psDocument)
        {
            const char *pszOverlayName =
                CPLGetXMLValue(psDocument, "name", nullptr);
            if (pszOverlayName != nullptr &&
                strcmp(pszOverlayName, CPLGetBasename(pszFilename)) != 0)
            {
                osOverlayName = pszOverlayName;
            }
            const char *pszOverlayDescription =
                CPLGetXMLValue(psDocument, "description", nullptr);
            if (pszOverlayDescription != nullptr)
            {
                osOverlayDescription = pszOverlayDescription;
            }
        }

        CPLDestroyXMLNode(psNode);

        // FIXME
        GDALDataset *poDS = Open(osSubFilename, poParent, nRec + 1);
        if (poDS != nullptr)
        {
            poDS->SetDescription(pszFilename);

            if (!osOverlayName.empty())
            {
                poDS->SetMetadataItem("NAME", osOverlayName);
            }
            if (!osOverlayDescription.empty())
            {
                poDS->SetMetadataItem("DESCRIPTION", osOverlayDescription);
            }
        }

        return poDS;
    }

    CPLAssert(psDocument != nullptr);
    CPLAssert(psGroundOverlay != nullptr);
    CPLAssert(psRegion != nullptr);

    std::array<double, 4> adfExtents = {0, 0, 0, 0};
    if (!KmlSuperOverlayGetBoundingBox(psGroundOverlay, adfExtents))
    {
        CPLDestroyXMLNode(psNode);
        return nullptr;
    }

    const char *pszIcon = CPLGetXMLValue(psGroundOverlay, "Icon.href", nullptr);
    if (pszIcon == nullptr)
    {
        CPLDestroyXMLNode(psNode);
        return nullptr;
    }
    auto poDSIcon = KmlSuperOverlayLoadIcon(pszFilename, pszIcon);
    if (poDSIcon == nullptr)
    {
        CPLDestroyXMLNode(psNode);
        return nullptr;
    }

    int nFactor;
    if (poParent != nullptr)
        nFactor = poParent->nFactor / 2;
    else
    {
        int nDepth = 0;
        if (!KmlSuperOverlayComputeDepth(pszFilename, psDocument, nDepth))
        {
            CPLDestroyXMLNode(psNode);
            return nullptr;
        }
        nFactor = 1 << nDepth;
    }

    auto poDS = std::make_unique<KmlSuperOverlayReadDataset>();
    poDS->osFilename = pszFilename;
    poDS->psRoot = psNode;
    poDS->psDocument = psDocument;
    poDS->poParent = poParent;
    poDS->nFactor = nFactor;
    poDS->nRasterXSize = nFactor * poDSIcon->GetRasterXSize();
    poDS->nRasterYSize = nFactor * poDSIcon->GetRasterYSize();
    poDS->adfGeoTransform[0] = adfExtents[0];
    poDS->adfGeoTransform[1] =
        (adfExtents[2] - adfExtents[0]) / poDS->nRasterXSize;
    poDS->adfGeoTransform[3] = adfExtents[3];
    poDS->adfGeoTransform[5] =
        -(adfExtents[3] - adfExtents[1]) / poDS->nRasterYSize;
    poDS->nBands = 4;
    for (int i = 0; i < 4; i++)
        poDS->SetBand(i + 1, std::make_unique<KmlSuperOverlayRasterBand>(
                                 poDS.get(), i + 1));
    poDS->SetDescription(pszFilename);
    poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    while (poDS->poParent == nullptr && nFactor > 1)
    {
        nFactor /= 2;

        auto poOvrDS = std::make_unique<KmlSuperOverlayReadDataset>();

        poOvrDS->bIsOvr = true;
        // The life-time of objects is such that poOvrDS is destroyed when
        // poDS is destroyed.
        // coverity[escape]
        poOvrDS->poParent = poDS.get();
        poOvrDS->nFactor = nFactor;
        poOvrDS->nRasterXSize = nFactor * poDSIcon->GetRasterXSize();
        poOvrDS->nRasterYSize = nFactor * poDSIcon->GetRasterYSize();
        poOvrDS->adfGeoTransform[0] = adfExtents[0];
        poOvrDS->adfGeoTransform[1] =
            (adfExtents[2] - adfExtents[0]) / poOvrDS->nRasterXSize;
        poOvrDS->adfGeoTransform[3] = adfExtents[3];
        poOvrDS->adfGeoTransform[5] =
            -(adfExtents[3] - adfExtents[1]) / poOvrDS->nRasterYSize;
        poOvrDS->nBands = 4;
        for (int i = 0; i < 4; i++)
            poOvrDS->SetBand(i + 1, std::make_unique<KmlSuperOverlayRasterBand>(
                                        poOvrDS.get(), i + 1));
        poOvrDS->SetDescription(pszFilename);
        poOvrDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

        poDS->m_apoOverviewDS.push_back(std::move(poOvrDS));
    }
    poDS->poDSIcon = std::move(poDSIcon);

    return poDS.release();
}

/************************************************************************/
/*                    KmlSuperOverlayDatasetDelete()                    */
/************************************************************************/

static CPLErr KmlSuperOverlayDatasetDelete(CPL_UNUSED const char *fileName)
{
    /* Null implementation, so that people can Delete("MEM:::") */
    return CE_None;
}

/************************************************************************/
/*                    GDALRegister_KMLSUPEROVERLAY()                    */
/************************************************************************/

void CPL_DLL GDALRegister_KMLSUPEROVERLAY()

{
    if (GDALGetDriverByName("KMLSUPEROVERLAY") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("KMLSUPEROVERLAY");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Kml Super Overlay");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 "
                              "CInt16 CInt32 CFloat32 CFloat64");

    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "kml kmz");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='NAME' type='string' description='Overlay name'/>"
        "   <Option name='DESCRIPTION' type='string' description='Overlay "
        "description'/>"
        "   <Option name='ALTITUDE' type='float' description='Distance above "
        "the earth surface, in meters, interpreted according to the altitude "
        "mode'/>"
        "   <Option name='ALTITUDEMODE' type='string-select' "
        "default='clampToGround' description='Specifies hows the altitude is "
        "interpreted'>"
        "       <Value>clampToGround</Value>"
        "       <Value>absolute</Value>"
        "       <Value>relativeToSeaFloor</Value>"
        "       <Value>clampToSeaFloor</Value>"
        "   </Option>"
        "   <Option name='FORMAT' type='string-select' default='JPEG' "
        "description='Format of the tiles'>"
        "       <Value>PNG</Value>"
        "       <Value>JPEG</Value>"
        "       <Value>AUTO</Value>"
        "   </Option>"
        "   <Option name='FIX_ANTIMERIDIAN' type='boolean' description='Fix "
        "for images crossing the antimeridian causing errors in Google Earth' "
        "/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = KmlSuperOverlayReadDataset::Identify;
    poDriver->pfnOpen = KmlSuperOverlayReadDataset::Open;
    poDriver->pfnCreateCopy = KmlSuperOverlayCreateCopy;
    poDriver->pfnDelete = KmlSuperOverlayDatasetDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
