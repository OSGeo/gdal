/******************************************************************************
 *
 * Code for Optional Exception Handling through UseExceptions(),
 * DontUseExceptions()
 *
 * It uses CPLSetErrorHandler to provide a custom function
 * which notifies the bindings of errors.
 *
 * This is not thread safe.
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

%{
void VeryQuietErrorHandler(CPLErr eclass, int code, const char *msg ) {
  /* If the error class is CE_Fatal, we want to have a message issued
     because the CPL support code does an abort() before any exception
     can be generated */
  if (eclass == CE_Fatal ) {
    CPLDefaultErrorHandler(eclass, code, msg );
  }
}
%}

%inline %{
void UseExceptions() {
  CPLSetErrorHandler( (CPLErrorHandler) VeryQuietErrorHandler );
}

void DontUseExceptions() {
  CPLSetErrorHandler( CPLDefaultErrorHandler );
}
%}

%include exception.i

%exception {
    CPLErrorReset();
    $action
    CPLErr eclass = CPLGetLastErrorType();
    if ( eclass == CE_Failure || eclass == CE_Fatal ) {
#if defined(SWIGCSHARP)
      SWIG_CSharpException(SWIG_RuntimeError, CPLGetLastErrorMsg());
#else
      SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
#endif
    }
}
