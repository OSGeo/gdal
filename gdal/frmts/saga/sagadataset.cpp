/******************************************************************************
 * Project:  SAGA GIS Binary Driver
 * Purpose:  Implements the SAGA GIS Binary Grid Format.
 * Author:   Volker Wichmann, wichmann@laserdata.at
 *   (Based on gsbgdataset.cpp by Kevin Locke and Frank Warmerdam)
 *
 ******************************************************************************
 * Copyright (c) 2009, Volker Wichmann <wichmann@laserdata.at>
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"

#include <cassert>
#include <cfloat>
#include <climits>

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

#ifndef INT_MAX
# define INT_MAX 2147483647
#endif /* INT_MAX */

/* NODATA Values */
//#define SG_NODATA_GDT_Bit 0.0
static const GByte SG_NODATA_GDT_Byte = 255;
#define SG_NODATA_GDT_UInt16    65535
#define SG_NODATA_GDT_Int16     -32767
#define SG_NODATA_GDT_UInt32    4294967295U
#define SG_NODATA_GDT_Int32     -2147483647
#define SG_NODATA_GDT_Float32   -99999.0
#define SG_NODATA_GDT_Float64   -99999.0

/************************************************************************/
/* ==================================================================== */
/*                              SAGADataset                             */
/* ==================================================================== */
/************************************************************************/

class SAGARasterBand;

class SAGADataset : public GDALPamDataset
{
    friend class SAGARasterBand;

    static CPLErr WriteHeader( CPLString osHDRFilename, GDALDataType eType,
                               int nXSize, int nYSize,
                               double dfMinX, double dfMinY,
                               double dfCellsize, double dfNoData,
                               double dfZFactor, bool bTopToBottom );
    VSILFILE *fp;
    char     *pszProjection;

  public:
        SAGADataset();
    virtual ~SAGADataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                char **papszParmList );
    static GDALDataset *CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData );

    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr SetProjection( const char * ) override;
    virtual char **GetFileList() override;

    CPLErr GetGeoTransform( double *padfGeoTransform ) override;
    CPLErr SetGeoTransform( double *padfGeoTransform ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                            SAGARasterBand                            */
/* ==================================================================== */
/************************************************************************/

class SAGARasterBand : public GDALPamRasterBand
{
    friend class SAGADataset;

    int             m_Cols;
    int             m_Rows;
    double          m_Xmin;
    double          m_Ymin;
    double          m_Cellsize;
    double          m_NoData;
    int             m_ByteOrder;
    int             m_nBits;

    void            SetDataType( GDALDataType eType );
    void            SwapBuffer(void* pImage);

public:
    SAGARasterBand( SAGADataset *, int );

    CPLErr          IReadBlock( int, int, void * ) override;
    CPLErr          IWriteBlock( int, int, void * ) override;

    double          GetNoDataValue( int *pbSuccess = NULL ) override;
};

/************************************************************************/
/*                           SAGARasterBand()                           */
/************************************************************************/

SAGARasterBand::SAGARasterBand( SAGADataset *poDS_, int nBand_ ) :
    m_Cols(0),
    m_Rows(0),
    m_Xmin(0.0),
    m_Ymin(0.0),
    m_Cellsize(0.0),
    m_NoData(0.0),
    m_ByteOrder(0),
    m_nBits(0)
{
    poDS = poDS_;
    nBand = nBand_;

    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                            SetDataType()                             */
/************************************************************************/

void SAGARasterBand::SetDataType( GDALDataType eType )

{
    eDataType = eType;
    return;
}

/************************************************************************/
/*                             SwapBuffer()                             */
/************************************************************************/

void SAGARasterBand::SwapBuffer(void* pImage)
{

#ifdef CPL_LSB
    const bool bSwap = ( m_ByteOrder == 1);
#else
    const bool bSwap = ( m_ByteOrder == 0);
#endif

    if (bSwap)
    {
        if ( m_nBits == 16 )
        {
            short* pImage16 = reinterpret_cast<short *>( pImage );
            for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
            {
                CPL_SWAP16PTR( pImage16 + iPixel );
            }
        }
        else if ( m_nBits == 32 )
        {
            int* pImage32 = reinterpret_cast<int *>( pImage );
            for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
            {
                CPL_SWAP32PTR( pImage32 + iPixel );
            }
        }
        else if ( m_nBits == 64 )
        {
            double* pImage64 = reinterpret_cast<double *>( pImage );
            for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
            {
                CPL_SWAP64PTR( pImage64 + iPixel );
            }
        }
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAGARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    SAGADataset *poGDS = static_cast<SAGADataset *>( poDS );
    vsi_l_offset offset = static_cast<vsi_l_offset>( m_nBits / 8 )
        * nRasterXSize * (nRasterYSize - nBlockYOff - 1);

    if( VSIFSeekL( poGDS->fp, offset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
              "Unable to seek to beginning of grid row.\n" );
        return CE_Failure;
    }
    if( VSIFReadL( pImage, m_nBits / 8, nBlockXSize,
       poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
              "Unable to read block from grid file.\n" );
        return CE_Failure;
    }

    SwapBuffer(pImage);

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr SAGARasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                    void *pImage )

{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to write block, dataset opened read only.\n" );
        return CE_Failure;
    }

    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    const vsi_l_offset offset = static_cast<vsi_l_offset> (m_nBits / 8)
        * nRasterXSize * (nRasterYSize - nBlockYOff - 1);
    SAGADataset *poGDS = static_cast<SAGADataset *>(poDS);
    assert( poGDS != NULL );

    if( VSIFSeekL( poGDS->fp, offset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
              "Unable to seek to beginning of grid row.\n" );
        return CE_Failure;
    }

    SwapBuffer(pImage);

    const bool bSuccess = (
        VSIFWriteL( pImage, m_nBits / 8, nBlockXSize, poGDS->fp )
        == static_cast<unsigned>(nBlockXSize) );

    SwapBuffer(pImage);

    if (!bSuccess)
    {
        CPLError( CE_Failure, CPLE_FileIO,
              "Unable to write block to grid file.\n" );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double SAGARasterBand::GetNoDataValue( int * pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return m_NoData;
}

/************************************************************************/
/* ==================================================================== */
/*                              SAGADataset                             */
/* ==================================================================== */
/************************************************************************/

SAGADataset::SAGADataset() :
    fp(NULL),
    pszProjection(CPLStrdup(""))
{}

SAGADataset::~SAGADataset()

{
    CPLFree( pszProjection );
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** SAGADataset::GetFileList()
{
    const CPLString osPath = CPLGetPath( GetDescription() );
    const CPLString osName = CPLGetBasename( GetDescription() );

    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    if( !EQUAL(CPLGetExtension( GetDescription() ), "sg-grd-z") )
    {
        // Header file.
        CPLString osFilename = CPLFormCIFilename( osPath, osName, ".sgrd" );
        papszFileList = CSLAddString( papszFileList, osFilename );

        // projections file.
        osFilename = CPLFormCIFilename( osPath, osName, "prj" );
        VSIStatBufL sStatBuf;
        if( VSIStatExL( osFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
            papszFileList = CSLAddString( papszFileList, osFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SAGADataset::GetProjectionRef()

{
    if (pszProjection && strlen(pszProjection) > 0)
        return pszProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr SAGADataset::SetProjection( const char *pszSRS )

{
/* -------------------------------------------------------------------- */
/*      Reset coordinate system on the dataset.                         */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszSRS );

    if( strlen(pszSRS) == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Convert to ESRI WKT.                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS( pszSRS );
    char *pszESRI_SRS = NULL;

    oSRS.morphToESRI();
    oSRS.exportToWkt( &pszESRI_SRS );

/* -------------------------------------------------------------------- */
/*      Write to .prj file.                                             */
/* -------------------------------------------------------------------- */
    const CPLString osPrjFilename = CPLResetExtension( GetDescription(), "prj" );
    VSILFILE *l_fp = VSIFOpenL( osPrjFilename.c_str(), "wt" );
    if( l_fp != NULL )
    {
        VSIFWriteL( pszESRI_SRS, 1, strlen(pszESRI_SRS), l_fp );
        VSIFWriteL( reinterpret_cast<void *>( const_cast<char *>(  "\n" ) ),
                    1, 1, l_fp );
        VSIFCloseL( l_fp );
    }

    CPLFree( pszESRI_SRS );

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAGADataset::Open( GDALOpenInfo * poOpenInfo )

{
    /* -------------------------------------------------------------------- */
    /*  We assume the user is pointing to the binary (i.e. .sdat) file or a */
    /*  compressed raster (.sg-grd-z) file.                                 */
    /* -------------------------------------------------------------------- */
    CPLString osExtension(CPLGetExtension(poOpenInfo->pszFilename));

    if( !EQUAL(osExtension, "sdat") &&
        !EQUAL(osExtension, "sg-grd-z") )
    {
        return NULL;
    }

    CPLString osPath, osFullname, osName, osHDRFilename;

    if (EQUAL(osExtension, "sg-grd-z") &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsizip"))
    {
        osPath = "/vsizip/{";
        osPath += poOpenInfo->pszFilename;
        osPath += "}/";

        char ** filesinzip = VSIReadDir(osPath);
        if (filesinzip == NULL)
            return NULL; //empty zip file

        CPLString file;
        for (int iFile = 0; filesinzip != NULL && filesinzip[iFile] != NULL; iFile++)
        {
            if (EQUAL(CPLGetExtension(filesinzip[iFile]), "sdat"))
            {
                 file = filesinzip[iFile];
                 break;
            }
        }

        CSLDestroy(filesinzip);

        osFullname = CPLFormFilename (osPath, file, NULL);
        osName = CPLGetBasename(file);
        osHDRFilename = CPLFormFilename (osPath, CPLGetBasename(file) , "sgrd");
    }
    else
    {
        osFullname = poOpenInfo->pszFilename;
        osPath = CPLGetPath( poOpenInfo->pszFilename );
        osName = CPLGetBasename(poOpenInfo->pszFilename);
        osHDRFilename = CPLFormCIFilename( osPath, CPLGetBasename( poOpenInfo->pszFilename ), "sgrd" );
    }

    VSILFILE *fp = VSIFOpenL( osHDRFilename, "r" );
    if( fp == NULL )
    {
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Is this file a SAGA header file?  Read a few lines of text      */
    /*      searching for something starting with nrows or ncols.           */
    /* -------------------------------------------------------------------- */
    int nRows = -1;
    int nCols = -1;
    double dXmin = 0.0;
    double dYmin = 0.0;
    double dCellsize = 0.0;
    double dNoData = 0.0;
    double dZFactor = 0.0;
    int nLineCount = 0;
    char szDataFormat[20] = "DOUBLE";
    char szByteOrderBig[10] = "FALSE";
    char szTopToBottom[10] = "FALSE";

    const char *pszLine = NULL;
    while( (pszLine = CPLReadLineL( fp )) != NULL )
    {
        nLineCount++;

        if( nLineCount > 50 || strlen(pszLine) > 1000 )
            break;

        char **papszTokens
            = CSLTokenizeStringComplex( pszLine, " =", TRUE, FALSE );
        if( CSLCount( papszTokens ) < 2 )
        {
            CSLDestroy( papszTokens );
            continue;
        }

        char **papszHDR = CSLAddString( NULL, pszLine );

        if( STARTS_WITH_CI(papszTokens[0], "CELLCOUNT_X") )
            nCols = atoi(papszTokens[1]);
        else if( STARTS_WITH_CI(papszTokens[0], "CELLCOUNT_Y") )
            nRows = atoi(papszTokens[1]);
        else if( STARTS_WITH_CI(papszTokens[0], "POSITION_XMIN") )
            dXmin = CPLAtofM(papszTokens[1]);
        else if( STARTS_WITH_CI(papszTokens[0], "POSITION_YMIN") )
            dYmin = CPLAtofM(papszTokens[1]);
        else if( STARTS_WITH_CI(papszTokens[0], "CELLSIZE") )
            dCellsize = CPLAtofM(papszTokens[1]);
        else if( STARTS_WITH_CI(papszTokens[0], "NODATA_VALUE") )
            dNoData = CPLAtofM(papszTokens[1]);
        else if( STARTS_WITH_CI(papszTokens[0], "DATAFORMAT") )
            strncpy( szDataFormat, papszTokens[1], sizeof(szDataFormat)-1 );
        else if( STARTS_WITH_CI(papszTokens[0], "BYTEORDER_BIG") )
            strncpy( szByteOrderBig, papszTokens[1], sizeof(szByteOrderBig)-1 );
        else if( STARTS_WITH_CI(papszTokens[0], "TOPTOBOTTOM") )
            strncpy( szTopToBottom, papszTokens[1], sizeof(szTopToBottom)-1 );
        else if( STARTS_WITH_CI(papszTokens[0], "Z_FACTOR") )
            dZFactor = CPLAtofM(papszTokens[1]);

        CSLDestroy( papszTokens );
        CSLDestroy( papszHDR );
    }

    VSIFCloseL( fp );

    /* -------------------------------------------------------------------- */
    /*      Did we get the required keywords?  If not we return with        */
    /*      this never having been considered to be a match. This isn't     */
    /*      an error!                                                       */
    /* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 )
    {
        return NULL;
    }

    if (!GDALCheckDatasetDimensions(nCols, nRows))
    {
        return NULL;
    }

    if( STARTS_WITH_CI(szTopToBottom, "TRUE") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Currently the SAGA Binary Grid driver does not support\n"
                  "SAGA grids written TOPTOBOTTOM.\n");
        return NULL;
    }
    if( dZFactor != 1.0)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Currently the SAGA Binary Grid driver does not support\n"
                  "ZFACTORs other than 1.\n");
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    SAGADataset *poDS = new SAGADataset();

    poDS->eAccess = poOpenInfo->eAccess;
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fp = VSIFOpenL( osFullname.c_str(), "rb" );
    else
        poDS->fp = VSIFOpenL( osFullname.c_str(), "r+b" );

    if( poDS->fp == NULL )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "VSIFOpenL(%s) failed unexpectedly.",
                  osFullname.c_str() );
        return NULL;
    }

    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

    SAGARasterBand *poBand = new SAGARasterBand( poDS, 1 );

    /* -------------------------------------------------------------------- */
    /*      Figure out the byte order.                                      */
    /* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(szByteOrderBig, "TRUE") )
        poBand->m_ByteOrder = 1;
    else if( STARTS_WITH_CI(szByteOrderBig, "FALSE") )
        poBand->m_ByteOrder = 0;

    /* -------------------------------------------------------------------- */
    /*      Figure out the data type.                                       */
    /* -------------------------------------------------------------------- */
    if( EQUAL(szDataFormat,"BIT") )
    {
        poBand->SetDataType(GDT_Byte);
        poBand->m_nBits = 8;
    }
    else if( EQUAL(szDataFormat,"BYTE_UNSIGNED") )
    {
        poBand->SetDataType(GDT_Byte);
        poBand->m_nBits = 8;
    }
    else if( EQUAL(szDataFormat,"BYTE") )
    {
        poBand->SetDataType(GDT_Byte);
        poBand->m_nBits = 8;
    }
    else if( EQUAL(szDataFormat,"SHORTINT_UNSIGNED") )
    {
        poBand->SetDataType(GDT_UInt16);
        poBand->m_nBits = 16;
    }
    else if( EQUAL(szDataFormat,"SHORTINT") )
    {
        poBand->SetDataType(GDT_Int16);
        poBand->m_nBits = 16;
    }
    else if( EQUAL(szDataFormat,"INTEGER_UNSIGNED") )
    {
        poBand->SetDataType(GDT_UInt32);
        poBand->m_nBits = 32;
    }
    else if( EQUAL(szDataFormat,"INTEGER") )
    {
        poBand->SetDataType(GDT_Int32);
        poBand->m_nBits = 32;
    }
    else if( EQUAL(szDataFormat,"FLOAT") )
    {
        poBand->SetDataType(GDT_Float32);
        poBand->m_nBits = 32;
    }
    else if( EQUAL(szDataFormat,"DOUBLE") )
    {
        poBand->SetDataType(GDT_Float64);
        poBand->m_nBits = 64;
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SAGA driver does not support the dataformat %s.",
                  szDataFormat );
        delete poBand;
        delete poDS;
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Save band information                                           */
    /* -------------------------------------------------------------------- */
    poBand->m_Xmin = dXmin;
    poBand->m_Ymin = dYmin;
    poBand->m_NoData = dNoData;
    poBand->m_Cellsize = dCellsize;
    poBand->m_Rows = nRows;
    poBand->m_Cols = nCols;

    poDS->SetBand( 1, poBand );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for a .prj file.                                          */
/* -------------------------------------------------------------------- */
    const char  *pszPrjFilename = CPLFormCIFilename( osPath, osName, "prj" );

    fp = VSIFOpenL( pszPrjFilename, "r" );

    if( fp != NULL )
    {
        VSIFCloseL( fp );

        char **papszLines = CSLLoad( pszPrjFilename );

        OGRSpatialReference oSRS;
        if( oSRS.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }

        CSLDestroy( papszLines );
    }

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SAGADataset::GetGeoTransform( double *padfGeoTransform )
{
    if( padfGeoTransform == NULL )
        return CE_Failure;

    SAGARasterBand *poGRB = static_cast<SAGARasterBand *>(GetRasterBand( 1 ));

    if( poGRB == NULL )
    {
        padfGeoTransform[0] = 0;
        padfGeoTransform[1] = 1;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = 0;
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = 1;
        return CE_Failure;
    }

    /* check if we have a PAM GeoTransform stored */
    CPLPushErrorHandler( CPLQuietErrorHandler );
    CPLErr eErr = GDALPamDataset::GetGeoTransform( padfGeoTransform );
    CPLPopErrorHandler();

    if( eErr == CE_None )
        return CE_None;

    padfGeoTransform[1] = poGRB->m_Cellsize;
    padfGeoTransform[5] = poGRB->m_Cellsize * -1.0;
    padfGeoTransform[0] = poGRB->m_Xmin - poGRB->m_Cellsize / 2;
    padfGeoTransform[3] = poGRB->m_Ymin + (nRasterYSize - 1) * poGRB->m_Cellsize + poGRB->m_Cellsize / 2;

    /* tilt/rotation is not supported by SAGA grids */
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[2] = 0.0;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr SAGADataset::SetGeoTransform( double *padfGeoTransform )
{

    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set GeoTransform, dataset opened read only.\n" );
        return CE_Failure;
    }

    SAGARasterBand *poGRB = static_cast<SAGARasterBand *>(GetRasterBand( 1 ));

    if( poGRB == NULL || padfGeoTransform == NULL)
        return CE_Failure;

    if( padfGeoTransform[1] != padfGeoTransform[5] * -1.0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to set GeoTransform, SAGA binary grids only support "
                  "the same cellsize in x-y.\n" );
        return CE_Failure;
    }

    const double dfMinX = padfGeoTransform[0] + padfGeoTransform[1] / 2;
    const double dfMinY =
        padfGeoTransform[5] * (nRasterYSize - 0.5) + padfGeoTransform[3];

    const CPLString osPath = CPLGetPath( GetDescription() );
    const CPLString osName = CPLGetBasename( GetDescription() );
    const CPLString osHDRFilename = CPLFormCIFilename( osPath, osName, ".sgrd" );

    CPLErr eErr = WriteHeader( osHDRFilename, poGRB->GetRasterDataType(),
                               poGRB->nRasterXSize, poGRB->nRasterYSize,
                               dfMinX, dfMinY, padfGeoTransform[1],
                               poGRB->m_NoData, 1.0, false );

    if( eErr == CE_None )
    {
        poGRB->m_Xmin = dfMinX;
        poGRB->m_Ymin = dfMinY;
        poGRB->m_Cellsize = padfGeoTransform[1];
        poGRB->m_Cols = nRasterXSize;
        poGRB->m_Rows = nRasterYSize;
    }

    return eErr;
}

/************************************************************************/
/*                             WriteHeader()                            */
/************************************************************************/

CPLErr SAGADataset::WriteHeader( CPLString osHDRFilename, GDALDataType eType,
                                 int nXSize, int nYSize,
                                 double dfMinX, double dfMinY,
                                 double dfCellsize, double dfNoData,
                                 double dfZFactor, bool bTopToBottom )

{
    VSILFILE *fp = VSIFOpenL( osHDRFilename, "wt" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to write .sgrd file %s.",
                  osHDRFilename.c_str() );
        return CE_Failure;
    }

    VSIFPrintfL( fp, "NAME\t= %s\n", CPLGetBasename( osHDRFilename ) );
    VSIFPrintfL( fp, "DESCRIPTION\t=\n" );
    VSIFPrintfL( fp, "UNIT\t=\n" );
    VSIFPrintfL( fp, "DATAFILE_OFFSET\t= 0\n" );

    if( eType == GDT_Int32 )
        VSIFPrintfL( fp, "DATAFORMAT\t= INTEGER\n" );
    else if( eType == GDT_UInt32 )
        VSIFPrintfL( fp, "DATAFORMAT\t= INTEGER_UNSIGNED\n" );
    else if( eType == GDT_Int16 )
        VSIFPrintfL( fp, "DATAFORMAT\t= SHORTINT\n" );
    else if( eType == GDT_UInt16 )
        VSIFPrintfL( fp, "DATAFORMAT\t= SHORTINT_UNSIGNED\n" );
    else if( eType == GDT_Byte )
        VSIFPrintfL( fp, "DATAFORMAT\t= BYTE_UNSIGNED\n" );
    else if( eType == GDT_Float32 )
        VSIFPrintfL( fp, "DATAFORMAT\t= FLOAT\n" );
    else //if( eType == GDT_Float64 )
        VSIFPrintfL( fp, "DATAFORMAT\t= DOUBLE\n" );
#ifdef CPL_LSB
    VSIFPrintfL( fp, "BYTEORDER_BIG\t= FALSE\n" );
#else
    VSIFPrintfL( fp, "BYTEORDER_BIG\t= TRUE\n" );
#endif

    VSIFPrintfL( fp, "POSITION_XMIN\t= %.10f\n", dfMinX );
    VSIFPrintfL( fp, "POSITION_YMIN\t= %.10f\n", dfMinY );
    VSIFPrintfL( fp, "CELLCOUNT_X\t= %d\n", nXSize );
    VSIFPrintfL( fp, "CELLCOUNT_Y\t= %d\n", nYSize );
    VSIFPrintfL( fp, "CELLSIZE\t= %.10f\n", dfCellsize );
    VSIFPrintfL( fp, "Z_FACTOR\t= %f\n", dfZFactor );
    VSIFPrintfL( fp, "NODATA_VALUE\t= %f\n", dfNoData );
    if (bTopToBottom)
        VSIFPrintfL( fp, "TOPTOBOTTOM\t= TRUE\n" );
    else
        VSIFPrintfL( fp, "TOPTOBOTTOM\t= FALSE\n" );

    VSIFCloseL( fp );

    return CE_None;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *SAGADataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **papszParmList )

{
    if( nXSize <= 0 || nYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Unable to create grid, both X and Y size must be "
                  "non-negative.\n" );

        return NULL;
    }

    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "SAGA Binary Grid only supports 1 band" );
        return NULL;
    }

    if( eType != GDT_Byte && eType != GDT_UInt16 && eType != GDT_Int16
        && eType != GDT_UInt32 && eType != GDT_Int32 && eType != GDT_Float32
        && eType != GDT_Float64 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SAGA Binary Grid only supports Byte, UInt16, Int16, "
                  "UInt32, Int32, Float32 and Float64 datatypes.  Unable to "
                  "create with type %s.\n", GDALGetDataTypeName( eType ) );

        return NULL;
    }

    VSILFILE *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return NULL;
    }

    double dfNoDataVal = 0.0;

    const char* pszNoDataValue = CSLFetchNameValue(papszParmList, "NODATA_VALUE");
    if (pszNoDataValue)
    {
        dfNoDataVal = CPLAtofM(pszNoDataValue);
    }
    else
    {
      switch (eType)  /* GDT_Byte, GDT_UInt16, GDT_Int16, GDT_UInt32  */
      {  /* GDT_Int32, GDT_Float32, GDT_Float64 */
        case (GDT_Byte):
        {
            dfNoDataVal = SG_NODATA_GDT_Byte;
            break;
        }
        case (GDT_UInt16):
        {
            dfNoDataVal = SG_NODATA_GDT_UInt16;
            break;
        }
        case (GDT_Int16):
        {
            dfNoDataVal = SG_NODATA_GDT_Int16;
            break;
        }
        case (GDT_UInt32):
        {
            dfNoDataVal = SG_NODATA_GDT_UInt32;
            break;
        }
        case (GDT_Int32):
        {
            dfNoDataVal = SG_NODATA_GDT_Int32;
            break;
        }
        default:
        case (GDT_Float32):
        {
            dfNoDataVal = SG_NODATA_GDT_Float32;
            break;
        }
        case (GDT_Float64):
        {
            dfNoDataVal = SG_NODATA_GDT_Float64;
            break;
        }
      }
    }

    double dfNoDataForAlignment;
    void* abyNoData = &dfNoDataForAlignment;
    GDALCopyWords(&dfNoDataVal, GDT_Float64, 0,
                  abyNoData, eType, 0, 1);

    const CPLString osHdrFilename = CPLResetExtension( pszFilename, "sgrd" );
    CPLErr eErr = WriteHeader( osHdrFilename, eType,
                               nXSize, nYSize,
                               0.0, 0.0, 1.0,
                               dfNoDataVal, 1.0, false );

    if( eErr != CE_None )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    if( CPLFetchBool( papszParmList , "FILL_NODATA", true ) )
    {
        const int nDataTypeSize = GDALGetDataTypeSize(eType) / 8;
        GByte* pabyNoDataBuf = reinterpret_cast<GByte *>(
            VSIMalloc2(nDataTypeSize, nXSize ) );
        if (pabyNoDataBuf == NULL)
        {
            VSIFCloseL( fp );
            return NULL;
        }

        for( int iCol = 0; iCol < nXSize; iCol++)
        {
            memcpy(pabyNoDataBuf + iCol * nDataTypeSize, abyNoData, nDataTypeSize);
        }

        for( int iRow = 0; iRow < nYSize; iRow++ )
        {
            if( VSIFWriteL( pabyNoDataBuf, nDataTypeSize, nXSize, fp )
                != static_cast<unsigned>( nXSize ) )
            {
                VSIFCloseL( fp );
                VSIFree(pabyNoDataBuf);
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to write grid cell.  Disk full?\n" );
                return NULL;
            }
        }

        VSIFree(pabyNoDataBuf);
    }

    VSIFCloseL( fp );

    return reinterpret_cast<GDALDataset *>(
        GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *SAGADataset::CreateCopy( const char *pszFilename,
                                      GDALDataset *poSrcDS,
                                      int bStrict, CPL_UNUSED char **papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )
{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SAGA driver does not support source dataset with zero band.\n");
        return NULL;
    }
    else if (nBands > 1)
    {
        if( bStrict )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Unable to create copy, SAGA Binary Grid "
                      "format only supports one raster band.\n" );
            return NULL;
        }
        else
            CPLError( CE_Warning, CPLE_NotSupported,
                      "SAGA Binary Grid format only supports one "
                      "raster band, first band will be copied.\n" );
    }

    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( 1 );

    char** papszCreateOptions = CSLSetNameValue( NULL, "FILL_NODATA", "NO" );

    int bHasNoDataValue = FALSE;
    const double dfNoDataValue = poSrcBand->GetNoDataValue(&bHasNoDataValue);
    if (bHasNoDataValue)
        papszCreateOptions = CSLSetNameValue(papszCreateOptions, "NODATA_VALUE",
                                             CPLSPrintf("%.16g", dfNoDataValue));

    GDALDataset* poDstDS =
        Create(pszFilename, poSrcBand->GetXSize(), poSrcBand->GetYSize(),
               1, poSrcBand->GetRasterDataType(), papszCreateOptions);
    CSLDestroy(papszCreateOptions);

    if (poDstDS == NULL)
        return NULL;

    /* -------------------------------------------------------------------- */
    /*      Copy band data.                                                 */
    /* -------------------------------------------------------------------- */

    CPLErr eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS,
                                              (GDALDatasetH) poDstDS,
                                              NULL,
                                              pfnProgress, pProgressData );

    if (eErr == CE_Failure)
    {
        delete poDstDS;
        return NULL;
    }

    double  adfGeoTransform[6];

    poSrcDS->GetGeoTransform( adfGeoTransform );
    poDstDS->SetGeoTransform( adfGeoTransform );

    poDstDS->SetProjection( poSrcDS->GetProjectionRef() );

    return poDstDS;
}

/************************************************************************/
/*                          GDALRegister_SAGA()                         */
/************************************************************************/

void GDALRegister_SAGA()

{
    if( GDALGetDriverByName( "SAGA" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SAGA" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "SAGA GIS Binary Grid (.sdat, .sg-grd-z)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#SAGA" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "sdat sg-grd-z" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte Int16 "
                               "UInt16 Int32 UInt32 Float32 Float64" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = SAGADataset::Open;
    poDriver->pfnCreate = SAGADataset::Create;
    poDriver->pfnCreateCopy = SAGADataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
