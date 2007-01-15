/************************************************************************/
/*				NUMPYDataset				*/
/************************************************************************/
#ifndef GDAL_NUMPY_ARRAY_H
#define GDAL_NUMPY_ARRAY_H


#include "gdal_priv.h"
#include "Python.h"
#include "../../frmts/mem/memdataset.h"
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
#endif