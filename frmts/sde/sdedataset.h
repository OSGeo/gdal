#ifndef SDERASTER_INCLUDED
#define SDERASTER_INCLUDED

#include "gdal_sde.h"


class SDEDataset : public GDALDataset
{
    friend class SDERasterBand;

    private:
        
        // SDE-specific stuff
        SE_CONNECTION*      hConnection;
        char                *pszLayerName;
        char                *pszColumnName;
        long                nSubDataCount;

        
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
    protected:        

        virtual CPLErr  GetGeoTransform( double * padfTransform );
        virtual int     GetRasterCount(void);
        virtual int     GetRasterXSize(void);
        virtual int     GetRasterYSize(void);
        
        const char *GetProjectionRef();
};



#endif
