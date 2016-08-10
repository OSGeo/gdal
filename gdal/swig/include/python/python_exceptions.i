/*
 * This was the cpl_exceptions.i code.  But since python is the only one
 * different (should support old method as well as new one)
 * it was moved into this file.
 */
%{
static int bUseExceptions=0;
static CPLErrorHandler pfnPreviousHandler = CPLDefaultErrorHandler;

static void CPL_STDCALL
PythonBindingErrorHandler(CPLErr eclass, int code, const char *msg )
{
  /*
  ** Generally we want to suppress error reporting if we have exceptions
  ** enabled as the error message will be in the exception thrown in
  ** Python.
  */

  /* If the error class is CE_Fatal, we want to have a message issued
     because the CPL support code does an abort() before any exception
     can be generated */
  if (eclass == CE_Fatal ) {
    pfnPreviousHandler(eclass, code, msg );
  }

  /*
  ** We do not want to interfere with warnings or debug messages since
  ** they won't be translated into exceptions.
  */
  if (eclass == CE_Warning || eclass == CE_Debug ) {
    pfnPreviousHandler(eclass, code, msg );
  }
}
%}

%inline %{

static
int GetUseExceptions() {
  CPLErrorReset();
  return bUseExceptions;
}

static
void UseExceptions() {
  CPLErrorReset();
  if( !bUseExceptions )
  {
    bUseExceptions = 1;
    pfnPreviousHandler =
        CPLSetErrorHandler( (CPLErrorHandler) PythonBindingErrorHandler );
  }
}

static
void DontUseExceptions() {
  CPLErrorReset();
  if( bUseExceptions )
  {
    bUseExceptions = 0;
    CPLSetErrorHandler( pfnPreviousHandler );
  }
}
%}

%{
/* Completely unrelated: just to avoid Coverity warnings */

static int bReturnSame = 1;

void NeverCallMePlease() {
    bReturnSame = 0;
}

/* Some SWIG code generates dead code, which Coverity warns about */
template<class T> static T ReturnSame(T x)
{
    if( bReturnSame )
        return x;
    return 0;
}

%}

%include exception.i

%exception {

    if ( bUseExceptions ) {
        CPLErrorReset();
    }
    $action
%#ifndef SED_HACKS
    if ( bUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
%#endif
}
