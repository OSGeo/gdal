/************************************************************************/
/*				NUMPYDataset				*/
/************************************************************************/
#ifndef GDAL_NUMPY_ARRAY_H
#define GDAL_NUMPY_ARRAY_H


#include "gdal_priv.h"


CPL_C_START

GDALRasterBandH CPL_DLL MEMCreateRasterBand( GDALDataset *, int, GByte *,
                                             GDALDataType, int, int, int );
CPL_C_END


#ifdef _DEBUG
#undef _DEBUG
#include "Python.h"
#define _DEBUG
#else
#include "Python.h"
#endif
#include "numpy/arrayobject.h"

class NUMPYDataset : public GDALDataset
{
    PyArrayObject *psArray;

    double	  adfGeoTransform[6];
    char	  *pszProjection;

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

  public:
                 NUMPYDataset();
                 ~NUMPYDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    static GDALDataset *Open( GDALOpenInfo * );
};

PyObject* GDALRegister_NUMPY(void);
#endif /* GDAL_NUMPY_ARRAY_H */

