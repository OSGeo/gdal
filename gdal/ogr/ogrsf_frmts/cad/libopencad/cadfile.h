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
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/

#ifndef CADFILE_H
#define CADFILE_H

#include "cadfileio.h"
#include "cadclasses.h"
#include "cadtables.h"
#include "caddictionary.h"

#include <string>

/**
 * @brief The abstact CAD file class
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
        READ_ALL,       /**< read all available information */
        READ_FAST,      /**< read some methadata */
        READ_FASTEST    /**< read only geometry and layers */
    };

public:
    CADFile (CADFileIO* poFileIO);
    virtual                 ~CADFile();

public:
    const CADHeader&        getHeader() const;
    const CADClasses&       getClasses() const;
    const CADTables&        getTables() const;

public:
    virtual int             parseFile(enum OpenOptions eOptions);
    virtual size_t          getLayersCount() const;
    virtual CADLayer&       getLayer(size_t index);

    /**
     * @brief returns NamedObjectDictionary (root) of all others dictionaries
     * @return pointer to the root CADDictionary
     */
    virtual CADDictionary   getNOD() = 0;

//    virtual size_t GetBlocksCount();
//    virtual CADBlockObject * GetBlock( size_t index );

protected:
    /**
     * @brief Get CAD Object from file
     * @param index Object index
     * @param bHandlesOnly set TRUE if object data should be skipped, and only object handles should be read.
     * @return pointer to CADObject or nullptr. User have to free returned pointer.
     */
    virtual CADObject *     getObject( long index, bool bHandlesOnly = false ) = 0;

    /**
     * @brief read geometry from CAD file
     * @param handle Handle of CAD object
     * @param handle Handle of BlockRef (0 if geometry is not in block reference)
     * @return NULL if failed or pointer which mast be feed by user
     */
    virtual CADGeometry *   getGeometry( long index, long blockrefhandle = 0 ) = 0;

    /**
     * @brief initially read some basic values and section locator
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int             readSectionLocator() = 0;

    /**
     * @brief Read header from CAD file
     * @param eOptions Read options
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int             readHeader(enum OpenOptions eOptions) = 0;

    /**
     * @brief Read classes from CAD file
     * @param eOptions Read options
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int             readClasses(enum OpenOptions eOptions) = 0;

    /**
     * @brief Create the file map for fast access to CAD objects
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int             createFileMap() = 0;

    /**
     * @brief Read tables from CAD file
     * @param eOptions Read options
     * @return CADErrorCodes::SUCCESS if OK, or error code
     */
    virtual int             readTables(enum OpenOptions eOptions);

protected:
    CADFileIO*              fileIO;
    CADHeader               header;
    CADClasses              classes;
    CADTables               tables;

protected:
    std::map<long, long>    objectsMap; // object index <-> file offset
};


#endif // CADFILE_H
