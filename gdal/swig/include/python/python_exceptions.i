/*
 * This was the cpl_exceptions.i code.  But since python is the only one
 * different (should support old method as well as new one)
 * it was moved into this file.
 */
%{
int bUseExceptions=0;
CPLErrorHandler pfnPreviousHandler = CPLDefaultErrorHandler;

void CPL_STDCALL 
PythonBindingErrorHandler(CPLErr eclass, int code, const char *msg ) 
{
  /* 
  ** Generally we want to supress error reporting if we have exceptions
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

int GetUseExceptions() {
  return bUseExceptions;
}

void UseExceptions() {
  bUseExceptions = 1;
  pfnPreviousHandler = 
    CPLSetErrorHandler( (CPLErrorHandler) PythonBindingErrorHandler );
}

void DontUseExceptions() {
  bUseExceptions = 0;
  CPLSetErrorHandler( pfnPreviousHandler );
}
%}

%include exception.i

%exception {

    if ( bUseExceptions ) {
        CPLErrorReset();
    }
    $action
    if ( bUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
}

