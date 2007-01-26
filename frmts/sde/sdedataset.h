#ifndef SDERASTER_INCLUDED
#define SDERASTER_INCLUDED

#include "gdal_sde.h"


class SDEDataset : public GDALPamDataset
{
    friend class SDERasterBand;

    private:
        
        // SDE-specific stuff
        SE_CONNECTION*      hConnection;
        char                *pszLayerName;
        char                *pszColumnName;
        long                nSubDataCount;
        long                nBands;
        long                nRasterXSize;
        long                nRasterYSize;
        
        double              dfMinX, dfMaxX, dfMinY, dfMaxY;
        
        GDALDataType        eDataType;
        SE_RASCOLINFO* paohSDERasterColumns;
        SE_RASCOLINFO hRasterColumn;

        
        CPLErr                  ComputeRasterInfo(void);
        SE_RASBANDINFO* paohSDERasterBands;
        
    public:
        SDEDataset(SE_CONNECTION* connection);
        ~SDEDataset();
        
        static GDALDataset *Open( GDALOpenInfo * );
        
        CPLErr  GetGeoTransform( double * padfTransform );
        int     GetRasterCount(void);
        int     GetRasterXSize(void);
        int     GetRasterYSize(void);
        
        const char *GetProjectionRef();
};



#endif
