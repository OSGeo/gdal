#ifndef SDERASTER_INCLUDED
#define SDERASTER_INCLUDED

#include "gdal_sde.h"


class SDEDataset : public GDALDataset
{
    friend class SDERasterBand;

    private:
        LONG                nSubDataCount;
        char*               pszWKT;

        double              dfMinX, dfMaxX, dfMinY, dfMaxY;

        GDALDataType        eDataType;
        SE_RASCOLINFO*      paohSDERasterColumns;
        SE_RASCOLINFO       hRasterColumn;


        CPLErr              ComputeRasterInfo(void);
        SE_RASBANDINFO*     paohSDERasterBands;

    public:
        SDEDataset();
        ~SDEDataset();

        static GDALDataset *Open( GDALOpenInfo * );

    protected:
        // SDE-specific stuff
        SE_CONNECTION      hConnection;
        SE_RASTERATTR      hAttributes;
        SE_STREAM          hStream;

        char                *pszLayerName;
        char                *pszColumnName;

        virtual CPLErr  GetGeoTransform( double * padfTransform );
        virtual int     GetRasterCount(void);
        virtual int     GetRasterXSize(void);
        virtual int     GetRasterYSize(void);

        const char *GetProjectionRef();
};

#endif
