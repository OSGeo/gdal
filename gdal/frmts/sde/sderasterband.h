#ifndef SDERASTERBAND_INCLUDED
#define SDERASTERBAND_INCLUDED

#include "gdal_sde.h"

/************************************************************************/
/* ==================================================================== */
/*                            SDERasterBand                             */
/* ==================================================================== */
/************************************************************************/
class SDEDataset;

class SDERasterBand : public GDALRasterBand
{
    friend class SDEDataset;

    private:
        const SE_RASBANDINFO* poBand;

        static double                  MorphESRIRasterDepth( int gtype );
        static GDALDataType            MorphESRIRasterType( int gtype );
        void                    ComputeColorTable();
        CPLErr                  InitializeBand( int nOverview );
        SE_QUERYINFO&           InitializeQuery();
        SE_RASCONSTRAINT&       InitializeConstraint (  long* nBlockXOff,
                                                        long* nBlockYOff);
        CPLErr                  QueryRaster( SE_RASCONSTRAINT& constraint ) const;

        int                     nOverview;
        int                     nOverviews;
        long                    nBlockSize;
        double                  dfDepth;
        LONG                    nSDERasterType;
        SE_QUERYINFO            hQuery;
        SE_RASCONSTRAINT        hConstraint;
        GDALRasterBand**        papoOverviews;
        GDALColorTable*         poColorTable;

    public:
        SDERasterBand( SDEDataset* poDS,
                       int nBand,
                       int nOverview,
                       const SE_RASBANDINFO* band);

        ~SDERasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *pdfStdDev ) override;
    virtual GDALDataType GetRasterDataType();
    virtual GDALColorTable *GetColorTable() override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual double GetMinimum( int *pbSuccess ) override;
    virtual double GetMaximum( int *pbSuccess ) override;
    virtual int GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int nOverview) override;
};

#endif
