/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  GDALDataset/GDALRasterBand declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_pam.h"
#include "nitflib.h"
#include "ogr_spatialref.h"
#include "gdal_proxy.h"
#include <map>

CPLErr NITFSetColorInterpretation( NITFImage *psImage, 
                                   int nBand,
                                   GDALColorInterp eInterp );

/* Unused in normal builds. Caller code in nitfdataset.cpp is protected by #ifdef ESRI_BUILD */
#ifdef ESRI_BUILD
/* -------------------------------------------------------------------- */
/*      Functions in nitf_gcprpc.cpp.                                   */
/* -------------------------------------------------------------------- */

void NITFDensifyGCPs( GDAL_GCP **psGCPs, int *pnGCPCount );
void NITFUpdateGCPsWithRPC( NITFRPC00BInfo *psRPCInfo,
                            GDAL_GCP       *psGCPs,
                            int            *pnGCPCount );
#endif

/************************************************************************/
/* ==================================================================== */
/*				NITFDataset				*/
/* ==================================================================== */
/************************************************************************/

class NITFRasterBand;
class NITFWrapperRasterBand;

class NITFDataset : public GDALPamDataset
{
    friend class NITFRasterBand;
    friend class NITFWrapperRasterBand;

    NITFFile    *psFile;
    NITFImage   *psImage;

    GDALPamDataset *poJ2KDataset;
    int         bJP2Writing;

    GDALPamDataset *poJPEGDataset;

    int         bGotGeoTransform;
    double      adfGeoTransform[6];

    char        *pszProjection;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

    GDALMultiDomainMetadata oSpecialMD;

#ifdef ESRI_BUILD
    void         InitializeNITFDESMetadata();
    void         InitializeNITFDESs();
    void         InitializeNITFTREs();
#endif
    void         InitializeNITFMetadata();
    void         InitializeCGMMetadata();
    void         InitializeTextMetadata();
    void         InitializeTREMetadata();

    GIntBig     *panJPEGBlockOffset;
    GByte       *pabyJPEGBlock;
    int          nQLevel;

    int          ScanJPEGQLevel( GUIntBig *pnDataStart );
    CPLErr       ScanJPEGBlocks( void );
    CPLErr       ReadJPEGBlock( int, int );
    void         CheckGeoSDEInfo();
    char**       AddFile(char **papszFileList, const char* EXTENSION, const char* extension);

    int          nIMIndex;
    CPLString    osNITFFilename;

    CPLString    osRSetVRT;
    int          CheckForRSets( const char *pszFilename );

    char       **papszTextMDToWrite;
    char       **papszCgmMDToWrite;
    
    int          bInLoadXML;

  protected:
    virtual int         CloseDependentDatasets();

  public:
                 NITFDataset();
                 ~NITFDataset();

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, 
                               int nBandCount, int *panBandList,
                               char **papszOptions );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual char **GetFileList(void);

    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual void   FlushCache();
    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *OpenInternal( GDALOpenInfo *, GDALDataset *poWritableJ2KDataset,
                              int bOpenForCreate);
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *
    NITFCreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                    int bStrict, char **papszOptions, 
                    GDALProgressFunc pfnProgress, void * pProgressData );
    static GDALDataset *
             NITFDatasetCreate( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions );

};

/************************************************************************/
/* ==================================================================== */
/*                            NITFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class NITFRasterBand : public GDALPamRasterBand
{
    friend class NITFDataset;

    NITFImage   *psImage;

    GDALColorTable *poColorTable;

    GByte       *pUnpackData;

    int          bScanlineAccess;

  public:
                   NITFRasterBand( NITFDataset *, int );
                  ~NITFRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr SetColorInterpretation( GDALColorInterp );
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual double GetNoDataValue( int *pbSuccess = NULL );

    void Unpack(GByte* pData);
};

/************************************************************************/
/* ==================================================================== */
/*                        NITFProxyPamRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/* This class is potentially of general interest and could be moved to gdal_proxy.h */
/* We don't proxy all methods. Generally speaking, the getters go to PAM first and */
/* then to the underlying band if no value exist in PAM. The setters aren't */
/* overriden, so they go to PAM */

class NITFProxyPamRasterBand : public GDALPamRasterBand
{
    private:
        std::map<CPLString, char**> oMDMap;

    protected:
        virtual GDALRasterBand* RefUnderlyingRasterBand() = 0;
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand);

        virtual CPLErr IReadBlock( int, int, void * );
        virtual CPLErr IWriteBlock( int, int, void * );
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int );

    public:
                         ~NITFProxyPamRasterBand();

        virtual char      **GetMetadata( const char * pszDomain = ""  );
        /*virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain = ""  );*/
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain = "" );
        /*virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain = "" );*/
        virtual CPLErr FlushCache();
        /*virtual char **GetCategoryNames();*/
        virtual double GetNoDataValue( int *pbSuccess = NULL );
        virtual double GetMinimum( int *pbSuccess = NULL );
        virtual double GetMaximum(int *pbSuccess = NULL );
        /*virtual double GetOffset( int *pbSuccess = NULL );
        virtual double GetScale( int *pbSuccess = NULL );*/
        /*virtual const char *GetUnitType();*/
        virtual GDALColorInterp GetColorInterpretation();
        virtual GDALColorTable *GetColorTable();
        virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0);

        /*
        virtual CPLErr SetCategoryNames( char ** );
        virtual CPLErr SetNoDataValue( double );
        virtual CPLErr SetColorTable( GDALColorTable * );
        virtual CPLErr SetColorInterpretation( GDALColorInterp );
        virtual CPLErr SetOffset( double );
        virtual CPLErr SetScale( double );
        virtual CPLErr SetUnitType( const char * );
        */

        virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                    double *pdfMin, double *pdfMax,
                                    double *pdfMean, double *padfStdDev );
        virtual CPLErr ComputeStatistics( int bApproxOK,
                                        double *pdfMin, double *pdfMax,
                                        double *pdfMean, double *pdfStdDev,
                                        GDALProgressFunc, void *pProgressData );
        /*virtual CPLErr SetStatistics( double dfMin, double dfMax,
                                    double dfMean, double dfStdDev );*/
        virtual CPLErr ComputeRasterMinMax( int, double* );

        virtual int HasArbitraryOverviews();
        virtual int GetOverviewCount();
        virtual GDALRasterBand *GetOverview(int);
        virtual GDALRasterBand *GetRasterSampleOverview( int );
        virtual CPLErr BuildOverviews( const char *, int, int *,
                                    GDALProgressFunc, void * );

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eDT, char **papszOptions );

        /*virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                            int nBuckets, int * panHistogram,
                            int bIncludeOutOfRange, int bApproxOK,
                            GDALProgressFunc, void *pProgressData );

        virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                            int *pnBuckets, int ** ppanHistogram,
                                            int bForce,
                                            GDALProgressFunc, void *pProgressData);
        virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                            int nBuckets, int *panHistogram );*/

        /*virtual const GDALRasterAttributeTable *GetDefaultRAT();
        virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );*/

        virtual GDALRasterBand *GetMaskBand();
        virtual int             GetMaskFlags();
        virtual CPLErr          CreateMaskBand( int nFlags );
};

/************************************************************************/
/* ==================================================================== */
/*                       NITFWrapperRasterBand                          */
/* ==================================================================== */
/************************************************************************/

/* This class is used to wrap bands from JPEG or JPEG2000 datasets in */
/* bands of the NITF dataset. Previously a trick was applied in the */
/* relevant drivers to define a SetColorInterpretation() method and */
/* to make sure they keep the proper pointer to their "natural" dataset */
/* This trick is no longer necessary with the NITFWrapperRasterBand */
/* We just override the few specific methods where we want that */
/* the NITFWrapperRasterBand behaviour differs from the JPEG/JPEG2000 one */

class NITFWrapperRasterBand : public NITFProxyPamRasterBand
{
  GDALRasterBand* poBaseBand;
  GDALColorTable* poColorTable;
  GDALColorInterp eInterp;

  protected:
    /* Pure virtual method of the NITFProxyPamRasterBand */
    virtual GDALRasterBand* RefUnderlyingRasterBand();

  public:
                   NITFWrapperRasterBand( NITFDataset * poDS,
                                          GDALRasterBand* poBaseBand,
                                          int nBand);
                  ~NITFWrapperRasterBand();
    
    /* Methods from GDALRasterBand we want to override */
    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr          SetColorInterpretation( GDALColorInterp );
    
    virtual GDALColorTable *GetColorTable();

    /* Specific method */
    void                    SetColorTableFromNITFBandInfo(); 
};

