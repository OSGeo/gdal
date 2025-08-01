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
#ifndef CADFILE_H
#define CADFILE_H

#include "caddictionary.h"
#include "cadclasses.h"
#include "cadfileio.h"
#include "cadtables.h"

/**
 * @brief The abstract CAD file class
 */
class OCAD_EXTERN CADFile
{
    friend class CADTables;
    friend class CADLayer;

public:
    /**
     * @brief The CAD file open options enum
     */
    enum OpenOptions
    {
        READ_ALL, /**< read all available information */
        READ_FAST, /**< read some methadata */
        READ_FASTEST    /**< read only geometry and layers */
    };

public:
    explicit                 CADFile( CADFileIO * poFileIO );
    virtual                 ~CADFile();

public:
    const CADHeader & getHeader() const;
    const CADClasses& getClasses() const;
    const CADTables & getTables() const;

public:
    virtual int    ParseFile( enum OpenOptions eOptions, bool bReadUnsupportedGeometries = true );
    virtual size_t GetLayersCount() const;
    virtual CADLayer& GetLayer( size_t index );

    /**
     * @brief returns NamedObjectDictionary (root) of all others dictionaries
     * @return root CADDictionary
     */
    virtual CADDictionary GetNOD() = 0;

//    virtual size_t GetBlocksCount();
//    virtual CADBlockObject * GetBlock( size_t index );

protected:
    /**
     * @brief Get CAD Object from file
     * @param dObjectHandle Object handle
     * @param bHandlesOnly set TRUE if object data should be skipped, and only object handles should be read.
     * @return pointer to CADObject or nullptr. User have to free returned pointer.
     */
    virtual CADObject * GetObject( long dObjectHandle, bool bHandlesOnly = false ) = 0;

    /**
     * @brief read geometry from CAD file
     * @param iLayerIndex layer index (counts from 0)
     * @param dHandle Handle of CAD object
     * @param dBlockRefHandle Handle of BlockRef (0 if geometry is not in block reference)
     * @return NULL if failed or pointer which mast be feed by user
     */
    virtual CADGeometry * GetGeometry( size_t iLayerIndex, long dHandle, long dBlockRefHandle = 0 ) = 0;

    /**
     * @brief initially read some basic values and section locator
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int ReadSectionLocators() = 0;

    /**
     * @brief Read header from CAD file
     * @param eOptions Read options
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int ReadHeader( enum OpenOptions eOptions ) = 0;

    /**
     * @brief Read classes from CAD file
     * @param eOptions Read options
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int ReadClasses( enum OpenOptions eOptions ) = 0;

    /**
     * @brief Create the file map for fast access to CAD objects
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int CreateFileMap() = 0;

    /**
     * @brief Read tables from CAD file
     * @param eOptions Read options
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int ReadTables( enum OpenOptions eOptions );

    /**
     * @brief returns value of flag Read Unsupported Geometries
     */
    bool isReadingUnsupportedGeometries();

protected:
    CADFileIO * pFileIO;
    CADHeader  oHeader;
    CADClasses oClasses;
    CADTables  oTables;

protected:
    std::map<long, long> mapObjects; // object index <-> file offset
    bool bReadingUnsupportedGeometries;

private:
    CADFile(CADFile&) = delete;
    CADFile& operator=(const CADFile&) = delete;
    CADFile(CADFile&&) = delete;
    CADFile& operator=(CADFile&&) = delete;
};

#endif // CADFILE_H
