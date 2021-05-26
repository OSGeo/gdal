/* This is a GDAL specific variant and not the same as the libgeotiff generated geo_config.h */
#ifndef GEO_CONFIG_H
#define GEO_CONFIG_H

#include "cpl_config.h"

#ifndef __COVERITY__
#include "cpl_string.h"
#ifdef sprintf
#undef sprintf
#endif
#define sprintf CPLsprintf
#endif

/* Hide symbols in GDAL builds --with-hide-internal-symbols */
#if defined(USE_GCC_VISIBILITY_FLAG)
#define GTIF_DLL
#else
#define GTIF_DLL CPL_DLL
#endif

#ifdef RENAME_INTERNAL_LIBTIFF_SYMBOLS
#include "gdal_libtiff_symbol_rename.h"
#endif

#ifdef RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS
#include "gdal_libgeotiff_symbol_rename.h"
#endif

#endif /* ndef GEO_CONFIG_H */
