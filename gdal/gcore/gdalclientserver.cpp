/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  GDAL Client/server dataset mechanism.
 * Author:   Even Rouault, <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault, <even dot rouault at mines-paris dot org>
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
#include "gdal_rat.h"
#include "cpl_spawn.h"
#include "cpl_multiproc.h"

/* REMINDER: upgrade this number when the on-wire protocol changes */
/* Note: please at least keep the version exchange protocol unchanged ! */
#define GDAL_CLIENT_SERVER_PROTOCOL_MAJOR 1
#define GDAL_CLIENT_SERVER_PROTOCOL_MINOR 0

#include <map>
#include <vector>

CPL_C_START
int CPL_DLL GDALServerLoop(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout);
const char* GDALClientDatasetGetFilename(const char* pszFilename);
CPL_C_END

#define BUFFER_SIZE 1024
typedef struct
{
    CPL_FILE_HANDLE fin;
    CPL_FILE_HANDLE fout;
    int             bOK;
    GByte           abyBuffer[BUFFER_SIZE];
    int             nBufferSize;
} GDALPipe;

typedef struct
{
    CPLSpawnedProcess *sp;
    GDALPipe          *p;
} GDALServerSpawnedProcess;

typedef struct
{
    int    bUpdated;
    double dfComplete;
    char  *pszProgressMsg;
    int    bRet;
    void  *hMutex;
} GDALServerAsyncProgress;

typedef enum
{
    INSTR_INVALID = 0,
    INSTR_GetGDALVersion = 1, /* do not change this ! */
    INSTR_EXIT,
    INSTR_EXIT_FAIL,
    INSTR_SetConfigOption,
    INSTR_Progress,
    INSTR_Reset,
    INSTR_Open,
    INSTR_Identify,
    INSTR_Create,
    INSTR_CreateCopy,
    INSTR_QuietDelete,
    INSTR_AddBand,
    INSTR_GetGeoTransform,
    INSTR_SetGeoTransform,
    INSTR_GetProjectionRef,
    INSTR_SetProjection,
    INSTR_GetGCPCount,
    INSTR_GetGCPProjection,
    INSTR_GetGCPs,
    INSTR_SetGCPs,
    INSTR_GetFileList,
    INSTR_FlushCache,
    INSTR_SetDescription,
    INSTR_GetMetadata,
    INSTR_GetMetadataItem,
    INSTR_SetMetadata,
    INSTR_SetMetadataItem,
    INSTR_IRasterIO_Read,
    INSTR_IRasterIO_Write,
    INSTR_IBuildOverviews,
    INSTR_AdviseRead,
    INSTR_CreateMaskBand,
    INSTR_Band_First,
    INSTR_Band_FlushCache,
    INSTR_Band_GetCategoryNames,
    INSTR_Band_SetCategoryNames,
    INSTR_Band_SetDescription,
    INSTR_Band_GetMetadata,
    INSTR_Band_GetMetadataItem,
    INSTR_Band_SetMetadata,
    INSTR_Band_SetMetadataItem,
    INSTR_Band_GetColorInterpretation,
    INSTR_Band_SetColorInterpretation,
    INSTR_Band_GetNoDataValue,
    INSTR_Band_GetMinimum,
    INSTR_Band_GetMaximum,
    INSTR_Band_GetOffset,
    INSTR_Band_GetScale,
    INSTR_Band_SetNoDataValue,
    INSTR_Band_SetOffset,
    INSTR_Band_SetScale,
    INSTR_Band_IReadBlock,
    INSTR_Band_IWriteBlock,
    INSTR_Band_IRasterIO_Read,
    INSTR_Band_IRasterIO_Write,
    INSTR_Band_GetStatistics,
    INSTR_Band_ComputeStatistics,
    INSTR_Band_SetStatistics,
    INSTR_Band_ComputeRasterMinMax,
    INSTR_Band_GetHistogram,
    INSTR_Band_GetDefaultHistogram,
    INSTR_Band_SetDefaultHistogram,
    INSTR_Band_HasArbitraryOverviews,
    INSTR_Band_GetOverviewCount,
    INSTR_Band_GetOverview,
    INSTR_Band_GetMaskBand,
    INSTR_Band_GetMaskFlags,
    INSTR_Band_CreateMaskBand,
    INSTR_Band_Fill,
    INSTR_Band_GetColorTable,
    INSTR_Band_SetColorTable,
    INSTR_Band_GetUnitType,
    INSTR_Band_SetUnitType,
    INSTR_Band_BuildOverviews,
    INSTR_Band_GetDefaultRAT,
    INSTR_Band_SetDefaultRAT,
    INSTR_Band_AdviseRead,
    INSTR_Band_End,
    INSTR_END
} InstrEnum;

#ifdef DEBUG
static const char* apszInstr[] =
{
    "INVALID",
    "GetGDALVersion",
    "EXIT",
    "FAIL",
    "SetConfigOption",
    "Progress",
    "Reset",
    "Open",
    "Identify",
    "Create",
    "CreateCopy",
    "QuietDelete",
    "AddBand",
    "GetGeoTransform",
    "SetGeoTransform",
    "GetProjectionRef",
    "SetProjection",
    "GetGCPCount",
    "GetGCPProjection",
    "GetGCPs",
    "SetGCPs",
    "GetFileList",
    "FlushCache",
    "SetDescription",
    "GetMetadata",
    "GetMetadataItem",
    "SetMetadata",
    "SetMetadataItem",
    "IRasterIO_Read",
    "IRasterIO_Write",
    "IBuildOverviews",
    "AdviseRead",
    "CreateMaskBand",
    "Band_First",
    "Band_FlushCache",
    "Band_GetCategoryNames",
    "Band_SetCategoryNames",
    "Band_SetDescription",
    "Band_GetMetadata",
    "Band_GetMetadataItem",
    "Band_SetMetadata",
    "Band_SetMetadataItem",
    "Band_GetColorInterpretation",
    "Band_SetColorInterpretation",
    "Band_GetNoDataValue",
    "Band_GetMinimum",
    "Band_GetMaximum",
    "Band_GetOffset",
    "Band_GetScale",
    "Band_SetNoDataValue",
    "Band_SetOffset",
    "Band_SetScale",
    "Band_IReadBlock",
    "Band_IWriteBlock",
    "Band_IRasterIO_Read",
    "Band_IRasterIO_Write",
    "Band_GetStatistics",
    "Band_ComputeStatistics",
    "Band_SetStatistics",
    "Band_ComputeRasterMinMax",
    "Band_GetHistogram",
    "Band_GetDefaultHistogram",
    "Band_SetDefaultHistogram",
    "Band_HasArbitraryOverviews",
    "Band_GetOverviewCount",
    "Band_GetOverview",
    "Band_GetMaskBand",
    "Band_GetMaskFlags",
    "Band_CreateMaskBand",
    "Band_Fill",
    "Band_GetColorTable",
    "Band_SetColorTable",
    "Band_GetUnitType",
    "Band_SetUnitType",
    "Band_BuildOverviews",
    "Band_GetDefaultRAT",
    "Band_SetDefaultRAT",
    "Band_AdviseRead",
    "Band_End",
    "END",
};
#endif

static const GByte abyEndOfJunkMarker[] = { 0xDE, 0xAD, 0xBE, 0xEF };

/* Recycling of connexions to child processes */
#define MAX_RECYCLED        128
#define DEFAULT_RECYCLED    4
static int bRecycleChild = FALSE;
static int nMaxRecycled = 0;
static GDALServerSpawnedProcess* aspRecycled[MAX_RECYCLED];

/************************************************************************/
/*                          EnterObject                                 */
/************************************************************************/

#ifdef DEBUG_VERBOSE
class EnterObject
{
    const char* pszFunction;

    public:
        EnterObject(const char* pszFunction) : pszFunction(pszFunction)
        {
            CPLDebug("GDAL", "Enter %s", pszFunction);
        }

        ~EnterObject()
        {
            CPLDebug("GDAL", "Leave %s", pszFunction);
        }
};

#define CLIENT_ENTER() EnterObject o(__FUNCTION__)
#else
#define CLIENT_ENTER() while(0)
#endif

/************************************************************************/
/*                       GDALClientDataset                              */
/************************************************************************/

class GDALClientDataset: public GDALPamDataset
{
    GDALServerSpawnedProcess                         *ssp;
    GDALPipe                                         *p;
    CPLString                                         osProjection;
    CPLString                                         osGCPProjection;
    int                                               bFreeDriver;
    int                                               nGCPCount;
    GDAL_GCP                                         *pasGCPs;
    std::map<CPLString, char**>                       aoMapMetadata;
    std::map< std::pair<CPLString,CPLString>, char*>  aoMapMetadataItem;
    GDALServerAsyncProgress                          *async;

        int                      mCreateCopy(const char* pszFilename,
                                             GDALDataset* poSrcDS,
                                             int bStrict, char** papszOptions,
                                             GDALProgressFunc pfnProgress,
                                             void * pProgressData);
        int                      mCreate( const char * pszName,
                                          int nXSize, int nYSize, int nBands,
                                          GDALDataType eType,
                                          char ** papszOptions );

                                  GDALClientDataset(GDALServerSpawnedProcess* ssp);

        static GDALClientDataset* CreateAndConnect();

    protected:
       virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );
       virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace);
    public:
                            GDALClientDataset(GDALPipe* p);
                            ~GDALClientDataset();

        int                 Init(const char* pszFilename, GDALAccess eAccess);

        void                AttachAsyncProgress(GDALServerAsyncProgress* async) { this->async = async; }
        int                 ProcessAsyncProgress();

        virtual void        FlushCache();

        virtual CPLErr        AddBand( GDALDataType eType, 
                                   char **papszOptions=NULL );

        //virtual void        SetDescription( const char * );

        virtual const char* GetMetadataItem( const char * pszName,
                                             const char * pszDomain = ""  );
        virtual char      **GetMetadata( const char * pszDomain = "" );
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                         const char * pszDomain = "" );
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                             const char * pszValue,
                                             const char * pszDomain = "" );

        virtual const char* GetProjectionRef();
        virtual CPLErr SetProjection( const char * );

        virtual CPLErr GetGeoTransform( double * );
        virtual CPLErr SetGeoTransform( double * );

        virtual int    GetGCPCount();
        virtual const char *GetGCPProjection();
        virtual const GDAL_GCP *GetGCPs();
        virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                                const char *pszGCPProjection );

        virtual char      **GetFileList(void);

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, 
                                int nBandCount, int *panBandList,
                                char **papszOptions );

        virtual CPLErr          CreateMaskBand( int nFlags );

        static GDALDataset *Open( GDALOpenInfo * );
        static int          Identify( GDALOpenInfo * );
        static GDALDataset *CreateCopy( const char * pszFilename, 
                                        GDALDataset * poSrcDS, int bStrict, char ** papszOptions, 
                                        GDALProgressFunc pfnProgress, void * pProgressData );
        static GDALDataset* Create( const char * pszName,
                                    int nXSize, int nYSize, int nBands,
                                    GDALDataType eType,
                                    char ** papszOptions );
};

/************************************************************************/
/*                       GDALClientRasterBand                           */
/************************************************************************/

class GDALClientRasterBand : public GDALPamRasterBand
{
    GDALPipe                                        *p;
    int                                              iSrvBand;
    std::map<int, GDALRasterBand*>                   aMapOvrBands;
    std::map<int, GDALRasterBand*>                   aMapOvrBandsCurrent;
    GDALRasterBand                                  *poMaskBand;
    std::map<CPLString, char**>                      aoMapMetadata;
    std::map< std::pair<CPLString,CPLString>, char*> aoMapMetadataItem;
    char                                           **papszCategoryNames;
    GDALColorTable                                  *poColorTable;
    char                                            *pszUnitType;
    GDALRasterAttributeTable                        *poRAT;
    std::vector<GDALRasterBand*>                     apoOldMaskBands;

    int                WriteInstr(InstrEnum instr);

    double             GetDouble( InstrEnum instr, int *pbSuccess );
    CPLErr             SetDouble( InstrEnum instr, double dfVal );

    protected:

        virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage);
        virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void* pImage);
        virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace );

    public:
        GDALClientRasterBand(GDALPipe* p, int iSrvBand,
                         GDALClientDataset* poDS, int nBand, GDALAccess eAccess,
                         int nRasterXSize, int nRasterYSize,
                         GDALDataType eDataType, int nBlockXSize, int nBlockYSize);
        ~GDALClientRasterBand();
        
        int GetSrvBand() const { return iSrvBand; }

        void ClearOverviewCache() { aMapOvrBandsCurrent.clear(); }

        virtual CPLErr FlushCache();

        virtual void        SetDescription( const char * );

        virtual const char* GetMetadataItem( const char * pszName,
                                             const char * pszDomain = ""  );
        virtual char      **GetMetadata( const char * pszDomain = "" );
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                         const char * pszDomain = "" );
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                             const char * pszValue,
                                             const char * pszDomain = "" );

        virtual GDALColorInterp GetColorInterpretation();
        virtual CPLErr SetColorInterpretation( GDALColorInterp );

        virtual char **GetCategoryNames();
        virtual double GetNoDataValue( int *pbSuccess = NULL );
        virtual double GetMinimum( int *pbSuccess = NULL );
        virtual double GetMaximum(int *pbSuccess = NULL );
        virtual double GetOffset( int *pbSuccess = NULL );
        virtual double GetScale( int *pbSuccess = NULL );

        virtual GDALColorTable *GetColorTable();
        virtual CPLErr SetColorTable( GDALColorTable * ); 

        virtual const char *GetUnitType();
        virtual CPLErr SetUnitType( const char * );

        virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0);

        virtual CPLErr SetCategoryNames( char ** );
        virtual CPLErr SetNoDataValue( double );
        virtual CPLErr SetOffset( double );
        virtual CPLErr SetScale( double );

        virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                    double *pdfMin, double *pdfMax, 
                                    double *pdfMean, double *padfStdDev );
        virtual CPLErr ComputeStatistics( int bApproxOK, 
                                        double *pdfMin, double *pdfMax, 
                                        double *pdfMean, double *pdfStdDev,
                                        GDALProgressFunc, void *pProgressData );
        virtual CPLErr SetStatistics( double dfMin, double dfMax, 
                                      double dfMean, double dfStdDev );
        virtual CPLErr ComputeRasterMinMax( int, double* );

        virtual CPLErr GetHistogram( double dfMin, double dfMax, 
                                     int nBuckets, int *panHistogram, 
                                     int bIncludeOutOfRange, int bApproxOK,
                                     GDALProgressFunc pfnProgress, 
                                     void *pProgressData );

        virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                            int *pnBuckets, int ** ppanHistogram,
                                            int bForce,
                                            GDALProgressFunc, void *pProgressData);
        virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                            int nBuckets, int *panHistogram );

        virtual int HasArbitraryOverviews();
        virtual int GetOverviewCount();
        virtual GDALRasterBand *GetOverview(int);

        virtual GDALRasterBand *GetMaskBand();
        virtual int             GetMaskFlags();
        virtual CPLErr          CreateMaskBand( int nFlags );

        virtual CPLErr BuildOverviews( const char *, int, int *,
                                       GDALProgressFunc, void * );

        virtual const GDALRasterAttributeTable *GetDefaultRAT();
        virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, char **papszOptions );
        /*
        virtual GDALRasterBand *GetRasterSampleOverview( int );
        */

};

/************************************************************************/
/*                          GDALPipeBuild()                             */
/************************************************************************/

static GDALPipe* GDALPipeBuild(CPLSpawnedProcess* sp)
{
    GDALPipe* p = (GDALPipe*)CPLMalloc(sizeof(GDALPipe));
    p->bOK = TRUE;
    p->fin = CPLSpawnAsyncGetInputFileHandle(sp);
    p->fout = CPLSpawnAsyncGetOutputFileHandle(sp);
    p->nBufferSize = 0;
    return p;
}

static GDALPipe* GDALPipeBuild(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout)
{
    GDALPipe* p = (GDALPipe*)CPLMalloc(sizeof(GDALPipe));
    p->bOK = TRUE;
    p->fin = fin;
    p->fout = fout;
    p->nBufferSize = 0;
    return p;
}

/************************************************************************/
/*                      GDALPipeWrite_internal()                        */
/************************************************************************/

static int GDALPipeWrite_internal(GDALPipe* p, const void* data, int length)
{
    if(!p->bOK)
        return FALSE;
    int nRet = CPLPipeWrite(p->fout, data, length);
    if( !nRet )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Write to pipe failed");
        p->bOK = FALSE;
    }
    return nRet;
}

/************************************************************************/
/*                        GDALPipeFlushBuffer()                         */
/************************************************************************/

static int GDALPipeFlushBuffer(GDALPipe * p)
{
    if( p->nBufferSize == 0 )
        return TRUE;
    if( GDALPipeWrite_internal(p, p->abyBuffer, p->nBufferSize) )
    {
        p->nBufferSize = 0;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                            GDALPipeFree()                            */
/************************************************************************/

static void GDALPipeFree(GDALPipe * p)
{
    GDALPipeFlushBuffer(p);
    CPLFree(p);
}

/************************************************************************/
/*                            GDALPipeRead()                            */
/************************************************************************/

static int GDALPipeRead(GDALPipe* p, void* data, int length)
{
    if(!p->bOK)
        return FALSE;
    if(!GDALPipeFlushBuffer(p))
        return FALSE;
    if( CPLPipeRead(p->fin, data, length) )
        return TRUE;
    // fprintf(stderr, "[%d] Read from pipe failed\n", (int)getpid());
    CPLError(CE_Failure, CPLE_AppDefined, "Read from pipe failed");
    p->bOK = FALSE;
    return FALSE;
}

/************************************************************************/
/*                           GDALPipeWrite()                            */
/************************************************************************/

static int GDALPipeWrite(GDALPipe* p, const void* data,
                                 int length)
{
    //return GDALPipeWrite_internal(p, data, length);
    GByte* pCur = (GByte*) data;
    int nRemain = length;
    while( nRemain > 0 )
    {
        if( p->nBufferSize + nRemain <= BUFFER_SIZE )
        {
            memcpy(p->abyBuffer + p->nBufferSize, pCur, nRemain);
            pCur += nRemain;
            p->nBufferSize += nRemain;
            nRemain = 0;
        }
        else if( nRemain > BUFFER_SIZE )
        {
            if( !GDALPipeFlushBuffer(p) )
                return FALSE;
            if( !GDALPipeWrite_internal(p, pCur, nRemain) )
                return FALSE;
            pCur += nRemain;
            nRemain = 0;
        }
        else
        {
            memcpy(p->abyBuffer + p->nBufferSize, pCur,
                   BUFFER_SIZE - p->nBufferSize);
            pCur += (BUFFER_SIZE - p->nBufferSize);
            nRemain -= (BUFFER_SIZE - p->nBufferSize);
            p->nBufferSize = BUFFER_SIZE;
            if( !GDALPipeFlushBuffer(p) )
                return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                          GDALPipeRead()                              */
/************************************************************************/

static int GDALPipeRead(GDALPipe* p, int* pnInt)
{
    return GDALPipeRead(p, pnInt, 4);
}

static int GDALPipeRead(GDALPipe* p, CPLErr* peErr)
{
    return GDALPipeRead(p, peErr, 4);
}

static int GDALPipeRead(GDALPipe* p, double* pdfDouble)
{
    return GDALPipeRead(p, pdfDouble, 8);
}

static int GDALPipeRead_nolength(GDALPipe* p, int nLength, void* pabyData)
{
    return GDALPipeRead(p, pabyData, nLength);
}

static int GDALPipeRead(GDALPipe* p, int nExpectedLength, void* pabyData)
{
    int nLength;
    return GDALPipeRead(p, &nLength) &&
           nLength == nExpectedLength &&
           GDALPipeRead_nolength(p, nLength, pabyData);
}

static int GDALPipeRead(GDALPipe* p, char** ppszStr)
{
    int nLength;
    if( !GDALPipeRead(p, &nLength) || nLength < 0 )
    {
        *ppszStr = NULL;
        return FALSE;
    }
    if( nLength == 0 )
    {
        *ppszStr = NULL;
        return TRUE;
    }
    *ppszStr = (char*) CPLMalloc(nLength + 1);
    if( nLength > 0 && !GDALPipeRead_nolength(p, nLength, *ppszStr) )
    {
        CPLFree(*ppszStr);
        *ppszStr = NULL;
        return FALSE;
    }
    (*ppszStr)[nLength] = 0;
    return TRUE;
}


static int GDALPipeRead(GDALPipe* p, char*** ppapszStr)
{
    int nStrCount;
    if( !GDALPipeRead(p, &nStrCount) )
        return FALSE;
    if( nStrCount < 0 )
    {
        *ppapszStr = NULL;
        return TRUE;
    }

    *ppapszStr = (char**) CPLMalloc(sizeof(char*) * (nStrCount + 1));
    for(int i=0;i<nStrCount;i++)
    {
        if( !GDALPipeRead(p, (*ppapszStr) + i) )
        {
            CSLDestroy(*ppapszStr);
            *ppapszStr = NULL;
            return FALSE;
        }
    }
    (*ppapszStr)[nStrCount] = NULL;
    return TRUE;
}

static int GDALPipeRead(GDALPipe* p, int nItems, int** ppanInt)
{
    int nSize;
    *ppanInt = NULL;
    if( !GDALPipeRead(p, &nSize) )
        return FALSE;
    if( nSize != nItems * (int)sizeof(int) )
        return FALSE;
    *ppanInt = (int*) CPLMalloc(nSize);
    if( !GDALPipeRead_nolength(p, nSize, *ppanInt) )
        return FALSE;
    return TRUE;
}

static int GDALPipeRead(GDALPipe* p, GDALColorTable** ppoColorTable)
{
    int nPaletteInterp, nCount;
    *ppoColorTable = NULL;
    if( !GDALPipeRead(p, &nPaletteInterp) )
        return FALSE;
    GDALColorTable* poColorTable;
    if( nPaletteInterp < 0 )
    {
        poColorTable = NULL;
    }
    else
    {
        if( !GDALPipeRead(p, &nCount) )
            return FALSE;
        poColorTable = new GDALColorTable((GDALPaletteInterp)nPaletteInterp);
        for(int i=0; i<nCount; i++)
        {
            int c1, c2, c3, c4;
            if( !GDALPipeRead(p, &c1) ||
                !GDALPipeRead(p, &c2) ||
                !GDALPipeRead(p, &c3) ||
                !GDALPipeRead(p, &c4) )
            {
                delete poColorTable;
                return FALSE;
            }
            GDALColorEntry eEntry;
            eEntry.c1 = (short)c1;
            eEntry.c2 = (short)c2;
            eEntry.c3 = (short)c3;
            eEntry.c4 = (short)c4;
            poColorTable->SetColorEntry(i, &eEntry);
        }
    }
    *ppoColorTable = poColorTable;
    return TRUE;
}

static int GDALPipeRead(GDALPipe* p, GDALRasterAttributeTable** ppoRAT)
{
    *ppoRAT = NULL;
    char* pszRAT = NULL;
    if( !GDALPipeRead(p, &pszRAT))
        return FALSE;
    if( pszRAT == NULL )
        return TRUE;

    CPLXMLNode* poNode = CPLParseXMLString( pszRAT );
    CPLFree(pszRAT);
    if( poNode == NULL )
        return FALSE;

    *ppoRAT = new GDALRasterAttributeTable();
    if( (*ppoRAT)->XMLInit(poNode, NULL) != CE_None )
    {
        CPLDestroyXMLNode(poNode);
        delete *ppoRAT;
        *ppoRAT = NULL;
        return FALSE;
    }
    CPLDestroyXMLNode(poNode);
    return TRUE;
}

static int GDALPipeRead(GDALPipe* p, int* pnGCPCount, GDAL_GCP** ppasGCPs)
{
    *pnGCPCount = 0;
    *ppasGCPs = NULL;
    int nGCPCount;
    if( !GDALPipeRead(p, &nGCPCount) )
        return FALSE;
    GDAL_GCP* pasGCPs = (GDAL_GCP* )CPLCalloc(nGCPCount, sizeof(GDAL_GCP));
    for(int i=0;i<nGCPCount;i++)
    {
        if( !GDALPipeRead(p, &pasGCPs[i].pszId) ||
            !GDALPipeRead(p, &pasGCPs[i].pszInfo) ||
            !GDALPipeRead(p, &pasGCPs[i].dfGCPPixel) ||
            !GDALPipeRead(p, &pasGCPs[i].dfGCPLine) ||
            !GDALPipeRead(p, &pasGCPs[i].dfGCPX) ||
            !GDALPipeRead(p, &pasGCPs[i].dfGCPY) ||
            !GDALPipeRead(p, &pasGCPs[i].dfGCPZ) )
        {
            GDALDeinitGCPs(i, pasGCPs);
            CPLFree(pasGCPs);
            return FALSE;
        }
    }
    *pnGCPCount = nGCPCount;
    *ppasGCPs = pasGCPs;
    return TRUE;
}

static int GDALPipeRead(GDALPipe* p, GDALClientDataset* poDS,
                        GDALRasterBand** ppoBand)
{
    int iSrvBand;
    *ppoBand = NULL;
    if( !GDALPipeRead(p, &iSrvBand) )
        return FALSE;
    if( iSrvBand < 0 )
        return TRUE;

    int iBand, nBandAccess, nXSize, nYSize, nDataType, nBlockXSize, nBlockYSize;
    if( !GDALPipeRead(p, &iBand) ||
        !GDALPipeRead(p, &nBandAccess) ||
        !GDALPipeRead(p, &nXSize) ||
        !GDALPipeRead(p, &nYSize) ||
        !GDALPipeRead(p, &nDataType) ||
        !GDALPipeRead(p, &nBlockXSize) ||
        !GDALPipeRead(p, &nBlockYSize) )
    {
        return FALSE;
    }

    char* pszDescription = NULL;
    if( !GDALPipeRead(p, &pszDescription) )
        return FALSE;

    GDALClientRasterBand* poBand = new GDALClientRasterBand(p, iSrvBand,
                                                  poDS, iBand, (GDALAccess)nBandAccess,
                                                  nXSize, nYSize,
                                                  (GDALDataType)nDataType,
                                                  nBlockXSize, nBlockYSize);
    if( pszDescription != NULL )
        poBand->GDALMajorObject::SetDescription(pszDescription);
    CPLFree(pszDescription);

    *ppoBand = poBand;
    return TRUE;
}

/************************************************************************/
/*                GDALSkipUntilEndOfJunkMarker()                        */
/************************************************************************/

static int GDALSkipUntilEndOfJunkMarker(GDALPipe* p)
{
    if(!p->bOK)
        return FALSE;
    GByte c;
    size_t nIter = 0;
    int nStep = 0;
    CPLString osJunk;
    int nMarkerSize = (int)sizeof(abyEndOfJunkMarker);
    GByte abyBuffer[sizeof(abyEndOfJunkMarker)];
    if( !GDALPipeRead_nolength(p, sizeof(abyBuffer), abyBuffer ) )
        return FALSE;
    if( memcmp(abyEndOfJunkMarker, abyBuffer, sizeof(abyBuffer)) == 0 )
        return TRUE;
    while(TRUE)
    {
        if( nIter < sizeof(abyBuffer) )
            c = abyBuffer[nIter ++];
        else if( !GDALPipeRead_nolength(p, 1, &c ) )
            return FALSE;

        if( c != 0 )
            osJunk += c;
        if( c == abyEndOfJunkMarker[0] ) nStep = 1;
        else if( c == abyEndOfJunkMarker[nStep] )
        {
            nStep ++;
            if( nStep == nMarkerSize )
            {
                osJunk.resize(osJunk.size() - nMarkerSize);
                if( osJunk.size() )
                    CPLDebug("GDAL", "Got junk : %s", osJunk.c_str());
                return TRUE;
            }
        }
        else
            nStep = 0;
    }
}

/************************************************************************/
/*                         GDALPipeWrite()                              */
/************************************************************************/

static int GDALPipeWrite(GDALPipe* p, int nInt)
{
    return GDALPipeWrite(p, &nInt, 4);
}

static int GDALPipeWrite(GDALPipe* p, double dfDouble)
{
    return GDALPipeWrite(p, &dfDouble, 8);
}

static int GDALPipeWrite_nolength(GDALPipe* p, int nLength, const void* pabyData)
{
    return GDALPipeWrite(p, pabyData, nLength);
}

static int GDALPipeWrite(GDALPipe* p, int nLength, const void* pabyData)
{
    if( !GDALPipeWrite(p, nLength) ||
        !GDALPipeWrite_nolength(p, nLength, pabyData) )
        return FALSE;
    return TRUE;
}

static int GDALPipeWrite(GDALPipe* p, const char* pszStr)
{
    if( pszStr == NULL )
        return GDALPipeWrite(p, 0);
    return GDALPipeWrite(p, (int)strlen(pszStr) + 1, pszStr);
}

static int GDALPipeWrite(GDALPipe* p, char** papszStr)
{
    if( papszStr == NULL )
        return GDALPipeWrite(p, -1);

    int nCount = CSLCount(papszStr);
    if( !GDALPipeWrite(p, nCount) )
        return FALSE;
    for(int i=0; i < nCount; i++)
    {
        if( !GDALPipeWrite(p, papszStr[i]) )
            return FALSE;
    }
    return TRUE;
}

static int GDALPipeWrite(GDALPipe* p,
                         std::vector<GDALRasterBand*>& aBands,
                         GDALRasterBand* poBand)
{
    if( poBand == NULL )
        GDALPipeWrite(p, -1);
    else
    {
        GDALPipeWrite(p, (int)aBands.size());
        aBands.push_back(poBand);
        GDALPipeWrite(p, poBand->GetBand());
        GDALPipeWrite(p, poBand->GetAccess());
        GDALPipeWrite(p, poBand->GetXSize());
        GDALPipeWrite(p, poBand->GetYSize());
        GDALPipeWrite(p, poBand->GetRasterDataType());
        int nBlockXSize, nBlockYSize;
        poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        GDALPipeWrite(p, nBlockXSize);
        GDALPipeWrite(p, nBlockYSize);
        GDALPipeWrite(p, poBand->GetDescription() );
    }
    return TRUE;
}

static int GDALPipeWrite(GDALPipe* p, GDALColorTable* poColorTable)
{
    if( poColorTable == NULL )
    {
        if( !GDALPipeWrite(p, -1) )
            return FALSE;
    }
    else
    {
        int nCount = poColorTable->GetColorEntryCount();
        if( !GDALPipeWrite(p, poColorTable->GetPaletteInterpretation()) ||
            !GDALPipeWrite(p, nCount) )
            return FALSE;

        for(int i=0; i < nCount; i++)
        {
            const GDALColorEntry* poColorEntry = poColorTable->GetColorEntry(i);
            if( !GDALPipeWrite(p, poColorEntry->c1) ||
                !GDALPipeWrite(p, poColorEntry->c2) ||
                !GDALPipeWrite(p, poColorEntry->c3) ||
                !GDALPipeWrite(p, poColorEntry->c4) )
                return FALSE;
        }
    }
    return TRUE;
}

static int GDALPipeWrite(GDALPipe* p, const GDALRasterAttributeTable* poRAT)
{
    int bRet;
    if( poRAT == NULL )
        bRet = GDALPipeWrite(p, (const char*)NULL);
    else
    {
        CPLXMLNode* poNode = poRAT->Serialize();
        if( poNode != NULL )
        {
            char* pszRAT = CPLSerializeXMLTree(poNode);
            bRet = GDALPipeWrite(p, pszRAT);
            CPLFree(pszRAT);
            CPLDestroyXMLNode(poNode);
        }
        else
            bRet = GDALPipeWrite(p, (const char*)NULL);
    }
    return bRet;
}

static int GDALPipeWrite(GDALPipe* p, int nGCPCount, const GDAL_GCP* pasGCPs)
{
    if( !GDALPipeWrite(p, nGCPCount ) )
        return FALSE;
    for(int i=0;i<nGCPCount;i++)
    {
        if( !GDALPipeWrite(p, pasGCPs[i].pszId) ||
            !GDALPipeWrite(p, pasGCPs[i].pszInfo) ||
            !GDALPipeWrite(p, pasGCPs[i].dfGCPPixel) ||
            !GDALPipeWrite(p, pasGCPs[i].dfGCPLine) ||
            !GDALPipeWrite(p, pasGCPs[i].dfGCPX) ||
            !GDALPipeWrite(p, pasGCPs[i].dfGCPY) ||
            !GDALPipeWrite(p, pasGCPs[i].dfGCPZ) )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                    GDALPipeWriteConfigOption()                       */
/************************************************************************/

static int GDALPipeWriteConfigOption(GDALPipe* p, const char* pszKey,
                                     int bWriteIfNonNull = TRUE)
{
    const char* pszVal = CPLGetConfigOption(pszKey, NULL);
    if( pszVal == NULL && !bWriteIfNonNull )
        return TRUE;
    return GDALPipeWrite(p, INSTR_SetConfigOption) &&
           GDALPipeWrite(p, pszKey) &&
           GDALPipeWrite(p, pszVal);
}

/************************************************************************/
/*                    GDALEmitEndOfJunkMarker()                         */
/************************************************************************/

/* When receiving an instruction : */
/* - read all input arguments */
/* - do the call to the dataset or the band */
/* - as the previous call may potentially emit */
/*   unwanted content on the stdout, we emit */
/*   a special marker that the receiver will */
/*   wait until interpreting the rest of the */
/*   output arguments */
/* - emit output arguments */
static int GDALEmitEndOfJunkMarker(GDALPipe* p)
{
    return GDALPipeWrite_nolength(p, sizeof(abyEndOfJunkMarker),
                                  abyEndOfJunkMarker) ==
                                        (int)sizeof(abyEndOfJunkMarker);
}

/************************************************************************/
/*                       GDALConsumeErrors()                            */
/************************************************************************/

static void GDALConsumeErrors(GDALPipe* p)
{
    int nErrors;
    if( !GDALPipeRead(p, &nErrors) )
        return;
    for(int i=0;i<nErrors;i++)
    {
        int       eErr;
        int       nErrNo;
        char     *pszErrorMsg = NULL;
        if( !GDALPipeRead(p, &eErr) ||
            !GDALPipeRead(p, &nErrNo) ||
            !GDALPipeRead(p, &pszErrorMsg) )
            return;
        CPLError((CPLErr)eErr, nErrNo, "%s", pszErrorMsg);
        CPLFree(pszErrorMsg);
    }
}

/************************************************************************/
/*                       GDALEmitReset()                                */
/************************************************************************/

static int GDALEmitReset(GDALPipe* p)
{
    int bOK;
    if( !GDALPipeWrite(p, INSTR_Reset) ||
        !GDALSkipUntilEndOfJunkMarker(p) ||
        !GDALPipeRead(p, &bOK) )
        return FALSE;
    GDALConsumeErrors(p);
    return bOK;
}

/************************************************************************/
/*                       GDALEmitEXIT()                                 */
/************************************************************************/

static int GDALEmitEXIT(GDALPipe* p, InstrEnum instr = INSTR_EXIT )
{
    int bOK;
    if( !GDALPipeWrite(p, instr) ||
        !GDALSkipUntilEndOfJunkMarker(p) ||
        !GDALPipeRead(p, &bOK) )
        return FALSE;
    return bOK;
}

/************************************************************************/
/*                    GDALServerSpawnAsyncFinish()                      */
/************************************************************************/

static int GDALServerSpawnAsyncFinish(GDALServerSpawnedProcess* ssp)
{
    if( bRecycleChild && ssp->p->bOK )
    {
        /* Store the descriptor in a free slot if available for a */
        /* later reuse */
        CPLMutexHolderD(GDALGetpDMMutex());
        for(int i = 0; i < nMaxRecycled; i ++)
        {
            if( aspRecycled[i] == NULL )
            {
                if( !GDALEmitReset(ssp->p) )
                    return FALSE;

                aspRecycled[i] = ssp;
                return TRUE;
            }
        }
    }

    if(ssp->p->bOK)
    {
        if( !GDALEmitEXIT(ssp->p) )
            return FALSE;
    }

    CPLDebug("GDAL", "Destroy spawned process %p", ssp);
    GDALPipeFree(ssp->p);
    int nRet = CPLSpawnAsyncFinish(ssp->sp, TRUE);
    CPLFree(ssp);
    return nRet;
}

/************************************************************************/
/*                      GDALCheckServerVersion()                        */
/************************************************************************/

static int GDALCheckServerVersion(GDALPipe* p)
{
    GDALPipeWrite(p, INSTR_GetGDALVersion);
    char bIsLSB = CPL_IS_LSB;
    GDALPipeWrite_nolength(p, 1, &bIsLSB);
    GDALPipeWrite(p, GDAL_RELEASE_NAME);
    GDALPipeWrite(p, GDAL_VERSION_MAJOR);
    GDALPipeWrite(p, GDAL_VERSION_MINOR);
    GDALPipeWrite(p, GDAL_CLIENT_SERVER_PROTOCOL_MAJOR);
    GDALPipeWrite(p, GDAL_CLIENT_SERVER_PROTOCOL_MINOR);
    GDALPipeWrite(p, 0); /* extra bytes */

    char* pszVersion = NULL;
    int nMajor, nMinor, nProtocolMajor, nProtocolMinor, nExtraBytes;
    if( !GDALPipeRead(p, &pszVersion) ||
        !GDALPipeRead(p, &nMajor) ||
        !GDALPipeRead(p, &nMinor) ||
        !GDALPipeRead(p, &nProtocolMajor) ||
        !GDALPipeRead(p, &nProtocolMinor) ||
        !GDALPipeRead(p, &nExtraBytes) )
    {
        CPLFree(pszVersion);
        return FALSE;
    }

    if( nExtraBytes > 0 )
    {
        void* pTemp = VSIMalloc(nExtraBytes);
        if( !pTemp )
        {
            CPLFree(pszVersion);
            return FALSE;
        }
        if( !GDALPipeRead_nolength(p, nExtraBytes, pTemp) )
        {
            CPLFree(pszVersion);
            CPLFree(pTemp);
            return FALSE;
        }
        CPLFree(pTemp);
    }

    CPLDebug("GDAL",
             "Server version : %s (%d.%d), "
             "Server protocol version = %d.%d",
             pszVersion,
             nMajor, nMinor,
             nProtocolMajor, nProtocolMinor);
    CPLDebug("GDAL",
             "Client version : %s (%d.%d), "
             "Client protocol version = %d.%d",
             GDAL_RELEASE_NAME, GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR,
             GDAL_CLIENT_SERVER_PROTOCOL_MAJOR,
             GDAL_CLIENT_SERVER_PROTOCOL_MINOR);
    if( nProtocolMajor != GDAL_CLIENT_SERVER_PROTOCOL_MAJOR )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDAL server (GDAL version=%s, protocol version=%d.%d) is "
                 "incompatible with GDAL client (GDAL version=%s, protocol version=%d.%d)",
                 pszVersion,
                 nProtocolMajor, nProtocolMinor,
                 GDAL_RELEASE_NAME,
                 GDAL_CLIENT_SERVER_PROTOCOL_MAJOR,
                 GDAL_CLIENT_SERVER_PROTOCOL_MINOR);
        CPLFree(pszVersion);
        return FALSE;
    }
    else if( nProtocolMinor != GDAL_CLIENT_SERVER_PROTOCOL_MINOR )
    {
        CPLDebug("GDAL", "Note: client/server protocol versions differ by minor number.");
    }
    CPLFree(pszVersion);
    return TRUE;
}

/************************************************************************/
/*                       GDALServerLoopForked()                         */
/************************************************************************/

#ifndef WIN32
static int GDALServerLoopForked(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout)
{
    /* Do not try to close datasets at process closing */
    GDALNullifyOpenDatasetsList();

    memset(aspRecycled, 0, sizeof(aspRecycled));

    return GDALServerLoop(fin, fout);
}
#endif

/************************************************************************/
/*                      GDALServerSpawnAsync()                          */
/************************************************************************/

static GDALServerSpawnedProcess* GDALServerSpawnAsync()
{
    if( bRecycleChild )
    {
        /* Try to find an existing unused descriptor to reuse it */
        CPLMutexHolderD(GDALGetpDMMutex());
        for(int i = 0; i < nMaxRecycled; i ++)
        {
            if( aspRecycled[i] != NULL )
            {
                GDALServerSpawnedProcess* ssp = aspRecycled[i];
                aspRecycled[i] = NULL;
                return ssp;
            }
        }
    }

#ifdef WIN32
    const char* pszSpawnServer = CPLGetConfigOption("GDAL_RPC_SERVER", "gdalserver");
#else
    const char* pszSpawnServer = CPLGetConfigOption("GDAL_RPC_SERVER", "NO");
#endif
    if( EQUAL(pszSpawnServer, "YES") || EQUAL(pszSpawnServer, "ON") ||
        EQUAL(pszSpawnServer, "TRUE")  || EQUAL(pszSpawnServer, "1") )
        pszSpawnServer = "gdalserver";
    const char* apszGDALServer[] = { pszSpawnServer, "-run", NULL };
    int bCheckVersions = TRUE;

    CPLSpawnedProcess* sp;
#ifndef WIN32
    if( EQUAL(pszSpawnServer, "NO") || EQUAL(pszSpawnServer, "OFF") ||
        EQUAL(pszSpawnServer, "FALSE")  || EQUAL(pszSpawnServer, "0") )
    {
        sp = CPLSpawnAsync(GDALServerLoopForked, NULL, TRUE, TRUE, FALSE);
        bCheckVersions = FALSE;
    }
    else
#endif
        sp = CPLSpawnAsync(NULL, apszGDALServer, TRUE, TRUE, FALSE);

    if( sp == NULL )
        return NULL;

    GDALServerSpawnedProcess* ssp =
        (GDALServerSpawnedProcess*)CPLMalloc(sizeof(GDALServerSpawnedProcess));
    ssp->sp = sp;
    ssp->p = GDALPipeBuild(sp);

    CPLDebug("GDAL", "Create spawned process %p", ssp);
    if( bCheckVersions && !GDALCheckServerVersion(ssp->p) )
    {
        GDALServerSpawnAsyncFinish(ssp);
        return NULL;
    }
    return ssp;
}

/************************************************************************/
/*                        CPLErrOnlyRet()                               */
/************************************************************************/

static CPLErr CPLErrOnlyRet(GDALPipe* p)
{
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                         RunErrorHandler()                            */
/************************************************************************/

class GDALServerErrorDesc
{
    public:
        GDALServerErrorDesc() {}

        CPLErr    eErr;
        int       nErrNo;
        CPLString osErrorMsg;
};

static void CPL_STDCALL RunErrorHandler(CPLErr eErr, int nErrNo,
                                        const char* pszErrorMsg)
{
    GDALServerErrorDesc oDesc;
    oDesc.eErr = eErr;
    oDesc.nErrNo = nErrNo;
    oDesc.osErrorMsg = pszErrorMsg;
    std::vector<GDALServerErrorDesc>* paoErrors =
        (std::vector<GDALServerErrorDesc>*) CPLGetErrorHandlerUserData();
    if( paoErrors )
        paoErrors->push_back(oDesc);
}

/************************************************************************/
/*                        RunAsyncProgress()                            */
/************************************************************************/

static int CPL_STDCALL RunAsyncProgress(double dfComplete,
                                        const char *pszMessage,
                                        void *pProgressArg)
{
    /* We don't send the progress right now, since some drivers like ECW */
    /* call the progress callback from an helper thread, while calling methods */
    /* on the source dataset. So we could end up sending mixed content on the pipe */
    /* to the client. The best is to transmit the progress in a regularly called method */
    /* of the dataset, such as IReadBlock() / IRasterIO() */
    GDALServerAsyncProgress* asyncp = (GDALServerAsyncProgress*)pProgressArg;
    CPLMutexHolderD(&(asyncp->hMutex));
    asyncp->bUpdated = TRUE;
    asyncp->dfComplete = dfComplete;
    CPLFree(asyncp->pszProgressMsg);
    asyncp->pszProgressMsg = (pszMessage) ? CPLStrdup(pszMessage) : NULL;
    return asyncp->bRet;
}

/************************************************************************/
/*                        RunSyncProgress()                             */
/************************************************************************/

static int CPL_STDCALL RunSyncProgress(double dfComplete,
                                       const char *pszMessage,
                                       void *pProgressArg)
{
    GDALPipe* p = (GDALPipe*)pProgressArg;
    if( !GDALPipeWrite(p, INSTR_Progress) ||
        !GDALPipeWrite(p, dfComplete) ||
        !GDALPipeWrite(p, pszMessage) )
        return FALSE;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return FALSE;
    int bRet = FALSE;
    if( !GDALPipeRead(p, &bRet) )
        return FALSE;
    GDALConsumeErrors(p);
    return bRet;
}

/************************************************************************/
/*                         GDALServerLoop()                             */
/************************************************************************/

static int GDALServerLoop(GDALPipe* p,
                          GDALDataset* poSrcDS,
                          GDALProgressFunc pfnProgress, void* pProgressData)
{
    GDALDataset* poDS = NULL;
    std::vector<GDALRasterBand*> aBands;
    std::vector<GDALServerErrorDesc> aoErrors;
    int nRet = 1;
    GDALServerAsyncProgress asyncp;
    memset(&asyncp, 0, sizeof(asyncp));
    asyncp.bRet = TRUE;
    void* pBuffer = NULL;
    int nBufferSize = 0;

    const char* pszOldVal = CPLGetConfigOption("GDAL_RPC", NULL);
    char* pszOldValDup = (pszOldVal) ? CPLStrdup(pszOldVal) : NULL;
    CPLSetThreadLocalConfigOption("GDAL_RPC", "OFF");

    if( poSrcDS == NULL )
        CPLPushErrorHandlerEx(RunErrorHandler, &aoErrors);

    // fprintf(stderr, "[%d] started\n", (int)getpid());
    while(TRUE)
    {
        int instr;
        if( !GDALPipeRead(p, &instr) )
        {
            // fprintf(stderr, "[%d] instr failed\n", (int)getpid());
            break;
        }

        // fprintf(stderr, "[%d] %s\n", (int)getpid(), (instr >= 0 && instr < INSTR_END) ? apszInstr[instr] : "unknown");

        GDALRasterBand* poBand = NULL;

        if( instr == INSTR_EXIT )
        {
            if( poSrcDS == NULL && poDS != NULL )
            {
                GDALClose((GDALDatasetH)poDS);
                poDS = NULL;
                aBands.resize(0);
            }
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, TRUE);
            nRet = 0;
            break;
        }
        else if( instr == INSTR_EXIT_FAIL )
        {
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, TRUE);
            break;
        }
        else if( instr == INSTR_GetGDALVersion ||
                 instr == 0x01000000 )
        {
            /* Do not change this protocol ! */
            char bClientIsLSB;
            char* pszClientVersion = NULL;
            int nClientMajor, nClientMinor,
                nClientProtocolMajor, nClientProtocolMinor,
                nExtraBytes;
            if( !GDALPipeRead_nolength(p, 1, &bClientIsLSB) )
                break;
            if( bClientIsLSB != CPL_IS_LSB )
            {
                fprintf(stderr, "Server does not understand client endianness.\n");
                break;
            }

            if (!GDALPipeRead(p, &pszClientVersion) ||
                !GDALPipeRead(p, &nClientMajor) ||
                !GDALPipeRead(p, &nClientMinor) ||
                !GDALPipeRead(p, &nClientProtocolMajor) ||
                !GDALPipeRead(p, &nClientProtocolMinor) ||
                !GDALPipeRead(p, &nExtraBytes) )
            {
                CPLFree(pszClientVersion);
                break;
            }

            if( nExtraBytes > 0 )
            {
                void* pTemp = VSIMalloc(nExtraBytes);
                if( !pTemp )
                {
                    CPLFree(pszClientVersion);
                    break;
                }
                if( !GDALPipeRead_nolength(p, nExtraBytes, pTemp) )
                {
                    CPLFree(pszClientVersion);
                    CPLFree(pTemp);
                    break;
                }
                CPLFree(pTemp);
            }

            GDALPipeWrite(p, GDAL_RELEASE_NAME);
            GDALPipeWrite(p, GDAL_VERSION_MAJOR);
            GDALPipeWrite(p, GDAL_VERSION_MINOR);
            GDALPipeWrite(p, GDAL_CLIENT_SERVER_PROTOCOL_MAJOR);
            GDALPipeWrite(p, GDAL_CLIENT_SERVER_PROTOCOL_MINOR);
            GDALPipeWrite(p, 0); /* extra bytes */
            continue;
        }
        else if( instr == INSTR_SetConfigOption )
        {
            char *pszKey = NULL, *pszValue = NULL;
            if( !GDALPipeRead(p, &pszKey) ||
                !GDALPipeRead(p, &pszValue) )
                break;
            CPLSetConfigOption(pszKey, pszValue);
            CPLFree(pszKey);
            CPLFree(pszValue);
            continue;
        }
        else if( instr == INSTR_Progress )
        {
            double dfProgress;
            char* pszProgressMsg = NULL;
            if( !GDALPipeRead(p, &dfProgress) ||
                !GDALPipeRead(p, &pszProgressMsg) )
                break;
            int nRet = pfnProgress(dfProgress, pszProgressMsg, pProgressData);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, nRet);
            CPLFree(pszProgressMsg);
        }
        else if( instr == INSTR_Reset )
        {
            if( poSrcDS == NULL && poDS != NULL )
            {
                GDALClose((GDALDatasetH)poDS);
                poDS = NULL;
                aBands.resize(0);
            }
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, TRUE);
        }
        else if( instr == INSTR_Open )
        {
            int nAccess;
            char* pszFilename = NULL;
            if( !GDALPipeRead(p, &nAccess) ||
                !GDALPipeRead(p, &pszFilename) )
                break;
            if( poSrcDS != NULL )
                poDS = poSrcDS;
            else if( poDS == NULL && pszFilename != NULL )
                poDS = (GDALDataset*) GDALOpen(pszFilename, (GDALAccess)nAccess);
            CPLFree(pszFilename);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, poDS != NULL);
            if( poDS != NULL )
            {
                GDALPipeWrite(p, poDS->GetDescription() );
                GDALDriver* poDriver = poDS->GetDriver();
                if( poDriver != NULL )
                {
                    GDALPipeWrite(p, poDriver->GetDescription() );
                    char** papszItems = poDriver->GetMetadata();
                    for(int i = 0; papszItems[i] != NULL; i++ )
                    {
                        char* pszKey = NULL;
                        const char* pszVal = CPLParseNameValue(papszItems[i], &pszKey );
                        if( pszKey != NULL )
                        {
                            GDALPipeWrite(p, pszKey );
                            GDALPipeWrite(p, pszVal );
                            CPLFree(pszKey);
                        }
                    }
                    GDALPipeWrite(p, (const char*)NULL);
                }
                else
                    GDALPipeWrite(p, (const char*)NULL);

                GDALPipeWrite(p, poDS->GetRasterXSize());
                GDALPipeWrite(p, poDS->GetRasterYSize());
                int nBands = poDS->GetRasterCount();
                GDALPipeWrite(p, nBands);
                int i;
                int bAllSame = TRUE;
                GDALRasterBand* poFirstBand = NULL;
                int nFBBlockXSize, nFBBlockYSize;

                /* Check if all bands are identical */
                for(i=0;i<nBands;i++)
                {
                    GDALRasterBand* poBand = poDS->GetRasterBand(i+1);
                    if( strlen(poBand->GetDescription()) > 0 )
                    {
                        bAllSame = FALSE;
                        break;
                    }
                    if( i == 0 )
                    {
                        poFirstBand = poBand;
                        poBand->GetBlockSize(&nFBBlockXSize, &nFBBlockYSize);
                    }
                    else
                    {
                        int nBlockXSize, nBlockYSize;
                        poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
                        if( poBand->GetXSize() != poFirstBand->GetXSize() ||
                            poBand->GetYSize() != poFirstBand->GetYSize() ||
                            poBand->GetRasterDataType() != poFirstBand->GetRasterDataType() ||
                            nBlockXSize != nFBBlockXSize ||
                            nBlockYSize != nFBBlockYSize )
                        {
                            bAllSame = FALSE;
                            break;
                        }
                    }
                }

                /* Transmit bands */
                GDALPipeWrite(p, bAllSame);
                for(i=0;i<nBands;i++)
                {
                    GDALRasterBand* poBand = poDS->GetRasterBand(i+1);
                    if( i > 0 && bAllSame )
                        aBands.push_back(poBand);
                    else
                        GDALPipeWrite(p, aBands, poBand);
                }
            }
        }
        else if( instr == INSTR_Identify )
        {
            char* pszFilename = NULL;
            if( !GDALPipeRead(p, &pszFilename) )
                break;
            int bRet = GDALIdentifyDriver(pszFilename, NULL) != NULL;
            CPLFree(pszFilename);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bRet);
            aoErrors.resize(0);
        }
        else if( instr == INSTR_Create )
        {
            char* pszFilename = NULL;
            int nXSize, nYSize, nBands, nDataType;
            char** papszOptions = NULL;
            GDALDriver* poDriver = NULL;
            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBands) ||
                !GDALPipeRead(p, &nDataType) ||
                !GDALPipeRead(p, &papszOptions) )
            {
                CPLFree(pszFilename);
                break;
            }
            const char* pszDriver = CSLFetchNameValue(papszOptions, "SERVER_DRIVER");
            CPLString osDriver;
            if( pszDriver != NULL )
            {
                osDriver = pszDriver;
                pszDriver = osDriver.c_str();
                poDriver = (GDALDriver* )GDALGetDriverByName(pszDriver);
            }
            papszOptions = CSLSetNameValue(papszOptions, "SERVER_DRIVER", NULL);
            if( poDriver != NULL )
            {
                poDS = poDriver->Create(pszFilename, nXSize, nYSize, nBands,
                                        (GDALDataType)nDataType,
                                        papszOptions);
            }
            else
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                         (pszDriver) ? pszDriver : "(unknown)");

            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, poDS != NULL);
            CPLFree(pszFilename);
            CSLDestroy(papszOptions);
        }
        else if( instr == INSTR_CreateCopy )
        {
            char* pszFilename = NULL;
            char** papszCreateOptions = NULL;
            GDALDriver* poDriver = NULL;
            int bStrict = FALSE;

            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL ||
                !GDALPipeRead(p, &bStrict) ||
                !GDALPipeRead(p, &papszCreateOptions) )
            {
                CPLFree(pszFilename);
                break;
            }
            const char* pszDriver = CSLFetchNameValue(papszCreateOptions, "SERVER_DRIVER");
            CPLString osDriver;
            if( pszDriver != NULL )
            {
                osDriver = pszDriver;
                pszDriver = osDriver.c_str();
                poDriver = (GDALDriver* )GDALGetDriverByName(pszDriver);
            }
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "SERVER_DRIVER", NULL);
            GDALPipeWrite(p, poDriver != NULL);
            if( poDriver != NULL )
            {
                GDALClientDataset* poSrcDS = new GDALClientDataset(p);
                if( !poSrcDS->Init(NULL, GA_ReadOnly) )
                {
                    delete poSrcDS;
                    CPLFree(pszFilename);
                    CSLDestroy(papszCreateOptions);
                    break;
                }
                poSrcDS->AttachAsyncProgress(&asyncp);

                poDS = poDriver->CreateCopy(pszFilename, poSrcDS,
                                            bStrict, papszCreateOptions,
                                            RunAsyncProgress, &asyncp);

                int bProgressRet = poSrcDS->ProcessAsyncProgress();
                GDALClose((GDALDatasetH)poSrcDS);

                if( !bProgressRet && poDS != NULL )
                {
                    GDALClose((GDALDatasetH)poDS);
                    poDS = NULL;
                }

                if( !GDALEmitEXIT(p, (poDS != NULL) ? INSTR_EXIT : INSTR_EXIT_FAIL) )
                    break;
            }
            else
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                         (pszDriver) ? pszDriver : "(unknown)");

            CPLFree(pszFilename);
            CSLDestroy(papszCreateOptions);
        }
        else if( instr == INSTR_QuietDelete )
        {
            char* pszFilename = NULL;
            GDALDriver* poDriver = NULL;

            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL )
                break;
            /* The driver instance is not used, so use a random one */
            poDriver = (GDALDriver* )GDALGetDriverByName("GTIFF");
            if( poDriver != NULL )
            {
                poDriver->QuietDelete(pszFilename);
            }
            GDALEmitEndOfJunkMarker(p);
            CPLFree(pszFilename);
        }
        else if( instr == INSTR_AddBand )
        {
            if( poDS == NULL )
                break;
            int nType;
            char** papszOptions = NULL;
            if( !GDALPipeRead(p, &nType) ||
                !GDALPipeRead(p, &papszOptions) )
                break;
            CPLErr eErr = poDS->AddBand((GDALDataType)nType, papszOptions);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr == CE_None )
            {
                int nBandCount = poDS->GetRasterCount();
                GDALPipeWrite(p, aBands, poDS->GetRasterBand(nBandCount));
            }
            CSLDestroy(papszOptions);
        }
        else if( instr == INSTR_GetGeoTransform )
        {
            if( poDS == NULL )
                break;
            double adfGeoTransform[6];
            CPLErr eErr = poDS->GetGeoTransform(adfGeoTransform);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr != CE_Failure )
            {
                GDALPipeWrite(p, 6 * sizeof(double), adfGeoTransform);
            }
        }
        else if( instr == INSTR_SetGeoTransform )
        {
            if( poDS == NULL )
                break;
            double adfGeoTransform[6];
            if( !GDALPipeRead(p, 6 * sizeof(double), adfGeoTransform) )
                break;
            CPLErr eErr = poDS->SetGeoTransform(adfGeoTransform);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_GetProjectionRef )
        {
            if( poDS == NULL )
                break;
            const char* pszVal = poDS->GetProjectionRef();
            //GDALPipeWrite(p, strlen("some_junk\xDE"), "some_junk\xDE");
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, pszVal);
        }
        else if( instr == INSTR_SetProjection )
        {
            if( poDS == NULL )
                break;
            char* pszProjection = NULL;
            if( !GDALPipeRead(p, &pszProjection) )
                break;
            CPLErr eErr = poDS->SetProjection(pszProjection);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(pszProjection);
        }
        else if( instr == INSTR_GetGCPCount )
        {
            if( poDS == NULL )
                break;
            int nGCPCount = poDS->GetGCPCount();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, nGCPCount );
        }
        else if( instr == INSTR_GetGCPProjection )
        {
            if( poDS == NULL )
                break;
            const char* pszVal = poDS->GetGCPProjection();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, pszVal);
        }
        else if( instr == INSTR_GetGCPs )
        {
            if( poDS == NULL )
                break;
            int nGCPCount = poDS->GetGCPCount();
            const GDAL_GCP* pasGCPs = poDS->GetGCPs();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, nGCPCount, pasGCPs);
        }
        else if( instr == INSTR_SetGCPs )
        {
            if( poDS == NULL )
                break;
            int nGCPCount;
            GDAL_GCP* pasGCPs = NULL;
            if( !GDALPipeRead(p, &nGCPCount, &pasGCPs) )
                break;
            char* pszGCPProjection = NULL;
            if( !GDALPipeRead(p, &pszGCPProjection) )
                break;
            CPLErr eErr = poDS->SetGCPs(nGCPCount, pasGCPs, pszGCPProjection);
            GDALDeinitGCPs(nGCPCount, pasGCPs);
            CPLFree(pasGCPs);
            CPLFree(pszGCPProjection);

            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr );

        }
        else if( instr == INSTR_GetFileList )
        {
            if( poDS == NULL )
                break;
            char** papszFileList = poDS->GetFileList();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, papszFileList);
            CSLDestroy(papszFileList);
        }
        else if( instr == INSTR_FlushCache )
        {
            if( poDS )
                poDS->FlushCache();
            GDALEmitEndOfJunkMarker(p);
        }
        /*else if( instr == INSTR_SetDescription )
        {
            if( poDS == NULL )
                break;
            char* pszDescription = NULL;
            if( !GDALPipeRead(p, &pszDescription) )
                break;
            poDS->SetDescription(pszDescription);
            CPLFree(pszDescription);
            GDALEmitEndOfJunkMarker(p);
        }*/
        else if( instr == INSTR_GetMetadata )
        {
            if( poDS == NULL )
                break;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &pszDomain) )
                break;
            char** papszMD = poDS->GetMetadata(pszDomain);
            GDALEmitEndOfJunkMarker(p);
            CPLFree(pszDomain);
            GDALPipeWrite(p, papszMD);
        }
        else if( instr == INSTR_GetMetadataItem )
        {
            if( poDS == NULL )
                break;
            char* pszName = NULL;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &pszName) ||
                !GDALPipeRead(p, &pszDomain) )
            {
                CPLFree(pszName);
                CPLFree(pszDomain);
                break;
            }
            const char* pszVal = poDS->GetMetadataItem(pszName, pszDomain);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, pszVal);
            CPLFree(pszName);
            CPLFree(pszDomain);
        }
        else if( instr == INSTR_SetMetadata )
        {
            if( poDS == NULL )
                break;
            char** papszMetadata = NULL;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &papszMetadata) ||
                !GDALPipeRead(p, &pszDomain) )
            {
                CSLDestroy(papszMetadata);
                CPLFree(pszDomain);
                break;
            }
            CPLErr eErr = poDS->SetMetadata(papszMetadata, pszDomain);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CSLDestroy(papszMetadata);
            CPLFree(pszDomain);
        }
        else if( instr == INSTR_SetMetadataItem )
        {
            if( poDS == NULL )
                break;
            char* pszName = NULL;
            char* pszValue = NULL;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &pszName) ||
                !GDALPipeRead(p, &pszValue) ||
                !GDALPipeRead(p, &pszDomain) )
            {
                CPLFree(pszName);
                CPLFree(pszValue);
                CPLFree(pszDomain);
                break;
            }
            CPLErr eErr = poDS->SetMetadataItem(pszName, pszValue, pszDomain);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(pszName);
            CPLFree(pszValue);
            CPLFree(pszDomain);
        }
        else if( instr == INSTR_IBuildOverviews )
        {
            if( poDS == NULL )
                break;
            char* pszResampling = NULL;
            int nOverviews;
            int* panOverviewList = NULL;
            int nListBands;
            int* panBandList = NULL;
            if( !GDALPipeRead(p, &pszResampling) ||
                !GDALPipeRead(p, &nOverviews) ||
                !GDALPipeRead(p, nOverviews, &panOverviewList) ||
                !GDALPipeRead(p, &nListBands) ||
                !GDALPipeRead(p, nListBands, &panBandList) )
            {
                CPLFree(pszResampling);
                CPLFree(panOverviewList);
                CPLFree(panBandList);
                break;
            }

            CPLErr eErr = poDS->BuildOverviews(pszResampling,
                                               nOverviews, panOverviewList,
                                               nListBands, panBandList,
                                               RunSyncProgress, p);

            CPLFree(pszResampling);
            CPLFree(panOverviewList);
            CPLFree(panBandList);

            if( !GDALEmitEXIT(p, (eErr != CE_Failure) ? INSTR_EXIT : INSTR_EXIT_FAIL) )
                break;
        }
        else if( instr == INSTR_AdviseRead )
        {
            if( poDS == NULL )
                break;
            int nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize;
            int nDT;
            int nBandCount;
            int *panBandList = NULL;
            char** papszOptions = NULL;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nDT) ||
                !GDALPipeRead(p, &nBandCount) ||
                !GDALPipeRead(p, nBandCount, &panBandList) ||
                !GDALPipeRead(p, &papszOptions) )
            {
                CPLFree(panBandList);
                CSLDestroy(papszOptions);
                break;
            }

            CPLErr eErr = poDS->AdviseRead(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                                           (GDALDataType)nDT,
                                           nBandCount, panBandList,
                                           papszOptions);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(panBandList);
            CSLDestroy(papszOptions);
        }
        else if( instr == INSTR_IRasterIO_Read )
        {
            if( poDS == NULL )
                break;
            int nXOff, nYOff, nXSize, nYSize;
            int nBufXSize, nBufYSize;
            GDALDataType eBufType;
            int nBufType;
            int nBandCount;
            int* panBandMap = NULL;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nBufType) ||
                !GDALPipeRead(p, &nBandCount) ||
                !GDALPipeRead(p, nBandCount, &panBandMap) )
                break;
            eBufType = (GDALDataType)nBufType;
            int nSize = nBufXSize * nBufYSize * nBandCount *
                (GDALGetDataTypeSize(eBufType) / 8);
            if( nSize > nBufferSize )
            {
                nBufferSize = nSize;
                pBuffer = CPLRealloc(pBuffer, nSize);
            }

            CPLErr eErr = poDS->RasterIO(GF_Read,
                                         nXOff, nYOff, nXSize, nYSize,
                                         pBuffer, nBufXSize, nBufYSize,
                                         eBufType,
                                         nBandCount, panBandMap,
                                         0, 0, 0);
            CPLFree(panBandMap);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr != CE_Failure )
                GDALPipeWrite(p, nSize, pBuffer);
        }
        else if( instr == INSTR_IRasterIO_Write )
        {
            if( poDS == NULL )
                break;
            int nXOff, nYOff, nXSize, nYSize;
            int nBufXSize, nBufYSize;
            GDALDataType eBufType;
            int nBufType;
            int nBandCount;
            int* panBandMap = NULL;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nBufType) ||
                !GDALPipeRead(p, &nBandCount) ||
                !GDALPipeRead(p, nBandCount, &panBandMap) )
                break;
            eBufType = (GDALDataType)nBufType;
            int nExpectedSize = nBufXSize * nBufYSize * nBandCount *
                (GDALGetDataTypeSize(eBufType) / 8);
            int nSize;
            if( !GDALPipeRead(p, &nSize) )
                break;
            if( nSize != nExpectedSize )
                break;
            if( nSize > nBufferSize )
            {
                nBufferSize = nSize;
                pBuffer = CPLRealloc(pBuffer, nSize);
            }
            if( !GDALPipeRead_nolength(p, nSize, pBuffer) )
                break;

            CPLErr eErr = poDS->RasterIO(GF_Write,
                                         nXOff, nYOff, nXSize, nYSize,
                                         pBuffer, nBufXSize, nBufYSize,
                                         eBufType,
                                         nBandCount, panBandMap,
                                         0, 0, 0);
            CPLFree(panBandMap);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_CreateMaskBand )
        {
            if( poDS == NULL )
                break;
            int nFlags;
            if( !GDALPipeRead(p, &nFlags) )
                break;
            CPLErr eErr = poDS->CreateMaskBand(nFlags);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr > INSTR_Band_First && instr < INSTR_Band_End )
        {
            int iBand;
            if( !GDALPipeRead(p, &iBand) )
                break;
            if( iBand < 0 || iBand >= (int)aBands.size() )
                break;
            poBand = aBands[iBand];
        }
        else
            break;

        if( instr == INSTR_Band_FlushCache )
        {
            CPLErr eErr = poBand->FlushCache();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_GetCategoryNames )
        {
            char** papszCategoryNames = poBand->GetCategoryNames();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, papszCategoryNames);
        }
        else if( instr == INSTR_Band_SetCategoryNames )
        {
            char** papszCategoryNames = NULL;
            if( !GDALPipeRead(p, &papszCategoryNames) )
                break;
            CPLErr eErr = poBand->SetCategoryNames(papszCategoryNames);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CSLDestroy(papszCategoryNames);
        }
        else if( instr == INSTR_Band_SetDescription )
        {
            char* pszDescription = NULL;
            if( !GDALPipeRead(p, &pszDescription) )
                break;
            poBand->SetDescription(pszDescription);
            CPLFree(pszDescription);
            GDALEmitEndOfJunkMarker(p);
        }
        else if( instr == INSTR_Band_GetMetadata )
        {
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &pszDomain) )
                break;
            char** papszMD = poBand->GetMetadata(pszDomain);
            GDALEmitEndOfJunkMarker(p);
            CPLFree(pszDomain);
            GDALPipeWrite(p, papszMD);
        }
        else if( instr == INSTR_Band_GetMetadataItem )
        {
            char* pszName = NULL;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &pszName) ||
                !GDALPipeRead(p, &pszDomain) )
            {
                CPLFree(pszName);
                CPLFree(pszDomain);
                break;
            }
            const char* pszVal = poBand->GetMetadataItem(pszName, pszDomain);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, pszVal);
            CPLFree(pszName);
            CPLFree(pszDomain);
        }
        else if( instr == INSTR_Band_SetMetadata )
        {
            char** papszMetadata = NULL;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &papszMetadata) ||
                !GDALPipeRead(p, &pszDomain) )
            {
                CSLDestroy(papszMetadata);
                CPLFree(pszDomain);
                break;
            }
            CPLErr eErr = poBand->SetMetadata(papszMetadata, pszDomain);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CSLDestroy(papszMetadata);
            CPLFree(pszDomain);
        }
        else if( instr == INSTR_Band_SetMetadataItem )
        {
            char* pszName = NULL;
            char* pszValue = NULL;
            char* pszDomain = NULL;
            if( !GDALPipeRead(p, &pszName) ||
                !GDALPipeRead(p, &pszValue) ||
                !GDALPipeRead(p, &pszDomain) )
            {
                CPLFree(pszName);
                CPLFree(pszValue);
                CPLFree(pszDomain);
                break;
            }
            CPLErr eErr = poBand->SetMetadataItem(pszName, pszValue, pszDomain);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(pszName);
            CPLFree(pszValue);
            CPLFree(pszDomain);
        }
        else if( instr == INSTR_Band_GetColorInterpretation )
        {
            GDALColorInterp eInterp = poBand->GetColorInterpretation();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eInterp);
        }
        else if( instr == INSTR_Band_SetColorInterpretation )
        {
            int nVal;
            if( !GDALPipeRead(p, &nVal ) )
                break;
            CPLErr eErr = poBand->SetColorInterpretation((GDALColorInterp)nVal);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_GetNoDataValue )
        {
            int bSuccess;
            double dfVal = poBand->GetNoDataValue(&bSuccess);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bSuccess);
            GDALPipeWrite(p, dfVal);
        }
        else if( instr == INSTR_Band_GetMinimum )
        {
            int bSuccess;
            double dfVal = poBand->GetMinimum(&bSuccess);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bSuccess);
            GDALPipeWrite(p, dfVal);
        }
        else if( instr == INSTR_Band_GetMaximum )
        {
            int bSuccess;
            double dfVal = poBand->GetMaximum(&bSuccess);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bSuccess);
            GDALPipeWrite(p, dfVal);
        }
        else if( instr == INSTR_Band_GetScale )
        {
            int bSuccess;
            double dfVal = poBand->GetScale(&bSuccess);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bSuccess);
            GDALPipeWrite(p, dfVal);
        }
        else if( instr == INSTR_Band_GetOffset )
        {
            int bSuccess;
            double dfVal = poBand->GetOffset(&bSuccess);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bSuccess);
            GDALPipeWrite(p, dfVal);
        }
        else if( instr == INSTR_Band_SetNoDataValue )
        {
            double dfVal;
            if( !GDALPipeRead(p, &dfVal ) )
                break;
            CPLErr eErr = poBand->SetNoDataValue(dfVal);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_SetOffset )
        {
            double dfVal;
            if( !GDALPipeRead(p, &dfVal ) )
                break;
            CPLErr eErr = poBand->SetOffset(dfVal);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_SetScale )
        {
            double dfVal;
            if( !GDALPipeRead(p, &dfVal ) )
                break;
            CPLErr eErr = poBand->SetScale(dfVal);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_IReadBlock )
        {
            int nBlockXOff, nBlockYOff;
            if( !GDALPipeRead(p, &nBlockXOff) ||
                !GDALPipeRead(p, &nBlockYOff) )
                break;
            int nBlockXSize, nBlockYSize;
            poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
            int nSize = nBlockXSize * nBlockYSize *
                (GDALGetDataTypeSize(poBand->GetRasterDataType()) / 8);
            if( nSize > nBufferSize )
            {
                nBufferSize = nSize;
                pBuffer = CPLRealloc(pBuffer, nSize);
            }
            CPLErr eErr = poBand->ReadBlock(nBlockXOff, nBlockYOff, pBuffer);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            GDALPipeWrite(p, nSize, pBuffer);
        }
        else if( instr == INSTR_Band_IWriteBlock )
        {
            int nBlockXOff, nBlockYOff, nSize;
            if( !GDALPipeRead(p, &nBlockXOff) ||
                !GDALPipeRead(p, &nBlockYOff) ||
                !GDALPipeRead(p, &nSize) )
                break;
            int nBlockXSize, nBlockYSize;
            poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
            int nExpectedSize = nBlockXSize * nBlockYSize *
                (GDALGetDataTypeSize(poBand->GetRasterDataType()) / 8);
            if( nExpectedSize != nSize )
                break;
            if( nSize > nBufferSize )
            {
                nBufferSize = nSize;
                pBuffer = CPLRealloc(pBuffer, nSize);
            }
            if( !GDALPipeRead_nolength(p, nSize, pBuffer) )
                break;

            CPLErr eErr = poBand->WriteBlock(nBlockXOff, nBlockYOff, pBuffer);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_IRasterIO_Read )
        {
            int nXOff, nYOff, nXSize, nYSize;
            int nBufXSize, nBufYSize;
            GDALDataType eBufType;
            int nBufType;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nBufType) )
                break;
            eBufType = (GDALDataType)nBufType;
            int nSize = nBufXSize * nBufYSize *
                (GDALGetDataTypeSize(eBufType) / 8);
            if( nSize > nBufferSize )
            {
                nBufferSize = nSize;
                pBuffer = CPLRealloc(pBuffer, nSize);
            }

            CPLErr eErr = poBand->RasterIO(GF_Read,
                                           nXOff, nYOff, nXSize, nYSize,
                                           pBuffer, nBufXSize, nBufYSize,
                                           eBufType, 0, 0);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            GDALPipeWrite(p, nSize, pBuffer);
        }
        else if( instr == INSTR_Band_IRasterIO_Write )
        {
            int nXOff, nYOff, nXSize, nYSize;
            int nBufXSize, nBufYSize;
            GDALDataType eBufType;
            int nBufType;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nBufType) )
                break;
            eBufType = (GDALDataType)nBufType;
            int nExpectedSize = nBufXSize * nBufYSize *
                (GDALGetDataTypeSize(eBufType) / 8);
            int nSize;
            if( !GDALPipeRead(p, &nSize) )
                break;
            if( nSize != nExpectedSize )
                break;
            if( nSize > nBufferSize )
            {
                nBufferSize = nSize;
                pBuffer = CPLRealloc(pBuffer, nSize);
            }
            if( !GDALPipeRead_nolength(p, nSize, pBuffer) )
                break;

            CPLErr eErr = poBand->RasterIO(GF_Write,
                                           nXOff, nYOff, nXSize, nYSize,
                                           pBuffer, nBufXSize, nBufYSize,
                                           eBufType, 0, 0);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_GetStatistics )
        {
            int bApproxOK, bForce;
            if( !GDALPipeRead(p, &bApproxOK) ||
                !GDALPipeRead(p, &bForce) )
                break;
            double dfMin = 0.0, dfMax = 0.0, dfMean = 0.0, dfStdDev = 0.0;
            CPLErr eErr = poBand->GetStatistics(bApproxOK, bForce,
                                                &dfMin, &dfMax, &dfMean, &dfStdDev);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr == CE_None )
            {
                GDALPipeWrite(p, dfMin);
                GDALPipeWrite(p, dfMax);
                GDALPipeWrite(p, dfMean);
                GDALPipeWrite(p, dfStdDev);
            }
        }
        else if( instr == INSTR_Band_ComputeStatistics )
        {
            int bApproxOK;
            if( !GDALPipeRead(p, &bApproxOK) )
                break;
            double dfMin = 0.0, dfMax = 0.0, dfMean = 0.0, dfStdDev = 0.0;
            CPLErr eErr = poBand->ComputeStatistics(bApproxOK,
                                                    &dfMin, &dfMax, &dfMean, &dfStdDev,
                                                    NULL, NULL);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr != CE_Failure )
            {
                GDALPipeWrite(p, dfMin);
                GDALPipeWrite(p, dfMax);
                GDALPipeWrite(p, dfMean);
                GDALPipeWrite(p, dfStdDev);
            }
        }
        else if( instr == INSTR_Band_SetStatistics )
        {
            double dfMin, dfMax, dfMean, dfStdDev;
            if( !GDALPipeRead(p, &dfMin) ||
                !GDALPipeRead(p, &dfMax) ||
                !GDALPipeRead(p, &dfMean) ||
                !GDALPipeRead(p, &dfStdDev) )
                break;
            CPLErr eErr = poBand->SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_ComputeRasterMinMax )
        {
            int bApproxOK;
            if( !GDALPipeRead(p, &bApproxOK) )
                break;
            double adfMinMax[2];
            CPLErr eErr = poBand->ComputeRasterMinMax(bApproxOK, adfMinMax);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr != CE_Failure )
            {
                GDALPipeWrite(p, adfMinMax[0]);
                GDALPipeWrite(p, adfMinMax[1]);
            }
        }
        else if( instr == INSTR_Band_GetHistogram )
        {
            double dfMin, dfMax;
            int nBuckets, bIncludeOutOfRange, bApproxOK;
            if( !GDALPipeRead(p, &dfMin) ||
                !GDALPipeRead(p, &dfMax) ||
                !GDALPipeRead(p, &nBuckets) ||
                !GDALPipeRead(p, &bIncludeOutOfRange) ||
                !GDALPipeRead(p, &bApproxOK) )
                break;
            int* panHistogram = (int*) CPLMalloc(sizeof(int) * nBuckets);
            CPLErr eErr = poBand->GetHistogram(dfMin, dfMax, 
                                     nBuckets, panHistogram, 
                                     bIncludeOutOfRange, bApproxOK, NULL, NULL);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr != CE_Failure )
            {
                GDALPipeWrite(p, nBuckets * sizeof(int), panHistogram);
            }
            CPLFree(panHistogram);
        }
        else if( instr == INSTR_Band_GetDefaultHistogram )
        {
            double dfMin, dfMax;
            int nBuckets;
            int bForce;
            if( !GDALPipeRead(p, &bForce) )
                break;
            int* panHistogram = NULL;
            CPLErr eErr = poBand->GetDefaultHistogram(&dfMin, &dfMax,
                                                      &nBuckets, &panHistogram,
                                                      bForce, NULL, NULL);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            if( eErr != CE_Failure )
            {
                GDALPipeWrite(p, dfMin);
                GDALPipeWrite(p, dfMax);
                GDALPipeWrite(p, nBuckets);
                GDALPipeWrite(p, nBuckets * sizeof(int) , panHistogram);
            }
            CPLFree(panHistogram);
        }
        else if( instr == INSTR_Band_SetDefaultHistogram )
        {
            double dfMin, dfMax;
            int nBuckets;
            int* panHistogram = NULL;
            if( !GDALPipeRead(p, &dfMin) ||
                !GDALPipeRead(p, &dfMax) ||
                !GDALPipeRead(p, &nBuckets) ||
                !GDALPipeRead(p, nBuckets, &panHistogram) )
            {
                CPLFree(panHistogram);
                break;
            }
            CPLErr eErr = poBand->SetDefaultHistogram(dfMin, dfMax,
                                                      nBuckets, panHistogram);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(panHistogram);
        }
        else if( instr == INSTR_Band_HasArbitraryOverviews )
        {
            int nVal = poBand->HasArbitraryOverviews();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, nVal);
        }
        else if( instr == INSTR_Band_GetOverviewCount )
        {
            int nVal = poBand->GetOverviewCount();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, nVal);
        }
        else if( instr == INSTR_Band_GetOverview )
        {
            int iOvr;
            if( !GDALPipeRead(p, &iOvr) )
                break;
            GDALRasterBand* poOvrBand = poBand->GetOverview(iOvr);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, aBands, poOvrBand);
        }
        else if( instr == INSTR_Band_GetMaskBand )
        {
            GDALRasterBand* poMaskBand = poBand->GetMaskBand();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, aBands, poMaskBand);
        }
        else if( instr == INSTR_Band_GetMaskFlags )
        {
            int nVal = poBand->GetMaskFlags();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, nVal);
        }
        else if( instr == INSTR_Band_CreateMaskBand )
        {
            int nFlags;
            if( !GDALPipeRead(p, &nFlags) )
                break;
            CPLErr eErr = poBand->CreateMaskBand(nFlags);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_Fill )
        {
            double dfReal, dfImag;
            if( !GDALPipeRead(p, &dfReal) ||
                !GDALPipeRead(p, &dfImag) )
                break;
            CPLErr eErr = poBand->Fill(dfReal, dfImag);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_GetColorTable )
        {
            GDALColorTable* poColorTable = poBand->GetColorTable();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, poColorTable);
        }
        else if( instr == INSTR_Band_SetColorTable )
        {
            GDALColorTable* poColorTable = NULL;
            if( !GDALPipeRead(p, &poColorTable) )
                break;
            CPLErr eErr = poBand->SetColorTable(poColorTable);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            delete poColorTable;
        }
        else if( instr == INSTR_Band_GetUnitType )
        {
            const char* pszVal = poBand->GetUnitType();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, pszVal);
        }
        else if( instr == INSTR_Band_SetUnitType )
        {
            char* pszUnitType = NULL;
            if( !GDALPipeRead(p, &pszUnitType) )
                break;
            CPLErr eErr = poBand->SetUnitType(pszUnitType);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(pszUnitType);
        }
        else if( instr == INSTR_Band_BuildOverviews )
        {
            char* pszResampling = NULL;
            int nOverviews;
            int* panOverviewList = NULL;
            if( !GDALPipeRead(p, &pszResampling) ||
                !GDALPipeRead(p, &nOverviews) ||
                !GDALPipeRead(p, nOverviews, &panOverviewList) )
            {
                CPLFree(pszResampling);
                CPLFree(panOverviewList);
                break;
            }
            CPLErr eErr = poBand->BuildOverviews(pszResampling, nOverviews,
                                                 panOverviewList, NULL, NULL);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CPLFree(pszResampling);
            CPLFree(panOverviewList);
        }
        else if( instr == INSTR_Band_GetDefaultRAT )
        {
            const GDALRasterAttributeTable* poRAT = poBand->GetDefaultRAT();
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, poRAT);
        }
        else if( instr == INSTR_Band_SetDefaultRAT )
        {
            GDALRasterAttributeTable* poRAT = NULL;
            if( !GDALPipeRead(p, &poRAT) )
                break;
            CPLErr eErr = poBand->SetDefaultRAT(poRAT);
            delete poRAT;

            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
        }
        else if( instr == INSTR_Band_AdviseRead )
        {
            int nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize;
            int nDT;
            char** papszOptions = NULL;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nDT) ||
                !GDALPipeRead(p, &papszOptions) )
                break;
            CPLErr eErr = poBand->AdviseRead(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                                             (GDALDataType)nDT, papszOptions);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, eErr);
            CSLDestroy(papszOptions);
        }

        if( poSrcDS == NULL )
        {
            GDALPipeWrite(p, (int)aoErrors.size());
            for(int i=0;i<(int)aoErrors.size();i++)
            {
                GDALPipeWrite(p, aoErrors[i].eErr);
                GDALPipeWrite(p, aoErrors[i].nErrNo);
                GDALPipeWrite(p, aoErrors[i].osErrorMsg);
            }
            aoErrors.resize(0);
        }
        else
            GDALPipeWrite(p, 0);
    }

    if( poSrcDS == NULL )
        CPLPopErrorHandler();

    CPLSetThreadLocalConfigOption("GDAL_RPC", pszOldValDup);
    CPLFree(pszOldValDup);

    // fprintf(stderr, "[%d] finished = %d\n", (int)getpid(), nRet);

    if( poSrcDS == NULL && poDS != NULL )
        GDALClose((GDALDatasetH)poDS);

    CPLFree(pBuffer);

    CPLFree(asyncp.pszProgressMsg);
    if( asyncp.hMutex )
        CPLDestroyMutex(asyncp.hMutex);

    return nRet;
}

/************************************************************************/
/*                         GDALServerLoop()                             */
/************************************************************************/

int GDALServerLoop(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout)
{
#ifndef WIN32
    unsetenv("CPL_SHOW_MEM_STATS");
#endif
    CPLSetConfigOption("GDAL_RPC", "NO");

    GDALPipe* p = GDALPipeBuild(fin, fout);

    int nRet = GDALServerLoop(p, NULL, NULL, NULL);

    GDALPipeFree(p);

    return nRet;
}

/************************************************************************/
/*                        GDALClientDataset()                           */
/************************************************************************/

GDALClientDataset::GDALClientDataset(GDALServerSpawnedProcess* ssp)
{
    this->ssp = ssp;
    this->p = ssp->p;
    bFreeDriver = FALSE;
    nGCPCount = 0;
    pasGCPs = NULL;
    async = NULL;
}

/************************************************************************/
/*                        GDALClientDataset()                           */
/************************************************************************/

GDALClientDataset::GDALClientDataset(GDALPipe* p)
{
    this->ssp = NULL;
    this->p = p;
    bFreeDriver = FALSE;
    nGCPCount = 0;
    pasGCPs = NULL;
    async = NULL;
}
/************************************************************************/
/*                       ~GDALClientDataset()                           */
/************************************************************************/

GDALClientDataset::~GDALClientDataset()
{
    FlushCache();

    ProcessAsyncProgress();

    std::map<CPLString, char**>::iterator oIter = aoMapMetadata.begin();
    for( ; oIter != aoMapMetadata.end(); ++oIter )
        CSLDestroy(oIter->second);

    std::map< std::pair<CPLString,CPLString>, char*>::iterator oIterItem =
        aoMapMetadataItem.begin();
    for( ; oIterItem != aoMapMetadataItem.end(); ++oIterItem )
        CPLFree(oIterItem->second);

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs(nGCPCount, pasGCPs);
        CPLFree(pasGCPs);
    }

    if( ssp != NULL )
        GDALServerSpawnAsyncFinish(ssp);
    if( bFreeDriver )
        delete poDriver;
}

/************************************************************************/
/*                       ProcessAsyncProgress()                         */
/************************************************************************/

int GDALClientDataset::ProcessAsyncProgress()
{
    if( !async ) return TRUE;
    CPLMutexHolderD(&(async->hMutex));
    if( !async->bUpdated ) return async->bRet;
    async->bUpdated = FALSE;
    if( !GDALPipeWrite(p, INSTR_Progress) ||
        !GDALPipeWrite(p, async->dfComplete) ||
        !GDALPipeWrite(p, async->pszProgressMsg) )
        return TRUE;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return TRUE;

    int bRet = TRUE;
    if( !GDALPipeRead(p, &bRet) )
        return TRUE;
    async->bRet = bRet;
    GDALConsumeErrors(p);
    return bRet;
}
 
/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GDALClientDataset::IBuildOverviews( const char *pszResampling, 
                                           int nOverviews, int *panOverviewList, 
                                           int nListBands, int *panBandList,
                                           GDALProgressFunc pfnProgress, 
                                           void * pProgressData )
{
    CLIENT_ENTER();
    if( nOverviews < 0 || nOverviews > 1000 ||
        nListBands < 0 || nListBands > GetRasterCount() )
        return CE_Failure;

    GDALPipeWriteConfigOption(p, "BIGTIFF_OVERVIEW");
    GDALPipeWriteConfigOption(p, "COMPRESS_OVERVIEW");
    GDALPipeWriteConfigOption(p, "PREDICTOR_OVERVIEW");
    GDALPipeWriteConfigOption(p, "JPEG_QUALITY_OVERVIEW");
    GDALPipeWriteConfigOption(p, "PHOTOMETRIC_OVERVIEW");
    GDALPipeWriteConfigOption(p, "USE_RRD");
    GDALPipeWriteConfigOption(p, "HFA_USE_RRD");
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_OVR_BLOCKSIZE");
    GDALPipeWriteConfigOption(p, "GTIFF_DONT_WRITE_BLOCKS");

    if( !GDALPipeWrite(p, INSTR_IBuildOverviews) ||
        !GDALPipeWrite(p, pszResampling) ||
        !GDALPipeWrite(p, nOverviews) ||
        !GDALPipeWrite(p, nOverviews * sizeof(int), panOverviewList) ||
        !GDALPipeWrite(p, nListBands) ||
        !GDALPipeWrite(p, nListBands * sizeof(int), panBandList) )
        return CE_Failure;

    if( GDALServerLoop(p, NULL, pfnProgress, pProgressData) != 0 )
    {
        GDALConsumeErrors(p);
        return CE_Failure;
    }

    GDALConsumeErrors(p);

    for(int i=0; i<nBands;i++)
        ((GDALClientRasterBand*)papoBands[i])->ClearOverviewCache();

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALClientDataset::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType, 
                                 int nBandCount, int *panBandMap,
                                 int nPixelSpace, int nLineSpace, int nBandSpace)
{
    CLIENT_ENTER();
    CPLErr eRet = CE_Failure;

    ProcessAsyncProgress();

    if( eRWFlag == GF_Read )
    {
        /*if( GetAccess() == GA_Update )
            FlushCache();*/

        if( !GDALPipeWrite(p, INSTR_IRasterIO_Read) ||
            !GDALPipeWrite(p, nXOff) ||
            !GDALPipeWrite(p, nYOff) ||
            !GDALPipeWrite(p, nXSize) ||
            !GDALPipeWrite(p, nYSize) ||
            !GDALPipeWrite(p, nBufXSize) ||
            !GDALPipeWrite(p, nBufYSize) ||
            !GDALPipeWrite(p, eBufType) ||
            !GDALPipeWrite(p, nBandCount) ||
            !GDALPipeWrite(p, nBandCount * sizeof(int), panBandMap) )
            return CE_Failure;
        if( !GDALSkipUntilEndOfJunkMarker(p) )
            return CE_Failure;

        if( !GDALPipeRead(p, &eRet) )
            return eRet;
        if( eRet != CE_Failure )
        {
            int nSize;
            if( !GDALPipeRead(p, &nSize) )
                return CE_Failure;
            int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
            GIntBig nExpectedSize = (GIntBig)nBufXSize * nBufYSize * nBandCount * nDataTypeSize;
            if( nSize != nExpectedSize )
                return CE_Failure;
            if( nPixelSpace == nDataTypeSize &&
                nLineSpace == nBufXSize * nDataTypeSize &&
                nBandSpace == nBufYSize * nLineSpace )
            {
                if( !GDALPipeRead_nolength(p, nSize, pData) )
                    return CE_Failure;
            }
            else
            {
                GByte* pBuf = (GByte*)VSIMalloc(nSize);
                if( pBuf == NULL )
                    return CE_Failure;
                if( !GDALPipeRead_nolength(p, nSize, pBuf) )
                {
                    VSIFree(pBuf);
                    return CE_Failure;
                }
                for(int iBand=0;iBand<nBandCount;iBand++)
                {
                    for(int j=0;j<nBufYSize;j++)
                    {
                        GDALCopyWords( pBuf + (iBand * nBufYSize + j) * nBufXSize * nDataTypeSize,
                                       eBufType, nDataTypeSize,
                                       (GByte*)pData + iBand * nBandSpace + j * nLineSpace,
                                       eBufType, nPixelSpace,
                                       nBufXSize);
                    }
                }
                VSIFree(pBuf);
            }
        }
    }
    else
    {
        if( !GDALPipeWrite(p, INSTR_IRasterIO_Write) ||
            !GDALPipeWrite(p, nXOff) ||
            !GDALPipeWrite(p, nYOff) ||
            !GDALPipeWrite(p, nXSize) ||
            !GDALPipeWrite(p, nYSize) ||
            !GDALPipeWrite(p, nBufXSize) ||
            !GDALPipeWrite(p, nBufYSize) ||
            !GDALPipeWrite(p, eBufType) ||
            !GDALPipeWrite(p, nBandCount) ||
            !GDALPipeWrite(p, nBandCount * sizeof(int), panBandMap) )
            return CE_Failure;

        int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
        GIntBig nSizeBig = (GIntBig)nBufXSize * nBufYSize * nBandCount * nDataTypeSize;
        int nSize = (int)nSizeBig;
        if( nSizeBig != nSize )
            return CE_Failure;
        if( nPixelSpace == nDataTypeSize &&
            nLineSpace == nBufXSize * nDataTypeSize &&
            nBandSpace == nBufYSize * nLineSpace  )
        {
            if( !GDALPipeWrite(p, nSize, pData) )
                return CE_Failure;
        }
        else
        {
            GByte* pBuf = (GByte*)VSIMalloc(nSize);
            if( pBuf == NULL )
                return CE_Failure;
            for(int iBand=0;iBand<nBandCount;iBand++)
            {
                for(int j=0;j<nBufYSize;j++)
                {
                    GDALCopyWords( (GByte*)pData + iBand * nBandSpace + j * nLineSpace,
                                   eBufType, nPixelSpace,
                                   pBuf + (iBand * nBufYSize + j) * nBufXSize * nDataTypeSize,
                                   eBufType, nDataTypeSize,
                                   nBufXSize );
                }
            }
            if( !GDALPipeWrite(p, nSize, pBuf) )
            {
                VSIFree(pBuf);
                return CE_Failure;
            }
            VSIFree(pBuf);
        }

        if( !GDALSkipUntilEndOfJunkMarker(p) )
            return CE_Failure;
        if( !GDALPipeRead(p, &eRet) )
            return eRet;
    }

    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALClientDataset::GetGeoTransform( double * padfTransform )
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetGeoTransform) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet != CE_Failure )
    {
        if( !GDALPipeRead(p, 6 * sizeof(double), padfTransform) )
            return CE_Failure;
    }
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALClientDataset::SetGeoTransform( double * padfTransform )
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_SetGeoTransform) ||
        !GDALPipeWrite(p, 6 * sizeof(double), padfTransform) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* GDALClientDataset::GetProjectionRef()
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetProjectionRef) )
        return osProjection;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return osProjection;

    char* pszStr = NULL;
    if( !GDALPipeRead(p, &pszStr) )
        return osProjection;
    GDALConsumeErrors(p);
    if( pszStr == NULL )
        return NULL;
    osProjection = pszStr;
    CPLFree(pszStr);
    return osProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GDALClientDataset::SetProjection(const char* pszProjection)
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_SetProjection) ||
        !GDALPipeWrite(p, pszProjection))
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GDALClientDataset::GetGCPCount()
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetGCPCount) )
        return 0;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return 0;

    int nGCPCount;
    if( !GDALPipeRead(p, &nGCPCount) )
        return 0;
    GDALConsumeErrors(p);
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char * GDALClientDataset::GetGCPProjection()
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetGCPProjection) )
        return osGCPProjection;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return osGCPProjection;

    char* pszStr = NULL;
    if( !GDALPipeRead(p, &pszStr) )
        return osGCPProjection;
    GDALConsumeErrors(p);
    if( pszStr == NULL )
        return NULL;
    osGCPProjection = pszStr;
    CPLFree(pszStr);
    return osGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP * GDALClientDataset::GetGCPs()
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetGCPs) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs(nGCPCount, pasGCPs);
        CPLFree(pasGCPs);
        pasGCPs = NULL;
    }
    nGCPCount = 0;

    if( !GDALPipeRead(p, &nGCPCount, &pasGCPs) )
        return NULL;

    GDALConsumeErrors(p);
    return pasGCPs;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr GDALClientDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                                   const char *pszGCPProjection ) 
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_SetGCPs) ||
        !GDALPipeWrite(p, nGCPCount, pasGCPList) ||
        !GDALPipeWrite(p, pszGCPProjection) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** GDALClientDataset::GetFileList()
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetFileList) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    char** papszFileList = NULL;
    if( !GDALPipeRead(p, &papszFileList) )
        return NULL;
    GDALConsumeErrors(p);
    return papszFileList;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** GDALClientDataset::GetMetadata( const char * pszDomain )
{
    CLIENT_ENTER();
    if( pszDomain == NULL )
        pszDomain = "";
    std::map<CPLString, char**>::iterator oIter = aoMapMetadata.find(CPLString(pszDomain));
    if( oIter != aoMapMetadata.end() )
    {
        CSLDestroy(oIter->second);
        aoMapMetadata.erase(oIter);
    }
    if( !GDALPipeWrite(p, INSTR_GetMetadata) ||
        !GDALPipeWrite(p, pszDomain) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    char** papszMD = NULL;
    if( !GDALPipeRead(p, &papszMD) )
        return NULL;
    aoMapMetadata[pszDomain] = papszMD;
    GDALConsumeErrors(p);
    return papszMD;
}

/************************************************************************/
/*                        SetDescription()                              */
/************************************************************************/

/*void GDALClientDataset::SetDescription( const char * pszDescription )
{
    sDescription = pszDescription;
    if( !GDALPipeWrite(p, INSTR_SetDescription) ||
        !GDALPipeWrite(p, pszDescription) ||
        !GDALSkipUntilEndOfJunkMarker(p))
        return;
    GDALConsumeErrors(p);
}*/

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char* GDALClientDataset::GetMetadataItem( const char * pszName,
                                            const char * pszDomain )
{
    CLIENT_ENTER();
    if( pszDomain == NULL )
        pszDomain = "";
    std::pair<CPLString,CPLString> oPair =
        std::pair<CPLString,CPLString> (CPLString(pszDomain), CPLString(pszName));
    std::map< std::pair<CPLString,CPLString>, char*>::iterator oIter =
        aoMapMetadataItem.find(oPair);
    if( oIter != aoMapMetadataItem.end() )
    {
        CPLFree(oIter->second);
        aoMapMetadataItem.erase(oIter);
    }
    if( !GDALPipeWrite(p, INSTR_GetMetadataItem) ||
        !GDALPipeWrite(p, pszName) ||
        !GDALPipeWrite(p, pszDomain) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    char* pszItem = NULL;
    if( !GDALPipeRead(p, &pszItem) )
        return NULL;
    aoMapMetadataItem[oPair] = pszItem;
    GDALConsumeErrors(p);
    return pszItem;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

CPLErr GDALClientDataset::SetMetadata( char ** papszMetadata,
                                       const char * pszDomain )
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_SetMetadata) ||
        !GDALPipeWrite(p, papszMetadata) ||
        !GDALPipeWrite(p, pszDomain) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr GDALClientDataset::SetMetadataItem( const char * pszName,
                                           const char * pszValue,
                                           const char * pszDomain )
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_SetMetadataItem) ||
        !GDALPipeWrite(p, pszName) ||
        !GDALPipeWrite(p, pszValue) ||
        !GDALPipeWrite(p, pszDomain) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void GDALClientDataset::FlushCache()
{
    CLIENT_ENTER();
    SetPamFlags(0);
    GDALPamDataset::FlushCache();
    GDALPipeWrite(p, INSTR_FlushCache);
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return;
    GDALConsumeErrors(p);
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr GDALClientDataset::AddBand( GDALDataType eType, 
                                   char **papszOptions )
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_AddBand) ||
        !GDALPipeWrite(p, eType) ||
        !GDALPipeWrite(p, papszOptions) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;
    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet == CE_None )
    {
        GDALRasterBand* poBand = NULL;
        if( !GDALPipeRead(p, this, &poBand) )
            return CE_Failure;
        SetBand(GetRasterCount() + 1, poBand);
    }
    GDALConsumeErrors(p);
    return eRet;    
}


/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr GDALClientDataset::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize, 
                                  GDALDataType eDT, 
                                  int nBandCount, int *panBandList,
                                  char **papszOptions )
{
    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_AdviseRead) ||
        !GDALPipeWrite(p, nXOff) ||
        !GDALPipeWrite(p, nYOff) ||
        !GDALPipeWrite(p, nXSize) ||
        !GDALPipeWrite(p, nYSize) ||
        !GDALPipeWrite(p, nBufXSize) ||
        !GDALPipeWrite(p, nBufYSize) ||
        !GDALPipeWrite(p, eDT) ||
        !GDALPipeWrite(p, nBandCount) ||
        !GDALPipeWrite(p, nBandCount * sizeof(int), panBandList) ||
        !GDALPipeWrite(p, papszOptions) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

CPLErr GDALClientDataset::CreateMaskBand( int nFlags )
{
    CLIENT_ENTER();
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_INTERNAL_MASK", bRecycleChild);
    if( !GDALPipeWrite(p, INSTR_CreateMaskBand) ||
        !GDALPipeWrite(p, nFlags) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                       GDALClientRasterBand()                         */
/************************************************************************/

GDALClientRasterBand::GDALClientRasterBand(GDALPipe* p, int iSrvBand, 
                                   GDALClientDataset* poDS, int nBand, GDALAccess eAccess,
                                   int nRasterXSize, int nRasterYSize,
                                   GDALDataType eDataType,
                                   int nBlockXSize, int nBlockYSize)
{
    this->p = p;
    this->iSrvBand = iSrvBand;
    this->poDS = poDS;
    this->nBand = nBand;
    this->eAccess = eAccess;
    this->nRasterXSize = nRasterXSize;
    this->nRasterYSize = nRasterYSize;
    this->eDataType = eDataType;
    this->nBlockXSize = nBlockXSize;
    this->nBlockYSize = nBlockYSize;
    papszCategoryNames = NULL;
    poColorTable = NULL;
    pszUnitType = NULL;
    poMaskBand = NULL;
    poRAT = NULL;
}

/************************************************************************/
/*                         ~GDALClientRasterBand()                          */
/************************************************************************/

GDALClientRasterBand::~GDALClientRasterBand()
{
    CSLDestroy(papszCategoryNames);
    delete poColorTable;
    CPLFree(pszUnitType);
    delete poMaskBand;
    delete poRAT;

    std::map<int, GDALRasterBand*>::iterator oIter = aMapOvrBands.begin();
    for( ; oIter != aMapOvrBands.end(); ++oIter )
        delete oIter->second;

    std::map< std::pair<CPLString,CPLString>, char*>::iterator oIterItem =
        aoMapMetadataItem.begin();
    for( ; oIterItem != aoMapMetadataItem.end(); ++oIterItem )
        CPLFree(oIterItem->second);

    std::map<CPLString, char**>::iterator oIterMD = aoMapMetadata.begin();
    for( ; oIterMD != aoMapMetadata.end(); ++oIterMD )
        CSLDestroy(oIterMD->second);
    
    for(int i=0; i < (int)apoOldMaskBands.size(); i++)
        delete apoOldMaskBands[i];
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

int GDALClientRasterBand::WriteInstr(InstrEnum instr)
{
    return GDALPipeWrite(p, instr) &&
           GDALPipeWrite(p, iSrvBand);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::FlushCache()
{
    CLIENT_ENTER();
    CPLErr eErr = GDALPamRasterBand::FlushCache();
    if( eErr == CE_None )
    {
        if( !WriteInstr(INSTR_Band_FlushCache) )
            return CE_Failure;
        return CPLErrOnlyRet(p);
    }
    return eErr;
}
/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char ** GDALClientRasterBand::GetCategoryNames()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetCategoryNames) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    CSLDestroy(papszCategoryNames);
    papszCategoryNames = NULL;
    if( !GDALPipeRead(p, &papszCategoryNames) )
        return NULL;
    GDALConsumeErrors(p);
    return papszCategoryNames;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr GDALClientRasterBand::SetCategoryNames( char ** papszCategoryNames )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetCategoryNames) ||
        !GDALPipeWrite(p, papszCategoryNames) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                        SetDescription()                              */
/************************************************************************/

void GDALClientRasterBand::SetDescription( const char * pszDescription )
{
    CLIENT_ENTER();
    sDescription = pszDescription;
    if( !WriteInstr(INSTR_Band_SetDescription) ||
        !GDALPipeWrite(p, pszDescription) ||
        !GDALSkipUntilEndOfJunkMarker(p))
        return;
    GDALConsumeErrors(p);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** GDALClientRasterBand::GetMetadata( const char * pszDomain )
{
    CLIENT_ENTER();
    if( pszDomain == NULL )
        pszDomain = "";
    std::map<CPLString, char**>::iterator oIter = aoMapMetadata.find(CPLString(pszDomain));
    if( oIter != aoMapMetadata.end() )
    {
        CSLDestroy(oIter->second);
        aoMapMetadata.erase(oIter);
    }
    if( !WriteInstr(INSTR_Band_GetMetadata) ||
        !GDALPipeWrite(p, pszDomain) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    char** papszMD = NULL;
    if( !GDALPipeRead(p, &papszMD) )
        return NULL;
    aoMapMetadata[pszDomain] = papszMD;
    GDALConsumeErrors(p);
    return papszMD;
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char* GDALClientRasterBand::GetMetadataItem( const char * pszName,
                                            const char * pszDomain )
{
    CLIENT_ENTER();
    if( pszDomain == NULL )
        pszDomain = "";
    std::pair<CPLString,CPLString> oPair =
        std::pair<CPLString,CPLString> (CPLString(pszDomain), CPLString(pszName));
    std::map< std::pair<CPLString,CPLString>, char*>::iterator oIter =
        aoMapMetadataItem.find(oPair);
    if( oIter != aoMapMetadataItem.end() )
    {
        CPLFree(oIter->second);
        aoMapMetadataItem.erase(oIter);
    }
    if( !WriteInstr(INSTR_Band_GetMetadataItem) ||
        !GDALPipeWrite(p, pszName) ||
        !GDALPipeWrite(p, pszDomain) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    char* pszItem = NULL;
    if( !GDALPipeRead(p, &pszItem) )
        return NULL;
    aoMapMetadataItem[oPair] = pszItem;
    GDALConsumeErrors(p);
    return pszItem;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::SetMetadata( char ** papszMetadata,
                                   const char * pszDomain )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetMetadata) ||
        !GDALPipeWrite(p, papszMetadata) ||
        !GDALPipeWrite(p, pszDomain) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::SetMetadataItem( const char * pszName,
                                          const char * pszValue,
                                          const char * pszDomain )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetMetadataItem) ||
        !GDALPipeWrite(p, pszName) ||
        !GDALPipeWrite(p, pszValue) ||
        !GDALPipeWrite(p, pszDomain) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALClientRasterBand::GetColorInterpretation()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetColorInterpretation) )
        return GCI_Undefined;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return GCI_Undefined;

    int nInt;
    if( !GDALPipeRead(p, &nInt) )
        return GCI_Undefined;
    GDALConsumeErrors(p);
    return (GDALColorInterp)nInt;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GDALClientRasterBand::SetColorInterpretation(GDALColorInterp eInterp)
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetColorInterpretation) ||
        !GDALPipeWrite(p, eInterp) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

CPLErr GDALClientRasterBand::GetStatistics( int bApproxOK, int bForce,
                                            double *pdfMin, double *pdfMax, 
                                            double *pdfMean, double *pdfStdDev )
{
    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_RPC_FORCE_APPROX", "NO")) )
        bApproxOK = TRUE;
    if( !WriteInstr( INSTR_Band_GetStatistics) ||
        !GDALPipeWrite(p, bApproxOK) ||
        !GDALPipeWrite(p, bForce) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet == CE_None )
    {
        double dfMin, dfMax, dfMean, dfStdDev;
        if( !GDALPipeRead(p, &dfMin) ||
            !GDALPipeRead(p, &dfMax) ||
            !GDALPipeRead(p, &dfMean) ||
            !GDALPipeRead(p, &dfStdDev) )
            return CE_Failure;
        if( pdfMin ) *pdfMin = dfMin;
        if( pdfMax ) *pdfMax = dfMax;
        if( pdfMean ) *pdfMean = dfMean;
        if( pdfStdDev ) *pdfStdDev = dfStdDev;
    }
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr GDALClientRasterBand::ComputeStatistics( int bApproxOK, 
                                                double *pdfMin,
                                                double *pdfMax, 
                                                double *pdfMean,
                                                double *pdfStdDev,
                                                GDALProgressFunc pfnProgress,
                                                void *pProgressData )
{
    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_RPC_FORCE_APPROX", "NO")) )
        bApproxOK = TRUE;
    if( !WriteInstr(INSTR_Band_ComputeStatistics) ||
        !GDALPipeWrite(p, bApproxOK) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet != CE_Failure )
    {
        double dfMin, dfMax, dfMean, dfStdDev;
        if( !GDALPipeRead(p, &dfMin) ||
            !GDALPipeRead(p, &dfMax) ||
            !GDALPipeRead(p, &dfMean) ||
            !GDALPipeRead(p, &dfStdDev) )
            return CE_Failure;
        if( pdfMin ) *pdfMin = dfMin;
        if( pdfMax ) *pdfMax = dfMax;
        if( pdfMean ) *pdfMean = dfMean;
        if( pdfStdDev ) *pdfStdDev = dfStdDev;
    }
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

CPLErr GDALClientRasterBand::SetStatistics( double dfMin, double dfMax, 
                                        double dfMean, double dfStdDev )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetStatistics) ||
        !GDALPipeWrite(p, dfMin) ||
        !GDALPipeWrite(p, dfMax) ||
        !GDALPipeWrite(p, dfMean) ||
        !GDALPipeWrite(p, dfStdDev) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                        ComputeRasterMinMax()                         */
/************************************************************************/

CPLErr GDALClientRasterBand::ComputeRasterMinMax( int bApproxOK,
                                                  double* padfMinMax )
{
    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_RPC_FORCE_APPROX", "NO")) )
        bApproxOK = TRUE;
    if( !WriteInstr(INSTR_Band_ComputeRasterMinMax) ||
        !GDALPipeWrite(p, bApproxOK) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet != CE_Failure )
    {
        if( !GDALPipeRead(p, padfMinMax + 0) ||
            !GDALPipeRead(p, padfMinMax + 1) )
            return CE_Failure;
    }
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr GDALClientRasterBand::GetHistogram( double dfMin, double dfMax, 
                                           int nBuckets, int *panHistogram, 
                                           int bIncludeOutOfRange,
                                           int bApproxOK,
                                           GDALProgressFunc pfnProgress, 
                                           void *pProgressData )
{
    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_RPC_FORCE_APPROX", "NO")) )
        bApproxOK = TRUE;
    if( !WriteInstr(INSTR_Band_GetHistogram) ||
        !GDALPipeWrite(p, dfMin) ||
        !GDALPipeWrite(p, dfMax) ||
        !GDALPipeWrite(p, nBuckets) ||
        !GDALPipeWrite(p, bIncludeOutOfRange) ||
        !GDALPipeWrite(p, bApproxOK) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet != CE_Failure )
    {
        int nSize;
        if( !GDALPipeRead(p, &nSize) ||
            nSize != nBuckets * (int)sizeof(int) ||
            !GDALPipeRead_nolength(p, nSize, panHistogram) )
            return CE_Failure;
    }
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr GDALClientRasterBand::GetDefaultHistogram( double *pdfMin,
                                                  double *pdfMax,
                                                  int *pnBuckets,
                                                  int ** ppanHistogram,
                                                  int bForce,
                                                  GDALProgressFunc,
                                                  void *pProgressData )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetDefaultHistogram) ||
        !GDALPipeWrite(p, bForce) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;
    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet != CE_Failure )
    {
        double dfMin, dfMax;
        int nBuckets, nSize;
        if( !GDALPipeRead(p, &dfMin) ||
            !GDALPipeRead(p, &dfMax) ||
            !GDALPipeRead(p, &nBuckets) ||
            !GDALPipeRead(p, &nSize) )
            return CE_Failure;
        if( nSize != nBuckets * (int)sizeof(int) )
            return CE_Failure;
        if( pdfMin ) *pdfMin = dfMin;
        if( pdfMax ) *pdfMax = dfMax;
        if( pnBuckets ) *pnBuckets = nBuckets;
        if( ppanHistogram )
        {
            *ppanHistogram = (int*)CPLMalloc(nSize);
            if( !GDALPipeRead_nolength(p, nSize, *ppanHistogram) )
                return CE_Failure;
        }
        else
        {
            int *panHistogram = (int*)CPLMalloc(nSize);
            if( !GDALPipeRead_nolength(p, nSize, panHistogram) )
            {
                CPLFree(panHistogram);
                return CE_Failure;
            }
            CPLFree(panHistogram);
        }
    }
    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr GDALClientRasterBand::SetDefaultHistogram( double dfMin, double dfMax,
                                              int nBuckets, int *panHistogram )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetDefaultHistogram) ||
        !GDALPipeWrite(p, dfMin) ||
        !GDALPipeWrite(p, dfMax) ||
        !GDALPipeWrite(p, nBuckets) ||
        !GDALPipeWrite(p, nBuckets * sizeof(int), panHistogram) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr GDALClientRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage)
{
    CLIENT_ENTER();
    if( poDS != NULL )
        ((GDALClientDataset*)poDS)->ProcessAsyncProgress();

    if( !WriteInstr(INSTR_Band_IReadBlock) ||
        !GDALPipeWrite(p, nBlockXOff) ||
        !GDALPipeWrite(p, nBlockYOff) )
        return CE_Failure;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CE_Failure;

    CPLErr eRet = CE_Failure;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    int nSize;
    if( !GDALPipeRead(p, &nSize) ||
        nSize != nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8) ||
        !GDALPipeRead_nolength(p, nSize, pImage) )
        return CE_Failure;

    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff, void* pImage)
{
    CLIENT_ENTER();
    int nSize = nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8);
    if( !WriteInstr(INSTR_Band_IWriteBlock) ||
        !GDALPipeWrite(p, nBlockXOff) ||
        !GDALPipeWrite(p, nBlockYOff) ||
        !GDALPipeWrite(p, nSize, pImage) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALClientRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void * pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nPixelSpace, int nLineSpace )
{
    CLIENT_ENTER();
    CPLErr eRet = CE_Failure;

    if( poDS != NULL )
        ((GDALClientDataset*)poDS)->ProcessAsyncProgress();

    if( eRWFlag == GF_Read )
    {
        /*if( GetAccess() == GA_Update )
            FlushCache();*/

        if( !WriteInstr(INSTR_Band_IRasterIO_Read) ||
            !GDALPipeWrite(p, nXOff) ||
            !GDALPipeWrite(p, nYOff) ||
            !GDALPipeWrite(p, nXSize) ||
            !GDALPipeWrite(p, nYSize) ||
            !GDALPipeWrite(p, nBufXSize) ||
            !GDALPipeWrite(p, nBufYSize) ||
            !GDALPipeWrite(p, eBufType) )
            return CE_Failure;
        if( !GDALSkipUntilEndOfJunkMarker(p) )
            return CE_Failure;

        if( !GDALPipeRead(p, &eRet) )
            return eRet;

        int nSize;
        if( !GDALPipeRead(p, &nSize) )
            return CE_Failure;
        int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
        GIntBig nExpectedSize = (GIntBig)nBufXSize * nBufYSize * nDataTypeSize;
        if( nSize != nExpectedSize )
            return CE_Failure;
        if( nPixelSpace == nDataTypeSize &&
            nLineSpace == nBufXSize * nDataTypeSize )
        {
            if( !GDALPipeRead_nolength(p, nSize, pData) )
                return CE_Failure;
        }
        else
        {
            GByte* pBuf = (GByte*)VSIMalloc(nSize);
            if( pBuf == NULL )
                return CE_Failure;
            if( !GDALPipeRead_nolength(p, nSize, pBuf) )
            {
                VSIFree(pBuf);
                return CE_Failure;
            }
            for(int j=0;j<nBufYSize;j++)
            {
                GDALCopyWords( pBuf + j * nBufXSize * nDataTypeSize,
                               eBufType, nDataTypeSize,
                               (GByte*)pData + j * nLineSpace,
                               eBufType, nPixelSpace,
                               nBufXSize );
            }
            VSIFree(pBuf);
        }
    }
    else
    {
        if( !WriteInstr(INSTR_Band_IRasterIO_Write) ||
            !GDALPipeWrite(p, nXOff) ||
            !GDALPipeWrite(p, nYOff) ||
            !GDALPipeWrite(p, nXSize) ||
            !GDALPipeWrite(p, nYSize) ||
            !GDALPipeWrite(p, nBufXSize) ||
            !GDALPipeWrite(p, nBufYSize) ||
            !GDALPipeWrite(p, eBufType) )
            return CE_Failure;

        int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
        GIntBig nSizeBig = (GIntBig)nBufXSize * nBufYSize * nDataTypeSize;
        int nSize = (int)nSizeBig;
        if( nSizeBig != nSize )
            return CE_Failure;
        if( nPixelSpace == nDataTypeSize &&
            nLineSpace == nBufXSize * nDataTypeSize )
        {
            if( !GDALPipeWrite(p, nSize, pData) )
                return CE_Failure;
        }
        else
        {
            GByte* pBuf = (GByte*)VSIMalloc(nSize);
            if( pBuf == NULL )
                return CE_Failure;
            for(int j=0;j<nBufYSize;j++)
            {
                GDALCopyWords( (GByte*)pData + j * nLineSpace,
                               eBufType, nPixelSpace,
                               pBuf + j * nBufXSize * nDataTypeSize,
                               eBufType, nDataTypeSize,
                               nBufXSize );
            }
            if( !GDALPipeWrite(p, nSize, pBuf) )
            {
                VSIFree(pBuf);
                return CE_Failure;
            }
            VSIFree(pBuf);
        }

        if( !GDALSkipUntilEndOfJunkMarker(p) )
            return CE_Failure;
        if( !GDALPipeRead(p, &eRet) )
            return eRet;
    }

    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

int GDALClientRasterBand::HasArbitraryOverviews()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_HasArbitraryOverviews) )
        return 0;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return 0;

    int nInt;
    if( !GDALPipeRead(p, &nInt) )
        return 0;
    GDALConsumeErrors(p);
    return nInt;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GDALClientRasterBand::GetOverviewCount()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetOverviewCount) )
        return 0;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return 0;

    int nInt;
    if( !GDALPipeRead(p, &nInt) )
        return 0;
    GDALConsumeErrors(p);
    return nInt;
}

/************************************************************************/
/*                             GetDouble()                              */
/************************************************************************/

double GDALClientRasterBand::GetDouble( InstrEnum instr, int *pbSuccess )
{
    if( pbSuccess ) *pbSuccess = FALSE;
    if( !WriteInstr( instr) )
        return 0;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return 0;

    int bSuccess;
    double dfRet;
    if( !GDALPipeRead(p, &bSuccess) ||
        !GDALPipeRead(p, &dfRet) )
        return 0;
    if( pbSuccess )
        *pbSuccess = bSuccess;
    GDALConsumeErrors(p);
    return dfRet;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALClientRasterBand::GetNoDataValue( int *pbSuccess )
{
    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetNoDataValue, pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double GDALClientRasterBand::GetMaximum( int *pbSuccess )
{
    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetMaximum, pbSuccess);
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double GDALClientRasterBand::GetMinimum( int *pbSuccess )
{
    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetMinimum, pbSuccess);
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GDALClientRasterBand::GetOffset( int *pbSuccess )
{
    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetOffset, pbSuccess);
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GDALClientRasterBand::GetScale( int *pbSuccess )
{
    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetScale, pbSuccess);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GDALClientRasterBand::GetColorTable()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetColorTable) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    GDALColorTable* poNewColorTable = NULL;
    if( !GDALPipeRead(p, &poNewColorTable) )
        return NULL;

    if( poNewColorTable != NULL && poColorTable != NULL )
    {
        *poColorTable = *poNewColorTable;
        delete poNewColorTable;
    }
    else if( poNewColorTable != NULL && poColorTable == NULL )
    {
        poColorTable = poNewColorTable;
    }
    else if( poColorTable != NULL )
    {
        delete poColorTable;
        poColorTable = NULL;
    }

    GDALConsumeErrors(p);
    return poColorTable;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *GDALClientRasterBand::GetUnitType()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetUnitType) )
        return "";
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return "";

    CPLFree(pszUnitType);
    pszUnitType = NULL;
    if( !GDALPipeRead(p, &pszUnitType) )
        return "";
    GDALConsumeErrors(p);
    return pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::SetUnitType( const char * pszUnit )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetUnitType) ||
        !GDALPipeWrite(p, pszUnit) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}
/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GDALClientRasterBand::SetColorTable(GDALColorTable* poColorTable)
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetColorTable) )
        return CE_Failure;
    if( !GDALPipeWrite(p, poColorTable) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                              SetDouble()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::SetDouble( InstrEnum instr, double dfVal )
{
    if( !WriteInstr(instr) ||
        !GDALPipeWrite(p, dfVal) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GDALClientRasterBand::SetNoDataValue( double dfVal )
{
    CLIENT_ENTER();
    return SetDouble(INSTR_Band_SetNoDataValue, dfVal);
}

/************************************************************************/
/*                             SetScale()                               */
/************************************************************************/

CPLErr GDALClientRasterBand::SetScale( double dfVal )
{
    CLIENT_ENTER();
    return SetDouble(INSTR_Band_SetScale, dfVal);
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GDALClientRasterBand::SetOffset( double dfVal )
{
    CLIENT_ENTER();
    return SetDouble(INSTR_Band_SetOffset, dfVal);
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GDALClientRasterBand::GetOverview(int iOverview)
{
    CLIENT_ENTER();
    std::map<int, GDALRasterBand*>::iterator oIter =
        aMapOvrBandsCurrent.find(iOverview);
    if( oIter != aMapOvrBandsCurrent.end() )
        return oIter->second;

    if( !WriteInstr(INSTR_Band_GetOverview) ||
        !GDALPipeWrite(p, iOverview) )
        return NULL;

    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;
    
    GDALRasterBand* poBand = NULL;
    if( !GDALPipeRead(p, (GDALClientDataset*) NULL, &poBand) )
        return NULL;

    GDALConsumeErrors(p);

    aMapOvrBands[iOverview] = poBand;
    aMapOvrBandsCurrent[iOverview] = poBand;
    return poBand;
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *GDALClientRasterBand::GetMaskBand()
{
    CLIENT_ENTER();
    if( poMaskBand )
        return poMaskBand;

    if( !WriteInstr(INSTR_Band_GetMaskBand) )
        return NULL;

    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    GDALRasterBand* poBand = NULL;
    if( !GDALPipeRead(p, (GDALClientDataset*) NULL, &poBand) )
        return NULL;

    GDALConsumeErrors(p);

    poMaskBand = poBand;
    return poMaskBand;
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int GDALClientRasterBand::GetMaskFlags()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetMaskFlags) )
        return 0;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return 0;
    int nFlags;
    if( !GDALPipeRead(p, &nFlags) )
        return 0;
    GDALConsumeErrors(p);
    return nFlags;
}

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

CPLErr GDALClientRasterBand::CreateMaskBand( int nFlags )
{
    CLIENT_ENTER();
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_INTERNAL_MASK", bRecycleChild);
    if( !WriteInstr(INSTR_Band_CreateMaskBand) ||
        !GDALPipeWrite(p, nFlags) )
        return CE_Failure;
    CPLErr eErr = CPLErrOnlyRet(p);
    if( eErr == CE_None && poMaskBand != NULL )
    {
        apoOldMaskBands.push_back(poMaskBand);
        poMaskBand = NULL;
    }
    return eErr;
}

/************************************************************************/
/*                                Fill()                                */
/************************************************************************/

CPLErr GDALClientRasterBand::Fill(double dfRealValue, double dfImaginaryValue)
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_Fill) ||
        !GDALPipeWrite(p, dfRealValue) ||
        !GDALPipeWrite(p, dfImaginaryValue) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

CPLErr GDALClientRasterBand::BuildOverviews( const char * pszResampling,
                                             int nOverviews,
                                             int * panOverviewList,
                                             GDALProgressFunc pfnProgress,
                                             void * pProgressData )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_BuildOverviews) ||
        !GDALPipeWrite(p, pszResampling) ||
        !GDALPipeWrite(p, nOverviews) ||
        !GDALPipeWrite(p, nOverviews * sizeof(int), panOverviewList) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

const GDALRasterAttributeTable *GDALClientRasterBand::GetDefaultRAT()
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_GetDefaultRAT) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    GDALRasterAttributeTable* poNewRAT = NULL;
    if( !GDALPipeRead(p, &poNewRAT) )
        return NULL;
    
    if( poNewRAT != NULL && poRAT != NULL )
    {
        *poRAT = *poNewRAT;
        delete poNewRAT;
    }
    else if( poNewRAT != NULL && poRAT == NULL )
    {
        poRAT = poNewRAT;
    }
    else if( poRAT != NULL )
    {
        delete poRAT;
        poRAT = NULL;
    }

    GDALConsumeErrors(p);
    return poRAT;
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr GDALClientRasterBand::SetDefaultRAT( const GDALRasterAttributeTable * poRAT )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_SetDefaultRAT) ||
        !GDALPipeWrite(p, poRAT) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                     int nBufXSize, int nBufYSize, 
                                     GDALDataType eDT, char **papszOptions )
{
    CLIENT_ENTER();
    if( !WriteInstr(INSTR_Band_AdviseRead) ||
        !GDALPipeWrite(p, nXOff) ||
        !GDALPipeWrite(p, nYOff) ||
        !GDALPipeWrite(p, nXSize) ||
        !GDALPipeWrite(p, nYSize) ||
        !GDALPipeWrite(p, nBufXSize) ||
        !GDALPipeWrite(p, nBufYSize) ||
        !GDALPipeWrite(p, eDT) ||
        !GDALPipeWrite(p, papszOptions) )
        return CE_Failure;
    return CPLErrOnlyRet(p);
}

/************************************************************************/
/*                        CreateAndConnect()                            */
/************************************************************************/

GDALClientDataset* GDALClientDataset::CreateAndConnect()
{
    GDALServerSpawnedProcess* ssp = GDALServerSpawnAsync();
    if( ssp == NULL )
        return NULL;
    return new GDALClientDataset(ssp);
}

/************************************************************************/
/*                                Init()                                */
/************************************************************************/

int GDALClientDataset::Init(const char* pszFilename, GDALAccess eAccess)
{
    // FIXME find a way of transmitting the relevant config options to the forked Open() ?
    GDALPipeWriteConfigOption(p, "GTIFF_POINT_GEO_IGNORE", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_OVR_BLOCKSIZE", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GTIFF_LINEAR_UNITS", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GTIFF_IGNORE_READ_ERRORS", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_PDF_RENDERING_OPTIONS", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_PDF_DPI", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_PDF_LIB", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_PDF_LAYERS", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_PDF_LAYERS_OFF", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_JPEG_TO_RGB", bRecycleChild);
    GDALPipeWriteConfigOption(p, "RPFTOC_FORCE_RGBA", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_NETCDF_BOTTOMUP", bRecycleChild);
    GDALPipeWriteConfigOption(p, "OGR_SQLITE_SYNCHRONOUS", bRecycleChild);

    if( !GDALPipeWrite(p, INSTR_Open) ||
        !GDALPipeWrite(p, eAccess) ||
        !GDALPipeWrite(p, pszFilename))
        return FALSE;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return FALSE;
    int bRet = FALSE;
    if( !GDALPipeRead(p, &bRet) )
        return FALSE;

    if( bRet == FALSE )
    {
        GDALConsumeErrors(p);
        return FALSE;
    }

    this->eAccess = eAccess;

    char* pszDescription = NULL;
    if( !GDALPipeRead(p, &pszDescription) )
        return FALSE;
    if( pszDescription != NULL )
        SetDescription(pszDescription);
    CPLFree(pszDescription);

    char* pszDriverName = NULL;
    if( !GDALPipeRead(p, &pszDriverName) )
        return FALSE;

    if( pszDriverName != NULL )
    {
        bFreeDriver = TRUE;
        poDriver = new GDALDriver();
        poDriver->SetDescription(pszDriverName);
        CPLFree(pszDriverName);
        pszDriverName = NULL;

        while(TRUE)
        {
            char* pszKey = NULL, *pszVal = NULL;
            if( !GDALPipeRead(p, &pszKey) )
                return FALSE;
            if( pszKey == NULL )
                break;
            if( !GDALPipeRead(p, &pszVal) )
            {
                CPLFree(pszKey);
                CPLFree(pszVal);
                return FALSE;
            }
            poDriver->SetMetadataItem( pszKey, pszVal );
            CPLFree(pszKey);
            CPLFree(pszVal);
        }
    }
    CPLFree(pszDriverName);

    int bAllSame;
    if( !GDALPipeRead(p, &nRasterXSize) ||
        !GDALPipeRead(p, &nRasterYSize) ||
        !GDALPipeRead(p, &nBands) ||
        !GDALPipeRead(p, &bAllSame) )
        return FALSE;

    for(int i=0;i<nBands;i++)
    {
        GDALRasterBand* poBand = NULL;
        if( i > 0 && bAllSame )
        {
            GDALClientRasterBand* poFirstBand = (GDALClientRasterBand*) GetRasterBand(1);
            int nBlockXSize, nBlockYSize;
            poFirstBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
            poBand = new GDALClientRasterBand(p, poFirstBand->GetSrvBand() + i,
                                              this, i + 1, poFirstBand->GetAccess(),
                                              poFirstBand->GetXSize(),
                                              poFirstBand->GetYSize(),
                                              poFirstBand->GetRasterDataType(),
                                              nBlockXSize, nBlockYSize);
        }
        else
        {
            if( !GDALPipeRead(p, this, &poBand) )
                return FALSE;
            if( poBand == NULL )
                return FALSE;
        }

        SetBand(i+1, poBand);
    }

    GDALConsumeErrors(p);

    return TRUE;
}

/************************************************************************/
/*                     GDALClientDatasetGetFilename()                   */
/************************************************************************/

const char* GDALClientDatasetGetFilename(const char* pszFilename)
{
    const char* pszSpawn = CPLGetConfigOption("GDAL_RPC", "NO");
    if( EQUAL(pszSpawn, "NO") || EQUAL(pszSpawn, "OFF") ||
        EQUAL(pszSpawn, "FALSE") || EQUAL(pszSpawn, "0") )
    {
        return NULL;
    }

    if( EQUALN(pszFilename, "RPC:", 6) )
        return pszFilename + 6;

    /* Those datasets cannot work in a multi-process context */
    if( EQUALN(pszFilename, "MEM:::", 6) ||
        strstr(pszFilename, "/vsimem/") != NULL ||
        strstr(pszFilename, "/vsimem\\") != NULL )
        return NULL;

    if( !(EQUAL(pszSpawn, "YES") || EQUAL(pszSpawn, "ON") ||
               EQUAL(pszSpawn, "TRUE") || EQUAL(pszSpawn, "1")) )
    {
        CPLString osExt(CPLGetExtension(pszFilename));

        /* If the file extension is listed in the GDAL_RPC, then */
        /* we have a match */
        char** papszTokens = CSLTokenizeString2( pszSpawn, " ,", CSLT_HONOURSTRINGS );
        if( CSLFindString(papszTokens, osExt) >= 0 )
        {
            CSLDestroy(papszTokens);
            return pszFilename;
        }

        /* Otherwise let's suppose that driver names are listed in GDAL_RPC */
        /* and check if the file extension matches the extension declared by the */
        /* driver */
        char** papszIter = papszTokens;
        while( *papszIter != NULL )
        {
            GDALDriverH hDriver = GDALGetDriverByName(*papszIter);
            if( hDriver != NULL )
            {
                const char* pszDriverExt =
                    GDALGetMetadataItem(hDriver, GDAL_DMD_EXTENSION, NULL);
                if( pszDriverExt != NULL && EQUAL(pszDriverExt, osExt) )
                {
                    CSLDestroy(papszTokens);
                    return pszFilename;
                }
            }
            papszIter++;
        }
        CSLDestroy(papszTokens);
        return NULL;
    }

    return pszFilename;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GDALClientDataset::Open( GDALOpenInfo * poOpenInfo )
{
    const char* pszFilename =
        GDALClientDatasetGetFilename(poOpenInfo->pszFilename);
    if( pszFilename == NULL )
        return NULL;

    CLIENT_ENTER();

    GDALClientDataset* poDS = CreateAndConnect();
    if( poDS == NULL )
        return NULL;

    CPLErrorReset();
    if( !poDS->Init(pszFilename, poOpenInfo->eAccess) )
    {
        if( CPLGetLastErrorType() == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Could not open %s",
                    pszFilename);
        }
        delete poDS;
        return NULL;
    }
    if( poDS != NULL )
        CPLErrorReset();

    return poDS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GDALClientDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    const char* pszFilename =
        GDALClientDatasetGetFilename(poOpenInfo->pszFilename);
    if( pszFilename == NULL )
        return FALSE;

    CLIENT_ENTER();

    GDALServerSpawnedProcess* ssp = GDALServerSpawnAsync();
    if( ssp == NULL )
        return FALSE;

    GDALPipe* p = ssp->p;
    if( !GDALPipeWrite(p, INSTR_Identify) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALSkipUntilEndOfJunkMarker(p) )
    {
        GDALServerSpawnAsyncFinish(ssp);
        return FALSE;
    }

    int bRet;
    if( !GDALPipeRead(p, &bRet) )
    {
        GDALServerSpawnAsyncFinish(ssp);
        return FALSE;
    }

    GDALServerSpawnAsyncFinish(ssp);
    return bRet;
}

/************************************************************************/
/*                      GDALClientDatasetQuietDelete()                      */
/************************************************************************/

static int GDALClientDatasetQuietDelete(GDALPipe* p,
                                    const char* pszFilename)
{
    if( !GDALPipeWrite(p, INSTR_QuietDelete) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALSkipUntilEndOfJunkMarker(p) )
        return FALSE;
    GDALConsumeErrors(p);
    return TRUE;
}

/************************************************************************/
/*                          mCreateCopy()                               */
/************************************************************************/

int GDALClientDataset::mCreateCopy( const char* pszFilename,
                                    GDALDataset* poSrcDS,
                                    int bStrict, char** papszOptions, 
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData )
{
    const char* pszServerDriver =
        CSLFetchNameValue(papszOptions, "SERVER_DRIVER");
    if( pszServerDriver == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Creation options should contain a SERVER_DRIVER item");
        return FALSE;
    }

    if( !CSLFetchBoolean(papszOptions, "APPEND_SUBDATASET", FALSE) )
    {
        if( !GDALClientDatasetQuietDelete(p, pszFilename) )
            return FALSE;
    }

    GDALPipeWriteConfigOption(p, "GTIFF_POINT_GEO_IGNORE", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GTIFF_DELETE_ON_ERROR", bRecycleChild);
    GDALPipeWriteConfigOption(p, "ESRI_XML_PAM", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", bRecycleChild);
    GDALPipeWriteConfigOption(p, "OGR_SQLITE_SYNCHRONOUS", bRecycleChild);

    if( !GDALPipeWrite(p, INSTR_CreateCopy) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, bStrict) ||
        !GDALPipeWrite(p, papszOptions) )
        return FALSE;

    int bDriverOK;
    if( !GDALPipeRead(p, &bDriverOK) )
        return FALSE;

    if( !bDriverOK )
    {
        GDALConsumeErrors(p);
        return FALSE;
    }

    if( GDALServerLoop(p,
                       poSrcDS,
                       pfnProgress, pProgressData) != 0 )
    {
        GDALConsumeErrors(p);
        return FALSE;
    }

    GDALConsumeErrors(p);

    return Init(NULL, GA_Update);
}

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/

GDALDataset *GDALClientDataset::CreateCopy( const char * pszFilename, 
                                            GDALDataset * poSrcDS, int bStrict,
                                            char ** papszOptions, 
                                            GDALProgressFunc pfnProgress,
                                            void * pProgressData )
{
    CLIENT_ENTER();

    GDALClientDataset* poDS = CreateAndConnect();
    if( poDS !=NULL && !poDS->mCreateCopy(pszFilename, poSrcDS, bStrict,
                                          papszOptions,
                                          pfnProgress, pProgressData) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                            mCreate()                                 */
/************************************************************************/

int GDALClientDataset::mCreate( const char * pszFilename,
                            int nXSize, int nYSize, int nBands,
                            GDALDataType eType,
                            char ** papszOptions )
{
    const char* pszServerDriver =
        CSLFetchNameValue(papszOptions, "SERVER_DRIVER");
    if( pszServerDriver == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Creation options should contain a SERVER_DRIVER item");
        return FALSE;
    }

    if( !CSLFetchBoolean(papszOptions, "APPEND_SUBDATASET", FALSE) )
    {
        if( !GDALClientDatasetQuietDelete(p, pszFilename) )
            return FALSE;
    }

    GDALPipeWriteConfigOption(p,"GTIFF_POINT_GEO_IGNORE", bRecycleChild);
    GDALPipeWriteConfigOption(p,"GTIFF_DELETE_ON_ERROR", bRecycleChild);
    GDALPipeWriteConfigOption(p,"ESRI_XML_PAM", bRecycleChild);
    GDALPipeWriteConfigOption(p,"GTIFF_DONT_WRITE_BLOCKS", bRecycleChild);

    if( !GDALPipeWrite(p, INSTR_Create) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, nXSize) ||
        !GDALPipeWrite(p, nYSize) ||
        !GDALPipeWrite(p, nBands) ||
        !GDALPipeWrite(p, eType) ||
        !GDALPipeWrite(p, papszOptions) )
        return FALSE;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return FALSE;
    int bOK;
    if( !GDALPipeRead(p, &bOK) )
        return FALSE;

    if( !bOK )
    {
        GDALConsumeErrors(p);
        return FALSE;
    }

    GDALConsumeErrors(p);

    return Init(NULL, GA_Update);
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset* GDALClientDataset::Create( const char * pszName,
                                    int nXSize, int nYSize, int nBands,
                                    GDALDataType eType,
                                    char ** papszOptions )
{
    CLIENT_ENTER();

    GDALClientDataset* poDS = CreateAndConnect();
    if( poDS != NULL && !poDS->mCreate(pszName, nXSize, nYSize, nBands,
                                       eType, papszOptions) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                       GDALSpawnUnloadDriver()                        */
/************************************************************************/

static void GDALSpawnUnloadDriver(GDALDriver* poDriver)
{
    if( bRecycleChild )
    {
        /* Kill all unused descriptors */
        bRecycleChild = FALSE;
        for(int i=0;i<nMaxRecycled;i++)
        {
            if( aspRecycled[i] != NULL )
            {
                GDALServerSpawnAsyncFinish(aspRecycled[i]);
                aspRecycled[i] = NULL;
            }
        }
    }
}

/************************************************************************/
/*                         GDALGetRPCDriver()                         */
/************************************************************************/

GDALDriver* GDALGetRPCDriver()
{
    static GDALDriver* poDriver = NULL;
    CPLMutexHolderD(GDALGetpDMMutex());
    if( poDriver == NULL )
    {
#ifdef DEBUG
        CPLAssert(INSTR_END + 1 == sizeof(apszInstr) / sizeof(apszInstr[0]));
#endif
        /* If asserted, change GDAL_CLIENT_SERVER_PROTOCOL_MAJOR / GDAL_CLIENT_SERVER_PROTOCOL_MINOR */
        CPLAssert(INSTR_END + 1 == 80);

        const char* pszSpawnRecycle = CPLGetConfigOption("GDAL_RPC_RECYCLE", "YES");
        if( atoi(pszSpawnRecycle) > 0 )
        {
            bRecycleChild = TRUE;
            nMaxRecycled = MIN(atoi(pszSpawnRecycle), MAX_RECYCLED);
        }
        else if( CSLTestBoolean(pszSpawnRecycle) )
        {
            bRecycleChild = TRUE;
            nMaxRecycled = DEFAULT_RECYCLED;
        }
        memset(aspRecycled, 0, sizeof(aspRecycled));

        poDriver = new GDALDriver();

        poDriver->SetDescription( "RPC" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "RPC" );

        poDriver->pfnOpen = GDALClientDataset::Open;
        poDriver->pfnIdentify = GDALClientDataset::Identify;
        poDriver->pfnCreateCopy = GDALClientDataset::CreateCopy;
        poDriver->pfnCreate = GDALClientDataset::Create;
        poDriver->pfnUnloadDriver = GDALSpawnUnloadDriver;
    }
    return poDriver;
}
