#include "../vrt/vrtdataset.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "derivedlist.h"

#include <iostream>

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
};


ComplexDerivedDataset::ComplexDerivedDataset(int nXSize, int nYSize) : VRTDataset(nXSize,nYSize)
{
  poDriver = NULL;
  SetWritable(FALSE);
}

ComplexDerivedDataset::~ComplexDerivedDataset()
{
}

GDALDataset * ComplexDerivedDataset::Open(GDALOpenInfo * poOpenInfo)
{
  /* Try to open original dataset */
  CPLString filename(poOpenInfo->pszFilename);
 
  /* DERIVED_SUBDATASET should be first domain */
  size_t dsds_pos = filename.find("DERIVED_SUBDATASET:");

  if (dsds_pos == std::string::npos)
    {
    /* Unable to Open in this case */
    return NULL;
    }

  /* Next, we need to now which derived dataset to compute */
  size_t alg_pos = filename.find(":",dsds_pos+20);
  if (alg_pos == std::string::npos)
    {
    /* Unable to Open if we do not find the name of the derived dataset */
    return NULL;
    }

  CPLString odDerivedName = filename.substr(dsds_pos+19,alg_pos-dsds_pos-19);

  CPLDebug("ComplexDerivedDataset::Open","Derived dataset identified: %s",odDerivedName.c_str());

  CPLString pixelFunctionName = "";
  bool datasetFound = false;
  
  for(unsigned int derivedId = 0; derivedId<NB_DERIVED_DATASETS;++derivedId)
    {
    if(odDerivedName == asDDSDesc[derivedId].pszDatasetName)
      {
      datasetFound = true;
      pixelFunctionName = asDDSDesc[derivedId].pszPixelFunction;
      }
    }

  if(!datasetFound)
    {
    return NULL;
    }
  
  CPLString odFilename = filename.substr(alg_pos+1,filename.size() - alg_pos);

  GDALDataset * poTmpDS = (GDALDataset*)GDALOpen(odFilename, GA_ReadOnly);
  
   if( poTmpDS == NULL )
     return NULL;

   int nbBands = poTmpDS->GetRasterCount();
  
   if(nbBands == 0)
     {
     GDALClose(poTmpDS);
     return NULL;
     }

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
     poDS->SetBand(nBand,poBand);
     
     poBand->SetPixelFunctionName("mod");
     poBand->SetSourceTransferType(poTmpDS->GetRasterBand(nBand)->GetRasterDataType());
 
     GDALProxyPoolDataset* proxyDS;
     proxyDS = new GDALProxyPoolDataset(         odFilename,
                                                 poDS->nRasterXSize,
                                                 poDS->nRasterYSize,
                                                 GA_ReadOnly,
                                                 TRUE);
     for(int j=0;j<nbBands;++j)
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
