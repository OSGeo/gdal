#ifndef SDERASTERBAND_INCLUDED
#define SDERASTERBAND_INCLUDED

#include "gdal_sde.h"

/************************************************************************/
/* ==================================================================== */
/*                            SDERasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SDERasterBand : public GDALPamRasterBand
{
    friend class SDEDataset;
    
    public:

        SDERasterBand( SDEDataset * poDS, int nBand , const SE_RASBANDINFO& band);
    
    GDALColorTable*         ComputeColorTable(const SE_RASBANDINFO& band);
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax, 
                                  double *pdfMean, double *pdfStdDev );
//    virtual double GetMinimum( int *pbSuccess );
//    virtual double GetMaximum( int *pbSuccess );
//    virtual double GetNoDataValue( int *pbSuccess );
//
//    virtual GDALColorInterp GetColorInterpretation();
//    virtual GDALColorTable *GetColorTable();
};

#endif
