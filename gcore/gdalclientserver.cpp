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

#include "cpl_port.h"

#ifdef WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET CPL_SOCKET;
  #ifndef HAVE_GETADDRINFO
    #define HAVE_GETADDRINFO 1
  #endif
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/un.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  typedef int CPL_SOCKET;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define SOCKADDR struct sockaddr
  #define WSAGetLastError() errno
  #define WSACleanup()
  #define closesocket(s) close(s)
#endif

#include "gdal_pam.h"
#include "gdal_rat.h"
#include "cpl_spawn.h"
#include "cpl_multiproc.h"

/*! 
\page gdal_api_proxy GDAL API Proxy

\section gdal_api_proxy_intro Introduction

(GDAL >= 1.10.0)

When dealing with some file formats, particularly the drivers relying on third-party
(potentially closed-source) libraries, it is difficult to ensure that those third-party
libraries will be robust to hostile/corrupted datasource.

The implemented solution is to have a (private) API_PROXY driver that will expose a GDALClientDataset
object, which will forward all the GDAL API calls to another process ("server"), where the real driver
will be effectively run. This way, if the server aborts due to a fatal error, the calling process will
be unaffected and will report a clean error instead of aborting itself.

\section gdal_api_proxy_enabling How to enable ?

The API_PROXY mechanism can be enabled by setting the GDAL_API_PROXY config option to YES.
The option can also be set to a list of file extensions that must be the only ones to trigger
this mechanism (e.g. GDAL_API_PROXY=ecw,sid).

When enabled, datasets can be handled with GDALOpen(), GDALCreate() or GDALCreateCopy() with
their nominal filename (or connexion string).

Alternatively, the API_PROXY mechanism can be used selectively on a datasource by prefixing its
name with API_PROXY:, for example GDALOpen("API_PROXY:foo.tif", GA_ReadOnly).

\section gdal_api_proxy_options Advanced options

For now, the server launched is the gdalserver executable on Windows. On Unix, the default behaviour is
to just fork() the current process. It is also possible to launch the gdalserver executable
by forcing GDAL_API_PROXY_SERVER=YES.
The full filename of the gdalserver executable can also be specified in the GDAL_API_PROXY_SERVER.

It is also possible to connect to a gdalserver in TCP, possibly on a remote host. In that case,
gdalserver must be launched on a host with "gdalserver -tcpserver the_tcp_port". And the client
must set GDAL_API_PROXY_SERVER="hostname:the_tcp_port", where hostname is a string or a IP address.

In case of many dataset opening or creation, to avoid the cost of repeated process forking,
a pool of unused connections is established. Each time a dataset is closed, the associated connection
is pushed in the pool (if there's an empty bucket). When a following dataset is to be opened, one of those
connections will be reused. This behaviour is controlled with the GDAL_API_PROXY_CONN_POOL config option
that is set to YES by default, and will keep a maximum of 4 unused connections.
GDAL_API_PROXY_CONN_POOL can be set to a integer value to specify the maximum number of unused connections.

\section gdal_api_proxy_limitations Limitations

Datasets stored in the memory virtual file system (/vsimem) or handled by the MEM driver are excluded from
the API Proxy mechanism.

Additionnaly, for GDALCreate() or GDALCreateCopy(), the VRT driver is also excluded from that mechanism.

Currently, the client dataset returned is not protected by a mutex, so it is unsafe to use it concurrently
from multiple threads. However, it is safe to use several client datasets from multiple threads.

*/

/* REMINDER: upgrade this number when the on-wire protocol changes */
/* Note: please at least keep the version exchange protocol unchanged ! */
#define GDAL_CLIENT_SERVER_PROTOCOL_MAJOR 1
#define GDAL_CLIENT_SERVER_PROTOCOL_MINOR 0

#include <map>
#include <vector>

CPL_C_START
int CPL_DLL GDALServerLoop(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout);
const char* GDALClientDatasetGetFilename(const char* pszFilename);
int CPL_DLL GDALServerLoopSocket(CPL_SOCKET nSocket);
CPL_C_END

#define BUFFER_SIZE 1024
typedef struct
{
    CPL_FILE_HANDLE fin;
    CPL_FILE_HANDLE fout;
    CPL_SOCKET      nSocket;
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
/*                            MyChdir()                                 */
/************************************************************************/

static void MyChdir(const char* pszCWD)
{
#ifdef WIN32
    SetCurrentDirectory(pszCWD);
#else
    chdir(pszCWD);
#endif
}

/************************************************************************/
/*                        MyChdirRootDirectory()                        */
/************************************************************************/

static void MyChdirRootDirectory()
{
#ifdef WIN32
    SetCurrentDirectory("C:\\");
#else
    chdir("/");
#endif
}

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
    GByte                                             abyCaps[16]; /* 16 * 8 = 128 > INSTR_END */

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
        int                 SupportsInstr(InstrEnum instr) const { return abyCaps[instr / 8] & (1 << (instr % 8)); }

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
        static CPLErr       Delete( const char * pszName );
};

/************************************************************************/
/*                       GDALClientRasterBand                           */
/************************************************************************/

class GDALClientRasterBand : public GDALPamRasterBand
{
    friend class GDALClientDataset;

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
    GByte                                            abyCaps[16]; /* 16 * 8 = 128 > INSTR_END */

    int                WriteInstr(InstrEnum instr);

    double             GetDouble( InstrEnum instr, int *pbSuccess );
    CPLErr             SetDouble( InstrEnum instr, double dfVal );

    GDALRasterBand    *CreateFakeMaskBand();

    int                                              bEnableLineCaching;
    int                                              nSuccessiveLinesRead;
    GDALDataType                                     eLastBufType;
    int                                              nLastYOff;
    GByte                                           *pabyCachedLines;
    GDALDataType                                     eCachedBufType;
    int                                              nCachedYStart;
    int                                              nCachedLines;

    void    InvalidateCachedLines();
    CPLErr  IRasterIO_read_internal(
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                int nPixelSpace, int nLineSpace );
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
                             GDALDataType eDataType, int nBlockXSize, int nBlockYSize,
                             GByte abyCaps[16]);
        ~GDALClientRasterBand();
        
        int GetSrvBand() const { return iSrvBand; }
        int SupportsInstr(InstrEnum instr) const { return abyCaps[instr / 8] & (1 << (instr % 8)); }

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

        virtual GDALRasterAttributeTable *GetDefaultRAT();
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
    p->nSocket = INVALID_SOCKET;
    p->nBufferSize = 0;
    return p;
}

static GDALPipe* GDALPipeBuild(CPL_SOCKET nSocket)
{
    GDALPipe* p = (GDALPipe*)CPLMalloc(sizeof(GDALPipe));
    p->bOK = TRUE;
    p->fin = CPL_FILE_INVALID_HANDLE;
    p->fout = CPL_FILE_INVALID_HANDLE;
    p->nSocket = nSocket;
    p->nBufferSize = 0;
    return p;
}

static GDALPipe* GDALPipeBuild(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout)
{
    GDALPipe* p = (GDALPipe*)CPLMalloc(sizeof(GDALPipe));
    p->bOK = TRUE;
    p->fin = fin;
    p->fout = fout;
    p->nSocket = INVALID_SOCKET;
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
    if( p->fout != CPL_FILE_INVALID_HANDLE )
    {
        int nRet = CPLPipeWrite(p->fout, data, length);
        if( !nRet )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Write to pipe failed");
            p->bOK = FALSE;
        }
        return nRet;
    }
    else
    {
        const char* pabyData = (const char*) data;
        int nRemain = length;
        while( nRemain > 0 )
        {
            int nRet = send(p->nSocket, pabyData, nRemain, 0);
            if( nRet < 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Write to socket failed");
                p->bOK = FALSE;
                return FALSE;
            }
            pabyData += nRet;
            nRemain -= nRet;
        }
        return TRUE;
    }
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
    if( p->nSocket != INVALID_SOCKET )
    {
        closesocket(p->nSocket);
        WSACleanup();
    }
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
    
    if( p->fout != CPL_FILE_INVALID_HANDLE )
    {
        if( CPLPipeRead(p->fin, data, length) )
            return TRUE;
        // fprintf(stderr, "[%d] Read from pipe failed\n", (int)getpid());
        CPLError(CE_Failure, CPLE_AppDefined, "Read from pipe failed");
        p->bOK = FALSE;
        return FALSE;
    }
    else
    {
        char* pabyData = (char*) data;
        int nRemain = length;
        while( nRemain > 0 )
        {
            int nRet = recv(p->nSocket, pabyData, nRemain, 0);
            if( nRet <= 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Read from socket failed");
                p->bOK = FALSE;
                return FALSE;
            }
            pabyData += nRet;
            nRemain -= nRet;
        }
        return TRUE;
    }

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
    *ppszStr = (nLength < INT_MAX-1) ? (char*) VSIMalloc(nLength + 1) : NULL;
    if( *ppszStr == NULL )
        return FALSE;
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

    *ppapszStr = (char**) VSIMalloc2(sizeof(char*), (nStrCount + 1));
    if( *ppapszStr == NULL )
        return FALSE;
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
    *ppanInt = (int*) VSIMalloc(nSize);
    if( *ppanInt == NULL )
        return FALSE;
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

    *ppoRAT = new GDALDefaultRasterAttributeTable();
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
                        GDALRasterBand** ppoBand, GByte abyCaps[16])
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
                                                  nBlockXSize, nBlockYSize, abyCaps);
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
        CPLError((CPLErr)eErr, nErrNo, "%s", pszErrorMsg ? pszErrorMsg : "unknown");
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
        CPLMutexHolderD(GDALGetphDMMutex());
        for(int i = 0; i < nMaxRecycled; i ++)
        {
            if( aspRecycled[i] == NULL )
            {
                if( !GDALEmitReset(ssp->p) )
                    break;

                aspRecycled[i] = ssp;
                return TRUE;
            }
        }
    }

    if(ssp->p->bOK)
    {
        GDALEmitEXIT(ssp->p);
    }

    CPLDebug("GDAL", "Destroy spawned process %p", ssp);
    GDALPipeFree(ssp->p);
    int nRet = ssp->sp ? CPLSpawnAsyncFinish(ssp->sp, TRUE, TRUE) : 0;
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
void CPLReinitAllMutex();

static int GDALServerLoopForked(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout)
{
    /* Do not try to close datasets at process closing */
    GDALNullifyOpenDatasetsList();
    /* Nullify the existing mutex to avoid issues with locked mutex by */
    /* parent's process threads */
    GDALNullifyProxyPoolSingleton();
#ifdef CPL_MULTIPROC_PTHREAD
    CPLReinitAllMutex();
#endif

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
        CPLMutexHolderD(GDALGetphDMMutex());
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
    const char* pszSpawnServer = CPLGetConfigOption("GDAL_API_PROXY_SERVER", "gdalserver");
#else
    const char* pszSpawnServer = CPLGetConfigOption("GDAL_API_PROXY_SERVER", "NO");
#endif

    const char* pszColon = strchr(pszSpawnServer, ':');
    if( pszColon != NULL &&
        pszColon != pszSpawnServer + 1 /* do not confuse with c:/some_path/gdalserver.exe */ )
    {
        CPLString osHost(pszSpawnServer);
        osHost.resize(pszColon - pszSpawnServer);
        CPL_SOCKET nConnSocket = INVALID_SOCKET;
        int nRet;

#ifdef WIN32
        WSADATA wsaData;

        nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (nRet != NO_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "WSAStartup() failed with error: %d\n", nRet);
            return NULL;
        }
#endif

#ifdef HAVE_GETADDRINFO
        struct addrinfo sHints;
        struct addrinfo* psResults = NULL, *psResultsIter;
        memset(&sHints, 0, sizeof(struct addrinfo));
        sHints.ai_family = AF_UNSPEC;
        sHints.ai_socktype = SOCK_STREAM;
        sHints.ai_flags = 0;
        sHints.ai_protocol = IPPROTO_TCP;

        nRet = getaddrinfo(osHost, pszColon + 1, &sHints, &psResults);
        if (nRet)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "getaddrinfo(): %s", gai_strerror(nRet));
            WSACleanup();
            return NULL;
        }

        for( psResultsIter = psResults;
             psResultsIter != NULL;
             psResultsIter = psResultsIter->ai_next)
        {
            nConnSocket = socket(psResultsIter->ai_family,
                                 psResultsIter->ai_socktype,
                                 psResultsIter->ai_protocol);
            if (nConnSocket == INVALID_SOCKET)
                continue;

            if (connect(nConnSocket, psResultsIter->ai_addr,
                        psResultsIter->ai_addrlen) != SOCKET_ERROR)
                break;

            closesocket(nConnSocket);
        }

        freeaddrinfo(psResults);

        if (psResultsIter == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Could not connect");
            WSACleanup();
            return NULL;
        }
#else
        struct sockaddr_in sockAddrIn;
        int nPort = atoi(pszColon + 1);
        sockAddrIn.sin_family = AF_INET;
        sockAddrIn.sin_addr.s_addr = inet_addr(osHost);
        if (sockAddrIn.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *hp;
            hp = gethostbyname(osHost);
            if (hp == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unknown host : %s", osHost.c_str());
                WSACleanup();
                return NULL;
            }
            else
            {
                sockAddrIn.sin_family = hp->h_addrtype;
                memcpy(&(sockAddrIn.sin_addr.s_addr), hp->h_addr, hp->h_length);
            }
        }
        sockAddrIn.sin_port = htons(nPort);

        nConnSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (nConnSocket == INVALID_SOCKET)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "socket() failed with error: %d", WSAGetLastError());
            WSACleanup();
            return NULL;
        }

        if (connect(nConnSocket, (const SOCKADDR *)&sockAddrIn, sizeof (sockAddrIn)) == SOCKET_ERROR )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "connect() function failed with error: %d", WSAGetLastError());
            closesocket(nConnSocket);
            WSACleanup();
            return NULL;
        }
#endif

        GDALServerSpawnedProcess* ssp =
                (GDALServerSpawnedProcess*)CPLMalloc(sizeof(GDALServerSpawnedProcess));
        ssp->sp = NULL;
        ssp->p = GDALPipeBuild(nConnSocket);

        CPLDebug("GDAL", "Create spawned process %p", ssp);
        if( !GDALCheckServerVersion(ssp->p) )
        {
            GDALServerSpawnAsyncFinish(ssp);
            return NULL;
        }
        return ssp;
    }

#ifndef WIN32
    VSIStatBuf sStat;
    if( VSIStat(pszSpawnServer, &sStat) == 0 && sStat.st_size == 0 )
    {
        int nConnSocket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (nConnSocket >= 0)
        {
            struct sockaddr_un sockAddrUnix;
            sockAddrUnix.sun_family = AF_UNIX;
            CPLStrlcpy(sockAddrUnix.sun_path, pszSpawnServer, sizeof(sockAddrUnix.sun_path));

            if (connect(nConnSocket, (const SOCKADDR *)&sockAddrUnix, sizeof (sockAddrUnix)) >= 0 )
            {
                GDALServerSpawnedProcess* ssp =
                    (GDALServerSpawnedProcess*)CPLMalloc(sizeof(GDALServerSpawnedProcess));
                ssp->sp = NULL;
                ssp->p = GDALPipeBuild(nConnSocket);

                CPLDebug("GDAL", "Create spawned process %p", ssp);
                if( !GDALCheckServerVersion(ssp->p) )
                {
                    GDALServerSpawnAsyncFinish(ssp);
                    return NULL;
                }
                return ssp;
            }
            else
                closesocket(nConnSocket);
        }
    }
#endif

    if( EQUAL(pszSpawnServer, "YES") || EQUAL(pszSpawnServer, "ON") ||
        EQUAL(pszSpawnServer, "TRUE")  || EQUAL(pszSpawnServer, "1") )
        pszSpawnServer = "gdalserver";
#ifdef WIN32
    const char* apszGDALServer[] = { pszSpawnServer, "-stdinout", NULL };
#else
    const char* apszGDALServer[] = { pszSpawnServer, "-pipe_in", "{pipe_in}", "-pipe_out", "{pipe_out}", NULL };
    if( strstr(pszSpawnServer, "gdalserver") == NULL )
        apszGDALServer[1] = NULL;
#endif
    int bCheckVersions = TRUE;

    CPLSpawnedProcess* sp;
#ifndef WIN32
    if( EQUAL(pszSpawnServer, "NO") || EQUAL(pszSpawnServer, "OFF") ||
        EQUAL(pszSpawnServer, "FALSE")  || EQUAL(pszSpawnServer, "0") )
    {
        sp = CPLSpawnAsync(GDALServerLoopForked, NULL, TRUE, TRUE, FALSE, NULL);
        bCheckVersions = FALSE;
    }
    else
#endif
        sp = CPLSpawnAsync(NULL, apszGDALServer, TRUE, TRUE, FALSE, NULL);

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

    const char* pszOldVal = CPLGetConfigOption("GDAL_API_PROXY", NULL);
    char* pszOldValDup = (pszOldVal) ? CPLStrdup(pszOldVal) : NULL;
    CPLSetThreadLocalConfigOption("GDAL_API_PROXY", "OFF");

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
                MyChdirRootDirectory();
                aBands.resize(0);
            }
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, TRUE);
        }
        else if( instr == INSTR_Open )
        {
            int nAccess;
            char* pszFilename = NULL;
            char* pszCWD = NULL;
            if( !GDALPipeRead(p, &nAccess) ||
                !GDALPipeRead(p, &pszFilename) ||
                !GDALPipeRead(p, &pszCWD) )
            {
                CPLFree(pszFilename);
                CPLFree(pszCWD);
                break;
            }
            if( pszCWD != NULL )
            {
                MyChdir(pszCWD);
                CPLFree(pszCWD);
            }
            if( poSrcDS != NULL )
                poDS = poSrcDS;
            else if( poDS == NULL && pszFilename != NULL )
                poDS = (GDALDataset*) GDALOpen(pszFilename, (GDALAccess)nAccess);
            CPLFree(pszFilename);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, poDS != NULL);
            if( poDS != NULL )
            {
                CPLAssert(INSTR_END < 128);
                GByte abyCaps[16]; /* 16 * 8 = 128 */
                memset(abyCaps, 0, sizeof(abyCaps));
                /* We implement all known instructions (except marker ones) */
                for(int c = 1; c < INSTR_END; c++)
                {
                    if( c != INSTR_Band_First && c != INSTR_Band_End )
                        abyCaps[c / 8] |= (1 << (c % 8));
                }
                GDALPipeWrite(p, sizeof(abyCaps), abyCaps);
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
                int nFBBlockXSize = 0, nFBBlockYSize = 0;

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
            char* pszCWD = NULL;
            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL ||
                !GDALPipeRead(p, &pszCWD) )
            {
                CPLFree(pszFilename);
                CPLFree(pszCWD);
                break;
            }

            if( pszCWD != NULL )
            {
                MyChdir(pszCWD);
                CPLFree(pszCWD);
            }

            int bRet = GDALIdentifyDriver(pszFilename, NULL) != NULL;
            CPLFree(pszFilename);
            GDALEmitEndOfJunkMarker(p);
            GDALPipeWrite(p, bRet);
            aoErrors.resize(0);
        }
        else if( instr == INSTR_Create )
        {
            char* pszFilename = NULL;
            char* pszCWD = NULL;
            int nXSize, nYSize, nBands, nDataType;
            char** papszOptions = NULL;
            GDALDriver* poDriver = NULL;
            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL ||
                !GDALPipeRead(p, &pszCWD) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBands) ||
                !GDALPipeRead(p, &nDataType) ||
                !GDALPipeRead(p, &papszOptions) )
            {
                CPLFree(pszFilename);
                CPLFree(pszCWD);
                break;
            }

            if( pszCWD != NULL )
            {
                MyChdir(pszCWD);
                CPLFree(pszCWD);
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
            char* pszSrcDescription = NULL;
            char* pszCWD = NULL;
            char** papszCreateOptions = NULL;
            GDALDriver* poDriver = NULL;
            int bStrict = FALSE;

            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL ||
                !GDALPipeRead(p, &pszSrcDescription) ||
                !GDALPipeRead(p, &pszCWD) ||
                !GDALPipeRead(p, &bStrict) ||
                !GDALPipeRead(p, &papszCreateOptions) )
            {
                CPLFree(pszFilename);
                CPLFree(pszSrcDescription);
                CPLFree(pszCWD);
                break;
            }

            CPLFree(pszSrcDescription);

            if( pszCWD != NULL )
            {
                MyChdir(pszCWD);
                CPLFree(pszCWD);
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
            char* pszCWD = NULL;

            if( !GDALPipeRead(p, &pszFilename) ||
                pszFilename == NULL ||
                !GDALPipeRead(p, &pszCWD) )
            {
                CPLFree(pszFilename);
                CPLFree(pszCWD);
                break;
            }

            if( pszCWD != NULL )
            {
                MyChdir(pszCWD);
                CPLFree(pszCWD);
            }

            GDALDriver::QuietDelete(pszFilename);

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
            int nPixelSpace, nLineSpace, nBandSpace;
            int* panBandMap = NULL;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nBufType) ||
                !GDALPipeRead(p, &nBandCount) ||
                !GDALPipeRead(p, nBandCount, &panBandMap) ||
                !GDALPipeRead(p, &nPixelSpace) ||
                !GDALPipeRead(p, &nLineSpace) ||
                !GDALPipeRead(p, &nBandSpace) )
            {
                CPLFree(panBandMap);
                break;
            }
            /* Note: only combinations of nPixelSpace, nLineSpace and
               nBandSpace that lead to compate band-interleaved or pixel-
               interleaved buffers are valid. Other combinations will lead to segfault */
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
                                         nPixelSpace, nLineSpace, nBandSpace);
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
            int nPixelSpace, nLineSpace, nBandSpace;
            int* panBandMap = NULL;
            if( !GDALPipeRead(p, &nXOff) ||
                !GDALPipeRead(p, &nYOff) ||
                !GDALPipeRead(p, &nXSize) ||
                !GDALPipeRead(p, &nYSize) ||
                !GDALPipeRead(p, &nBufXSize) ||
                !GDALPipeRead(p, &nBufYSize) ||
                !GDALPipeRead(p, &nBufType) ||
                !GDALPipeRead(p, &nBandCount) ||
                !GDALPipeRead(p, nBandCount, &panBandMap) ||
                !GDALPipeRead(p, &nPixelSpace) ||
                !GDALPipeRead(p, &nLineSpace) ||
                !GDALPipeRead(p, &nBandSpace) )
            {
                CPLFree(panBandMap);
                break;
            }
            /* Note: only combinations of nPixelSpace, nLineSpace and
               nBandSpace that lead to compate band-interleaved or pixel-
               interleaved buffers are valid. Other combinations will lead to segfault */
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
                                         nPixelSpace, nLineSpace, nBandSpace);
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
            int* panHistogram = (int*) VSIMalloc2(sizeof(int), nBuckets);
            if( panHistogram == NULL )
                break;
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

    CPLSetThreadLocalConfigOption("GDAL_API_PROXY", pszOldValDup);
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
    CPLSetConfigOption("GDAL_API_PROXY", "NO");

    GDALPipe* p = GDALPipeBuild(fin, fout);

    int nRet = GDALServerLoop(p, NULL, NULL, NULL);

    GDALPipeFree(p);

    return nRet;
}

/************************************************************************/
/*                      GDALServerLoopSocket()                          */
/************************************************************************/

int GDALServerLoopSocket(CPL_SOCKET nSocket)
{
#ifndef WIN32
    unsetenv("CPL_SHOW_MEM_STATS");
#endif
    CPLSetConfigOption("GDAL_API_PROXY", "NO");

    GDALPipe* p = GDALPipeBuild(nSocket);

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
    memset(abyCaps, 0, sizeof(abyCaps));
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
    memset(abyCaps, 0, sizeof(abyCaps));
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
    if( !SupportsInstr(INSTR_IBuildOverviews) )
        return GDALPamDataset::IBuildOverviews(pszResampling, nOverviews, panOverviewList,
                                               nListBands, panBandList,
                                               pfnProgress, pProgressData);

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
    if( !SupportsInstr(( eRWFlag == GF_Read ) ? INSTR_IRasterIO_Read : INSTR_IRasterIO_Write ) )
        return GDALPamDataset::IRasterIO( eRWFlag,
                                          nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize,
                                          eBufType, 
                                          nBandCount, panBandMap,
                                          nPixelSpace, nLineSpace, nBandSpace );

    CLIENT_ENTER();
    CPLErr eRet = CE_Failure;

    ProcessAsyncProgress();

    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    int bDirectCopy;
    if( nPixelSpace == nDataTypeSize &&
        nLineSpace == nBufXSize * nDataTypeSize &&
        (nBandSpace == nBufYSize * nLineSpace ||
            (nBandSpace == 0 && nBandCount == 1)) )
    {
        bDirectCopy = TRUE;
    }
    else if( nBandCount > 1 &&
                nPixelSpace == nBandCount * nDataTypeSize &&
                nLineSpace == nBufXSize * nPixelSpace &&
                nBandSpace == nBandCount )
    {
        bDirectCopy = TRUE;
    }
    else
        bDirectCopy = FALSE;

    if( eRWFlag == GF_Write )
    {
        for(int i=0;i<nBands;i++)
            ((GDALClientRasterBand*)GetRasterBand(i+1))->InvalidateCachedLines();
    }

    if( !GDALPipeWrite(p, ( eRWFlag == GF_Read ) ? INSTR_IRasterIO_Read : INSTR_IRasterIO_Write ) ||
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

    if( bDirectCopy )
    {
        if( !GDALPipeWrite(p, nPixelSpace) ||
            !GDALPipeWrite(p, nLineSpace) ||
            !GDALPipeWrite(p, nBandSpace) )
            return CE_Failure;
    }
    else
    {
        if( !GDALPipeWrite(p, 0) ||
            !GDALPipeWrite(p, 0) ||
            !GDALPipeWrite(p, 0) )
            return CE_Failure;
    }

    if( eRWFlag == GF_Read )
    {
        if( !GDALSkipUntilEndOfJunkMarker(p) )
            return CE_Failure;

        if( !GDALPipeRead(p, &eRet) )
            return eRet;
        if( eRet != CE_Failure )
        {
            int nSize;
            if( !GDALPipeRead(p, &nSize) )
                return CE_Failure;
            GIntBig nExpectedSize = (GIntBig)nBufXSize * nBufYSize * nBandCount * nDataTypeSize;
            if( nSize != nExpectedSize )
                return CE_Failure;
            if( bDirectCopy )
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
        GIntBig nSizeBig = (GIntBig)nBufXSize * nBufYSize * nBandCount * nDataTypeSize;
        int nSize = (int)nSizeBig;
        if( nSizeBig != nSize )
            return CE_Failure;
        if( bDirectCopy  )
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
    if( !SupportsInstr(INSTR_GetGeoTransform) )
        return GDALPamDataset::GetGeoTransform(padfTransform);

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
    if( !SupportsInstr(INSTR_SetGeoTransform) )
        return GDALPamDataset::SetGeoTransform(padfTransform);

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
    if( !SupportsInstr(INSTR_GetProjectionRef) )
        return GDALPamDataset::GetProjectionRef();

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
    if( !SupportsInstr(INSTR_SetProjection) )
        return GDALPamDataset::SetProjection(pszProjection);

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
    if( !SupportsInstr(INSTR_GetGCPCount) )
        return GDALPamDataset::GetGCPCount();

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
    if( !SupportsInstr(INSTR_GetGCPProjection) )
        return GDALPamDataset::GetGCPProjection();

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
    if( !SupportsInstr(INSTR_GetGCPs) )
        return GDALPamDataset::GetGCPs();

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
    if( !SupportsInstr(INSTR_SetGCPs) )
        return GDALPamDataset::SetGCPs(nGCPCount, pasGCPList, pszGCPProjection);

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
    if( !SupportsInstr(INSTR_GetFileList) )
        return GDALPamDataset::GetFileList();

    CLIENT_ENTER();
    if( !GDALPipeWrite(p, INSTR_GetFileList) )
        return NULL;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return NULL;

    char** papszFileList = NULL;
    if( !GDALPipeRead(p, &papszFileList) )
        return NULL;
    GDALConsumeErrors(p);

    /* If server is Windows and client is Unix, then replace backslahes */
    /* by slashes */
#ifndef WIN32
    char** papszIter = papszFileList;
    while( papszIter != NULL && *papszIter != NULL )
    {
        char* pszIter = *papszIter;
        char* pszBackSlash;
        while( (pszBackSlash = strchr(pszIter, '\\')) != NULL )
        {
            *pszBackSlash = '/';
            pszIter = pszBackSlash + 1;
        }
        papszIter ++;
    }
#endif

    return papszFileList;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** GDALClientDataset::GetMetadata( const char * pszDomain )
{
    if( !SupportsInstr(INSTR_GetMetadata) )
        return GDALPamDataset::GetMetadata(pszDomain);

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
    if( !SupportsInstr(INSTR_GetMetadataItem) )
        return GDALPamDataset::GetMetadataItem(pszName, pszDomain);

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
    if( !SupportsInstr(INSTR_SetMetadata) )
        return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);

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
    if( !SupportsInstr(INSTR_SetMetadataItem) )
        return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);

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
    if( !SupportsInstr(INSTR_FlushCache) )
    {
        GDALPamDataset::FlushCache();
        return;
    }

    for(int i=0;i<nBands;i++)
        ((GDALClientRasterBand*)GetRasterBand(i+1))->InvalidateCachedLines();

    CLIENT_ENTER();
    SetPamFlags(0);
    GDALPamDataset::FlushCache();
    if( !GDALPipeWrite(p, INSTR_FlushCache) ||
        !GDALSkipUntilEndOfJunkMarker(p) )
        return;
    GDALConsumeErrors(p);
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr GDALClientDataset::AddBand( GDALDataType eType, 
                                   char **papszOptions )
{
    if( !SupportsInstr(INSTR_AddBand) )
        return GDALPamDataset::AddBand(eType, papszOptions);

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
        if( !GDALPipeRead(p, this, &poBand, abyCaps) )
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
    if( !SupportsInstr(INSTR_AdviseRead) )
        return GDALPamDataset::AdviseRead(nXOff, nYOff, nXSize, nYSize,
                                          nBufXSize, nBufYSize, eDT, nBandCount, panBandList,
                                          papszOptions);

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
    if( !SupportsInstr(INSTR_CreateMaskBand) )
        return GDALPamDataset::CreateMaskBand(nFlags);

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
                                           GDALClientDataset* poDS,
                                           int nBand, GDALAccess eAccess,
                                           int nRasterXSize, int nRasterYSize,
                                           GDALDataType eDataType,
                                           int nBlockXSize, int nBlockYSize,
                                           GByte abyCapsIn[16])
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
    memcpy(abyCaps, abyCapsIn, sizeof(abyCaps));
    bEnableLineCaching = CSLTestBoolean(CPLGetConfigOption("GDAL_API_PROXY_LINE_CACHING", "YES"));
    nSuccessiveLinesRead = 0;
    eLastBufType = GDT_Unknown;
    nLastYOff = -1;
    pabyCachedLines = NULL;
    eCachedBufType = GDT_Unknown;
    nCachedYStart = -1;
    nCachedLines = 0;

}

/************************************************************************/
/*                         ~GDALClientRasterBand()                      */
/************************************************************************/

GDALClientRasterBand::~GDALClientRasterBand()
{
    CSLDestroy(papszCategoryNames);
    delete poColorTable;
    CPLFree(pszUnitType);
    delete poMaskBand;
    delete poRAT;
    CPLFree(pabyCachedLines);

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
/*                       CreateFakeMaskBand()                           */
/************************************************************************/

GDALRasterBand* GDALClientRasterBand::CreateFakeMaskBand()
{
    if( poMaskBand == NULL )
        poMaskBand = new GDALAllValidMaskBand(this);
    return poMaskBand;
}

/************************************************************************/
/*                             WriteInstr()                             */
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
    if( !SupportsInstr(INSTR_Band_FlushCache) )
        return GDALPamRasterBand::FlushCache();

    InvalidateCachedLines();

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
    if( !SupportsInstr(INSTR_Band_GetCategoryNames) )
        return GDALPamRasterBand::GetCategoryNames();

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
    if( !SupportsInstr(INSTR_Band_SetCategoryNames) )
        return GDALPamRasterBand::SetCategoryNames(papszCategoryNames);

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
    if( !SupportsInstr(INSTR_Band_SetDescription) )
    {
        GDALPamRasterBand::SetDescription(pszDescription);
        return;
    }

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
    if( !SupportsInstr(INSTR_Band_GetMetadata) )
        return GDALPamRasterBand::GetMetadata(pszDomain);

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
    if( !SupportsInstr(INSTR_Band_GetMetadataItem) )
        return GDALPamRasterBand::GetMetadataItem(pszName, pszDomain);

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
    if( !SupportsInstr(INSTR_Band_SetMetadata) )
        return GDALPamRasterBand::SetMetadata(papszMetadata, pszDomain);

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
    if( !SupportsInstr(INSTR_Band_SetMetadataItem) )
        return GDALPamRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);

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
    if( !SupportsInstr(INSTR_Band_GetColorInterpretation) )
        return GDALPamRasterBand::GetColorInterpretation();

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
    if( !SupportsInstr(INSTR_Band_SetColorInterpretation) )
        return GDALPamRasterBand::SetColorInterpretation(eInterp);

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
    if( !SupportsInstr(INSTR_Band_GetStatistics) )
        return GDALPamRasterBand::GetStatistics(
            bApproxOK, bForce, pdfMin, pdfMax, pdfMean, pdfStdDev);

    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_API_PROXY_FORCE_APPROX", "NO")) )
        bApproxOK = TRUE;
    CPLErr eDefaultRet = CE_Failure;
    if( CSLTestBoolean(CPLGetConfigOption("QGIS_HACK", "NO")) )
    {
        if( pdfMin ) *pdfMin = 0;
        if( pdfMax ) *pdfMax = 255;
        if( pdfMean ) *pdfMean = 0;
        if( pdfStdDev ) *pdfStdDev = 0;
        eDefaultRet = CE_None;
    }
    if( !WriteInstr( INSTR_Band_GetStatistics) ||
        !GDALPipeWrite(p, bApproxOK) ||
        !GDALPipeWrite(p, bForce) )
        return eDefaultRet;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return eDefaultRet;

    CPLErr eRet = eDefaultRet;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet == CE_None )
    {
        double dfMin, dfMax, dfMean, dfStdDev;
        if( !GDALPipeRead(p, &dfMin) ||
            !GDALPipeRead(p, &dfMax) ||
            !GDALPipeRead(p, &dfMean) ||
            !GDALPipeRead(p, &dfStdDev) )
            return eDefaultRet;
        if( pdfMin ) *pdfMin = dfMin;
        if( pdfMax ) *pdfMax = dfMax;
        if( pdfMean ) *pdfMean = dfMean;
        if( pdfStdDev ) *pdfStdDev = dfStdDev;
    }
    else if( eDefaultRet == CE_None )
        eRet = eDefaultRet;
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
    if( !SupportsInstr(INSTR_Band_ComputeStatistics) )
        return GDALPamRasterBand::ComputeStatistics(
            bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev, pfnProgress, pProgressData);

    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_API_PROXY_FORCE_APPROX", "NO")) )
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
    if( !SupportsInstr(INSTR_Band_SetStatistics) )
        return GDALPamRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);

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
    if( !SupportsInstr(INSTR_Band_ComputeRasterMinMax) )
        return GDALPamRasterBand::ComputeRasterMinMax(bApproxOK, padfMinMax);

    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_API_PROXY_FORCE_APPROX", "NO")) )
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
    if( !SupportsInstr(INSTR_Band_GetHistogram) )
        return GDALPamRasterBand::GetHistogram(
            dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange, bApproxOK, pfnProgress, pProgressData);

    CLIENT_ENTER();
    if( !bApproxOK && CSLTestBoolean(CPLGetConfigOption("GDAL_API_PROXY_FORCE_APPROX", "NO")) )
        bApproxOK = TRUE;
    CPLErr eDefaultRet = CE_Failure;
    if( CSLTestBoolean(CPLGetConfigOption("QGIS_HACK", "NO")) )
    {
        memset(panHistogram, 0, sizeof(int) * nBuckets);
        eDefaultRet = CE_None;
    }
    if( !WriteInstr(INSTR_Band_GetHistogram) ||
        !GDALPipeWrite(p, dfMin) ||
        !GDALPipeWrite(p, dfMax) ||
        !GDALPipeWrite(p, nBuckets) ||
        !GDALPipeWrite(p, bIncludeOutOfRange) ||
        !GDALPipeWrite(p, bApproxOK) )
        return eDefaultRet;
    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return eDefaultRet;

    CPLErr eRet = eDefaultRet;
    if( !GDALPipeRead(p, &eRet) )
        return eRet;
    if( eRet != CE_Failure )
    {
        int nSize;
        if( !GDALPipeRead(p, &nSize) ||
            nSize != nBuckets * (int)sizeof(int) ||
            !GDALPipeRead_nolength(p, nSize, panHistogram) )
            return eDefaultRet;
    }
    else if( eDefaultRet == CE_None )
        eRet = eDefaultRet;
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
                                                  GDALProgressFunc pfnProgress,
                                                  void *pProgressData )
{
    if( !SupportsInstr(INSTR_Band_GetDefaultHistogram) )
        return GDALPamRasterBand::GetDefaultHistogram(
            pdfMin, pdfMax, pnBuckets, ppanHistogram, bForce, pfnProgress, pProgressData);

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
            *ppanHistogram = (int*)VSIMalloc(nSize);
            if( *ppanHistogram == NULL )
                return CE_Failure;
            if( !GDALPipeRead_nolength(p, nSize, *ppanHistogram) )
                return CE_Failure;
        }
        else
        {
            int *panHistogram = (int*)VSIMalloc(nSize);
            if( panHistogram == NULL )
                return CE_Failure;
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
    if( !SupportsInstr(INSTR_Band_SetDefaultHistogram) )
        return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);

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
    if( !SupportsInstr(INSTR_Band_IReadBlock) )
        return CE_Failure;

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
    if( !SupportsInstr(INSTR_Band_IWriteBlock) )
        return CE_Failure;

    InvalidateCachedLines();

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
/*                      IRasterIO_read_internal()                       */
/************************************************************************/

CPLErr GDALClientRasterBand::IRasterIO_read_internal(
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void * pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nPixelSpace, int nLineSpace )
{
    CPLErr eRet = CE_Failure;

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

    GDALConsumeErrors(p);
    return eRet;
}

/************************************************************************/
/*                       InvalidateCachedLines()                        */
/************************************************************************/

void GDALClientRasterBand::InvalidateCachedLines()
{
    nSuccessiveLinesRead = 0;
    nCachedYStart = -1;
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
    if( !SupportsInstr( (eRWFlag == GF_Read) ? INSTR_Band_IRasterIO_Read : INSTR_Band_IRasterIO_Write) )
        return GDALPamRasterBand::IRasterIO( eRWFlag,
                                             nXOff, nYOff, nXSize, nYSize,
                                             pData, nBufXSize, nBufYSize,
                                             eBufType, 
                                             nPixelSpace, nLineSpace );

    CLIENT_ENTER();
    CPLErr eRet = CE_Failure;

    if( poDS != NULL )
        ((GDALClientDataset*)poDS)->ProcessAsyncProgress();

    if( eRWFlag == GF_Read )
    {
        /*if( GetAccess() == GA_Update )
            FlushCache();*/

        /* Detect scanline reading pattern and read several rows in advance */
        /* to save a few client/server roundtrips */
        if( bEnableLineCaching &&
            nXOff == 0 && nXSize == nRasterXSize && nYSize == 1 &&
            nBufXSize == nXSize && nBufYSize == nYSize )
        {
            int nBufTypeSize = GDALGetDataTypeSize(eBufType) / 8;

            /* Is the current line already cached ? */
            if( nCachedYStart >= 0 &&
                nYOff >= nCachedYStart && nYOff < nCachedYStart + nCachedLines &&
                eBufType == eCachedBufType )
            {
                nSuccessiveLinesRead ++;

                int nCachedBufTypeSize = GDALGetDataTypeSize(eCachedBufType) / 8;
                GDALCopyWords(pabyCachedLines + (nYOff - nCachedYStart) * nXSize * nCachedBufTypeSize,
                              eCachedBufType, nCachedBufTypeSize,
                              pData, eBufType, nPixelSpace,
                              nXSize);
                nLastYOff = nYOff;
                eLastBufType = eBufType;
                return CE_None;
            }

            if( nYOff == nLastYOff + 1 &&
                eBufType == eLastBufType )
            {
                nSuccessiveLinesRead ++;
                if( nSuccessiveLinesRead >= 2 )
                {
                    if( pabyCachedLines == NULL )
                    {
                        nCachedLines = 10 * 1024 * 1024 / (nXSize * nBufTypeSize);
                        if( nCachedLines > 1 )
                            pabyCachedLines = (GByte*) VSIMalloc(
                                nCachedLines * nXSize * nBufTypeSize);
                    }
                    if( pabyCachedLines != NULL )
                    {
                        int nLinesToRead = nCachedLines;
                        if( nYOff + nLinesToRead > nRasterYSize )
                            nLinesToRead = nRasterYSize - nYOff;
                        eRet = IRasterIO_read_internal( nXOff, nYOff, nXSize, nLinesToRead,
                                                        pabyCachedLines, nXSize, nLinesToRead,
                                                        eBufType,
                                                        nBufTypeSize, nBufTypeSize * nXSize );
                        if( eRet == CE_None )
                        {
                            eCachedBufType = eBufType;
                            nCachedYStart = nYOff;

                            int nCachedBufTypeSize = GDALGetDataTypeSize(eCachedBufType) / 8;
                            GDALCopyWords(pabyCachedLines + (nYOff - nCachedYStart) * nXSize * nCachedBufTypeSize,
                                        eCachedBufType, nCachedBufTypeSize,
                                        pData, eBufType, nPixelSpace,
                                        nXSize);
                            nLastYOff = nYOff;
                            eLastBufType = eBufType;

                            return CE_None;
                        }
                        else
                            InvalidateCachedLines();
                    }
                }
            }
            else
                InvalidateCachedLines();
        }
        else
            InvalidateCachedLines();

        nLastYOff = nYOff;
        eLastBufType = eBufType;

        return IRasterIO_read_internal( nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize,
                                        eBufType,
                                        nPixelSpace, nLineSpace );
    }
    else
    {
        InvalidateCachedLines();

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

        GDALConsumeErrors(p);
        return eRet;
    }
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

int GDALClientRasterBand::HasArbitraryOverviews()
{
    if( !SupportsInstr(INSTR_Band_HasArbitraryOverviews) )
        return GDALPamRasterBand::HasArbitraryOverviews();

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
    if( !SupportsInstr(INSTR_Band_GetOverviewCount) )
        return GDALPamRasterBand::GetOverviewCount();

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
    if( !SupportsInstr(INSTR_Band_GetNoDataValue) )
        return GDALPamRasterBand::GetNoDataValue(pbSuccess);

    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetNoDataValue, pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double GDALClientRasterBand::GetMaximum( int *pbSuccess )
{
    if( !SupportsInstr(INSTR_Band_GetMaximum) )
        return GDALPamRasterBand::GetMaximum(pbSuccess);

    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetMaximum, pbSuccess);
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double GDALClientRasterBand::GetMinimum( int *pbSuccess )
{
    if( !SupportsInstr(INSTR_Band_GetMinimum) )
        return GDALPamRasterBand::GetMinimum(pbSuccess);

    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetMinimum, pbSuccess);
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GDALClientRasterBand::GetOffset( int *pbSuccess )
{
    if( !SupportsInstr(INSTR_Band_GetOffset) )
        return GDALPamRasterBand::GetOffset(pbSuccess);

    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetOffset, pbSuccess);
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GDALClientRasterBand::GetScale( int *pbSuccess )
{
    if( !SupportsInstr(INSTR_Band_GetScale) )
        return GDALPamRasterBand::GetScale(pbSuccess);

    CLIENT_ENTER();
    return GetDouble(INSTR_Band_GetScale, pbSuccess);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GDALClientRasterBand::GetColorTable()
{
    if( !SupportsInstr(INSTR_Band_GetColorTable) )
        return GDALPamRasterBand::GetColorTable();

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
    if( !SupportsInstr(INSTR_Band_GetUnitType) )
        return GDALPamRasterBand::GetUnitType();

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
    return pszUnitType ? pszUnitType : "";
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr GDALClientRasterBand::SetUnitType( const char * pszUnit )
{
    if( !SupportsInstr(INSTR_Band_SetUnitType) )
        return GDALPamRasterBand::SetUnitType(pszUnit);

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
    if( !SupportsInstr(INSTR_Band_SetColorTable) )
        return GDALPamRasterBand::SetColorTable(poColorTable);

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
    if( !SupportsInstr(INSTR_Band_SetNoDataValue) )
        return GDALPamRasterBand::SetNoDataValue(dfVal);

    CLIENT_ENTER();
    return SetDouble(INSTR_Band_SetNoDataValue, dfVal);
}

/************************************************************************/
/*                             SetScale()                               */
/************************************************************************/

CPLErr GDALClientRasterBand::SetScale( double dfVal )
{
    if( !SupportsInstr(INSTR_Band_SetScale) )
        return GDALPamRasterBand::SetScale(dfVal);

    CLIENT_ENTER();
    return SetDouble(INSTR_Band_SetScale, dfVal);
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GDALClientRasterBand::SetOffset( double dfVal )
{
    if( !SupportsInstr(INSTR_Band_SetOffset) )
        return GDALPamRasterBand::SetOffset(dfVal);

    CLIENT_ENTER();
    return SetDouble(INSTR_Band_SetOffset, dfVal);
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GDALClientRasterBand::GetOverview(int iOverview)
{
    if( !SupportsInstr(INSTR_Band_GetOverview) )
        return GDALPamRasterBand::GetOverview(iOverview);

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
    if( !GDALPipeRead(p, (GDALClientDataset*) NULL, &poBand, abyCaps) )
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
    if( !SupportsInstr(INSTR_Band_GetMaskBand) )
        return GDALPamRasterBand::GetMaskBand();

    CLIENT_ENTER();
    if( poMaskBand )
        return poMaskBand;

    if( !WriteInstr(INSTR_Band_GetMaskBand) )
        return CreateFakeMaskBand();

    if( !GDALSkipUntilEndOfJunkMarker(p) )
        return CreateFakeMaskBand();

    GDALRasterBand* poBand = NULL;
    if( !GDALPipeRead(p, (GDALClientDataset*) NULL, &poBand, abyCaps) )
        return CreateFakeMaskBand();

    GDALConsumeErrors(p);

    poMaskBand = poBand;
    return poMaskBand;
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int GDALClientRasterBand::GetMaskFlags()
{
    if( !SupportsInstr(INSTR_Band_GetMaskFlags) )
        return GDALPamRasterBand::GetMaskFlags();

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
    if( !SupportsInstr(INSTR_Band_CreateMaskBand) )
        return GDALPamRasterBand::CreateMaskBand(nFlags);

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
    if( !SupportsInstr(INSTR_Band_Fill) )
        return GDALPamRasterBand::Fill(dfRealValue, dfImaginaryValue);

    InvalidateCachedLines();

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
    if( !SupportsInstr(INSTR_Band_BuildOverviews) )
        return GDALPamRasterBand::BuildOverviews(pszResampling, nOverviews, panOverviewList,
                                                 pfnProgress, pProgressData);

    InvalidateCachedLines();

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

GDALRasterAttributeTable *GDALClientRasterBand::GetDefaultRAT()
{
    if( !SupportsInstr(INSTR_Band_GetDefaultRAT) )
        return GDALPamRasterBand::GetDefaultRAT();

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
    if( !SupportsInstr(INSTR_Band_SetDefaultRAT) )
        return GDALPamRasterBand::SetDefaultRAT(poRAT);

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
    if( !SupportsInstr(INSTR_Band_AdviseRead) )
        return GDALPamRasterBand::AdviseRead(nXOff, nYOff, nXSize, nYSize,
                                             nBufXSize, nBufYSize,
                                             eDT, papszOptions);

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

    char* pszCWD = CPLGetCurrentDir();

    if( !GDALPipeWrite(p, INSTR_Open) ||
        !GDALPipeWrite(p, eAccess) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, pszCWD))
    {
        CPLFree(pszCWD);
        return FALSE;
    }
    CPLFree(pszCWD);
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

    if( !GDALPipeRead(p, sizeof(abyCaps), abyCaps) )
        return FALSE;

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
                                              nBlockXSize, nBlockYSize,
                                              abyCaps);
        }
        else
        {
            if( !GDALPipeRead(p, this, &poBand, abyCaps) )
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

static int IsSeparateExecutable()
{
#ifdef WIN32
    return TRUE;
#else
    const char* pszSpawnServer = CPLGetConfigOption("GDAL_API_PROXY_SERVER", "NO");
    if( EQUAL(pszSpawnServer, "NO") || EQUAL(pszSpawnServer, "OFF") ||
        EQUAL(pszSpawnServer, "FALSE")  || EQUAL(pszSpawnServer, "0") )
        return FALSE;
    else
        return TRUE;
#endif
}

const char* GDALClientDatasetGetFilename(const char* pszFilename)
{
    const char* pszSpawn;
    if( EQUALN(pszFilename, "API_PROXY:", strlen("API_PROXY:")) )
    {
        pszFilename += strlen("API_PROXY:");
        pszSpawn = "YES";
    }
    else
    {
        pszSpawn = CPLGetConfigOption("GDAL_API_PROXY", "NO");
        if( EQUAL(pszSpawn, "NO") || EQUAL(pszSpawn, "OFF") ||
            EQUAL(pszSpawn, "FALSE") || EQUAL(pszSpawn, "0") )
        {
            return NULL;
        }
    }

    /* Those datasets cannot work in a multi-process context */
     /* /vsistdin/ and /vsistdout/ can work on Unix in the fork() only context (i.e. GDAL_API_PROXY_SERVER undefined) */
     /* since the forked process will inherit the same descriptors as the parent */

    if( EQUALN(pszFilename, "MEM:::", 6) ||
        strstr(pszFilename, "/vsimem/") != NULL ||
        strstr(pszFilename, "/vsimem\\") != NULL ||
        (strstr(pszFilename, "/vsistdout/") != NULL && IsSeparateExecutable()) ||
        (strstr(pszFilename, "/vsistdin/") != NULL && IsSeparateExecutable()) ||
        EQUALN(pszFilename,"NUMPY:::",8) )
        return NULL;

    if( !(EQUAL(pszSpawn, "YES") || EQUAL(pszSpawn, "ON") ||
          EQUAL(pszSpawn, "TRUE") || EQUAL(pszSpawn, "1")) )
    {
        CPLString osExt(CPLGetExtension(pszFilename));

        /* If the file extension is listed in the GDAL_API_PROXY, then */
        /* we have a match */
        char** papszTokens = CSLTokenizeString2( pszSpawn, " ,", CSLT_HONOURSTRINGS );
        if( CSLFindString(papszTokens, osExt) >= 0 )
        {
            CSLDestroy(papszTokens);
            return pszFilename;
        }

        /* Otherwise let's suppose that driver names are listed in GDAL_API_PROXY */
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

    char* pszCWD = CPLGetCurrentDir();

    GDALPipe* p = ssp->p;
    if( !GDALPipeWrite(p, INSTR_Identify) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, pszCWD) ||
        !GDALSkipUntilEndOfJunkMarker(p) )
    {
        GDALServerSpawnAsyncFinish(ssp);
        CPLFree(pszCWD);
        return FALSE;
    }

    CPLFree(pszCWD);

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
/*                     GDALClientDatasetQuietDelete()                   */
/************************************************************************/

static int GDALClientDatasetQuietDelete(GDALPipe* p,
                                    const char* pszFilename)
{
    char* pszCWD = CPLGetCurrentDir();
    if( !GDALPipeWrite(p, INSTR_QuietDelete) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, pszCWD) ||
        !GDALSkipUntilEndOfJunkMarker(p) )
    {
        CPLFree(pszCWD);
        return FALSE;
    }
    CPLFree(pszCWD);
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
    /*if( !SupportsInstr(INSTR_CreateCopy) )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "CreateCopy() not supported by server");
        return FALSE;
    }*/

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
    GDALPipeWriteConfigOption(p, "GDAL_PDF_WRITE_GEOREF_ON_IMAGE", bRecycleChild);
    GDALPipeWriteConfigOption(p, "GDAL_PDF_OGC_BP_WRITE_WKT", bRecycleChild);

    char* pszCWD = CPLGetCurrentDir();

    if( !GDALPipeWrite(p, INSTR_CreateCopy) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, poSrcDS->GetDescription()) ||
        !GDALPipeWrite(p, pszCWD) ||
        !GDALPipeWrite(p, bStrict) ||
        !GDALPipeWrite(p, papszOptions) )
    {
        CPLFree(pszCWD);
        return FALSE;
    }
    CPLFree(pszCWD);

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
    /*if( !SupportsInstr(INSTR_Create) )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Create() not supported by server");
        return FALSE;
    }*/

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

    char* pszCWD = CPLGetCurrentDir();

    if( !GDALPipeWrite(p, INSTR_Create) ||
        !GDALPipeWrite(p, pszFilename) ||
        !GDALPipeWrite(p, pszCWD) ||
        !GDALPipeWrite(p, nXSize) ||
        !GDALPipeWrite(p, nYSize) ||
        !GDALPipeWrite(p, nBands) ||
        !GDALPipeWrite(p, eType) ||
        !GDALPipeWrite(p, papszOptions) )
    {
        CPLFree(pszCWD);
        return FALSE;
    }
    CPLFree(pszCWD);
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
/*                              Delete()                                */
/************************************************************************/

CPLErr GDALClientDataset::Delete( const char * pszFilename )
{
    pszFilename =
        GDALClientDatasetGetFilename(pszFilename);
    if( pszFilename == NULL )
        return CE_Failure;

    CLIENT_ENTER();

    GDALServerSpawnedProcess* ssp = GDALServerSpawnAsync();
    if( ssp == NULL )
        return CE_Failure;

    GDALPipe* p = ssp->p;
    if( !GDALClientDatasetQuietDelete(p, pszFilename) )
    {
        GDALServerSpawnAsyncFinish(ssp);
        return CE_Failure;
    }

    GDALServerSpawnAsyncFinish(ssp);
    return CE_None;
}

/************************************************************************/
/*                      GDALUnloadAPIPROXYDriver()                      */
/************************************************************************/
static GDALDriver* poAPIPROXYDriver = NULL;

static void GDALUnloadAPIPROXYDriver(GDALDriver* poDriver)
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
    poAPIPROXYDriver = NULL; 
}

/************************************************************************/
/*                       GDALGetAPIPROXYDriver()                        */
/************************************************************************/

GDALDriver* GDALGetAPIPROXYDriver()
{
    CPLMutexHolderD(GDALGetphDMMutex());
    if( poAPIPROXYDriver == NULL )
    {
#ifdef DEBUG
        CPLAssert(INSTR_END + 1 == sizeof(apszInstr) / sizeof(apszInstr[0]));
#endif
        /* If asserted, change GDAL_CLIENT_SERVER_PROTOCOL_MAJOR / GDAL_CLIENT_SERVER_PROTOCOL_MINOR */
        CPLAssert(INSTR_END + 1 == 80);

        const char* pszConnPool = CPLGetConfigOption("GDAL_API_PROXY_CONN_POOL", "YES");
        if( atoi(pszConnPool) > 0 )
        {
            bRecycleChild = TRUE;
            nMaxRecycled = MIN(atoi(pszConnPool), MAX_RECYCLED);
        }
        else if( CSLTestBoolean(pszConnPool) )
        {
            bRecycleChild = TRUE;
            nMaxRecycled = DEFAULT_RECYCLED;
        }
        memset(aspRecycled, 0, sizeof(aspRecycled));

        poAPIPROXYDriver = new GDALDriver();

        poAPIPROXYDriver->SetDescription( "API_PROXY" );
        poAPIPROXYDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "API_PROXY" );

        poAPIPROXYDriver->pfnOpen = GDALClientDataset::Open;
        poAPIPROXYDriver->pfnIdentify = GDALClientDataset::Identify;
        poAPIPROXYDriver->pfnCreateCopy = GDALClientDataset::CreateCopy;
        poAPIPROXYDriver->pfnCreate = GDALClientDataset::Create;
        poAPIPROXYDriver->pfnDelete = GDALClientDataset::Delete;
        poAPIPROXYDriver->pfnUnloadDriver = GDALUnloadAPIPROXYDriver;
    }
    return poAPIPROXYDriver;
}
