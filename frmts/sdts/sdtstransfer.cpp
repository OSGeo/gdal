/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSTransfer class.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/05/11 12:55:31  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/*                            SDTSTransfer()                            */
/************************************************************************/

SDTSTransfer::SDTSTransfer()

{
    nLayers = 0;
    panLayerCATDEntry = NULL;
}

/************************************************************************/
/*                           ~SDTSTransfer()                            */
/************************************************************************/

SDTSTransfer::~SDTSTransfer()

{
    if( nLayers > 0 )
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
    if( oCATD.GetModuleFilePath( "IREF" ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't find IREF module in transfer `%s'.\n",
                  pszFilename );
        return FALSE;
    }
    
    if( !oIREF.Read( oCATD.GetModuleFilePath( "IREF" ) ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Build an index of layer types we recognise and care about.      */
/* -------------------------------------------------------------------- */
    int	iCATDLayer;

    panLayerCATDEntry = (int *) CPLMalloc(sizeof(int) * oCATD.GetEntryCount());

    for( iCATDLayer = 0; iCATDLayer < oCATD.GetEntryCount(); iCATDLayer++ )
    {
        switch( oCATD.GetEntryType(iCATDLayer) )
        {
          case SLTPoint:
          case SLTLine:
          case SLTAttr:
            panLayerCATDEntry[nLayers++] = iCATDLayer;
            break;

          default:
            /* ignore */
            break;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

/**
 * Reinitialize this object, freeing all transfer specific resources.
 */

void SDTSTransfer::Close()

{
    CPLFree( panLayerCATDEntry );
    nLayers = 0;
}

/************************************************************************/
/*                            GetLayerType()                            */
/************************************************************************/

SDTSLayerType SDTSTransfer::GetLayerType( int iEntry )

{
    if( iEntry < 0 || iEntry >= nLayers )
        return SLTUnknown;

    return oCATD.GetEntryType( panLayerCATDEntry[iEntry] );
}

/************************************************************************/
/*                         GetLayerCATDEntry()                          */
/************************************************************************/

int SDTSTransfer::GetLayerCATDEntry( int iEntry )

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
    SDTSLineReader	*poLineReader;
    
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTLine )
    {
        return NULL;
    }

    
    poLineReader = new SDTSLineReader( &oIREF );
    
    if( !poLineReader->Open(
        		oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        delete poLineReader;
        return NULL;
    }
    else
    {
        return poLineReader;
    }
}

/************************************************************************/
/*                        GetLayerPointReader()                         */
/************************************************************************/

SDTSPointReader *SDTSTransfer::GetLayerPointReader( int iEntry )

{
    SDTSPointReader	*poPointReader;
    
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTPoint )
    {
        return NULL;
    }

    
    poPointReader = new SDTSPointReader( &oIREF );
    
    if( !poPointReader->Open(
        		oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        delete poPointReader;
        return NULL;
    }
    else
    {
        return poPointReader;
    }
}

/************************************************************************/
/*                         GetLayerAttrReader()                         */
/************************************************************************/

SDTSAttrReader *SDTSTransfer::GetLayerAttrReader( int iEntry )

{
    SDTSAttrReader	*poAttrReader;
    
    if( iEntry < 0
        || iEntry >= nLayers
        || oCATD.GetEntryType( panLayerCATDEntry[iEntry] ) != SLTAttr )
    {
        return NULL;
    }

    
    poAttrReader = new SDTSAttrReader( &oIREF );
    
    if( !poAttrReader->Open(
        		oCATD.GetEntryFilePath( panLayerCATDEntry[iEntry] ) ) )
    {
        delete poAttrReader;
        return NULL;
    }
    else
    {
        return poAttrReader;
    }
}
