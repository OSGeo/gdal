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

        CPLErr              ComputeRasterInfo();
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

        virtual CPLErr  GetGeoTransform( double * padfTransform ) override;
        virtual int     GetRasterCount();
        virtual int     GetRasterXSize();
        virtual int     GetRasterYSize();

        const char *GetProjectionRef() override;
};

#endif
