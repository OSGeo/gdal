/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
 *
  * SPDX-License-Identifier: MIT
 *******************************************************************************/
#ifndef OPENCAD_H
#define OPENCAD_H

#define OCAD_VERSION    "0.3.4"
#define OCAD_VERSION_MAJOR 0
#define OCAD_VERSION_MINOR 3
#define OCAD_VERSION_REV   4

#ifndef OCAD_COMPUTE_VERSION
#define OCAD_COMPUTE_VERSION(maj,min,rev) ((maj)*10000+(min)*100+rev) // maj - any, min < 99, rev < 99
#endif

#define OCAD_VERSION_NUM OCAD_COMPUTE_VERSION(OCAD_VERSION_MAJOR,OCAD_VERSION_MINOR,OCAD_VERSION_REV)

/*  check if the current version is at least major.minor.revision */
#define CHECK_VERSION(major,minor,rev) \
    (OCAD_VERSION_MAJOR > (major) || \
    (OCAD_VERSION_MAJOR == (major) && OCAD_VERSION_MINOR > (minor)) || \
    (OCAD_VERSION_MAJOR == (major) && OCAD_VERSION_MINOR == (minor) && OCAD_VERSION_REV >= (release)))

#define DWG_VERSION_STR_SIZE  6

#ifndef OCAD_EXTERN
#ifdef OCAD_STATIC
  #define OCAD_EXTERN extern
#else
#   if defined (_MSC_VER)
#    ifdef OCAD_EXPORTS
#      define OCAD_EXTERN __declspec(dllexport) // extern 
#      else
#      define OCAD_EXTERN __declspec(dllimport) // extern 
#      endif
#    else
#     if defined(__GNUC__) && __GNUC__ >= 4
#       define OCAD_EXTERN __attribute__((visibility("default")))
#     else
#       define OCAD_EXTERN                extern
#     endif
#   endif
#endif
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
# define OCAD_PRINT_FUNC_FORMAT( format_idx, arg_idx ) __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#else
#  define OCAD_PRINT_FUNC_FORMAT( format_idx, arg_idx )
#endif

void DebugMsg(const char *, ...) OCAD_PRINT_FUNC_FORMAT (1,2);

#endif // OPENCAD_H
