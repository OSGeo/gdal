#ifndef KMLSUPEROVERLAYDATASET_H_INCLUDED
#define KMLSUPEROVERLAYDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"

CPL_C_START
void CPL_DLL GDALRegister_KMLSUPEROVERLAY(void);
CPL_C_END

/************************************************************************/
/*				KmlSuperOverlayDataset				*/
/************************************************************************/
class OGRCoordinateTransformation;

class CPL_DLL KmlSuperOverlayDataset : public GDALDataset
{
  private:

    int         bGeoTransformSet;
    char        *pszProjection;

  public:
                 KmlSuperOverlayDataset();
    virtual      ~KmlSuperOverlayDataset();

    static GDALDataset *Open(GDALOpenInfo *);    

    static GDALDataset *Create(const char* pszFilename, int nXSize, int nYSize, int nBands,
      GDALDataType eType, char **papszOptions);

    static GDALDataset *CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
      int bStrict, char ** papszOptions, GDALProgressFunc pfnProgress, void * pProgressData );

    const char *GetProjectionRef();
};

#endif /* ndef KMLSUPEROVERLAYDATASET_H_INCLUDED */

