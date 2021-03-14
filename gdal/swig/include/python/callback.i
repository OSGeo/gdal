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


static int CPL_STDCALL
PyProgressProxy( double dfComplete, const char *pszMessage, void *pData ) CPL_UNUSED;

static int CPL_STDCALL
PyProgressProxy( double dfComplete, const char *pszMessage, void *pData )

{
    PyProgressData *psInfo = (PyProgressData *) pData;
    PyObject *psArgs, *psResult;
    int      bContinue = TRUE;

    if( psInfo->nLastReported == (int) (100.0 * dfComplete) )
        return TRUE;

    if( psInfo->psPyCallback == NULL || psInfo->psPyCallback == Py_None )
        return TRUE;

    psInfo->nLastReported = (int) (100.0 * dfComplete);

    if( pszMessage == NULL )
        pszMessage = "";

    SWIG_PYTHON_THREAD_BEGIN_BLOCK;

    if( psInfo->psPyCallbackData == NULL )
        psArgs = Py_BuildValue("(dsO)", dfComplete, pszMessage, Py_None );
    else
        psArgs = Py_BuildValue("(dsO)", dfComplete, pszMessage,
	                       psInfo->psPyCallbackData );

    psResult = PyObject_CallObject( psInfo->psPyCallback, psArgs);
    Py_XDECREF(psArgs);

    if( PyErr_Occurred() != NULL )
    {
        PyErr_Print();
        PyErr_Clear();
        SWIG_PYTHON_THREAD_END_BLOCK;
        return FALSE;
    }

    if( psResult == NULL )
    {
        SWIG_PYTHON_THREAD_END_BLOCK;
        return TRUE;
    }

    if( psResult == Py_None )
    {
        SWIG_PYTHON_THREAD_END_BLOCK;
        return TRUE;
    }

    if( !PyArg_Parse( psResult, "i", &bContinue ) )
    {
        PyErr_Clear();
        CPLError(CE_Failure, CPLE_AppDefined, "bad progress return value");
        Py_XDECREF(psResult);
        SWIG_PYTHON_THREAD_END_BLOCK;
        return FALSE;
    }

    Py_XDECREF(psResult);
    SWIG_PYTHON_THREAD_END_BLOCK;

    return bContinue;
}
%}

