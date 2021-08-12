/******************************************************************************
 *
 * Project:  MSG Driver
 * Purpose:  GDALDataset driver for MSG translator for read support.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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
 ******************************************************************************/
#include "cpl_port.h"  // Must be first.

#include "gdal_frmts.h"
#include "msgdataset.h"
#include "prologue.h"
#include "xritheaderparser.h"
#include "reflectancecalculator.h"

#include "PublicDecompWT_headers.h"

#include <memory>
#include <vector>

#if _MSC_VER > 1000
#include  <io.h>
#else
#include <stdio.h>
#endif

CPL_CVSID("$Id$")

const double MSGDataset::rCentralWvl[12] = {0.635, 0.810, 1.640, 3.900, 6.250, 7.350, 8.701, 9.660, 10.800, 12.000, 13.400, 0.750};
const double MSGDataset::rVc[12] = {-1, -1, -1, 2569.094, 1598.566, 1362.142, 1149.083, 1034.345, 930.659, 839.661, 752.381, -1};
const double MSGDataset::rA[12] = {-1, -1, -1, 0.9959, 0.9963, 0.9991, 0.9996, 0.9999, 0.9983, 0.9988, 0.9981, -1};
const double MSGDataset::rB[12] = {-1, -1, -1, 3.471, 2.219, 0.485, 0.181, 0.060, 0.627, 0.397, 0.576, -1};
const int MSGDataset::iCentralPixelVIS_IR = 1856; // center pixel VIS and IR
const int MSGDataset::iCentralPixelHRV = 5566; // center pixel HRV
int MSGDataset::iCurrentSatelliteHint = 1;     // satellite number hint 1,2,3,4 for MSG1, MSG2, MSG3 and MSG4
const char *MSGDataset::metadataDomain = "msg"; // the metadata domain

#define MAX_SATELLITES 4

/************************************************************************/
/*                    MSGDataset()                                     */
/************************************************************************/

MSGDataset::MSGDataset()

{
  pszProjection = CPLStrdup("");
  adfGeoTransform[0] = 0.0;
  adfGeoTransform[1] = 1.0;
  adfGeoTransform[2] = 0.0;
  adfGeoTransform[3] = 0.0;
  adfGeoTransform[4] = 0.0;
  adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                    ~MSGDataset()                                     */
/************************************************************************/

MSGDataset::~MSGDataset()

{
  delete poTransform;

  CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *MSGDataset::_GetProjectionRef()

{
  return pszProjection;
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr MSGDataset::_SetProjection( const char * pszNewProjection )
{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MSGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                       Open()                                         */
/************************************************************************/

GDALDataset *MSGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*     Does this look like a MSG file                                   */
/* -------------------------------------------------------------------- */
    //if( poOpenInfo->fp == nullptr)
    //  return nullptr;
    // Do not touch the fp .. it will close by itself if not null after we return (whether it is recognized as HRIT or not)

    std::string command_line (poOpenInfo->pszFilename);

    MSGCommand command;
    std::string sErr = command.parse(command_line);
    if (sErr.length() > 0)
    {
        if (sErr.compare("-") != 0) // this driver does not recognize this format .. be silent and return false so that another driver can try
          CPLError( CE_Failure, CPLE_AppDefined, "%s", (sErr+"\n").c_str() );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Read the prologue.                                              */
/* -------------------------------------------------------------------- */
    Prologue pp;

    int iCurrentSatellite = iCurrentSatelliteHint; // Start with the hint. It is nice to have but don't rely on it...

    std::string sPrologueFileName = command.sPrologueFileName(iCurrentSatellite, 1);
    bool fPrologueExists = (access(sPrologueFileName.c_str(), 0) == 0);

    // Make sure we're testing for MSG1,2,3 or 4 exactly once, start with the hint which is the most recently used.
    int iTries = 1;
    while (!fPrologueExists && (iTries < MAX_SATELLITES))
    {
      iCurrentSatellite = 1 + iCurrentSatellite % MAX_SATELLITES;
      sPrologueFileName = command.sPrologueFileName(iCurrentSatellite, 1);
      fPrologueExists = (access(sPrologueFileName.c_str(), 0) == 0);
      ++iTries;
    }

    if (fPrologueExists)
    {
      iCurrentSatelliteHint = iCurrentSatellite; // set the hint
      std::ifstream p_file(sPrologueFileName.c_str(), std::ios::in | std::ios::binary);
      XRITHeaderParser xhp(p_file);
      if (xhp.isValid() && xhp.isPrologue())
        pp.read(p_file);
      p_file.close();
    }
    else
    {
        std::string l_sErr = "The prologue of the data set could not be found at the location specified:\n" + sPrologueFileName + "\n";
        CPLError( CE_Failure, CPLE_AppDefined, "%s",
                  l_sErr.c_str() );
        return nullptr;
    }

// We're confident the string is formatted as an MSG command_line

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    MSGDataset *poDS = new MSGDataset();
    poDS->command = command; // copy it

    /* -------------------------------------------------------------------- */
    /*      Set the current satellite for this DS                           */
    /* -------------------------------------------------------------------- */

    poDS->iCurrentSatellite = iCurrentSatellite; // copy it

    /* -------------------------------------------------------------------- */
    /*      Capture raster size from MSG prologue and submit it to GDAL     */
    /* -------------------------------------------------------------------- */

    if (command.channel[11] != 0) // the HRV band
    {
      poDS->nRasterXSize = pp.idr()->ReferenceGridHRV->NumberOfColumns;
      poDS->nRasterYSize = abs(pp.idr()->PlannedCoverageHRV->UpperNorthLinePlanned - pp.idr()->PlannedCoverageHRV->LowerSouthLinePlanned) + 1;
    }
    else
    {
      poDS->nRasterXSize = abs(pp.idr()->PlannedCoverageVIS_IR->WesternColumnPlanned - pp.idr()->PlannedCoverageVIS_IR->EasternColumnPlanned) + 1;
      poDS->nRasterYSize = abs(pp.idr()->PlannedCoverageVIS_IR->NorthernLinePlanned - pp.idr()->PlannedCoverageVIS_IR->SouthernLinePlanned) + 1;
    }

/* -------------------------------------------------------------------- */
/*      Set Georeference Information                                    */
/* -------------------------------------------------------------------- */

    double rPixelSizeX;
    double rPixelSizeY;
    double rMinX;
    double rMaxY;

    if (command.channel[11] != 0)
    {
      rPixelSizeX = 1000 * pp.idr()->ReferenceGridHRV->ColumnDirGridStep;
      rPixelSizeY = 1000 * pp.idr()->ReferenceGridHRV->LineDirGridStep;
      // The MSG Level 1.5 Image Data Format Description (page 22f) defines the center of pixel (5566,5566) from SE as sub satellite point for the HRV channel (0°,0° for operational MSG).
      rMinX = -rPixelSizeX * (pp.idr()->ReferenceGridHRV->NumberOfColumns - iCentralPixelHRV + 0.5);
      rMaxY = rPixelSizeY * (pp.idr()->ReferenceGridHRV->NumberOfLines - iCentralPixelHRV + 0.5); //scan direction south -> north
    }
    else
    {
      rPixelSizeX = 1000 * pp.idr()->ReferenceGridVIS_IR->ColumnDirGridStep;
      rPixelSizeY = 1000 * pp.idr()->ReferenceGridVIS_IR->LineDirGridStep;
      // The MSG Level 1.5 Image Data Format Description (page 22f) defines the center of pixel (1856,1856) from SE as sub satellite point for the VIS_IR channels (0°,0° for operational MSG).
      rMinX = -rPixelSizeX * (pp.idr()->ReferenceGridVIS_IR->NumberOfColumns  - iCentralPixelVIS_IR + 0.5);
      rMaxY = rPixelSizeY * (pp.idr()->ReferenceGridVIS_IR->NumberOfLines - iCentralPixelVIS_IR + 0.5); // The y scan direction is always south -> north
    }
    poDS->adfGeoTransform[0] = rMinX;
    poDS->adfGeoTransform[3] = rMaxY;
    poDS->adfGeoTransform[1] = rPixelSizeX;
    poDS->adfGeoTransform[5] = -rPixelSizeY;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;

/* -------------------------------------------------------------------- */
/*      Set Projection Information                                      */
/* -------------------------------------------------------------------- */

    poDS->oSRS.SetGEOS(  0, 35785831, 0, 0 );
    poDS->oSRS.SetWellKnownGeogCS( "WGS84" ); // Temporary line to satisfy ERDAS (otherwise the ellips is "unnamed"). Eventually this should become the custom a and b ellips (CGMS).
    CPLFree( poDS->pszProjection );
    poDS->oSRS.exportToWkt( &(poDS->pszProjection) );

    // The following are 3 different try-outs for also setting the ellips a and b parameters.
    // We leave them out for now however because this does not work. In gdalwarp, when choosing some
    // specific target SRS, the result is an error message:
    //
    // ERROR 1: geocentric transformation missing z or ellps
    // ERROR 1: GDALWarperOperation::ComputeSourceWindow() failed because
    // the pfnTransformer failed.
    //
    // I can't explain the reason for the message at this time (could be a problem in the way the SRS is set here,
    // but also a bug in Proj.4 or GDAL.
    /*
    oSRS.SetGeogCS( NULL, NULL, NULL, 6378169, 295.488065897, NULL, 0, NULL, 0 );

    oSRS.SetGeogCS( "unnamed ellipse", "unknown", "unnamed", 6378169, 295.488065897, "Greenwich", 0.0);

    if( oSRS.importFromProj4("+proj=geos +h=35785831 +a=6378169 +b=6356583.8") == OGRERR_NONE )
    {
        oSRS.exportToWkt( &(poDS->pszProjection) );
    }
    */

/* -------------------------------------------------------------------- */
/*   Create a transformer to LatLon (only for Reflectance calculation)  */
/* -------------------------------------------------------------------- */

    OGRSpatialReference* poSRSLongLat = poDS->oSRS.CloneGeogCS();
    if( poSRSLongLat )
    {
        poSRSLongLat->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poDS->poTransform = OGRCreateCoordinateTransformation( &(poDS->oSRS),poSRSLongLat );
        delete poSRSLongLat;
    }
/* -------------------------------------------------------------------- */
/*      Set the radiometric calibration parameters.                     */
/* -------------------------------------------------------------------- */

    memcpy( poDS->rCalibrationOffset,  pp.rpr()->Cal_Offset, sizeof(double) * 12 );
    memcpy( poDS->rCalibrationSlope,  pp.rpr()->Cal_Slope, sizeof(double) * 12 );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = command.iNrChannels()*command.iNrCycles;
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new MSGRasterBand( poDS, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The MSG driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*                 Set DataSet metadata information                    */
/* -------------------------------------------------------------------- */
    CPLString metadataValue;
    metadataValue.Printf("%d", poDS->iCurrentSatellite);
    poDS->SetMetadataItem("satellite_number", metadataValue.c_str(), metadataDomain);

    return poDS;
}

/************************************************************************/
/*                         MSGRasterBand()                              */
/************************************************************************/

const double MSGRasterBand::rRTOA[12] = {20.76, 23.24, 19.85, -1, -1, -1, -1, -1, -1, -1, -1, 25.11};

MSGRasterBand::MSGRasterBand( MSGDataset *poDSIn, int nBandIn )
: fScanNorth(false)
, iLowerShift(0)
, iSplitLine(0)
, iLowerWestColumnPlanned(0)

{
    this->poDS = poDSIn;
    this->nBand = nBandIn;

    // Find if we're dealing with MSG1, MSG2, MSG3 or MSG4
    // Doing this per band is the only way to guarantee time-series when the satellite is changed

    int iCurrentSatellite = poDSIn->iCurrentSatellite; // Start with the satellite selected by the dataset

    std::string sPrologueFileName = poDSIn->command.sPrologueFileName(iCurrentSatellite, 1);
    bool fPrologueExists = (access(sPrologueFileName.c_str(), 0) == 0);

    // Make sure we're testing for MSG1,2,3 or 4 exactly once, start with the one of the dataset
    int iTries = 1;
    while (!fPrologueExists && (iTries < MAX_SATELLITES))
    {
      iCurrentSatellite = 1 + iCurrentSatellite % MAX_SATELLITES;
      sPrologueFileName = poDSIn->command.sPrologueFileName(iCurrentSatellite, 1);
      fPrologueExists = (access(sPrologueFileName.c_str(), 0) == 0);
      ++iTries;
    }

    iSatellite = iCurrentSatellite; // From here on, the satellite that corresponds to this band is settled to the current satellite

    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = poDSIn->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Open an input file and capture the header for the nr. of bits.  */
/* -------------------------------------------------------------------- */
    int iStrip = 1;
    int iChannel = poDSIn->command.iChannel(1 + ((nBand - 1) % poDSIn->command.iNrChannels()));
    std::string input_file = poDSIn->command.sFileName(iSatellite, nBand, iStrip);
    while ((access(input_file.c_str(), 0) != 0) && (iStrip <= poDSIn->command.iNrStrips(iChannel))) // compensate for missing strips
      input_file = poDSIn->command.sFileName(iSatellite, nBand, ++iStrip);

    if (iStrip <= poDSIn->command.iNrStrips(iChannel))
    {
      std::ifstream i_file (input_file.c_str(), std::ios::in|std::ios::binary);

      if (i_file.good())
      {
        XRITHeaderParser xhp (i_file);

        if (xhp.isValid())
        {
          // Data type is either 8 or 16 bits .. we tell this to GDAL here
          eDataType = GDT_Byte; // default .. always works
          if (xhp.nrBitsPerPixel() > 8)
          {
            if (poDSIn->command.cDataConversion == 'N')
              eDataType = GDT_UInt16; // normal case: MSG 10 bits data
            else if (poDSIn->command.cDataConversion == 'B')
              eDataType = GDT_Byte; // output data type Byte
            else
              eDataType = GDT_Float32; // Radiometric calibration
          }

          // make IReadBlock be called once per file
          nBlockYSize = xhp.nrRows();

          // remember the scan direction

          fScanNorth = xhp.isScannedNorth();
        }
      }

      i_file.close();
    }
    else if (nBand > 1)
    {
      // missing entire band .. take data from first band
      MSGRasterBand* pFirstRasterBand = (MSGRasterBand*)poDSIn->GetRasterBand(1);
      eDataType = pFirstRasterBand->eDataType;
      nBlockYSize = pFirstRasterBand->nBlockYSize;
      fScanNorth = pFirstRasterBand->fScanNorth;
    }
    else // also first band is missing .. do something for fail-safety
    {
      if (poDSIn->command.cDataConversion == 'N')
        eDataType = GDT_UInt16; // normal case: MSG 10 bits data
      else if (poDSIn->command.cDataConversion == 'B')
        eDataType = GDT_Byte; // output data type Byte
      else
        eDataType = GDT_Float32; // Radiometric calibration

      // nBlockYSize : default
      // fScanNorth : default
    }
/* -------------------------------------------------------------------- */
/*      For the HRV band, read the prologue for shift and splitline.    */
/* -------------------------------------------------------------------- */

    if (iChannel == 12)
    {
      if (fPrologueExists)
      {
        std::ifstream p_file(sPrologueFileName.c_str(), std::ios::in|std::ios::binary);
        XRITHeaderParser xhp(p_file);
        Prologue pp;
        if (xhp.isValid() && xhp.isPrologue())
          pp.read(p_file);
        p_file.close();

        iLowerShift = pp.idr()->PlannedCoverageHRV->UpperWestColumnPlanned - pp.idr()->PlannedCoverageHRV->LowerWestColumnPlanned;
        iSplitLine = abs(pp.idr()->PlannedCoverageHRV->UpperNorthLinePlanned - pp.idr()->PlannedCoverageHRV->LowerNorthLinePlanned) + 1; // without the "+ 1" the image of 1-Jan-2005 splits incorrectly
        iLowerWestColumnPlanned = pp.idr()->PlannedCoverageHRV->LowerWestColumnPlanned;
      }
    }

/* -------------------------------------------------------------------- */
/*  Initialize the ReflectanceCalculator with the band-dependent info.  */
/* -------------------------------------------------------------------- */

    int iCycle = 1 + (nBand - 1) / poDSIn->command.iNrChannels();
    std::string sTimeStamp = poDSIn->command.sCycle(iCycle);

    m_rc = new ReflectanceCalculator(sTimeStamp, rRTOA[iChannel-1]);

/* -------------------------------------------------------------------- */
/*  Set DataSet metadata information                                   */
/* -------------------------------------------------------------------- */
    CPLString metadataValue;
    metadataValue.Printf("%.10f", poDSIn->rCalibrationOffset[iChannel - 1]);
    SetMetadataItem("calibration_offset", metadataValue.c_str(), poDSIn->metadataDomain);
    metadataValue.Printf("%.10f", poDSIn->rCalibrationSlope[iChannel - 1]);
    SetMetadataItem("calibration_slope", metadataValue.c_str(), poDSIn->metadataDomain);
    metadataValue.Printf("%d", iChannel);
    SetMetadataItem("channel_number", metadataValue.c_str(), poDSIn->metadataDomain);
}

/************************************************************************/
/*                           ~MSGRasterBand()                           */
/************************************************************************/
MSGRasterBand::~MSGRasterBand()
{
    delete m_rc;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr MSGRasterBand::IReadBlock( int /*nBlockXOff*/, int nBlockYOff,
                                  void * pImage )

{

    MSGDataset  *poGDS = (MSGDataset *) poDS;

    int iBytesPerPixel = 1;
    if (eDataType == GDT_UInt16)
      iBytesPerPixel = 2;
    else if (eDataType == GDT_Float32)
      iBytesPerPixel = 4;
/* -------------------------------------------------------------------- */
/*      Calculate the correct input file name based on nBlockYOff       */
/* -------------------------------------------------------------------- */

    int strip_number;
    int iChannel = poGDS->command.iChannel(1 + ((nBand - 1) % poGDS->command.iNrChannels()));

    if (fScanNorth)
      strip_number = nBlockYOff + 1;
    else
      strip_number = poGDS->command.iNrStrips(iChannel) - nBlockYOff;

    std::string strip_input_file = poGDS->command.sFileName(iSatellite, nBand, strip_number);

/* -------------------------------------------------------------------- */
/*      Open the input file                                             */
/* -------------------------------------------------------------------- */
    if (access(strip_input_file.c_str(), 0) == 0) // does it exist?
    {
      std::ifstream i_file (strip_input_file.c_str(), std::ios::in|std::ios::binary);

      if (i_file.good())
      {
        XRITHeaderParser xhp (i_file);

        if (xhp.isValid())
        {
          std::vector <short> QualityInfo;
          unsigned short chunk_height = xhp.nrRows();
          unsigned short chunk_bpp = xhp.nrBitsPerPixel();
          unsigned short chunk_width = xhp.nrColumns();
          unsigned __int8 NR = (unsigned __int8)chunk_bpp;
          unsigned int nb_ibytes = static_cast<unsigned int>(xhp.dataSize());
          int iShift = 0;
          bool fSplitStrip = false; // in the split strip the "shift" only happens before the split "row"
          int iSplitRow = 0;
          if (iChannel == 12)
          {
            iSplitRow = iSplitLine % xhp.nrRows();
            int iSplitBlock = iSplitLine / xhp.nrRows();
            fSplitStrip = (nBlockYOff == iSplitBlock); // in the split strip the "shift" only happens before the split "row"

            // When iLowerShift > 0, the lower HRV image is shifted to the right
            // When iLowerShift < 0, the lower HRV image is shifted to the left
            // The available raster may be wider than needed, so that time series don't fall outside the raster.

            if (nBlockYOff <= iSplitBlock)
              iShift = -iLowerShift;
            // iShift < 0 means upper image moves to the left
            // iShift > 0 means upper image moves to the right
          }

          unsigned char* ibuf = new (std::nothrow) unsigned char[nb_ibytes];
          if (ibuf == nullptr )
          {
             CPLError( CE_Failure, CPLE_AppDefined,
                  "Not enough memory to perform wavelet decompression\n");
            return CE_Failure;
          }

          i_file.read( (char *)ibuf, nb_ibytes);

          Util::CDataFieldCompressedImage  img_compressed(ibuf,
                                  nb_ibytes*8,
                                  (unsigned char)chunk_bpp,
                                  chunk_width,
                                  chunk_height      );

          Util::CDataFieldUncompressedImage img_uncompressed;

          //****************************************************
          //*** Here comes the wavelets decompression routine
          COMP::DecompressWT(img_compressed, NR, img_uncompressed, QualityInfo);
          //****************************************************

          // convert:
          // Depth:
          // 8 bits -> 8 bits
          // 10 bits -> 16 bits (img_uncompressed contains the 10 bits data in packed form)
          // Geometry:
          // chunk_width x chunk_height to nBlockXSize x nBlockYSize

          // cases:
          // combination of the following:
          // - scan direction can be north or south
          // - eDataType can be GDT_Byte, GDT_UInt16 or GDT_Float32
          // - nBlockXSize == chunk_width or nBlockXSize > chunk_width
          // - when nBlockXSize > chunk_width, fSplitStrip can be true or false
          // we won't distinguish the following cases:
          // - NR can be == 8 or != 8
          // - when nBlockXSize > chunk_width, iShift iMinCOff-iMaxCOff <= iShift <= 0

          int nBlockSize = nBlockXSize * nBlockYSize;
          int y = chunk_width * chunk_height;
          int iStep = -1;
          if (fScanNorth) // image is the other way around
          {
            y = -1; // See how y is used below: += happens first, the result is used in the []
            iStep = 1;
          }

          COMP::CImage cimg (img_uncompressed); // unpack
          if (eDataType == GDT_Byte)
          {
            if (nBlockXSize == chunk_width) // optimized version
            {
              if (poGDS->command.cDataConversion == 'B')
              {
                for( int i = 0; i < nBlockSize; ++i )
                    ((GByte *)pImage)[i] = cimg.Get()[y+=iStep] / 4;
              }
              else
              {
                for( int i = 0; i < nBlockSize; ++i )
                    ((GByte *)pImage)[i] = cimg.Get()[y+=iStep];
              }
            }
            else
            {
              // initialize to 0's (so that it does not have to be done in an 'else' statement <performance>)
              memset(pImage, 0, nBlockXSize * nBlockYSize * iBytesPerPixel);
              if (poGDS->command.cDataConversion == 'B')
              {
                for( int j = 0; j < chunk_height; ++j ) // assumption: nBlockYSize == chunk_height
                {
                  int iXOffset = j * nBlockXSize + iShift;
                  iXOffset += nBlockXSize - iLowerWestColumnPlanned - 1; // Position the HRV part in the frame; -1 to compensate the pre-increment in the for-loop
                  if (fSplitStrip && (j >= iSplitRow)) // In splitstrip, below splitline, thus do not shift!!
                    iXOffset -= iShift;
                  for (int i = 0; i < chunk_width; ++i)
                    ((GByte *)pImage)[++iXOffset] = cimg.Get()[y+=iStep] / 4;
                }
              }
              else
              {
                for( int j = 0; j < chunk_height; ++j ) // assumption: nBlockYSize == chunk_height
                {
                  int iXOffset = j * nBlockXSize + iShift;
                  iXOffset += nBlockXSize - iLowerWestColumnPlanned - 1; // Position the HRV part in the frame; -1 to compensate the pre-increment in the for-loop
                  if (fSplitStrip && (j >= iSplitRow)) // In splitstrip, below splitline, thus do not shift!!
                    iXOffset -= iShift;
                  for (int i = 0; i < chunk_width; ++i)
                    ((GByte *)pImage)[++iXOffset] = cimg.Get()[y+=iStep];
                }
              }
            }
          }
          else if (eDataType == GDT_UInt16) // this is our "normal case" if scan direction is South: 10 bit MSG data became 2 bytes per pixel
          {
            if (nBlockXSize == chunk_width) // optimized version
            {
              for( int i = 0; i < nBlockSize; ++i )
                  ((GUInt16 *)pImage)[i] = cimg.Get()[y+=iStep];
            }
            else
            {
              // initialize to 0's (so that it does not have to be done in an 'else' statement <performance>)
              memset(pImage, 0, nBlockXSize * nBlockYSize * iBytesPerPixel);
              for( int j = 0; j < chunk_height; ++j ) // assumption: nBlockYSize == chunk_height
              {
                int iXOffset = j * nBlockXSize + iShift;
                iXOffset += nBlockXSize - iLowerWestColumnPlanned - 1; // Position the HRV part in the frame; -1 to compensate the pre-increment in the for-loop
                if (fSplitStrip && (j >= iSplitRow)) // In splitstrip, below splitline, thus do not shift!!
                  iXOffset -= iShift;
                for (int i = 0; i < chunk_width; ++i)
                  ((GUInt16 *)pImage)[++iXOffset] = cimg.Get()[y+=iStep];
              }
            }
          }
          else if (eDataType == GDT_Float32) // radiometric calibration is requested
          {
            if (nBlockXSize == chunk_width) // optimized version
            {
              for( int i = 0; i < nBlockSize; ++i )
                ((float *)pImage)[i] = (float)rRadiometricCorrection(cimg.Get()[y+=iStep], iChannel, nBlockYOff * nBlockYSize + i / nBlockXSize, i % nBlockXSize, poGDS);
            }
            else
            {
              // initialize to 0's (so that it does not have to be done in an 'else' statement <performance>)
              memset(pImage, 0, nBlockXSize * nBlockYSize * iBytesPerPixel);
              for( int j = 0; j < chunk_height; ++j ) // assumption: nBlockYSize == chunk_height
              {
                int iXOffset = j * nBlockXSize + iShift;
                iXOffset += nBlockXSize - iLowerWestColumnPlanned - 1; // Position the HRV part in the frame; -1 to compensate the pre-increment in the for-loop
                if (fSplitStrip && (j >= iSplitRow)) // In splitstrip, below splitline, thus do not shift!!
                  iXOffset -= iShift;
                int iXFrom = nBlockXSize - iLowerWestColumnPlanned + iShift; // i is used as the iCol parameter in rRadiometricCorrection
                int iXTo = nBlockXSize - iLowerWestColumnPlanned + chunk_width + iShift;
                for (int i = iXFrom; i < iXTo; ++i) // range always equal to chunk_width .. this is to utilize i to get iCol
                  ((float *)pImage)[++iXOffset] = (float)rRadiometricCorrection(cimg.Get()[y+=iStep], iChannel, nBlockYOff * nBlockYSize + j, (fSplitStrip && (j >= iSplitRow))?(i - iShift):i, poGDS);
              }
            }
          }
        }
        else // header could not be opened .. make sure block contains 0's
          memset(pImage, 0, nBlockXSize * nBlockYSize * iBytesPerPixel);
      }
      else // file could not be opened .. make sure block contains 0's
        memset(pImage, 0, nBlockXSize * nBlockYSize * iBytesPerPixel);

      i_file.close();
    }
    else // file does not exist .. make sure block contains 0's
      memset(pImage, 0, nBlockXSize * nBlockYSize * iBytesPerPixel);

    return CE_None;
}

double MSGRasterBand::rRadiometricCorrection(unsigned int iDN, int iChannel, int iRow, int iCol, MSGDataset* poGDS) const
{
  int iIndex = iChannel - 1; // just for speed optimization

  double rSlope = poGDS->rCalibrationSlope[iIndex];
  double rOffset = poGDS->rCalibrationOffset[iIndex];

  // Reflectance for visual bands, temperature for IR bands.
  if (poGDS->command.cDataConversion == 'T')
  {
    double rRadiance = rOffset + (iDN * rSlope);

                if (iChannel >= 4 && iChannel <= 11) // Channels 4 to 11 (infrared): Temperature
                {
                        const double rC1 = 1.19104e-5;
                        const double rC2 = 1.43877e+0;

                        double cc2 = rC2 * poGDS->rVc[iIndex];
                        double cc1 = rC1 * pow(poGDS->rVc[iIndex], 3) / rRadiance;
                        // cppcheck suggests using log1p() but not sure how portable this would be
                        // cppcheck-suppress unpreciseMathCall
                        double rTemperature = ((cc2 / log(cc1 + 1)) - poGDS->rB[iIndex]) / poGDS->rA[iIndex];
                        return rTemperature;
                }
                else // Channels 1,2,3 and 12 (visual): Reflectance
                {
      double rLon = poGDS->adfGeoTransform[0] + iCol * poGDS->adfGeoTransform[1]; // X, in "geos" meters
      double rLat = poGDS->adfGeoTransform[3] + iRow * poGDS->adfGeoTransform[5]; // Y, in "geos" meters
      if ((poGDS->poTransform != nullptr) && poGDS->poTransform->Transform( 1, &rLon, &rLat )) // transform it to latlon
              return m_rc->rGetReflectance(rRadiance, rLat, rLon);
                        else
                                return 0;
                }
  }
  else // radiometric
  {
    if (poGDS->command.cDataConversion == 'R')
      return rOffset + (iDN * rSlope);
    else
    {
      double rFactor = 10 / pow(poGDS->rCentralWvl[iIndex], 2);
      return rFactor * (rOffset + (iDN * rSlope));
    }
  }
}

/************************************************************************/
/*                      GDALRegister_MSG()                              */
/************************************************************************/

void GDALRegister_MSG()

{
    if( GDALGetDriverByName( "MSG" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MSG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,  "MSG HRIT Data" );

    poDriver->pfnOpen = MSGDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
