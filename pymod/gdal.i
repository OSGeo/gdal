/******************************************************************************
 * $Id$
 *
 * Name:     gdal.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.33  2001/10/19 16:05:31  warmerda
 * fixed several bugs in SetMetadata impl
 *
 * Revision 1.32  2001/10/19 15:43:52  warmerda
 * added SetGCPs, and SetMetadata support
 *
 * Revision 1.31  2001/10/10 20:47:49  warmerda
 * added some OSR methods
 *
 * Revision 1.30  2001/07/18 04:44:17  warmerda
 * added CPL_CVSID
 *
 * Revision 1.29  2001/05/07 14:50:44  warmerda
 * added python access to GDALComputeRasterMinMax
 *
 * Revision 1.28  2001/03/15 03:20:03  warmerda
 * fixed return type for OGRErr to be in
 *
 * Revision 1.27  2001/01/22 22:34:06  warmerda
 * added median cut, and dithering algorithms
 *
 * Revision 1.26  2000/12/14 17:38:49  warmerda
 * added GDALDriver.Delete
 *
 * Revision 1.25  2000/11/29 16:58:59  warmerda
 * fixed MajorObject handling
 *
 * Revision 1.24  2000/11/17 17:17:04  warmerda
 * added importfromESRI, and preliminary (nonworking) modern swig support
 *
 * Revision 1.23  2000/10/30 21:25:41  warmerda
 * added access to CPL error functions
 *
 * Revision 1.22  2000/10/30 14:15:15  warmerda
 * Fixed nodata function name.
 *
 * Revision 1.21  2000/10/20 04:20:59  warmerda
 * added SetStatePlane
 *
 * Revision 1.20  2000/10/06 15:31:34  warmerda
 * added nodata support
 *
 * Revision 1.19  2000/08/30 20:06:14  warmerda
 * added projection method list functions
 */

%module _gdal

%{
#include "gdal.h"
#include "../alg/gdal_alg.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"
#include "gdal_py.h"

CPL_CVSID("$Id$");

#ifdef SWIGTYPE_GDALDatasetH
#  define SWIG_GetPtr_2(s,d,t)  SWIG_ConvertPtr( s, d,(SWIGTYPE##t),1)
#else
#  define SWIG_GetPtr_2(s,d,t)  SWIG_GetPtr( s, d, #t)
#endif
%}

%native(NumPyArrayToGDALFilename) py_NumPyArrayToGDALFilename;

void CPLDebug( const char *, const char * );
void CPLErrorReset();
int CPLGetLastErrorNo();
const char *CPLGetLastErrorMsg();

typedef int GDALDataType;
typedef int GDALAccess;
typedef int GDALRWFlag;
typedef int GDALColorInterp;
typedef int GDALPaletteInterp;

int  GDALGetDataTypeSize( GDALDataType );
const char * GDALGetDataTypeName( GDALDataType );

/*! Translate a GDALColorInterp into a user displayable string. */
const char *GDALGetColorInterpretationName( GDALColorInterp );

/*! Translate a GDALPaletteInterp into a user displayable string. */
const char *GDALGetPaletteInterpretationName( GDALPaletteInterp );

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

typedef void *GDALMajorObjectH;
typedef void *GDALDatasetH;
typedef void *GDALRasterBandH;
typedef void *GDALDriverH;
typedef void *GDALProjDefH;
typedef void *GDALColorTableH;

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

void GDALAllRegister( void );
void GDALRegister_NUMPY( void );

GDALDatasetH  GDALOpen( const char *, GDALAccess );

GDALDriverH  GDALGetDriverByName( const char * );
int          GDALGetDriverCount();
GDALDriverH  GDALGetDriver( int );
int          GDALRegisterDriver( GDALDriverH );
void         GDALDeregisterDriver( GDALDriverH );
int	     GDALDeleteDataset( GDALDriverH, const char * );

const char  *GDALGetDriverShortName( GDALDriverH );
const char  *GDALGetDriverLongName( GDALDriverH );
const char  *GDALGetDriverHelpTopic( GDALDriverH );

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

GDALDriverH  GDALGetDatasetDriver( GDALDatasetH );
void    GDALClose( GDALDatasetH );
int 	GDALGetRasterXSize( GDALDatasetH );
int 	GDALGetRasterYSize( GDALDatasetH );
int 	GDALGetRasterCount( GDALDatasetH );
GDALRasterBandH  GDALGetRasterBand( GDALDatasetH, int );
const char  *GDALGetProjectionRef( GDALDatasetH );
int      GDALSetProjection( GDALDatasetH, const char * );
int      GDALReferenceDataset( GDALDatasetH );
int      GDALDereferenceDataset( GDALDatasetH );
int      GDALGetGCPCount( GDALDatasetH );
const char *GDALGetGCPProjection( GDALDatasetH );

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDALDataType  GDALGetRasterDataType( GDALRasterBandH );
void 	GDALGetBlockSize( GDALRasterBandH,
	                  int * pnXSize, int * pnYSize );

int  GDALGetRasterBandXSize( GDALRasterBandH );
int  GDALGetRasterBandYSize( GDALRasterBandH );

GDALColorInterp  GDALGetRasterColorInterpretation( GDALRasterBandH );
GDALColorTableH  GDALGetRasterColorTable( GDALRasterBandH );
int              GDALSetRasterColorTable( GDALRasterBandH, GDALColorTableH );

int              GDALSetRasterNoDataValue( GDALRasterBandH, double );

/* category names missing ... needs special binding */

int              GDALGetOverviewCount( GDALRasterBandH );
GDALRasterBandH  GDALGetOverview( GDALRasterBandH, int );
int              GDALFlushRasterCache( GDALRasterBandH );


/* ==================================================================== */
/*      Color tables.                                                   */
/* ==================================================================== */
typedef struct
{
    short      c1;      /* gray, red, cyan or hue */
    short      c2;      /* green, magenta, or lightness */
    short      c3;      /* blue, yellow, or saturation */
    short      c4;      /* alpha or blackband */
} GDALColorEntry;

GDALColorTableH  GDALCreateColorTable( GDALPaletteInterp );
void             GDALDestroyColorTable( GDALColorTableH );
GDALColorTableH  GDALCloneColorTable( GDALColorTableH );
GDALPaletteInterp  GDALGetPaletteInterpretation( GDALColorTableH );
int              GDALGetColorEntryCount( GDALColorTableH );
const GDALColorEntry  *GDALGetColorEntry( GDALColorTableH, int );
int  GDALGetColorEntryAsRGB( GDALColorTableH, int, GDALColorEntry *);
void  GDALSetColorEntry( GDALColorTableH, int, const GDALColorEntry * );

/* ==================================================================== */
/*      Projections                                                     */
/* ==================================================================== */

GDALProjDefH  GDALCreateProjDef( const char * );
int    	 GDALReprojectToLongLat( GDALProjDefH, double *, double * );
int    	 GDALReprojectFromLongLat( GDALProjDefH, double *, double * );
void     GDALDestroyProjDef( GDALProjDefH );
const char *GDALDecToDMS( double, const char *, int );

/* ==================================================================== */
/*      GDAL Cache Management                                           */
/* ==================================================================== */

void  GDALSetCacheMax( int nBytes );
int   GDALGetCacheMax();
int   GDALGetCacheUsed();
int   GDALFlushCacheBlock();

/* ==================================================================== */
/*	Support function for progress callbacks to python.              */
/* ==================================================================== */

%{

typedef struct {
    PyObject *psPyCallback;
    PyObject *psPyCallbackData;
    int nLastReported;
} PyProgressData;

/************************************************************************/
/*                          PyProgressProxy()                           */
/************************************************************************/

int PyProgressProxy( double dfComplete, const char *pszMessage, void *pData )

{
    PyProgressData *psInfo = (PyProgressData *) pData;
    PyObject *psArgs, *psResult;
    int      bContinue = TRUE;

    if( psInfo->nLastReported == (int) (100.0 * dfComplete) )
        return TRUE;

    if( psInfo->psPyCallback == NULL || psInfo->psPyCallback == Py_None )
        return TRUE;

    psInfo->nLastReported = (int) 100.0 * dfComplete;
    
    if( pszMessage == NULL )
        pszMessage = "";

    if( psInfo->psPyCallbackData == NULL )
        psArgs = Py_BuildValue("(dsO)", dfComplete, pszMessage, Py_None );
    else
        psArgs = Py_BuildValue("(dsO)", dfComplete, pszMessage, 
	                       psInfo->psPyCallbackData );

    psResult = PyEval_CallObject( psInfo->psPyCallback, psArgs);
    Py_XDECREF(psArgs);

    if( psResult == NULL )
    {
        return TRUE;
    }

    if( psResult == Py_None )
    {
	Py_XDECREF(Py_None);
        return TRUE;
    }

    if( !PyArg_Parse( psResult, "i", &bContinue ) )
    {
        PyErr_SetString(PyExc_ValueError, "bad progress return value");
	return FALSE;
    }

    Py_XDECREF(psResult);

    return bContinue;    
}

%}

/* ==================================================================== */
/*      Special custom functions.                                       */
/* ==================================================================== */

%{
/************************************************************************/
/*                           GDALBuildOverviews()                       */
/************************************************************************/
static PyObject *
py_GDALBuildOverviews(PyObject *self, PyObject *args) {

    char *pszSwigDS = NULL;
    GDALDatasetH hDS = NULL;   
    char *pszResampling = "NEAREST";
    PyObject *psPyOverviewList = NULL, *psPyBandList = NULL;
    int   nOverviews, *panOverviewList, i;
    int    eErr;
    PyProgressData sProgressInfo;

    self = self;
    sProgressInfo.psPyCallback = NULL;
    sProgressInfo.psPyCallbackData = NULL;
    if(!PyArg_ParseTuple(args,"ssO!O!|OO:GDALBuildOverviews",	
			 &pszSwigDS, &pszResampling, 
		         &PyList_Type, &psPyOverviewList, 
			 &PyList_Type, &psPyBandList,
                         &(sProgressInfo.psPyCallback), 
		         &(sProgressInfo.psPyCallbackData) ) )
        return NULL;

    if (SWIG_GetPtr_2(pszSwigDS,(void **) &hDS,_GDALDatasetH)) {
        PyErr_SetString(PyExc_TypeError,
	   	        "Type error in argument 1 of GDALBuildOverviews."
			" Expected _GDALDatasetH.");
        return NULL;
    }

    nOverviews = PyList_Size(psPyOverviewList);
    panOverviewList = (int *) CPLCalloc(sizeof(int),nOverviews);
    for( i = 0; i < nOverviews; i++ )
    {
	if( !PyArg_Parse( PyList_GET_ITEM(psPyOverviewList,i), "i", 
			  panOverviewList+i) )
        {
	    PyErr_SetString(PyExc_ValueError, "bad overview value");
	    return NULL;
        }
    }

    eErr = GDALBuildOverviews( hDS, pszResampling, nOverviews, panOverviewList,
			       0, NULL, PyProgressProxy, &sProgressInfo );

    CPLFree( panOverviewList );

    return Py_BuildValue( "i", eErr );
}

%}

%native(GDALBuildOverviews) py_GDALBuildOverviews;

%{
/************************************************************************/
/*                           GDALCreateCopy()                           */
/************************************************************************/
static PyObject *
py_GDALCreateCopy(PyObject *self, PyObject *args) {

    PyObject *poPyOptions=NULL;
    char *pszSwigDriver=NULL, *pszFilename=NULL, *pszSwigSourceDS=NULL;
    int  bStrict = FALSE;
    GDALDriverH hDriver = NULL;
    GDALDatasetH hSourceDS = NULL, hTargetDS = NULL;   
    char **papszOptions = NULL;
    PyProgressData sProgressInfo;

    self = self;
    sProgressInfo.psPyCallback = NULL;
    sProgressInfo.psPyCallbackData = NULL;
    if(!PyArg_ParseTuple(args,"sss|iO!OO:GDALCreateCopy",	
			 &pszSwigDriver, &pszFilename, &pszSwigSourceDS, 
			 &bStrict, &PyList_Type, &poPyOptions,
			 &(sProgressInfo.psPyCallback), 
		         &(sProgressInfo.psPyCallbackData)) )
        return NULL;

    if (SWIG_GetPtr_2(pszSwigDriver,(void **) &hDriver,_GDALDriverH)) {
        PyErr_SetString(PyExc_TypeError,
	   	        "Type error in argument 1 of GDALCreateCopy."
			" Expected _GDALDriverH.");
        return NULL;
    }
	
    if (SWIG_GetPtr_2(pszSwigSourceDS,(void **) &hSourceDS, _GDALDatasetH )) {
        PyErr_SetString(PyExc_TypeError,
	   	        "Type error in argument 3 of GDALCreateCopy."
			" Expected _GDALDatasetH.");
        return NULL;
    }

    if( poPyOptions != NULL )
    {
        int i;

	for( i = 0; i < PyList_Size(poPyOptions); i++ )
        {
            char *pszItem = NULL;

	    if( !PyArg_Parse(PyList_GET_ITEM(poPyOptions,i), "s", 
			     &pszItem) )
            {
	        PyErr_SetString(PyExc_ValueError, "bad option list item");
	        return NULL;
            }
            papszOptions = CSLAddString( papszOptions, pszItem );
        }
    }

    hTargetDS = GDALCreateCopy( hDriver, pszFilename, hSourceDS, bStrict, 
			        papszOptions, PyProgressProxy, &sProgressInfo);
	
    CSLDestroy( papszOptions );

    if( hTargetDS == NULL )
    {
        Py_INCREF(Py_None);
	return Py_None;
    }
    else
    {
        char  szSwigTarget[48];

#ifdef SWIGTYPE_GDALDatasetH
	SWIG_MakePtr( szSwigTarget, hTargetDS, SWIGTYPE_GDALDatasetH );	
#else
	SWIG_MakePtr( szSwigTarget, hTargetDS, "_GDALDatasetH" );	
#endif
	return Py_BuildValue( "s", szSwigTarget );
    }
}

%}

%native(GDALCreateCopy) py_GDALCreateCopy;

%{
/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/
static PyObject *
py_GDALCreate(PyObject *self, PyObject *args) {

    PyObject *poPyOptions=NULL;
    char *pszSwigDriver=NULL, *pszFilename=NULL;
    int  nXSize, nYSize, nBands, nDataType;
    GDALDriverH hDriver = NULL;
    GDALDatasetH hTargetDS = NULL;   
    char **papszOptions = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"ssiiii|O:GDALCreate",	
			 &pszSwigDriver, &pszFilename, 
			 &nXSize, &nYSize, &nBands, &nDataType,
			 &PyList_Type, &poPyOptions ))
        return NULL;

    if (SWIG_GetPtr_2(pszSwigDriver,(void **) &hDriver, _GDALDriverH )) {
        PyErr_SetString(PyExc_TypeError,
	   	        "Type error in argument 1 of GDALCreate."
			" Expected _GDALDriverH.");
        return NULL;
    }
	
    if( poPyOptions != NULL )
    {
        int i;

	for( i = 0; i < PyList_Size(poPyOptions); i++ )
        {
            char *pszItem = NULL;

	    if( !PyArg_Parse(PyList_GET_ITEM(poPyOptions,i), "s", 
			     &pszItem) )
            {
	        PyErr_SetString(PyExc_ValueError, "bad option list item");
	        return NULL;
            }
            papszOptions = CSLAddString( papszOptions, pszItem );
        }
    }

    hTargetDS = GDALCreate( hDriver, pszFilename, nXSize, nYSize, nBands, 
			    nDataType, papszOptions );
	
    CSLDestroy( papszOptions );

    if( hTargetDS == NULL )
    {
        Py_INCREF(Py_None);
	return Py_None;
    }
    else
    {
        char  szSwigTarget[48];

#ifdef SWIGTYPE_GDALDatasetH
	SWIG_MakePtr( szSwigTarget, hTargetDS, SWIGTYPE_GDALDatasetH );	
#else
	SWIG_MakePtr( szSwigTarget, hTargetDS, "_GDALDatasetH" );	
#endif
	return Py_BuildValue( "s", szSwigTarget );
    }
}

%}

%native(GDALCreate) py_GDALCreate;

%{
/************************************************************************/
/*                         GDALReadRaster()                             */
/************************************************************************/
static PyObject *
py_GDALReadRaster(PyObject *self, PyObject *args) {

    PyObject *result;
    GDALRasterBandH  _arg0;
    char *_argc0 = NULL;
    int  _arg1;
    int  _arg2;
    int  _arg3;
    int  _arg4;
    int  _arg5;
    int  _arg6;
    GDALDataType  _arg7;
    char *result_string;
    int  result_size;

    self = self;
    if(!PyArg_ParseTuple(args,"siiiiiii:GDALReadRaster",
                         &_argc0,&_arg1,&_arg2,&_arg3,&_arg4,
                         &_arg5,&_arg6,&_arg7)) 
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALRasterBandH )) {
            PyErr_SetString(PyExc_TypeError,
			    "Type error in argument 1 of GDALReadRaster."
			    " Expected _GDALRasterBandH.");
            return NULL;
        }
    }
	
    result_size = _arg5 * _arg6 * (GDALGetDataTypeSize(_arg7)/8);
    result_string = (char *) malloc(result_size+1);

    if( GDALRasterIO(_arg0, GF_Read, _arg1, _arg2, _arg3, _arg4, 
		     (void *) result_string, 
		     _arg5, _arg6, _arg7, 0, 0 ) != CE_None )
    {
	free( result_string );
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }

    result = PyString_FromStringAndSize(result_string,result_size);

    free( result_string );

    return result;
}

%}

%native(GDALReadRaster) py_GDALReadRaster;

%{
/************************************************************************/
/*                          GDALWriteRaster()                           */
/************************************************************************/
static PyObject *
py_GDALWriteRaster(PyObject *self, PyObject *args) {

    GDALRasterBandH  _arg0;
    char *_argc0 = NULL;
    int  _arg1;
    int  _arg2;
    int  _arg3;
    int  _arg4;
    char *strbuffer_arg = NULL;
    int  strbuffer_size;
    int  _arg5;
    int  _arg6;
    GDALDataType  _arg7;

    self = self;
    if(!PyArg_ParseTuple(args,"siiiis#iii:GDALWriteRaster",
                         &_argc0,&_arg1,&_arg2,&_arg3,&_arg4,
                         &strbuffer_arg,&strbuffer_size,&_arg5,&_arg6,&_arg7)) 
        return NULL;



    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALRasterBandH)) {
            PyErr_SetString(PyExc_TypeError,
			    "Type error in argument 1 of GDALWriteRaster."
			    " Expected _GDALRasterBandH.");
            return NULL;
        }
    }
	
    if( GDALRasterIO(_arg0, GF_Write, _arg1, _arg2, _arg3, _arg4, 
		     (void *) strbuffer_arg,
		     _arg5, _arg6, _arg7, 0, 0 ) != CE_None )
    {
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

%}

%native(GDALWriteRaster) py_GDALWriteRaster;


%{
/************************************************************************/
/*                            GDALGetGCPs()                             */
/************************************************************************/
static PyObject *
py_GDALGetGCPs(PyObject *self, PyObject *args) {

    GDALDatasetH  _arg0;
    char *_argc0 = NULL;
    const GDAL_GCP * pasGCPList;
    PyObject *psList;
    int iGCP;

    self = self;
    if(!PyArg_ParseTuple(args,"s:GDALGetGCPs",&_argc0))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALDatasetH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of GDALGetGCPs."
                            "  Expected _GDALDatasetH.");
            return NULL;
        }
    }

    pasGCPList = GDALGetGCPs( _arg0 );	

    psList = PyList_New(GDALGetGCPCount(_arg0));
    for( iGCP = 0; pasGCPList != NULL && iGCP < GDALGetGCPCount(_arg0); iGCP++)
    {
	PyList_SetItem(psList, iGCP, 
                       Py_BuildValue("(ssddddd)", 
                                     pasGCPList[iGCP].pszId,
                                     pasGCPList[iGCP].pszInfo,
                                     pasGCPList[iGCP].dfGCPPixel,
                                     pasGCPList[iGCP].dfGCPLine,
                                     pasGCPList[iGCP].dfGCPX,
                                     pasGCPList[iGCP].dfGCPY,
                                     pasGCPList[iGCP].dfGCPZ ) );
    }

    return psList;
}
%}

%native(GDALGetGCPs) py_GDALGetGCPs;

%{
/************************************************************************/
/*                            GDALSetGCPs()                             */
/************************************************************************/
static PyObject *
py_GDALSetGCPs(PyObject *self, PyObject *args) {

    GDALDatasetH  _arg0;
    char *_argc0 = NULL;
    GDAL_GCP * pasGCPList;
    char * pszProjection = "";
    PyObject *psList;
    int iGCP, nGCPCount;
    CPLErr eErr;

    self = self;
    if(!PyArg_ParseTuple(args,"sO!s:GDALGetGCPs",
			&_argc0, &PyList_Type, &psList, &pszProjection))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALDatasetH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of GDALSetGCPs."
                            "  Expected _GDALDatasetH.");
            return NULL;
        }
    }

    nGCPCount = PyList_Size(psList);
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPCount);
    GDALInitGCPs( nGCPCount, pasGCPList );

    for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        char *pszId = NULL, *pszInfo = NULL;

	if( !PyArg_Parse( PyList_GET_ITEM(psList,iGCP), "(ssddddd)", 
	                  &pszId, &pszInfo, 
	                  &(pasGCPList[iGCP].dfGCPPixel),
	                  &(pasGCPList[iGCP].dfGCPLine),
	                  &(pasGCPList[iGCP].dfGCPX),
	                  &(pasGCPList[iGCP].dfGCPY),
	                  &(pasGCPList[iGCP].dfGCPZ) ) )
        {
	    PyErr_SetString(PyExc_ValueError, "improper GCP tuple");
	    return NULL;
        }

        CPLFree( pasGCPList[iGCP].pszId );
	pasGCPList[iGCP].pszId = CPLStrdup(pszId);
        CPLFree( pasGCPList[iGCP].pszInfo );
	pasGCPList[iGCP].pszInfo = CPLStrdup(pszInfo);
    }

    eErr = GDALSetGCPs( _arg0, nGCPCount, pasGCPList, pszProjection );
	
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );    

    if( eErr != CE_None )
    {	
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(GDALSetGCPs) py_GDALSetGCPs;

%{
/************************************************************************/
/*                        GDALGetGeoTransform()                         */
/************************************************************************/
static PyObject *
py_GDALGetGeoTransform(PyObject *self, PyObject *args) {

    GDALDatasetH  _arg0;
    char *_argc0 = NULL;
    double geotransform[6];

    self = self;
    if(!PyArg_ParseTuple(args,"s:GDALGetGeoTransform",&_argc0))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALDatasetH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of GDALGetGeoTransform."
                            "  Expected _GDALDatasetH.");
            return NULL;
        }
    }
	
    GDALGetGeoTransform(_arg0,geotransform);

    return Py_BuildValue("dddddd",
	                 geotransform[0],
	                 geotransform[1],
	                 geotransform[2],
	                 geotransform[3],
	                 geotransform[4],
	                 geotransform[5] );
}
%}

%native(GDALGetGeoTransform) py_GDALGetGeoTransform;

%{
/************************************************************************/
/*                        GDALSetGeoTransform()                         */
/************************************************************************/
static PyObject *
py_GDALSetGeoTransform(PyObject *self, PyObject *args) {

    GDALDatasetH  _arg0;
    char *_argc0 = NULL;
    double geotransform[6];
    int    err;

    self = self;
    if(!PyArg_ParseTuple(args,"s(dddddd):GDALSetGeoTransform",&_argc0,
	geotransform+0, geotransform+1, geotransform+2, 
	geotransform+3, geotransform+4, geotransform+5) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALDatasetH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of GDALSetGeoTransform."
                            "  Expected _GDALDatasetH.");
            return NULL;
        }
    }
	
    err = GDALSetGeoTransform(_arg0,geotransform);

    if( err != CE_None )
    {
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }
    
    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(GDALSetGeoTransform) py_GDALSetGeoTransform;

%{
/************************************************************************/
/*                       GDALGetRasterHistogram()                       */
/************************************************************************/
static PyObject *
py_GDALGetRasterHistogram(PyObject *self, PyObject *args) {

    GDALRasterBandH  hBand;
    char *_argc0 = NULL;
    double dfMin = -0.5, dfMax = 255.5;
    int nBuckets = 256, bIncludeOutOfRange = FALSE, bApproxOK = FALSE;
    int *panHistogram, i;
    PyObject *psList;

    self = self;
    if(!PyArg_ParseTuple(args,"s|ddiii:GDALGetRasterHistogram",&_argc0,
			 &dfMin, &dfMax, &nBuckets, &bIncludeOutOfRange, 
	 	         &bApproxOK))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &hBand,_GDALRasterBandH)) {
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALGetRasterHistogram."
                          "  Expected _GDALRasterBandH.");
            return NULL;
        }
    }
	
    panHistogram = (int *) CPLCalloc(sizeof(int),nBuckets);
    GDALGetRasterHistogram(hBand, dfMin, dfMax, nBuckets, panHistogram, 
			   bIncludeOutOfRange, bApproxOK, 
		           GDALDummyProgress, NULL);

    psList = PyList_New(nBuckets);
    for( i = 0; i < nBuckets; i++ )
	PyList_SetItem(psList, i, Py_BuildValue("i", panHistogram[i] ));

    CPLFree( panHistogram );

    return psList;
}
%}

%native(GDALGetRasterHistogram) py_GDALGetRasterHistogram;

%{
/************************************************************************/
/*                        GDALComputeRasterMinMax()                     */
/************************************************************************/
static PyObject *
py_GDALComputeRasterMinMax(PyObject *self, PyObject *args) {

    GDALRasterBandH  hBand;
    char *_argc0 = NULL;
    int bApproxOK = 0;
    double adfMinMax[2];

    self = self;
    if(!PyArg_ParseTuple(args,"s|i:GDALGetRasterMinMax",&_argc0,&bApproxOK))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &hBand,_GDALRasterBandH)) {
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALGetRasterMinMax."
                          "  Expected _GDALRasterBandH.");
            return NULL;
        }
    }

    GDALComputeRasterMinMax( hBand, bApproxOK, adfMinMax );

    return Py_BuildValue("dd", adfMinMax[0], adfMinMax[1] );
}
%}

%native(GDALComputeRasterMinMax) py_GDALComputeRasterMinMax;

%{
/************************************************************************/
/*                       GDALGetMetadata()                              */
/************************************************************************/
static PyObject *
py_GDALGetMetadata(PyObject *self, PyObject *args) {

    GDALMajorObjectH  hObject;
    char *_argc0 = NULL;
    PyObject *psDict;
    int i;
    char **papszMetadata;
    char *pszDomain = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"s|s:GDALGetMetadata",&_argc0, &pszDomain))
        return NULL;

    if (_argc0) {
#ifdef SWIGTYPE_GDALDatasetH
        if (SWIG_ConvertPtr(_argc0,(void **) &hObject,NULL,0) ) 
#else
        if (SWIG_GetPtr(_argc0,(void **) &hObject,NULL )) 
#endif
	{
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALGetMetadata."
                          "  Expected _GDALMajorObjectH.");
            return NULL;
        }
    }

    psDict = PyDict_New();
    
    papszMetadata = GDALGetMetadata( hObject, pszDomain );

    for( i = 0; i < CSLCount(papszMetadata); i++ )
    {
	char	*pszKey;
	const char *pszValue;

	pszValue = CPLParseNameValue( papszMetadata[i], &pszKey );
	if( pszValue != NULL )
	    PyDict_SetItem( psDict, 
                            Py_BuildValue("s", pszKey ), 
                            Py_BuildValue("s", pszValue ) );
	CPLFree( pszKey );
    }

    return psDict;
}
%}

%native(GDALGetMetadata) py_GDALGetMetadata;

%{
/************************************************************************/
/*                          GDALSetMetadata()                           */
/************************************************************************/
static PyObject *
py_GDALSetMetadata(PyObject *self, PyObject *args) {

    GDALMajorObjectH  hObject;
    char *_argc0 = NULL;
    PyObject *psDict;
    char **papszMetadata = NULL;
    char *pszDomain = NULL;
    int nPos = 0;
    PyObject *psKey, *psValue;
    CPLErr eErr;

    self = self;
    if(!PyArg_ParseTuple(args,"sO!|s:GDALSetMetadata",&_argc0, 
			 &PyDict_Type, &psDict, &pszDomain))
        return NULL;

    if (_argc0) {
#ifdef SWIGTYPE_GDALDatasetH
        if (SWIG_ConvertPtr(_argc0,(void **) &hObject,NULL,0) ) 
#else
        if (SWIG_GetPtr(_argc0,(void **) &hObject,NULL )) 
#endif
	{
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALSetMetadata."
                          "  Expected _GDALMajorObjectH.");
            return NULL;
        }
    }

    while( PyDict_Next( psDict, &nPos, &psKey, &psValue ) ) 
    {
        char *pszKey, *pszValue;
        
	if( !PyArg_Parse( psKey, "s", &pszKey )
	    || !PyArg_Parse( psValue, "s", &pszValue ) )
        {
	    PyErr_SetString(PyExc_TypeError,
                    "Metadata dictionary keys and values must be strings.");
            return NULL;
        }

	printf( "Set %s=%s\n", pszKey, pszValue );
        papszMetadata = CSLSetNameValue( papszMetadata, pszKey, pszValue );
    }

    eErr = GDALSetMetadata( hObject, papszMetadata, pszDomain );

    CSLDestroy( papszMetadata );

    if( eErr != CE_None )
    {
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }
    
    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(GDALSetMetadata) py_GDALSetMetadata;

%{
/************************************************************************/
/*                         GDALGetDescription()                         */
/************************************************************************/
static PyObject *
py_GDALGetDescription(PyObject *self, PyObject *args) {

    GDALMajorObjectH  hObject;
    char *_argc0 = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"s:GDALGetDescription",&_argc0))
        return NULL;

    if (_argc0) {
#ifdef SWIGTYPE_GDALDatasetH
        if (SWIG_ConvertPtr(_argc0,(void **) &hObject,NULL,0) ) 
#else
        if (SWIG_GetPtr(_argc0,(void **) &hObject,NULL )) 
#endif
	{
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALGetDescription."
                          "  Expected _GDALMajorObjectH.");
            return NULL;
        }
    }

    return Py_BuildValue("s", GDALGetDescription(hObject) );
}
%}

%native(GDALGetDescription) py_GDALGetDescription;

%{
/************************************************************************/
/*                         GDALGetRasterNoDataValue()                   */
/************************************************************************/
static PyObject *
py_GDALGetRasterNoDataValue(PyObject *self, PyObject *args) {

    GDALRasterBandH  hObject;
    char *_argc0 = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"s:GDALGetNoDataValue",&_argc0))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &hObject,_GDALRasterBandH)) {
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALGetNoDataValue."
                          "  Expected _GDALRasterBandH.");
            return NULL;
        }
    }

    return Py_BuildValue("d", GDALGetRasterNoDataValue(hObject,NULL) );
}
%}

%native(GDALGetRasterNoDataValue) py_GDALGetRasterNoDataValue;

/* -------------------------------------------------------------------- */
/*      Algorithms                                                      */
/* -------------------------------------------------------------------- */

%{
/************************************************************************/
/*                           GDALBuildOverviews()                       */
/************************************************************************/
static PyObject *
py_GDALComputeMedianCutPCT(PyObject *self, PyObject *args) {

    char *pszSwigRed, *pszSwigGreen, *pszSwigBlue;
    char *pszSwigCT = NULL;
    GDALRasterBandH   hRed, hGreen, hBlue;
    GDALColorTableH   hColorTable = NULL;
    int               nColors = 256;
    int               eErr;
    PyProgressData sProgressInfo;

    self = self;
    sProgressInfo.psPyCallback = NULL;
    sProgressInfo.psPyCallbackData = NULL;
    if(!PyArg_ParseTuple(args,"sssis|OO:GDALComputeMedianCutPCT",	
			 &pszSwigRed, &pszSwigGreen, &pszSwigBlue,
			 &nColors, &pszSwigCT,
                         &(sProgressInfo.psPyCallback), 
		         &(sProgressInfo.psPyCallbackData) ) )
        return NULL;

    if (SWIG_GetPtr_2(pszSwigRed,(void **) &hRed,_GDALRasterBandH)
	|| SWIG_GetPtr_2(pszSwigGreen,(void **) &hGreen,_GDALRasterBandH)
	|| SWIG_GetPtr_2(pszSwigBlue,(void **) &hBlue,_GDALRasterBandH))
    {
        PyErr_SetString(PyExc_TypeError,
   	      "Type error with raster band in GDALComputeMedianCutPCT."
	      " Expected _GDALRasterBandH." );
        return NULL;
    }

    if (SWIG_GetPtr_2(pszSwigCT,(void **) &hColorTable,_GDALColorTableH))
    {
        PyErr_SetString(PyExc_TypeError,
   	      "Type error with argument 5 in GDALComputeMedianCutPCT."
	      " Expected _GDALColorTableH." );
        return NULL;
    }

    eErr = GDALComputeMedianCutPCT( hRed, hGreen, hBlue, NULL,
	                            nColors, hColorTable, 
	                            PyProgressProxy, &sProgressInfo );

    return Py_BuildValue( "i", eErr );
}

%}

%native(GDALComputeMedianCutPCT) py_GDALComputeMedianCutPCT;

%{
/************************************************************************/
/*                         GDALDitherRGB2PCT()                          */
/************************************************************************/
static PyObject *
py_GDALDitherRGB2PCT(PyObject *self, PyObject *args) {

    char *pszSwigRed, *pszSwigGreen, *pszSwigBlue, *pszSwigTarget;
    char *pszSwigCT = NULL;
    GDALRasterBandH   hRed, hGreen, hBlue, hTarget;
    GDALColorTableH   hColorTable = NULL;
    int               eErr;
    PyProgressData sProgressInfo;

    self = self;
    sProgressInfo.psPyCallback = NULL;
    sProgressInfo.psPyCallbackData = NULL;
    if(!PyArg_ParseTuple(args,"sssss|OO:GDALDitherRGB2PCT",	
			 &pszSwigRed, &pszSwigGreen, &pszSwigBlue,
	                 &pszSwigTarget,
			 &pszSwigCT,
                         &(sProgressInfo.psPyCallback), 
		         &(sProgressInfo.psPyCallbackData) ) )
        return NULL;

    if (SWIG_GetPtr_2(pszSwigRed,(void **) &hRed,_GDALRasterBandH)
	|| SWIG_GetPtr_2(pszSwigGreen,(void **) &hGreen,_GDALRasterBandH)
	|| SWIG_GetPtr_2(pszSwigBlue,(void **) &hBlue,_GDALRasterBandH)
	|| SWIG_GetPtr_2(pszSwigTarget,(void **) &hTarget,_GDALRasterBandH))
    {
        PyErr_SetString(PyExc_TypeError,
   	      "Type error with raster band in GDALDitherRGB2PCT."
	      " Expected _GDALRasterBandH." );
        return NULL;
    }

    if (SWIG_GetPtr_2(pszSwigCT,(void **) &hColorTable,_GDALColorTableH))
    {
        PyErr_SetString(PyExc_TypeError,
   	      "Type error with argument 5 in GDALDitherRGB2PCT."
	      " Expected _GDALColorTableH." );
        return NULL;
    }

    eErr = GDALDitherRGB2PCT( hRed, hGreen, hBlue, hTarget, 
		              hColorTable, 	
	                      PyProgressProxy, &sProgressInfo );

    return Py_BuildValue( "i", eErr );
}

%}

%native(GDALDitherRGB2PCT) py_GDALDitherRGB2PCT;

/* -------------------------------------------------------------------- */
/*      OGRSpatialReference stuff.                                      */
/* -------------------------------------------------------------------- */
typedef void *OGRSpatialReferenceH;                               
typedef void *OGRCoordinateTransformationH;

OGRSpatialReferenceH OSRNewSpatialReference( const char * /* = NULL */);
void    OSRDestroySpatialReference( OGRSpatialReferenceH );

int     OSRReference( OGRSpatialReferenceH );
int     OSRDereference( OGRSpatialReferenceH );

int     OSRImportFromEPSG( OGRSpatialReferenceH, int );
OGRSpatialReferenceH OSRCloneGeogCS( OGRSpatialReferenceH );

int     OSRMorphToESRI( OGRSpatialReferenceH );
int     OSRMorphFromESRI( OGRSpatialReferenceH );
int     OSRValidate( OGRSpatialReferenceH );

int     OSRSetAttrValue( OGRSpatialReferenceH hSRS, const char * pszNodePath,
                         const char * pszNewNodeValue );
const char *OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                             const char * pszName, int iChild /* = 0 */ );

int     OSRSetLinearUnits( OGRSpatialReferenceH, const char *, double );
double  OSRGetLinearUnits( OGRSpatialReferenceH, char ** );

int     OSRIsGeographic( OGRSpatialReferenceH );
int     OSRIsProjected( OGRSpatialReferenceH );
int     OSRIsSameGeogCS( OGRSpatialReferenceH, OGRSpatialReferenceH );
int     OSRIsSame( OGRSpatialReferenceH, OGRSpatialReferenceH );

int     OSRSetProjCS( OGRSpatialReferenceH, const char * );
int     OSRSetWellKnownGeogCS( OGRSpatialReferenceH, const char * );

int     OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                      const char * pszGeogName,
                      const char * pszDatumName,
                      const char * pszEllipsoidName,
                      double dfSemiMajor, double dfInvFlattening,
                      const char * pszPMName /* = NULL */,
                      double dfPMOffset /* = 0.0 */,
                      const char * pszUnits /* = NULL */,
                      double dfConvertToRadians /* = 0.0 */ );

double  OSRGetSemiMajor( OGRSpatialReferenceH, int    * /* = NULL */ );
double  OSRGetSemiMinor( OGRSpatialReferenceH, int    * /* = NULL */ );
double  OSRGetInvFlattening( OGRSpatialReferenceH, int    * /* = NULL */ );

int     OSRSetAuthority( OGRSpatialReferenceH hSRS,
                         const char * pszTargetKey,
                         const char * pszAuthority,
                         int nCode );
int     OSRSetProjParm( OGRSpatialReferenceH, const char *, double );
double  OSRGetProjParm( OGRSpatialReferenceH hSRS,
                        const char * pszParmName, 
                        double dfDefault /* = 0.0 */,
                        int    * /* = NULL */ );

int     OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth );
int     OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth );
int     OSRSetStatePlane( OGRSpatialReferenceH hSRS, int nZone, int bNAD83 );

%{
/************************************************************************/
/*                          OSRImportFromESRI()                         */
/************************************************************************/
static PyObject *
py_OSRImportFromESRI(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    OGRErr err;
    PyObject *py_prj = NULL;
    char **prj = NULL;
    int    i;

    self = self;
    if(!PyArg_ParseTuple(args,"sO!:OSRImportFromESRI",
	&_argc0, &PyList_Type, &py_prj) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRImportFromESRI."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    for( i = 0; i < PyList_Size(py_prj); i++ )
    {
        char      *line = NULL;
        if( !PyArg_Parse( PyList_GET_ITEM(py_prj,i), "s", &line ) )
        {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 2 of OSRImportFromESRI."
                            "  Expected list of strings.");
            return NULL;
        }
        prj = CSLAddString( prj, line );
    }

    err = OSRImportFromESRI( _arg0, prj );
    CSLDestroy( prj );

    return Py_BuildValue( "i", err );
}
%}

%native(OSRImportFromESRI) py_OSRImportFromESRI;

%{
/************************************************************************/
/*                          OSRImportFromWkt()                          */
/************************************************************************/
static PyObject *
py_OSRImportFromWkt(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *wkt;
    OGRErr err;

    self = self;
    if(!PyArg_ParseTuple(args,"ss:OSRImportFromWkt",&_argc0,&wkt) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRImportFromWkt."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRImportFromWkt( _arg0, &wkt );

    return Py_BuildValue( "i", err );
}
%}

%native(OSRImportFromWkt) py_OSRImportFromWkt;

%{
/************************************************************************/
/*                          OSRExportToProj4()                          */
/************************************************************************/
static PyObject *
py_OSRExportToProj4(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *wkt = NULL;
    OGRErr err;
    PyObject *ret;

    self = self;
    if(!PyArg_ParseTuple(args,"s:OSRExportToProj4",&_argc0) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRExportToProj4."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRExportToProj4( _arg0, &wkt );
    if( wkt == NULL )
	wkt = "";

    ret = Py_BuildValue( "s", wkt );
    OGRFree( wkt );
    return ret;
}
%}

%native(OSRExportToProj4) py_OSRExportToProj4;

%{
/************************************************************************/
/*                           OSRExportToWkt()                           */
/************************************************************************/
static PyObject *
py_OSRExportToWkt(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *wkt = NULL;
    OGRErr err;
    PyObject *ret;

    self = self;
    if(!PyArg_ParseTuple(args,"s:OSRExportToWkt",&_argc0) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRExportToWkt."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRExportToWkt( _arg0, &wkt );
    if( wkt == NULL )
	wkt = "";

    ret = Py_BuildValue( "s", wkt );
    OGRFree( wkt );
    return ret;
}
%}

%native(OSRExportToWkt) py_OSRExportToWkt;

%{
/************************************************************************/
/*                        OSRExportToPrettyWkt()                        */
/************************************************************************/
static PyObject *
py_OSRExportToPrettyWkt(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *wkt = NULL;
    int  bSimplify = FALSE;
    OGRErr err;
    PyObject *ret;

    self = self;
    if(!PyArg_ParseTuple(args,"s|i:OSRExportToPrettyWkt",&_argc0, &bSimplify) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRExportToWkt."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRExportToPrettyWkt( _arg0, &wkt, bSimplify );
    if( wkt == NULL )
	wkt = "";

    ret = Py_BuildValue( "s", wkt );
    OGRFree( wkt );
    return ret;
}
%}

%native(OSRExportToPrettyWkt) py_OSRExportToPrettyWkt;

/* -------------------------------------------------------------------- */
/*      OGRCoordinateTransform C API.                                   */
/* -------------------------------------------------------------------- */
OGRCoordinateTransformationH
OCTNewCoordinateTransformation( OGRSpatialReferenceH hSourceSRS,
                                OGRSpatialReferenceH hTargetSRS );
void OCTDestroyCoordinateTransformation( OGRCoordinateTransformationH );

%{
/************************************************************************/
/*                             OCTTransform                             */
/************************************************************************/
static PyObject *
py_OCTTransform(PyObject *self, PyObject *args) {

    OGRCoordinateTransformationH _arg0;
    PyObject *pnts;
    char *_argc0 = NULL;
    double *x, *y, *z;
    int    pnt_count, i;

    self = self;
    if(!PyArg_ParseTuple(args,"sO!:OCTTransform",&_argc0, &PyList_Type, &pnts))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,
			  (void **) &_arg0,_OGRCoordinateTransformationH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OCTTransform."
                            "  Expected _OGRCoordinateTransformationH.");
            return NULL;
        }
    }

    pnt_count = PyList_Size(pnts);
    x = (double *) CPLCalloc(sizeof(double),pnt_count);
    y = (double *) CPLCalloc(sizeof(double),pnt_count);
    z = (double *) CPLCalloc(sizeof(double),pnt_count);
    for( i = 0; i < pnt_count; i++ )
    {
	if( !PyArg_ParseTuple(PyList_GET_ITEM(pnts,i), "dd|d", x+i, y+i, z+i) )
        {
	    CPLFree( x );
	    CPLFree( y );
	    CPLFree( z );
	    return NULL;
        }
    }  	

    /* perform the transformation */
    if( !OCTTransform( _arg0, pnt_count, x, y, z ) )
    {
        CPLFree( x );
        CPLFree( y );
        CPLFree( z );

        PyErr_SetString(PyExc_TypeError,"OCTTransform failed.");
	return NULL;
    }

    /* make a new points list */
    pnts = PyList_New(pnt_count);
    for( i = 0; i < pnt_count; i++ )
    {
	PyList_SetItem(pnts, i, Py_BuildValue("(ddd)", x[i], y[i], z[i] ) );
    }

    CPLFree( x );
    CPLFree( y );
    CPLFree( z );

    return pnts;
}
%}

%native(OCTTransform) py_OCTTransform;

%{
/************************************************************************/
/*                        OPTGetProjectionMethods()                     */
/************************************************************************/
static PyObject *
py_OPTGetProjectionMethods(PyObject *self, PyObject *args) {

    PyObject *py_MList;
    char     **papszMethods;
    int      iMethod;
    
    self = self;
    args = args;

    papszMethods = OPTGetProjectionMethods();
    py_MList = PyList_New(CSLCount(papszMethods));

    for( iMethod = 0; papszMethods[iMethod] != NULL; iMethod++ )
    {
	char    *pszUserMethodName;
	char    **papszParameters;
	PyObject *py_PList;
	int       iParam;

	papszParameters = OPTGetParameterList( papszMethods[iMethod], 
					       &pszUserMethodName );
        if( papszParameters == NULL )
            return NULL;

	py_PList = PyList_New(CSLCount(papszParameters));
	for( iParam = 0; papszParameters[iParam] != NULL; iParam++ )
       	{
	    char    *pszType;
	    char    *pszUserParamName;
            double  dfDefault;

	    OPTGetParameterInfo( papszMethods[iMethod], 
				 papszParameters[iParam], 
				 &pszUserParamName, 
				 &pszType, &dfDefault );
	    PyList_SetItem(py_PList, iParam, 
			   Py_BuildValue("(sssd)", 
					 papszParameters[iParam], 
					 pszUserParamName, 
                                         pszType, dfDefault ));
	}
	
	CSLDestroy( papszParameters );

	PyList_SetItem(py_MList, iMethod, 
		       Py_BuildValue("(ssO)", 
		                     papszMethods[iMethod], 
				     pszUserMethodName, 
		                     py_PList));
    }

    CSLDestroy( papszMethods );

    return py_MList;
}
%}

%native(OPTGetProjectionMethods) py_OPTGetProjectionMethods;


