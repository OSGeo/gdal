/******************************************************************************
 * $Id$
 * Project:  SAGA GIS Binary Driver
 * Purpose:  Implements the SAGA GIS Binary Grid Format.
 * Author:   Volker Wichmann, wichmann@laserdata.at
 *	         (Based on gsbgdataset.cpp by Kevin Locke and Frank Warmerdam)
 *
 ******************************************************************************
 * Copyright (c) 2009, Volker Wichmann <wichmann@laserdata.at>
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

#include <float.h>
#include <limits.h>
#include <assert.h>

#include "gdal_pam.h"

CPL_CVSID("$Id$");

#ifndef INT_MAX
# define INT_MAX 2147483647
#endif /* INT_MAX */

/* NODATA Values */
//#define	SG_NODATA_GDT_Bit	0.0
#define SG_NODATA_GDT_Byte		255.0
#define	SG_NODATA_GDT_UInt16	65535.0
#define	SG_NODATA_GDT_Int16		-32767.0
#define	SG_NODATA_GDT_UInt32	4294967295.0
#define	SG_NODATA_GDT_Int32		-2147483647.0
#define	SG_NODATA_GDT_Float32	-99999.0
#define	SG_NODATA_GDT_Float64	-99999.0


CPL_C_START
void	GDALRegister_SAGA(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*                              SAGADataset                             */
/* ==================================================================== */
/************************************************************************/

class SAGARasterBand;

class SAGADataset : public GDALPamDataset
{
    friend class		SAGARasterBand;

    static const double	dNODATA_VALUE_DEFAULT;

	static CPLErr		WriteHeader( CPLString osHDRFilename, GDALDataType eType,
									GInt16 nXSize, GInt16 nYSize,
									double dfMinX, double dfMinY,
									double dfCellsize, double dfNoData,
									double dfZFactor, bool bTopToBottom );
    FILE				*fp;

  public:
		~SAGADataset();

		static GDALDataset		*Open( GDALOpenInfo * );
		static GDALDataset		*Create( const char * pszFilename,
			 							int nXSize, int nYSize, int nBands,
										GDALDataType eType,
										char **papszParmList );
		static GDALDataset		*CreateCopy( const char *pszFilename,
										GDALDataset *poSrcDS,
										int bStrict, char **papszOptions,
										GDALProgressFunc pfnProgress,
										void *pProgressData );
		static CPLErr			Delete( const char *pszFilename );

		CPLErr					GetGeoTransform( double *padfGeoTransform );
		CPLErr					SetGeoTransform( double *padfGeoTransform );
};


const double SAGADataset::dNODATA_VALUE_DEFAULT = -99999.0;



/************************************************************************/
/* ==================================================================== */
/*                            SAGARasterBand                            */
/* ==================================================================== */
/************************************************************************/

class SAGARasterBand : public GDALPamRasterBand
{
    friend class	SAGADataset;
    
	int				m_Cols;
	int				m_Rows;
	double			m_Xmin;
	double			m_Ymin;
	double			m_Cellsize;
	double			m_NoData;
	int				m_ByteOrder;
    int				m_nBits;

	void			SetDataType( GDALDataType eType );

  public:

		SAGARasterBand( SAGADataset *, int );
		~SAGARasterBand();
    
		CPLErr		IReadBlock( int, int, void * );
		CPLErr		IWriteBlock( int, int, void * );

		double		GetNoDataValue( int *pbSuccess = NULL );
};

/************************************************************************/
/*                           SAGARasterBand()                           */
/************************************************************************/

SAGARasterBand::SAGARasterBand( SAGADataset *poDS, int nBand )

{
    this->poDS = poDS;
    nBand = nBand;
    
    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                           ~SAGARasterBand()                          */
/************************************************************************/

SAGARasterBand::~SAGARasterBand( )

{
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
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAGARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
				   void * pImage )

{
    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
		return CE_Failure;

    SAGADataset *poGDS = dynamic_cast<SAGADataset *>(poDS);
    

	switch( eDataType )		/* GDT_Byte, GDT_UInt16, GDT_Int16, GDT_UInt32 */
	{						/* GDT_Int32, GDT_Float32, GDT_Float64 */
	default:
		break;

	case GDT_Byte:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GByte) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(GByte), nBlockXSize,
		   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		GByte		*pfImage;
		pfImage		= (GByte *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}

	case GDT_UInt16:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GUInt16) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(GUInt16), nBlockXSize,
			   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		GUInt16		*pfImage;
		pfImage		= (GUInt16 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}

	case GDT_Int16:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GInt16) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(GInt16), nBlockXSize,
			   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		GInt16		*pfImage;
		pfImage		= (GInt16 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}

	case GDT_UInt32:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GUInt32) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(GUInt32), nBlockXSize,
			   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		GUInt32		*pfImage;
		pfImage		= (GUInt32 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}

	case GDT_Int32:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GInt32) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(GInt32), nBlockXSize,
			   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		GInt32		*pfImage;
		pfImage		= (GInt32 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}

	case GDT_Float32:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(float) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(float), nBlockXSize,
			   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		float		*pfImage;
		pfImage		= (float *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}

	case GDT_Float64:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(double) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}
		if( VSIFReadL( pImage, sizeof(double), nBlockXSize,
			   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to read block from grid file.\n" );
			return CE_Failure;
		}
		double		*pfImage;
		pfImage		= (double *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			switch( m_nBits )
			{
			default:
				break;

			case 16:
				if( m_ByteOrder == 0 )	CPL_LSBPTR16( pfImage+iPixel );	else	CPL_MSBPTR16( pfImage+iPixel );
				break;

			case 32:
				if( m_ByteOrder == 0 )	CPL_LSBPTR32( pfImage+iPixel );	else	CPL_MSBPTR32( pfImage+iPixel );
				break;

			case 64:
				if( m_ByteOrder == 0 )	CPL_LSBPTR64( pfImage+iPixel );	else	CPL_MSBPTR64( pfImage+iPixel );
				break;
			}
		}
		break;
		}
	}

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

    SAGADataset *poGDS = dynamic_cast<SAGADataset *>(poDS);
    assert( poGDS != NULL );


	switch( eDataType )		/* GDT_Byte, GDT_UInt16, GDT_Int16, GDT_UInt32 */
	{						/* GDT_Int32, GDT_Float32, GDT_Float64 */
	default:
		break;

	case GDT_Byte:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GByte) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		/*GByte *pfImage = (GByte *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR32( pfImage+iPixel );
		}*/

		if( VSIFWriteL( pImage, sizeof(GByte), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}

	case GDT_UInt16:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GUInt16) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		GUInt16 *pfImage = (GUInt16 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR16( pfImage+iPixel )
		}

		if( VSIFWriteL( pImage, sizeof(GUInt16), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}

	case GDT_Int16:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GInt16) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		GInt16 *pfImage = (GInt16 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR16( pfImage+iPixel );
		}

		if( VSIFWriteL( pImage, sizeof(GInt16), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}

	case GDT_UInt32:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GUInt32) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		GUInt32 *pfImage = (GUInt32 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR32( pfImage+iPixel );
		}

		if( VSIFWriteL( pImage, sizeof(GUInt32), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}

	case GDT_Int32:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(GInt32) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		GInt32 *pfImage = (GInt32 *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR32( pfImage+iPixel );
		}

		if( VSIFWriteL( pImage, sizeof(GInt32), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}

	case GDT_Float32:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(float) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		float *pfImage = (float *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR32( pfImage+iPixel );
		}

		if( VSIFWriteL( pImage, sizeof(float), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}

	case GDT_Float64:
		{
		if( VSIFSeekL( poGDS->fp, sizeof(double) * nRasterXSize * (nRasterYSize - nBlockYOff - 1), SEEK_SET ) != 0 )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to seek to beginning of grid row.\n" );
			return CE_Failure;
		}

		double *pfImage = (double *)pImage;
		for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
		{
			CPL_LSBPTR64( pfImage+iPixel );
		}

		if( VSIFWriteL( pImage, sizeof(double), nBlockXSize,
				poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
		{
			CPLError( CE_Failure, CPLE_FileIO,
				  "Unable to write block to grid file.\n" );
			return CE_Failure;
		}
		break;
		}
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
/*                              SAGADataset	                            */
/* ==================================================================== */
/************************************************************************/

SAGADataset::~SAGADataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAGADataset::Open( GDALOpenInfo * poOpenInfo )

{

	/* -------------------------------------------------------------------- */
	/*	We assume the user is pointing to the binary (ie. .sdat) file.	    */
	/* -------------------------------------------------------------------- */
	if( !EQUAL(CPLGetExtension( poOpenInfo->pszFilename ), "sdat"))
	{
		return NULL;
	}

    CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    CPLString osName = CPLGetBasename( poOpenInfo->pszFilename );
    CPLString osHDRFilename;

	osHDRFilename = CPLFormCIFilename( osPath, osName, ".sgrd" );


    FILE	*fp;

    fp = VSIFOpenL( osHDRFilename, "r" );
    
    if( fp == NULL )
    {
        return NULL;
    }

	/* -------------------------------------------------------------------- */
	/*      Is this file a SAGA header file?  Read a few lines of text      */
	/*      searching for something starting with nrows or ncols.           */
	/* -------------------------------------------------------------------- */
    const char		*pszLine;
    int				nRows = -1, nCols = -1, nBands = 1;
	double			dXmin, dYmin, dCellsize, dNoData, dZFactor;
    int				nLineCount			= 0;
    char			szDataFormat[20]	= "DOUBLE";
    char            szByteOrderBig[10]	= "FALSE";
	char			szTopToBottom[10]	= "FALSE";
    char            **papszHDR			= NULL;
    
	
    while( (pszLine = CPLReadLineL( fp )) )    
    {
        char	**papszTokens;

        nLineCount++;

        if( nLineCount > 50 || strlen(pszLine) > 1000 )
            break;

        papszHDR = CSLAddString( papszHDR, pszLine );

        papszTokens = CSLTokenizeStringComplex( pszLine, " =", TRUE, FALSE );
        if( CSLCount( papszTokens ) < 2 )
        {		
            CSLDestroy( papszTokens );
            continue;
        }

        if( EQUALN(papszTokens[0],"CELLCOUNT_X",strlen("CELLCOUNT_X")) )
            nCols = atoi(papszTokens[1]);
        else if( EQUALN(papszTokens[0],"CELLCOUNT_Y",strlen("CELLCOUNT_Y")) )
            nRows = atoi(papszTokens[1]);
		else if( EQUALN(papszTokens[0],"POSITION_XMIN",strlen("POSITION_XMIN")) )
			dXmin = atof(papszTokens[1]);
        else if( EQUALN(papszTokens[0],"POSITION_YMIN",strlen("POSITION_YMIN")) )
            dYmin = atof(papszTokens[1]);
        else if( EQUALN(papszTokens[0],"CELLSIZE",strlen("CELLSIZE")) )
            dCellsize = atof(papszTokens[1]);
        else if( EQUALN(papszTokens[0],"NODATA_VALUE",strlen("NODATA_VALUE")) )
            dNoData = atof(papszTokens[1]);
        else if( EQUALN(papszTokens[0],"DATAFORMAT",strlen("DATAFORMAT")) )
            strncpy( szDataFormat, papszTokens[1], sizeof(szDataFormat)-1 );
        else if( EQUALN(papszTokens[0],"BYTEORDER_BIG",strlen("BYTEORDER_BIG")) )
            strncpy( szByteOrderBig, papszTokens[1], sizeof(szByteOrderBig)-1 );
		else if( EQUALN(papszTokens[0],"TOPTOBOTTOM",strlen("TOPTOBOTTOM")) )
            strncpy( szTopToBottom, papszTokens[1], sizeof(szTopToBottom)-1 );
		else if( EQUALN(papszTokens[0],"Z_FACTOR",strlen("Z_FACTOR")) )
            dZFactor = atof(papszTokens[1]);

		CSLDestroy( papszTokens );
    }

    VSIFCloseL( fp );

	CSLDestroy( papszHDR );

	if( EQUALN(szTopToBottom,"TRUE",strlen("TRUE")) )
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
	/*      Did we get the required keywords?  If not we return with        */
	/*      this never having been considered to be a match. This isn't     */
	/*      an error!                                                       */
	/* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 )
    {
        return NULL;
    }
	
	
	/* -------------------------------------------------------------------- */
	/*      Create a corresponding GDALDataset.                             */
	/* -------------------------------------------------------------------- */
    SAGADataset	*poDS = new SAGADataset();

	poDS->eAccess = poOpenInfo->eAccess;
    if( poOpenInfo->eAccess == GA_ReadOnly )
    	poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
    	poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );

    if( poDS->fp == NULL )
    {
		delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "VSIFOpenL(%s) failed unexpectedly.", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

	poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

	SAGARasterBand *poBand = new SAGARasterBand( poDS, 1 );


	/* -------------------------------------------------------------------- */
	/*      Figure out the byte order.                                      */
	/* -------------------------------------------------------------------- */
    if( EQUALN(szByteOrderBig,"TRUE",strlen("TRUE")) )
		poBand->m_ByteOrder = 1;
    else if( EQUALN(szByteOrderBig,"FALSE",strlen("FALSE")) )
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
	poBand->m_Xmin		= dXmin;
	poBand->m_Ymin		= dYmin;
	poBand->m_NoData	= dNoData;
	poBand->m_Cellsize	= dCellsize;
	poBand->m_Rows		= nRows;
	poBand->m_Cols		= nCols;
	
	poDS->SetBand( 1, poBand );
    poDS->SetDescription( poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SAGADataset::GetGeoTransform( double *padfGeoTransform )
{
    if( padfGeoTransform == NULL )
		return CE_Failure;

    SAGARasterBand *poGRB = dynamic_cast<SAGARasterBand *>(GetRasterBand( 1 ));

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

    SAGARasterBand *poGRB = dynamic_cast<SAGARasterBand *>(GetRasterBand( 1 ));

    if( poGRB == NULL || padfGeoTransform == NULL)
		return CE_Failure;

	if( padfGeoTransform[1] != padfGeoTransform[5] * -1.0 )
	{
		CPLError( CE_Failure, CPLE_NotSupported,
			"Unable to set GeoTransform, SAGA binary grids only support "
			"the same cellsize in x-y.\n" );
		return CE_Failure;
	}

    double dfMinX = padfGeoTransform[0] + padfGeoTransform[1] / 2;
    double dfMinY =
        padfGeoTransform[5] * (nRasterYSize - 0.5) + padfGeoTransform[3];

    CPLString osPath		= CPLGetPath( GetDescription() );
    CPLString osName		= CPLGetBasename( GetDescription() );
	CPLString osHDRFilename = CPLFormCIFilename( osPath, osName, ".sgrd" );

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
								GInt16 nXSize, GInt16 nYSize,
								double dfMinX, double dfMinY,
								double dfCellsize, double dfNoData,
								double dfZFactor, bool bTopToBottom )

{

	FILE	*fp;

    fp = VSIFOpenL( osHDRFilename, "wt" );

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
	else if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
		CPLError( CE_Failure, CPLE_IllegalArg,
			  "Unable to create grid, SAGA Binary Grid format "
			  "only supports sizes up to %dx%d.  %dx%d not supported.\n",
			  INT_MAX, INT_MAX, nXSize, nYSize );

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

    FILE *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return NULL;
    }

	char *pszHdrFilename = CPLStrdup( CPLResetExtension( pszFilename, "sgrd" ) );
	CPLErr eErr = WriteHeader( pszHdrFilename, eType,
								nXSize, nYSize,
								0.0, 0.0, 1.0,
								(float)SAGADataset::dNODATA_VALUE_DEFAULT, 1.0, false );

    if( eErr != CE_None )
    {
		VSIFCloseL( fp );
		return NULL;
    }

	double	fVal;

	switch (eType)	/* GDT_Byte, GDT_UInt16, GDT_Int16, GDT_UInt32  */
	{				/* GDT_Int32, GDT_Float32, GDT_Float64 */

	case (GDT_Byte):
		fVal = SG_NODATA_GDT_Byte;
		break;
	case (GDT_UInt16):
		fVal = SG_NODATA_GDT_UInt16;
		CPL_LSBPTR16( &fVal );
		break;
	case (GDT_Int16):
		fVal = SG_NODATA_GDT_Int16;
		CPL_LSBPTR16( &fVal );
		break;
	case (GDT_UInt32):
		fVal = SG_NODATA_GDT_UInt32;
		CPL_LSBPTR32( &fVal );
		break;
	case (GDT_Int32):
		fVal = SG_NODATA_GDT_Int32;
		CPL_LSBPTR32( &fVal );
		break;
	default:
	case (GDT_Float32):
		fVal = SG_NODATA_GDT_Float32;
		CPL_LSBPTR32( &fVal );
		break;
	case (GDT_Float64):
		fVal = SG_NODATA_GDT_Float64;
		CPL_LSBPTR64( &fVal );
		break;
	}

	for( int iRow = 0; iRow < nYSize; iRow++ )
    {
		for( int iCol=0; iCol<nXSize; iCol++ )
		{
			switch (eType)	/* GDT_Byte, GDT_UInt16, GDT_Int16, GDT_UInt32  */
			{				/* GDT_Int32, GDT_Float32, GDT_Float64 */

			default:
				break;

			case (GDT_Byte):
				if( VSIFWriteL( (GByte *)&fVal, sizeof(GByte), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			case (GDT_UInt16):
				if( VSIFWriteL( (GUInt16 *)&fVal, sizeof(GUInt16), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			case (GDT_Int16):
				if( VSIFWriteL( (GInt16 *)&fVal, sizeof(GInt16), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			case (GDT_UInt32):
				if( VSIFWriteL( (GUInt32 *)&fVal, sizeof(GUInt32), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			case (GDT_Int32):
				if( VSIFWriteL( (GInt32 *)&fVal, sizeof(GInt32), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			case (GDT_Float32):
				if( VSIFWriteL( (float *)&fVal, sizeof(float), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			case (GDT_Float64):
				if( VSIFWriteL( (double *)&fVal, sizeof(double), 1, fp ) != 1 )
				{
					VSIFCloseL( fp );
					CPLError( CE_Failure, CPLE_FileIO,
						  "Unable to write grid cell.  Disk full?\n" );
					return NULL;
				}
				break;
			}
		}
    }

    VSIFCloseL( fp );

    return (GDALDataset *)GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *SAGADataset::CreateCopy( const char *pszFilename,
				      GDALDataset *poSrcDS,
				      int bStrict, char **papszOptions,
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
    if( poSrcBand->GetXSize() > INT_MAX || poSrcBand->GetYSize() > INT_MAX )
    {
		CPLError( CE_Failure, CPLE_IllegalArg,
			  "Unable to create grid, SAGA Binary Grid format "
			  "only supports sizes up to %dx%d.  %dx%d not supported.\n",
			  INT_MAX, INT_MAX,
			  poSrcBand->GetXSize(), poSrcBand->GetYSize() );

		return NULL;
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated\n" );
        return NULL;
    }

    FILE    *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return NULL;
    }

    GInt16  nXSize = poSrcBand->GetXSize();
    GInt16  nYSize = poSrcBand->GetYSize();
    double  adfGeoTransform[6];

    poSrcDS->GetGeoTransform( adfGeoTransform );

    double dfMinX = adfGeoTransform[0] + adfGeoTransform[1] / 2;
    double dfMaxX = adfGeoTransform[1] * (nXSize - 0.5) + adfGeoTransform[0];
    double dfMinY = adfGeoTransform[5] * (nYSize - 0.5) + adfGeoTransform[3];
    double dfMaxY = adfGeoTransform[3] + adfGeoTransform[5] / 2;

	/* -------------------------------------------------------------------- */
	/*      Copy band data.	                                                */
	/* -------------------------------------------------------------------- */

	CPLErr	eErr;

	switch (poSrcBand->GetRasterDataType())	/* GDT_Byte, GDT_UInt16  */
	{		/* GDT_Int16, GDT_UInt32, GDT_Int32, GDT_Float32, GDT_Float64 */

	default:
		break;

	case (GDT_Byte):
		{
		GByte	*pfData = (GByte *)VSIMalloc2( nXSize, sizeof( GByte ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int		bSrcHasNDValue;
		GByte	fSrcNoDataValue = (GByte)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_Byte, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				//CPL_LSBPTR32( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(GByte), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}

		case (GDT_UInt16):
		{
		GUInt16	*pfData = (GUInt16 *)VSIMalloc2( nXSize, sizeof( GUInt16 ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int		bSrcHasNDValue;
		GUInt16	fSrcNoDataValue = (GUInt16)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_UInt16, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				CPL_LSBPTR16( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(GUInt16), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}

	case (GDT_Int16):
		{
		GInt16	*pfData = (GInt16 *)VSIMalloc2( nXSize, sizeof( GInt16 ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int		bSrcHasNDValue;
		GInt16	fSrcNoDataValue = (GInt16)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_Int16, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				CPL_LSBPTR16( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(GInt16), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}

	case (GDT_UInt32):
		{
		GUInt32	*pfData = (GUInt32 *)VSIMalloc2( nXSize, sizeof( GUInt32 ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int		bSrcHasNDValue;
		GUInt32	fSrcNoDataValue = (GUInt32)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_UInt32, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				CPL_LSBPTR32( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(GUInt32), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}

	case (GDT_Int32):
		{
		GInt32	*pfData = (GInt32 *)VSIMalloc2( nXSize, sizeof( GInt32 ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int     bSrcHasNDValue;
		GInt32   fSrcNoDataValue = (GInt32)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_Int32, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				CPL_LSBPTR32( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(GInt32), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}

	case GDT_Float32:
		{
		float *pfData = (float *)VSIMalloc2( nXSize, sizeof( float ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int     bSrcHasNDValue;
		float   fSrcNoDataValue = (float)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_Float32, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				CPL_LSBPTR32( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(float), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}

		case GDT_Float64:
		{
		double *pfData = (double *)VSIMalloc2( nXSize, sizeof( double ) );
		if( pfData == NULL )
		{
			VSIFCloseL( fp );
			CPLError( CE_Failure, CPLE_OutOfMemory,
				  "Unable to create copy, unable to allocate line buffer.\n" );
			return NULL;
		}

		int     bSrcHasNDValue;
		double  fSrcNoDataValue = (double)poSrcBand->GetNoDataValue( &bSrcHasNDValue );

		for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
		{
			eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
							nXSize, 1, pfData,
							nXSize, 1, GDT_Float64, 0, 0 );

			if( eErr != CE_None )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				return NULL;
			}

			for( int iCol=0; iCol<nXSize; iCol++ )
			{
				if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
					pfData[iCol] = fSrcNoDataValue;

				CPL_LSBPTR64( pfData+iCol );
			}

			if( VSIFWriteL( (void *)pfData, sizeof(double), nXSize,
					fp ) != static_cast<unsigned>(nXSize) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_FileIO,
					  "Unable to write grid row. Disk full?\n" );
				return NULL;
			}

			if( !pfnProgress( static_cast<double>(iRow)/nYSize,
					  NULL, pProgressData ) )
			{
				VSIFCloseL( fp );
				VSIFree( pfData );
				CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
				return NULL;
			}
		}
		VSIFree( pfData );
		break;
		}
	}

	char *pszHdrFilename = CPLStrdup( CPLResetExtension( pszFilename, "sgrd" ) );

	eErr = WriteHeader( pszHdrFilename, poSrcBand->GetRasterDataType(),
						nXSize, nYSize,
						dfMinX, dfMinY, adfGeoTransform[1],
						poSrcBand->GetNoDataValue(), 1.0, false );

	VSIFCloseL( fp );
    CPLFree( pszHdrFilename );

	if( eErr != CE_None )
        return NULL;


    GDALPamDataset *poDstDS = (GDALPamDataset *)GDALOpen( pszFilename,
							  GA_Update );
    if( poDstDS == NULL )
    {
		VSIUnlink( pszFilename );
		CPLError( CE_Failure, CPLE_FileIO,
			  "Unable to open copy of dataset.\n" );
		return NULL;
    }
    else if( dynamic_cast<SAGADataset *>(poDstDS) == NULL )
    {
		VSIUnlink( pszFilename );
		delete poDstDS;
		CPLError( CE_Failure, CPLE_FileIO,
			  "Copy dataset not opened as SAGA Binary Grid!?\n" );
		return NULL;
    }

    GDALRasterBand *poDstBand = poSrcDS->GetRasterBand(1);
    if( poDstBand == NULL )
    {
		VSIUnlink( pszFilename );
		delete poDstDS;
		CPLError( CE_Failure, CPLE_FileIO,
			  "Unable to open copy of raster band?\n" );
		return NULL;
    }

	if( !bStrict )
		CPLPopErrorHandler();

    return poDstDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

CPLErr SAGADataset::Delete( const char *pszFilename )

{
    VSIStatBufL sStat;
    
    if( VSIStatL( pszFilename, &sStat ) != 0 )
    {
		CPLError( CE_Failure, CPLE_FileIO,
			  "Unable to stat() %s.\n", pszFilename );
		return CE_Failure;
    }
    
    if( !VSI_ISREG( sStat.st_mode ) )
    {
		CPLError( CE_Failure, CPLE_FileIO,
			  "%s is not a regular file, not removed.\n", pszFilename );
		return CE_Failure;
    }

    if( VSIUnlink( pszFilename ) != 0 )
    {
		CPLError( CE_Failure, CPLE_FileIO,
			  "Error unlinking %s.\n", pszFilename );
		return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_SAGA()                         */
/************************************************************************/

void GDALRegister_SAGA()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "SAGA" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "SAGA" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "SAGA GIS Binary Grid (.sdat)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SAGA" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "sdat" );
		poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
				   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );

        poDriver->pfnOpen = SAGADataset::Open;
		poDriver->pfnCreate = SAGADataset::Create;
		poDriver->pfnCreateCopy = SAGADataset::CreateCopy;
		poDriver->pfnDelete = SAGADataset::Delete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
