/******************************************************************************
 * $Id$
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Read/write LizardTech's MrSID file format - Version 4+ SDK.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#define NO_DELETE

#include "gdaljp2abstractdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "gdaljp2metadata.h"
#include <string>

#include <geo_normalize.h>
#include <geovalues.h>

CPL_CVSID("$Id$");

CPL_C_START
double GTIFAngleToDD( double dfAngle, int nUOMAngle );
void CPL_DLL LibgeotiffOneTimeInit();
CPL_C_END

// Key Macros from Makefile:
//   MRSID_ESDK: Means we have the encoding SDK (version 5 or newer required)
//   MRSID_J2K: Means we are enabling MrSID SDK JPEG2000 support. 

#include "lt_types.h"
#include "lt_base.h"
#include "lt_fileSpec.h"
#include "lt_ioFileStream.h"
#include "lt_utilStatusStrings.h"
#include "lti_geoCoord.h"
#include "lti_pixel.h"
#include "lti_navigator.h"
#include "lti_sceneBuffer.h"
#include "lti_metadataDatabase.h"
#include "lti_metadataRecord.h"
#include "lti_utils.h"
#include "lti_delegates.h"
#include "lt_utilStatus.h"
#include "MrSIDImageReader.h"

#ifdef MRSID_J2K
#  include "J2KImageReader.h"
#endif

// It seems that LT_STS_UTIL_TimeUnknown was added in version 6, also
// the first version with lti_version.h
#ifdef LT_STS_UTIL_TimeUnknown
#  include "lti_version.h"
#endif

// Are we using version 6 or newer?
#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 6
#  define MRSID_POST5
#endif

#ifdef MRSID_ESDK
# include "MG3ImageWriter.h"
# include "MG3WriterParams.h"
# include "MG2ImageWriter.h"
# include "MG2WriterParams.h"
# ifdef MRSID_HAVE_MG4WRITE
#   include "MG4ImageWriter.h"
#   include "MG4WriterParams.h"
# endif
# ifdef MRSID_J2K
#   ifdef MRSID_POST5
#     include "JP2WriterManager.h"
#     include "JPCWriterParams.h"
#   else
#     include "J2KImageWriter.h"
#     include "J2KWriterParams.h"
#   endif
# endif
#endif /* MRSID_ESDK */

#ifdef MRSID_POST5
#  define MRSID_HAVE_GETWKT
#endif

#include "mrsidstream.h"

LT_USE_NAMESPACE(LizardTech)

/* -------------------------------------------------------------------- */
/*      Various wrapper templates used to force new/delete to happen    */
/*      in the same heap.  See bug 1213 and MSDN knowledgebase          */
/*      article 122675.                                                 */
/* -------------------------------------------------------------------- */

template <class T>
class LTIDLLPixel : public T
{
public:
   LTIDLLPixel(LTIColorSpace colorSpace,
            lt_uint16 numBands,
            LTIDataType dataType) : T(colorSpace,numBands,dataType) {}
   virtual ~LTIDLLPixel() {};
};

template <class T>
class LTIDLLReader : public T
{
public:
   LTIDLLReader(const LTFileSpec& fileSpec,
                bool useWorldFile = false) : T(fileSpec, useWorldFile) {}
   LTIDLLReader(LTIOStreamInf &oStream,
                bool useWorldFile = false) : T(oStream, useWorldFile) {}
   LTIDLLReader(LTIOStreamInf *poStream,
                LTIOStreamInf *poWorldFile = NULL) : T(poStream, poWorldFile) {}
   virtual ~LTIDLLReader() {};
};

template <class T>
class LTIDLLNavigator : public T
{
public:
   LTIDLLNavigator(const LTIImage& image ) : T(image) {}
   virtual ~LTIDLLNavigator() {};
};

template <class T>
class LTIDLLBuffer : public T
{
public:
   LTIDLLBuffer(const LTIPixel& pixelProps,
                  lt_uint32 totalNumCols,
                  lt_uint32 totalNumRows,
                  void** data ) : T(pixelProps,totalNumCols,totalNumRows,data) {}
   virtual ~LTIDLLBuffer() {};
};

template <class T>
class LTIDLLCopy : public T
{
public:
   LTIDLLCopy(const T& original) : T(original) {}
   virtual ~LTIDLLCopy() {};
};

template <class T>
class LTIDLLWriter : public T
{
public:
    LTIDLLWriter(LTIImageStage *image) : T(image) {}
    virtual ~LTIDLLWriter() {}
};

template <class T>
class LTIDLLDefault : public T
{
public:
    LTIDLLDefault() : T() {}
    virtual ~LTIDLLDefault() {}
};

/* -------------------------------------------------------------------- */
/*      Interface to MrSID SDK progress reporting.                      */
/* -------------------------------------------------------------------- */

class MrSIDProgress : public LTIProgressDelegate
{
public:
    MrSIDProgress(GDALProgressFunc f, void *arg) : m_f(f), m_arg(arg) {}
    virtual ~MrSIDProgress() {}
    virtual LT_STATUS setProgressStatus(float fraction)
    {
        if (!m_f)
            return LT_STS_BadContext;
        if( !m_f( fraction, NULL, m_arg ) )
            return LT_STS_Failure;
        return LT_STS_Success;
    }
private:
    GDALProgressFunc m_f;
    void *m_arg;
};

/************************************************************************/
/* ==================================================================== */
/*                              MrSIDDataset                            */
/* ==================================================================== */
/************************************************************************/

class MrSIDDataset : public GDALJP2AbstractDataset
{
    friend class MrSIDRasterBand;

    LTIOStreamInf       *poStream;
    LTIOFileStream      oLTIStream;
    LTIVSIStream        oVSIStream;

#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 7
    LTIImageFilter      *poImageReader;
#else
    LTIImageReader      *poImageReader;
#endif

#ifdef MRSID_ESDK
    LTIGeoFileImageWriter *poImageWriter;
#endif

    LTIDLLNavigator<LTINavigator>  *poLTINav;
    LTIDLLCopy<LTIMetadataDatabase> *poMetadata;
    const LTIPixel      *poNDPixel;

    LTIDLLBuffer<LTISceneBuffer>  *poBuffer;
    int                 nBlockXSize, nBlockYSize;
    int                 bPrevBlockRead;
    int                 nPrevBlockXOff, nPrevBlockYOff;

    LTIDataType         eSampleType;
    GDALDataType        eDataType;
    LTIColorSpace       eColorSpace;

    double              dfCurrentMag;

    GTIFDefn            *psDefn;

    MrSIDDataset       *poParentDS;
    int                 bIsOverview;
    int                 nOverviewCount;
    MrSIDDataset        **papoOverviewDS;

    CPLString           osMETFilename;

    CPLErr              OpenZoomLevel( lt_int32 iZoom );
    char                *SerializeMetadataRec( const LTIMetadataRecord* );
    int                 GetMetadataElement( const char *, void *, int=0 );
    void                FetchProjParms();
    void                GetGTIFDefn();
    char                *GetOGISDefn( GTIFDefn * );

    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int, void *,
                                   int, int, GDALDataType, int, int *,int,
                                   int, int );

  protected:
    virtual int         CloseDependentDatasets();

    virtual CPLErr      IBuildOverviews( const char *, int, int *,
                                         int, int *, GDALProgressFunc, void * );

  public:
                MrSIDDataset(int bIsJPEG2000);
                ~MrSIDDataset();

    static GDALDataset  *Open( GDALOpenInfo * poOpenInfo, int bIsJP2 );

    virtual char      **GetFileList();

#ifdef MRSID_ESDK
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszParmList );
    virtual void        FlushCache( void );
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                           MrSIDRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class MrSIDRasterBand : public GDALPamRasterBand
{
    friend class MrSIDDataset;

    LTIPixel        *poPixel;

    int             nBlockSize;

    int             bNoDataSet;
    double          dfNoDataValue;

    MrSIDDataset    *poGDS;

    GDALColorInterp eBandInterp;

  public:

                MrSIDRasterBand( MrSIDDataset *, int );
                ~MrSIDRasterBand();

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    CPLErr                  SetColorInterpretation( GDALColorInterp eNewInterp );
    virtual double          GetNoDataValue( int * );
    virtual int             GetOverviewCount();
    virtual GDALRasterBand  *GetOverview( int );

    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax, 
                                  double *pdfMean, double *pdfStdDev );

#ifdef MRSID_ESDK
    virtual CPLErr          IWriteBlock( int, int, void * );
#endif
};

/************************************************************************/
/*                           MrSIDRasterBand()                          */
/************************************************************************/

MrSIDRasterBand::MrSIDRasterBand( MrSIDDataset *poDS, int nBand )
{
    this->poDS = poDS;
    poGDS = poDS;
    this->nBand = nBand;
    this->eDataType = poDS->eDataType;

/* -------------------------------------------------------------------- */
/*      Set the block sizes and buffer parameters.                      */
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;
//#ifdef notdef
    if( poDS->GetRasterXSize() > 2048 )
        nBlockXSize = 1024;
    if( poDS->GetRasterYSize() > 128 )
        nBlockYSize = 128;
    else
        nBlockYSize = poDS->GetRasterYSize();
//#endif

    nBlockSize = nBlockXSize * nBlockYSize;
    poPixel = new LTIDLLPixel<LTIPixel>( poDS->eColorSpace, poDS->nBands,
                                         poDS->eSampleType );


/* -------------------------------------------------------------------- */
/*      Set NoData values.                                              */
/*                                                                      */
/*      This logic is disabled for now since the MrSID nodata           */
/*      semantics are different than GDAL.  In MrSID all bands must     */
/*      match the nodata value for that band in order for the pixel     */
/*      to be considered nodata, otherwise all values are valid.        */
/* -------------------------------------------------------------------- */
#ifdef notdef
     if ( poDS->poNDPixel )
     {
         switch( poDS->eSampleType )
         {
             case LTI_DATATYPE_UINT8:
             case LTI_DATATYPE_SINT8:
                 dfNoDataValue = (double)
                     poDS->poNDPixel->getSampleValueUint8( nBand - 1 );
                 break;
             case LTI_DATATYPE_UINT16:
                 dfNoDataValue = (double)
                     poDS->poNDPixel->getSampleValueUint16( nBand - 1 );
                 break;
             case LTI_DATATYPE_FLOAT32:
                 dfNoDataValue =
                     poDS->poNDPixel->getSampleValueFloat32( nBand - 1 );
                 break;
             case LTI_DATATYPE_SINT16:
                 dfNoDataValue = (double)
                     *(GInt16 *)poDS->poNDPixel->getSampleValueAddr( nBand - 1 );
                 break;
             case LTI_DATATYPE_UINT32:
                 dfNoDataValue = (double)
                     *(GUInt32 *)poDS->poNDPixel->getSampleValueAddr( nBand - 1 );
                 break;
             case LTI_DATATYPE_SINT32:
                 dfNoDataValue = (double)
                     *(GInt32 *)poDS->poNDPixel->getSampleValueAddr( nBand - 1 );
                 break;
             case LTI_DATATYPE_FLOAT64:
                 dfNoDataValue =
                     *(double *)poDS->poNDPixel->getSampleValueAddr( nBand - 1 );
                 break;

             case LTI_DATATYPE_INVALID:
                 CPLAssert( FALSE );
                 break;
         }
         bNoDataSet = TRUE;
     }
     else
#endif
     {
        dfNoDataValue = 0.0;
        bNoDataSet = FALSE;
     }

    switch( poGDS->eColorSpace )
    {
        case LTI_COLORSPACE_RGB:
            if( nBand == 1 )
                eBandInterp = GCI_RedBand;
            else if( nBand == 2 )
                eBandInterp = GCI_GreenBand;
            else if( nBand == 3 )
                eBandInterp = GCI_BlueBand;
            else
                eBandInterp = GCI_Undefined;
            break;

#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
        case LTI_COLORSPACE_RGBA:
            if( nBand == 1 )
                eBandInterp = GCI_RedBand;
            else if( nBand == 2 )
                eBandInterp = GCI_GreenBand;
            else if( nBand == 3 )
                eBandInterp = GCI_BlueBand;
            else if( nBand == 4 )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
            break;
#endif

        case LTI_COLORSPACE_CMYK:
            if( nBand == 1 )
                eBandInterp = GCI_CyanBand;
            else if( nBand == 2 )
                eBandInterp = GCI_MagentaBand;
            else if( nBand == 3 )
                eBandInterp = GCI_YellowBand;
            else if( nBand == 4 )
                eBandInterp = GCI_BlackBand;
            else
                eBandInterp = GCI_Undefined;
            break;

        case LTI_COLORSPACE_GRAYSCALE:
            eBandInterp = GCI_GrayIndex;
            break;

        default:
            eBandInterp = GCI_Undefined;
            break;
    }
}

/************************************************************************/
/*                            ~MrSIDRasterBand()                        */
/************************************************************************/

MrSIDRasterBand::~MrSIDRasterBand()
{
    if ( poPixel )
        delete poPixel;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MrSIDRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )
{
#ifdef MRSID_ESDK
    if( poGDS->eAccess == GA_Update )
    {
        CPLDebug( "MrSID", 
                  "IReadBlock() - DSDK - read on updatable file fails." );
        memset( pImage, 0, nBlockSize * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }
#endif /* MRSID_ESDK */

    CPLDebug( "MrSID", "IReadBlock(%d,%d)", nBlockXOff, nBlockYOff );

    if ( !poGDS->bPrevBlockRead
         || poGDS->nPrevBlockXOff != nBlockXOff
         || poGDS->nPrevBlockYOff != nBlockYOff )
    {
        GInt32      nLine = nBlockYOff * nBlockYSize;
        GInt32      nCol = nBlockXOff * nBlockXSize;

        // XXX: The scene, passed to LTIImageStage::read() call must be
        // inside the image boundaries. So we should detect the last strip and
        // form the scene properly.
        CPLDebug( "MrSID", 
                  "IReadBlock - read() %dx%d block at %d,%d.", 
                  nBlockXSize, nBlockYSize, nCol, nLine );
                  
        if(!LT_SUCCESS( poGDS->poLTINav->setSceneAsULWH(
                            nCol, nLine,
                            (nCol+nBlockXSize>poGDS->GetRasterXSize())?
                            (poGDS->GetRasterXSize()-nCol):nBlockXSize,
                            (nLine+nBlockYSize>poGDS->GetRasterYSize())?
                            (poGDS->GetRasterYSize()-nLine):nBlockYSize,
                            poGDS->dfCurrentMag) ))
            
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MrSIDRasterBand::IReadBlock(): Failed to set scene position." );
            return CE_Failure;
        }

        if ( !poGDS->poBuffer )
        {
            poGDS->poBuffer =
                new LTIDLLBuffer<LTISceneBuffer>( *poPixel, nBlockXSize, nBlockYSize, NULL );
//            poGDS->poBuffer =
//                new LTISceneBuffer( *poPixel, nBlockXSize, nBlockYSize, NULL );
        }

        if(!LT_SUCCESS(poGDS->poImageReader->read(poGDS->poLTINav->getScene(),
                                                  *poGDS->poBuffer)))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MrSIDRasterBand::IReadBlock(): Failed to load image." );
            return CE_Failure;
        }

        poGDS->bPrevBlockRead = TRUE;
        poGDS->nPrevBlockXOff = nBlockXOff;
        poGDS->nPrevBlockYOff = nBlockYOff;
    }

    memcpy( pImage, poGDS->poBuffer->getTotalBandData(nBand - 1), 
            nBlockSize * (GDALGetDataTypeSize(poGDS->eDataType) / 8) );

    return CE_None;
}

#ifdef MRSID_ESDK

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr MrSIDRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )
{
    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

#ifdef DEBUG
    CPLDebug( "MrSID", "IWriteBlock(): nBlockXOff=%d, nBlockYOff=%d",
              nBlockXOff, nBlockYOff );
#endif

    LTIScene        oScene( nBlockXOff * nBlockXSize,
                            nBlockYOff * nBlockYSize,
                            nBlockXSize, nBlockYSize, 1.0);
    LTISceneBuffer  oSceneBuf( *poPixel, poGDS->nBlockXSize,
                               poGDS->nBlockYSize, &pImage );

    if( !LT_SUCCESS(poGDS->poImageWriter->writeBegin(oScene)) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDRasterBand::IWriteBlock(): writeBegin failed." );
        return CE_Failure;
    }

    if( !LT_SUCCESS(poGDS->poImageWriter->writeStrip(oSceneBuf, oScene)) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDRasterBand::IWriteBlock(): writeStrip failed." );
        return CE_Failure;
    }

    if( !LT_SUCCESS(poGDS->poImageWriter->writeEnd()) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDRasterBand::IWriteBlock(): writeEnd failed." );
        return CE_Failure;
    }

    return CE_None;
}

#endif /* MRSID_ESDK */

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr MrSIDRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                   int nXOff, int nYOff, int nXSize, int nYSize,
                                   void * pData, int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType,
                                   int nPixelSpace, int nLineSpace )
    
{
/* -------------------------------------------------------------------- */
/*      Fallback to default implementation if the whole scanline        */
/*      without subsampling requested.                                  */
/* -------------------------------------------------------------------- */
    if ( nXSize == poGDS->GetRasterXSize()
         && nXSize == nBufXSize
         && nYSize == nBufYSize )
    {
        return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                          nXSize, nYSize, pData,
                                          nBufXSize, nBufYSize, eBufType,
                                          nPixelSpace, nLineSpace );
    }

/* -------------------------------------------------------------------- */
/*      Handle via the dataset level IRasterIO()                        */
/* -------------------------------------------------------------------- */
    return poGDS->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, 
                             nBufXSize, nBufYSize, eBufType, 
                             1, &nBand, nPixelSpace, nLineSpace, 0 );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MrSIDRasterBand::GetColorInterpretation()

{
    return eBandInterp;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/*                                                                      */
/*      This would normally just be used by folks using the MrSID code  */
/*      to read JP2 streams in other formats (such as NITF) and         */
/*      providing their own color interpretation regardless of what     */
/*      MrSID might think the stream itself says.                       */
/************************************************************************/

CPLErr MrSIDRasterBand::SetColorInterpretation( GDALColorInterp eNewInterp )

{
    eBandInterp = eNewInterp;

    return CE_None;
}

/************************************************************************/
/*                           GetStatistics()                            */
/*                                                                      */
/*      We override this method so that we can force generation of      */
/*      statistics if approx ok is true since we know that a small      */
/*      overview is always available, and that computing statistics     */
/*      from it is very fast.                                           */
/************************************************************************/

CPLErr MrSIDRasterBand::GetStatistics( int bApproxOK, int bForce,
                                       double *pdfMin, double *pdfMax, 
                                       double *pdfMean, double *pdfStdDev )

{
    if( bApproxOK )
        bForce = TRUE;

    return GDALPamRasterBand::GetStatistics( bApproxOK, bForce, 
                                             pdfMin, pdfMax, 
                                             pdfMean, pdfStdDev );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double MrSIDRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = bNoDataSet;

        return dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int MrSIDRasterBand::GetOverviewCount()

{
    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *MrSIDRasterBand::GetOverview( int i )

{
    if( i < 0 || i >= poGDS->nOverviewCount )
        return NULL;
    else
        return poGDS->papoOverviewDS[i]->GetRasterBand( nBand );
}

/************************************************************************/
/*                           MrSIDDataset()                             */
/************************************************************************/

MrSIDDataset::MrSIDDataset(int bIsJPEG2000)
{
    poStream = NULL;
    poImageReader = NULL;
#ifdef MRSID_ESDK
    poImageWriter = NULL;
#endif
    poLTINav = NULL;
    poMetadata = NULL;
    poNDPixel = NULL;
    eSampleType = LTI_DATATYPE_UINT8;
    nBands = 0;
    eDataType = GDT_Byte;
    
    poBuffer = NULL;
    bPrevBlockRead = FALSE;
    nPrevBlockXOff = 0;
    nPrevBlockYOff = 0;
    
    psDefn = NULL;
    
    dfCurrentMag = 1.0;
    bIsOverview = FALSE;
    poParentDS = this;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    
    poDriver = (GDALDriver*) GDALGetDriverByName( bIsJPEG2000 ? "JP2MrSID" : "MrSID" );
}

/************************************************************************/
/*                            ~MrSIDDataset()                           */
/************************************************************************/

MrSIDDataset::~MrSIDDataset()
{
    FlushCache();

#ifdef MRSID_ESDK
    if ( poImageWriter )
        delete poImageWriter;
#endif

    if ( poBuffer )
        delete poBuffer;
    if ( poMetadata )
        delete poMetadata;
    if ( poLTINav )
        delete poLTINav;
    if ( poImageReader && !bIsOverview )
#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 7
    {
        poImageReader->release();
        poImageReader = NULL;
    }
#else
        delete poImageReader;
#endif
    // points to another member, don't delete
    poStream = NULL;

    if ( psDefn )
        delete psDefn;
    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int MrSIDDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();

    if ( papoOverviewDS )
    {
        for( int i = 0; i < nOverviewCount; i++ )
            delete papoOverviewDS[i];
        CPLFree( papoOverviewDS );
        papoOverviewDS = NULL;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr MrSIDDataset::IRasterIO( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType, 
                                int nBandCount, int *panBandMap,
                                int nPixelSpace, int nLineSpace, int nBandSpace )

{
/* -------------------------------------------------------------------- */
/*      We need various criteria to skip out to block based methods.    */
/* -------------------------------------------------------------------- */
    int bUseBlockedIO = bForceCachedIO;

    if( nYSize == 1 || nXSize * ((double) nYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( nBufYSize == 1 || nBufXSize * ((double) nBufYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( CSLTestBoolean( CPLGetConfigOption( "GDAL_ONE_BIG_READ", "NO") ) )
        bUseBlockedIO = FALSE;

    if( bUseBlockedIO )
        return GDALDataset::BlockBasedRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace );
    CPLDebug( "MrSID", "RasterIO() - using optimized dataset level IO." );
    
/* -------------------------------------------------------------------- */
/*      What is our requested window relative to the base dataset.      */
/*      We want to operate from here on as if we were operating on      */
/*      the full res band.                                              */
/* -------------------------------------------------------------------- */
    int nZoomMag = (int) ((1/dfCurrentMag) * 1.0000001);

    nXOff *= nZoomMag;
    nYOff *= nZoomMag;
    nXSize *= nZoomMag;
    nYSize *= nZoomMag;

/* -------------------------------------------------------------------- */
/*      We need to figure out the best zoom level to use for this       */
/*      request.  We apply a small fudge factor to make sure that       */
/*      request just very, very slightly larger than a zoom level do    */
/*      not force us to the next level.                                 */
/* -------------------------------------------------------------------- */
    int iOverview = 0;
    double dfZoomMag = MIN((nXSize / (double)nBufXSize), 
                           (nYSize / (double)nBufYSize));

    for( nZoomMag = 1; 
         nZoomMag * 2 < (dfZoomMag + 0.1) 
             && iOverview < poParentDS->nOverviewCount; 
         nZoomMag *= 2, iOverview++ ) {}

/* -------------------------------------------------------------------- */
/*      Work out the size of the temporary buffer and allocate it.      */
/*      The temporary buffer will generally be at a moderately          */
/*      higher resolution than the buffer of data requested.            */
/* -------------------------------------------------------------------- */
    int  nTmpPixelSize;
    LTIPixel       oPixel( eColorSpace, nBands, eSampleType );
    
    LT_STATUS eLTStatus;
    unsigned int maxWidth;
    unsigned int maxHeight;

    eLTStatus = poImageReader->getDimsAtMag(1.0/nZoomMag,maxWidth,maxHeight);

    if( !LT_SUCCESS(eLTStatus)) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDataset::IRasterIO(): Failed to get zoomed image dimensions.\n%s",
                  getLastStatusString( eLTStatus ) );
        return CE_Failure;
    }

    int maxWidthAtL0 = bIsOverview?poParentDS->GetRasterXSize():this->GetRasterXSize();
    int maxHeightAtL0 = bIsOverview?poParentDS->GetRasterYSize():this->GetRasterYSize();

    int sceneUlXOff = nXOff / nZoomMag;
    int sceneUlYOff = nYOff / nZoomMag;
    int sceneWidth  = (int)(nXSize * (double) maxWidth / (double)maxWidthAtL0 + 0.99);
    int sceneHeight = (int)(nYSize * (double) maxHeight / (double)maxHeightAtL0 + 0.99);

    if( (sceneUlXOff + sceneWidth) > (int) maxWidth )
        sceneWidth = maxWidth - sceneUlXOff;

    if( (sceneUlYOff + sceneHeight) > (int) maxHeight )
        sceneHeight = maxHeight - sceneUlYOff;

    LTISceneBuffer oLTIBuffer( oPixel, sceneWidth, sceneHeight, NULL );

    nTmpPixelSize = GDALGetDataTypeSize( eDataType ) / 8;

/* -------------------------------------------------------------------- */
/*      Create navigator, and move to the requested scene area.         */
/* -------------------------------------------------------------------- */
    LTINavigator oNav( *poImageReader );
    
    if( !LT_SUCCESS(oNav.setSceneAsULWH( sceneUlXOff, sceneUlYOff, 
                                         sceneWidth, sceneHeight, 
                                         1.0 / nZoomMag )) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDataset::IRasterIO(): Failed to set scene position." );

        return CE_Failure;
    }

    CPLDebug( "MrSID", 
              "Dataset:IRasterIO(%d,%d %dx%d -> %dx%d -> %dx%d, zoom=%d)",
              nXOff, nYOff, nXSize, nYSize, 
              sceneWidth, sceneHeight,
              nBufXSize, nBufYSize, 
              nZoomMag );

    if( !oNav.isSceneValid() )
        CPLDebug( "MrSID", "LTINavigator in invalid state." );

/* -------------------------------------------------------------------- */
/*      Read into the buffer.                                           */
/* -------------------------------------------------------------------- */

    eLTStatus = poImageReader->read(oNav.getScene(),oLTIBuffer);
    if(!LT_SUCCESS(eLTStatus) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDRasterBand::IRasterIO(): Failed to load image.\n%s",
                  getLastStatusString( eLTStatus ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If we are pulling the data at a matching resolution, try to     */
/*      do a more direct copy without subsampling.                      */
/* -------------------------------------------------------------------- */
    int         iBufLine, iBufPixel;

    if( nBufXSize == sceneWidth && nBufYSize == sceneHeight )
    {
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GByte *pabySrcBand = (GByte *) 
                oLTIBuffer.getTotalBandData( panBandMap[iBand] - 1 );
	  
            for( int iLine = 0; iLine < nBufYSize; iLine++ )
	    {
                GDALCopyWords( pabySrcBand + iLine*nTmpPixelSize*sceneWidth,
                               eDataType, nTmpPixelSize, 
                               ((GByte *)pData) + iLine*nLineSpace 
                               + iBand * nBandSpace, 
                               eBufType, nPixelSpace,
                               nBufXSize );
	    }
	}
    }

/* -------------------------------------------------------------------- */
/*      Manually resample to our target buffer.                         */
/* -------------------------------------------------------------------- */
    else
    {
        for( iBufLine = 0; iBufLine < nBufYSize; iBufLine++ )
	{
            int iTmpLine = (int) floor(((iBufLine+0.5) / nBufYSize)*sceneHeight);

            for( iBufPixel = 0; iBufPixel < nBufXSize; iBufPixel++ )
	    {
                int iTmpPixel = (int) 
                    floor(((iBufPixel+0.5) / nBufXSize) * sceneWidth);

                for( int iBand = 0; iBand < nBandCount; iBand++ )
		{
                    GByte *pabySrc, *pabyDst;

                    pabyDst = ((GByte *) pData) 
                        + nPixelSpace * iBufPixel
                        + nLineSpace * iBufLine
                        + nBandSpace * iBand;

                    pabySrc = (GByte *) oLTIBuffer.getTotalBandData( 
                        panBandMap[iBand] - 1 );
                    pabySrc += (iTmpLine * sceneWidth + iTmpPixel) * nTmpPixelSize;

                    if( eDataType == eBufType )
                        memcpy( pabyDst, pabySrc, nTmpPixelSize );
                    else
                        GDALCopyWords( pabySrc, eDataType, 0, 
                                       pabyDst, eBufType, 0, 1 );
		}
	    }
	}
    }

    return CE_None;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr MrSIDDataset::IBuildOverviews( const char *, int, int *,
                                      int, int *, GDALProgressFunc,
                                      void * )
{
	CPLError( CE_Warning, CPLE_AppDefined,
			  "MrSID overviews are built-in, so building external "
			  "overviews is unnecessary. Ignoring.\n" );

	return CE_None;
}

/************************************************************************/
/*                        SerializeMetadataRec()                        */
/************************************************************************/

char *MrSIDDataset::SerializeMetadataRec( const LTIMetadataRecord *poMetadataRec )
{
    GUInt32  iNumDims = 0;
    const GUInt32  *paiDims = NULL;
    const void     *pData = poMetadataRec->getArrayData( iNumDims, paiDims );
    GUInt32        i, j, k = 0, iLength;
    char           *pszMetadata = CPLStrdup( "" );

    for ( i = 0; i < iNumDims; i++ )
    {
        // stops on large binary data
        if ( poMetadataRec->getDataType() == LTI_METADATA_DATATYPE_UINT8 
             && paiDims[i] > 1024 )
            return pszMetadata;

        for ( j = 0; j < paiDims[i]; j++ )
        {
            CPLString osTemp;

            switch( poMetadataRec->getDataType() )
            {
                case LTI_METADATA_DATATYPE_UINT8:
                case LTI_METADATA_DATATYPE_SINT8:
                    osTemp.Printf( "%d", ((GByte *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_UINT16:
                    osTemp.Printf( "%u", ((GUInt16 *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_SINT16:
                    osTemp.Printf( "%d", ((GInt16 *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_UINT32:
                    osTemp.Printf( "%u", ((GUInt32 *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_SINT32:
                    osTemp.Printf( "%d", ((GInt32 *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_FLOAT32:
                    osTemp.Printf( "%f", ((float *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_FLOAT64:
                    osTemp.Printf( "%f", ((double *)pData)[k++] );
                    break;
                case LTI_METADATA_DATATYPE_ASCII:
                    osTemp = ((const char **)pData)[k++];
                    break;
                default:
                    osTemp = "";
                    break;
            }

            iLength = strlen(pszMetadata) + strlen(osTemp) + 2;

            pszMetadata = (char *)CPLRealloc( pszMetadata, iLength );
            if ( !EQUAL( pszMetadata, "" ) )
                strncat( pszMetadata, ",", 1 );
            strncat( pszMetadata, osTemp, iLength );
        }
    }

    return pszMetadata;
}

/************************************************************************/
/*                          GetMetadataElement()                        */
/************************************************************************/

int MrSIDDataset::GetMetadataElement( const char *pszKey, void *pValue,
                                      int iLength )
{
    if ( !poMetadata->has( pszKey ) )
        return FALSE;

    const LTIMetadataRecord *poMetadataRec = NULL;
    poMetadata->get( pszKey, poMetadataRec );

    if ( !poMetadataRec->isScalar() )
        return FALSE;

    // XXX: return FALSE if we have more than one element in metadata record
    int iSize;
    switch( poMetadataRec->getDataType() )
    {
         case LTI_METADATA_DATATYPE_UINT8:
         case LTI_METADATA_DATATYPE_SINT8:
             iSize = 1;
             break;
         case LTI_METADATA_DATATYPE_UINT16:
         case LTI_METADATA_DATATYPE_SINT16:
             iSize = 2;
             break;
         case LTI_METADATA_DATATYPE_UINT32:
         case LTI_METADATA_DATATYPE_SINT32:
         case LTI_METADATA_DATATYPE_FLOAT32:
             iSize = 4;
             break;
         case LTI_METADATA_DATATYPE_FLOAT64:
             iSize = 8;
             break;
         case LTI_METADATA_DATATYPE_ASCII:
             iSize = iLength;
             break;
         default:
             iSize = 0;
             break;
    }

    if ( poMetadataRec->getDataType() == LTI_METADATA_DATATYPE_ASCII )
    {
        strncpy( (char *)pValue,
                 ((const char**)poMetadataRec->getScalarData())[0], iSize );
        ((char *)pValue)[iSize - 1] = '\0';
    }
    else
        memcpy( pValue, poMetadataRec->getScalarData(), iSize );

    return TRUE;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** MrSIDDataset::GetFileList()
{
    char** papszFileList = GDALPamDataset::GetFileList();

    if (osMETFilename.size() != 0)
        papszFileList = CSLAddString(papszFileList, osMETFilename.c_str());

    return papszFileList;
}

/************************************************************************/
/*                             OpenZoomLevel()                          */
/************************************************************************/

CPLErr MrSIDDataset::OpenZoomLevel( lt_int32 iZoom )
{
/* -------------------------------------------------------------------- */
/*      Get image geometry.                                            */
/* -------------------------------------------------------------------- */
    if ( iZoom != 0 )
    {
        lt_uint32 iWidth, iHeight;
        dfCurrentMag = LTIUtils::levelToMag( iZoom );
        poImageReader->getDimsAtMag( dfCurrentMag, iWidth, iHeight );
        nRasterXSize = iWidth;
        nRasterYSize = iHeight;
    }
    else
    {
        dfCurrentMag = 1.0;
        nRasterXSize = poImageReader->getWidth();
        nRasterYSize = poImageReader->getHeight();
    }

    nBands = poImageReader->getNumBands();
    nBlockXSize = nRasterXSize;
    nBlockYSize = poImageReader->getStripHeight();

    CPLDebug( "MrSID", "Opened zoom level %d with size %dx%d.",
              iZoom, nRasterXSize, nRasterYSize );

    try
    {
        poLTINav = new LTIDLLNavigator<LTINavigator>( *poImageReader );
    }
    catch ( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDataset::OpenZoomLevel(): "
                  "Failed to create LTINavigator object." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*  Handle sample type and color space.                                 */
/* -------------------------------------------------------------------- */
    eColorSpace = poImageReader->getColorSpace();
    eSampleType = poImageReader->getDataType();
    switch ( eSampleType )
    {
      case LTI_DATATYPE_UINT16:
        eDataType = GDT_UInt16;
        break;
      case LTI_DATATYPE_SINT16:
        eDataType = GDT_Int16;
        break;
      case LTI_DATATYPE_UINT32:
        eDataType = GDT_UInt32;
        break;
      case LTI_DATATYPE_SINT32:
        eDataType = GDT_Int32;
        break;
      case LTI_DATATYPE_FLOAT32:
        eDataType = GDT_Float32;
        break;
      case LTI_DATATYPE_FLOAT64:
        eDataType = GDT_Float64;
        break;
      case LTI_DATATYPE_UINT8:
      case LTI_DATATYPE_SINT8:
      default:
        eDataType = GDT_Byte;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Read georeferencing.                                            */
/* -------------------------------------------------------------------- */
    if ( !poImageReader->isGeoCoordImplicit() )
    {
        const LTIGeoCoord& oGeo = poImageReader->getGeoCoord();
        oGeo.get( adfGeoTransform[0], adfGeoTransform[3],
                  adfGeoTransform[1], adfGeoTransform[5],
                  adfGeoTransform[2], adfGeoTransform[4] );
        
        adfGeoTransform[0] = adfGeoTransform[0] - adfGeoTransform[1] / 2;
        adfGeoTransform[3] = adfGeoTransform[3] - adfGeoTransform[5] / 2;
        bGeoTransformValid = TRUE;
    }
    else if( iZoom == 0 )
    {
        bGeoTransformValid = 
            GDALReadWorldFile( GetDescription(), NULL,
                               adfGeoTransform )
            || GDALReadWorldFile( GetDescription(), ".wld",
                                  adfGeoTransform );
    }
    
/* -------------------------------------------------------------------- */
/*      Read wkt.                                                       */
/* -------------------------------------------------------------------- */
#ifdef MRSID_HAVE_GETWKT
    if( !poImageReader->isGeoCoordImplicit() )
    {
        const LTIGeoCoord& oGeo = poImageReader->getGeoCoord();
        
        if( oGeo.getWKT() )
        {
            /* Workaround probable issue with GeoDSK 7 on 64bit Linux */
            if (!(pszProjection != NULL && !EQUALN(pszProjection, "LOCAL_CS", 8)
                && EQUALN( oGeo.getWKT(), "LOCAL_CS", 8)))
            {
                CPLFree( pszProjection );
                pszProjection =  CPLStrdup( oGeo.getWKT() );
            }
        }
    }
#endif // HAVE_MRSID_GETWKT

/* -------------------------------------------------------------------- */
/*      Special case for https://zulu.ssc.nasa.gov/mrsid/mrsid.pl       */
/*      where LandSat .SID are accompanied by a .met file with the      */
/*      projection                                                      */
/* -------------------------------------------------------------------- */
    if (iZoom == 0 && (pszProjection == NULL || pszProjection[0] == '\0') &&
        EQUAL(CPLGetExtension(GetDescription()), "sid"))
    {
        const char* pszMETFilename = CPLResetExtension(GetDescription(), "met");
        VSILFILE* fp = VSIFOpenL(pszMETFilename, "rb");
        if (fp)
        {
            const char* pszLine;
            int nCountLine = 0;
            int nUTMZone = 0;
            int bWGS84 = FALSE;
            int bUnitsMeter = FALSE;
            while ( (pszLine = CPLReadLine2L(fp, 200, NULL)) != NULL &&
                    ++nCountLine < 1000 )
            {
                if (nCountLine == 1 && strcmp(pszLine, "::MetadataFile") != 0)
                    break;
                if (EQUALN(pszLine, "Projection UTM ", 15))
                    nUTMZone = atoi(pszLine + 15);
                else if (EQUAL(pszLine, "Datum WGS84"))
                    bWGS84 = TRUE;
                else if (EQUAL(pszLine, "Units Meters"))
                    bUnitsMeter = TRUE;
            }
            VSIFCloseL(fp);

            /* Images in southern hemisphere have negative northings in the */
            /* .sdw file. A bit weird, but anyway we must use the northern */
            /* UTM SRS for consistency */
            if (nUTMZone >= 1 && nUTMZone <= 60 && bWGS84 && bUnitsMeter)
            {
                osMETFilename = pszMETFilename;
                
                OGRSpatialReference oSRS;
                oSRS.importFromEPSG(32600 + nUTMZone);
                CPLFree(pszProjection);
                pszProjection = NULL;
                oSRS.exportToWkt(&pszProjection);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Read NoData value.                                              */
/* -------------------------------------------------------------------- */
    poNDPixel = poImageReader->getNoDataPixel();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int             iBand;

    for( iBand = 1; iBand <= nBands; iBand++ )
        SetBand( iBand, new MrSIDRasterBand( this, iBand ) );

    return CE_None;
}

/************************************************************************/
/*                         MrSIDIdentify()                              */
/*                                                                      */
/*          Identify method that only supports MrSID files.             */
/************************************************************************/

static int MrSIDIdentify( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 32 )
        return FALSE;

    if ( !EQUALN((const char *) poOpenInfo->pabyHeader, "msid", 4) )
        return FALSE;

#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
    lt_uint8 gen;
    bool raster;
    LT_STATUS eStat =
        MrSIDImageReaderInterface::getMrSIDGeneration(poOpenInfo->pabyHeader, gen, raster);
    if (!LT_SUCCESS(eStat) || !raster)
       return FALSE;
#endif

    return TRUE;
}

/************************************************************************/
/*                          MrSIDOpen()                                 */
/*                                                                      */
/*          Open method that only supports MrSID files.                 */
/************************************************************************/

static GDALDataset* MrSIDOpen( GDALOpenInfo *poOpenInfo )
{
    if (!MrSIDIdentify(poOpenInfo))
        return NULL;
        
    return MrSIDDataset::Open( poOpenInfo, FALSE );
}


#ifdef MRSID_J2K

static const unsigned char jpc_header[] = 
{0xff,0x4f};

/************************************************************************/
/*                         JP2Identify()                                */
/*                                                                      */
/*        Identify method that only supports JPEG2000 files.            */
/************************************************************************/

static int JP2Identify( GDALOpenInfo *poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 32 )
        return FALSE;

    if( memcmp( poOpenInfo->pabyHeader, jpc_header, sizeof(jpc_header) ) == 0 )
    {
        const char *pszExtension;

        pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
        
        if( !EQUAL(pszExtension,"jpc") && !EQUAL(pszExtension,"j2k") 
            && !EQUAL(pszExtension,"jp2") && !EQUAL(pszExtension,"jpx") 
            && !EQUAL(pszExtension,"j2c") && !EQUAL(pszExtension,"ntf"))
            return FALSE;
    }
    else if( !EQUALN((const char *) poOpenInfo->pabyHeader + 4, "jP  ", 4) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                            JP2Open()                                 */
/*                                                                      */
/*      Open method that only supports JPEG2000 files.                  */
/************************************************************************/

static GDALDataset* JP2Open( GDALOpenInfo *poOpenInfo )
{
    if (!JP2Identify(poOpenInfo))
        return NULL;
        
    return MrSIDDataset::Open( poOpenInfo, TRUE );
}

#endif // MRSID_J2K

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MrSIDDataset::Open( GDALOpenInfo * poOpenInfo, int bIsJP2 )
{
    if(poOpenInfo->fpL)
    {
        VSIFCloseL( poOpenInfo->fpL );
        poOpenInfo->fpL = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Make sure we have hooked CSV lookup for GDAL_DATA.              */
/* -------------------------------------------------------------------- */
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MrSIDDataset    *poDS;
    LT_STATUS       eStat;

    poDS = new MrSIDDataset(bIsJP2);

    // try the LTIOFileStream first, since it uses filesystem caching
    eStat = poDS->oLTIStream.initialize( poOpenInfo->pszFilename, "rb" );
    if ( LT_SUCCESS(eStat) )
    {
        eStat = poDS->oLTIStream.open();
        if ( LT_SUCCESS(eStat) )
            poDS->poStream = &(poDS->oLTIStream);
    }

    // fall back on VSI for non-files
    if ( !LT_SUCCESS(eStat) || !poDS->poStream )
    {
        eStat = poDS->oVSIStream.initialize( poOpenInfo->pszFilename, "rb" );
        if ( !LT_SUCCESS(eStat) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "LTIVSIStream::initialize(): "
                      "failed to open file \"%s\".\n%s",
                      poOpenInfo->pszFilename, getLastStatusString( eStat ) );
            delete poDS;
            return NULL;
        }

        eStat = poDS->oVSIStream.open();
        if ( !LT_SUCCESS(eStat) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "LTIVSIStream::open(): "
                      "failed to open file \"%s\".\n%s",
                      poOpenInfo->pszFilename, getLastStatusString( eStat ) );
            delete poDS;
            return NULL;
        }

        poDS->poStream = &(poDS->oVSIStream);
    }

#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 7

#ifdef MRSID_J2K
    if ( bIsJP2 )
    {
        J2KImageReader  *reader = J2KImageReader::create();
        eStat = reader->initialize( *(poDS->poStream) );
        poDS->poImageReader = reader;
    }
    else
#endif /* MRSID_J2K */
    {
        MrSIDImageReader    *reader = MrSIDImageReader::create();
        eStat = reader->initialize( poDS->poStream, NULL );
        poDS->poImageReader = reader;           
    }

#else /* LTI_SDK_MAJOR < 7 */

#ifdef MRSID_J2K
    if ( bIsJP2 )
    {
        poDS->poImageReader =
            new LTIDLLReader<J2KImageReader>( *(poDS->poStream), true );
        eStat = poDS->poImageReader->initialize();
    }
    else
#endif /* MRSID_J2K */
    {
        poDS->poImageReader =
            new LTIDLLReader<MrSIDImageReader>( poDS->poStream, NULL );
        eStat = poDS->poImageReader->initialize();
    }

#endif /* LTI_SDK_MAJOR >= 7 */

    if ( !LT_SUCCESS(eStat) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "LTIImageReader::initialize(): "
                  "failed to initialize reader from the stream \"%s\".\n%s",
                  poOpenInfo->pszFilename, getLastStatusString( eStat ) );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read metadata.                                                  */
/* -------------------------------------------------------------------- */
    poDS->poMetadata = new LTIDLLCopy<LTIMetadataDatabase>(
        poDS->poImageReader->getMetadata() );
    const GUInt32       iNumRecs = poDS->poMetadata->getIndexCount();
    GUInt32             i;

    for ( i = 0; i < iNumRecs; i++ )
    {
        const LTIMetadataRecord *poMetadataRec = NULL;
        if ( LT_SUCCESS(poDS->poMetadata->getDataByIndex(i, poMetadataRec)) )
        {
            char    *pszElement = poDS->SerializeMetadataRec( poMetadataRec );
            char    *pszKey = CPLStrdup( poMetadataRec->getTagName() );
            char    *pszTemp = pszKey;

            // GDAL metadata keys should not contain ':' and '=' characters.
            // We will replace them with '_'.
            do
            {
                if ( *pszTemp == ':' || *pszTemp == '=' )
                    *pszTemp = '_';
            }
            while ( *++pszTemp );

            poDS->SetMetadataItem( pszKey, pszElement );

            CPLFree( pszElement );
            CPLFree( pszKey );
        }
    }

/* -------------------------------------------------------------------- */
/*      Add MrSID version.                                              */
/* -------------------------------------------------------------------- */
#ifdef MRSID_J2K
    if( !bIsJP2 )
#endif
    {
#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
        lt_uint8 gen;
        bool raster;
        MrSIDImageReaderInterface::getMrSIDGeneration(poOpenInfo->pabyHeader, gen, raster);
        poDS->SetMetadataItem( "VERSION", CPLString().Printf("MG%d%s", gen, raster ? "" : " LiDAR") );
#else
        lt_uint8 major;
        lt_uint8 minor;
        char letter;
        MrSIDImageReader* poMrSIDImageReader = (MrSIDImageReader*)poDS->poImageReader;
        poMrSIDImageReader->getVersion(major, minor, minor, letter);
        if (major < 2) 
            major = 2;
        poDS->SetMetadataItem( "VERSION", CPLString().Printf("MG%d", major) );
#endif
    }

    poDS->GetGTIFDefn();
    
/* -------------------------------------------------------------------- */
/*      Get number of resolution levels (we will use them as overviews).*/
/* -------------------------------------------------------------------- */
#ifdef MRSID_J2K
    if( bIsJP2 )
        poDS->nOverviewCount
            = ((J2KImageReader *) (poDS->poImageReader))->getNumLevels();
    else
#endif
        poDS->nOverviewCount
            = ((MrSIDImageReader *) (poDS->poImageReader))->getNumLevels();

    if ( poDS->nOverviewCount > 0 )
    {
        lt_int32        i;

        poDS->papoOverviewDS = (MrSIDDataset **)
            CPLMalloc( poDS->nOverviewCount * (sizeof(void*)) );

        for ( i = 0; i < poDS->nOverviewCount; i++ )
        {
            poDS->papoOverviewDS[i] = new MrSIDDataset(bIsJP2);
            poDS->papoOverviewDS[i]->poImageReader = poDS->poImageReader;
            poDS->papoOverviewDS[i]->OpenZoomLevel( i + 1 );
            poDS->papoOverviewDS[i]->bIsOverview = TRUE;
            poDS->papoOverviewDS[i]->poParentDS = poDS;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create object for the whole image.                              */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->OpenZoomLevel( 0 );

    CPLDebug( "MrSID",
              "Opened image: width %d, height %d, bands %d",
              poDS->nRasterXSize, poDS->nRasterYSize, poDS->nBands );

    if( poDS->nBands > 1 )
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );

    if (bIsJP2)
    {
        poDS->LoadJP2Metadata(poOpenInfo);
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Initialize the overview manager for mask band support.          */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                    EPSGProjMethodToCTProjMethod()                    */
/*                                                                      */
/*      Convert between the EPSG enumeration for projection methods,    */
/*      and the GeoTIFF CT codes.                                       */
/*      Explicitly copied from geo_normalize.c of the GeoTIFF package   */
/************************************************************************/

static int EPSGProjMethodToCTProjMethod( int nEPSG )

{
    /* see trf_method.csv for list of EPSG codes */
    
    switch( nEPSG )
    {
      case 9801:
        return( CT_LambertConfConic_1SP );

      case 9802:
        return( CT_LambertConfConic_2SP );

      case 9803:
        return( CT_LambertConfConic_2SP ); /* Belgian variant not supported */

      case 9804:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9805:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9806:
        return( CT_CassiniSoldner );

      case 9807:
        return( CT_TransverseMercator );

      case 9808:
        return( CT_TransvMercator_SouthOriented );

      case 9809:
        return( CT_ObliqueStereographic );

      case 9810:
        return( CT_PolarStereographic );

      case 9811:
        return( CT_NewZealandMapGrid );

      case 9812:
        return( CT_ObliqueMercator ); /* is hotine actually different? */

      case 9813:
        return( CT_ObliqueMercator_Laborde );

      case 9814:
        return( CT_ObliqueMercator_Rosenmund ); /* swiss  */

      case 9815:
        return( CT_ObliqueMercator );

      case 9816: /* tunesia mining grid has no counterpart */
        return( KvUserDefined );
    }

    return( KvUserDefined );
}

/* EPSG Codes for projection parameters.  Unfortunately, these bear no
   relationship to the GeoTIFF codes even though the names are so similar. */

#define EPSGNatOriginLat         8801
#define EPSGNatOriginLong        8802
#define EPSGNatOriginScaleFactor 8805
#define EPSGFalseEasting         8806
#define EPSGFalseNorthing        8807
#define EPSGProjCenterLat        8811
#define EPSGProjCenterLong       8812
#define EPSGAzimuth              8813
#define EPSGAngleRectifiedToSkewedGrid 8814
#define EPSGInitialLineScaleFactor 8815
#define EPSGProjCenterEasting    8816
#define EPSGProjCenterNorthing   8817
#define EPSGPseudoStdParallelLat 8818
#define EPSGPseudoStdParallelScaleFactor 8819
#define EPSGFalseOriginLat       8821
#define EPSGFalseOriginLong      8822
#define EPSGStdParallel1Lat      8823
#define EPSGStdParallel2Lat      8824
#define EPSGFalseOriginEasting   8826
#define EPSGFalseOriginNorthing  8827
#define EPSGSphericalOriginLat   8828
#define EPSGSphericalOriginLong  8829
#define EPSGInitialLongitude     8830
#define EPSGZoneWidth            8831

/************************************************************************/
/*                            SetGTParmIds()                            */
/*                                                                      */
/*      This is hardcoded logic to set the GeoTIFF parameter            */
/*      identifiers for all the EPSG supported projections.  As the     */
/*      trf_method.csv table grows with new projections, this code      */
/*      will need to be updated.                                        */
/*      Explicitly copied from geo_normalize.c of the GeoTIFF package.  */
/************************************************************************/

static int SetGTParmIds( int nCTProjection, 
                         int *panProjParmId, 
                         int *panEPSGCodes )

{
    int anWorkingDummy[7];

    if( panEPSGCodes == NULL )
        panEPSGCodes = anWorkingDummy;
    if( panProjParmId == NULL )
        panProjParmId = anWorkingDummy;

    memset( panEPSGCodes, 0, sizeof(int) * 7 );

    /* psDefn->nParms = 7; */
    
    switch( nCTProjection )
    {
      case CT_CassiniSoldner:
      case CT_NewZealandMapGrid:
        panProjParmId[0] = ProjNatOriginLatGeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_ObliqueMercator:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[2] = ProjAzimuthAngleGeoKey;
        panProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        panProjParmId[4] = ProjScaleAtCenterGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[3] = EPSGAngleRectifiedToSkewedGrid;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting;
        panEPSGCodes[6] = EPSGProjCenterNorthing;
        return TRUE;

      case CT_ObliqueMercator_Laborde:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[2] = ProjAzimuthAngleGeoKey;
        panProjParmId[4] = ProjScaleAtCenterGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting;
        panEPSGCodes[6] = EPSGProjCenterNorthing;
        return TRUE;
        
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_PolarStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
        panProjParmId[0] = ProjNatOriginLatGeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[4] = EPSGNatOriginScaleFactor;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_LambertConfConic_2SP:
        panProjParmId[0] = ProjFalseOriginLatGeoKey;
        panProjParmId[1] = ProjFalseOriginLongGeoKey;
        panProjParmId[2] = ProjStdParallel1GeoKey;
        panProjParmId[3] = ProjStdParallel2GeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGFalseOriginLat;
        panEPSGCodes[1] = EPSGFalseOriginLong;
        panEPSGCodes[2] = EPSGStdParallel1Lat;
        panEPSGCodes[3] = EPSGStdParallel2Lat;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_SwissObliqueCylindrical:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        /* EPSG codes? */
        return TRUE;

      default:
        return( FALSE );
    }
}

static const char *papszDatumEquiv[] =
{
    "Militar_Geographische_Institut",
    "Militar_Geographische_Institute",
    "World_Geodetic_System_1984",
    "WGS_1984",
    "WGS_72_Transit_Broadcast_Ephemeris",
    "WGS_1972_Transit_Broadcast_Ephemeris",
    "World_Geodetic_System_1972",
    "WGS_1972",
    "European_Terrestrial_Reference_System_89",
    "European_Reference_System_1989",
    NULL
};

/************************************************************************/
/*                          WKTMassageDatum()                           */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/*      Explicitly copied from the gt_wkt_srs.cpp.                      */
/************************************************************************/

static void WKTMassageDatum( char ** ppszDatum )

{
    int         i, j;
    char        *pszDatum = *ppszDatum;

    if (pszDatum[0] == '\0')
        return;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
            && !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
            && !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
        {
            pszDatum[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    for( i = 1, j = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[j] == '_' && pszDatum[i] == '_' )
            continue;

        pszDatum[++j] = pszDatum[i];
    }
    if( pszDatum[j] == '_' )
        pszDatum[j] = '\0';
    else
        pszDatum[j+1] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Search for datum equivelences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
    {
        if( EQUAL(*ppszDatum,papszDatumEquiv[i]) )
        {
            CPLFree( *ppszDatum );
            *ppszDatum = CPLStrdup( papszDatumEquiv[i+1] );
            return;
        }
    }
}

/************************************************************************/
/*                           FetchProjParms()                           */
/*                                                                      */
/*      Fetch the projection parameters for a particular projection     */
/*      from MrSID metadata, and fill the GTIFDefn structure out        */
/*      with them.                                                      */
/*      Copied from geo_normalize.c of the GeoTIFF package.             */
/************************************************************************/

void MrSIDDataset::FetchProjParms()
{
    double dfNatOriginLong = 0.0, dfNatOriginLat = 0.0, dfRectGridAngle = 0.0;
    double dfFalseEasting = 0.0, dfFalseNorthing = 0.0, dfNatOriginScale = 1.0;
    double dfStdParallel1 = 0.0, dfStdParallel2 = 0.0, dfAzimuth = 0.0;

/* -------------------------------------------------------------------- */
/*      Get the false easting, and northing if available.               */
/* -------------------------------------------------------------------- */
    if( !GetMetadataElement( "GEOTIFF_NUM::3082::ProjFalseEastingGeoKey",
                             &dfFalseEasting )
        && !GetMetadataElement( "GEOTIFF_NUM::3090:ProjCenterEastingGeoKey",
                                &dfFalseEasting ) )
        dfFalseEasting = 0.0;
        
    if( !GetMetadataElement( "GEOTIFF_NUM::3083::ProjFalseNorthingGeoKey",
                             &dfFalseNorthing )
        && !GetMetadataElement( "GEOTIFF_NUM::3091::ProjCenterNorthingGeoKey",
                                &dfFalseNorthing ) )
        dfFalseNorthing = 0.0;

    switch( psDefn->CTProjection )
    {
/* -------------------------------------------------------------------- */
      case CT_Stereographic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey",
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_ObliqueMercator: /* hotine */
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3094::ProjAzimuthAngleGeoKey",
                                &dfAzimuth ) == 0 )
            dfAzimuth = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3096::ProjRectifiedGridAngleGeoKey",
                                &dfRectGridAngle ) == 0 )
            dfRectGridAngle = 90.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3093::ProjScaleAtCenterGeoKey",
                                   &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfAzimuth;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParm[3] = dfRectGridAngle;
        psDefn->ProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CassiniSoldner:
      case CT_Polyconic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;


        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3093::ProjScaleAtCenterGeoKey",
                                   &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AzimuthalEquidistant:
      case CT_MillerCylindrical:
      case CT_Equirectangular:
      case CT_Gnomonic:
      case CT_LambertAzimEqualArea:
      case CT_Orthographic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Robinson:
      case CT_Sinusoidal:
      case CT_VanDerGrinten:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_PolarStereographic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3095::ProjStraightVertPoleLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey",
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3093::ProjScaleAtCenterGeoKey",
                                   &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjStraightVertPoleLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_2SP:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3078::ProjStdParallel1GeoKey",
                                &dfStdParallel1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3079::ProjStdParallel2GeoKey",
                                &dfStdParallel2 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjFalseOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjFalseOriginLongGeoKey;
        psDefn->ProjParm[2] = dfStdParallel1;
        psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[3] = dfStdParallel2;
        psDefn->ProjParmId[3] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AlbersEqualArea:
      case CT_EquidistantConic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3078::ProjStdParallel1GeoKey",
                                &dfStdParallel1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3079::ProjStdParallel2GeoKey",
                                &dfStdParallel2 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfStdParallel2;
        psDefn->ProjParmId[1] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[2] = dfNatOriginLat;
        psDefn->ProjParmId[2] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[3] = dfNatOriginLong;
        psDefn->ProjParmId[3] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;
    }
}

/************************************************************************/
/*                            GetGTIFDefn()                             */
/*      This function borrowed from the GTIFGetDefn() function.         */
/*      See geo_normalize.c from the GeoTIFF package.                   */
/************************************************************************/

void MrSIDDataset::GetGTIFDefn()
{
    double      dfInvFlattening;

/* -------------------------------------------------------------------- */
/*      Make sure we have hooked CSV lookup for GDAL_DATA.              */
/* -------------------------------------------------------------------- */
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Initially we default all the information we can.                */
/* -------------------------------------------------------------------- */
    psDefn = new( GTIFDefn );
    psDefn->Model = KvUserDefined;
    psDefn->PCS = KvUserDefined;
    psDefn->GCS = KvUserDefined;
    psDefn->UOMLength = KvUserDefined;
    psDefn->UOMLengthInMeters = 1.0;
    psDefn->UOMAngle = KvUserDefined;
    psDefn->UOMAngleInDegrees = 1.0;
    psDefn->Datum = KvUserDefined;
    psDefn->Ellipsoid = KvUserDefined;
    psDefn->SemiMajor = 0.0;
    psDefn->SemiMinor = 0.0;
    psDefn->PM = KvUserDefined;
    psDefn->PMLongToGreenwich = 0.0;

    psDefn->ProjCode = KvUserDefined;
    psDefn->Projection = KvUserDefined;
    psDefn->CTProjection = KvUserDefined;

    psDefn->nParms = 0;
    for( int i = 0; i < MAX_GTIF_PROJPARMS; i++ )
    {
        psDefn->ProjParm[i] = 0.0;
        psDefn->ProjParmId[i] = 0;
    }

    psDefn->MapSys = KvUserDefined;
    psDefn->Zone = 0;

/* -------------------------------------------------------------------- */
/*      Try to get the overall model type.                              */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::1024::GTModelTypeGeoKey",
                        &(psDefn->Model) );

/* -------------------------------------------------------------------- */
/*      Try to get a PCS.                                               */
/* -------------------------------------------------------------------- */
    if( GetMetadataElement( "GEOTIFF_NUM::3072::ProjectedCSTypeGeoKey",
                            &(psDefn->PCS) )
        && psDefn->PCS != KvUserDefined )
    {
        /*
         * Translate this into useful information.
         */
        GTIFGetPCSInfo( psDefn->PCS, NULL, &(psDefn->ProjCode),
                        &(psDefn->UOMLength), &(psDefn->GCS) );
    }

/* -------------------------------------------------------------------- */
/*       If we have the PCS code, but didn't find it in the CSV files   */
/*      (likely because we can't find them) we will try some ``jiffy    */
/*      rules'' for UTM and state plane.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined && psDefn->ProjCode == KvUserDefined )
    {
        int     nMapSys, nZone;
        int     nGCS = psDefn->GCS;

        nMapSys = GTIFPCSToMapSys( psDefn->PCS, &nGCS, &nZone );
        if( nMapSys != KvUserDefined )
        {
            psDefn->ProjCode = (short) GTIFMapSysToProj( nMapSys, nZone );
            psDefn->GCS = (short) nGCS;
        }
    }
   
/* -------------------------------------------------------------------- */
/*      If the Proj_ code is specified directly, use that.              */
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode == KvUserDefined )
        GetMetadataElement( "GEOTIFF_NUM::3074::ProjectionGeoKey",
                            &(psDefn->ProjCode) );
    
    if( psDefn->ProjCode != KvUserDefined )
    {
        /*
         * We have an underlying projection transformation value.  Look
         * this up.  For a PCS of ``WGS 84 / UTM 11'' the transformation
         * would be Transverse Mercator, with a particular set of options.
         * The nProjTRFCode itself would correspond to the name
         * ``UTM zone 11N'', and doesn't include datum info.
         */
        GTIFGetProjTRFInfo( psDefn->ProjCode, NULL, &(psDefn->Projection),
                            psDefn->ProjParm );
        
        /*
         * Set the GeoTIFF identity of the parameters.
         */
        psDefn->CTProjection = (short)
            EPSGProjMethodToCTProjMethod( psDefn->Projection );

        SetGTParmIds( psDefn->CTProjection, psDefn->ProjParmId, NULL);
        psDefn->nParms = 7;
    }

/* -------------------------------------------------------------------- */
/*      Try to get a GCS.  If found, it will override any implied by    */
/*      the PCS.                                                        */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2048::GeographicTypeGeoKey",
                        &(psDefn->GCS) );

/* -------------------------------------------------------------------- */
/*      Derive the datum, and prime meridian from the GCS.              */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        GTIFGetGCSInfo( psDefn->GCS, NULL, &(psDefn->Datum), &(psDefn->PM),
                        &(psDefn->UOMAngle) );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle the GCS angular units.  GeogAngularUnitsGeoKey           */
/*      overrides the GCS or PCS setting.                               */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2054::GeogAngularUnitsGeoKey",
                        &(psDefn->UOMAngle) );
    if( psDefn->UOMAngle != KvUserDefined )
    {
        GTIFGetUOMAngleInfo( psDefn->UOMAngle, NULL,
                             &(psDefn->UOMAngleInDegrees) );
    }

/* -------------------------------------------------------------------- */
/*      Check for a datum setting, and then use the datum to derive     */
/*      an ellipsoid.                                                   */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2050::GeogGeodeticDatumGeoKey",
                        &(psDefn->Datum) );

    if( psDefn->Datum != KvUserDefined )
    {
        GTIFGetDatumInfo( psDefn->Datum, NULL, &(psDefn->Ellipsoid) );
    }

/* -------------------------------------------------------------------- */
/*      Check for an explicit ellipsoid.  Use the ellipsoid to          */
/*      derive the ellipsoid characteristics, if possible.              */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2056::GeogEllipsoidGeoKey",
                        &(psDefn->Ellipsoid) );

    if( psDefn->Ellipsoid != KvUserDefined )
    {
        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, NULL,
                              &(psDefn->SemiMajor), &(psDefn->SemiMinor) );
    }

/* -------------------------------------------------------------------- */
/*      Check for overridden ellipsoid parameters.  It would be nice    */
/*      to warn if they conflict with provided information, but for     */
/*      now we just override.                                           */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2057::GeogSemiMajorAxisGeoKey",
                        &(psDefn->SemiMajor) );
    GetMetadataElement( "GEOTIFF_NUM::2058::GeogSemiMinorAxisGeoKey",
                        &(psDefn->SemiMinor) );
    
    if( GetMetadataElement( "GEOTIFF_NUM::2059::GeogInvFlatteningGeoKey",
                            &dfInvFlattening ) == 1 )
    {
        if( dfInvFlattening != 0.0 )
            psDefn->SemiMinor = 
                psDefn->SemiMajor * (1 - 1.0/dfInvFlattening);
    }

/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2051::GeogPrimeMeridianGeoKey",
                        &(psDefn->PM) );

    if( psDefn->PM != KvUserDefined )
    {
        GTIFGetPMInfo( psDefn->PM, NULL, &(psDefn->PMLongToGreenwich) );
    }
    else
    {
        GetMetadataElement( "GEOTIFF_NUM::2061::GeogPrimeMeridianLongGeoKey",
                            &(psDefn->PMLongToGreenwich) );

        psDefn->PMLongToGreenwich =
            GTIFAngleToDD( psDefn->PMLongToGreenwich,
                           psDefn->UOMAngle );
    }

/* -------------------------------------------------------------------- */
/*      Have the projection units of measure been overridden?  We       */
/*      should likely be doing something about angular units too,       */
/*      but these are very rarely not decimal degrees for actual        */
/*      file coordinates.                                               */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::3076::ProjLinearUnitsGeoKey",
                        &(psDefn->UOMLength) );

    if( psDefn->UOMLength != KvUserDefined )
    {
        GTIFGetUOMLengthInfo( psDefn->UOMLength, NULL,
                              &(psDefn->UOMLengthInMeters) );
    }

/* -------------------------------------------------------------------- */
/*      Handle a variety of user defined transform types.               */
/* -------------------------------------------------------------------- */
    if( GetMetadataElement( "GEOTIFF_NUM::3075::ProjCoordTransGeoKey",
                            &(psDefn->CTProjection) ) )
    {
        FetchProjParms();
    }

/* -------------------------------------------------------------------- */
/*      Try to set the zoned map system information.                    */
/* -------------------------------------------------------------------- */
    psDefn->MapSys = GTIFProjToMapSys( psDefn->ProjCode, &(psDefn->Zone) );

/* -------------------------------------------------------------------- */
/*      If this is UTM, and we were unable to extract the projection    */
/*      parameters from the CSV file, just set them directly now,       */
/*      since it's pretty easy, and a common case.                      */
/* -------------------------------------------------------------------- */
    if( (psDefn->MapSys == MapSys_UTM_North
         || psDefn->MapSys == MapSys_UTM_South)
        && psDefn->CTProjection == KvUserDefined )
    {
        psDefn->CTProjection = CT_TransverseMercator;
        psDefn->nParms = 7;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[0] = 0.0;
            
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[1] = psDefn->Zone*6 - 183.0;
        
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[4] = 0.9996;
        
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[5] = 500000.0;
        
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        if( psDefn->MapSys == MapSys_UTM_North )
            psDefn->ProjParm[6] = 0.0;
        else
            psDefn->ProjParm[6] = 10000000.0;
    }

    if ( pszProjection )
        CPLFree( pszProjection );
    pszProjection = GetOGISDefn( psDefn );
}


/************************************************************************/
/*                       GTIFToCPLRecyleString()                        */
/*                                                                      */
/*      This changes a string from the libgeotiff heap to the GDAL      */
/*      heap.                                                           */
/************************************************************************/

static void GTIFToCPLRecycleString( char **ppszTarget )

{
    if( *ppszTarget == NULL )
        return;

    char *pszTempString = CPLStrdup(*ppszTarget);
    GTIFFreeMemory( *ppszTarget );
    *ppszTarget = pszTempString;
}

/************************************************************************/
/*                          GetOGISDefn()                               */
/*  Copied from the gt_wkt_srs.cpp.                                     */
/************************************************************************/

char *MrSIDDataset::GetOGISDefn( GTIFDefn *psDefn )
{
    OGRSpatialReference oSRS;

    if( psDefn->Model != ModelTypeProjected 
        && psDefn->Model != ModelTypeGeographic )
        return CPLStrdup("");
    
/* -------------------------------------------------------------------- */
/*      If this is a projected SRS we set the PROJCS keyword first      */
/*      to ensure that the GEOGCS will be a child.                      */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeProjected )
    {
        char    *pszPCSName;
        int     bPCSNameSet = FALSE;

        if( psDefn->PCS != KvUserDefined )
        {

            if( GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL ) )
                bPCSNameSet = TRUE;
            
            oSRS.SetNode( "PROJCS", bPCSNameSet ? pszPCSName : "unnamed" );
            if( bPCSNameSet )
                GTIFFreeMemory( pszPCSName );

            oSRS.SetAuthority( "PROJCS", "EPSG", psDefn->PCS );
        }
        else
        {
            char szPCSName[200];
            strcpy( szPCSName, "unnamed" );
            if ( GetMetadataElement( "GEOTIFF_NUM::1026::GTCitationGeoKey",
                                     szPCSName, sizeof(szPCSName) ) )
                oSRS.SetNode( "PROJCS", szPCSName );
        }
    }
    
/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char        *pszGeogName = NULL;
    char        *pszDatumName = NULL;
    char        *pszPMName = NULL;
    char        *pszSpheroidName = NULL;
    char        *pszAngularUnits = NULL;
    double      dfInvFlattening, dfSemiMajor;
    char        szGCSName[200];
    
    if( GetMetadataElement( "GEOTIFF_NUM::2049::GeogCitationGeoKey",
                            szGCSName, sizeof(szGCSName) ) )
        pszGeogName = CPLStrdup(szGCSName);
    else
    {
        GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL, NULL );
        GTIFToCPLRecycleString(&pszGeogName);
    }
    GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
    GTIFToCPLRecycleString(&pszDatumName);
    GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
    GTIFToCPLRecycleString(&pszPMName);
    GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );
    GTIFToCPLRecycleString(&pszSpheroidName);
    
    GTIFGetUOMAngleInfo( psDefn->UOMAngle, &pszAngularUnits, NULL );
    GTIFToCPLRecycleString(&pszAngularUnits);
    if( pszAngularUnits == NULL )
        pszAngularUnits = CPLStrdup("unknown");

    if( pszDatumName != NULL )
        WKTMassageDatum( &pszDatumName );

    dfSemiMajor = psDefn->SemiMajor;
    if( psDefn->SemiMajor == 0.0 )
    {
        pszSpheroidName = CPLStrdup("unretrievable - using WGS84");
        dfSemiMajor = SRS_WGS84_SEMIMAJOR;
        dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    else if( (psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
             || (psDefn->SemiMinor / psDefn->SemiMajor) > 1.00000000000000001 )
        dfInvFlattening = -1.0 / (psDefn->SemiMinor/psDefn->SemiMajor - 1.0);
    else
        dfInvFlattening = 0.0; /* special flag for infinity */

    oSRS.SetGeogCS( pszGeogName, pszDatumName, 
                    pszSpheroidName, dfSemiMajor, dfInvFlattening,
                    pszPMName,
                    psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                    pszAngularUnits,
                    psDefn->UOMAngleInDegrees * 0.0174532925199433 );

    if( psDefn->GCS != KvUserDefined )
        oSRS.SetAuthority( "GEOGCS", "EPSG", psDefn->GCS );

    if( psDefn->Datum != KvUserDefined )
        oSRS.SetAuthority( "DATUM", "EPSG", psDefn->Datum );

    if( psDefn->Ellipsoid != KvUserDefined )
        oSRS.SetAuthority( "SPHEROID", "EPSG", psDefn->Ellipsoid );

    CPLFree( pszGeogName );
    CPLFree( pszDatumName );
    CPLFree( pszPMName );
    CPLFree( pszSpheroidName );
    CPLFree( pszAngularUnits );
        
/* ==================================================================== */
/*      Handle projection parameters.                                   */
/* ==================================================================== */
    if( psDefn->Model == ModelTypeProjected )
    {
/* -------------------------------------------------------------------- */
/*      Make a local copy of parms, and convert back into the           */
/*      angular units of the GEOGCS and the linear units of the         */
/*      projection.                                                     */
/* -------------------------------------------------------------------- */
        double          adfParm[10];
        int             i;

        for( i = 0; i < MIN(10,psDefn->nParms); i++ )
            adfParm[i] = psDefn->ProjParm[i];

        adfParm[0] /= psDefn->UOMAngleInDegrees;
        adfParm[1] /= psDefn->UOMAngleInDegrees;
        adfParm[2] /= psDefn->UOMAngleInDegrees;
        adfParm[3] /= psDefn->UOMAngleInDegrees;
        
        adfParm[5] /= psDefn->UOMLengthInMeters;
        adfParm[6] /= psDefn->UOMLengthInMeters;
        
/* -------------------------------------------------------------------- */
/*      Translation the fundamental projection.                         */
/* -------------------------------------------------------------------- */
        switch( psDefn->CTProjection )
        {
          case CT_TransverseMercator:
            oSRS.SetTM( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_TransvMercator_SouthOriented:
            oSRS.SetTMSO( adfParm[0], adfParm[1],
                          adfParm[4],
                          adfParm[5], adfParm[6] );
            break;

          case CT_Mercator:
            oSRS.SetMercator( adfParm[0], adfParm[1],
                              adfParm[4],
                              adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueStereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_Stereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueMercator: /* hotine */
            oSRS.SetHOM( adfParm[0], adfParm[1],
                         adfParm[2], adfParm[3],
                         adfParm[4],
                         adfParm[5], adfParm[6] );
            break;
        
          case CT_EquidistantConic: 
            oSRS.SetEC( adfParm[0], adfParm[1],
                        adfParm[2], adfParm[3],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_CassiniSoldner:
            oSRS.SetCS( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_Polyconic:
            oSRS.SetPolyconic( adfParm[0], adfParm[1],
                               adfParm[5], adfParm[6] );
            break;

          case CT_AzimuthalEquidistant:
            oSRS.SetAE( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_MillerCylindrical:
            oSRS.SetMC( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_Equirectangular:
            oSRS.SetEquirectangular( adfParm[0], adfParm[1],
                                     adfParm[5], adfParm[6] );
            break;
        
          case CT_Gnomonic:
            oSRS.SetGnomonic( adfParm[0], adfParm[1],
                              adfParm[5], adfParm[6] );
            break;
        
          case CT_LambertAzimEqualArea:
            oSRS.SetLAEA( adfParm[0], adfParm[1],
                          adfParm[5], adfParm[6] );
            break;
        
          case CT_Orthographic:
            oSRS.SetOrthographic( adfParm[0], adfParm[1],
                                  adfParm[5], adfParm[6] );
            break;
        
          case CT_Robinson:
            oSRS.SetRobinson( adfParm[1],
                              adfParm[5], adfParm[6] );
            break;
        
          case CT_Sinusoidal:
            oSRS.SetSinusoidal( adfParm[1],
                                adfParm[5], adfParm[6] );
            break;
        
          case CT_VanDerGrinten:
            oSRS.SetVDG( adfParm[1],
                         adfParm[5], adfParm[6] );
            break;

          case CT_PolarStereographic:
            oSRS.SetPS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_LambertConfConic_2SP:
            oSRS.SetLCC( adfParm[2], adfParm[3],
                         adfParm[0], adfParm[1],
                         adfParm[5], adfParm[6] );
            break;

          case CT_LambertConfConic_1SP:
            oSRS.SetLCC1SP( adfParm[0], adfParm[1],
                            adfParm[4],
                            adfParm[5], adfParm[6] );
            break;
        
          case CT_AlbersEqualArea:
            oSRS.SetACEA( adfParm[0], adfParm[1],
                          adfParm[2], adfParm[3],
                          adfParm[5], adfParm[6] );
            break;

          case CT_NewZealandMapGrid:
            oSRS.SetNZMG( adfParm[0], adfParm[1],
                          adfParm[5], adfParm[6] );
            break;
        }

/* -------------------------------------------------------------------- */
/*      Set projection units.                                           */
/* -------------------------------------------------------------------- */
        char    *pszUnitsName = NULL;
        
        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );

        if( pszUnitsName != NULL && psDefn->UOMLength != KvUserDefined )
        {
            oSRS.SetLinearUnits( pszUnitsName, psDefn->UOMLengthInMeters );
            oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", psDefn->UOMLength );
        }
        else
            oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );

        GTIFFreeMemory( pszUnitsName );
    }
    
/* -------------------------------------------------------------------- */
/*      Return the WKT serialization of the object.                     */
/* -------------------------------------------------------------------- */
    char        *pszWKT;

    oSRS.FixupOrdering();

    if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
        return pszWKT;
    else
        return NULL;
}

#ifdef MRSID_ESDK

/************************************************************************/
/* ==================================================================== */
/*                        MrSIDDummyImageReader                         */
/*                                                                      */
/*  This is a helper class to wrap GDAL calls in MrSID interface.       */
/* ==================================================================== */
/************************************************************************/

class MrSIDDummyImageReader : public LTIImageReader
{
  public:

    MrSIDDummyImageReader( GDALDataset *poSrcDS );
    ~MrSIDDummyImageReader();
    LT_STATUS           initialize();
    lt_int64            getPhysicalFileSize(void) const { return 0; };

  private:
    GDALDataset         *poDS;
    GDALDataType        eDataType;
    LTIDataType         eSampleType;
    const LTIPixel      *poPixel;
    
    double              adfGeoTransform[6];

    virtual LT_STATUS   decodeStrip( LTISceneBuffer& stripBuffer,
                                     const LTIScene& stripScene );
    virtual LT_STATUS   decodeBegin( const LTIScene& )
                            { return LT_STS_Success; };
    virtual LT_STATUS   decodeEnd() { return LT_STS_Success; };
};

/************************************************************************/
/*                        MrSIDDummyImageReader()                       */
/************************************************************************/

MrSIDDummyImageReader::MrSIDDummyImageReader( GDALDataset *poSrcDS ) :
                                            LTIImageReader(), poDS(poSrcDS)
{
    poPixel = NULL;
}

/************************************************************************/
/*                        ~MrSIDDummyImageReader()                      */
/************************************************************************/

MrSIDDummyImageReader::~MrSIDDummyImageReader()
{
    if ( poPixel )
        delete poPixel;
}

/************************************************************************/
/*                             initialize()                             */
/************************************************************************/

LT_STATUS MrSIDDummyImageReader::initialize()
{
    LT_STATUS eStat = LT_STS_Uninit;
#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 6
    if ( !LT_SUCCESS(eStat = LTIImageReader::init()) )
        return eStat;
#else
    if ( !LT_SUCCESS(eStat = LTIImageReader::initialize()) )
        return eStat;
#endif
    
    lt_uint16 nBands = (lt_uint16)poDS->GetRasterCount();
    LTIColorSpace eColorSpace = LTI_COLORSPACE_RGB;
    switch ( nBands )
    {
        case 1:
            eColorSpace = LTI_COLORSPACE_GRAYSCALE;
            break;
        case 3:
            eColorSpace = LTI_COLORSPACE_RGB;
            break;
        default:
            eColorSpace = LTI_COLORSPACE_MULTISPECTRAL;
            break;
    }
    
    eDataType = poDS->GetRasterBand(1)->GetRasterDataType();
    switch ( eDataType )
    {
        case GDT_UInt16:
            eSampleType = LTI_DATATYPE_UINT16;
            break;
        case GDT_Int16:
            eSampleType = LTI_DATATYPE_SINT16;
            break;
        case GDT_UInt32:
            eSampleType = LTI_DATATYPE_UINT32;
            break;
        case GDT_Int32:
            eSampleType = LTI_DATATYPE_SINT32;
            break;
        case GDT_Float32:
            eSampleType = LTI_DATATYPE_FLOAT32;
            break;
        case GDT_Float64:
            eSampleType = LTI_DATATYPE_FLOAT64;
            break;
        case GDT_Byte:
        default:
            eSampleType = LTI_DATATYPE_UINT8;
            break;
    }

    poPixel = new LTIDLLPixel<LTIPixel>( eColorSpace, nBands, eSampleType );
    if ( !LT_SUCCESS(setPixelProps(*poPixel)) )
        return LT_STS_Failure;

    if ( !LT_SUCCESS(setDimensions(poDS->GetRasterXSize(),
                                   poDS->GetRasterYSize())) )
        return LT_STS_Failure;

    if ( poDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
#ifdef MRSID_SDK_40
        LTIGeoCoord oGeo( adfGeoTransform[0] + adfGeoTransform[1] / 2,
                          adfGeoTransform[3] + adfGeoTransform[5] / 2,
                          adfGeoTransform[1], adfGeoTransform[5],
                          adfGeoTransform[2], adfGeoTransform[4], NULL,
                          poDS->GetProjectionRef() );
#else
        LTIGeoCoord oGeo( adfGeoTransform[0] + adfGeoTransform[1] / 2,
                          adfGeoTransform[3] + adfGeoTransform[5] / 2,
                          adfGeoTransform[1], adfGeoTransform[5],
                          adfGeoTransform[2], adfGeoTransform[4], 
                          poDS->GetProjectionRef() );
#endif
        if ( !LT_SUCCESS(setGeoCoord( oGeo )) )
            return LT_STS_Failure;
    }

    /*int     bSuccess;
    double  dfNoDataValue = poDS->GetNoDataValue( &bSuccess );
    if ( bSuccess )
    {
        LTIPixel    oNoDataPixel( *poPixel );
        lt_uint16   iBand;

        for (iBand = 0; iBand < (lt_uint16)poDS->GetRasterCount(); iBand++)
            oNoDataPixel.setSampleValueFloat32( iBand, dfNoDataValue );
        if ( !LT_SUCCESS(setNoDataPixel( &oNoDataPixel )) )
            return LT_STS_Failure;
    }*/

    setDefaultDynamicRange();
#if !defined(LTI_SDK_MAJOR) || LTI_SDK_MAJOR < 8
    setClassicalMetadata();
#endif

    return LT_STS_Success;
}

/************************************************************************/
/*                             decodeStrip()                            */
/************************************************************************/

LT_STATUS MrSIDDummyImageReader::decodeStrip(LTISceneBuffer& stripData,
                                             const LTIScene& stripScene)

{
    const lt_int32  nXOff = stripScene.getUpperLeftCol();
    const lt_int32  nYOff = stripScene.getUpperLeftRow();
    const lt_int32  nBufXSize = stripScene.getNumCols();
    const lt_int32  nBufYSize = stripScene.getNumRows();
    const lt_int32  nDataBufXSize = stripData.getTotalNumCols();
    const lt_int32  nDataBufYSize = stripData.getTotalNumRows();
    const lt_uint16 nBands = poPixel->getNumBands();

    void *pData = CPLMalloc(nDataBufXSize * nDataBufYSize * poPixel->getNumBytes());
    if ( !pData )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDummyImageReader::decodeStrip(): "
                  "Cannot allocate enough space for scene buffer" );
        return LT_STS_Failure;
    }

    poDS->RasterIO( GF_Read, nXOff, nYOff, nBufXSize, nBufYSize, 
                    pData, nBufXSize, nBufYSize, eDataType, nBands, NULL, 
                    0, 0, 0 );

    stripData.importDataBSQ( pData );
    CPLFree( pData );
    return LT_STS_Success;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void MrSIDDataset::FlushCache()

{
    GDALDataset::FlushCache();
}

/************************************************************************/
/*                          MrSIDCreateCopy()                           */
/************************************************************************/

static GDALDataset *
MrSIDCreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData )

{ 
    const char* pszVersion = CSLFetchNameValue(papszOptions, "VERSION");
#ifdef MRSID_HAVE_MG4WRITE
    int iVersion = pszVersion ? atoi(pszVersion) : 4;
#else
    int iVersion = pszVersion ? atoi(pszVersion) : 3;
#endif
    LT_STATUS eStat = LT_STS_Uninit;

#ifdef DEBUG
    bool bMeter = false;
#else
    bool bMeter = true;
#endif    
    
    if (poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "MrSID driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return NULL;
    }

    MrSIDProgress oProgressDelegate(pfnProgress, pProgressData);
    if( LT_FAILURE( eStat = oProgressDelegate.setProgressStatus(0) ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDProgress.setProgressStatus failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }

    // Create the file.                                               
    MrSIDDummyImageReader oImageReader( poSrcDS );
    if( LT_FAILURE( eStat = oImageReader.initialize() ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDummyImageReader.Initialize failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }

    LTIGeoFileImageWriter *poImageWriter = NULL;
    switch (iVersion)
    {
    case 2: {
        // Output Mrsid Version 2 file.
#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
        LTIDLLDefault<MG2ImageWriter> *poMG2ImageWriter;
        poMG2ImageWriter = new LTIDLLDefault<MG2ImageWriter>;
        eStat = poMG2ImageWriter->initialize(&oImageReader);
#else
        LTIDLLWriter<MG2ImageWriter> *poMG2ImageWriter;
        poMG2ImageWriter = new LTIDLLWriter<MG2ImageWriter>(&oImageReader);
        eStat = poMG2ImageWriter->initialize();
#endif
        if( LT_FAILURE( eStat ) )
        {
            delete poMG2ImageWriter;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MG2ImageWriter.initialize() failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }

#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
        eStat = poMG2ImageWriter->setEncodingApplication("MrSID Driver",
                                                         GDALVersionInfo("--version"));
        if( LT_FAILURE( eStat ) )
        {
            delete poMG2ImageWriter;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MG2ImageWriter.setEncodingApplication() failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }
#endif

        poMG2ImageWriter->setUsageMeterEnabled(bMeter);

        poMG2ImageWriter->params().setBlockSize(poMG2ImageWriter->params().getBlockSize());

        // check for compression option
        const char* pszValue = CSLFetchNameValue(papszOptions, "COMPRESSION");
        if( pszValue != NULL )
            poMG2ImageWriter->params().setCompressionRatio( (float)atof(pszValue) );

        poImageWriter = poMG2ImageWriter;

        break; }
    case 3: {
        // Output Mrsid Version 3 file.
#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
        LTIDLLDefault<MG3ImageWriter> *poMG3ImageWriter;
        poMG3ImageWriter = new LTIDLLDefault<MG3ImageWriter>;
        eStat = poMG3ImageWriter->initialize(&oImageReader);
#else
        LTIDLLWriter<MG3ImageWriter> *poMG3ImageWriter;
        poMG3ImageWriter = new LTIDLLWriter<MG3ImageWriter>(&oImageReader);
        eStat = poMG3ImageWriter->initialize();
#endif
        if( LT_FAILURE( eStat ) )
        {
            delete poMG3ImageWriter;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MG3ImageWriter.initialize() failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }

#if defined(LTI_SDK_MAJOR) && LTI_SDK_MAJOR >= 8
        eStat = poMG3ImageWriter->setEncodingApplication("MrSID Driver",
                                                         GDALVersionInfo("--version"));
        if( LT_FAILURE( eStat ) )
        {
            delete poMG3ImageWriter;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MG3ImageWriter.setEncodingApplication() failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }
#endif

        // usage meter should only be disabled for debugging
        poMG3ImageWriter->setUsageMeterEnabled(bMeter);

#if !defined(LTI_SDK_MAJOR) || LTI_SDK_MAJOR < 8
        // Set 64-bit Interface for large files.
        poMG3ImageWriter->setFileStream64(true);
#endif

        // set 2 pass optimizer option
        if( CSLFetchNameValue(papszOptions, "TWOPASS") != NULL )
            poMG3ImageWriter->params().setTwoPassOptimizer( true );

        // set filesize in KB
        const char* pszValue = CSLFetchNameValue(papszOptions, "FILESIZE");
        if( pszValue != NULL )
            poMG3ImageWriter->params().setTargetFilesize( atoi(pszValue) );

        poImageWriter = poMG3ImageWriter;

        break; }
#ifdef MRSID_HAVE_MG4WRITE
    case 4: {
        // Output Mrsid Version 4 file.
        LTIDLLDefault<MG4ImageWriter> *poMG4ImageWriter;
        poMG4ImageWriter = new LTIDLLDefault<MG4ImageWriter>;
        eStat = poMG4ImageWriter->initialize(&oImageReader, NULL, NULL);
        if( LT_FAILURE( eStat ) )
        {
            delete poMG4ImageWriter;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MG3ImageWriter.initialize() failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }

        eStat = poMG4ImageWriter->setEncodingApplication("MrSID Driver",
                                                         GDALVersionInfo("--version"));
        if( LT_FAILURE( eStat ) )
        {
            delete poMG4ImageWriter;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MG3ImageWriter.setEncodingApplication() failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }

        // usage meter should only be disabled for debugging
        poMG4ImageWriter->setUsageMeterEnabled(bMeter);

        // set 2 pass optimizer option
        if( CSLFetchNameValue(papszOptions, "TWOPASS") != NULL )
            poMG4ImageWriter->params().setTwoPassOptimizer( true );

        // set filesize in KB
        const char* pszValue = CSLFetchNameValue(papszOptions, "FILESIZE");
        if( pszValue != NULL )
            poMG4ImageWriter->params().setTargetFilesize( atoi(pszValue) );

        poImageWriter = poMG4ImageWriter;

        break; }
#endif /* MRSID_HAVE_MG4WRITE */
    default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MrSID generation specified (VERSION=%s).",
                  pszVersion );
        return NULL;
    }

    // set output filename
    poImageWriter->setOutputFileSpec( pszFilename );

    // set progress delegate
    poImageWriter->setProgressDelegate(&oProgressDelegate);

    // set defaults
    poImageWriter->setStripHeight(poImageWriter->getStripHeight());

    // set MrSID world file
    if( CSLFetchNameValue(papszOptions, "WORLDFILE") != NULL )
        poImageWriter->setWorldFileSupport( true );

    // write the scene
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    const LTIScene oScene( 0, 0, nXSize, nYSize, 1.0 );
    if( LT_FAILURE( eStat = poImageWriter->write( oScene ) ) )
    {
        delete poImageWriter;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MG2ImageWriter.write() failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }

    delete poImageWriter;
/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

#ifdef MRSID_J2K
/************************************************************************/
/*                           JP2CreateCopy()                            */
/************************************************************************/

static GDALDataset *
JP2CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData )

{ 
#ifdef DEBUG
    bool bMeter = false;
#else
    bool bMeter = true;
#endif    

    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    LT_STATUS  eStat;
    
    if (poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "MrSID driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return NULL;
    }

    MrSIDProgress oProgressDelegate(pfnProgress, pProgressData);
    if( LT_FAILURE( eStat = oProgressDelegate.setProgressStatus(0) ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDProgress.setProgressStatus failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }

    // Create the file.   
    MrSIDDummyImageReader oImageReader( poSrcDS );
    eStat = oImageReader.initialize();
    if( eStat != LT_STS_Success )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDummyImageReader.Initialize failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }
      
#if !defined(MRSID_POST5)
    J2KImageWriter oImageWriter(&oImageReader);
    eStat = oImageWriter.initialize();
#elif !defined(LTI_SDK_MAJOR) || LTI_SDK_MAJOR < 8
    JP2WriterManager oImageWriter(&oImageReader);
    eStat = oImageWriter.initialize();
#else
    JP2WriterManager oImageWriter;
    eStat = oImageWriter.initialize(&oImageReader);
#endif
    if( eStat != LT_STS_Success )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "J2KImageWriter.Initialize failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }

#if !defined(LTI_SDK_MAJOR) || LTI_SDK_MAJOR < 8
    // Set 64-bit Interface for large files.
    oImageWriter.setFileStream64(true);
#endif

    oImageWriter.setUsageMeterEnabled(bMeter);
      
    // set output filename
    oImageWriter.setOutputFileSpec( pszFilename );

    // set progress delegate
    oImageWriter.setProgressDelegate(&oProgressDelegate);

    // Set defaults
    //oImageWriter.setStripHeight(oImageWriter.getStripHeight());

    // set MrSID world file
    if( CSLFetchNameValue(papszOptions, "WORLDFILE") != NULL )
        oImageWriter.setWorldFileSupport( true );
      
    // check for compression option
    const char* pszValue = CSLFetchNameValue(papszOptions, "COMPRESSION");
    if( pszValue != NULL )
        oImageWriter.params().setCompressionRatio( (float)atof(pszValue) );
        
    pszValue = CSLFetchNameValue(papszOptions, "XMLPROFILE");
    if( pszValue != NULL )
    {
        LTFileSpec xmlprofile(pszValue);
        eStat = oImageWriter.params().readProfile(xmlprofile);
        if( eStat != LT_STS_Success )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "JPCWriterParams.readProfile failed.\n%s",
                      getLastStatusString( eStat ) );
            return NULL;
        }
    }

    // write the scene
    const LTIScene oScene( 0, 0, nXSize, nYSize, 1.0 );
    eStat = oImageWriter.write( oScene );
    if( eStat != LT_STS_Success )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "J2KImageWriter.write() failed.\n%s",
                  getLastStatusString( eStat ) );
        return NULL;
    }
  
/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    GDALPamDataset *poDS = (GDALPamDataset*) JP2Open(&oOpenInfo);

    if( poDS )  
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}
#endif /* MRSID_J2K */
#endif /* MRSID_ESDK */

/************************************************************************/
/*                        GDALRegister_MrSID()                          */
/************************************************************************/

void GDALRegister_MrSID()

{
    GDALDriver  *poDriver;
    
    if (! GDAL_CHECK_VERSION("MrSID driver"))
        return;

/* -------------------------------------------------------------------- */
/*      MrSID driver.                                                   */
/* -------------------------------------------------------------------- */
    if( GDALGetDriverByName( "MrSID" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MrSID" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                        "Multi-resolution Seamless Image Database (MrSID)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_mrsid.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "sid" );

#ifdef MRSID_ESDK
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
// Version 2 Options
"   <Option name='COMPRESSION' type='double' description='Set compression ratio (0.0 default is meant to be lossless)'/>"
// Version 3 Options
"   <Option name='TWOPASS' type='int' description='Use twopass optimizer algorithm'/>"
"   <Option name='FILESIZE' type='int' description='Set target file size (0 implies lossless compression)'/>"
// Version 2 and 3 Option
"   <Option name='WORLDFILE' type='boolean' description='Write out world file'/>"
// Version Type
"   <Option name='VERSION' type='int' description='Valid versions are 2 and 3, default = 3'/>"
"</CreationOptionList>" );

        poDriver->pfnCreateCopy = MrSIDCreateCopy;

#else
        /* In read-only mode, we support VirtualIO. I don't think this is the case */
        /* for MrSIDCreateCopy() */
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
#endif
        poDriver->pfnIdentify = MrSIDIdentify;
        poDriver->pfnOpen = MrSIDOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }

/* -------------------------------------------------------------------- */
/*      JP2MRSID driver.                                                */
/* -------------------------------------------------------------------- */
#ifdef MRSID_J2K
    if( GDALGetDriverByName( "JP2MrSID" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "JP2MrSID" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                        "MrSID JPEG2000" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_jp2mrsid.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );

#ifdef MRSID_ESDK
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='COMPRESSION' type='double' description='Set compression ratio (0.0 default is meant to be lossless)'/>"
"   <Option name='WORLDFILE' type='boolean' description='Write out world file'/>"
"   <Option name='XMLPROFILE' type='string' description='Use named xml profile file'/>"
"</CreationOptionList>" );

        poDriver->pfnCreateCopy = JP2CreateCopy;
#else
        /* In read-only mode, we support VirtualIO. I don't think this is the case */
        /* for JP2CreateCopy() */
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
#endif
        poDriver->pfnIdentify = JP2Identify;
        poDriver->pfnOpen = JP2Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
#endif /* def MRSID_J2K */
}

#if defined(MRSID_USE_TIFFSYMS_WORKAROUND)
extern "C" {

/* This is not pretty but I am not sure how else to get the plugin to build
 * against the ESDK.  ESDK symbol dependencies bring in __TIFFmemcpy and
 * __gtiff_size, which are not exported from gdal.dll.  Rather than link these
 * symbols from the ESDK distribution of GDAL, or link in the entire gdal.lib
 * statically, it seemed safer and smaller to bring in just the objects that
 * wouldsatisfy these symbols from the enclosing GDAL build.  However, doing
 * so pulls in a few more dependencies.  /Gy and /OPT:REF did not seem to help
 * things, so I have implemented no-op versions of these symbols since they
 * do not actually get called.  If the MrSID ESDK ever comes to require the
 * actual versions of these functions, we'll hope duplicate symbol errors will
 * bring attention back to this problem.
 */
void TIFFClientOpen() {}
void TIFFError() {}
void TIFFGetField() {}
void TIFFSetField() {}

}
#endif
