#include "gdal_pam.h"


CPL_CVSID("$Id: sdedataset.cpp 10650 2007-01-18 05:02:33Z hobu $");

CPL_C_START
void    GDALRegister_SDE(void);


CPL_C_END

#include <sdetype.h>
#include <sdeerno.h>
#include <sderaster.h>

#include "cpl_string.h"
#include "ogr_spatialref.h"




/************************************************************************/
/* ==================================================================== */
/*              SDEDataset              */
/* ==================================================================== */
/************************************************************************/

typedef struct SDERasterColumns {
  char         szTableName[SE_QUALIFIED_TABLE_NAME+1];
  char         szColumnName[SE_MAX_COLUMN_LEN+1];
  SE_RASCOLINFO* hSDERasterColumn;
} SDERasterColumns;


class SDERasterBand;

class SDEDataset : public GDALPamDataset
{
    friend class SDERasterBand;

    private:
        
        // SDE-specific stuff
        SE_CONNECTION*      hConnection;
        char                *pszLayerName;
        char                *pszColumnName;
        long                nSubDataCount;
        SE_RASCOLINFO* paohSDERasterColumns;
        SE_RASCOLINFO pohSDERasterColumn;

        
        GDALColorTable *poCT;

    public:
        SDEDataset(SE_CONNECTION* connection);
        ~SDEDataset();
        
        static GDALDataset *Open( GDALOpenInfo * );
        
        CPLErr  GetGeoTransform( double * padfTransform );
        const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            SDERasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SDERasterBand : public GDALPamRasterBand
{
    friend class SDEDataset;
    
    public:

        SDERasterBand( SDEDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
//    virtual double GetMinimum( int *pbSuccess );
//    virtual double GetMaximum( int *pbSuccess );
//    virtual double GetNoDataValue( int *pbSuccess );
//
//    virtual GDALColorInterp GetColorInterpretation();
//    virtual GDALColorTable *GetColorTable();
};
