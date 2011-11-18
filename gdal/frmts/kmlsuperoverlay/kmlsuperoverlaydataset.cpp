/******************************************************************************
 * $Id$
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
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

using namespace std;

/************************************************************************/
/*                           GenerateTiles()                            */
/************************************************************************/
void GenerateTiles(std::string filename, 
                   int zoom, int rxsize, 
                   int rysize, int ix, int iy, 
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
   
    if (isJpegDriver == false)//Jpeg dataset only has one or three bands
    {
        if (bands < 4)//add transparency to files with one band or three bands
        {
            poTmpDataset->AddBand(GDT_Byte);
            alphaBand = poTmpDataset->GetRasterBand(poTmpDataset->GetRasterCount());
        }
    }

    int rowOffset = rysize/dysize;
    int loopCount = rysize/rowOffset;
    for (int row = 0; row < loopCount; row++)
    {
        if (isJpegDriver == false)
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
            bool isSigned = false;
            double noDataValue = poBand->GetNoDataValue(&hasNoData);
            const char* pixelType = poBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            if (pixelType)
            {
                if (strcmp(pixelType, "SIGNEDBYTE") == 0)
                {
                    isSigned = true; 
                }
            }

            GDALRasterBand* poBandtmp = NULL;
            if (poTmpDataset)
            {
                poBandtmp = poTmpDataset->GetRasterBand(band);
            }

            int yOffset = ry + row * rowOffset;
            bool bReadFailed = false;
            if (poBand)
            {
                CPLErr errTest = 
                    poBand->RasterIO( GF_Read, rx, yOffset, rxsize, rowOffset, pafScanline, dxsize, 1, GDT_Byte, 0, 0);

                if ( errTest == CE_Failure )
                {
                    hasNoData = 1;
                    bReadFailed = true;
                }
            }


            //fill the true or false for hadnoData array if the source data has nodata value
            if (isJpegDriver == false)
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
                        if (tmpv == noDataValue || bReadFailed == true)
                        {
                            hadnoData[j] = true;
                        }
                    }
                }
            }

            if (poBandtmp && bReadFailed == false)
            {
                poBandtmp->RasterIO(GF_Write, 0, row, dxsize, 1, pafScanline, dxsize, 1, GDT_Byte, 
                                    0, 0);
            }
        } 

        //fill the values for alpha band
        if (isJpegDriver == false)
        {
            if (alphaBand)
            {
                for (int i = 0; i < dxsize; i++)
                {
                    if (hadnoData[i] == true)
                    {
                        pafScanline[i] = 0;
                    }
                    else
                    {
                        pafScanline[i] = 255;
                    }
                }    

                alphaBand->RasterIO(GF_Write, 0, row, dxsize, 1, pafScanline, dxsize, 1, GDT_Byte, 
                                    0, 0);
            }
        }
    }

    delete [] pafScanline;
    delete [] hadnoData;

    GDALDataset* outDs = poOutputTileDriver->CreateCopy(filename.c_str(), poTmpDataset, FALSE, NULL, NULL, NULL);

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
                     int tilesize)
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
    // If we haven't writen any features yet, output the layer's schema
    VSIFPrintfL(fp, "<kml xmlns=\"http://earth.google.com/kml/2.1\">\n");
    VSIFPrintfL(fp, "\t<Document>\n");
    VSIFPrintfL(fp, "\t\t<name>%s</name>\n", tmpfilename);
    VSIFPrintfL(fp, "\t\t<description></description>\n");
    VSIFPrintfL(fp, "\t\t<Style>\n");
    VSIFPrintfL(fp, "\t\t\t<ListStyle id=\"hideChildren\">\n");
    VSIFPrintfL(fp, "\t\t\t\t<listItemType>checkHideChildren</listItemType>\n");
    VSIFPrintfL(fp, "\t\t\t</ListStyle>\n");
    VSIFPrintfL(fp, "\t\t</Style>\n");
    VSIFPrintfL(fp, "\t\t<Region>\n \t\t<LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t<north>%f</north>\n", north);
    VSIFPrintfL(fp, "\t\t\t\t<south>%f</south>\n", south);
    VSIFPrintfL(fp, "\t\t\t\t<east>%f</east>\n", east);
    VSIFPrintfL(fp, "\t\t\t\t<west>%f</west>\n", west);
    VSIFPrintfL(fp, "\t\t\t</LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t</Region>\n");
    VSIFPrintfL(fp, "\t\t<NetworkLink>\n");
    VSIFPrintfL(fp, "\t\t\t<open>1</open>\n");
    VSIFPrintfL(fp, "\t\t\t<Region>\n");
    VSIFPrintfL(fp, "\t\t\t\t<Lod>\n");
    VSIFPrintfL(fp, "\t\t\t\t\t<minLodPixels>%d</minLodPixels>\n", minlodpixels);
    VSIFPrintfL(fp, "\t\t\t\t\t<maxLodPixels>-1</maxLodPixels>\n");
    VSIFPrintfL(fp, "\t\t\t\t</Lod>\n");
    VSIFPrintfL(fp, "\t\t\t\t<LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t\t<north>%f</north>\n", north);
    VSIFPrintfL(fp, "\t\t\t\t\t<south>%f</south>\n", south);
    VSIFPrintfL(fp, "\t\t\t\t\t<east>%f</east>\n", east);
    VSIFPrintfL(fp, "\t\t\t\t\t<west>%f</west>\n", west);
    VSIFPrintfL(fp, "\t\t\t\t</LatLonAltBox>\n");
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
                      std::string fileExt)
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

    std::vector<int> xchildren;
    std::vector<int> ychildern;

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

    VSIFPrintfL(fp, "<kml xmlns=\"http://earth.google.com/kml/2.1\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n");
    VSIFPrintfL(fp, "\t<Document>\n");
    VSIFPrintfL(fp, "\t\t<name>%d/%d/%d.kml</name>\n", zoom, ix, iy);
    VSIFPrintfL(fp, "\t\t<Style>\n");
    VSIFPrintfL(fp, "\t\t\t<ListStyle id=\"hideChildren\">\n");
    VSIFPrintfL(fp, "\t\t\t\t<listItemType>checkHideChildren</listItemType>\n");
    VSIFPrintfL(fp, "\t\t\t</ListStyle>\n");
    VSIFPrintfL(fp, "\t\t</Style>\n");
    VSIFPrintfL(fp, "\t\t<Region>\n");
    VSIFPrintfL(fp, "\t\t\t<Lod>\n");
    VSIFPrintfL(fp, "\t\t\t\t<minLodPixels>%d</minLodPixels>\n", 128);
    VSIFPrintfL(fp, "\t\t\t\t<maxLodPixels>%d</maxLodPixels>\n", maxLodPix);
    VSIFPrintfL(fp, "\t\t\t</Lod>\n");
    VSIFPrintfL(fp, "\t\t\t<LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t\t\t<north>%f</north>\n", tnorth);
    VSIFPrintfL(fp, "\t\t\t\t<south>%f</south>\n", tsouth);
    VSIFPrintfL(fp, "\t\t\t\t<east>%f</east>\n", teast);
    VSIFPrintfL(fp, "\t\t\t\t<west>%f</west>\n", twest);
    VSIFPrintfL(fp, "\t\t\t</LatLonAltBox>\n");
    VSIFPrintfL(fp, "\t\t</Region>\n");
    VSIFPrintfL(fp, "\t\t<GroundOverlay>\n");
    VSIFPrintfL(fp, "\t\t\t<drawOrder>%d</drawOrder>\n", zoom);
    VSIFPrintfL(fp, "\t\t\t<Icon>\n");
    VSIFPrintfL(fp, "\t\t\t\t<href>%d%s</href>\n", iy, fileExt.c_str());
    VSIFPrintfL(fp, "\t\t\t</Icon>\n");
    VSIFPrintfL(fp, "\t\t\t<gx:LatLonQuad>\n");
    VSIFPrintfL(fp, "\t\t\t\t<coordinates>\n");
    VSIFPrintfL(fp, "\t\t\t\t\t%f, %f, 0\n", lowerleftT, leftbottomT);
    VSIFPrintfL(fp, "\t\t\t\t\t%f, %f, 0\n", lowerrightT, rightbottomT);
    VSIFPrintfL(fp, "\t\t\t\t\t%f, %f, 0\n", upperrightT, righttopT);
    VSIFPrintfL(fp, "\t\t\t\t\t%f, %f, 0\n", upperleftT, lefttopT);
    VSIFPrintfL(fp, "\t\t\t\t</coordinates>\n");
    VSIFPrintfL(fp, "\t\t\t</gx:LatLonQuad>\n");
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

            VSIFPrintfL(fp, "\t\t<NetworkLink>\n");
            VSIFPrintfL(fp, "\t\t\t<name>%d/%d/%d%s</name>\n", zoom+1, cx, cy, fileExt.c_str());
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
/*                           zipWithMinizip()                           */
/************************************************************************/
bool zipWithMinizip(std::vector<std::string> srcFiles, std::string srcDirectory, std::string targetFile)
{
    void  *zipfile = CPLCreateZip(targetFile.c_str(), NULL);
    if (!zipfile)
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to open target zip file.." );
        return false;
    }

    std::vector <std::string>::iterator v1_Iter;
    for(v1_Iter = srcFiles.begin(); v1_Iter != srcFiles.end(); v1_Iter++)
    {
        std::string fileRead = *v1_Iter;

        // Find relative path and open new file with zip file
        std::string relativeFileReadPath = fileRead;
        int remNumChars = srcDirectory.size();
        if(remNumChars > 0)
        {
            int f = fileRead.find(srcDirectory);
            if( f >= 0 )
            {
                relativeFileReadPath.erase(f, remNumChars + 1); // 1 added for backslash at the end
            }      
        }

        std::basic_string<char>::iterator iter1;
        for (iter1 = relativeFileReadPath.begin(); iter1 != relativeFileReadPath.end(); iter1++)
        {
            int f = relativeFileReadPath.find("\\");
            if (f >= 0)
            {
                relativeFileReadPath.replace(f, 1, "/");
            }
            else
            {
                break;
            }
        }
        if (CPLCreateFileInZip(zipfile, relativeFileReadPath.c_str(), NULL) != CE_None)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to create file within the zip file.." );
            return false;
        }

        // Read source file and write to zip file
        VSILFILE* fp = VSIFOpenL(fileRead.c_str(), "rb");
        if (fp == NULL)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Could not open source file.." );
            return false;
        }

        // Read file in buffer
        std::string fileData;
        const unsigned int bufSize = 1024;
        char buf[bufSize];
        int nRead;
        while((nRead = VSIFReadL(buf, 1, bufSize, fp)) != 0)
        {
            if ( CPLWriteFileInZip(zipfile, buf, nRead) != CE_None )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Could not write to file within zip file.." );
                CPLCloseFileInZip(zipfile);
                CPLCloseZip(zipfile);
                VSIFCloseL(fp);
                return false;
            }
        }

        VSIFCloseL(fp);

        // Close one src file zipped completely
        if ( CPLCloseFileInZip(zipfile) != CE_None )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Could not close file written within zip file.." );
            CPLCloseZip(zipfile);
            return false;
        }
    }

    CPLCloseZip(zipfile);

    return true;
}

/************************************************************************/
/*                       KmlSuperOverlayDataset()                       */
/************************************************************************/

KmlSuperOverlayDataset::KmlSuperOverlayDataset()

{
    bGeoTransformSet = FALSE;
}

/************************************************************************/
/*                      ~KmlSuperOverlayDataset()                       */
/************************************************************************/

KmlSuperOverlayDataset::~KmlSuperOverlayDataset()
   
{
    FlushCache();
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *KmlSuperOverlayDataset::GetProjectionRef()
   
{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                   KMLSuperOverlayRecursiveUnlink()                   */
/************************************************************************/

static void KMLSuperOverlayRecursiveUnlink( const char *pszName )

{
    char **papszFileList;
    int i;

    papszFileList = CPLReadDir( pszName );

    for( i = 0; papszFileList != NULL && papszFileList[i] != NULL; i++ )
    {
        VSIStatBufL  sStatBuf;

        if( EQUAL(papszFileList[i],".") || EQUAL(papszFileList[i],"..") )
            continue;

        CPLString osFullFilename =
                 CPLFormFilename( pszName, papszFileList[i], NULL );

        VSIStatL( osFullFilename, &sStatBuf );

        if( VSI_ISREG( sStatBuf.st_mode ) )
        {
            VSIUnlink( osFullFilename );
        }
        else if( VSI_ISDIR( sStatBuf.st_mode ) )
        {
            KMLSuperOverlayRecursiveUnlink( osFullFilename );
        }
    }

    CSLDestroy( papszFileList );

    VSIRmdir( pszName );
}

/************************************************************************/
/*                           CreateCopy()                               */
/************************************************************************/

GDALDataset *KmlSuperOverlayDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                                                 int bStrict, char ** papszOptions, GDALProgressFunc pfnProgress, void * pProgressData)
{
    bool isKmz = false;

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
    CPLString outDir = output_dir;
    CPLFree(output_dir);
    output_dir = NULL;

    KmlSuperOverlayDataset *poDsDummy = new KmlSuperOverlayDataset();

    if (isKmz)
    {
        outDir = CPLFormFilename(outDir, CPLSPrintf("kmlsuperoverlaytmp_%p", poDsDummy) , NULL);
        if (VSIMkdir(outDir, 0755) != 0)
        {
            CPLError( CE_Failure, CPLE_None,
                    "Cannot create %s", outDir.c_str() );
            delete poDsDummy;
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
        delete poDsDummy;
        if (isKmz)
            KMLSuperOverlayRecursiveUnlink(outDir);
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

    //Zoom levels of the pyramid.
    int maxzoom;
    int tilexsize;
    int tileysize;
    // Let the longer side determine the max zoom level and x/y tilesizes.
    if ( xsize >= ysize )
    {
        double dtilexsize = xsize;
        while (dtilexsize > 400) //calculate x tile size
        {
            dtilexsize = dtilexsize/2;
        }

        maxzoom   = static_cast<int>(log( (double)xsize / dtilexsize ) / log(2.0));
        tilexsize = (int)dtilexsize;
        tileysize = (int)( (double)(dtilexsize * ysize) / xsize );
    }
    else
    {
        double dtileysize = ysize;
        while (dtileysize > 400) //calculate y tile size
        {
            dtileysize = dtileysize/2;
        }

        maxzoom   = static_cast<int>(log( (double)ysize / dtileysize ) / log(2.0));
        tileysize = (int)dtileysize;
        tilexsize = (int)( (double)(dtileysize * xsize) / ysize );
    }
    maxzoom = 0;

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
    if (isKmz)
    {
        tmpFileName = CPLFormFilename(outDir, "tmp.kml", NULL);
        nRet = GenerateRootKml(tmpFileName.c_str(), pszFilename, north, south, east, west, (int)tilexsize);
        fileVector.push_back(tmpFileName);
    }
    else
    {
        nRet = GenerateRootKml(pszFilename, pszFilename, north, south, east, west, (int)tilexsize);
    }
    
    if (nRet == FALSE)
    {
        OGRCoordinateTransformation::DestroyCT( poTransform );
        delete poDsDummy;
        if (isKmz)
            KMLSuperOverlayRecursiveUnlink(outDir);
        return NULL;
    }

    for (int zoom = maxzoom; zoom >= 0; --zoom)
    {
        int rmaxxsize = static_cast<int>(pow(2.0, (maxzoom-zoom)) * tilexsize);
        int rmaxysize = static_cast<int>(pow(2.0, (maxzoom-zoom)) * tileysize);

        int xloop = (int)xsize/rmaxxsize;
        int yloop = (int)ysize/rmaxysize;

        xloop = xloop>0 ? xloop : 1;
        yloop = yloop>0 ? yloop : 1;
        for (int ix = 0; ix < xloop; ix++)
        {
            int rxsize = (int)(rmaxxsize);
            int rx = (int)(ix * rmaxxsize);

            for (int iy = 0; iy < yloop; iy++)
            {
                int rysize = (int)(rmaxysize);
                int ry = (int)(ysize - (iy * rmaxysize)) - rysize;

                int dxsize = (int)(rxsize/rmaxxsize * tilexsize);
                int dysize = (int)(rysize/rmaxysize * tileysize);

                std::stringstream zoomStr;
                std::stringstream ixStr;
                std::stringstream iyStr;

                zoomStr << zoom;
                ixStr << ix;
                iyStr << iy;

                std::string zoomDir = outDir;
                zoomDir+= "/" + zoomStr.str();
                VSIMkdir(zoomDir.c_str(), 0775);
        

                zoomDir = zoomDir + "/" + ixStr.str();
                VSIMkdir(zoomDir.c_str(), 0775);

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
                                 dxsize, dysize, tmpSouth, adfGeoTransform[0], xsize, ysize, maxzoom, poTransform, fileExt);
            }
        }
    }

    OGRCoordinateTransformation::DestroyCT( poTransform );
    poTransform = NULL;
    
    if (isKmz)
    {
        std::string outputfile = pszFilename;
        bool zipDone = true;
        if (zipWithMinizip(fileVector, outDir, outputfile) == false)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to do zip.." );
            zipDone = false;
        }

        KMLSuperOverlayRecursiveUnlink(outDir);

        if (zipDone == false)
        {
            delete poDsDummy;
            return NULL;
        }
    }

    return poDsDummy;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *KmlSuperOverlayDataset::Open(GDALOpenInfo *)

{
    return NULL;
}

/************************************************************************/
/*                    KmlSuperOverlayDatasetDelete()                    */
/************************************************************************/

static CPLErr KmlSuperOverlayDatasetDelete(const char* fileName)
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
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Kml Super Overlay" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='JPEG' description='Force of the tiles'>"
"       <Value>PNG</Value>"
"       <Value>JPEG</Value>"
"   </Option>"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = KmlSuperOverlayDataset::Open;
        poDriver->pfnCreateCopy = KmlSuperOverlayDataset::CreateCopy;
        poDriver->pfnDelete = KmlSuperOverlayDatasetDelete;
      
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

