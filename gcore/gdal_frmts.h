/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Prototypes for all format specific driver initializations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.35  2005/01/06 20:27:51  fwarmerdam
 * added NDF driver
 *
 * Revision 1.34  2004/12/26 16:16:33  fwarmerdam
 * added ida format
 *
 * Revision 1.33  2004/12/20 16:14:00  fwarmerdam
 * added GDALRegister_JP2ECW()
 *
 * Revision 1.32  2004/11/30 16:59:37  lichun
 * Added ILWIS
 *
 * Revision 1.31  2004/10/22 14:14:49  fwarmerdam
 * Added PCRaster.
 *
 * Revision 1.30  2004/10/21 19:22:41  fwarmerdam
 * Added GDALRegister_RS2().
 *
 * Revision 1.29  2004/10/16 14:39:04  fwarmerdam
 * added GMT format
 *
 * Revision 1.28  2004/09/16 18:23:42  fwarmerdam
 * added airsar
 *
 * Revision 1.27  2004/09/03 19:06:03  warmerda
 * added CPG driver
 *
 * Revision 1.26  2004/05/26 14:15:56  warmerda
 * Added LAN (.LAN/.GIS format).
 *
 * Revision 1.25  2004/01/07 20:06:12  warmerda
 * Added netCDF register
 *
 * Revision 1.24  2003/12/13 00:00:07  jimg
 * Added GDALRegister_DODS().
 *
 * Revision 1.23  2003/12/11 06:18:43  warmerda
 * added BT driver
 *
 * Revision 1.22  2003/09/09 12:14:25  dron
 * Added PCIDSK driver.
 *
 * Revision 1.21  2003/04/23 12:24:26  dron
 * MrSID driver added, EFF removed.
 *
 * Revision 1.20  2002/12/03 19:02:39  dron
 * Added BMP driver.
 *
 * Revision 1.19  2002/12/03 04:41:16  warmerda
 * added NITF
 *
 * Revision 1.18  2002/10/03 05:41:27  warmerda
 * added GSC Geogrid format
 *
 * Revision 1.17  2002/09/26 18:10:59  warmerda
 * added JP2KAK
 *
 * Revision 1.16  2002/09/19 14:49:54  warmerda
 * added jpeg2000
 *
 * Revision 1.15  2002/08/13 16:59:53  dron
 * New driver: EOSAT FAST format
 *
 * Revision 1.14  2002/07/16 13:30:27  dron
 * New driver: HDF4 dataset.
 *
 * Revision 1.13  2002/05/08 16:33:25  dron
 * NOAA Polar Orbiter Dataset reader added
 *
 * Revision 1.12  2002/04/12 20:19:15  warmerda
 * added xpm
 *
 * Revision 1.11  2002/03/04 21:54:30  warmerda
 * added envi format
 *
 * Revision 1.10  2001/12/08 04:44:23  warmerda
 * added BSB
 *
 * Revision 1.9  2001/11/27 14:39:02  warmerda
 * added usgsdem
 *
 * Revision 1.8  2001/11/16 21:14:03  warmerda
 * added VRT dataset
 *
 * Revision 1.7  2001/09/19 15:26:14  warmerda
 * added doq2
 *
 * Revision 1.6  2001/07/05 23:53:53  nemec
 * Add FIT file format
 *
 * Revision 1.5  2001/05/15 13:20:32  warmerda
 * added fujibas
 *
 * Revision 1.4  2001/04/02 17:12:01  warmerda
 * Added ecw support.
 *
 * Revision 1.3  2001/03/12 15:17:03  warmerda
 * added aaigrid
 *
 * Revision 1.2  2001/03/06 03:53:44  sperkins
 * Added FITS format support.
 *
 * Revision 1.1  2001/02/06 16:30:05  warmerda
 * New
 *
 */

#ifndef GDAL_FRMTS_H_INCLUDED
#define GDAL_FRMTS_H_INCLUDED

#include "cpl_port.h"

CPL_C_START
void CPL_DLL GDALRegister_GDB(void);
void CPL_DLL GDALRegister_GTiff(void);
void CPL_DLL GDALRegister_GXF(void);
void CPL_DLL GDALRegister_OGDI(void);
void CPL_DLL GDALRegister_HFA(void);
void CPL_DLL GDALRegister_AAIGrid(void);
void CPL_DLL GDALRegister_AIGrid(void);
void CPL_DLL GDALRegister_AIGrid2(void);
void CPL_DLL GDALRegister_CEOS(void);
void CPL_DLL GDALRegister_SAR_CEOS(void);
void CPL_DLL GDALRegister_SDTS(void);
void CPL_DLL GDALRegister_ELAS(void);
void CPL_DLL GDALRegister_EHdr(void);
void CPL_DLL GDALRegister_PAux(void);
void CPL_DLL GDALRegister_ENVI(void);
void CPL_DLL GDALRegister_DOQ1(void);
void CPL_DLL GDALRegister_DOQ2(void);
void CPL_DLL GDALRegister_DTED(void);
void CPL_DLL GDALRegister_MFF(void);
void CPL_DLL GDALRegister_HKV(void);
void CPL_DLL GDALRegister_PNG(void);
void CPL_DLL GDALRegister_JPEG(void);
void CPL_DLL GDALRegister_JPEG2000(void);
void CPL_DLL GDALRegister_JP2KAK(void);
void CPL_DLL GDALRegister_MEM(void);
void CPL_DLL GDALRegister_JDEM(void);
void CPL_DLL GDALRegister_GRASS(void);
void CPL_DLL GDALRegister_PNM(void);
void CPL_DLL GDALRegister_GIF(void);
void CPL_DLL GDALRegister_Envisat(void);
void CPL_DLL GDALRegister_FITS(void);
void CPL_DLL GDALRegister_ECW(void);
void CPL_DLL GDALRegister_JP2ECW(void);
void CPL_DLL GDALRegister_FujiBAS(void);
void CPL_DLL GDALRegister_FIT(void);
void CPL_DLL GDALRegister_VRT(void);
void CPL_DLL GDALRegister_USGSDEM(void);
void CPL_DLL GDALRegister_FAST(void);
void CPL_DLL GDALRegister_HDF4(void);
void CPL_DLL GDALRegister_HDF4Image(void);
void CPL_DLL GDALRegister_L1B(void);
void CPL_DLL GDALRegister_LDF(void);
void CPL_DLL GDALRegister_BSB(void);
void CPL_DLL GDALRegister_XPM(void);
void CPL_DLL GDALRegister_BMP(void);
void CPL_DLL GDALRegister_GSC(void);
void CPL_DLL GDALRegister_NITF(void);
void CPL_DLL GDALRegister_MrSID(void);
void CPL_DLL GDALRegister_PCIDSK(void);
void CPL_DLL GDALRegister_BT(void);
void CPL_DLL GDALRegister_DODS(void);
void CPL_DLL GDALRegister_GMT(void);
void CPL_DLL GDALRegister_netCDF(void);
void CPL_DLL GDALRegister_LAN(void);
void CPL_DLL GDALRegister_CPG(void);
void CPL_DLL GDALRegister_AirSAR(void);
void CPL_DLL GDALRegister_RS2(void);
void CPL_DLL GDALRegister_ILWIS(void);
void CPL_DLL GDALRegister_PCRaster(void);
void CPL_DLL GDALRegister_IDA(void);
void CPL_DLL GDALRegister_NDF(void);
CPL_C_END

#endif /* ndef GDAL_FRMTS_H_INCLUDED */
