/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFADictionary class for managing the
 *           dictionary read from the HFA file.  Most work done by the
 *           HFAType, and HFAField classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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

#include "hfa_p.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

static const char *apszDefDefn[] = {

    "Edsc_Table",
    "{1:lnumrows,}Edsc_Table",

    "Edsc_Column", 
    "{1:lnumRows,1:LcolumnDataPtr,1:e4:integer,real,complex,string,dataType,1:lmaxNumChars,}Edsc_Column", 
    
    "Eprj_Size",
    "{1:dwidth,1:dheight,}Eprj_Size",

    "Eprj_Coordinate",
    "{1:dx,1:dy,}Eprj_Coordinate",

    "Eprj_MapInfo", 
    "{0:pcproName,1:*oEprj_Coordinate,upperLeftCenter,1:*oEprj_Coordinate,lowerRightCenter,1:*oEprj_Size,pixelSize,0:pcunits,}Eprj_MapInfo",
    
    "Eimg_StatisticsParameters830", 
    "{0:poEmif_String,LayerNames,1:*bExcludedValues,1:oEmif_String,AOIname,1:lSkipFactorX,1:lSkipFactorY,1:*oEdsc_BinFunction,BinFunction,}Eimg_StatisticsParameters830",
    
    "Esta_Statistics", 
    "{1:dminimum,1:dmaximum,1:dmean,1:dmedian,1:dmode,1:dstddev,}Esta_Statistics",

    "Edsc_BinFunction", 
    "{1:lnumBins,1:e4:direct,linear,logarithmic,explicit,binFunctionType,1:dminLimit,1:dmaxLimit,1:*bbinLimits,}Edsc_BinFunction",

    "Eimg_NonInitializedValue", 
    "{1:*bvalueBD,}Eimg_NonInitializedValue",

    "Eprj_MapProjection842",
    "{1:x{1:x{0:pcstring,}Emif_String,type,1:x{0:pcstring,}Emif_String,MIFDictionary,0:pCMIFObject,}Emif_MIFObject,projection,1:x{0:pcstring,}Emif_String,title,}Eprj_MapProjection842",

    "Emif_MIFObject",
    "{1:x{0:pcstring,}Emif_String,type,1:x{0:pcstring,}Emif_String,MIFDictionary,0:pCMIFObject,}Emif_MIFObject",

    "Eprj_ProParameters",
    "{1:e2:EPRJ_INTERNAL,EPRJ_EXTERNAL,proType,1:lproNumber,0:pcproExeName,0:pcproName,1:lproZone,0:pdproParams,1:*oEprj_Spheroid,proSpheroid,}Eprj_ProParameters",
    
    "Eprj_Datum",
    "{0:pcdatumname,1:e3:EPRJ_DATUM_PARAMETRIC,EPRJ_DATUM_GRID,EPRJ_DATUM_REGRESSION,type,0:pdparams,0:pcgridname,}Eprj_Datum",

    "Eprj_Spheroid",
    "{0:pcsphereName,1:da,1:db,1:deSquared,1:dradius,}Eprj_Spheroid",

    NULL,
    NULL };
    
    

/************************************************************************/
/* ==================================================================== */
/*	                       HFADictionary                            */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                           HFADictionary()                            */
/************************************************************************/

HFADictionary::HFADictionary( const char * pszString )

{
    int		i;
    
    nTypes = 0;
    nTypesMax = 0;
    papoTypes = NULL;

    osDictionaryText = pszString;
    bDictionaryTextDirty = FALSE;

/* -------------------------------------------------------------------- */
/*      Read all the types.                                             */
/* -------------------------------------------------------------------- */
    while( pszString != NULL && *pszString != '.' )
    {
        HFAType		*poNewType;

        poNewType = new HFAType();
        pszString = poNewType->Initialize( pszString );

        if( pszString != NULL )
            AddType( poNewType );
        else
            delete poNewType;
    }

/* -------------------------------------------------------------------- */
/*      Complete the definitions.                                       */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nTypes; i++ )
    {
        papoTypes[i]->CompleteDefn( this );
    }
}

/************************************************************************/
/*                           ~HFADictionary()                           */
/************************************************************************/

HFADictionary::~HFADictionary()

{
    int		i;

    for( i = 0; i < nTypes; i++ )
        delete papoTypes[i];
    
    CPLFree( papoTypes );
}

/************************************************************************/
/*                              AddType()                               */
/************************************************************************/

void HFADictionary::AddType( HFAType *poType )

{
    if( nTypes == nTypesMax )
    {
        nTypesMax = nTypes * 2 + 10;
        papoTypes = (HFAType **) CPLRealloc( papoTypes,
                                             sizeof(void*) * nTypesMax );
    }

    papoTypes[nTypes++] = poType;
}

/************************************************************************/
/*                              FindType()                              */
/************************************************************************/

HFAType * HFADictionary::FindType( const char * pszName )

{
    int		i;

    for( i = 0; i < nTypes; i++ )
    {
        if( papoTypes[i]->pszTypeName != NULL &&
            strcmp(pszName,papoTypes[i]->pszTypeName) == 0 )
            return( papoTypes[i] );
    }

/* -------------------------------------------------------------------- */
/*      Check if this is a type have other knowledge of.  If so, add    */
/*      it to the dictionary now.  I'm not sure how some files end      */
/*      up being distributed using types not in the dictionary.         */
/* -------------------------------------------------------------------- */
    for( i = 0; apszDefDefn[i] != NULL; i += 2 )
    {
        if( strcmp( pszName, apszDefDefn[i] ) == 0 )
        {
            HFAType *poNewType = new HFAType();

            poNewType->Initialize( apszDefDefn[i+1] );
            AddType( poNewType );
            poNewType->CompleteDefn( this );

            if( osDictionaryText.size() > 0 )
                osDictionaryText.erase( osDictionaryText.size() - 1, 1 );
            osDictionaryText += apszDefDefn[i+1];
            osDictionaryText += ",.";

            bDictionaryTextDirty = TRUE;

            return poNewType;
        }
    }

    return NULL;
}

/************************************************************************/
/*                            GetItemSize()                             */
/*                                                                      */
/*      Get the size of a basic (atomic) item.                          */
/************************************************************************/

int HFADictionary::GetItemSize( char chType )

{
    switch( chType )
    {
      case '1':
      case '2':
      case '4':
      case 'c':
      case 'C':
        return 1;

      case 'e':
      case 's':
      case 'S':
        return 2;

      case 't':
      case 'l':
      case 'L':
      case 'f':
        return 4;

      case 'd':
      case 'm':
        return 8;

      case 'M':
        return 16;

      case 'b':
        return -1;
        
      case 'o':
      case 'x':
        return 0;

      default:
        CPLAssert( FALSE );
    }

    return 0;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void HFADictionary::Dump( FILE * fp )

{
    int		i;
    
    VSIFPrintf( fp, "\nHFADictionary:\n" );

    for( i = 0; i < nTypes; i++ )
    {
        papoTypes[i]->Dump( fp );
    }
}
