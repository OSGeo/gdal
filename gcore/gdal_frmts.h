/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Prototypes for all format specific driver initialization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_FRMTS_H_INCLUDED
#define GDAL_FRMTS_H_INCLUDED

#include "cpl_port.h"

CPL_C_START
void CPL_DLL GDALRegister_GTiff(void);
void CPL_DLL GDALRegister_GXF(void);
void CPL_DLL GDALRegister_HFA(void);
void CPL_DLL GDALRegister_AAIGrid(void);
void CPL_DLL GDALRegister_GRASSASCIIGrid(void);
void CPL_DLL GDALRegister_ISG(void);
void CPL_DLL GDALRegister_AIGrid(void);
// void CPL_DLL GDALRegister_AIGrid2(void);
void CPL_DLL GDALRegister_CEOS(void);
void CPL_DLL GDALRegister_SAR_CEOS(void);
void CPL_DLL GDALRegister_SDTS(void);
void CPL_DLL GDALRegister_ELAS(void);
void CPL_DLL GDALRegister_EHdr(void);
void CPL_DLL GDALRegister_GenBin(void);
void CPL_DLL GDALRegister_PAux(void);
void CPL_DLL GDALRegister_ENVI(void);
void CPL_DLL GDALRegister_DOQ1(void);
void CPL_DLL GDALRegister_DOQ2(void);
void CPL_DLL GDALRegister_DTED(void);
void CPL_DLL GDALRegister_MFF(void);
void CPL_DLL GDALRegister_HKV(void);
void CPL_DLL GDALRegister_PNG(void);
void DeclareDeferredPNGPlugin(void);
void CPL_DLL GDALRegister_DDS(void);
void CPL_DLL DeclareDeferredDDSPlugin(void);
void CPL_DLL GDALRegister_GTA(void);
void CPL_DLL DeclareDeferredGTAPlugin(void);
void CPL_DLL GDALRegister_JPEG(void);
void DeclareDeferredJPEGPlugin(void);
void CPL_DLL GDALRegister_JP2KAK(void);
void DeclareDeferredJP2KAKPlugin(void);
void CPL_DLL GDALRegister_JPIPKAK(void);
void DeclareDeferredJPIPKAKPlugin(void);
void CPL_DLL GDALRegister_MEM(void);
void CPL_DLL GDALRegister_JDEM(void);
void CPL_DLL GDALRegister_RASDAMAN(void);
void CPL_DLL GDALRegister_PNM(void);
void CPL_DLL GDALRegister_GIF(void);
void CPL_DLL DeclareDeferredGIFPlugin(void);
void CPL_DLL GDALRegister_BIGGIF(void);
void CPL_DLL GDALRegister_Envisat(void);
void CPL_DLL GDALRegister_FITS(void);
void CPL_DLL DeclareDeferredFITSPlugin(void);
void CPL_DLL GDALRegister_ECW(void);
void DeclareDeferredECWPlugin(void);
void CPL_DLL GDALRegister_JP2ECW(void);
void CPL_DLL GDALRegister_ECW_JP2ECW();
void CPL_DLL GDALRegister_FujiBAS(void);
void CPL_DLL GDALRegister_FIT(void);
void CPL_DLL GDALRegister_VRT(void);
void CPL_DLL GDALRegister_GTI(void);
void CPL_DLL GDALRegister_USGSDEM(void);
void CPL_DLL GDALRegister_FAST(void);
void CPL_DLL GDALRegister_HDF4(void);
void CPL_DLL DeclareDeferredHDF4Plugin(void);
void CPL_DLL GDALRegister_HDF4Image(void);
void CPL_DLL GDALRegister_L1B(void);
void CPL_DLL GDALRegister_LDF(void);
void CPL_DLL GDALRegister_BSB(void);
void CPL_DLL GDALRegister_XPM(void);
void CPL_DLL GDALRegister_BMP(void);
void CPL_DLL GDALRegister_GSC(void);
void CPL_DLL GDALRegister_NITF(void);
void DeclareDeferredNITFPlugin(void);
void CPL_DLL GDALRegister_RPFTOC(void);
void CPL_DLL GDALRegister_ECRGTOC(void);
void CPL_DLL GDALRegister_MrSID(void);
void DeclareDeferredMrSIDPlugin(void);
void CPL_DLL GDALRegister_MG4Lidar(void);
void CPL_DLL GDALRegister_PCIDSK(void);
void DeclareDeferredPCIDSKPlugin(void);
void CPL_DLL GDALRegister_BT(void);
void CPL_DLL GDALRegister_netCDF(void);
void DeclareDeferredNetCDFPlugin(void);
void CPL_DLL GDALRegister_LAN(void);
void CPL_DLL GDALRegister_CPG(void);
void CPL_DLL GDALRegister_AirSAR(void);
void CPL_DLL GDALRegister_RS2(void);
void CPL_DLL GDALRegister_ILWIS(void);
void CPL_DLL GDALRegister_PCRaster(void);
void DeclareDeferredPCRasterPlugin(void);
void CPL_DLL GDALRegister_IDA(void);
void CPL_DLL GDALRegister_NDF(void);
void CPL_DLL GDALRegister_RMF(void);
void CPL_DLL GDALRegister_BAG(void);
void CPL_DLL GDALRegister_S102(void);
void CPL_DLL GDALRegister_S104(void);
void CPL_DLL GDALRegister_S111(void);
void CPL_DLL GDALRegister_HDF5(void);
void DeclareDeferredHDF5Plugin(void);
void CPL_DLL GDALRegister_HDF5Image(void);
void CPL_DLL GDALRegister_MSGN(void);
void CPL_DLL GDALRegister_MSG(void);
void DeclareDeferredMSGPlugin(void);
void CPL_DLL GDALRegister_RIK(void);
void CPL_DLL GDALRegister_Leveller(void);
void CPL_DLL GDALRegister_SGI(void);
void CPL_DLL GDALRegister_SRTMHGT(void);
void CPL_DLL GDALRegister_DIPEx(void);
void CPL_DLL GDALRegister_ISIS3(void);
void CPL_DLL GDALRegister_ISIS2(void);
void CPL_DLL GDALRegister_PDS(void);
void DeclareDeferredPDSPlugin(void);
void CPL_DLL GDALRegister_PDS4(void);
void CPL_DLL GDALRegister_VICAR(void);
void CPL_DLL GDALRegister_IDRISI(void);
void CPL_DLL GDALRegister_Terragen(void);
void CPL_DLL GDALRegister_WCS(void);
void DeclareDeferredWCSPlugin(void);
void CPL_DLL GDALRegister_WMS(void);
void DeclareDeferredWMSPlugin(void);
void CPL_DLL GDALRegister_HTTP(void);
void CPL_DLL GDALRegister_GSAG(void);
void CPL_DLL GDALRegister_GSBG(void);
void CPL_DLL GDALRegister_GS7BG(void);
void CPL_DLL GDALRegister_GRIB(void);
void DeclareDeferredGRIBPlugin(void);
void CPL_DLL GDALRegister_INGR(void);
void CPL_DLL GDALRegister_ERS(void);
void CPL_DLL GDALRegister_PALSARJaxa(void);
void CPL_DLL GDALRegister_DIMAP();
void CPL_DLL GDALRegister_GFF(void);
void CPL_DLL GDALRegister_COSAR(void);
void CPL_DLL GDALRegister_TSX(void);
void CPL_DLL GDALRegister_ADRG(void);
void CPL_DLL GDALRegister_SRP(void);
void CPL_DLL GDALRegister_COASP(void);
void CPL_DLL GDALRegister_BLX(void);
void CPL_DLL GDALRegister_LCP(void);
void CPL_DLL GDALRegister_EIR(void);
void CPL_DLL GDALRegister_ESRIC(void);
void CPL_DLL GDALRegister_GEOR(void);
void DeclareDeferredGEORPlugin(void);
void CPL_DLL GDALRegister_TIL(void);
void CPL_DLL GDALRegister_R(void);
void CPL_DLL GDALRegister_Rasterlite(void);
void DeclareDeferredRasterlitePlugin(void);
void CPL_DLL GDALRegister_PostGISRaster(void);
void DeclareDeferredPostGISRasterPlugin(void);
void CPL_DLL GDALRegister_NWT_GRD(void);
void CPL_DLL GDALRegister_NWT_GRC(void);
void CPL_DLL GDALRegister_SAGA(void);
void CPL_DLL GDALRegister_KMLSUPEROVERLAY(void);
void CPL_DLL GDALRegister_GTX(void);
void CPL_DLL GDALRegister_LOSLAS(void);
void CPL_DLL GDALRegister_NTv2(void);
void CPL_DLL GDALRegister_CTable2(void);
void CPL_DLL GDALRegister_JP2OpenJPEG(void);
void DeclareDeferredOPENJPEGPlugin(void);
void CPL_DLL GDALRegister_XYZ(void);
void CPL_DLL GDALRegister_HF2(void);
void CPL_DLL GDALRegister_PDF(void);
void DeclareDeferredPDFPlugin(void);
void CPL_DLL GDALRegister_MAP(void);
void CPL_DLL GDALRegister_OZI(void);
void CPL_DLL GDALRegister_ACE2(void);
void CPL_DLL GDALRegister_CTG(void);
void CPL_DLL GDALRegister_SNODAS(void);
void CPL_DLL GDALRegister_WEBP(void);
void DeclareDeferredWEBPPlugin(void);
void CPL_DLL GDALRegister_ZMap(void);
void CPL_DLL GDALRegister_NGSGEOID(void);
void CPL_DLL GDALRegister_MBTiles(void);
void CPL_DLL GDALRegister_IRIS(void);
void CPL_DLL GDALRegister_KRO(void);
void CPL_DLL GDALRegister_KEA(void);
void DeclareDeferredKEAPlugin(void);
void CPL_DLL GDALRegister_ROIPAC(void);
void CPL_DLL GDALRegister_PLMOSAIC(void);
void CPL_DLL GDALRegister_CALS(void);
void CPL_DLL GDALRegister_ISCE(void);
void CPL_DLL GDALRegister_WMTS(void);
void DeclareDeferredWMTSPlugin(void);
void CPL_DLL GDALRegister_SAFE(void);
void CPL_DLL GDALRegister_SENTINEL2(void);
void CPL_DLL GDALRegister_mrf(void);
void DeclareDeferredMRFPlugin(void);
void CPL_DLL GDALRegister_RRASTER(void);
void CPL_DLL GDALRegister_Derived(void);
void CPL_DLL GDALRegister_PRF(void);
void CPL_DLL GDALRegister_NULL(void);
void CPL_DLL GDALRegister_EEDAI(void);
void CPL_DLL GDALRegister_EEDA(void);
void CPL_DLL GDALRegister_SIGDEM(void);
void CPL_DLL GDALRegister_BYN(void);
void CPL_DLL GDALRegister_TileDB(void);
void DeclareDeferredTileDBPlugin(void);
void CPL_DLL GDALRegister_DAAS(void);
void CPL_DLL GDALRegister_COG(void);
void CPL_DLL GDALRegister_RDB(void);
void CPL_DLL GDALRegister_EXR(void);
void DeclareDeferredEXRPlugin(void);
void CPL_DLL GDALRegister_AVIF(void);
void DeclareDeferredAVIFPlugin(void);
void CPL_DLL GDALRegister_HEIF(void);
void DeclareDeferredHEIFPlugin(void);
void CPL_DLL GDALRegister_TGA(void);
void CPL_DLL GDALRegister_OGCAPI(void);
void CPL_DLL GDALRegister_STACTA(void);
void CPL_DLL GDALRegister_Zarr(void);
void DeclareDeferredZarrPlugin(void);
void CPL_DLL GDALRegister_STACIT(void);
void CPL_DLL GDALRegister_JPEGXL(void);
void DeclareDeferredJPEGXLPlugin(void);
void CPL_DLL GDALRegister_BASISU(void);
void CPL_DLL GDALRegister_KTX2(void);
void CPL_DLL GDALRegister_BASISU_KTX2(void);
void DeclareDeferredBASISU_KTX2Plugin(void);
void CPL_DLL GDALRegister_NOAA_B(void);
void CPL_DLL GDALRegister_NSIDCbin(void);
void CPL_DLL GDALRegister_SNAP_TIFF(void);
void CPL_DLL GDALRegister_RCM(void);
CPL_C_END

#endif /* ndef GDAL_FRMTS_H_INCLUDED */
