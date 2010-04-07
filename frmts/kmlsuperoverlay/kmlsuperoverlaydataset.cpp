// $Id$

#include "kmlsuperoverlaydataset.h"

#include <cmath>   /* fabs */
#include <cstring> /* strdup */
#include <iostream>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <fstream>
#include <zip.h>

#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "../../ogr/ogr_spatialref.h"

using namespace std;

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
   GDALClose(outDs);
}


void GenerateRootKml(const char* filename, 
                     const char* kmlfilename,
                     double north, 
                     double south, 
                     double east, 
                     double west, 
                     int tilesize)
{
   FILE* fp = VSIFOpen(filename, "wb");
   CPLAssert( NULL != fp );
   int minlodpixels = tilesize/2;

   const char* tmpfilename = CPLGetBasename(kmlfilename);
   // If we haven't writen any features yet, output the layer's schema
   VSIFPrintf(fp, "<kml xmlns=\"http://earth.google.com/kml/2.1\">\n");
   VSIFPrintf(fp, "\t<Document>\n");
   VSIFPrintf(fp, "\t\t<name>%s</name>\n", tmpfilename);
   VSIFPrintf(fp, "\t\t<description></description>\n");
   VSIFPrintf(fp, "\t\t<Style>\n");
   VSIFPrintf(fp, "\t\t\t<ListStyle id=\"hideChildren\">\n");
   VSIFPrintf(fp, "\t\t\t\t<listItemType>checkHideChildren</listItemType>\n");
   VSIFPrintf(fp, "\t\t\t</ListStyle>\n");
   VSIFPrintf(fp, "\t\t</Style>\n");
   VSIFPrintf(fp, "\t\t<Region>\n \t\t<LatLonAltBox>\n");
   VSIFPrintf(fp, "\t\t\t\t<north>%f</north>\n", north);
   VSIFPrintf(fp, "\t\t\t\t<south>%f</south>\n", south);
   VSIFPrintf(fp, "\t\t\t\t<east>%f</east>\n", east);
   VSIFPrintf(fp, "\t\t\t\t<west>%f</west>\n", west);
   VSIFPrintf(fp, "\t\t\t</LatLonAltBox>\n");
   VSIFPrintf(fp, "\t\t</Region>\n");
   VSIFPrintf(fp, "\t\t<NetworkLink>\n");
   VSIFPrintf(fp, "\t\t\t<open>1</open>\n");
   VSIFPrintf(fp, "\t\t\t<Region>\n");
   VSIFPrintf(fp, "\t\t\t\t<Lod>\n");
   VSIFPrintf(fp, "\t\t\t\t\t<minLodPixels>%d</minLodPixels>\n", minlodpixels);
   VSIFPrintf(fp, "\t\t\t\t\t<maxLodPixels>-1</maxLodPixels>\n");
   VSIFPrintf(fp, "\t\t\t\t</Lod>\n");
   VSIFPrintf(fp, "\t\t\t\t<LatLonAltBox>\n");
   VSIFPrintf(fp, "\t\t\t\t\t<north>%f</north>\n", north);
   VSIFPrintf(fp, "\t\t\t\t\t<south>%f</south>\n", south);
   VSIFPrintf(fp, "\t\t\t\t\t<east>%f</east>\n", east);
   VSIFPrintf(fp, "\t\t\t\t\t<west>%f</west>\n", west);
   VSIFPrintf(fp, "\t\t\t\t</LatLonAltBox>\n");
   VSIFPrintf(fp, "\t\t\t</Region>\n");
   VSIFPrintf(fp, "\t\t\t<Link>\n");
   VSIFPrintf(fp, "\t\t\t\t<href>0/0/0.kml</href>\n");
   VSIFPrintf(fp, "\t\t\t\t<viewRefreshMode>onRegion</viewRefreshMode>\n");
   VSIFPrintf(fp, "\t\t\t</Link>\n");
   VSIFPrintf(fp, "\t\t</NetworkLink>\n");
   VSIFPrintf(fp, "\t</Document>\n");
   VSIFPrintf(fp, "</kml>\n");

   VSIFClose(fp);
}

void GenerateChildKml(std::string filename, 
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

   FILE* fp = VSIFOpen(filename.c_str(), "wb");
   CPLAssert( NULL != fp );

   VSIFPrintf(fp, "<kml xmlns=\"http://earth.google.com/kml/2.1\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n");
   VSIFPrintf(fp, "\t<Document>\n");
   VSIFPrintf(fp, "\t\t<name>%d/%d/%d.kml</name>\n", zoom, ix, iy);
   VSIFPrintf(fp, "\t\t<Style>\n");
   VSIFPrintf(fp, "\t\t\t<ListStyle id=\"hideChildren\">\n");
   VSIFPrintf(fp, "\t\t\t\t<listItemType>checkHideChildren</listItemType>\n");
   VSIFPrintf(fp, "\t\t\t</ListStyle>\n");
   VSIFPrintf(fp, "\t\t</Style>\n");
   VSIFPrintf(fp, "\t\t<Region>\n");
   VSIFPrintf(fp, "\t\t\t<Lod>\n");
   VSIFPrintf(fp, "\t\t\t\t<minLodPixels>%d</minLodPixels>\n", 128);
   VSIFPrintf(fp, "\t\t\t\t<maxLodPixels>%d</maxLodPixels>\n", maxLodPix);
   VSIFPrintf(fp, "\t\t\t</Lod>\n");
   VSIFPrintf(fp, "\t\t\t<LatLonAltBox>\n");
   VSIFPrintf(fp, "\t\t\t\t<north>%f</north>\n", tnorth);
   VSIFPrintf(fp, "\t\t\t\t<south>%f</south>\n", tsouth);
   VSIFPrintf(fp, "\t\t\t\t<east>%f</east>\n", teast);
   VSIFPrintf(fp, "\t\t\t\t<west>%f</west>\n", twest);
   VSIFPrintf(fp, "\t\t\t</LatLonAltBox>\n");
   VSIFPrintf(fp, "\t\t</Region>\n");
   VSIFPrintf(fp, "\t\t<GroundOverlay>\n");
   VSIFPrintf(fp, "\t\t\t<drawOrder>%d</drawOrder>\n", zoom);
   VSIFPrintf(fp, "\t\t\t<Icon>\n");
   VSIFPrintf(fp, "\t\t\t\t<href>%d%s</href>\n", iy, fileExt.c_str());
   VSIFPrintf(fp, "\t\t\t</Icon>\n");
   VSIFPrintf(fp, "\t\t\t<gx:LatLonQuad>\n");
   VSIFPrintf(fp, "\t\t\t\t<coordinates>\n");
   VSIFPrintf(fp, "\t\t\t\t\t%f, %f, 0\n", lowerleftT, leftbottomT);
   VSIFPrintf(fp, "\t\t\t\t\t%f, %f, 0\n", lowerrightT, rightbottomT);
   VSIFPrintf(fp, "\t\t\t\t\t%f, %f, 0\n", upperrightT, righttopT);
   VSIFPrintf(fp, "\t\t\t\t\t%f, %f, 0\n", upperleftT, lefttopT);
   VSIFPrintf(fp, "\t\t\t\t</coordinates>\n");
   VSIFPrintf(fp, "\t\t\t</gx:LatLonQuad>\n");
   VSIFPrintf(fp, "\t\t</GroundOverlay>\n");

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

         VSIFPrintf(fp, "\t\t<NetworkLink>\n");
         VSIFPrintf(fp, "\t\t\t<name>%d/%d/%d%s</name>\n", zoom+1, cx, cy, fileExt.c_str());
         VSIFPrintf(fp, "\t\t\t<Region>\n");
         VSIFPrintf(fp, "\t\t\t\t<Lod>\n");
         VSIFPrintf(fp, "\t\t\t\t\t<minLodPixels>128</minLodPixels>\n");
         VSIFPrintf(fp, "\t\t\t\t\t<maxLodPixels>-1</maxLodPixels>\n");
         VSIFPrintf(fp, "\t\t\t\t</Lod>\n");
         VSIFPrintf(fp, "\t\t\t\t<LatLonAltBox>\n");
         VSIFPrintf(fp, "\t\t\t\t\t<north>%f</north>\n", cnorth);
         VSIFPrintf(fp, "\t\t\t\t\t<south>%f</south>\n", csouth);
         VSIFPrintf(fp, "\t\t\t\t\t<east>%f</east>\n", ceast);
         VSIFPrintf(fp, "\t\t\t\t\t<west>%f</west>\n", cwest);
         VSIFPrintf(fp, "\t\t\t\t</LatLonAltBox>\n");
         VSIFPrintf(fp, "\t\t\t</Region>\n");
         VSIFPrintf(fp, "\t\t\t<Link>\n");
         VSIFPrintf(fp, "\t\t\t\t<href>../../%d/%d/%d.kml</href>\n", zoom+1, cx, cy);
         VSIFPrintf(fp, "\t\t\t\t<viewRefreshMode>onRegion</viewRefreshMode>\n");
         VSIFPrintf(fp, "\t\t\t\t<viewFormat/>\n");
         VSIFPrintf(fp, "\t\t\t</Link>\n");
         VSIFPrintf(fp, "\t\t</NetworkLink>\n");
      }
   }

   VSIFPrintf(fp, "\t</Document>\n");
   VSIFPrintf(fp, "</kml>\n");
   VSIFClose(fp);
}

bool zipWithMinizip(std::vector<std::string> srcFiles, std::string srcDirectory, std::string targetFile)
{
   zipFile zipfile = zipOpen(targetFile.c_str(), 0);
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
      if (zipOpenNewFileInZip(zipfile, relativeFileReadPath.c_str(), 0, 0, 0, 0, 0, 0, Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK)
      {
         CPLError( CE_Failure, CPLE_FileIO,
                   "Unable to create file within the zip file.." );
         return false;
      }

      // Read source file and write to zip file
      std::ifstream inFile (fileRead.c_str(), std::ios_base::binary | std::ios_base::in);
      if (!inFile.is_open())
      {
         CPLError( CE_Failure, CPLE_FileIO,
                   "Could not open source file.." );
         return false;
      }
      if (!inFile.good())
      {
         CPLError( CE_Failure, CPLE_FileIO,
                   "Error reading source file.." );
         return false;
      }

      // Read file in buffer
      std::string fileData;
      const unsigned int bufSize = 1024;
      char buf[bufSize];
      do 
      {
         inFile.read(buf, bufSize);
         fileData.append(buf, inFile.gcount());
      } while (!inFile.eof() && inFile.good());

      if ( zipWriteInFileInZip(zipfile, static_cast<const void*>(fileData.data()), static_cast<unsigned int>(fileData.size())) != ZIP_OK )
      {
         CPLError( CE_Failure, CPLE_FileIO,
                   "Could not write to file within zip file.." );
         return false;
      }

      // Close one src file zipped completely
      if ( zipCloseFileInZip(zipfile) != ZIP_OK )
      {
         CPLError( CE_Failure, CPLE_FileIO,
                   "Could not close file written within zip file.." );
         return false;
      }
   }

   zipClose(zipfile, 0);
   return true;
}

/************************************************************************/
/*                            KmlSuperOverlayDataset()                             */
/************************************************************************/

KmlSuperOverlayDataset::KmlSuperOverlayDataset()

{
   pszProjection = NULL;
   bGeoTransformSet = FALSE;
}

/************************************************************************/
/*                            ~KmlSuperOverlayDataset()                            */
/************************************************************************/

KmlSuperOverlayDataset::~KmlSuperOverlayDataset()
   
{
   FlushCache();
   CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *KmlSuperOverlayDataset::GetProjectionRef()
   
{
   OGRSpatialReference poLatLong;
   poLatLong.SetWellKnownGeogCS( "WGS84" );
   poLatLong.exportToProj4(&pszProjection);
   if( pszProjection == NULL )
      return "";
   else
      return pszProjection;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *KmlSuperOverlayDataset::Create(const char* pszFilename, int nXSize, int nYSize, int nBands,
                                            GDALDataType eType, char **papszOptions)
{
   KmlSuperOverlayDataset *poDS;
   
   poDS = new KmlSuperOverlayDataset();
   poDS->eAccess = GA_Update;

   return poDS;
}

GDALDataset *KmlSuperOverlayDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                                                 int bStrict, char ** papszOptions, GDALProgressFunc pfnProgress, void * pProgressData)
{
   if (poSrcDS == NULL)
   {
      CPLError( CE_Failure, CPLE_FileIO,
                "Unable to create dataset.." );
      return NULL;
   }
   
   bool isKmz = false;
   
   //correct the file and get the directory
   const char* output_dir = NULL;
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
    
      output_dir = CPLGetPath(pszFilename);
      if (strcmp(output_dir, "") == 0)
      {
         output_dir = CPLGetCurrentDir();
      }
   }
   std::string outDir = output_dir;

   GDALDriver* poOutputTileDriver = NULL;
   const char* poOutputTileDriverName = "JPEG";
   bool isJpegDriver = true;

   if (papszOptions != NULL)
   {
      for(int i = 0; papszOptions[i] != NULL; i++)
      {
         std::string tmpFormatName = papszOptions[i];
         size_t charIndex = tmpFormatName.find("=");
         if (charIndex != std::string::npos)
         {
            std::string tmpFormat = tmpFormatName.substr(charIndex+1);
            std::string keyString = tmpFormatName.substr(0, charIndex);
            if (strcmp(keyString.c_str(), "format") == 0 || strcmp(keyString.c_str(), "FORMAT") == 0)
            {
               poOutputTileDriverName = CPLStrdup(tmpFormat.c_str());
               if (strcmp(poOutputTileDriverName, "png") == 0 || strcmp(poOutputTileDriverName, "PNG") == 0)
               {
                  isJpegDriver = false;
               }
               break;
            }
         }
      }
   }

   GDALDriver* poMemDriver = GetGDALDriverManager()->GetDriverByName("MEM");
   poOutputTileDriver = GetGDALDriverManager()->GetDriverByName(poOutputTileDriverName);

   if( poMemDriver == NULL || poOutputTileDriver == NULL)
   {
      CPLError( CE_Failure, CPLE_None,
                "Image export driver was not found.." );
      return NULL;
   }

   int bands = poSrcDS->GetRasterCount();
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
     
      char* projStr = strdup(poSrcDS->GetProjectionRef());
     
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
      // Note freeing projStr causes core dump.  CPLFree( projStr ); 
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

   std::vector<double> zoomxpixels;
   std::vector<double> zoomypixels;
   for (int zoom = 0; zoom < maxzoom + 1; zoom++)
   {
      zoomxpixels.push_back(adfGeoTransform[1] * pow(2.0, (maxzoom - zoom)));
      // zoomypixels.push_back(abs(adfGeoTransform[5]) * pow(2.0, (maxzoom - zoom)));
      zoomypixels.push_back(fabs(adfGeoTransform[5]) * pow(2.0, (maxzoom - zoom)));
   }

   std::string tmpFileName; 
   std::vector<std::string> dirVector;
   std::vector<std::string> fileVector;
   if (isKmz)
   {
      tmpFileName = outDir + "/" + "tmp.kml";
      GenerateRootKml(tmpFileName.c_str(), pszFilename, north, south, east, west, (int)tilexsize);
      fileVector.push_back(tmpFileName);
   }
   else
   {
      GenerateRootKml(pszFilename, pszFilename, north, south, east, west, (int)tilexsize);
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

            std::string zoomDir = outDir + "/" + zoomStr.str();
            VSIMkdir(zoomDir.c_str(), 0775);
        

            zoomDir = zoomDir + "/" + ixStr.str();
            VSIMkdir(zoomDir.c_str(), 0775);

            if (isKmz)
            {
               dirVector.push_back(zoomDir);
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

      //remove sub-directories and the files under those directories
      for (int i = 0; i < static_cast<int>(dirVector.size()); i++)
      {
         std::string zoomDir = dirVector[i];

         char** fileList = VSIReadDir(zoomDir.c_str());

         for( int j = 0; fileList && fileList[j]; ++j )
         {
            char* fi = fileList[j];
            std::string fn = zoomDir;
            if (fi)
            {
               std::string s1 = (const char*)fi;
               fn += "/";
               fn += s1;
            }
            VSIUnlink(fn.c_str());
         }
         CSLDestroy(fileList);
      
         VSIRmdir(zoomDir.c_str());
      }

      //remove the top directories
      for (int zoom = maxzoom; zoom >= 0; --zoom)
      {
         std::stringstream zoomStr;
         zoomStr << zoom;
         std::string zoomDir = outDir + "/" + zoomStr.str();
         VSIRmdir(zoomDir.c_str());
      }
      // VSIUnlink(tmpFileName.c_str());

      if (zipDone == false)
      {
         return NULL;
      }
   }

   if (output_dir != NULL)
   {
      delete[] output_dir;
      output_dir = NULL;
   }

   KmlSuperOverlayDataset *poDsDummy = new KmlSuperOverlayDataset();
   return poDsDummy;
}

GDALDataset *KmlSuperOverlayDataset::Open(GDALOpenInfo *)

{
   return NULL;
}

/************************************************************************/
/*                       KmlSuperOverlayDatasetDelete()                             */
/************************************************************************/

static CPLErr KmlSuperOverlayDatasetDelete(const char* fileName)
{
   /* Null implementation, so that people can Delete("MEM:::") */
   return CE_None;
}

/************************************************************************/
/*                          GDALRegister_KMLSUPEROVERLAY()                          */
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
      
      poDriver->pfnOpen = KmlSuperOverlayDataset::Open;
      poDriver->pfnCreate = KmlSuperOverlayDataset::Create;
      poDriver->pfnCreateCopy = KmlSuperOverlayDataset::CreateCopy;
      poDriver->pfnDelete = KmlSuperOverlayDatasetDelete;
      
      GetGDALDriverManager()->RegisterDriver( poDriver );
   }
}

