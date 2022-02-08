/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALAllRegister(), primary format registration.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_priv.h"
#include "gdal_frmts.h"
#include "ogrsf_frmts.h"

#ifdef GNM_ENABLED
   #include "gnm_frmts.h"
#endif

CPL_CVSID("$Id$")

#ifdef notdef
// we may have a use for this some day
static char *szConfiguredFormats = "GDAL_FORMATS";
#endif

/************************************************************************/
/*                          GDALAllRegister()                           */
/*                                                                      */
/*      Register all identifiably supported formats.                    */
/************************************************************************/

/**
 * Register all known configured GDAL drivers.
 *
 * This function will drive any of the following that are configured into
 * GDAL.  See <a href="http://gdal.org/formats_list.html">raster list</a> and
 * <a href="http://gdal.org/ogr_formats.html">vector full list</a>
 *
 * This function should generally be called once at the beginning of the
 * application.
 */

void CPL_STDCALL GDALAllRegister()

{
    auto poDriverManager = GetGDALDriverManager();
    // AutoLoadDrivers is a no-op if compiled with GDAL_NO_AUTOLOAD defined.
    poDriverManager->AutoLoadDrivers();

    // NOTE: frmts/drivers.ini in the same directory should be kept in same
    // order as this file

#ifdef FRMT_vrt
    GDALRegister_VRT();
    GDALRegister_Derived();
#endif

#ifdef FRMT_gtiff
    GDALRegister_GTiff();
    GDALRegister_COG();
#endif

#ifdef FRMT_nitf
    GDALRegister_NITF();
    GDALRegister_RPFTOC();
    GDALRegister_ECRGTOC();
#endif

#ifdef FRMT_hfa
    GDALRegister_HFA();
#endif

#ifdef FRMT_ceos2
    GDALRegister_SAR_CEOS();
#endif

#ifdef FRMT_ceos
    GDALRegister_CEOS();
#endif

#ifdef FRMT_jaxapalsar
    GDALRegister_PALSARJaxa();
#endif

#ifdef FRMT_gff
    GDALRegister_GFF();
#endif

#ifdef FRMT_elas
    GDALRegister_ELAS();
#endif

#ifdef FRMT_esric
    GDALRegister_ESRIC();
#endif

#ifdef FRMT_aigrid
//    GDALRegister_AIGrid2();
    GDALRegister_AIGrid();
#endif

#ifdef FRMT_aaigrid
    GDALRegister_AAIGrid();
    GDALRegister_GRASSASCIIGrid();
    GDALRegister_ISG();
#endif

#ifdef FRMT_sdts
    GDALRegister_SDTS();
#endif

#ifdef FRMT_dted
    GDALRegister_DTED();
#endif

#ifdef FRMT_png
    GDALRegister_PNG();
#endif

#ifdef FRMT_dds
    GDALRegister_DDS();
#endif

#ifdef FRMT_gta
    GDALRegister_GTA();
#endif

#ifdef FRMT_jpeg
    GDALRegister_JPEG();
#endif

#ifdef FRMT_mem
    GDALRegister_MEM();
#endif

#ifdef FRMT_jdem
    GDALRegister_JDEM();
#endif

#ifdef FRMT_rasdaman
    GDALRegister_RASDAMAN();
#endif

#ifdef FRMT_gif
    GDALRegister_GIF();
    GDALRegister_BIGGIF();
#endif

#ifdef FRMT_envisat
    GDALRegister_Envisat();
#endif

#ifdef FRMT_fits
    GDALRegister_FITS();
#endif

#ifdef FRMT_bsb
    GDALRegister_BSB();
#endif

#ifdef FRMT_xpm
    GDALRegister_XPM();
#endif

#ifdef FRMT_bmp
    GDALRegister_BMP();
#endif

#ifdef FRMT_dimap
    GDALRegister_DIMAP();
#endif

#ifdef FRMT_airsar
    GDALRegister_AirSAR();
#endif

#ifdef FRMT_rs2
    GDALRegister_RS2();
#endif

#ifdef FRMT_safe
    GDALRegister_SAFE();
#endif

#ifdef FRMT_pcidsk
    GDALRegister_PCIDSK();
#endif

#ifdef FRMT_pcraster
    GDALRegister_PCRaster();
#endif

#ifdef FRMT_ilwis
    GDALRegister_ILWIS();
#endif

#ifdef FRMT_sgi
    GDALRegister_SGI();
#endif

#ifdef FRMT_srtmhgt
    GDALRegister_SRTMHGT();
#endif

#ifdef FRMT_leveller
    GDALRegister_Leveller();
#endif

#ifdef FRMT_terragen
    GDALRegister_Terragen();
#endif

#ifdef FRMT_netcdf
    GDALRegister_netCDF();
#endif

#ifdef FRMT_hdf4
    GDALRegister_HDF4();
    GDALRegister_HDF4Image();
#endif

#ifdef FRMT_pds
    GDALRegister_ISIS3();
    GDALRegister_ISIS2();
    GDALRegister_PDS();
    GDALRegister_PDS4();
    GDALRegister_VICAR();
#endif

#ifdef FRMT_til
    GDALRegister_TIL();
#endif

#ifdef FRMT_ers
    GDALRegister_ERS();
#endif

#ifdef FRMT_jp2kak
    // JPEG2000 support using Kakadu toolkit
    GDALRegister_JP2KAK();
#endif

#ifdef FRMT_jpipkak
    // JPEG2000 support using Kakadu toolkit
    GDALRegister_JPIPKAK();
#endif

#ifdef FRMT_jp2lura
    // JPEG2000 support using Lurawave library
    GDALRegister_JP2Lura();
#endif

#ifdef FRMT_ecw
    GDALRegister_ECW();
    GDALRegister_JP2ECW();
#endif

#ifdef FRMT_openjpeg
    // JPEG2000 support using OpenJPEG library
    GDALRegister_JP2OpenJPEG();
#endif

#ifdef FRMT_l1b
    GDALRegister_L1B();
#endif

#ifdef FRMT_fit
    GDALRegister_FIT();
#endif

#ifdef FRMT_grib
    GDALRegister_GRIB();
#endif

#ifdef FRMT_mrsid
    GDALRegister_MrSID();
#endif

#ifdef FRMT_rmf
    GDALRegister_RMF();
#endif

#ifdef FRMT_wcs
    GDALRegister_WCS();
#endif

#ifdef FRMT_wms
    GDALRegister_WMS();
#endif

#ifdef FRMT_msgn
    GDALRegister_MSGN();
#endif

#ifdef FRMT_msg
    GDALRegister_MSG();
#endif

#ifdef FRMT_idrisi
    GDALRegister_IDRISI();
#endif

#ifdef FRMT_gsg
    GDALRegister_GSAG();
    GDALRegister_GSBG();
    GDALRegister_GS7BG();
#endif

#ifdef FRMT_cosar
    GDALRegister_COSAR();
#endif

#ifdef FRMT_tsx
    GDALRegister_TSX();
#endif

#ifdef FRMT_coasp
    GDALRegister_COASP();
#endif

#ifdef FRMT_r
    GDALRegister_R();
#endif

#ifdef FRMT_map
    GDALRegister_MAP();
#endif

#ifdef FRMT_kmlsuperoverlay
    GDALRegister_KMLSUPEROVERLAY();
#endif

#ifdef FRMT_webp
    GDALRegister_WEBP();
#endif

#ifdef FRMT_pdf
    GDALRegister_PDF();
#endif

#ifdef FRMT_rasterlite
    GDALRegister_Rasterlite();
#endif

#ifdef FRMT_mbtiles
    GDALRegister_MBTiles();
#endif

#ifdef FRMT_plmosaic
    GDALRegister_PLMOSAIC();
#endif

#ifdef FRMT_cals
    GDALRegister_CALS();
#endif

#ifdef FRMT_wmts
    GDALRegister_WMTS();
#endif

#ifdef FRMT_sentinel2
    GDALRegister_SENTINEL2();
#endif

#ifdef FRMT_mrf
    GDALRegister_mrf();
#endif

#ifdef FRMT_tiledb
    GDALRegister_TileDB();
#endif

#ifdef FRMT_rdb
    GDALRegister_RDB();
#endif
/* -------------------------------------------------------------------- */
/*      Put raw formats at the end of the list. These drivers support   */
/*      various ASCII-header labeled formats, so the driver could be    */
/*      confused if you have files in some of above formats and such    */
/*      ASCII-header in the same directory.                             */
/* -------------------------------------------------------------------- */

#ifdef FRMT_raw
    GDALRegister_PNM();
    GDALRegister_DOQ1();
    GDALRegister_DOQ2();
    GDALRegister_PAux();
    GDALRegister_MFF();
    GDALRegister_HKV();
    GDALRegister_GSC();
    GDALRegister_FAST();
    GDALRegister_BT();
    GDALRegister_LAN();
    GDALRegister_CPG();
    GDALRegister_NDF();
    GDALRegister_EIR();
    GDALRegister_DIPEx();
    GDALRegister_LCP();
    GDALRegister_GTX();
    GDALRegister_LOSLAS();
    GDALRegister_NTv2();
    GDALRegister_CTable2();
    GDALRegister_ACE2();
    GDALRegister_SNODAS();
    GDALRegister_KRO();
    GDALRegister_ROIPAC();
    GDALRegister_RRASTER();
    GDALRegister_BYN();
#endif

#ifdef FRMT_arg
    GDALRegister_ARG();
#endif

/* -------------------------------------------------------------------- */
/*      Our test for the following is weak or expensive so we try       */
/*      them last.                                                      */
/* -------------------------------------------------------------------- */

#ifdef FRMT_rik
    GDALRegister_RIK();
#endif

#ifdef FRMT_usgsdem
    GDALRegister_USGSDEM();
#endif

#ifdef FRMT_gxf
    GDALRegister_GXF();
#endif

#ifdef FRMT_grass
    GDALRegister_GRASS();
#endif

#ifdef FRMT_dods
    GDALRegister_DODS();
#endif

/* Register KEA before HDF5 */
#ifdef FRMT_kea
    GDALRegister_KEA();
#endif

#ifdef FRMT_hdf5
    GDALRegister_BAG();
    GDALRegister_HDF5();
    GDALRegister_HDF5Image();
#endif

#ifdef FRMT_northwood
    GDALRegister_NWT_GRD();
    GDALRegister_NWT_GRC();
#endif

#ifdef FRMT_adrg
    GDALRegister_ADRG();
    GDALRegister_SRP();
#endif

#ifdef FRMT_blx
    GDALRegister_BLX();
#endif

#ifdef FRMT_georaster
    GDALRegister_GEOR();
#endif

#ifdef FRMT_postgisraster
    GDALRegister_PostGISRaster();
#endif

#ifdef FRMT_saga
    GDALRegister_SAGA();
#endif

#ifdef FRMT_xyz
    GDALRegister_XYZ();
#endif

#ifdef FRMT_hf2
    GDALRegister_HF2();
#endif

#ifdef FRMT_ozi
    GDALRegister_OZI();
#endif

#ifdef FRMT_ctg
    GDALRegister_CTG();
#endif

#ifdef FRMT_zmap
    GDALRegister_ZMap();
#endif

#ifdef FRMT_ngsgeoid
    GDALRegister_NGSGEOID();
#endif

#ifdef FRMT_iris
    GDALRegister_IRIS();
#endif

#ifdef FRMT_prf
    GDALRegister_PRF();
#endif

#ifdef FRMT_eeda
    GDALRegister_EEDAI();
    GDALRegister_EEDA();
#endif

#ifdef FRMT_daas
    GDALRegister_DAAS();
#endif

#ifdef FRMT_null
    GDALRegister_NULL();
#endif

#ifdef FRMT_sigdem
    GDALRegister_SIGDEM();
#endif

#ifdef FRMT_exr
    GDALRegister_EXR();
#endif

#ifdef FRMT_heif
    GDALRegister_HEIF();
#endif

#ifdef FRMT_tga
    GDALRegister_TGA();
#endif

#ifdef FRMT_ogcapi
    GDALRegister_OGCAPI();
#endif

#ifdef FRMT_stacta
    GDALRegister_STACTA();
#endif

#ifdef FRMT_stacit
    GDALRegister_STACIT();
#endif

    // NOTE: you need to generally insert your own driver before that line.

    // NOTE: frmts/drivers.ini in the same directory should be kept in same
    // order as this file

/* -------------------------------------------------------------------- */
/*     GNM and OGR drivers                                              */
/* -------------------------------------------------------------------- */
#ifdef GNM_ENABLED
    GNMRegisterAllInternal();
#endif

    OGRRegisterAllInternal();

/* -------------------------------------------------------------------- */
/*      Put here drivers that absolutely need to look for side car      */
/*      files in their Identify()/Open() procedure.                     */
/* -------------------------------------------------------------------- */

#ifdef FRMT_raw
    GDALRegister_GenBin();
    GDALRegister_ENVI();
    GDALRegister_EHdr();
    GDALRegister_ISCE();
#endif

#ifdef FRMT_zarr
    GDALRegister_Zarr();
#endif

/* -------------------------------------------------------------------- */
/*      Register GDAL HTTP last, to let a chance to other drivers       */
/*      accepting URL to handle them before.                            */
/* -------------------------------------------------------------------- */
#if (!defined(GDAL_CMAKE_BUILD) && defined(FRMT_wcs)) || (defined(GDAL_CMAKE_BUILD) && defined(FRMT_http))
    GDALRegister_HTTP();
#endif

    poDriverManager->AutoLoadPythonDrivers();

/* -------------------------------------------------------------------- */
/*      Deregister any drivers explicitly marked as suppressed by the   */
/*      GDAL_SKIP environment variable.                                 */
/* -------------------------------------------------------------------- */
    poDriverManager->AutoSkipDrivers();

    poDriverManager->ReorderDrivers();
}
