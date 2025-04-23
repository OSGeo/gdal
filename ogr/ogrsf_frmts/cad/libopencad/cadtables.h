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
#ifndef CADTABLES_H
#define CADTABLES_H

#include "cadheader.h"
#include "cadlayer.h"

class CADFile;

/**
 * @brief The CAD tables class. Store tables
 */
class OCAD_EXTERN CADTables
{
public:
    /**
     * @brief The CAD table types enum
     */
    enum TableType
    {
        CurrentViewportTable,
        BlocksTable,
        LayersTable,
        StyleTable,
        LineTypesTable,
        ViewTable,
        UCSTable,
        ViewportTable,
        APPIDTable,
        EntityTable,
        ACADGroupDict,
        ACADMLineStyleDict,
        NamedObjectsDict,
        LayoutsDict,
        PlotSettingsDict,
        PlotStylesDict,
        BlockRecordPaperSpace,
        BlockRecordModelSpace
    };
public:
    CADTables();

    void      AddTable( enum TableType eType, const CADHandle& hHandle );
    CADHandle GetTableHandle( enum TableType eType );
    int       ReadTable( CADFile * const pCADFile, enum TableType eType );
    size_t    GetLayerCount() const;
    CADLayer& GetLayer( size_t iIndex );

protected:
    int  ReadLayersTable( CADFile * const pCADFile, long dLayerControlHandle );
    void FillLayer( const CADEntityObject * pEntityObject );
protected:
    std::map<enum TableType, CADHandle> mapTables;
    std::vector<CADLayer>               aLayers;
};

#endif // CADTABLES_H
