#include "../vrt/vrtdataset.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"

#include <iostream>

CPLErr ModulePixelFunc(void **papoSources, int nSources, void *pData,
                       int nXSize, int nYSize,
                       GDALDataType eSrcType, GDALDataType eBufType,
                       int nPixelSpace, int nLineSpace)
{
    int ii, iLine, iCol;
    double dfPixVal;
  
    /* ---- Init ---- */
    if (nSources != 1) return CE_Failure;

    if (GDALDataTypeIsComplex( eSrcType ))
    {
         double dfReal, dfImag;
        void *pReal = papoSources[0];
        void *pImag = ((GByte *)papoSources[0])
                    + GDALGetDataTypeSize( eSrcType ) / 8 / 2;

        /* ---- Set pixels ---- */
        for( iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( iCol = 0; iCol < nXSize; ++iCol, ++ii ) {

                /* Source raster pixels may be obtained with SRCVAL macro */
                dfReal = SRCVAL(pReal, eSrcType, ii);
                dfImag = SRCVAL(pImag, eSrcType, ii);
                
                dfPixVal = sqrt( dfReal * dfReal + dfImag * dfImag );
                
                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              ((GByte *)pData) + nLineSpace * iLine +
                              iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    } else {
        /* ---- Set pixels ---- */
        for( iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( iCol = 0; iCol < nXSize; ++iCol, ++ii ) {

                /* Source raster pixels may be obtained with SRCVAL macro */
                dfPixVal = fabs(SRCVAL(papoSources[0], eSrcType, ii));

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              ((GByte *)pData) + nLineSpace * iLine +
                              iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
} /* ModulePixelFunc */


class ComplexDerivedDatasetContainer: public GDALPamDataset
{
    public:
        ComplexDerivedDatasetContainer() {}
};


class ComplexDerivedDataset : public VRTDataset
{
  public:
  ComplexDerivedDataset(int nXSize, int nYSize);
  ~ComplexDerivedDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};


ComplexDerivedDataset::ComplexDerivedDataset(int nXSize, int nYSize) : VRTDataset(nXSize,nYSize)
{
  poDriver = NULL;
  SetWritable(FALSE);
}

ComplexDerivedDataset::~ComplexDerivedDataset()
{
}

int ComplexDerivedDataset::Identify(GDALOpenInfo * poOpenInfo)
{
  if(STARTS_WITH_CI(poOpenInfo->pszFilename, "DERIVED_SUBDATASET:COMPLEX_AMPLITUDE:"))
    return TRUE;
  
  return FALSE;
}

GDALDataset * ComplexDerivedDataset::Open(GDALOpenInfo * poOpenInfo)
{
  if( !Identify(poOpenInfo) )
    {
    return NULL;
    }

  /* Try to open original dataset */
  CPLString filename(poOpenInfo->pszFilename);

  // TODO: check starts with
  
  /* DERIVED_SUBDATASET should be first domain */
  size_t dsds_pos = filename.find("DERIVED_SUBDATASET:");

  if (dsds_pos == std::string::npos)
    {
    /* Unable to Open in this case */
    return NULL;
    }

  /* DERIVED_SUBDATASET should be first domain */
  size_t alg_pos = filename.find(":",dsds_pos+20);
  if (alg_pos == std::string::npos)
    {
    /* Unable to Open in this case */
    return NULL;
    }

  VRTDerivedRasterBand::AddPixelFunction("mod",ModulePixelFunc);
  
  CPLString odFilename = filename.substr(alg_pos+1,filename.size() - alg_pos);

  GDALDataset * poTmpDS = (GDALDataset*)GDALOpen(odFilename, GA_ReadOnly);
  
   if( poTmpDS == NULL )
     return NULL;

   int nbBands = poTmpDS->GetRasterCount();

   int nRows = poTmpDS->GetRasterYSize();
   int nCols = poTmpDS->GetRasterXSize();

   ComplexDerivedDataset * poDS = new ComplexDerivedDataset(nCols,nRows);

   // Transfer metadata
   poDS->SetMetadata(poTmpDS->GetMetadata());

   // Transfer projection
   poDS->SetProjection(poTmpDS->GetProjectionRef());
   
   // Transfer geotransform
   double padfTransform[6];
   bool transformOk = poTmpDS->GetGeoTransform(padfTransform);
   if(transformOk)
     {
     poDS->SetGeoTransform(padfTransform);
     }
   
   // Transfer GCPs
   const char * gcpProjection = poTmpDS->GetGCPProjection();
   int nbGcps = poTmpDS->GetGCPCount();
   poDS->SetGCPs(nbGcps,poTmpDS->GetGCPs(),gcpProjection);

   // Map bands
   for(int nBand = 1; nBand <= nbBands; ++nBand)
     {
     VRTDerivedRasterBand * poBand;    

     GDALDataType type  = GDT_Float64;
     
     poBand = new VRTDerivedRasterBand(poDS,nBand,type,nCols,nRows);
     poBand->SetPixelFunctionName("mod");
     poBand->SetSourceTransferType(poTmpDS->GetRasterBand(nBand)->GetRasterDataType());
     poDS->SetBand(nBand,poBand);

     GDALProxyPoolDataset* proxyDS;
     proxyDS = new GDALProxyPoolDataset(         odFilename,
                                                 poDS->nRasterXSize,
                                                 poDS->nRasterYSize,
                                                 GA_ReadOnly,
                                                 TRUE);
     proxyDS->AddSrcBandDescription(poTmpDS->GetRasterBand(nBand)->GetRasterDataType(), 128, 128);
     
     poBand->AddComplexSource(proxyDS->GetRasterBand(nBand),0,0,nCols,nRows,0,0,nCols,nRows);
     proxyDS->Dereference();     
     }

   GDALClose(poTmpDS);

   return poDS;
}

void GDALRegister_ComplexDerived()
{
    if( GDALGetDriverByName( "COMPLEXDERIVED" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "COMPLEXDERIVED" );
#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
#endif
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Complex derived bands" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "TODO" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "NO" );

    poDriver->pfnOpen = ComplexDerivedDataset::Open;
    poDriver->pfnIdentify = ComplexDerivedDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
