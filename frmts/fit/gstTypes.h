#if 0

  Copyright (c) 2001 Keyhole, Inc.  All Rights Reserved.

  THIS IS UNPUBLISHED PROPRIETY SOURCE CODE OF KEYHOLE, INC.

#endif

#ifndef _gstTypes_h_
#define _gstTypes_h_

#include <stdarg.h>

#if     !defined(TRUE) || ((TRUE) != 1)
#define TRUE    (1)
#endif
#if     !defined(FALSE) || ((FALSE) != 0)
#define FALSE   (0)
#endif
#if     !defined(SUCCESS) || ((SUCCESS) != 0)
#define SUCCESS (0)
#endif
#if     !defined(FAILURE) || ((FAILURE) != -1)
#define FAILURE (-1)
#endif

typedef int (*gstItemGetFunc)(void *data, int tag, ...);

#if defined(__sgi)
#include <sys/types.h>
typedef uint16_t                        uint16;
typedef int16_t                         int16;
typedef uint32_t                        uint32;
typedef int32_t                         int32;
typedef uint64_t                        uint64;
typedef int64_t                         int64;
#elif defined(__linux__)
#include <sys/types.h>
typedef u_int16_t                       uint16;
typedef int16_t                         int16;
typedef u_int32_t                       uint32;
typedef int32_t                         int32;
typedef u_int64_t                       uint64;
typedef int64_t                         int64;
#define TRUE -1
#define FALSE 0
#elif defined(_WIN32)
typedef unsigned short                  uint16;
typedef short                           int16;
typedef unsigned long                   uint32;
typedef long                            int32;
typedef unsigned __int64                uint64;
typedef __int64                         int64;
#endif

typedef unsigned char                   uchar;


typedef uint32 gstFormatType;
typedef uint32 gstClassType;

struct gstFormatDesc {
    int tag;
    char *tagStr;
    uint32 flags;
    char *label;
    char *verbose;
    void *spec;
};

struct gstFmtDescSet {
    char *label;
    gstFormatDesc *fmtDesc;
    int fmtCount;
};

enum gstTagFlags {
    gstTagInt        = 0x00,
    gstTagUInt       = 0x01,
    gstTagFloat      = 0x02,
    gstTagDouble     = 0x03,
    gstTagEnumString = 0x04,
    gstTagEnumInt    = 0x05,
    gstTagString     = 0x06,
    gstTagBoolean    = 0x07,
    gstTagTypeMask   = 0x0f,

    gstTagImmutable  = 0x10,
    gstTagStateMask  = 0xf0

};

struct gstEnumStringSpec {
    char *name;
    char *desc;
};

struct gstEnumIntSpec {
    int tag;
    char *desc;
};

typedef uint32 gstPrimType;
enum {
    gstPoint           = 0x01,
    gstLine            = 0x02,
    gstPolygon         = 0x03,
    gstArea            = 0x04,
    gstPrimTypeMask    = 0x0f,

    gstStreet          = 0x10,
    gstPolyLine        = 0x20,
    gstPrimSubTypeMask = 0xf0
};

    

enum {
    GST_NODE = 0,
    GST_GROUP = 1,
    GST_GEODE = 2,
    GST_GEOSET = 3,
    GST_QUADNODE = 4
};


#endif // !_gstTypes_h_
