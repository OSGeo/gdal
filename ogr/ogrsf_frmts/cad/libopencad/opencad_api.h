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
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
  * SPDX-License-Identifier: MIT
 *******************************************************************************/
#ifndef OPENCAD_API_H
#define OPENCAD_API_H

#include "cadfile.h"

enum CADVersions
{
    DWG_R13   = 1012,
    DWG_R14   = 1014,
    DWG_R2000 = 1015,
    DWG_R2004 = 1018,
    DWG_R2007 = 1021,
    DWG_R2010 = 1024,
    DWG_R2013 = 1027,
    DXF_UNDEF = -1000,
    DXF_R13   = -DWG_R13,
    DXF_R14   = -DWG_R14,
    DXF_R2000 = -DWG_R2000,
    DXF_R2004 = -DWG_R2004,
    DXF_R2007 = -DWG_R2007,
    DXF_R2010 = -DWG_R2010,
    DXF_R2013 = -DWG_R2013
};

enum CADErrorCodes
{
    SUCCESS = 0,                /**< operation successfully executed */
    FILE_OPEN_FAILED,           /**< failed to open CAD file */
    UNSUPPORTED_VERSION,        /**< unsupported CAD file version */
    FILE_PARSE_FAILED,          /**< failed to parse file */
    SECTION_LOCATOR_READ_FAILED,/**< failed to read section locator */
    HEADER_SECTION_READ_FAILED, /**< failed to read header section */
    CLASSES_SECTION_READ_FAILED,/**< failed to read classes section */
    TABLES_SECTION_READ_FAILED, /**< failed to read tables section */
    BLOCKS_SECTION_READ_FAILED, /**< failed to read blocks section */
    ENTITIES_SECTION_READ_FAILED,/**< failed to read entities section */
    OBJECTS_SECTION_READ_FAILED, /**< failed to read objects section */
    THUMBNAILIMAGE_SECTION_READ_FAILED, /**< failed to read thumbnailimage section */
    TABLE_READ_FAILED,          /**< failed to read table*/
    VALUE_EXISTS                /**< the value already exist in the header */
};

OCAD_EXTERN int GetVersion();
OCAD_EXTERN const char * GetVersionString();
OCAD_EXTERN CADFile    * OpenCADFile( CADFileIO * pCADFileIO, enum CADFile::OpenOptions eOptions,
                                      bool bReadUnsupportedGeometries = false );
OCAD_EXTERN CADFile    * OpenCADFile( const char * pszFileName, enum CADFile::OpenOptions eOptions,
                                      bool bReadUnsupportedGeometries = false );
OCAD_EXTERN int GetLastErrorCode();
OCAD_EXTERN CADFileIO * GetDefaultFileIO( const char * pszFileName );
OCAD_EXTERN int IdentifyCADFile( CADFileIO * pCADFileIO, bool bOwn = true );
OCAD_EXTERN const char * GetCADFormats();

#endif // OPENCAD_API_H
