/* this ALWAYS GENERATED file contains the definitions for the interfaces */


/* File created by MIDL compiler version 5.01.0164 */
/* at Fri Jul 23 18:09:52 1999
 */
/* Compiler settings for Cover.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32, ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __Cover_h__
#define __Cover_h__

#ifdef __cplusplus
extern "C"{
#endif 

/* Forward Declarations */ 

#ifndef __DimensionImpl_FWD_DEFINED__
#define __DimensionImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class DimensionImpl DimensionImpl;
#else
typedef struct DimensionImpl DimensionImpl;
#endif /* __cplusplus */

#endif 	/* __DimensionImpl_FWD_DEFINED__ */


#ifndef __ColorTableImpl_FWD_DEFINED__
#define __ColorTableImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class ColorTableImpl ColorTableImpl;
#else
typedef struct ColorTableImpl ColorTableImpl;
#endif /* __cplusplus */

#endif 	/* __ColorTableImpl_FWD_DEFINED__ */


#ifndef __OGRRealGC_FWD_DEFINED__
#define __OGRRealGC_FWD_DEFINED__

#ifdef __cplusplus
typedef class OGRRealGC OGRRealGC;
#else
typedef struct OGRRealGC OGRRealGC;
#endif /* __cplusplus */

#endif 	/* __OGRRealGC_FWD_DEFINED__ */


#ifndef __GeoReferenceFactoryImpl_FWD_DEFINED__
#define __GeoReferenceFactoryImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class GeoReferenceFactoryImpl GeoReferenceFactoryImpl;
#else
typedef struct GeoReferenceFactoryImpl GeoReferenceFactoryImpl;
#endif /* __cplusplus */

#endif 	/* __GeoReferenceFactoryImpl_FWD_DEFINED__ */


#ifndef __GridCoverageFactoryImpl_FWD_DEFINED__
#define __GridCoverageFactoryImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class GridCoverageFactoryImpl GridCoverageFactoryImpl;
#else
typedef struct GridCoverageFactoryImpl GridCoverageFactoryImpl;
#endif /* __cplusplus */

#endif 	/* __GridCoverageFactoryImpl_FWD_DEFINED__ */


#ifndef __GridGeometryImpl_FWD_DEFINED__
#define __GridGeometryImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class GridGeometryImpl GridGeometryImpl;
#else
typedef struct GridGeometryImpl GridGeometryImpl;
#endif /* __cplusplus */

#endif 	/* __GridGeometryImpl_FWD_DEFINED__ */


#ifndef __GridInfoImpl_FWD_DEFINED__
#define __GridInfoImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class GridInfoImpl GridInfoImpl;
#else
typedef struct GridInfoImpl GridInfoImpl;
#endif /* __cplusplus */

#endif 	/* __GridInfoImpl_FWD_DEFINED__ */


#ifndef __AffineGeoReferenceImpl_FWD_DEFINED__
#define __AffineGeoReferenceImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class AffineGeoReferenceImpl AffineGeoReferenceImpl;
#else
typedef struct AffineGeoReferenceImpl AffineGeoReferenceImpl;
#endif /* __cplusplus */

#endif 	/* __AffineGeoReferenceImpl_FWD_DEFINED__ */


#ifndef __GridGeometryFactoryImpl_FWD_DEFINED__
#define __GridGeometryFactoryImpl_FWD_DEFINED__

#ifdef __cplusplus
typedef class GridGeometryFactoryImpl GridGeometryFactoryImpl;
#else
typedef struct GridGeometryFactoryImpl GridGeometryFactoryImpl;
#endif /* __cplusplus */

#endif 	/* __GridGeometryFactoryImpl_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "CoverageIdl.h"

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 


#ifndef __OGRCoverage_LIBRARY_DEFINED__
#define __OGRCoverage_LIBRARY_DEFINED__

/* library OGRCoverage */
/* [helpstring][version][uuid] */ 




























EXTERN_C const IID LIBID_OGRCoverage;

EXTERN_C const CLSID CLSID_DimensionImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2821-3f74-11d3-b406-0080c8e62564")
DimensionImpl;
#endif

EXTERN_C const CLSID CLSID_ColorTableImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2822-3f74-11d3-b406-0080c8e62564")
ColorTableImpl;
#endif

EXTERN_C const CLSID CLSID_OGRRealGC;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2823-3f74-11d3-b406-0080c8e62564")
OGRRealGC;
#endif

EXTERN_C const CLSID CLSID_GeoReferenceFactoryImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2824-3f74-11d3-b406-0080c8e62564")
GeoReferenceFactoryImpl;
#endif

EXTERN_C const CLSID CLSID_GridCoverageFactoryImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2825-3f74-11d3-b406-0080c8e62564")
GridCoverageFactoryImpl;
#endif

EXTERN_C const CLSID CLSID_GridGeometryImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2826-3f74-11d3-b406-0080c8e62564")
GridGeometryImpl;
#endif

EXTERN_C const CLSID CLSID_GridInfoImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2827-3f74-11d3-b406-0080c8e62564")
GridInfoImpl;
#endif

EXTERN_C const CLSID CLSID_AffineGeoReferenceImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2828-3f74-11d3-b406-0080c8e62564")
AffineGeoReferenceImpl;
#endif

EXTERN_C const CLSID CLSID_GridGeometryFactoryImpl;

#ifdef __cplusplus

class DECLSPEC_UUID("699a2829-3f74-11d3-b406-0080c8e62564")
GridGeometryFactoryImpl;
#endif
#endif /* __OGRCoverage_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif
