
//
// CPL
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

void PyErrorHandlerProxy( CPLErr eErrType, int nErrorCode, const char *pszMsg )

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

typedef int CPLErr;

%inline %{
  void Debug( const char *msg_class, const char *message ) {
    CPLDebug( msg_class, message );
  }
  void Error( CPLErr msg_class = CE_Failure, int err_code = 0, const char* msg = "error" ) {
    CPLError( msg_class, err_code, msg );
  }
  void PushErrorHandler( char const * handler = "CPLQuietErrorHandler" ) {
    return;
  }
%}

%rename (PopErrorHandler) CPLPopErrorHandler;
void CPLPopErrorHandler();

%rename (ErrorReset) CPLErrorReset;
void CPLErrorReset();

%rename (GetLastErrorNo) CPLGetLastErrorNo;
int CPLGetLastErrorNo();

%rename (GetLastErrorType) CPLGetLastErrorType;
CPLErr CPLGetLastErrorType();

%rename (GetLastErrorMsg) CPLGetLastErrorMsg;
char const *CPLGetLastErrorMsg();

%rename (PushFinderLocation) CPLPushFinderLocation;
void CPLPushFinderLocation( const char * );

%rename (PopFinderLocation) CPLPopFinderLocation;
void CPLPopFinderLocation();

%rename (FinderClean) CPLFinderClean;
void CPLFinderClean();

%rename (FindFile) CPLFindFile;
const char * CPLFindFile( const char *, const char * );

%rename (SetConfigOption) CPLSetConfigOption;
void CPLSetConfigOption( const char *, const char * );

%rename (GetConfigOption) CPLGetConfigOption;
const char * CPLGetConfigOption( const char *, const char * );
