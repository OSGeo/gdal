/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSTransfer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "sdts_al.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                            SDTSTransfer()                            */
/************************************************************************/

SDTSTransfer::SDTSTransfer() :
    nLayers(0),
    panLayerCATDEntry(nullptr),
    papoLayerReader(nullptr)
{}

/************************************************************************/
/*                           ~SDTSTransfer()                            */
/************************************************************************/

SDTSTransfer::~SDTSTransfer()

{
    Close();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/**
 * Open an SDTS transfer, and establish a list of data layers in the
 * transfer.
 *
 * @param pszFilename The name of the CATD file within the transfer.
 *
 * @return TRUE if the open success, or FALSE if it fails.
 */

int SDTSTransfer::Open( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Open the catalog.                                               */
/* -------------------------------------------------------------------- */
    if( !oCATD.Read( pszFilename ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read the IREF file.                                             */
/* -------------------------------------------------------------------- */
    if( oCATD.GetModuleFilePath( "IREF" ) == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't find IREF module in transfer `%s'.\n",
                  pszFilename );
        return FALSE;
    }

    if( !oIREF.Read( oCATD.GetModuleFilePath( "IREF" ) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read the XREF file.                                             */
/* -------------------------------------------------------------------- */
    if( oCATD.GetModuleFilePath( "XREF" ) == nullptr )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Can't find XREF module in transfer `%s'.\n",
                  pszFilename );
    }
    else if( !oXREF.Read( oCATD.GetModuleFilePath( "XREF" ) ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
              "Can't read XREF module, even though found in transfer `%s'.\n",
                  pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Build an index of layer types we recognise and care about.      */
/* -------------------------------------------------------------------- */
    panLayerCATDEntry = reinterpret_cast<int *>(
        CPLMalloc( sizeof(int) * oCATD.GetEntryCount() ) );

    for( int iCATDLayer = 0; iCATDLayer < oCATD.GetEntryCount(); iCATDLayer++ )
    {
        switch( oCATD.GetEntryType(iCATDLayer) )
        {
          case SLTPoint:
          case SLTLine:
          case SLTAttr:
          case SLTPoly:
          case SLTRaster:
            panLayerCATDEntry[nLayers++] = iCATDLayer;
            break;

          default:
            /* ignore */
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialized the related indexed readers list.                   */
/* -------------------------------------------------------------------- */
    papoLayerReader = reinterpret_cast<SDTSIndexedReader **>(
        CPLCalloc( sizeof(SDTSIndexedReader*), oCATD.GetEntryCount() ) );

    return TRUE;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSTransfer::Close()

{
    for( int i = 0; i < nLayers; i++ )
    {
        if( papoLayerReader[i] != nullptr )
            delete papoLayerReader[i];
    }
    CPLFree( papoLayerReader );
    papoLayerReader = nullptr;
    CPLFree( panLayerCATDEntry );
    panLayerCATDEntry = nullptr;
    nLayers = 0;
}

/************************************************************************/
/*                            GetLayerType()                            */
/************************************************************************/

/**
  Fetch type of requested feature layer.

  @param iEntry the index of the layer to fetch information on.  A value
  from zero to GetLayerCount()-1.

  @return the layer type.

  <ul>
  <li> SLTPoint: A point layer.  An SDTSPointReader is returned by
  SDTSTransfer::GetLayerIndexedReader().

  <li> SLTLine: A line layer.  An SDTSLineReader is returned by
  SDTSTransfer::GetLayerIndexedReader().

  <li> SLTAttr: An attribute primary or secondary layer.  An SDTSAttrReader
  is returned by SDTSTransfer::GetLayerIndexedReader().

  <li> SLTPoly: A polygon layer.  An SDTSPolygonReader is returned by
  SDTSTransfer::GetLayerIndexedReader().

  <li> SLTRaster: A raster layer.  SDTSTransfer::GetLayerIndexedReader()
  is not implemented.  Use SDTSTransfer::GetLayerRasterReader() instead.
  </ul>
 */

SDTSLayerType SDTSTransfer::GetLayerType( int iEntry ) const

{
    if( iEntry < 0 || iEntry >= nLayers )
        return SLTUnknown;

    return oCATD.GetEntryType( panLayerCATDEntry[iEntry] );
}

/************************************************************************/
/*                         GetLayerCATDEntry()                          */
/************************************************************************/

/**
  Fetch the CATD module index for a layer.   This can be used to fetch
  details about the layer/module from the SDTS_CATD object, such as its
  filename, and description.

  @param iEntry the layer index from 0 to GetLayerCount()-1.

  @return the module index suitable for use with the various SDTS_CATD
  methods.
 */

int SDTSTransfer::GetLayerCATDEntry( int iEntry ) const

{
    if( iEntry < 0 || iEntry >= nLayers )
        return -1;

    return panLayerCATDEntry[iEntry];
}

/************************************************************************/
/*                         GetLayerLineReader()                         */
/************************************************************************/

SDTSLineReader *SDTSTransfer::GetLayerLineReader( int iEntry )

{
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTLine )
    {
        return nullptr;
    }

    SDTSLineReader *poLineReader = new SDTSLineReader( &oIREF );

    if( !poLineReader->Open(
                        oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        oCATD.SetEntryTypeUnknown(iEntry) ; // to prevent further attempt
        delete poLineReader;
        return nullptr;
    }

    return poLineReader;
}

/************************************************************************/
/*                        GetLayerPointReader()                         */
/************************************************************************/

SDTSPointReader *SDTSTransfer::GetLayerPointReader( int iEntry )

{
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTPoint )
    {
        return nullptr;
    }

    SDTSPointReader *poPointReader = new SDTSPointReader( &oIREF );

    if( !poPointReader->Open(
                        oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        oCATD.SetEntryTypeUnknown(iEntry) ; // to prevent further attempt
        delete poPointReader;
        return nullptr;
    }

    return poPointReader;
}

/************************************************************************/
/*                       GetLayerPolygonReader()                        */
/************************************************************************/

SDTSPolygonReader *SDTSTransfer::GetLayerPolygonReader( int iEntry )

{
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTPoly )
    {
        return nullptr;
    }

    SDTSPolygonReader *poPolyReader = new SDTSPolygonReader();

    if( !poPolyReader->Open(
                        oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        oCATD.SetEntryTypeUnknown(iEntry) ; // to prevent further attempt
        delete poPolyReader;
        return nullptr;
    }

    return poPolyReader;
}

/************************************************************************/
/*                         GetLayerAttrReader()                         */
/************************************************************************/

SDTSAttrReader *SDTSTransfer::GetLayerAttrReader( int iEntry )

{
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTAttr )
    {
        return nullptr;
    }

    SDTSAttrReader *poAttrReader = new SDTSAttrReader();

    if( !poAttrReader->Open(
                        oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        oCATD.SetEntryTypeUnknown(iEntry) ; // to prevent further attempt
        delete poAttrReader;
        return nullptr;
    }

    return poAttrReader;
}

/************************************************************************/
/*                        GetLayerRasterReader()                        */
/************************************************************************/

/**
  Instantiate an SDTSRasterReader for the indicated layer.

  @param iEntry the index of the layer to instantiate a reader for.  A
  value between 0 and GetLayerCount()-1.

  @return a pointer to a new SDTSRasterReader object, or NULL if the method
  fails.

  NOTE: The reader returned from GetLayerRasterReader() becomes the
  responsibility of the caller to delete, and isn't automatically deleted
  when the SDTSTransfer is destroyed.  This method is different from
  the GetLayerIndexedReader() method in this regard.
  */

SDTSRasterReader *SDTSTransfer::GetLayerRasterReader( int iEntry )

{
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTRaster )
    {
        return nullptr;
    }

    SDTSRasterReader *poRasterReader = new SDTSRasterReader();

    if( !poRasterReader->Open( &oCATD, &oIREF,
                         oCATD.GetEntryModule(panLayerCATDEntry[iEntry] ) ) )
    {
        oCATD.SetEntryTypeUnknown(iEntry) ; // to prevent further attempt
        delete poRasterReader;
        return nullptr;
    }

    return poRasterReader;
}

/************************************************************************/
/*                        GetLayerModuleReader()                        */
/************************************************************************/

DDFModule *SDTSTransfer::GetLayerModuleReader( int iEntry )

{
    if( iEntry < 0 || iEntry >= nLayers )
    {
        return nullptr;
    }

    DDFModule *poModuleReader = new DDFModule;

    if( !poModuleReader->Open(
                        oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        oCATD.SetEntryTypeUnknown(iEntry) ; // to prevent further attempt
        delete poModuleReader;
        return nullptr;
    }

    return poModuleReader;
}

/************************************************************************/
/*                       GetLayerIndexedReader()                        */
/************************************************************************/

/**
  Returns a pointer to a reader of the appropriate type to the requested
  layer.

  Notes:
  <ul>
  <li> The returned reader remains owned by the SDTSTransfer, and will be
  destroyed when the SDTSTransfer is destroyed.  It should not be
  destroyed by the application.

  <li> If an indexed reader was already created for this layer using
  GetLayerIndexedReader(), it will be returned instead of creating a new
  reader.  Among other things this means that the returned reader may not
  be positioned to read from the beginning of the module, and may already
  have its index filled.

  <li> The returned reader will be of a type appropriate to the layer.
  See SDTSTransfer::GetLayerType() to see what reader classes correspond
  to what layer types, so it can be cast accordingly (if necessary).
  </ul>

  @param iEntry the index of the layer to instantiate a reader for.  A
  value between 0 and GetLayerCount()-1.

  @return a pointer to an appropriate reader or NULL if the method fails.
  */

SDTSIndexedReader *SDTSTransfer::GetLayerIndexedReader( int iEntry )

{
    if( papoLayerReader[iEntry] == nullptr )
    {
        switch( oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) )
        {
          case SLTAttr:
            papoLayerReader[iEntry] = GetLayerAttrReader( iEntry );
            break;

          case SLTPoint:
            papoLayerReader[iEntry] = GetLayerPointReader( iEntry );
            break;

          case SLTLine:
            papoLayerReader[iEntry] = GetLayerLineReader( iEntry );
            break;

          case SLTPoly:
            papoLayerReader[iEntry] = GetLayerPolygonReader( iEntry );
            break;

          default:
            break;
        }
    }

    return papoLayerReader[iEntry];
}

/************************************************************************/
/*                             FindLayer()                              */
/************************************************************************/

/**
  Fetch the SDTSTransfer layer number corresponding to a module name.

  @param pszModule the name of the module to search for, such as "PC01".

  @return the layer number (between 0 and GetLayerCount()-1 corresponding to
  the module, or -1 if it doesn't correspond to a layer.
  */

int SDTSTransfer::FindLayer( const char * pszModule )

{
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszModule,
                  oCATD.GetEntryModule( panLayerCATDEntry[iLayer] ) ) )
        {
            return iLayer;
        }
    }

    return -1;
}

/************************************************************************/
/*                        GetIndexedFeatureRef()                        */
/************************************************************************/

SDTSFeature *SDTSTransfer::GetIndexedFeatureRef( SDTSModId *poModId,
                                                 SDTSLayerType *peType )

{
/* -------------------------------------------------------------------- */
/*      Find the desired layer ... this is likely a significant slow    */
/*      point in the whole process ... perhaps the last found could     */
/*      be cached or something.                                         */
/* -------------------------------------------------------------------- */
    const int iLayer = FindLayer( poModId->szModule );
    if( iLayer == -1 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Get the reader, and read a feature from it.                     */
/* -------------------------------------------------------------------- */
    SDTSIndexedReader *poReader = GetLayerIndexedReader( iLayer );
    if( poReader == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      return type, if requested.                                      */
/* -------------------------------------------------------------------- */
    if( peType != nullptr )
        *peType = GetLayerType(iLayer);

    return poReader->GetIndexedFeatureRef( poModId->nRecord );
}

/************************************************************************/
/*                              GetAttr()                               */
/*                                                                      */
/*      Fetch the attribute information corresponding to a given        */
/*      SDTSModId.                                                      */
/************************************************************************/

/**
  Fetch the attribute fields given a particular module/record id.

  @param poModId an attribute record identifier, normally taken from the
  aoATID[] array of an SDTSIndexedFeature.

  @return a pointer to the DDFField containing the user attribute values as
  subfields.
  */

DDFField *SDTSTransfer::GetAttr( SDTSModId *poModId )

{
    SDTSAttrRecord *poAttrRecord = dynamic_cast<SDTSAttrRecord *>(
        GetIndexedFeatureRef( poModId ) );

    if( poAttrRecord == nullptr )
        return nullptr;

    return poAttrRecord->poATTR;
}

/************************************************************************/
/*                             GetBounds()                              */
/************************************************************************/

/**
  Fetch approximate bounds for a transfer by scanning all point layers
  and raster layers.

  For TVP datasets (where point layers are scanned) the results can, in
  theory miss some lines that go outside the bounds of the point layers.
  However, this isn't common since most TVP sets contain a bounding rectangle
  whose corners will define the most extreme extents.

  @param pdfMinX western edge of dataset
  @param pdfMinY southern edge of dataset
  @param pdfMaxX eastern edge of dataset
  @param pdfMaxY northern edge of dataset

  @return TRUE if success, or FALSE on a failure.
  */

int SDTSTransfer::GetBounds( double *pdfMinX, double *pdfMinY,
                             double *pdfMaxX, double *pdfMaxY )

{
    bool bFirst = true;

    for( int iLayer = 0; iLayer < GetLayerCount(); iLayer++ )
    {
        if( GetLayerType( iLayer ) == SLTPoint )
        {

            SDTSPointReader *poLayer = reinterpret_cast<SDTSPointReader *>(
                GetLayerIndexedReader( iLayer ) );
            if( poLayer == nullptr )
                continue;

            poLayer->Rewind();

            SDTSRawPoint *poPoint = nullptr;
            while( (poPoint = reinterpret_cast<SDTSRawPoint *>(
                      poLayer->GetNextFeature() ) ) != nullptr )
            {
                if( bFirst )
                {
                    *pdfMinX = poPoint->dfX;
                    *pdfMaxX = poPoint->dfX;
                    *pdfMinY = poPoint->dfY;
                    *pdfMaxY = poPoint->dfY;
                    bFirst = false;
                }
                else
                {
                    *pdfMinX = std::min( *pdfMinX, poPoint->dfX );
                    *pdfMaxX = std::max( *pdfMaxX, poPoint->dfX );
                    *pdfMinY = std::min( *pdfMinY, poPoint->dfY );
                    *pdfMaxY = std::max( *pdfMaxY, poPoint->dfY );
                }

                if( !poLayer->IsIndexed() )
                    delete poPoint;
            }
        }
        else if( GetLayerType( iLayer ) == SLTRaster )
        {
            SDTSRasterReader *poRL = GetLayerRasterReader( iLayer );
            if( poRL == nullptr )
                continue;

            double adfGeoTransform[6];
            poRL->GetTransform( adfGeoTransform );

            const double dfMinX = adfGeoTransform[0];
            const double dfMaxY = adfGeoTransform[3];
            const double dfMaxX = adfGeoTransform[0]
                + poRL->GetXSize() * adfGeoTransform[1];
            const double dfMinY = adfGeoTransform[3]
                + poRL->GetYSize() * adfGeoTransform[5];

            if( bFirst )
            {
                *pdfMinX = dfMinX;
                *pdfMaxX = dfMaxX;
                *pdfMinY = dfMinY;
                *pdfMaxY = dfMaxY;
                bFirst = false;
            }
            else
            {
                *pdfMinX = std::min( dfMinX, *pdfMinX );
                *pdfMaxX = std::max( dfMaxX, *pdfMaxX );
                *pdfMinY = std::min( dfMinY, *pdfMinY );
                *pdfMaxY = std::max( dfMaxY, *pdfMaxY );
            }

            delete poRL;
        }
    }

    return !bFirst;
}
