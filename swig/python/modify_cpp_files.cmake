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

if("${FILE}" MATCHES "gdal_wrap.cpp")
    string(REGEX REPLACE "result = \\(CPLErr\\)([^;]+)(\\;)"
                         [[CPL_IGNORE_RET_VAL(result = (CPLErr)\1)\2]]
           _CONTENTS "${_CONTENTS}")
endif()

string(REPLACE "PyObject *resultobj = 0;"
               "PyObject *resultobj = 0; int bLocalUseExceptionsCode = GetUseExceptions();"
       _CONTENTS "${_CONTENTS}")

string(REPLACE "#define SWIGPYTHON"
               "#define SWIGPYTHON\n\#define SED_HACKS"
       _CONTENTS "${_CONTENTS}")

string(REPLACE "return resultobj;"
               "if ( ReturnSame(bLocalUseExceptionsCode) ) { CPLErr eclass = CPLGetLastErrorType(); if ( eclass == CE_Failure || eclass == CE_Fatal ) { Py_XDECREF(resultobj); SWIG_Error( SWIG_RuntimeError, CPLGetLastErrorMsg() ); return NULL; } }\n  return resultobj;"
       _CONTENTS "${_CONTENTS}")

# Below works around https://github.com/swig/swig/issues/2638 and https://github.com/swig/swig/issues/2037#issuecomment-874372082
# to avoid the ""swig/python detected a memory leak of type 'OSRSpatialReferenceShadow *', no destructor found."
# error message of https://github.com/OSGeo/gdal/issues/4907
# The issue is that starting with SWIG 4.1, the SWIG_Python_DestroyModule(), which
# is run in the gdal module (since __init__.py loads gdal in first),
# removes the destructor on the OGRSpatialReferenceShadow type (among others),
# which prevents full free of object of those type.
# The following hack just makes SWIG_Python_DestroyModule() a no-op, which
# will leak a bit of memory, but anyway SWIG currently can only free one single
# SWIG module, so we had already memleaks
# To be revisted if above mentioned SWIG issues are resolved
string(REPLACE "if (--interpreter_counter != 0) // another sub-interpreter may still be using the swig_module's types"
               "/* Even Rouault / GDAL hack for SWIG >= 4.1 related to objects not being freed. See swig/python/modify_cpp_files.cmake for more details */\nif( 1 )"
       _CONTENTS "${_CONTENTS}")

file(WRITE ${FILE} "${_CONTENTS}")
