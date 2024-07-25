#ifdef RENAME_INTERNAL_SHAPELIB_SYMBOLS
#include "gdal_shapelib_symbol_rename.h"
#endif

#define SHP_RESTORE_SHX_HINT_MESSAGE                                           \
    " Set SHAPE_RESTORE_SHX config option to YES to restore or create it."

#include "shpopen.c"
