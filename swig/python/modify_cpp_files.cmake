# Sanitize SWIG output for analyzers (cppcheck, coverity, ...)

file(READ ${FILE} _CONTENTS)

# Following is to prevent Coverity Scan to warn about Dereference before null check (REVERSE_INULL)
string(REPLACE "if (!PyTuple_Check(args)) SWIG_fail;"
               "if (args == NULL || !PyTuple_Check(args)) SWIG_fail;"
       _CONTENTS "${_CONTENTS}")

string(REPLACE "argc = args ? (int)PyObject_Length(args) : 0"
               "argc = (int)PyObject_Length(args)"
       _CONTENTS "${_CONTENTS}")

string(REGEX REPLACE "if \\(SWIG_IsTmpObj\\(res([1-9])\\)\\)"
                     "if \(ReturnSame\(SWIG_IsTmpObj\(res\\1\)\)\)"
       _CONTENTS "${_CONTENTS}")

string(REGEX REPLACE "if\\( alloc([1-9]) == SWIG_NEWOBJ \\)"
                     "if \(ReturnSame(alloc\\1) == SWIG_NEWOBJ \)"
       _CONTENTS "${_CONTENTS}")

string(REPLACE "strncpy(buff, \"swig_ptr: \", 10)"
               "memcpy(buff, \"swig_ptr: \", 10)"
       _CONTENTS "${_CONTENTS}")

# Unused assigned variable
string(REPLACE "module_head = &swig_module;"
               "/*module_head = &swig_module;*/"
       _CONTENTS "${_CONTENTS}")

# SWIG defect fixed per https://github.com/swig/swig/commit/b0e29fbdf31bb94b11cb8a7cc830b4a76467afa3#diff-ba23eb3671d250e6a62261f19f653dd2R93
string(REPLACE "obj = PyUnicode_AsUTF8String(obj);"
               "obj = PyUnicode_AsUTF8String(obj); if (!obj) return SWIG_TypeError;"
       _CONTENTS "${_CONTENTS}")

# TODO: To remove once swig/python/GNUmakefile has gone
string(APPEND _CONTENTS "#define POST_PROCESSING_APPLIED\n")

if("${FILE}" MATCHES "gdal_wrap.cpp")
    string(REGEX REPLACE "result = \\(CPLErr\\)([^;]+)(\\;)"
                         [[CPL_IGNORE_RET_VAL(result = (CPLErr)\1)\2]]
           _CONTENTS "${_CONTENTS}")
endif()

if(NOT "${FILE}" MATCHES "gdal_array_wrap.cpp")
    string(REPLACE "PyObject *resultobj = 0;"
                   "PyObject *resultobj = 0; int bLocalUseExceptionsCode = bUseExceptions;"
           _CONTENTS "${_CONTENTS}")

    string(REPLACE "#define SWIGPYTHON"
                   "#define SWIGPYTHON\n\#define SED_HACKS"
           _CONTENTS "${_CONTENTS}")

    string(REPLACE "return resultobj;"
                   "if ( ReturnSame(bLocalUseExceptionsCode) ) { CPLErr eclass = CPLGetLastErrorType(); if ( eclass == CE_Failure || eclass == CE_Fatal ) { Py_XDECREF(resultobj); SWIG_Error( SWIG_RuntimeError, CPLGetLastErrorMsg() ); return NULL; } }\n  return resultobj;"
           _CONTENTS "${_CONTENTS}")
endif()

file(WRITE ${FILE} "${_CONTENTS}")
