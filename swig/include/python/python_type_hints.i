/*
 * SWIG generates Python type annotations containing C/C++ types:
 *
 * https://www.swig.org/Doc4.1/SWIGDocumentation.html#Python_annotations_c
 *
 * Since mypy won't recognize those by default, we define type aliases here
 * to map them back to well-known python types
 */
%pythoncode %{
# Type aliases
from enum import IntEnum
from typing_extensions import TypeAlias
void: TypeAlias = None
double: TypeAlias = float
OGRErr: TypeAlias = int
CPLErr: TypeAlias =int
# See cpl_error.h
# TODO: Typemap to convert return sto CPLErr(ret)
# TODO: Would this break python API ?
#class CPLErr(IntEnum):
#    CE_None = 0
#    CE_Debug = 1
#    CE_Warning = 2
#    CE_Failure = 3
#    CE_Fatal = 4
%}

