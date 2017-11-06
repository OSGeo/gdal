/************************************************************************/
/* ==================================================================== */
/*                              WCSDataset                              */
/* ==================================================================== */
/************************************************************************/

class WCSRasterBand;

class CPL_DLL WCSDataset : public GDALPamDataset
{
  friend class WCSRasterBand;
  friend class WCSDataset100;
  friend class WCSDataset110;
  friend class WCSDataset201;
  
    int         bServiceDirty;
    CPLXMLNode *psService;

    char       *apszCoverageOfferingMD[2];

    char      **papszSDSModifiers;

    int         m_Version; // eg 100 for 1.0.0, 110 for 1.1.0
    const char *Version();

    CPLString   osCRS; // name of the CRS
    char        *pszProjection; // (usually the) WKT of the CRS, from OGRSpatialReference.exportToWkt
    bool        native_crs; // the CRS is the native CRS of the server
    bool        axis_order_swap; // the CRS requires x and y coordinates to be swapped for requests
    double      adfGeoTransform[6];
    bool        SetCRS(CPLString crs, bool native);
    void        SetGeometry(std::vector<double> envelope, // xmin, ymin, xmax, ymax
                            std::vector<CPLString> axis_order,
                            std::vector<int> size, // 
                            std::vector<std::vector<double>> offsets);

    CPLString   osBandIdentifier;

    CPLString   osDefaultTime;
    std::vector<CPLString> aosTimePositions;

    int         TestUseBlockIO( int, int, int, int, int, int );
    CPLErr      DirectRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg);

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    virtual std::vector<double> GetExtent(int nXOff, int nYOff,
                                          int nXSize, int nYSize,
                                          int nBufXSize, int nBufYSize) = 0;

    virtual CPLString   GetCoverageRequest( bool scaled,
                                            int nBufXSize, int nBufYSize,
                                            std::vector<double> extent,
                                            CPLString osBandList) = 0;

    CPLErr      GetCoverage( int nXOff, int nYOff,
                             int nXSize, int nYSize,
                             int nBufXSize, int nBufYSize,
                             int nBandCount, int *panBandList,
                             CPLHTTPResult **ppsResult );

    virtual CPLString   DescribeCoverageRequest() {return "";};
    virtual CPLXMLNode *CoverageOffering(CPLXMLNode *psDC) = 0;

    int         DescribeCoverage();
    
    virtual bool        ExtractGridInfo() = 0;

    int         EstablishRasterDetails();

    virtual CPLErr      ParseCapabilities( CPLXMLNode *, CPLString ) = 0;

    virtual const char *ExceptionNodeName() = 0;
    
    int         ProcessError( CPLHTTPResult *psResult );
    
    GDALDataset *GDALOpenResult( CPLHTTPResult *psResult );
    
    void        FlushMemoryResult();
    
    CPLString   osResultFilename;
    
    GByte      *pabySavedDataBuffer;

    char      **papszHttpOptions;

    int         nMaxCols;
    int         nMaxRows;

  public:
                WCSDataset(int version);
    virtual ~WCSDataset();

    static WCSDataset *CreateFromMetadata( CPLString );
    static WCSDataset *CreateFromCapabilities(GDALOpenInfo *, CPLString, CPLString );
    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *GetProjectionRef(void) override;
    virtual char **GetFileList(void) override;

    virtual char      **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char *pszDomain ) override;
};

class CPL_DLL WCSDataset100 : public WCSDataset
{
    std::vector<double> GetExtent(int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize) override;
    CPLString   GetCoverageRequest( bool scaled,
                                    int nBufXSize, int nBufYSize,
                                    std::vector<double> extent,
                                    CPLString osBandList) override;
    CPLString   DescribeCoverageRequest() override;
    CPLXMLNode *CoverageOffering(CPLXMLNode *psDC) override;
    bool        ExtractGridInfo() override;
    CPLErr      ParseCapabilities( CPLXMLNode *, CPLString ) override;
    const char *ExceptionNodeName() override;
    
  public:

    WCSDataset100() : WCSDataset(100) {};
};

class CPL_DLL WCSDataset110 : public WCSDataset
{
    std::vector<double> GetExtent(int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize) override;
    CPLString   GetCoverageRequest( bool scaled,
                                    int nBufXSize, int nBufYSize,
                                    std::vector<double> extent,
                                    CPLString osBandList) override;
    CPLString   DescribeCoverageRequest() override;
    CPLXMLNode *CoverageOffering(CPLXMLNode *psDC) override;
    bool        ExtractGridInfo() override;
    CPLErr      ParseCapabilities( CPLXMLNode *, CPLString ) override;
    const char *ExceptionNodeName() override;
    
  public:

    WCSDataset110(int version) : WCSDataset(version) {};
};

class CPL_DLL WCSDataset201 : public WCSDataset110
{
    std::vector<double> GetExtent(int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize) override;
    CPLString   GetSubdataset(CPLString coverage);
    bool        SetFormat(CPLXMLNode *coverage);
    bool        ParseGridFunction(std::vector<CPLString> &axisOrder);
    int         ParseRange(CPLXMLNode *coverage, char ***metadata);
    CPLString   GetCoverageRequest( bool scaled,
                                    int nBufXSize, int nBufYSize,
                                    std::vector<double> extent,
                                    CPLString osBandList) override;
    CPLString   DescribeCoverageRequest() override;
    bool        Offset2GeoTransform(std::vector<double> origin,
                                    std::vector<std::vector<double>> offset);
    bool        ExtractGridInfo() override;
    
 public:
    
    WCSDataset201() : WCSDataset110(201) {};
    
};

#define DIGIT_ZERO '0'

