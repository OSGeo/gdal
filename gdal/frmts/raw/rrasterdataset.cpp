/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements R Raster Format.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "cpl_port.h"

#include "gdal_frmts.h"

#include "rawdataset.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                           RRASTERDataset                             */
/* ==================================================================== */
/************************************************************************/

class RRASTERDataset : public RawDataset
{
    CPLString   m_osGriFilename;
    double      m_adfGeoTransform[6];
    VSILFILE   *m_fpImage;
    CPLString   m_osProjection;

  public:
                RRASTERDataset();
       virtual ~RRASTERDataset();

    virtual char **GetFileList(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *GetProjectionRef(void) override;
};

/************************************************************************/
/* ==================================================================== */
/*                         RRASTERRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class RRASTERRasterBand: public RawRasterBand
{
      bool      m_bMinMaxValid;
      double    m_dfMin;
      double    m_dfMax;

  public:
      RRASTERRasterBand( GDALDataset *poDS, int nBand, void * fpRaw,
                    vsi_l_offset nImgOffset, int nPixelOffset,
                    int nLineOffset,
                    GDALDataType eDataType, int bNativeOrder );

      void SetMinMax( double dfMin, double dfMax );
      virtual double GetMinimum( int *pbSuccess = NULL ) override;
      virtual double GetMaximum(int *pbSuccess = NULL ) override;

#ifdef UPDATE_SUPPORTED
  protected:
      virtual CPLErr  IWriteBlock( int, int, void * );
      virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg );
#endif
};

/************************************************************************/
/*                           RRASTERDataset()                           */
/************************************************************************/

RRASTERRasterBand::RRASTERRasterBand( GDALDataset *poDSIn, int nBandIn,
                                      void * fpRawIn,
                                      vsi_l_offset nImgOffsetIn,
                                      int nPixelOffsetIn,
                                      int nLineOffsetIn,
                                      GDALDataType eDataTypeIn,
                                      int bNativeOrderIn ) :
    RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                  nLineOffsetIn, eDataTypeIn, bNativeOrderIn, TRUE),
    m_bMinMaxValid( false ),
    m_dfMin( 0.0 ),
    m_dfMax( 0.0 )
{
}

/************************************************************************/
/*                             SetMinMax()                              */
/************************************************************************/

void RRASTERRasterBand::SetMinMax( double dfMin, double dfMax )
{
    m_bMinMaxValid = true;
    m_dfMin = dfMin;
    m_dfMax = dfMax;
}

/************************************************************************/
/*                            GetMinimum()                              */
/************************************************************************/

double RRASTERRasterBand::GetMinimum( int *pbSuccess )
{
    if( m_bMinMaxValid )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfMin;
    }
    return RawRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                            GetMaximum()                              */
/************************************************************************/

double RRASTERRasterBand::GetMaximum(int *pbSuccess )
{
    if( m_bMinMaxValid )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfMax;
    }
    return RawRasterBand::GetMaximum(pbSuccess);
}

#ifdef UPDATE_SUPPORTED
/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RRASTERRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                       void * pImage )
{
    m_bMinMaxValid = false;
    return RawRasterBand::IWriteBlock(nBlockXOff, nBlockYOff, pImage);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr RRASTERRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                     int nXOff, int nYOff,
                                     int nXSize, int nYSize,
                                     void * pData, int nBufXSize, int nBufYSize,
                                     GDALDataType eBufType,
                                     GSpacing nPixelSpace, GSpacing nLineSpace,
                                     GDALRasterIOExtraArg* psExtraArg )

{
    if( eRWFlag == GF_Write )
        m_bMinMaxValid = false;
    return RawRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                     nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace, psExtraArg );
}
#endif

/************************************************************************/
/*                           RRASTERDataset()                           */
/************************************************************************/

RRASTERDataset::RRASTERDataset() :
    m_fpImage(NULL)
{
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                          ~RRASTERDataset()                           */
/************************************************************************/

RRASTERDataset::~RRASTERDataset()

{
    FlushCache();
    if( m_fpImage != NULL )
        VSIFCloseL(m_fpImage);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **RRASTERDataset::GetFileList()

{
    char **papszFileList = RawDataset::GetFileList();

    papszFileList = CSLAddString(papszFileList, m_osGriFilename);

    return papszFileList;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RRASTERDataset::GetGeoTransform( double * padfGeoTransform )
{
    memcpy( padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double) );
    return CE_None;
}

/************************************************************************/
/*                           GetProjectionRef()                         */
/************************************************************************/

const char * RRASTERDataset::GetProjectionRef()
{
    return m_osProjection.c_str();
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int RRASTERDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 40
        || poOpenInfo->fpL == NULL
        || !EQUAL( CPLGetExtension(poOpenInfo->pszFilename), "grd" )
        || strstr((const char *) poOpenInfo->pabyHeader, "ncols") == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "nrows") == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "xmin") == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "ymin") == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "xmax") == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "ymax") == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "datatype") == NULL )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RRASTERDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Update not supported");
        return NULL;
    }

    const char* pszLine = NULL;
    int nRows = 0;
    int nCols = 0;
    double dfXMin = 0.0;
    double dfYMin = 0.0;
    double dfXMax = 0.0;
    double dfYMax = 0.0;
    int l_nBands = 1;
    CPLString osDataType;
    CPLString osBandOrder;
    CPLString osProjection;
    CPLString osByteOrder;
    CPLString osNoDataValue("NA");
    CPLString osMinValue;
    CPLString osMaxValue;
    VSIRewindL(poOpenInfo->fpL);
    while( (pszLine = CPLReadLine2L(poOpenInfo->fpL, 1024, NULL)) != NULL )
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(pszLine, &pszKey);
        if( pszKey && pszValue )
        {
            if( EQUAL(pszKey, "ncols") )
                nCols = atoi(pszValue);
            else if( EQUAL(pszKey, "nrows") )
                nRows = atoi(pszValue);
            else if( EQUAL(pszKey, "xmin") )
                dfXMin = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "ymin") )
                dfYMin = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "xmax") )
                dfXMax = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "ymax") )
                dfYMax = CPLAtof(pszValue);
            else if( EQUAL(pszKey, "projection") )
                osProjection = pszValue;
            else if( EQUAL(pszKey, "nbands") )
                l_nBands = atoi(pszValue);
            else if( EQUAL(pszKey, "bandorder") )
                osBandOrder = pszValue;
            else if( EQUAL(pszKey, "datatype") )
                osDataType = pszValue;
            else if( EQUAL(pszKey, "byteorder") )
                osByteOrder = pszValue;
            else if( EQUAL(pszKey, "nodatavalue") )
                osNoDataValue = pszValue;
            else if( EQUAL(pszKey, "minvalue") )
                osMinValue = pszValue;
            else if( EQUAL(pszKey, "maxvalue") )
                osMaxValue = pszValue;
        }
        CPLFree(pszKey);
    }
    if( !GDALCheckDatasetDimensions(nCols, nRows) )
        return NULL;
    if( !GDALCheckBandCount(l_nBands, FALSE) )
        return NULL;

    GDALDataType eDT = GDT_Unknown;
    if( EQUAL(osDataType, "LOG1S") )
        eDT = GDT_Byte; // mapping TBC
    else if( EQUAL(osDataType, "INT1S") )
        eDT = GDT_Byte;
    else if( EQUAL(osDataType, "INT2S") )
        eDT = GDT_Int16;
    else if( EQUAL(osDataType, "INT4S") )
        eDT = GDT_Int32;
    else if( EQUAL(osDataType, "INT8S") )
        eDT = GDT_Float64; // Approximate matching
    else if( EQUAL(osDataType, "INT1U") )
        eDT = GDT_Byte;
    else if( EQUAL(osDataType, "INT2U") )
        eDT = GDT_UInt16;
    else if( EQUAL(osDataType, "INT4U") ) // Not documented
        eDT = GDT_UInt32;
    else if( EQUAL(osDataType, "FLT4S") )
        eDT = GDT_Float32;
    else if( EQUAL(osDataType, "FLT8S") )
        eDT = GDT_Float64;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled datatype=%s", osDataType.c_str() );
        return NULL;
    }
    if( l_nBands > 1 && osBandOrder.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing 'bandorder'" );
        return NULL;
    }

    int bNativeOrder = TRUE;
    if( EQUAL(osByteOrder, "little") )
    {
        bNativeOrder = CPL_IS_LSB;
    }
    else if( EQUAL(osByteOrder, "big") )
    {
        bNativeOrder = !CPL_IS_LSB;
    }
    else if( !EQUAL(osByteOrder, "") )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unhandled byteorder=%s. Assuming native order",
                 osByteOrder.c_str() );
    }

    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;
    const int nPixelSize = GDALGetDataTypeSizeBytes( eDT );
    if( l_nBands == 1 || EQUAL( osBandOrder, "BIL" ) )
    {
        nPixelOffset = nPixelSize;
        if( l_nBands != 0 && nPixelSize != 0 &&
            nCols > INT_MAX / ( l_nBands * nPixelSize ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Too many columns" );
            return NULL;
        }
        nLineOffset = nPixelSize * nCols * l_nBands;
        nBandOffset = nPixelSize * nCols;
    }
    else if( EQUAL( osBandOrder, "BIP" ) )
    {
        if( l_nBands != 0 && nPixelSize != 0 &&
            nCols > INT_MAX / ( l_nBands * nPixelSize ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Too many columns" );
            return NULL;
        }
        nPixelOffset = nPixelSize * l_nBands;
        nLineOffset = nPixelSize * nCols * l_nBands;
        nBandOffset = nPixelSize;
    }
    else if( EQUAL( osBandOrder, "BSQ" ) )
    {
        if( nPixelSize != 0 && nCols > INT_MAX / nPixelSize )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Too many columns" );
            return NULL;
        }
        nPixelOffset = nPixelSize;
        nLineOffset = nPixelSize * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    else if( l_nBands > 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Unknown bandorder" );
        return NULL;
    }

    CPLString osGriFilename;
    CPLString osDirname( CPLGetDirname(poOpenInfo->pszFilename) );
    CPLString osBasename( CPLGetBasename(poOpenInfo->pszFilename) );
    CPLString osGRDExtension( CPLGetExtension(poOpenInfo->pszFilename) );
    CPLString osGRIExtension( (osGRDExtension[0] == 'g') ? "gri" : "GRI" );
    char** papszSiblings = poOpenInfo->GetSiblingFiles();
    if( papszSiblings )
    {
        int iFile = CSLFindString(papszSiblings,
                            CPLFormFilename(NULL, osBasename, osGRIExtension) );
        if( iFile < 0 )
            return NULL;
        osGriFilename = CPLFormFilename( osDirname,
                                         papszSiblings[iFile], NULL );
    }
    else
    {
        osGriFilename = CPLFormFilename( osDirname, osBasename, osGRIExtension );
    }

    VSILFILE* fpImage = VSIFOpenL( osGriFilename, "rb" );
    if( fpImage == NULL )
        return NULL;

    RRASTERDataset* poDS = new RRASTERDataset;
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->m_adfGeoTransform[0] = dfXMin;
    poDS->m_adfGeoTransform[1] = (dfXMax - dfXMin) / nCols;
    poDS->m_adfGeoTransform[2] = 0.0;
    poDS->m_adfGeoTransform[3] = dfYMax;
    poDS->m_adfGeoTransform[4] = 0.0;
    poDS->m_adfGeoTransform[5] = -(dfYMax - dfYMin) / nRows;
    poDS->m_osGriFilename = osGriFilename;
    poDS->m_fpImage = fpImage;

    if( !osProjection.empty() )
    {
        OGRSpatialReference oSRS;
        if( oSRS.importFromProj4( osProjection.c_str() ) == OGRERR_NONE )
        {
            char* pszWKT = NULL;
            oSRS.exportToWkt( &pszWKT );
            if( pszWKT )
                poDS->m_osProjection = pszWKT;
            CPLFree( pszWKT );
        }
    }

    char** papszMinValues = CSLTokenizeString2(osMinValue, ":", 0);
    char** papszMaxValues = CSLTokenizeString2(osMaxValue, ":", 0);
    if( CSLCount(papszMinValues) != l_nBands ||
        CSLCount(papszMaxValues) != l_nBands )
    {
        CSLDestroy(papszMinValues);
        CSLDestroy(papszMaxValues);
        papszMinValues = NULL;
        papszMaxValues = NULL;
    }

    for( int i=1; i<=l_nBands; i++ )
    {
        RRASTERRasterBand* poBand = new RRASTERRasterBand(
                                  poDS, i, fpImage, nBandOffset * (i-1),
                                  nPixelOffset,
                                  nLineOffset, eDT, bNativeOrder );
        poDS->SetBand( i, poBand );
        if( EQUAL(osDataType, "INT1S") )
        {
            poDS->GetRasterBand(i)->SetMetadataItem(
                    "SIGNEDBYTE", "PIXELTYPE", "IMAGE_STRUCTURE" );
        }
        if( !EQUAL(osNoDataValue, "NA") )
        {
            double dfNoDataValue = CPLAtof(osNoDataValue);
            poBand->SetNoDataValue(dfNoDataValue);
        }
        if( papszMinValues && papszMaxValues )
        {
            poBand->SetMinMax( CPLAtof(papszMinValues[i-1]),
                               CPLAtof(papszMaxValues[i-1]) );
        }
    }
    CSLDestroy(papszMinValues);
    CSLDestroy(papszMaxValues);

    return poDS;
}

/************************************************************************/
/*                   GDALRegister_RRASTER()                             */
/************************************************************************/

void GDALRegister_RRASTER()

{
    if( GDALGetDriverByName( "RRASTER" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "RRASTER" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grd" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "R Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#RRASTER" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = RRASTERDataset::Open;
    poDriver->pfnIdentify = RRASTERDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
