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
        
        GDALDataType            MorphESRIRasterType( int gtype );
        GDALColorTable*         ComputeColorTable( void );  
        CPLErr                  InitializeBand( int nOverview );
        SE_QUERYINFO&           InitializeQuery( void ); 
        SE_RASCONSTRAINT&       InitializeConstraint (  long nBlockXOff,
                                                        long nBlockYOff);
        CPLErr                  QueryRaster( SE_RASCONSTRAINT& constraint );
        
		int						nOverview;
        int                     nOverviews;
        SE_STREAM               hStream;
        long                    nBlockSize;
        SE_QUERYINFO            hQuery;
        SE_RASCONSTRAINT        hConstraint;
        
    public:

        SDERasterBand( SDEDataset * poDS, 
                       int nBand, 
                       int nOverview, 
                       const SE_RASBANDINFO* band);
                       
        ~SDERasterBand( void );
    

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax, 
                                  double *pdfMean, double *pdfStdDev );
    virtual GDALDataType GetRasterDataType(void);
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();


    virtual double GetMinimum( int *pbSuccess );
    virtual double GetMaximum( int *pbSuccess );
    virtual int GetOverviewCount(void);
	virtual GDALRasterBand* GetOverview(int nOverview);
//    virtual double GetNoDataValue( int *pbSuccess );
//
};

#endif
