/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation for NSIDC binary format.
 * Author:   Michael Sumner, mdsumner@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include <iostream>  // for cout testing

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>
#include <algorithm>

using std::fill;

CPL_CVSID("$Id$")

  /************************************************************************/
  /* ==================================================================== */
  /*                              NSIDCbinDataset                            */
  /* ==================================================================== */
  /************************************************************************/

  class NSIDCbinDataset final: public GDALPamDataset
  {
    struct NSIDCbinHeader{
      // derived from DIPEx
      // GInt32      NBIH = {0};   /* bytes in header, normally 1024 */
      // GInt32      NBPR = {0};   /* bytes per data record (all bands of scanline) */
      // GInt32      IL = {0};     /* initial line - normally 1 */
      // GInt32      LL = {0};     /* last line */
      // GInt32      IE = {0};     /* initial element (pixel), normally 1 */
      // GInt32      LE = {0};     /* last element (pixel) */
      // GInt32      NC = {0};     /* number of channels (bands) */
      // GInt32      H4322 = {0};  /* header record identifier - always 4322. */
      //char        unused1[40] = {0};
      // GByte       IH19[4] = {0};/* data type, and size flags */
      // GInt32      IH20 = {0};   /* number of secondary headers */
      // GInt32      SRID = {0};
      // char        unused2[12] = {0};
      // double      YOffset = {0};
      // double      XOffset = {0};
      // double      YPixSize = {0};
      // double      XPixSize = {0};
      // double      Matrix[4] = {0};
      // char        unused3[344] = {0};
      // GUInt16     ColorTable[256] = {0};  /* RGB packed with 4 bits each */
      // char        unused4[32] = {0};

      char missing_int[6] = {0};
      char columns[6] = {0};
      char rows[6] = {0};
      char internal1[6] = {0};
      char latitude[6] = {0};
      char greenwich[6] = {0};
      char internal2[6] = {0};
      char jpole[6] = {0};
      char ipole[6] = {0};
      char instrument[6] = {0};
      char descriptor[6] = {0};
      char julian_start[6] = {0};
      char hour_start[6] = {0};
      char minute_start[6] = {0};
      char julian_end[6] = {0};
      char hour_end[6] = {0};
      char minute_end[6] = {0};
      char year[6] = {0};
      char julian[6] = {0};
      char channel[6] = {0};
      char scaling[6] = {0};


      char filename[24] = {0};
      char opt_imagetitle[80] = {0};
      char information[70] = {0};
    };

    VSILFILE    *fp;
    CPLString    osSRS{};

    NSIDCbinHeader  sHeader{};

    GDALDataType eRasterDataType;

    double      adfGeoTransform[6];

    CPL_DISALLOW_COPY_ASSIGN(NSIDCbinDataset)

  public:
    NSIDCbinDataset();
    ~NSIDCbinDataset() override;

    CPLErr GetGeoTransform( double * ) override;

    const char *_GetProjectionRef( void ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
      return GetSpatialRefFromOldGetProjectionRef();
    }
    static GDALDataset *Open( GDALOpenInfo * );
  };

/************************************************************************/
/* ==================================================================== */
/*                             NSIDCbinDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            NSIDCbinDataset()                             */
/************************************************************************/

NSIDCbinDataset::NSIDCbinDataset() :
  fp(nullptr),
  eRasterDataType(GDT_Unknown)
{
  adfGeoTransform[0] = 0.0;
  adfGeoTransform[1] = 1.0;
  adfGeoTransform[2] = 0.0;
  adfGeoTransform[3] = 0.0;
  adfGeoTransform[4] = 0.0;
  adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~NSIDCbinDataset()                            */
/************************************************************************/

NSIDCbinDataset::~NSIDCbinDataset()

{
  if( fp )
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
  fp = nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NSIDCbinDataset::Open( GDALOpenInfo * poOpenInfo )

{
  /* -------------------------------------------------------------------- */
  /*      First we check to see if the file has the expected header       */
  /*      bytes.                                                          */
  /* -------------------------------------------------------------------- */
  std::cout << "here we are0\n";

  if( poOpenInfo->nHeaderBytes < 300 || poOpenInfo->fpL == nullptr )
    return nullptr;
  std::cout << "here we are1\n";

  // if( CPL_LSBWORD32(*( reinterpret_cast<GInt32 *>( poOpenInfo->pabyHeader + 0 )))
  //     != 300 )
  //     return nullptr;
  // std::cout << "here we are2\n";

  //
  //     if( CPL_LSBWORD32(*( reinterpret_cast<GInt32 *>( poOpenInfo->pabyHeader + 28 )))
  //         != 4322 )
  //         return nullptr;

  /* -------------------------------------------------------------------- */
  /*      Create a corresponding GDALDataset.                             */
  /* -------------------------------------------------------------------- */
  NSIDCbinDataset *poDS = new NSIDCbinDataset();

  poDS->eAccess = poOpenInfo->eAccess;
  poDS->fp = poOpenInfo->fpL;
  poOpenInfo->fpL = nullptr;

  /* -------------------------------------------------------------------- */
  /*      Read the header information.                                    */
  /* -------------------------------------------------------------------- */
  if( VSIFReadL( &(poDS->sHeader), 300, 1, poDS->fp ) != 1 )
  {
    CPLError( CE_Failure, CPLE_FileIO,
              "Attempt to read 300 byte header filed on file %s\n",
              poOpenInfo->pszFilename );
    delete poDS;
    return nullptr;
  }


  /* -------------------------------------------------------------------- */
  /*      Extract information of interest from the header.                */
  /* -------------------------------------------------------------------- */

  // how to convert columns/rows char to int?
  if (EQUAL(poDS->sHeader.columns + 2, "316")) {
    poDS->nRasterXSize = 316;
    poDS->nRasterYSize= 332;
  }
  if (EQUAL(poDS->sHeader.columns + 2, "304")) {
    poDS->nRasterXSize = 304;
    poDS->nRasterYSize= 448;
  }


  // north is 304x448, south is 316x332
  if (!((poDS->nRasterXSize == 316) || (poDS->nRasterXSize == 304) )) {

    delete poDS;
    return nullptr;
  }
  if (!((poDS->nRasterYSize == 332) || (poDS->nRasterYSize == 448) )) {
    delete poDS;
    return nullptr;
  }
  bool south = poDS->nRasterXSize == 316;

  const int nBands = 1; //CPL_LSBWORD32( poDS->sHeader.NC );


  if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
      !GDALCheckBandCount(nBands, FALSE) )
  {
    delete poDS;
    return nullptr;
  }

  const int nNSIDCbinDataType = 0; //(poDS->sHeader.IH19[1] & 0x7e) >> 2;
  const int nBytesPerSample = 1; //poDS->sHeader.IH19[0];

  if( nNSIDCbinDataType == 0 && nBytesPerSample == 1 )
    poDS->eRasterDataType = GDT_Byte;
  else if( nNSIDCbinDataType == 1 && nBytesPerSample == 1 )
    poDS->eRasterDataType = GDT_Byte;
  else if( nNSIDCbinDataType == 16 && nBytesPerSample == 4 )
    poDS->eRasterDataType = GDT_Float32;
  else if( nNSIDCbinDataType == 17 && nBytesPerSample == 8 )
    poDS->eRasterDataType = GDT_Float64;
  else
  {
    delete poDS;
    CPLError( CE_Failure, CPLE_AppDefined,
              "Unrecognized image data type %d, with BytesPerSample=%d.",
              nNSIDCbinDataType, nBytesPerSample );
    return nullptr;
  }

  // if( nLineOffset <= 0 || nLineOffset > INT_MAX / nBands )
  // {
  //     delete poDS;
  //     CPLError( CE_Failure, CPLE_AppDefined,
  //               "Invalid values: nLineOffset = %d, nBands = %d.",
  //               nLineOffset, nBands );
  //     return nullptr;
  // }

  /* -------------------------------------------------------------------- */
  /*      Create band information objects.                                */
  /* -------------------------------------------------------------------- */
  CPLErrorReset();
  for( int iBand = 0; iBand < nBands; iBand++ )
  {
    poDS->SetBand( iBand+1,
                   new RawRasterBand( poDS, iBand+1, poDS->fp,
                                      300 + iBand * poDS->nRasterXSize,
                                      nBytesPerSample,
                                      poDS->nRasterXSize * nBands,
                                      poDS->eRasterDataType,
                                      CPL_IS_LSB,
                                      RawRasterBand::OwnFP::NO ) );
    if( CPLGetLastErrorType() != CE_None )
    {
      delete poDS;
      return nullptr;
    }
  }


  if( south )
  {
    poDS->adfGeoTransform[0] = -3950000.0;
    poDS->adfGeoTransform[1] = 25000;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = 4350000.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -25000;
  }
  else
  {
    poDS->adfGeoTransform[0] = -3837500;
    poDS->adfGeoTransform[1] = 25000;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = 5837500;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -25000;
  }


  // south or north
  OGRSpatialReference oSR;
  int epsg = -1;
  if (south) {
    epsg = 3976;
  } else {
    epsg = 3413;
  }
  if( oSR.importFromEPSG( epsg ) == OGRERR_NONE ) {
    char *pszWKT = nullptr;
    oSR.exportToWkt( &pszWKT );
    poDS->osSRS = pszWKT;
    CPLFree( pszWKT );
  }

  /* -------------------------------------------------------------------- */
  /*      Initialize any PAM information.                                 */
  /* -------------------------------------------------------------------- */
  poDS->SetDescription( poOpenInfo->pszFilename );
  poDS->TryLoadXML();

  // /* -------------------------------------------------------------------- */
  // /*      Check for external overviews.                                   */
  // /* -------------------------------------------------------------------- */
  //     poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
  //                                  poOpenInfo->GetSiblingFiles() );

  return poDS;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NSIDCbinDataset::_GetProjectionRef()

{
  return osSRS.c_str();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NSIDCbinDataset::GetGeoTransform( double * padfTransform )

{
  memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

  return CE_None;
}

/************************************************************************/
/*                          GDALRegister_NSIDCbin()                        */
/************************************************************************/

void GDALRegister_NSIDCbin()

{
  if( GDALGetDriverByName( "NSIDCbin" ) != nullptr )
    return;

  GDALDriver *poDriver = new GDALDriver();

  poDriver->SetDescription( "NSIDCbin" );
  poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
  poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NSIDCbin" );
  poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

  poDriver->pfnOpen = NSIDCbinDataset::Open;

  GetGDALDriverManager()->RegisterDriver( poDriver );
}
