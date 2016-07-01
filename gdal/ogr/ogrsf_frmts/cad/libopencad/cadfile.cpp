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


#include "cadfile.h"
#include "opencad_api.h"

#include <iostream>

CADFile::CADFile(CADFileIO* poFileIO)
{
    fileIO = poFileIO;
}

CADFile::~CADFile()
{
    if(nullptr != fileIO)
        delete fileIO;
}

const CADHeader& CADFile::getHeader() const
{
    return header;
}

const CADClasses& CADFile::getClasses() const
{
    return classes;
}

const CADTables &CADFile::getTables() const
{
    return tables;
}

int CADFile::parseFile(enum OpenOptions eOptions)
{
    if(nullptr == fileIO)
        return CADErrorCodes::FILE_OPEN_FAILED;

    if(!fileIO->IsOpened())
    {
        if(!fileIO->Open(CADFileIO::read | CADFileIO::binary))
            return CADErrorCodes::FILE_OPEN_FAILED;
    }

    int nResultCode;

    nResultCode = readSectionLocator ();
    if(nResultCode != CADErrorCodes::SUCCESS)
        return nResultCode;
    nResultCode = readHeader (eOptions);
    if(nResultCode != CADErrorCodes::SUCCESS)
        return nResultCode;
    nResultCode = readClasses (eOptions);
    if(nResultCode != CADErrorCodes::SUCCESS)
        return nResultCode;
    nResultCode = createFileMap ();
    if(nResultCode != CADErrorCodes::SUCCESS)
        return nResultCode;
    nResultCode = readTables (eOptions);
    if(nResultCode != CADErrorCodes::SUCCESS)
        return nResultCode;

    return CADErrorCodes::SUCCESS;
}

int CADFile::readTables(CADFile::OpenOptions /*eOptions*/)
{
    // TODO: read other tables in ALL option mode

    int nResult = tables.readTable(this, CADTables::LayersTable);
//    if(nResult != CADErrorCodes::SUCCESS)
        return nResult;

}

size_t CADFile::getLayersCount() const
{
    return tables.getLayerCount ();
}

CADLayer &CADFile::getLayer(size_t index)
{
    return tables.getLayer (index);
}
