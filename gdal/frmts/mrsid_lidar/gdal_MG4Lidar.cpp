/******************************************************************************
* Project:  MG4 Lidar GDAL Plugin
* Purpose:  Provide an orthographic view of MG4-encoded Lidar dataset
*                   for use with LizardTech Lidar SDK version 1.1.0
* Author:   Michael Rosen, mrosen@lizardtech.com
*
******************************************************************************
* Copyright (c) 2010, LizardTech
* All rights reserved.

* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are met:
*
*   Redistributions of source code must retain the above copyright notice, 
*   this list of conditions and the following disclaimer.
* 
*   Redistributions in binary form must reproduce the above copyright notice, 
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
*   Neither the name of the LizardTech nor the names of its contributors may 
*   be used to endorse or promote products derived from this software without 
*   specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
* POSSIBILITY OF SUCH DAMAGE.
****************************************************************************/
#include "lidar/MG4PointReader.h"
#include "lidar/FileIO.h"
#include "lidar/Error.h"
#include "lidar/Version.h"
#include <float.h>
LT_USE_LIDAR_NAMESPACE

#include "gdal_pam.h"
// #include "gdal_alg.h" // 1.6 and later have gridding algorithms

CPL_C_START
//void __declspec(dllexport) GDALRegister_MG4Lidar(void);
void CPL_DLL GDALRegister_MG4Lidar(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				MG4LidarDataset				*/
/* ==================================================================== */
/************************************************************************/

// Resolution Ratio between adjacent levels.
#define RESOLUTION_RATIO 2.0

class MG4LidarRasterBand;
class CropableMG4PointReader : public MG4PointReader
{
   CONCRETE_OBJECT(CropableMG4PointReader);
   void init (IO *io, Bounds *bounds)
   {
      MG4PointReader::init(io);
      if (bounds != NULL)
         setBounds(*bounds);
   }
};
CropableMG4PointReader::CropableMG4PointReader() : MG4PointReader() {};   
CropableMG4PointReader::~CropableMG4PointReader() {};   

IMPLEMENT_OBJECT_CREATE(CropableMG4PointReader);

static double MaxRasterSize = 2048.0;
static double MaxBlockSideSize = 1024.0;
class MG4LidarDataset : public GDALPamDataset
{
friend class MG4LidarRasterBand;
public:
   MG4LidarDataset();
   ~MG4LidarDataset();
   static GDALDataset *Open( GDALOpenInfo * );
   CPLErr 	GetGeoTransform( double * padfTransform );
   const char *GetProjectionRef();
protected:
   MG4PointReader *reader;
   FileIO *fileIO;
   CPLErr OpenZoomLevel( int Zoom );
   PointInfo requiredChannels;
   int nOverviewCount;
   MG4LidarDataset **papoOverviewDS;
   CPLXMLNode *poXMLPCView;
   bool ownsXML;
   int nBlockXSize, nBlockYSize;
   int iLevel;
};

/* ========================================  */
/*                            MG4LidarRasterBand                                          */
/* ========================================  */

class MG4LidarRasterBand : public GDALPamRasterBand
{
   friend class MG4LidarDataset;

public:

   MG4LidarRasterBand( MG4LidarDataset *, int, CPLXMLNode *, const char * );
   ~MG4LidarRasterBand();

   virtual CPLErr GetStatistics( int bApproxOK, int bForce, double *pdfMin, double *pdfMax, double *pdfMean, double *padfStdDev );
   virtual int GetOverviewCount();
   virtual GDALRasterBand * GetOverview( int i );
   virtual CPLErr IReadBlock( int, int, void * );
   virtual double GetNoDataValue( int *pbSuccess = NULL );

   protected:
   double getMaxValue();
   double nodatavalue;
   virtual const bool ElementPassesFilter(const PointData &, size_t);
   template<typename DTYPE>
   CPLErr   doReadBlock(int, int, void *);
   CPLXMLNode *poxmlBand;
   char **papszFilterClassCodes;
   char **papszFilterReturnNums;
   const char * Aggregation;
   CPLString ChannelName;
};


/************************************************************************/
/*                           MG4LidarRasterBand()                            */
/************************************************************************/

MG4LidarRasterBand::MG4LidarRasterBand( MG4LidarDataset *pods, int nband, CPLXMLNode *xmlBand, const char * name )
{
   this->poDS = pods;
   this->nBand = nband;
   this->poxmlBand = xmlBand;
   this->ChannelName = name;
   this->Aggregation = NULL;
   nBlockXSize = pods->nBlockXSize;
   nBlockYSize = pods->nBlockYSize;

   switch(pods->reader->getChannel(name)->getDataType())
   {
#define DO_CASE(ltdt, gdt) case (ltdt): eDataType = gdt; break;
      DO_CASE(DATATYPE_FLOAT64, GDT_Float64);
      DO_CASE(DATATYPE_FLOAT32, GDT_Float32);
      DO_CASE(DATATYPE_SINT32, GDT_Int32);
      DO_CASE(DATATYPE_UINT32, GDT_UInt32);
      DO_CASE(DATATYPE_SINT16, GDT_Int16);
      DO_CASE(DATATYPE_UINT16, GDT_UInt16);
      DO_CASE(DATATYPE_SINT8, GDT_Byte);
      DO_CASE(DATATYPE_UINT8, GDT_Byte);
default:
   CPLError(CE_Failure, CPLE_AssertionFailed,
      "Invalid datatype in MG4 file");
   break;
#undef DO_CASE      
   }
   // Coerce datatypes as required.
   const char * ForceDataType =  CPLGetXMLValue(pods->poXMLPCView, "Datatype", NULL);

   if (ForceDataType != NULL)
   {
      GDALDataType dt = GDALGetDataTypeByName(ForceDataType);
      if (dt != GDT_Unknown)
         eDataType = dt;
   }

   CPLXMLNode *poxmlFilter = CPLGetXMLNode(poxmlBand, "ClassificationFilter");
   if( poxmlFilter == NULL )
      poxmlFilter = CPLGetXMLNode(pods->poXMLPCView, "ClassificationFilter");
   if (poxmlFilter == NULL || poxmlFilter->psChild == NULL ||
       poxmlFilter->psChild->pszValue == NULL)
      papszFilterClassCodes = NULL;
   else
      papszFilterClassCodes = CSLTokenizeString(poxmlFilter->psChild->pszValue);

   poxmlFilter = CPLGetXMLNode(poxmlBand, "ReturnNumberFilter");
   if( poxmlFilter == NULL )
      poxmlFilter = CPLGetXMLNode(pods->poXMLPCView, "ReturnNumberFilter");
   if (poxmlFilter == NULL || poxmlFilter->psChild == NULL ||
       poxmlFilter->psChild->pszValue == NULL)
      papszFilterReturnNums = NULL;
   else
      papszFilterReturnNums = CSLTokenizeString(poxmlFilter->psChild->pszValue);

   
   CPLXMLNode * poxmlAggregation = CPLGetXMLNode(poxmlBand, "AggregationMethod");
   if( poxmlAggregation == NULL )
      poxmlAggregation = CPLGetXMLNode(pods->poXMLPCView, "AggregationMethod");
   if (poxmlAggregation == NULL || poxmlAggregation->psChild == NULL ||
       poxmlAggregation->psChild->pszValue == NULL)
      Aggregation = "Mean";
   else
      Aggregation = poxmlAggregation->psChild->pszValue;

   nodatavalue = getMaxValue();

   CPLXMLNode * poxmlIntepolation = CPLGetXMLNode(poxmlBand, "InterpolationMethod");
   if( poxmlIntepolation == NULL )
      poxmlIntepolation = CPLGetXMLNode(pods->poXMLPCView, "InterpolationMethod");
   if (poxmlIntepolation != NULL )
   {
      CPLXMLNode * poxmlMethod= NULL;
      char ** papszParams = NULL;
      if (((poxmlMethod = CPLSearchXMLNode(poxmlIntepolation, "None")) != NULL) &&
          poxmlMethod->psChild != NULL && poxmlMethod->psChild->pszValue != NULL)
      {
         papszParams = CSLTokenizeString(poxmlMethod->psChild->pszValue);
         if (!EQUAL(papszParams[0], "MAX"))
            nodatavalue = CPLAtof(papszParams[0]);
      }
      // else if .... Add support for other interpolation methods here.
      CSLDestroy(papszParams);
   }
   const char * filter = NULL;
   if (papszFilterClassCodes != NULL && papszFilterReturnNums != NULL)
      filter ="Classification and Return";
   if (papszFilterClassCodes != NULL)
      filter = "Classification";
   else if (papszFilterReturnNums != NULL)
      filter = "Return";
   CPLString osDesc;
   if (filter)
      osDesc.Printf("%s of %s (filtered by %s)", Aggregation, ChannelName.c_str(), filter);
   else
      osDesc.Printf("%s of %s", Aggregation, ChannelName.c_str());
   SetDescription(osDesc);
}

/************************************************************************/
/*                          ~MG4LidarRasterBand()                            */
/************************************************************************/
MG4LidarRasterBand::~MG4LidarRasterBand()
{
   CSLDestroy(papszFilterClassCodes);
   CSLDestroy(papszFilterReturnNums);
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int MG4LidarRasterBand::GetOverviewCount()
{
   MG4LidarDataset *poGDS = (MG4LidarDataset *) poDS;
   return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/
GDALRasterBand *MG4LidarRasterBand::GetOverview( int i )
{
   MG4LidarDataset *poGDS = (MG4LidarDataset *) poDS;

   if( i < 0 || i >= poGDS->nOverviewCount )
      return NULL;
   else
      return poGDS->papoOverviewDS[i]->GetRasterBand( nBand );
}
template<typename DTYPE>
const DTYPE GetChannelElement(const ChannelData &channel, size_t idx)
{
   DTYPE retval = static_cast<DTYPE>(0);
   switch (channel.getDataType())
   {
   case (DATATYPE_FLOAT64):
      retval = static_cast<DTYPE>(static_cast<const double*>(channel.getData())[idx]);
      break;
   case (DATATYPE_FLOAT32):
      retval = static_cast<DTYPE>(static_cast<const float *>(channel.getData())[idx]);
      break;
   case (DATATYPE_SINT32):
      retval = static_cast<DTYPE>(static_cast<const long *>(channel.getData())[idx]);
      break;
   case (DATATYPE_UINT32):
      retval = static_cast<DTYPE>(static_cast<const unsigned long *>(channel.getData())[idx]);
      break;
   case (DATATYPE_SINT16):
      retval = static_cast<DTYPE>(static_cast<const short *>(channel.getData())[idx]);
      break;
   case (DATATYPE_UINT16):
      retval = static_cast<DTYPE>(static_cast<const unsigned short *>(channel.getData())[idx]);
      break;
   case (DATATYPE_SINT8):
      retval = static_cast<DTYPE>(static_cast<const char *>(channel.getData())[idx]);
      break;
   case (DATATYPE_UINT8):
      retval = static_cast<DTYPE>(static_cast<const unsigned char *>(channel.getData())[idx]);
      break;
   case (DATATYPE_SINT64):
      retval = static_cast<DTYPE>(static_cast<const GIntBig *>(channel.getData())[idx]);
      break;
   case (DATATYPE_UINT64):
      retval = static_cast<DTYPE>(static_cast<const GUIntBig *>(channel.getData())[idx]);
      break;
   default:
      break;
   }
   return retval;
}


const bool MG4LidarRasterBand::ElementPassesFilter(const PointData &pointdata, size_t i)
{
   bool bClassificationOK = true;
   bool bReturnNumOK = true;

   // Check if classification code is ok:  it was requested and it does match one of the requested codes
   const int classcode = GetChannelElement<int>(*pointdata.getChannel(CHANNEL_NAME_ClassId), i);
   char bufCode[16];
   sprintf(bufCode, "%d", classcode);
   bClassificationOK = (papszFilterClassCodes == NULL ? true :
      (CSLFindString(papszFilterClassCodes,bufCode)!=-1));

   if (bClassificationOK)
   {
      // Check if return num is ok:  it was requested and it does match one of the requested return numbers
      const long returnnum= static_cast<const unsigned char *>(pointdata.getChannel(CHANNEL_NAME_ReturnNum)->getData())[i];
      sprintf(bufCode, "%d", (int)returnnum);
      bReturnNumOK = (papszFilterReturnNums == NULL ? true :
         (CSLFindString(papszFilterReturnNums, bufCode)!=-1));
      if (!bReturnNumOK && CSLFindString(papszFilterReturnNums, "Last")!=-1)
      {  // Didn't find an explicit match (e.g. return number "1") so we handle a request for "Last" returns
         const long numreturns= GetChannelElement<long>(*pointdata.getChannel(CHANNEL_NAME_NumReturns), i);
         bReturnNumOK = (returnnum == numreturns);
      }
   }

   return bReturnNumOK && bClassificationOK;

}


template<typename DTYPE>
CPLErr   MG4LidarRasterBand::doReadBlock(int nBlockXOff, int nBlockYOff, void * pImage)
{
   MG4LidarDataset * poGDS = (MG4LidarDataset *)poDS;
   MG4PointReader *reader =poGDS->reader;

   struct Accumulator_t
   {
      DTYPE value;
      int count;
   } ;
   Accumulator_t * Accumulator = NULL;
   if (EQUAL(Aggregation, "Mean"))
   {
      Accumulator = new Accumulator_t[nBlockXSize*nBlockYSize];
      memset (Accumulator, 0, sizeof(Accumulator_t)*nBlockXSize*nBlockYSize);
   }
   for (int i = 0; i < nBlockXSize; i++)
   {
      for (int j = 0; j < nBlockYSize; j++)
      {
         static_cast<DTYPE*>(pImage)[i*nBlockYSize+j] = static_cast<DTYPE>(nodatavalue);
      }
   }

   double geoTrans[6];
   poGDS->GetGeoTransform(geoTrans);   
   double xres = geoTrans[1];
   double yres = geoTrans[5];

   // Get the extent of the requested block.
   double xmin = geoTrans[0] + (nBlockXOff *nBlockXSize* xres);
   double xmax = xmin + nBlockXSize* xres;
   double ymax = reader->getBounds().y.max - (nBlockYOff * nBlockYSize* -yres);
   double ymin = ymax - nBlockYSize* -yres;
   Bounds bounds(xmin, xmax,  ymin, ymax, -HUGE_VAL, +HUGE_VAL); 
   PointData pointdata;
   pointdata.init(reader->getPointInfo(), 4096);
   double fraction = 1.0/pow(RESOLUTION_RATIO, poGDS->iLevel);
   CPLDebug( "MG4Lidar", "IReadBlock(x=%d y=%d, level=%d, fraction=%f)", nBlockXOff, nBlockYOff, poGDS->iLevel, fraction);
   Scoped<PointIterator> iter(reader->createIterator(bounds, fraction, reader->getPointInfo(), NULL));

   const double * x = pointdata.getX();
   const double * y = pointdata.getY();
   size_t nPoints;
   while ( (nPoints = iter->getNextPoints(pointdata)) != 0)
   {
      for( size_t i = 0; i < nPoints; i++ )
      {
         const ChannelData * channel = pointdata.getChannel(ChannelName);
         if (papszFilterClassCodes || papszFilterReturnNums)
         {
            if (!ElementPassesFilter(pointdata, i))
               continue;
         }
         double col = (x[i] - xmin) / xres;
         double row = (ymax - y[i]) / -yres;
         col = floor (col);
         row = floor (row);

         if (row < 0) 
            row = 0;
         else if (row >= nBlockYSize) 
            row = nBlockYSize - 1;
         if (col < 0) 
            col = 0;
         else if (col >= nBlockXSize ) 
            col = nBlockXSize - 1;

         int iCol = (int) (col);
         int iRow = (int) (row);
         const int offset =iRow* nBlockXSize + iCol;
         if (EQUAL(Aggregation, "Max"))
         {
            DTYPE value = GetChannelElement<DTYPE>(*channel, i);
            if (static_cast<DTYPE *>(pImage)[offset] == static_cast<DTYPE>(nodatavalue) ||
               static_cast<DTYPE *>(pImage)[offset] < value)
               static_cast<DTYPE *>(pImage)[offset] = value;
         }
         else if (EQUAL(Aggregation, "Min"))
         {
            DTYPE value = GetChannelElement<DTYPE>(*channel, i);
            if (static_cast<DTYPE *>(pImage)[offset] == static_cast<DTYPE>(nodatavalue) ||
               static_cast<DTYPE *>(pImage)[offset] > value)
               static_cast<DTYPE *>(pImage)[offset] = value;
         }
         else if (EQUAL(Aggregation, "Mean"))
         {
            DTYPE value = GetChannelElement<DTYPE>(*channel, i);
            Accumulator[offset].count++;
            Accumulator[offset].value += value;
            static_cast<DTYPE*>(pImage)[offset] = static_cast<DTYPE>(Accumulator[offset].value / Accumulator[offset].count);
         }
      }
   }
   
   delete[] Accumulator;
   return CE_None;
}

double MG4LidarRasterBand::getMaxValue()
{
   double retval;
   switch(eDataType)
   {
      #define DO_CASE(gdt, largestvalue)  case (gdt):\
         retval = static_cast<double>(largestvalue); \
         break;

      DO_CASE (GDT_Float64, DBL_MAX);
      DO_CASE (GDT_Float32, FLT_MAX);
      DO_CASE (GDT_Int32, INT_MAX);
      DO_CASE (GDT_UInt32, UINT_MAX);
      DO_CASE (GDT_Int16, SHRT_MAX);
      DO_CASE (GDT_UInt16, USHRT_MAX);
      DO_CASE (GDT_Byte, UCHAR_MAX);
      #undef DO_CASE
       default:
           retval = 0;
           break;
   }
   return retval;
}
/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MG4LidarRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
   // CPLErr eErr;
   switch(eDataType)
   {
#define DO_CASE(gdt, nativedt)  case (gdt):\
     /* eErr = */doReadBlock<nativedt>(nBlockXOff, nBlockYOff, pImage); \
   break;

      DO_CASE (GDT_Float64, double);
      DO_CASE (GDT_Float32, float);
      DO_CASE (GDT_Int32, long);
      DO_CASE (GDT_UInt32, unsigned long);
      DO_CASE (GDT_Int16, short);
      DO_CASE (GDT_UInt16, unsigned short);
      DO_CASE (GDT_Byte, char);
#undef DO_CASE
      default:
           return CE_Failure;
           break;

   }
   return CE_None;
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

CPLErr MG4LidarRasterBand::GetStatistics( int bApproxOK, int bForce,
                                         double *pdfMin, double *pdfMax, 
                                         double *pdfMean, double *pdfStdDev )

{
   bApproxOK = TRUE; 
   bForce = TRUE;

   return GDALPamRasterBand::GetStatistics( bApproxOK, bForce, 
      pdfMin, pdfMax, 
      pdfMean, pdfStdDev );   

}
/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double MG4LidarRasterBand::GetNoDataValue( int *pbSuccess )
{
   if (pbSuccess)
       *pbSuccess = TRUE;
   return nodatavalue;
}


/************************************************************************/
/*                            MG4LidarDataset()                             */
/************************************************************************/
MG4LidarDataset::MG4LidarDataset()
{
   reader = NULL;
   fileIO = NULL;

   poXMLPCView = NULL;
   ownsXML = false;
   nOverviewCount = 0;
   papoOverviewDS = NULL;

}
/************************************************************************/
/*                            ~MG4LidarDataset()                             */
/************************************************************************/

MG4LidarDataset::~MG4LidarDataset()

{
   FlushCache();
   if ( papoOverviewDS )
   {
      for( int i = 0; i < nOverviewCount; i++ )
         delete papoOverviewDS[i];
      CPLFree( papoOverviewDS );
   }
   if (ownsXML)
      CPLDestroyXMLNode(poXMLPCView);

   RELEASE(reader);
   RELEASE(fileIO);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MG4LidarDataset::GetGeoTransform( double * padfTransform )

{
   padfTransform[0] = reader->getBounds().x.min ;// Upper left X, Y
   padfTransform[3] = reader->getBounds().y.max; // 
   padfTransform[1] = reader->getBounds().x.length()/GetRasterXSize(); //xRes
   padfTransform[2] = 0.0;

   padfTransform[4] = 0.0;
   padfTransform[5] = -1 * reader->getBounds().y.length()/GetRasterYSize(); //yRes

   return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *MG4LidarDataset::GetProjectionRef()

{
   const char * wkt = CPLGetXMLValue(poXMLPCView, "GeoReference", NULL);
   if (wkt == NULL)
      wkt = reader->getWKT(); 
   return(wkt);
}

/************************************************************************/
/*                             OpenZoomLevel()                          */
/************************************************************************/

CPLErr MG4LidarDataset::OpenZoomLevel( int iZoom )
{
   /* -------------------------------------------------------------------- */
   /*      Get image geometry.                                            */
   /* -------------------------------------------------------------------- */
   iLevel = iZoom;

   // geo dimensions
   const double gWidth = reader->getBounds().x.length() ;
   const double gHeight = reader->getBounds().y.length() ;

   // geo res
   double xRes = pow(RESOLUTION_RATIO, iZoom) * gWidth / MaxRasterSize ;
   double yRes = pow(RESOLUTION_RATIO, iZoom) * gHeight / MaxRasterSize ;
   xRes = yRes = MAX(xRes, yRes);

   // pixel dimensions
   nRasterXSize  = static_cast<int>(gWidth / xRes  + 0.5);
   nRasterYSize  = static_cast<int>(gHeight / yRes + 0.5);

   nBlockXSize = static_cast<int>(MIN(MaxBlockSideSize , GetRasterXSize()));
   nBlockYSize = static_cast<int>(MIN(MaxBlockSideSize , GetRasterYSize())); 

   CPLDebug( "MG4Lidar", "Opened zoom level %d with size %dx%d.\n",
      iZoom, nRasterXSize, nRasterYSize );



   /* -------------------------------------------------------------------- */
   /*  Handle sample type and color space.                                 */
   /* -------------------------------------------------------------------- */
   //eColorSpace = poImageReader->getColorSpace();
   /* -------------------------------------------------------------------- */
   /*      Create band information objects.                                */
   /* -------------------------------------------------------------------- */
   size_t BandCount = 0;
   CPLXMLNode* xmlBand = poXMLPCView;
   bool bClass = false;
   bool bNumRets = false;
   bool bRetNum = false;
   while ((xmlBand = CPLSearchXMLNode(xmlBand, "Band")) != NULL)
   {
      CPLXMLNode * xmlChannel = CPLSearchXMLNode(xmlBand, "Channel");
      const char * name = "Z";
      if (xmlChannel && xmlChannel->psChild && xmlChannel->psChild->pszValue)
         name = xmlChannel->psChild->pszValue;
      
      BandCount++;
      MG4LidarRasterBand *band = new MG4LidarRasterBand(this, BandCount, xmlBand, name);
      SetBand(BandCount, band);
      if (band->papszFilterClassCodes) bClass = true;
      if (band->papszFilterReturnNums) bNumRets = true;
      if (bNumRets && CSLFindString(band->papszFilterReturnNums, "Last")) bRetNum = true;
      xmlBand = xmlBand->psNext;
   }
   nBands = BandCount;
   int nSDKChannels = BandCount + (bClass ? 1 : 0) + (bNumRets ? 1 : 0) + (bRetNum ? 1 : 0);
   if (BandCount == 0)  // default if no bands specified.
   {
      MG4LidarRasterBand *band = new MG4LidarRasterBand(this, 1, NULL, CHANNEL_NAME_Z);
      SetBand(1, band);
      nBands = 1;
      nSDKChannels = 1;
   }
   requiredChannels.init(nSDKChannels);
   const ChannelInfo *ci = NULL;
   for (int i=0; i<nBands; i++)
   {
      ci = reader->getChannel(dynamic_cast<MG4LidarRasterBand*>(papoBands[i])->ChannelName);
      requiredChannels.getChannel(i).init(*ci);
   }
   int iSDKChannels = nBands;
   if (bClass)
   {
      ci = reader->getChannel(CHANNEL_NAME_ClassId);
      requiredChannels.getChannel(iSDKChannels++).init(*ci);
   }
   if (bRetNum)
   {
      ci = reader->getChannel(CHANNEL_NAME_ReturnNum);
      requiredChannels.getChannel(iSDKChannels++).init(*ci);
   }
   if (bNumRets)
   {
      ci = reader->getChannel(CHANNEL_NAME_NumReturns);
      requiredChannels.getChannel(iSDKChannels++).init(*ci);
   }

   return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MG4LidarDataset::Open( GDALOpenInfo * poOpenInfo )

{
#ifdef notdef
   CPLPushErrorHandler( CPLLoggingErrorHandler );
   CPLSetConfigOption( "CPL_DEBUG", "ON" );
   CPLSetConfigOption( "CPL_LOG", "C:\\ArcGIS_GDAL\\jdem\\cpl.log" );
#endif

   if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes < 32 )
      return NULL;

   CPLXMLNode *pxmlPCView;

   // do something sensible for .sid files without a .view
   if( EQUALN((const char *) poOpenInfo->pabyHeader, "msid", 4) )
   {
      int gen;
      bool raster;
      if( !Version::getMrSIDFileVersion(poOpenInfo->pabyHeader, gen, raster)
          || raster )
         return NULL;

      CPLString xmltmp( "<PointCloudView><InputFile>" );
      xmltmp.append( poOpenInfo->pszFilename );
      xmltmp.append( "</InputFile></PointCloudView>" );
      pxmlPCView = CPLParseXMLString( xmltmp );
      if (pxmlPCView == NULL)
         return NULL;
   }
   else
   {
      // support .view xml
      if( !EQUALN((const char *) poOpenInfo->pabyHeader, "<PointCloudView", 15 ) )
         return NULL;

      pxmlPCView = CPLParseXMLFile( poOpenInfo->pszFilename );
      if (pxmlPCView == NULL)
          return NULL;
   }

   CPLXMLNode *psInputFile = CPLGetXMLNode( pxmlPCView, "InputFile" );
   if( psInputFile == NULL )
   {
      CPLError( CE_Failure, CPLE_OpenFailed, 
         "Failed to find <InputFile> in document." );
      CPLDestroyXMLNode(pxmlPCView);
      return NULL;
   }
   CPLString sidInputName(psInputFile->psChild->pszValue);
   if (CPLIsFilenameRelative(sidInputName))
   {
      CPLString dirname(CPLGetDirname(poOpenInfo->pszFilename));
      sidInputName = CPLString(CPLFormFilename(dirname, sidInputName, NULL));
   }
   GDALOpenInfo openinfo(sidInputName, GA_ReadOnly);

   /* -------------------------------------------------------------------- */
   /*      Check that particular fields in the header are valid looking    */
   /*      dates.                                                          */
   /* -------------------------------------------------------------------- */
   if( openinfo.fpL == NULL || openinfo.nHeaderBytes < 50 )
   {
      CPLDestroyXMLNode(pxmlPCView);
      return NULL;
   }

   /* check magic */
   // to do:  SDK should provide an API for this.
   if(  !EQUALN((const char *) openinfo.pabyHeader, "msid", 4)
      || (*(openinfo.pabyHeader+4) != 0x4 )) // Generation 4.  ... is there more we can check?
   {
      CPLDestroyXMLNode(pxmlPCView);
      return NULL;
   }

   /* -------------------------------------------------------------------- */
   /*      Create a corresponding GDALDataset.                             */
   /* -------------------------------------------------------------------- */
   MG4LidarDataset 	*poDS;

   poDS = new MG4LidarDataset();
   poDS->poXMLPCView = pxmlPCView;
   poDS->ownsXML = true;
   poDS->reader = CropableMG4PointReader::create();
   poDS->fileIO = FileIO::create();

   const char * pszClipExtent = CPLGetXMLValue(pxmlPCView, "ClipBox", NULL);
   MG4PointReader *r = MG4PointReader::create();
   FileIO* io = FileIO::create();

#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
   bool bIsUTF8 = CSLTestBoolean( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) );
   wchar_t *pwszFilename = NULL;
   if (bIsUTF8)
   {
      pwszFilename = CPLRecodeToWChar(openinfo.pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2);
      if (!pwszFilename)
      {
         RELEASE(r);
         RELEASE(io);
         return NULL;
       }
       io->init(pwszFilename, "r");
   }
   else
      io->init(openinfo.pszFilename, "r");
#else
   io->init(openinfo.pszFilename, "r");
#endif
   r->init(io);
   Bounds bounds = r->getBounds();
   if (pszClipExtent)
   {
      char ** papszClipExtent = CSLTokenizeString(pszClipExtent);
      int cslcount = CSLCount(papszClipExtent);
      if (cslcount != 4 && cslcount != 6)
      {
         CPLError( CE_Failure, CPLE_OpenFailed, 
            "Invalid ClipBox.  Must contain 4 or 6 floats." );
         CSLDestroy(papszClipExtent);
         delete poDS;
         RELEASE(r);
         RELEASE(io);
#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
         if ( pwszFilename ) 
            CPLFree( pwszFilename );
#endif
         return NULL;
      }
      if (!EQUAL(papszClipExtent[0], "NOFILTER"))
         bounds.x.min = CPLAtof(papszClipExtent[0]);
      if (!EQUAL(papszClipExtent[1], "NOFILTER"))
         bounds.x.max = CPLAtof(papszClipExtent[1]);
      if (!EQUAL(papszClipExtent[2], "NOFILTER"))
         bounds.y.min = CPLAtof(papszClipExtent[2]);
      if (!EQUAL(papszClipExtent[3], "NOFILTER"))
         bounds.y.max = CPLAtof(papszClipExtent[3]);
      if (cslcount == 6)
      {
         if (!EQUAL(papszClipExtent[4], "NOFILTER"))
            bounds.z.min = CPLAtof(papszClipExtent[4]);
         if (!EQUAL(papszClipExtent[5], "NOFILTER"))
            bounds.z.max = CPLAtof(papszClipExtent[5]);
      }
      CSLDestroy(papszClipExtent);
   }

#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
   if (bIsUTF8)
   {
      poDS->fileIO->init(pwszFilename, "r");
      CPLFree(pwszFilename);
   }
   else
      poDS->fileIO->init( openinfo.pszFilename, "r" );
#else
   poDS->fileIO->init( openinfo.pszFilename, "r" );
#endif
   dynamic_cast<CropableMG4PointReader *>(poDS->reader)->init(poDS->fileIO, &bounds);
   poDS->SetDescription(poOpenInfo->pszFilename);
   poDS->TryLoadXML(); 

   double pts_per_area = ((double)r->getNumPoints())/(r->getBounds().x.length()*r->getBounds().y.length());
   double average_pt_spacing = sqrt(1.0 / pts_per_area) ;
   double cell_side = average_pt_spacing;
   const char * pszCellSize = CPLGetXMLValue(pxmlPCView, "CellSize", NULL);
   if (pszCellSize)
      cell_side = CPLAtof(pszCellSize);
   MaxRasterSize = MAX(poDS->reader->getBounds().x.length()/cell_side, poDS->reader->getBounds().y.length()/cell_side);

   RELEASE(r);
   RELEASE(io);

   // Calculate the number of levels to expose.  The highest level correpsonds to a
   // raster size of 256 on the longest side.
   double blocksizefactor = MaxRasterSize/256.0;
   poDS->nOverviewCount = MAX(0, (int)(log(blocksizefactor)/log(RESOLUTION_RATIO) + 0.5));
   if ( poDS->nOverviewCount > 0 )
   {
      int i;

      poDS->papoOverviewDS = (MG4LidarDataset **)
         CPLMalloc( poDS->nOverviewCount * (sizeof(void*)) );

      for ( i = 0; i < poDS->nOverviewCount; i++ )
      {
         poDS->papoOverviewDS[i] = new MG4LidarDataset ();
         poDS->papoOverviewDS[i]->reader = RETAIN(poDS->reader);
         poDS->papoOverviewDS[i]->SetMetadata(poDS->GetMetadata("MG4Lidar"), "MG4Lidar");
         poDS->papoOverviewDS[i]->poXMLPCView = pxmlPCView;
         poDS->papoOverviewDS[i]->OpenZoomLevel( i+1 );
      }       
   }

   /* -------------------------------------------------------------------- */
   /*      Create object for the whole image.                              */
   /* -------------------------------------------------------------------- */
   poDS->OpenZoomLevel( 0 );

   CPLDebug( "MG4Lidar",
      "Opened image: width %d, height %d, bands %d",
      poDS->nRasterXSize, poDS->nRasterYSize, poDS->nBands );

   if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
   {
       delete poDS;
       return NULL;
   }

   if (! ((poDS->nBands == 1) || (poDS->nBands == 3)))
   {
      CPLDebug( "MG4Lidar",
         "Inappropriate number of bands (%d)", poDS->nBands );
      delete poDS;
      return(NULL);
   }

   return( poDS );
}

/************************************************************************/
/*                          GDALRegister_MG4Lidar()                        */
/************************************************************************/

void GDALRegister_MG4Lidar()

{
   GDALDriver	*poDriver;

    if (! GDAL_CHECK_VERSION("MG4Lidar driver"))
        return;

   if( GDALGetDriverByName( "MG4Lidar" ) == NULL )
   {
      poDriver = new GDALDriver();

      poDriver->SetDescription( "MG4Lidar" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
      poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
         "MrSID Generation 4 / Lidar (.sid)" );
      // To do:  update this help file in gdal.org
      poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
         "frmt_mrsid_lidar.html" );

      poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "view" );
      poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
         "Float64" );

      poDriver->pfnOpen = MG4LidarDataset::Open;

      GetGDALDriverManager()->RegisterDriver( poDriver );
   }
}
