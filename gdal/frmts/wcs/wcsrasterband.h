/************************************************************************/
/* ==================================================================== */
/*                            WCSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class WCSRasterBand : public GDALPamRasterBand
{
    friend class WCSDataset;

    int            iOverview;
    int            nResFactor;

    WCSDataset    *poODS;

    int            nOverviewCount;
    WCSRasterBand **papoOverviews;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

  public:

                   WCSRasterBand( WCSDataset *, int nBand, int iOverview );
    virtual ~WCSRasterBand();

    virtual double GetNoDataValue( int *pbSuccess = NULL ) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
};
