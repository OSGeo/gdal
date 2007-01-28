#ifndef SDERASTERBAND_INCLUDED
#define SDERASTERBAND_INCLUDED

#include "gdal_sde.h"

/************************************************************************/
/* ==================================================================== */
/*                            SDERasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SDERasterBand : public GDALRasterBand
{
    friend class SDEDataset;
    
    private:
        const SE_RASBANDINFO* poBand;
        
        GDALDataType            MorphESRIRasterType(int gtype);
        GDALColorTable*         ComputeColorTable();  
          
    public:

        SDERasterBand( SDEDataset * poDS, int nBand , const SE_RASBANDINFO* band);
    

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax, 
                                  double *pdfMean, double *pdfStdDev );
    virtual GDALDataType GetRasterDataType(void);
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();


    virtual double GetMinimum( int *pbSuccess );
    virtual double GetMaximum( int *pbSuccess );
//    virtual double GetNoDataValue( int *pbSuccess );
//
};

#endif
