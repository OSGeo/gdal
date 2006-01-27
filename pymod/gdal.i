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
 * Revision 1.123  2006/01/27 16:16:53  fwarmerdam
 * use internal OGRGetDriverByName
 *
 * Revision 1.122  2005/10/25 20:00:53  fwarmerdam
 * driver tracking on datasource now in core
 *
 * Revision 1.121  2005/08/05 20:29:22  fwarmerdam
 * pass nbytes in OGR_G_CreateFromWkb()
 *
 * Revision 1.120  2005/08/04 19:42:08  fwarmerdam
 * make value nullable in Set/Get ConfigOption
 *
 * Revision 1.119  2005/07/20 01:46:51  fwarmerdam
 * ogr coordinate dimension upgrades
 *
 * Revision 1.118  2005/06/10 21:51:03  fwarmerdam
 * fixed up osr.Clone() support
 *
 * Revision 1.117  2005/05/23 07:29:06  fwarmerdam
 * added some histogram related stuff
 *
 * Revision 1.116  2005/05/06 17:35:02  fwarmerdam
 * Added GetStatistics and TestBoolean
 *
 * Revision 1.115  2005/04/05 20:42:35  fwarmerdam
 * error handler is CPL_STDCALL
 *
 * Revision 1.114  2005/04/05 19:57:58  fwarmerdam
 * added STDCALL on PyProgressProxy
 *
 * Revision 1.113  2005/03/01 17:38:10  fwarmerdam
 * fixed pszSrcWKT in GDALCreateAndReprojectImage
 *
 * Revision 1.112  2005/02/21 04:10:32  fwarmerdam
 * added SetRasterColorInterpretation
 *
 * Revision 1.111  2005/02/02 19:59:03  fwarmerdam
 * added SetNextByIndex and GetFeaturesRead
 *
 * Revision 1.110  2005/01/05 21:21:38  fwarmerdam
 * added goode homolosine
 *
 * Revision 1.109  2005/01/03 22:19:09  fwarmerdam
 * added OGRLayer::SetSpatialFilterRect()
 *
 * Revision 1.108  2004/12/17 18:48:56  fwarmerdam
 * added dataset level read/write methods
 *
 * Revision 1.107  2004/12/02 19:53:02  fwarmerdam
 * Added GDALComputeBandStats()
 * Implement generic mechanism for progress callbacks, and use for
 * ComputeBandStats, Create and CreateCopy().
 *
 * Revision 1.106  2004/11/23 20:01:54  fwarmerdam
 * fixed a small init bug with pyprogressproxy stuff
 *
 * Revision 1.105  2004/11/11 18:32:03  fwarmerdam
 * Added SetBonne support
 *
 * Revision 1.104  2004/11/01 17:25:28  fwarmerdam
 * added CPL Escape functions
 *
 * Revision 1.103  2004/10/04 17:26:01  fwarmerdam
 * Added hack for SWIGTYPENULL for SWIG 1.3.
 *
 * Revision 1.102  2004/09/17 15:07:57  fwarmerdam
 * added getArea
 *
 * Revision 1.101  2004/08/11 19:04:34  warmerda
 * added warping related support
 *
 * Revision 1.100  2004/07/30 21:09:30  warmerda
 * added AddBand()
 *
 * Revision 1.99  2004/07/10 07:08:39  warmerda
 * added new GEOS methods
 *
 * Revision 1.98  2004/07/10 05:05:23  warmerda
 * added closerings
 *
 * Revision 1.97  2004/05/21 18:38:02  warmerda
 * added GetMinimum, GetMaximum, GetOffset and GetScale methods
 *
 * Revision 1.96  2004/05/10 17:09:15  warmerda
 * added autoidentifyepsg
 *
 * Revision 1.95  2004/05/06 19:27:06  dron
 * Added OSRSetProjection() wrapper.
 *
 * Revision 1.94  2004/04/24 21:28:20  warmerda
 * Added GetLastErrorType
 *
 * Revision 1.93  2004/04/02 17:40:43  warmerda
 * added GDALGeneralCmdLineProcessor() support
 *
 * Revision 1.92  2004/03/26 17:12:31  warmerda
 * added fill wrapper
 *
 * Revision 1.91  2004/03/23 15:34:37  warmerda
 * added field index check in GetField()
 *
 * Revision 1.90  2004/03/12 16:41:21  warmerda
 * Added some new cpl level functions
 *
 * Revision 1.89  2004/02/25 09:04:33  dron
 * Added wrappers for GDALPackedDMSToDec() and GDALDecToPackedDMS().
 *
 * Revision 1.88  2004/02/25 08:13:40  dron
 * Added wrapper for OSRExportToUSGS() function.
 *
 * Revision 1.87  2004/02/22 10:25:11  dron
 * Added wrapper for OSRImportFromUSGS().
 *
 * Revision 1.86  2004/02/09 06:06:19  warmerda
 * fix warning
 *
 * Revision 1.85  2004/02/08 09:13:56  aamici
 * optimize Band.ReadAsArray performance avoiding memory copy.
 *
 * Revision 1.84  2004/02/05 17:08:57  dron
 * Added wrapper for OSRSetHOM2PNO().
 *
 * Revision 1.83  2004/02/04 21:26:11  warmerda
 * added optimization loading into numpy array
 *
 * Revision 1.82  2004/01/31 09:54:28  dron
 * Fixed projection parameters number mismatch in PCI import/export functions.
 *
 * Revision 1.81  2004/01/30 09:58:32  dron
 * Wrapper for OSRExportToPCI() function.
 *
 * Revision 1.80  2004/01/29 15:22:30  dron
 * Added wrapper for OSRImportFromPCI().
 *
 * Revision 1.79  2004/01/22 21:27:38  dron
 * Added wrapper for GDALDataTypeIsComplex() function.
 *
 * Revision 1.78  2004/01/18 16:52:39  dron
 * Added wrapper for GDALGetDataTypeByName().
 *
 * Revision 1.77  2003/12/05 18:01:08  warmerda
 * Added ptrptr functions
 *
 * Revision 1.76  2003/12/02 16:39:04  warmerda
 * added GDALGetColorEntry support
 *
 * Revision 1.75  2003/10/09 15:27:52  warmerda
 * added OGRLayer::DeleteFeature() support
 *
 * Revision 1.74  2003/09/22 05:48:50  warmerda
 * added GML geometry support
 *
 * Revision 1.73  2003/08/27 15:40:05  warmerda
 * added OGRSetGenerate_DB2_V72_BYTE_ORDER()
 *
 * Revision 1.72  2003/06/23 14:07:30  warmerda
 * fixed typedef int int problem
 *
 * Revision 1.71  2003/06/21 23:26:27  warmerda
 * added TOWGS84 and PROJ.4 calls, OGRERR to int
 *
 * Revision 1.70  2003/06/19 17:13:28  warmerda
 * fixes for a few prototypes
 *
 * Revision 1.69  2003/06/18 18:39:14  warmerda
 * added OSRSetprojection functions
 *
 * Revision 1.68  2003/06/10 09:26:55  dron
 * Added SetAngularUnits() and GetAngularUnits().
 *
 * Revision 1.67  2003/05/30 15:38:59  warmerda
 * updated to use SetStatePlaneWithUnits
 *
 * Revision 1.66  2003/05/28 16:23:49  warmerda
 * added GDALTermProgress
 *
 * Revision 1.65  2003/04/22 17:28:41  warmerda
 * avoid use of assert()
 *
 * Revision 1.64  2003/04/08 22:13:00  warmerda
 * added new entry poins, and listtostringlist support
 *
 * Revision 1.63  2003/04/03 19:27:55  warmerda
 * added nullable string support, fixed ogr.Layer.SetAttributeFilter()
 *
 * Revision 1.62  2003/04/02 22:16:43  warmerda
 * Convert 'rootless' XML documents with a pseduo-root
 *
 * Revision 1.61  2003/03/25 05:58:37  warmerda
 * add better pointer and stringlist support
 *
 * Revision 1.60  2003/03/21 22:23:27  warmerda
 * added xml support
 *
 * Revision 1.59  2003/03/20 17:53:30  warmerda
 * added OGR OpenShared and reference coutnting stuff
 *
 * Revision 1.58  2003/03/18 06:05:12  warmerda
 * Added GDALDataset::FlushCache()
 *
 * Revision 1.57  2003/03/07 16:31:12  warmerda
 * added GetLayerByName
 *
 * Revision 1.56  2003/03/03 05:15:42  warmerda
 * added DeleteLayer and DeleteDataSource methods
 *
 * Revision 1.55  2003/03/02 17:13:35  warmerda
 * Fixed CPLPopErrorHandler.
 *
 * Revision 1.54  2003/03/02 17:11:27  warmerda
 * added error handling support
 *
 * Revision 1.53  2003/02/25 04:57:37  warmerda
 * added CopyGeogCSFrom()
 *
 * Revision 1.52  2003/02/06 04:50:57  warmerda
 * added the Fixup() method on OGRSpatialReference
 *
 * Revision 1.51  2003/01/08 18:17:24  warmerda
 * implemented a few new functions including custom CreateLayer
 *
 * Revision 1.50  2003/01/02 21:41:07  warmerda
 * added GetField(), and BuildPolygonFromEdges methods
 *
 * Revision 1.49  2002/12/18 18:33:01  warmerda
 * changed SetGCPs error to ValueError from TypeError
 *
 * Revision 1.48  2002/11/30 20:53:50  warmerda
 * added SetFromUserInput
 *
 * Revision 1.47  2002/11/30 17:52:18  warmerda
 * removed debugging statement
 *
 * Revision 1.46  2002/11/25 16:11:39  warmerda
 * added GetAuthorityCode/Name
 *
 * Revision 1.45  2002/11/04 21:15:15  warmerda
 * improved geometry creation error reporting
 *
 * Revision 1.44  2002/10/24 20:29:14  warmerda
 * fixed CreateFromWkb
 *
 * Revision 1.43  2002/10/24 16:51:17  warmerda
 * added lots of OGRGeometryH support
 *
 * Revision 1.42  2002/09/26 18:14:22  warmerda
 * added preliminary OGR support
 *
 * Revision 1.41  2002/09/11 14:30:06  warmerda
 * added GDALMajorObject.SetDescription()
 *
 * Revision 1.40  2002/08/15 15:34:58  warmerda
 * fixed problem with options passing in py_GDALCreate (bug 180)
 *
 * Revision 1.39  2002/07/18 15:33:51  warmerda
 * fixed last fix
 *
 * Revision 1.38  2002/07/18 15:27:15  warmerda
 * made CPLDebug() safe
 *
 * Revision 1.37  2002/06/27 15:41:49  warmerda
 * added minixml read/write stuff
 *
 * Revision 1.36  2002/05/28 18:52:23  warmerda
 * added GDALOpenShared
 *
 * Revision 1.35  2002/05/10 02:58:58  warmerda
 * added GDALGCPsToGeoTransform
 *
 * Revision 1.34  2001/11/17 21:18:26  warmerda
 * removed GDALProjDefH
 *
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
#include "cpl_minixml.h"
#include "ogr_api.h"
#include "gdalwarper.h"

CPL_CVSID("$Id$");

/*
** this seems to be required for SWIG 1.3 because of my hacky SWIG_GetPtr_2 
** macro
*/

#ifdef SWIGTYPE_GDALDatasetH
#define SWIGTYPENULL NULL
#endif

#ifdef SWIGTYPE_GDALDatasetH
#  define SWIG_GetPtr_2(s,d,t)  SWIG_ConvertPtr( s, d,(SWIGTYPE##t),1)
#else
#  define SWIG_GetPtr_2(s,d,t)  SWIG_GetPtr( s, d, #t)
#endif

typedef char **stringList;
typedef char *NULLableString;

%}

typedef void *NULLableString;

%native(NumPyArrayToGDALFilename) py_NumPyArrayToGDALFilename;

%include pointer.i

/* -------------------------------------------------------------------- */
/*      Special wrappers mapping Python Dict and List types to/from     */
/*      string lists.                                                   */
/* -------------------------------------------------------------------- */
typedef char **stringList;

%{
static PyObject *
py_DictToStringList(PyObject *self, PyObject *args) {

    PyObject *psDict;
    char **papszMetadata = NULL;
    int nPos = 0;
    PyObject *psKey, *psValue;
    char  szSwigTarget[48];

    self = self;
    if(!PyArg_ParseTuple(args,"O!:DictToStringList",
			 &PyDict_Type, &psDict))
        return NULL;

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

        papszMetadata = CSLSetNameValue( papszMetadata, pszKey, pszValue );
    }

    SWIG_MakePtr( szSwigTarget, papszMetadata, "_stringList" );	

    return Py_BuildValue( "s", szSwigTarget );
}
%}

%native(DictToStringList) py_DictToStringList;

%{
static PyObject *
py_StringListToDict(PyObject *self, PyObject *args) {

    PyObject *psDict;
    char **papszMetadata = NULL;
    int i;
    char  *pszSwigStringList = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"s:StringListToDict", &pszSwigStringList) )
        return NULL;

    if (SWIG_GetPtr_2(pszSwigStringList,(void **) &papszMetadata,_stringList) )
    {
        PyErr_SetString(PyExc_TypeError,
   	      "Type error with stringlist.  Expected _stringList." );
        return NULL;
    }

    psDict = PyDict_New();
    
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

%native(StringListToDict) py_StringListToDict;

%{
static PyObject *
py_ListToStringList(PyObject *self, PyObject *args) {

    PyObject *psList;
    char **papszStringList = NULL;
    char  szSwigTarget[48];
    int   iEntry;

    self = self;
    if(!PyArg_ParseTuple(args,"O!:ListToStringList",
			 &PyList_Type, &psList))
        return NULL;

    for( iEntry=0; iEntry < PyList_Size( psList ); iEntry++ )
    {
	char *pszItem = NULL;
        
	if( !PyArg_Parse( PyList_GET_ITEM(psList,iEntry), "s", &pszItem ) )
        {
	    PyErr_SetString(PyExc_TypeError,
	                    "String list item not a string.");
            return NULL;
        }

	papszStringList = CSLAddString( papszStringList, pszItem );
    }

    SWIG_MakePtr( szSwigTarget, papszStringList, "_stringList" );	

    return Py_BuildValue( "s", szSwigTarget );
}
%}

%native(ListToStringList) py_ListToStringList;

%{
static PyObject *
py_StringListToList(PyObject *self, PyObject *args) {

    PyObject *psList;
    char **papszStringList = NULL;
    int i, nCount;
    char  *pszSwigStringList = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"s:StringListToList", &pszSwigStringList) )
        return NULL;

    if (SWIG_GetPtr_2(pszSwigStringList,(void **) &papszStringList,
	              _stringList) )
    {
        PyErr_SetString(PyExc_TypeError,
   	      "Type error with stringlist.  Expected _stringList." );
        return NULL;
    }

    nCount = CSLCount(papszStringList);
    psList = PyList_New(nCount);

    for( i = 0; i < nCount; i++ )
	PyList_SetItem( psList, i, Py_BuildValue( "s", papszStringList[i] ));

    return psList;
}
%}

%native(StringListToList) py_StringListToList;

/* -------------------------------------------------------------------- */
/*      Special pointer to pointer manipulation functions.              */
/* -------------------------------------------------------------------- */
%{
static PyObject *ptrptrcreate(PyObject *self, PyObject *args) {

    char *type = "void";
    char fulltype[100];
    int  count = 1;
    char swig_ptr[100];
    void *ptrptr;

    self = self;
    if(!PyArg_ParseTuple(args,"|si:ptrptrcreate", &type, &count ) )
        return NULL;

    ptrptr = calloc(sizeof(void*),count);

    sprintf( fulltype, "_%s_pp", type );
    SWIG_MakePtr( swig_ptr, ptrptr, fulltype );

    return Py_BuildValue( "s", swig_ptr );
}
    
static PyObject *ptrptrset(PyObject *self, PyObject *args) {

    int    array_index = 0;
    char   *swig_target = NULL, *swig_value;
    void   *value = NULL;
    void   **target = NULL;
    
    self = self;
    if(!PyArg_ParseTuple(args,"ss|i:ptrptrset", 
                         &swig_target, &swig_value, &array_index))
        return NULL;

    SWIG_GetPtr_2( swig_value, &value, NULL );
    SWIG_GetPtr_2( swig_target, (void **) &target, NULL );

    if( target != NULL )
	target[array_index] = value;

    Py_INCREF(Py_None);
    return Py_None;
}
    
static PyObject *ptrptrvalue(PyObject *self, PyObject *args) {

    int    array_index = 0;
    char   *swig_target = NULL;
    void   *value = NULL;
    void   **target = NULL;
    char   result_ptr[100];
    
    self = self;
    if(!PyArg_ParseTuple(args,"s|i:ptrptrvalue", 
                         &swig_target, &array_index))
        return NULL;

    SWIG_GetPtr_2( swig_target, (void **) &target, NULL );

    if( target != NULL )
        value = target[array_index];

    if( value == NULL )
        strcpy( result_ptr, "NULL" );
    else
    {
        char type2[100], *type_off;

        type_off = strstr(swig_target+1,"_"); 
        if( type_off != NULL )
        {
	    strcpy( type2, type_off );
            if( type2[strlen(type2)-1] == 'p' )
                type2[strlen(type2)-1] = '\0'; /* trim trailing 'p' */
        }
        else
            strcpy( type2, "_void_p" );
        
        SWIG_MakePtr( result_ptr, value, type2 );
    }

    return Py_BuildValue( "s", result_ptr );
}
    
        
%}

%native(ptrptrset) ptrptrset;
%native(ptrptrvalue) ptrptrvalue;
%native(ptrptrcreate) ptrptrcreate;

/* -------------------------------------------------------------------- */
/*      CPL level stuff                                                 */
/* -------------------------------------------------------------------- */
void CPLErrorReset();
int CPLGetLastErrorNo();
int CPLGetLastErrorType();
const char *CPLGetLastErrorMsg();

void CSLDestroy(stringList);

const char *CPLGetConfigOption( const char *, NULLableString );
void        CPLSetConfigOption( const char *, NULLableString );
int         CSLTestBoolean( const char * );

void         CPLPushFinderLocation( const char * );
void         CPLPopFinderLocation();
void         CPLFinderClean();
const char  *CPLFindFile(const char *pszClass, const char *pszBasename);

char  *CPLEscapeString( const char *pszString, int nLength, int nScheme );
char *CPLUnescapeString( const char *pszString, int *pnLength, int nScheme );

/* -------------------------------------------------------------------- */
/*      General GDAL stuff.                                             */
/* -------------------------------------------------------------------- */
//#define GDALDataType int
//#define GDALAccess int
//#define GDALRWFlag int
//#define GDALColorInterp int
//#define GDALPaletteInterp int

typedef int GDALDataType;
typedef int GDALAccess;
typedef int GDALRWFlag;
typedef int GDALColorInterp;
typedef int GDALPaletteInterp;

int GDALGetDataTypeSize( GDALDataType );
int GDALDataTypeIsComplex( GDALDataType );
const char * GDALGetDataTypeName( GDALDataType );
GDALDataType GDALGetDataTypeByName( const char * );

/*! Translate a GDALColorInterp into a user displayable string. */
const char *GDALGetColorInterpretationName( GDALColorInterp );

/*! Translate a GDALPaletteInterp into a user displayable string. */
const char *GDALGetPaletteInterpretationName( GDALPaletteInterp );

const char *GDALDecToDMS( double, const char *, int );
double GDALPackedDMSToDec( double );
double GDALDecToPackedDMS( double );

int GDALTermProgress( double, const char *, void * );

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

typedef void *GDALMajorObjectH;
typedef void *GDALDatasetH;
typedef void *GDALRasterBandH;
typedef void *GDALDriverH;
typedef void *GDALColorTableH;

stringList GDALGetMetadata( GDALMajorObjectH, const char * );
int        GDALSetMetadata( GDALMajorObjectH, stringList, const char * );

const char *GDALGetDescription( GDALMajorObjectH );
void        GDALSetDescription( GDALMajorObjectH, const char * );

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

void GDALAllRegister( void );
void GDALRegister_NUMPY( void );

GDALDatasetH  GDALOpen( const char *, GDALAccess );
GDALDatasetH  GDALOpenShared( const char *, GDALAccess );

GDALDatasetH  GDALCreateCopy( GDALDriverH, const char *, GDALDatasetH,
                              int, stringList, void *, void * );
GDALDatasetH  GDALCreate( GDALDriverH hDriver,
                          const char *, int, int, int, int ,
                          stringList );

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
int     GDALAddBand( GDALDatasetH hDS, GDALDataType eType, 
                     char **papszOptions );
const char  *GDALGetProjectionRef( GDALDatasetH );
int      GDALSetProjection( GDALDatasetH, const char * );
int      GDALGetGeoTransform( GDALDatasetH, double * );
int      GDALSetGeoTransform( GDALDatasetH, double * );
int      GDALReferenceDataset( GDALDatasetH );
int      GDALDereferenceDataset( GDALDatasetH );
int      GDALGetGCPCount( GDALDatasetH );
const char *GDALGetGCPProjection( GDALDatasetH );
void     GDALFlushCache( GDALDatasetH );

int      GDALDatasetAdviseRead( GDALDatasetH hDS,
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    int nBXSize, int nBYSize, GDALDataType eBDataType,
    int nBandCount, int *panBandCount, stringList options );


/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDALDataType  GDALGetRasterDataType( GDALRasterBandH );
void 	GDALGetBlockSize( GDALRasterBandH,
	                  int * pnXSize, int * pnYSize );

int  GDALGetRasterBandXSize( GDALRasterBandH );
int  GDALGetRasterBandYSize( GDALRasterBandH );

GDALColorInterp  GDALGetRasterColorInterpretation( GDALRasterBandH );
int              GDALSetRasterColorInterpretation( GDALRasterBandH,
                                                   GDALColorInterp );	
GDALColorTableH  GDALGetRasterColorTable( GDALRasterBandH );
int              GDALSetRasterColorTable( GDALRasterBandH, GDALColorTableH );

double GDALGetRasterMinimum( GDALRasterBandH, int *pbSuccess );
double GDALGetRasterMaximum( GDALRasterBandH, int *pbSuccess );
double GDALGetRasterOffset( GDALRasterBandH, int *pbSuccess );
double GDALGetRasterScale( GDALRasterBandH, int *pbSuccess );

double GDALGetRasterNoDataValue( GDALRasterBandH, int * );
int    GDALSetRasterNoDataValue( GDALRasterBandH, double );

int    GDALGetRasterStatistics( GDALRasterBandH, int, int, 
		double *, double *, double *, double * );

void             GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK,
                 	                  double adfMinMax[2] );
/* category names missing ... needs special binding */

int              GDALGetOverviewCount( GDALRasterBandH );
GDALRasterBandH  GDALGetOverview( GDALRasterBandH, int );
int              GDALFlushRasterCache( GDALRasterBandH );
int              GDALFillRaster( GDALRasterBandH hBand, double dfRealValue,
		                 double dfImaginaryValue );

int GDALSetDefaultHistogram( GDALRasterBandH hBand, 
                             double dfMin, double dfMax, 
                             int nBuckets, int *panHistogram );
int      GDALRasterAdviseRead( GDALRasterBandH hRB, 
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    int nBXSize, int nBYSize, GDALDataType eBDataType, stringList options );

int GDALComputeBandStats( GDALRasterBandH hBand, int nSampleStep, 
                             double *pdfMean, double *pdfStdDev, 
                             void *pfnProgress, void *pProgressData );
/* ==================================================================== */
/*      Color tables.                                                   */
/* ==================================================================== */
typedef short *GDALColorEntry;

GDALColorTableH  GDALCreateColorTable( GDALPaletteInterp );
void             GDALDestroyColorTable( GDALColorTableH );
GDALColorTableH  GDALCloneColorTable( GDALColorTableH );
GDALPaletteInterp  GDALGetPaletteInterpretation( GDALColorTableH );
int              GDALGetColorEntryCount( GDALColorTableH );
const GDALColorEntry *GDALGetColorEntry( GDALColorTableH, int );
int  GDALGetColorEntryAsRGB( GDALColorTableH, int, GDALColorEntry *);
void  GDALSetColorEntry( GDALColorTableH, int, const GDALColorEntry * );

/* ==================================================================== */
/*      GDAL Cache Management                                           */
/* ==================================================================== */

void  GDALSetCacheMax( int nBytes );
int   GDALGetCacheMax();
int   GDALGetCacheUsed();
int   GDALFlushCacheBlock();

/* ==================================================================== */
/*      gdalwarper.h                                                    */
/* ==================================================================== */

int
GDALReprojectImage( GDALDatasetH hSrcDS, NULLableString pszSrcWKT, 
                    GDALDatasetH hDstDS, NULLableString pszDstWKT,
                    int eResampleAlg, double dfWarpMemoryLimit,
                    double dfMaxError,
                    void *pfnProgress, void *pProgressArg, 
                    void *psOptions );

int
GDALCreateAndReprojectImage( GDALDatasetH hSrcDS, NULLableString pszSrcWKT, 
                    NULLableString pszDstFilename, NULLableString pszDstWKT,
                    GDALDriverH hDstDriver, stringList papszCreateOptions,
                    int eResampleAlg, double dfWarpMemoryLimit,
                    double dfMaxError,
                    void *pfnProgress, void *pProgressArg, 
                    void *psOptions );

GDALDatasetH
GDALAutoCreateWarpedVRT( GDALDatasetH hSrcDS, 
                         NULLableString pszSrcWKT, NULLableString pszDstWKT, 
              		 int eResampleAlg, 
                         double dfMaxError, const void *psOptions );

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

int CPL_STDCALL
PyProgressProxy( double dfComplete, const char *pszMessage, void *pData )

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

%{
/************************************************************************/
/*                          MakeProgressInfo()                          */
/************************************************************************/
static PyObject *
py_MakeProgressInfo(PyObject *self, PyObject *args) {

    PyProgressData *psProgressInfo = NULL;
    char szSwigTarget[128], szCBPtr[128];

    self = self;

    psProgressInfo = (PyProgressData *) CPLCalloc(1,sizeof(PyProgressData));
    psProgressInfo->nLastReported = -1;
    psProgressInfo->psPyCallback = NULL;
    psProgressInfo->psPyCallbackData = NULL;

    if(!PyArg_ParseTuple(args,"OO:MakeProgressInfo",	
                         &(psProgressInfo->psPyCallback), 
		         &(psProgressInfo->psPyCallbackData) ) )
        return NULL;

    SWIG_MakePtr( szCBPtr, PyProgressProxy, "_void_p" );	
    SWIG_MakePtr( szSwigTarget, psProgressInfo, "_void_p" );	

    return Py_BuildValue( "(ss)", szCBPtr, szSwigTarget );
}
%}

%native(MakeProgressInfo) py_MakeProgressInfo;

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
    sProgressInfo.nLastReported = -1;
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
/*                         GDALReadRaster()                             */
/************************************************************************/
static PyObject *
py_GDALReadRaster(PyObject *self, PyObject *args) {

    PyObject *result = NULL;
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
    if(!PyArg_ParseTuple(args,"siiiiiii|O:GDALReadRaster",
                         &_argc0,&_arg1,&_arg2,&_arg3,&_arg4,
                         &_arg5,&_arg6,&_arg7,&result))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALRasterBandH )) {
            PyErr_SetString(PyExc_TypeError,
			    "Type error in argument 1 of GDALReadRaster."
			    " Expected _GDALRasterBandH.");
            return NULL;
        }
    }

    /* we either receive a buffer object to use or
     * allocate a suitable string object */
    if( result && result != Py_None )
    {
	if( PyObject_AsWriteBuffer(result, (void**) &result_string, &result_size) )
	{
	    PyErr_SetString(PyExc_TypeError, "No writable buffer from object");
	    return NULL;
	}
	if( result_size != _arg5 * _arg6 * (GDALGetDataTypeSize(_arg7)/8) )
	{
	    PyErr_SetString(PyExc_TypeError, "Unaligned buffer");
	    return NULL;
	}

	Py_INCREF(result);
    } else {
	result_size = _arg5 * _arg6 * (GDALGetDataTypeSize(_arg7)/8);
	result = PyString_FromStringAndSize(NULL, result_size);
	if( !result )
	    return NULL;
	result_string = PyString_AsString(result);
    }

    if( GDALRasterIO(_arg0, GF_Read, _arg1, _arg2, _arg3, _arg4, 
		     (void *) result_string, 
		     _arg5, _arg6, _arg7, 0, 0 ) != CE_None )
    {
	Py_XDECREF(result);
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }

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
/*                       GDALDatasetReadRaster()                        */
/************************************************************************/
static PyObject *
py_GDALDatasetReadRaster(PyObject *self, PyObject *args) {

    PyObject *result = NULL;
    PyObject *py_band_list;
    GDALDatasetH  _arg0;
    char *_argc0 = NULL;
    int  _arg1;
    int  _arg2;
    int  _arg3;
    int  _arg4;
    int  _arg5;
    int  _arg6;
    int  nBandCount, *panBandList = NULL, i;
    GDALDataType  _arg7;
    char *result_string;
    int  result_size;

    self = self;
    if(!PyArg_ParseTuple(args,"siiiiiiiO!|O:GDALDatasetReadRaster",
                         &_argc0,&_arg1,&_arg2,&_arg3,&_arg4,
                         &_arg5,&_arg6,&_arg7, 
			 &PyList_Type, &py_band_list, 
			 &result))
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALDatasetH )) {
            PyErr_SetString(PyExc_TypeError,
			    "Type error in argument 1 of GDALReadRaster."
			    " Expected _GDALRasterBandH.");
            return NULL;
        }
    }

    nBandCount = PyList_Size( py_band_list );
    panBandList = (int *) CPLMalloc(sizeof(int) * nBandCount);
    for( i = 0; i < nBandCount; i++ )
    {
	if( !PyArg_Parse( PyList_GET_ITEM(py_band_list,i), "i",
		          panBandList + i ) )
        {
	    PyErr_SetString(PyExc_ValueError, 
			    "failed to extra band from band list. ");
	    return NULL;
        }
    }

    /* we either receive a buffer object to use or
     * allocate a suitable string object */
    if( result && result != Py_None )
    {
	if( PyObject_AsWriteBuffer(result, (void**) &result_string, &result_size) )
	{
	    PyErr_SetString(PyExc_TypeError, "No writable buffer from object");
	    return NULL;
	}
	if( result_size != _arg5 * _arg6 * (GDALGetDataTypeSize(_arg7)/8) )
	{
	    PyErr_SetString(PyExc_TypeError, "Unaligned buffer");
	    return NULL;
	}

	Py_INCREF(result);
    } else {
	result_size = _arg5*_arg6*(GDALGetDataTypeSize(_arg7)/8)*nBandCount;
	result = PyString_FromStringAndSize(NULL, result_size);
	if( !result )
	    return NULL;
	result_string = PyString_AsString(result);
    }

    if( GDALDatasetRasterIO(_arg0, GF_Read, _arg1, _arg2, _arg3, _arg4, 
		     (void *) result_string, 
		     _arg5, _arg6, _arg7, nBandCount, panBandList, 
		     0, 0, 0 ) != CE_None )
    {
	Py_XDECREF(result);
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	result = NULL;
    }

    CPLFree( panBandList );
    return result;
}

%}

%native(GDALDatasetReadRaster) py_GDALDatasetReadRaster;

%{
/************************************************************************/
/*                       GDALDatasetWriteRaster()                       */
/************************************************************************/
static PyObject *
py_GDALDatasetWriteRaster(PyObject *self, PyObject *args) {

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
    PyObject *py_band_list = NULL;
    int nBandCount, *panBandList = NULL, i;

    self = self;
    if(!PyArg_ParseTuple(args,"siiiis#iiiO!:GDALDatasetWriteRaster",
                         &_argc0,&_arg1,&_arg2,&_arg3,&_arg4,
                         &strbuffer_arg,&strbuffer_size,&_arg5,&_arg6,&_arg7,
			 &PyList_Type, &py_band_list )) 
        return NULL;



    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_GDALDatasetH)) {
            PyErr_SetString(PyExc_TypeError,
			    "Type error in argument 1 of GDALWriteRaster."
			    " Expected _GDALRasterBandH.");
            return NULL;
        }
    }
	
    nBandCount = PyList_Size( py_band_list );
    panBandList = (int *) CPLMalloc(sizeof(int) * nBandCount);
    for( i = 0; i < nBandCount; i++ )
    {
	if( !PyArg_Parse( PyList_GET_ITEM(py_band_list,i), "i",
		          panBandList + i ) )
        {
	    PyErr_SetString(PyExc_ValueError, 
			    "failed to extra band from band list. ");
	    return NULL;
        }
    }

    if( GDALDatasetRasterIO(_arg0, GF_Write, _arg1, _arg2, _arg3, _arg4, 
		            (void *) strbuffer_arg,
		            _arg5, _arg6, _arg7, nBandCount, panBandList, 
			    0, 0, 0 ) != CE_None )
    {
        CPLFree( panBandList );
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }

    CPLFree( panBandList );

    Py_INCREF(Py_None);
    return Py_None;
}

%}

%native(GDALDatasetWriteRaster) py_GDALDatasetWriteRaster;


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
    if(!PyArg_ParseTuple(args,"sO!s:GDALSetGCPs",
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
	PyErr_SetString(PyExc_ValueError,CPLGetLastErrorMsg());
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(GDALSetGCPs) py_GDALSetGCPs;

%{
/************************************************************************/
/*                         GDALGCPsToGeoTransform()                     */
/************************************************************************/
static PyObject *
py_GDALGCPsToGeoTransform(PyObject *self, PyObject *args) {

    GDAL_GCP * pasGCPList;
    PyObject *psList;
    int iGCP, nGCPCount;
    int    bSuccess;
    int    bApproxOK = TRUE;
    double adfGeoTransform[6];

    self = self;
    if(!PyArg_ParseTuple(args,"O!i:GDALGCPsToGeoTransform",
			&PyList_Type, &psList, &bApproxOK))
        return NULL;

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
   
    bSuccess = GDALGCPsToGeoTransform( nGCPCount, pasGCPList, adfGeoTransform,
                                       bApproxOK );
	
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );    

    if( bSuccess )
    {	
        return Py_BuildValue( "dddddd", 
	                      adfGeoTransform[0],
	                      adfGeoTransform[1],
	                      adfGeoTransform[2],
	                      adfGeoTransform[3],
	                      adfGeoTransform[4],
	                      adfGeoTransform[5] );
    }
    else
    {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

%}

%native(GDALGCPsToGeoTransform) py_GDALGCPsToGeoTransform;

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
    PyProgressData sProgressInfo;

    self = self;
    sProgressInfo.nLastReported = -1;
    sProgressInfo.psPyCallback = NULL;
    sProgressInfo.psPyCallbackData = NULL;
    if(!PyArg_ParseTuple(args,"s|ddiiiOO:GDALGetRasterHistogram",&_argc0,
			 &dfMin, &dfMax, &nBuckets, &bIncludeOutOfRange, 
	 	         &bApproxOK,
                         &(sProgressInfo.psPyCallback), 
		         &(sProgressInfo.psPyCallbackData) ) )

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
			   PyProgressProxy, &sProgressInfo );

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
/*                      GDALGetDefaultHistogram()                       */
/************************************************************************/
static PyObject *
py_GDALGetDefaultHistogram(PyObject *self, PyObject *args) {

    GDALRasterBandH  hBand;
    char *_argc0 = NULL;
    int bForce = FALSE;
    int *panHistogram=NULL, i;
    PyObject *psList, *psResult;
    PyProgressData sProgressInfo;
    double dfMin, dfMax;
    int nBuckets;
    CPLErr eErr;

    self = self;
    sProgressInfo.nLastReported = -1;
    sProgressInfo.psPyCallback = NULL;
    sProgressInfo.psPyCallbackData = NULL;
    if(!PyArg_ParseTuple(args,"s|iOO:GDALGetDefaultHistogram",&_argc0,
			 &bForce,
                         &(sProgressInfo.psPyCallback), 
		         &(sProgressInfo.psPyCallbackData) ) )

        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &hBand,_GDALRasterBandH)) {
            PyErr_SetString(PyExc_TypeError,
                          "Type error in argument 1 of GDALGetRasterHistogram."
                          "  Expected _GDALRasterBandH.");
            return NULL;
        }
    }

    eErr = GDALGetDefaultHistogram(hBand, &dfMin, &dfMax, &nBuckets, 
				  &panHistogram, bForce, 
			          PyProgressProxy, &sProgressInfo );

    if( eErr == CE_Warning )
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    psList = PyList_New(nBuckets);
    for( i = 0; i < nBuckets; i++ )
	PyList_SetItem(psList, i, Py_BuildValue("i", panHistogram[i] ));

    CPLFree( panHistogram );

    psResult = Py_BuildValue( "(ddiO)", dfMin, dfMax, nBuckets, psList );
    Py_XDECREF(psList);
	
    return psResult;
}
%}

%native(GDALGetDefaultHistogram) py_GDALGetDefaultHistogram;

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
    sProgressInfo.nLastReported = -1;
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
    sProgressInfo.nLastReported = -1;
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

int     GDALChecksumImage( GDALRasterBandH, int, int, int, int );

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
int     OSRImportFromProj4( OGRSpatialReferenceH, const char *);
OGRSpatialReferenceH OSRCloneGeogCS( OGRSpatialReferenceH );
OGRSpatialReferenceH OSRClone( OGRSpatialReferenceH );

int     OSRImportFromXML( OGRSpatialReferenceH, const char * );

int     OSRMorphToESRI( OGRSpatialReferenceH );
int     OSRMorphFromESRI( OGRSpatialReferenceH );
int     OSRValidate( OGRSpatialReferenceH );
int     OSRFixupOrdering( OGRSpatialReferenceH );
int     OSRFixup( OGRSpatialReferenceH );
int     OSRStripCTParms( OGRSpatialReferenceH );

int     OSRSetAttrValue( OGRSpatialReferenceH hSRS, const char * pszNodePath,
                         const char * pszNewNodeValue );
const char *OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                             const char * pszName, int iChild /* = 0 */ );

int     OSRSetAngularUnits( OGRSpatialReferenceH, const char *, double );
double  OSRGetAngularUnits( OGRSpatialReferenceH, char ** );
int     OSRSetLinearUnits( OGRSpatialReferenceH, const char *, double );
double  OSRGetLinearUnits( OGRSpatialReferenceH, char ** );

int     OSRIsGeographic( OGRSpatialReferenceH );
int     OSRIsProjected( OGRSpatialReferenceH );
int     OSRIsSameGeogCS( OGRSpatialReferenceH, OGRSpatialReferenceH );
int     OSRIsSame( OGRSpatialReferenceH, OGRSpatialReferenceH );

int     OSRSetProjCS( OGRSpatialReferenceH, const char * );
int     OSRSetWellKnownGeogCS( OGRSpatialReferenceH, const char * );
int     OSRSetFromUserInput( OGRSpatialReferenceH, const char * );
int     OSRCopyGeogCSFrom( OGRSpatialReferenceH, OGRSpatialReferenceH );
int     OSRSetTOWGS84( OGRSpatialReferenceH hSRS, 
                              double, double, double, 
                              double, double, double, double );
int     OSRGetTOWGS84( OGRSpatialReferenceH hSRS, double *, int );

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
const char *OSRGetAuthorityCode( OGRSpatialReferenceH hSRS, 
	                         const char * pszTargetKey );
const char *OSRGetAuthorityName( OGRSpatialReferenceH hSRS, 
	                         const char * pszTargetKey );
OGRErr  OSRSetProjection( OGRSpatialReferenceH, const char * );
int     OSRSetProjParm( OGRSpatialReferenceH, const char *, double );
double  OSRGetProjParm( OGRSpatialReferenceH hSRS,
                        const char * pszParmName, double dfDefault, int * );
int     OSRSetNormProjParm( OGRSpatialReferenceH, const char *, double);
double  OSRGetNormProjParm( OGRSpatialReferenceH hSRS,
                            const char * pszParmName, double dfDefault, int *);

int     OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth );
int     OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth );
int     OSRSetStatePlaneWithUnits( OGRSpatialReferenceH hSRS, 
	                           int nZone, int bNAD83, 
                                   const char *pszOverrideUnitsName, 
                                   double dfOverrideUnits );
int     OSRAutoIdentifyEPSG( OGRSpatialReferenceH hSRS );

/** Albers Conic Equal Area */
int OSRSetACEA( OGRSpatialReferenceH hSRS, double dfStdP1, double dfStdP2,
                         double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );
    
/** Azimuthal Equidistant */
int  OSRSetAE( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Bonne */
int  OSRSetBonne( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Cylindrical Equal Area */
int OSRSetCEA( OGRSpatialReferenceH hSRS, double dfStdP1, double dfCentralMeridian,
                        double dfFalseEasting, double dfFalseNorthing );

/** Cassini-Soldner */
int OSRSetCS( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Equidistant Conic */
int OSRSetEC( OGRSpatialReferenceH hSRS, double dfStdP1, double dfStdP2,
                       double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Eckert IV */
int OSRSetEckertIV( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

/** Eckert VI */
int OSRSetEckertVI( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

/** Equirectangular */
int OSRSetEquirectangular(OGRSpatialReferenceH hSRS, 
                        double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing );

/** Gall Stereograpic */
int OSRSetGS( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                       double dfFalseEasting, double dfFalseNorthing );
    
/** Goode Homolosine */
int OSRSetGH( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
    	      double dfFalseEasting, double dfFalseNorthing );
    
/** Gnomonic */
int OSRSetGnomonic(OGRSpatialReferenceH hSRS, 
		      double dfCenterLat, double dfCenterLong,
                      double dfFalseEasting, double dfFalseNorthing );

/** Hotine Oblique Mercator  using azimuth angle */
int OSRSetHOM( OGRSpatialReferenceH hSRS,
               double dfCenterLat, double dfCenterLong,
               double dfAzimuth, double dfRectToSkew,
               double dfScale,
               double dfFalseEasting, double dfFalseNorthing );

/** Hotine Oblique Mercator using two points on centerline */
OGRErr OSRSetHOM2PNO( OGRSpatialReferenceH hSRS, double dfCenterLat,
                      double dfLat1, double dfLong1,
                      double dfLat2, double dfLong2,
                      double dfScale,
                      double dfFalseEasting, double dfFalseNorthing );

/** Krovak Oblique Conic Conformal */
int OSRSetKrovak( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                           double dfAzimuth, double dfPseudoStdParallelLat,
                           double dfScale, 
                           double dfFalseEasting, double dfFalseNorthing );

/** Lambert Azimuthal Equal-Area */
int OSRSetLAEA( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );

/** Lambert Conformal Conic */
int OSRSetLCC( OGRSpatialReferenceH hSRS, double dfStdP1, double dfStdP2,
                        double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing );

/** Lambert Conformal Conic 1SP */
int OSRSetLCC1SP( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                           double dfScale,
                           double dfFalseEasting, double dfFalseNorthing );

/** Lambert Conformal Conic (Belgium) */
int OSRSetLCCB( OGRSpatialReferenceH hSRS, double dfStdP1, double dfStdP2,
                         double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );
    
/** Miller Cylindrical */
int OSRSetMC( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Mercator */
int OSRSetMercator( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                             double dfScale, 
                             double dfFalseEasting, double dfFalseNorthing );

/** Mollweide */
int  OSRSetMollweide( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                              double dfFalseEasting, double dfFalseNorthing );

/** New Zealand Map Grid */
int OSRSetNZMG( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );

/** Oblique Stereographic */
int OSRSetOS( OGRSpatialReferenceH hSRS, double dfOriginLat, double dfCMeridian,
                       double dfScale,
                       double dfFalseEasting,double dfFalseNorthing);
    
/** Orthographic */
int OSRSetOrthographic( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                                 double dfFalseEasting,double dfFalseNorthing);

/** Polyconic */
int OSRSetPolyconic( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                              double dfFalseEasting, double dfFalseNorthing );

/** Polar Stereographic */
int OSRSetPS( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfScale,
                       double dfFalseEasting, double dfFalseNorthing);
    
/** Robinson */
int OSRSetRobinson( OGRSpatialReferenceH hSRS, double dfCenterLong, 
                             double dfFalseEasting, double dfFalseNorthing );
    
/** Sinusoidal */
int OSRSetSinusoidal( OGRSpatialReferenceH hSRS, double dfCenterLong, 
                               double dfFalseEasting, double dfFalseNorthing );
    
/** Stereographic */
int OSRSetStereographic( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                                  double dfScale,
                                 double dfFalseEasting,double dfFalseNorthing);
    
/** Swiss Oblique Cylindrical */
int OSRSetSOC( OGRSpatialReferenceH hSRS, double dfLatitudeOfOrigin, double dfCentralMeridian,
                        double dfFalseEasting, double dfFalseNorthing );
    
/** Transverse Mercator */
int OSRSetTM( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfScale,
                       double dfFalseEasting, double dfFalseNorthing );

/** Tunesia Mining Grid  */
int OSRSetTMG( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong, 
                        double dfFalseEasting, double dfFalseNorthing );

/** Transverse Mercator (South Oriented) */
int OSRSetTMSO( OGRSpatialReferenceH hSRS,
                           double dfCenterLat, double dfCenterLong,
                           double dfScale,
                           double dfFalseEasting, double dfFalseNorthing );

/** VanDerGrinten */
int OSRSetVDG( OGRSpatialReferenceH hSRS,
                          double dfCenterLong,
                          double dfFalseEasting, double dfFalseNorthing );

%{
/************************************************************************/
/*                          OSRImportFromESRI()                         */
/************************************************************************/
static PyObject *
py_OSRImportFromESRI(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    int err;
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
/*                          OSRImportFromPCI()                          */
/************************************************************************/
static PyObject *
py_OSRImportFromPCI(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char    *_argc0 = NULL;
    int     err;
    PyObject *py_parms = NULL;
    char    *proj, *units = NULL;
    double  *parms = NULL;
    int     i;

    self = self;
    if(!PyArg_ParseTuple(args,"ss|sO!:OSRImportFromPCI",
	&_argc0, &proj, &units, &PyTuple_Type, &py_parms) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRImportFromPCI."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }

    parms = CPLMalloc(17 * sizeof(double));
    if (py_parms)
    {
        for( i = 0; i < 17; i++ )
        {
            if (i < PyTuple_Size(py_parms))
            {
                if(!PyArg_Parse(PyTuple_GET_ITEM(py_parms,i), "d", &parms[i]))
                {
                    PyErr_SetString(PyExc_TypeError,
                                "Type error in argument 4 of OSRImportFromPCI."
                                "  Expected tuple of floats.");
                    return NULL;
                }
            }
            else
                parms[i] = 0.0;
        }
    }
    else
    {
        for ( i = 0; i < 17; i++ )
            parms[i] = 0.0;
    }

    err = OSRImportFromPCI( _arg0, proj, units, parms );

    if (parms)
        CPLFree(parms);
    return Py_BuildValue( "i", err );
}
%}

%native(OSRImportFromPCI) py_OSRImportFromPCI;

%{
/************************************************************************/
/*                          OSRImportFromWkt()                          */
/************************************************************************/
static PyObject *
py_OSRImportFromWkt(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *wkt;
    int err;

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
/*                          OSRImportFromUSGS()                         */
/************************************************************************/
static PyObject *
py_OSRImportFromUSGS(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char    *_argc0 = NULL;
    int     err;
    PyObject *py_parms = NULL;
    long    iProjSys, iZone, iDatum;
    double  *parms = NULL;
    int     i;

    self = self;
    if(!PyArg_ParseTuple(args,"sllO!l:OSRImportFromUSGS",
	&_argc0, &iProjSys, &iZone, &PyTuple_Type, &py_parms, &iDatum) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRImportFromUSGS."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }

    parms = CPLMalloc(15 * sizeof(double));
    if (py_parms)
    {
        for( i = 0; i < 15; i++ )
        {
            if (i < PyTuple_Size(py_parms))
            {
                if(!PyArg_Parse(PyTuple_GET_ITEM(py_parms,i), "d", &parms[i]))
                {
                    PyErr_SetString(PyExc_TypeError,
                                "Type error in argument 4 of OSRImportFromUSGS."
                                "  Expected tuple of floats.");
                    return NULL;
                }
            }
            else
                parms[i] = 0.0;
        }
    }
    else
    {
        for ( i = 0; i < 15; i++ )
            parms[i] = 0.0;
    }

    err = OSRImportFromUSGS( _arg0, iProjSys, iZone, parms, iDatum );

    if (parms)
        CPLFree(parms);
    return Py_BuildValue( "i", err );
}
%}

%native(OSRImportFromUSGS) py_OSRImportFromUSGS;

%{
/************************************************************************/
/*                          OSRExportToPCI()                            */
/************************************************************************/
static PyObject *
py_OSRExportToPCI(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *proj = NULL, *units = NULL;
    double *parms;
    int err;
    PyObject *ret;

    self = self;
    if(!PyArg_ParseTuple(args,"s:OSRExportToPCI",&_argc0) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRExportToPCI."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRExportToPCI( _arg0, &proj, &units, &parms );
    if( err != OGRERR_NONE )
    {
        PyErr_SetString(PyExc_TypeError,
                        "Failed to export given SpatialReference.");
        return NULL;
    }

    ret = Py_BuildValue( "(ss(ddddddddddddddddd))", proj, units,
                         parms[0], parms[1], parms[2], parms[3], parms[4],
                         parms[5], parms[6], parms[7], parms[8], parms[9],
                         parms[10], parms[11], parms[12], parms[13],
                         parms[14], parms[15], parms[16] );
    CPLFree( proj );
    CPLFree( units );
    CPLFree( parms );
    return ret;
}
%}

%native(OSRExportToPCI) py_OSRExportToPCI;

%{
/************************************************************************/
/*                          OSRExportToProj4()                          */
/************************************************************************/
static PyObject *
py_OSRExportToProj4(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *wkt = NULL;
    int err;
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
    int err;
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
    int err;
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

%{
/************************************************************************/
/*                          OSRExportToUSGS()                           */
/************************************************************************/
static PyObject *
py_OSRExportToUSGS(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char    *_argc0 = NULL;
    long    iProjSys, iZone, iDatum;
    double  *parms;
    int     err;
    PyObject *ret;

    self = self;
    if(!PyArg_ParseTuple(args,"s:OSRExportToUSGS",&_argc0) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRExportToUSGS."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRExportToUSGS( _arg0, &iProjSys, &iZone, &parms, &iDatum );
    if( err != OGRERR_NONE )
    {
        PyErr_SetString(PyExc_TypeError,
                        "Failed to export given SpatialReference.");
        return NULL;
    }

    ret = Py_BuildValue( "(ll(ddddddddddddddd)l)", iProjSys, iZone,
                         parms[0], parms[1], parms[2], parms[3], parms[4],
                         parms[5], parms[6], parms[7], parms[8], parms[9],
                         parms[10], parms[11], parms[12], parms[13],
                         parms[14], iDatum );
    CPLFree( parms );
    return ret;
}
%}

%native(OSRExportToUSGS) py_OSRExportToUSGS;

%{
/************************************************************************/
/*                           OSRExportToXML()                           */
/************************************************************************/
static PyObject *
py_OSRExportToXML(PyObject *self, PyObject *args) {

    OGRSpatialReferenceH _arg0;
    char *_argc0 = NULL;
    char *pszDialect = NULL;
    char *pszXML = NULL;
    int err;
    PyObject *ret;

    self = self;
    if(!PyArg_ParseTuple(args,"ss:OSRExportToXML",&_argc0, &pszDialect) )
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr_2(_argc0,(void **) &_arg0,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OSRExportToXML."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }
	
    err = OSRExportToXML( _arg0, &pszXML, pszDialect );
    if( pszXML == NULL )
	pszXML = CPLStrdup("");

    ret = Py_BuildValue( "s", pszXML );
    OGRFree( pszXML );
    return ret;
}
%}

%native(OSRExportToXML) py_OSRExportToXML;

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

%{
/************************************************************************/
/*                          XMLTreeToPyList()                           */
/************************************************************************/

static PyObject *XMLTreeToPyList( CPLXMLNode *psTree )

{
    PyObject *pyList;
    int      nChildCount = 0, iChild;
    CPLXMLNode *psChild;

    for( psChild = psTree->psChild; 
         psChild != NULL; 
         psChild = psChild->psNext )
        nChildCount++;

    pyList = PyList_New(nChildCount+2);

    PyList_SetItem( pyList, 0, Py_BuildValue( "i", (int) psTree->eType ) );
    PyList_SetItem( pyList, 1, Py_BuildValue( "s", psTree->pszValue ) );

    for( psChild = psTree->psChild, iChild = 2; 
         psChild != NULL; 
         psChild = psChild->psNext, iChild++ )
    {
        PyList_SetItem( pyList, iChild, XMLTreeToPyList( psChild ) );
    }

    return pyList; 
}

/************************************************************************/
/*                         CPLParseXMLString()                          */
/************************************************************************/
static PyObject *
py_CPLParseXMLString(PyObject *self, PyObject *args) {

    char *pszText = NULL;
    CPLXMLNode  *psXMLTree = NULL;
    PyObject    *pyList;

    self = self;
    if(!PyArg_ParseTuple(args,"s:CPLParseXMLString", &pszText ))
        return NULL;

    psXMLTree = CPLParseXMLString( pszText );
    if( psXMLTree == NULL )
    {
	PyErr_SetString(PyExc_TypeError,CPLGetLastErrorMsg());
	return NULL;
    }

    if( psXMLTree != NULL && psXMLTree->psNext != NULL )
    {
	CPLXMLNode *psFirst = psXMLTree;

	/* create a "pseudo" root if we have multiple elements */
        psXMLTree = CPLCreateXMLNode( NULL, CXT_Element, "" );
	psXMLTree->psChild = psFirst;
    }

    pyList = XMLTreeToPyList( psXMLTree );

    CPLDestroyXMLNode( psXMLTree );

    return pyList;
}
%}

%native(CPLParseXMLString) py_CPLParseXMLString;

%{
/************************************************************************/
/*                          PyListToXMLTree()                           */
/************************************************************************/

static CPLXMLNode *PyListToXMLTree( PyObject *pyList )

{
    int      nChildCount = 0, iChild, nType;
    CPLXMLNode *psThisNode;
    CPLXMLNode *psChild;
    char       *pszText = NULL;

    nChildCount = PyList_Size(pyList) - 2;
    if( nChildCount < 0 )
    {
        PyErr_SetString(PyExc_TypeError,"Error in input XMLTree." );
	return NULL;
    }

    PyArg_Parse( PyList_GET_ITEM(pyList,0), "i", &nType );
    PyArg_Parse( PyList_GET_ITEM(pyList,1), "s", &pszText );
    psThisNode = CPLCreateXMLNode( NULL, (CPLXMLNodeType) nType, pszText );

    for( iChild = 0; iChild < nChildCount; iChild++ )
    {
        psChild = PyListToXMLTree( PyList_GET_ITEM(pyList,iChild+2) );
        CPLAddXMLChild( psThisNode, psChild );
    }

    return psThisNode;
}

/************************************************************************/
/*                         CPLSerializeXMLTree()                        */
/************************************************************************/
static PyObject *
py_CPLSerializeXMLTree(PyObject *self, PyObject *args) {

    char *pszText = NULL;
    CPLXMLNode  *psXMLTree = NULL;
    PyObject    *pyList = NULL, *pyResult = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"O!:CPLSerializeXMLTree", 
			 &PyList_Type, &pyList ))
        return NULL;

    psXMLTree = PyListToXMLTree( pyList );
    if( psXMLTree == NULL )
	return NULL;

    if( psXMLTree->eType == CXT_Element 
        && psXMLTree->pszValue[0] == '\0' )
    {
	CPLXMLNode *psChild;

        /* We want to avoid including the root as an element level */
        pszText = NULL;
	for( psChild = psXMLTree->psChild; 
             psChild != NULL; psChild = psChild->psNext )
        {
            char *pszTextChunk;

            pszTextChunk = CPLSerializeXMLTree( psChild );
            if( pszText == NULL )
                pszText = pszTextChunk;
            else
            {
		pszText = (char *)
		    CPLRealloc(pszText, 
                               strlen(pszText)+strlen(pszTextChunk)+1);
                strcat( pszText, pszTextChunk );
                CPLFree( pszTextChunk );
            } 
        }
    }
    else
        pszText = CPLSerializeXMLTree( psXMLTree );

    CPLDestroyXMLNode( psXMLTree );

    pyResult = Py_BuildValue( "s", pszText );
    CPLFree( pszText );

    return pyResult;
}
%}

%native(CPLSerializeXMLTree) py_CPLSerializeXMLTree;

%{

/************************************************************************/
/*                             CPLDebug()                               */
/************************************************************************/
static PyObject *
py_CPLDebug(PyObject *self, PyObject *args) {

    char *pszText = NULL;
    char *pszMsgClass = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"ss:CPLDebug", &pszMsgClass, &pszText ))
        return NULL;

    CPLDebug( pszMsgClass, "%s", pszText );

    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(CPLDebug) py_CPLDebug;

%{

/************************************************************************/
/*                              CPLError()                              */
/************************************************************************/
static PyObject *
py_CPLError(PyObject *self, PyObject *args) {

    char *pszText = NULL;
    int  nErrClass, nErrCode;

    self = self;
    if(!PyArg_ParseTuple(args,"iis:CPLError", &nErrClass, &nErrCode, &pszText))
        return NULL;

    CPLError( nErrClass, nErrCode, "%s", pszText );

    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(CPLError) py_CPLError;

/* ==================================================================== */
/*      Support function for error reporting callbacks to python.       */
/* ==================================================================== */

%{

typedef struct _PyErrorHandlerData {
    PyObject *psPyErrorHandler;
    struct _PyErrorHandlerData *psPrevious;
} PyErrorHandlerData;

static PyErrorHandlerData *psPyHandlerStack = NULL;

/************************************************************************/
/*                        PyErrorHandlerProxy()                         */
/************************************************************************/

void CPL_STDCALL
PyErrorHandlerProxy( CPLErr eErrType, int nErrorCode, const char *pszMsg )

{
    PyObject *psArgs;
    PyObject *psResult;

    CPLAssert( psPyHandlerStack != NULL );
    if( psPyHandlerStack == NULL )
        return;

    psArgs = Py_BuildValue("(iis)", (int) eErrType, nErrorCode, pszMsg );

    psResult = PyEval_CallObject( psPyHandlerStack->psPyErrorHandler, psArgs);
    Py_XDECREF(psArgs);

    if( psResult != NULL )
    {
        Py_XDECREF( psResult );
    }
}

/************************************************************************/
/*                        CPLPushErrorHandler()                         */
/************************************************************************/
static PyObject *
py_CPLPushErrorHandler(PyObject *self, PyObject *args) {

    PyObject *psPyCallback = NULL;
    PyErrorHandlerData *psCBData = NULL;
    char *pszCallbackName = NULL;
    CPLErrorHandler pfnHandler = NULL;

    self = self;

    if(!PyArg_ParseTuple(args,"O:CPLPushErrorHandler",	&psPyCallback ) )
        return NULL;

    psCBData = (PyErrorHandlerData *) CPLCalloc(sizeof(PyErrorHandlerData),1);
    psCBData->psPrevious = psPyHandlerStack;
    psPyHandlerStack = psCBData;

    if( PyArg_Parse( psPyCallback, "s", &pszCallbackName ) )
    {
        if( EQUAL(pszCallbackName,"CPLQuietErrorHandler") )
	    pfnHandler = CPLQuietErrorHandler;
        else if( EQUAL(pszCallbackName,"CPLDefaultErrorHandler") )
	    pfnHandler = CPLDefaultErrorHandler;
        else if( EQUAL(pszCallbackName,"CPLLoggingErrorHandler") )
            pfnHandler = CPLLoggingErrorHandler;
        else
        {
	    PyErr_SetString(PyExc_ValueError,
   	            "Unsupported callback name in CPLPushErrorHandler");
            return NULL;
        }
    }
    else
    {
	PyErr_Clear();
	pfnHandler = PyErrorHandlerProxy;
        psCBData->psPyErrorHandler = psPyCallback;
        Py_INCREF( psPyCallback );
    }

    CPLPushErrorHandler( pfnHandler );

    Py_INCREF(Py_None);
    return Py_None;
}

%}

%native(CPLPushErrorHandler) py_CPLPushErrorHandler;

%{

/************************************************************************/
/*                         CPLPopErrorHandler()                         */
/************************************************************************/

static PyObject *
py_CPLPopErrorHandler(PyObject *self, PyObject *args) 
{
    PyErrorHandlerData *psCBData = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,":CPLPopErrorHandler" ) )
        return NULL;

    CPLPopErrorHandler();

    if( psPyHandlerStack != NULL )
    {								
	psCBData = psPyHandlerStack;
        psPyHandlerStack = psCBData->psPrevious;

        if( psCBData->psPyErrorHandler != NULL )
        {
            Py_XDECREF( psCBData->psPyErrorHandler );
        }
        CPLFree( psCBData );	
    }

    Py_INCREF(Py_None);
    return Py_None;
}
%}

%native(CPLPopErrorHandler) py_CPLPopErrorHandler;

/************************************************************************/
/*                    GDALGeneralCmdLineProcessor()                     */
/************************************************************************/
stringList PyGDALGeneralCmdLineProcessor( stringList papszArgs, int nOptions );

%{
char **PyGDALGeneralCmdLineProcessor( char **papszArgs, int nOptions )
{
    if( GDALGeneralCmdLineProcessor( CSLCount(papszArgs), &papszArgs, 
                                     nOptions ) <= 0 )
        return NULL;
    else
        return papszArgs;
}
%}

/* -------------------------------------------------------------------- */
/*      OGR_API Stuff.							*/
/* -------------------------------------------------------------------- */
typedef void *OGRGeometryH;

typedef void *OGRFieldDefnH;
typedef void *OGRFeatureDefnH;
typedef void *OGRFeatureH;

typedef struct
{
    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;
} OGREnvelope;

typedef int OGRwkbGeometryType;
typedef int OGRFieldType;
typedef int OGRJustification;

/************************************************************************/
/* -------------------------------------------------------------------- */
/*      Geometry related functions (ogr_geometry.h)                     */
/* -------------------------------------------------------------------- */
/************************************************************************/

/* From base OGRGeometry class */

void   OGR_G_DestroyGeometry( OGRGeometryH );
OGRGeometryH OGR_G_CreateGeometry( OGRwkbGeometryType );

int    OGR_G_GetDimension( OGRGeometryH );
int    OGR_G_GetCoordinateDimension( OGRGeometryH );
void   OGR_G_SetCoordinateDimension( OGRGeometryH, int );
OGRGeometryH OGR_G_Clone( OGRGeometryH );
void   OGR_G_GetEnvelope( OGRGeometryH, OGREnvelope * );
int    OGR_G_WkbSize( OGRGeometryH hGeom );
OGRwkbGeometryType OGR_G_GetGeometryType( OGRGeometryH );
const char *OGR_G_GetGeometryName( OGRGeometryH );
void   OGR_G_FlattenTo2D( OGRGeometryH );
void   OGR_G_CloseRings( OGRGeometryH );
char  *OGR_G_ExportToGML( OGRGeometryH );
OGRGeometryH OGR_G_CreateFromGML( const char * );

void   OGR_G_AssignSpatialReference( OGRGeometryH, OGRSpatialReferenceH );
OGRSpatialReferenceH OGR_G_GetSpatialReference( OGRGeometryH );
int OGR_G_Transform( OGRGeometryH, OGRCoordinateTransformationH );
int OGR_G_TransformTo( OGRGeometryH, OGRSpatialReferenceH );

int    OGR_G_Intersect( OGRGeometryH, OGRGeometryH );
int    OGR_G_Equal( OGRGeometryH, OGRGeometryH );
int    OGR_G_Disjoint( OGRGeometryH, OGRGeometryH );
int    OGR_G_Touches( OGRGeometryH, OGRGeometryH );
int    OGR_G_Crosses( OGRGeometryH, OGRGeometryH );
int    OGR_G_Within( OGRGeometryH, OGRGeometryH );
int    OGR_G_Contains( OGRGeometryH, OGRGeometryH );
int    OGR_G_Overlaps( OGRGeometryH, OGRGeometryH );

OGRGeometryH OGR_G_GetBoundary( OGRGeometryH );
OGRGeometryH OGR_G_ConvexHull( OGRGeometryH );
OGRGeometryH OGR_G_Buffer( OGRGeometryH, double, int );
OGRGeometryH OGR_G_Intersection( OGRGeometryH, OGRGeometryH );
OGRGeometryH OGR_G_Union( OGRGeometryH, OGRGeometryH );
OGRGeometryH OGR_G_Difference( OGRGeometryH, OGRGeometryH );
OGRGeometryH OGR_G_SymmetricDifference( OGRGeometryH, OGRGeometryH );
double OGR_G_Distance( OGRGeometryH, OGRGeometryH );

double OGR_G_GetArea( OGRGeometryH );
int    OGR_G_Centroid( OGRGeometryH, OGRGeometryH );

void   OGR_G_Empty( OGRGeometryH );

/* Methods for getting/setting vertices in points, line strings and rings */
int    OGR_G_GetPointCount( OGRGeometryH );
double OGR_G_GetX( OGRGeometryH, int );
double OGR_G_GetY( OGRGeometryH, int );
double OGR_G_GetZ( OGRGeometryH, int );
void   OGR_G_SetPoint( OGRGeometryH, int iPoint, 
                       double, double, double );
void   OGR_G_SetPoint_2D( OGRGeometryH, int iPoint, 
                          double, double );
void   OGR_G_AddPoint( OGRGeometryH, double, double, double );
void   OGR_G_AddPoint_2D( OGRGeometryH, double, double );

/* Methods for getting/setting rings and members collections */

int    OGR_G_GetGeometryCount( OGRGeometryH );
OGRGeometryH OGR_G_GetGeometryRef( OGRGeometryH, int );
int OGR_G_AddGeometry( OGRGeometryH, OGRGeometryH );
int OGR_G_AddGeometryDirectly( OGRGeometryH, OGRGeometryH );

%{
/************************************************************************/
/*                         OGR_G_CreateFromWkb()			*/
/* 									*/
/*	Operates as:							*/
/*	  OGRGeometryH OGR_G_CreateFromWkb( bin_string, 		*/
/*					    OGRSpatialReferenceH ); 	*/
/************************************************************************/
static PyObject *
py_OGR_G_CreateFromWkb(PyObject *self, PyObject *args) {

    char *wkb_in = NULL, *srs_in = NULL;
    OGRSpatialReferenceH hSRS = NULL;
    OGRErr eErr;
    OGRGeometryH hGeom = NULL;
    int    wkb_len = -1;

    self = self;
    if(!PyArg_ParseTuple(args,"z#s:OGR_G_CreateFromWkb", &wkb_in, &wkb_len,
                         &srs_in))
        return NULL;

    if( srs_in && strlen(srs_in) > 0 ) {
        if (SWIG_GetPtr_2(srs_in,
			  (void **) &hSRS,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 2 of OGR_G_CreateFromWkb."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }

    eErr = OGR_G_CreateFromWkb( wkb_in, hSRS, &hGeom, wkb_len );

    if( eErr != CE_None )
    {
        if( eErr == OGRERR_CORRUPT_DATA )
            PyErr_SetString(PyExc_ValueError,
	                    "Corrupt WKB geometry passed to OGR_G_CreateFromWkb." );
	else
            PyErr_SetString(PyExc_ValueError,
	                    "OGR_G_CreateFromWkb failed." );

        return NULL;
    }
    else
    {
	char _ptemp[128];
        SWIG_MakePtr(_ptemp, (char *) hGeom,"_OGRGeometryH");
        return Py_BuildValue("s",_ptemp);
    }
}
%}

%native(OGR_G_CreateFromWkb) py_OGR_G_CreateFromWkb;

%{
/************************************************************************/
/*                        OGR_G_CreateFromWkt()                         */
/*                                                                      */
/*      Operates as:                                                    */
/*        OGRGeometryH OGR_G_CreateFromWkt( string,                     */
/*                                          OGRSpatialReferenceH );     */
/************************************************************************/
static PyObject *
py_OGR_G_CreateFromWkt(PyObject *self, PyObject *args) {

    char *wkt_in = NULL, *srs_in = NULL;
    OGRSpatialReferenceH hSRS = NULL;
    OGRErr eErr;
    OGRGeometryH hGeom = NULL;

    self = self;
    if(!PyArg_ParseTuple(args,"ss:OGR_G_CreateFromWkt", &wkt_in, 
                         &srs_in))
        return NULL;

    if( srs_in && strlen(srs_in) > 0 ) {
        if (SWIG_GetPtr_2(srs_in,
			  (void **) &hSRS,_OGRSpatialReferenceH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 2 of OGR_G_CreateFromWkt."
                            "  Expected _OGRSpatialReferenceH.");
            return NULL;
        }
    }

    eErr = OGR_G_CreateFromWkt( &wkt_in, hSRS, &hGeom );

    if( eErr != CE_None )
    {
        if( eErr == OGRERR_CORRUPT_DATA )
            PyErr_SetString(PyExc_ValueError,
	                    "Corrupt WKT geometry passed to OGR_G_CreateFromWkt." );
	else
            PyErr_SetString(PyExc_ValueError,
	                    "OGR_G_CreateFromWkt failed." );

        return NULL;
    }
    else
    {
	char _ptemp[128];
        SWIG_MakePtr(_ptemp, (char *) hGeom,"_OGRGeometryH");
        return Py_BuildValue("s",_ptemp);
    }
}
%}

%native(OGR_G_CreateFromWkt) py_OGR_G_CreateFromWkt;

%{
/************************************************************************/
/*                        OGR_G_ExportToWkb()                           */
/************************************************************************/
static PyObject *
py_OGR_G_ExportToWkb(PyObject *self, PyObject *args) {

    char *geom_in = NULL;
    OGRGeometryH hGeom;
    unsigned char *pabyWkb = NULL;
    int            nWkbSize = 0, byte_order;
    OGRErr eErr;

    self = self;
    if(!PyArg_ParseTuple(args,"si:OGR_G_ExportToWkb", &geom_in, &byte_order))
        return NULL;

    if( geom_in ) {
        if (SWIG_GetPtr_2(geom_in,
			  (void **) &hGeom,_OGRGeometryH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OGR_G_ExportToWkb."
                            "  Expected _OGRGeometryH.");
            return NULL;
        }
    }

    nWkbSize = OGR_G_WkbSize( hGeom );
    pabyWkb = (unsigned char *) CPLMalloc( nWkbSize );
    eErr = OGR_G_ExportToWkb( hGeom, (OGRwkbByteOrder) byte_order, pabyWkb );

    if( eErr != CE_None )
    {
	CPLFree( pabyWkb );
        return NULL;
    }
    else
    {
        PyObject *result = NULL;
	result = PyString_FromStringAndSize( pabyWkb, nWkbSize );
        CPLFree( pabyWkb );
        return result;
    }
}
%}

%native(OGR_G_ExportToWkb) py_OGR_G_ExportToWkb;

%{
/************************************************************************/
/*                         OGR_G_ExportToWkt()                          */
/************************************************************************/
static PyObject *
py_OGR_G_ExportToWkt(PyObject *self, PyObject *args) {

    char *geom_in = NULL;
    OGRGeometryH hGeom;
    char         *pszWkt;
    OGRErr eErr;

    self = self;
    if(!PyArg_ParseTuple(args,"s:OGR_G_ExportToWkt", &geom_in))
        return NULL;

    if( geom_in ) {
        if (SWIG_GetPtr_2(geom_in,
			  (void **) &hGeom,_OGRGeometryH)) {
            PyErr_SetString(PyExc_TypeError,
                            "Type error in argument 1 of OGR_G_ExportToWkb."
                            "  Expected _OGRGeometryH.");
            return NULL;
        }
    }

    eErr = OGR_G_ExportToWkt( hGeom, &pszWkt );

    if( eErr != CE_None )
        return NULL;
    else
    {
        PyObject *result = NULL;

	result = Py_BuildValue( "s", pszWkt );
	CPLFree( pszWkt );
        return result;
    }
}
%}

%native(OGR_G_ExportToWkt) py_OGR_G_ExportToWkt;

/* OGRFieldDefn */

OGRFieldDefnH  OGR_Fld_Create( const char *, OGRFieldType );
void    OGR_Fld_Destroy( OGRFieldDefnH );

void    OGR_Fld_SetName( OGRFieldDefnH, const char * );
const char  *OGR_Fld_GetNameRef( OGRFieldDefnH );
OGRFieldType  OGR_Fld_GetType( OGRFieldDefnH );
void    OGR_Fld_SetType( OGRFieldDefnH, OGRFieldType );
OGRJustification  OGR_Fld_GetJustify( OGRFieldDefnH );
void    OGR_Fld_SetJustify( OGRFieldDefnH, OGRJustification );
int     OGR_Fld_GetWidth( OGRFieldDefnH );
void    OGR_Fld_SetWidth( OGRFieldDefnH, int );
int     OGR_Fld_GetPrecision( OGRFieldDefnH );
void    OGR_Fld_SetPrecision( OGRFieldDefnH, int );
void    OGR_Fld_Set( OGRFieldDefnH, const char *, OGRFieldType, 
                            int, int, OGRJustification );

const char  *OGR_GetFieldTypeName( OGRFieldType );

/* OGRFeatureDefn */

OGRFeatureDefnH  OGR_FD_Create( const char * );
void    OGR_FD_Destroy( OGRFeatureDefnH );
const char  *OGR_FD_GetName( OGRFeatureDefnH );
int     OGR_FD_GetFieldCount( OGRFeatureDefnH );
OGRFieldDefnH  OGR_FD_GetFieldDefn( OGRFeatureDefnH, int );
int     OGR_FD_GetFieldIndex( OGRFeatureDefnH, const char * );
void    OGR_FD_AddFieldDefn( OGRFeatureDefnH, OGRFieldDefnH );
OGRwkbGeometryType OGR_FD_GetGeomType( OGRFeatureDefnH );
void    OGR_FD_SetGeomType( OGRFeatureDefnH, OGRwkbGeometryType );
int     OGR_FD_Reference( OGRFeatureDefnH );
int     OGR_FD_Dereference( OGRFeatureDefnH );
int     OGR_FD_GetReferenceCount( OGRFeatureDefnH );

/* OGRFeature */

OGRFeatureH  OGR_F_Create( OGRFeatureDefnH );
void    OGR_F_Destroy( OGRFeatureH );
OGRFeatureDefnH  OGR_F_GetDefnRef( OGRFeatureH );

int  OGR_F_SetGeometryDirectly( OGRFeatureH, OGRGeometryH );
int  OGR_F_SetGeometry( OGRFeatureH, OGRGeometryH );
OGRGeometryH  OGR_F_GetGeometryRef( OGRFeatureH );
OGRFeatureH  OGR_F_Clone( OGRFeatureH );
int     OGR_F_Equal( OGRFeatureH, OGRFeatureH );

int     OGR_F_GetFieldCount( OGRFeatureH );
OGRFieldDefnH  OGR_F_GetFieldDefnRef( OGRFeatureH, int );
int     OGR_F_GetFieldIndex( OGRFeatureH, const char * );

int     OGR_F_IsFieldSet( OGRFeatureH, int );
void    OGR_F_UnsetField( OGRFeatureH, int );

int     OGR_F_GetFieldAsInteger( OGRFeatureH, int );
double  OGR_F_GetFieldAsDouble( OGRFeatureH, int );
const char  *OGR_F_GetFieldAsString( OGRFeatureH, int );
const int  *OGR_F_GetFieldAsIntegerList( OGRFeatureH, int, int * );
const double  *OGR_F_GetFieldAsDoubleList( OGRFeatureH, int, int * );
char   **OGR_F_GetFieldAsStringList( OGRFeatureH, int );


void    OGR_F_SetFieldInteger( OGRFeatureH, int, int );
void    OGR_F_SetFieldDouble( OGRFeatureH, int, double );
void    OGR_F_SetFieldString( OGRFeatureH, int, const char * );
void    OGR_F_SetFieldIntegerList( OGRFeatureH, int, int, int * );
void    OGR_F_SetFieldDoubleList( OGRFeatureH, int, int, double * );
void    OGR_F_SetFieldStringList( OGRFeatureH, int, char ** );

%{
/************************************************************************/
/*                           OGR_F_GetField()                           */
/************************************************************************/
static PyObject *
py_OGR_F_GetField(PyObject *self, PyObject *args) {

    OGRFeatureH  hFeat;
    char  *feat_in = NULL;
    int    iField;
    PyObject *result = NULL;	

    self = self;
    if(!PyArg_ParseTuple(args,"si:OGR_F_GetField", &feat_in, &iField))
        return NULL;

    if (SWIG_GetPtr_2(feat_in,(void **) &hFeat,_OGRFeatureH)) {
        PyErr_SetString(PyExc_TypeError,
                        "Type error in argument 1 of OGR_F_GetField."
                        "  Expected _OGRFeatureH.");
        return NULL;
    }

    if( iField < 0 || iField >= OGR_F_GetFieldCount( hFeat ) )
    {
        PyErr_SetString(PyExc_ValueError,
                        "Illegal field requested in GetField()." );
	return NULL;	
    }

    if( !OGR_F_IsFieldSet( hFeat, iField ) )
    {  
         Py_INCREF( Py_None );
         result = Py_None;
    }
    else
    {
        OGRFieldType eFType;
	int          nCount, i;
	const int   *panList = NULL;
	const double  *padfList = NULL;
        PyObject *psList;
        char        **papszList;

        eFType = OGR_Fld_GetType( OGR_F_GetFieldDefnRef( hFeat, iField ) );

        switch( eFType ) 
        {
          case OFTInteger:
            result = Py_BuildValue( "i", 
                       OGR_F_GetFieldAsInteger( hFeat, iField ) );
	    break;
          case OFTReal:
            result = Py_BuildValue( "d", 
                       OGR_F_GetFieldAsDouble( hFeat, iField ) );
	    break;
          case OFTString:
            result = Py_BuildValue( "s", 
                       OGR_F_GetFieldAsString( hFeat, iField ) );
	    break;
          case OFTBinary:
            result = PyString_FromStringAndSize("",0);
            break;
          case OFTIntegerList:
            panList = OGR_F_GetFieldAsIntegerList(hFeat,iField,&nCount);
            psList = PyList_New( nCount );
            for( i=0; i < nCount; i++ )
                PyList_SetItem(psList, i, Py_BuildValue("i", panList[i]));
            result = psList;
            break;
          case OFTRealList:
            padfList = OGR_F_GetFieldAsDoubleList(hFeat,iField,&nCount);
            psList = PyList_New( nCount );
            for( i=0; i < nCount; i++ )
                PyList_SetItem(psList, i, Py_BuildValue("d", padfList[i]));
            result = psList;
            break;
          case OFTStringList:
            papszList = OGR_F_GetFieldAsStringList(hFeat,iField);
            nCount = CSLCount(papszList);
            psList = PyList_New( nCount );
            for( i=0; i < nCount; i++ )
                PyList_SetItem(psList, i, Py_BuildValue("s", papszList[i]));
            result = psList;
            break;
          default:
            CPLAssert( FALSE );
            break;
        }
    }

    return result;
}
%}

%native(OGR_F_GetField) py_OGR_F_GetField;

long    OGR_F_GetFID( OGRFeatureH );
int     OGR_F_SetFID( OGRFeatureH, long );
void    OGR_F_DumpReadable( OGRFeatureH, FILE * );
int     OGR_F_SetFrom( OGRFeatureH, OGRFeatureH, int );

const char  *OGR_F_GetStyleString( OGRFeatureH );
void    OGR_F_SetStyleString( OGRFeatureH, const char * );

/* -------------------------------------------------------------------- */
/*      ogrsf_frmts.h                                                   */
/* -------------------------------------------------------------------- */

typedef void *OGRLayerH;
typedef void *OGRDataSourceH;
typedef void *OGRSFDriverH;

/* OGRLayer */

OGRGeometryH  OGR_L_GetSpatialFilter( OGRLayerH );
void    OGR_L_SetSpatialFilter( OGRLayerH, OGRGeometryH );
void    OGR_L_SetSpatialFilterRect( OGRLayerH, double, double, double, double);
int     OGR_L_SetAttributeFilter( OGRLayerH, NULLableString );
void    OGR_L_ResetReading( OGRLayerH );
int     OGR_L_SetNextByIndex( OGRLayerH, long );
OGRFeatureH  OGR_L_GetNextFeature( OGRLayerH );
OGRFeatureH  OGR_L_GetFeature( OGRLayerH, long );
int     OGR_L_SetFeature( OGRLayerH, OGRFeatureH );
int     OGR_L_CreateFeature( OGRLayerH, OGRFeatureH );
int     OGR_L_DeleteFeature( OGRLayerH, long );
OGRFeatureDefnH  OGR_L_GetLayerDefn( OGRLayerH );
OGRSpatialReferenceH  OGR_L_GetSpatialRef( OGRLayerH );
int     OGR_L_GetFeatureCount( OGRLayerH, int );
int     OGR_L_GetExtent( OGRLayerH, OGREnvelope *, int );
int     OGR_L_TestCapability( OGRLayerH, const char * );
int     OGR_L_CreateField( OGRLayerH, OGRFieldDefnH, int );
int     OGR_L_StartTransaction( OGRLayerH );
int     OGR_L_CommitTransaction( OGRLayerH );
int     OGR_L_RollbackTransaction( OGRLayerH );
int     OGR_L_Reference( OGRLayerH );
int     OGR_L_Dereference( OGRLayerH );
int     OGR_L_GetRefCount( OGRLayerH );
int     OGR_L_SyncToDisk( OGRLayerH );
int     OGR_L_GetFeaturesRead( OGRLayerH );

/* OGRDataSource */

void OGR_DS_Destroy( OGRDataSourceH );
const char  *OGR_DS_GetName( OGRDataSourceH );
int     OGR_DS_GetLayerCount( OGRDataSourceH );
OGRLayerH  OGR_DS_GetLayer( OGRDataSourceH, int );
OGRLayerH OGR_DS_GetLayerByName( OGRDataSourceH, const char * );
int     OGR_DS_DeleteLayer( OGRDataSourceH, int );
int     OGR_DS_TestCapability( OGRDataSourceH, const char * );
OGRLayerH  OGR_DS_ExecuteSQL( OGRDataSourceH, const char *, OGRGeometryH, 
                              const char * );
void    OGR_DS_ReleaseResultSet( OGRDataSourceH, OGRLayerH );
int     OGR_DS_Reference( OGRDataSourceH );
int     OGR_DS_Dereference( OGRDataSourceH );
int     OGR_DS_GetRefCount( OGRDataSourceH );
int     OGR_DS_GetSummaryRefCount( OGRDataSourceH );
OGRSFDriverH OGR_DS_GetDriver( OGRDataSourceH );
OGRLayerH OGR_DS_CopyLayer(OGRDataSourceH, OGRLayerH, const char*, stringList);
OGRLayerH OGR_DS_CreateLayer( OGRDataSourceH, const char *, 
                              OGRSpatialReferenceH, OGRwkbGeometryType,
	                      stringList );

/* OGRSFDriver */

const char  *OGR_Dr_GetName( OGRSFDriverH );
OGRDataSourceH  OGR_Dr_Open( OGRSFDriverH, const char *, int );
int     OGR_Dr_TestCapability( OGRSFDriverH, const char * );
OGRDataSourceH  OGR_Dr_CreateDataSource( OGRSFDriverH, const char *,
                                         stringList );
OGRDataSourceH OGR_Dr_CopyDataSource( OGRSFDriverH,  OGRDataSourceH, 
	                              const char *, stringList );
int     OGR_Dr_DeleteDataSource( OGRSFDriverH, const char * );

/* OGRSFDriverRegistrar */

OGRDataSourceH OGROpen( const char *, int, void * );
void           OGRRegisterDriver( OGRSFDriverH );
int            OGRGetDriverCount();
OGRSFDriverH   OGRGetDriverByName( const char * );
OGRSFDriverH   OGRGetDriver( int );
OGRDataSourceH OGROpenShared( const char *, int, OGRSFDriverH * );
int            OGRReleaseDataSource( OGRDataSourceH );
int            OGRGetOpenDSCount();
OGRDataSourceH OGRGetOpenDS(int);

/* note: this is also declared in ogrsf_frmts.h */
void  OGRRegisterAll();


%{
/************************************************************************/
/*                      OGRBuildPolygonFromEdges()                      */
/************************************************************************/
static PyObject *
py_OGRBuildPolygonFromEdges(PyObject *self, PyObject *args) {

    OGRGeometryH  hLineCollection, hPolygon;
    char  *lines_in = NULL;
    int    bBestEffort, bAutoClose;
    double dfTolerance;
    OGRErr eErr;
    char _ptemp[128];

    self = self;
    if(!PyArg_ParseTuple(args,"siid:OGRBuildPolygonFromEdges", &lines_in, 
                         &bBestEffort, &bAutoClose, &dfTolerance ))
        return NULL;

    if (SWIG_GetPtr_2(lines_in,(void **) &hLineCollection,_OGRGeometryH)) {
        PyErr_SetString(PyExc_TypeError,
                        "Type error in argument 1 of OGRBuildPolygonFromEdges."
                        "  Expected _OGRGeometryH.");
        return NULL;
    }

    hPolygon = OGRBuildPolygonFromEdges( hLineCollection, bBestEffort, 
                                         bAutoClose, dfTolerance, &eErr );

    if( eErr != OGRERR_NONE )
    {
        PyErr_SetString(PyExc_ValueError,
                        "Failed to assemble some or all edges into polygon rings." );
	return NULL;
    }

    SWIG_MakePtr(_ptemp, (char *) hPolygon,"_OGRGeometryH");
    return Py_BuildValue("s",_ptemp);
}
%}

%native(OGRBuildPolygonFromEdges) py_OGRBuildPolygonFromEdges;

int  OGRSetGenerate_DB2_V72_BYTE_ORDER( int bGenerate_DB2_V72_BYTE_ORDER );
