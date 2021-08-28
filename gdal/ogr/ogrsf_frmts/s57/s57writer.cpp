/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57Writer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "s57.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             S57Writer()                              */
/************************************************************************/

S57Writer::S57Writer() :
    poModule(nullptr),
    nNext0001Index(0),
    poRegistrar(nullptr),
    poClassContentExplorer(nullptr),
    m_nCOMF(nDEFAULT_COMF),
    m_nSOMF(nDEFAULT_SOMF)
{}

/************************************************************************/
/*                             ~S57Writer()                             */
/************************************************************************/

S57Writer::~S57Writer()

{
    Close();
}

/************************************************************************/
/*                               Close()                                */
/*                                                                      */
/*      Close the current S-57 dataset.                                 */
/************************************************************************/

bool S57Writer::Close()

{
    if( poModule != nullptr )
    {
        poModule->Close();
        delete poModule;
        poModule = nullptr;
    }
    return true;
}

/************************************************************************/
/*                           CreateS57File()                            */
/*                                                                      */
/*      Create a new output ISO8211 file with all the S-57 data         */
/*      definitions.                                                    */
/************************************************************************/

bool S57Writer::CreateS57File( const char *pszFilename )

{
    // TODO: What was oModule for if it was unused?
    // DDFModule  oModule;
    Close();

    nNext0001Index = 1;

/* -------------------------------------------------------------------- */
/*      Create and initialize new module.                               */
/* -------------------------------------------------------------------- */
    poModule = new DDFModule();
    poModule->Initialize();

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    DDFFieldDefn *poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0000", "",
                     "0001DSIDDSIDDSSI0001DSPM0001VRIDVRIDATTVVRIDVRPCVRID"
                     "VRPTVRIDSGCCVRIDSG2DVRIDSG3D0001FRIDFRIDFOIDFRIDATTF"
                     "FRIDNATFFRIDFFPCFRIDFFPTFRIDFSPCFRIDFSPT",
                     dsc_elementary,
                     dtc_char_string );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the '0001' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0001", "ISO 8211 Record Identifier", "",
                     dsc_elementary, dtc_bit_string,
                     "(b12)" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSID", "Data set identification field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "EXPP", "b11" );
    poFDefn->AddSubfield( "INTU", "b11" );
    poFDefn->AddSubfield( "DSNM", "A" );
    poFDefn->AddSubfield( "EDTN", "A" );
    poFDefn->AddSubfield( "UPDN", "A" );
    poFDefn->AddSubfield( "UADT", "A(8)" );
    poFDefn->AddSubfield( "ISDT", "A(8)" );
    poFDefn->AddSubfield( "STED", "R(4)" );
    poFDefn->AddSubfield( "PRSP", "b11" );
    poFDefn->AddSubfield( "PSDN", "A" );
    poFDefn->AddSubfield( "PRED", "A" );
    poFDefn->AddSubfield( "PROF", "b11" );
    poFDefn->AddSubfield( "AGEN", "b12" );
    poFDefn->AddSubfield( "COMT", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSSI field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSSI", "Data set structure information field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "DSTR", "b11" );
    poFDefn->AddSubfield( "AALL", "b11" );
    poFDefn->AddSubfield( "NALL", "b11" );
    poFDefn->AddSubfield( "NOMR", "b14" );
    poFDefn->AddSubfield( "NOCR", "b14" );
    poFDefn->AddSubfield( "NOGR", "b14" );
    poFDefn->AddSubfield( "NOLR", "b14" );
    poFDefn->AddSubfield( "NOIN", "b14" );
    poFDefn->AddSubfield( "NOCN", "b14" );
    poFDefn->AddSubfield( "NOED", "b14" );
    poFDefn->AddSubfield( "NOFA", "b14" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSPM field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSPM", "Data set parameter field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "HDAT", "b11" );
    poFDefn->AddSubfield( "VDAT", "b11" );
    poFDefn->AddSubfield( "SDAT", "b11" );
    poFDefn->AddSubfield( "CSCL", "b14" );
    poFDefn->AddSubfield( "DUNI", "b11" );
    poFDefn->AddSubfield( "HUNI", "b11" );
    poFDefn->AddSubfield( "PUNI", "b11" );
    poFDefn->AddSubfield( "COUN", "b11" );
    poFDefn->AddSubfield( "COMF", "b14" );
    poFDefn->AddSubfield( "SOMF", "b14" );
    poFDefn->AddSubfield( "COMT", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the VRID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "VRID", "Vector record identifier field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "RVER", "b12" );
    poFDefn->AddSubfield( "RUIN", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the VRPC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "VRPC", "Vector Record Pointer Control field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "VPUI", "b11" );
    poFDefn->AddSubfield( "VPIX", "b12" );
    poFDefn->AddSubfield( "NVPT", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the VRPT field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "VRPT", "Vector record pointer field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "NAME", "B(40)" );
    poFDefn->AddSubfield( "ORNT", "b11" );
    poFDefn->AddSubfield( "USAG", "b11" );
    poFDefn->AddSubfield( "TOPI", "b11" );
    poFDefn->AddSubfield( "MASK", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the ATTV field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "ATTV", "Vector record attribute field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SGCC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SGCC", "Coordinate Control Field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "CCUI", "b11" );
    poFDefn->AddSubfield( "CCIX", "b12" );
    poFDefn->AddSubfield( "CCNC", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG2D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG2D", "2-D coordinate field", "*",
                     dsc_array, dtc_bit_string );

    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG3D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG3D", "3-D coordinate (sounding array) field", "*",
                     dsc_array, dtc_bit_string );

    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );
    poFDefn->AddSubfield( "VE3D", "b24" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FRID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FRID", "Feature record identifier field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "PRIM", "b11" );
    poFDefn->AddSubfield( "GRUP", "b11" );
    poFDefn->AddSubfield( "OBJL", "b12" );
    poFDefn->AddSubfield( "RVER", "b12" );
    poFDefn->AddSubfield( "RUIN", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FOID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FOID", "Feature object identifier field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "AGEN", "b12" );
    poFDefn->AddSubfield( "FIDN", "b14" );
    poFDefn->AddSubfield( "FIDS", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the ATTF field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "ATTF", "Feature record attribute field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the NATF field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "NATF", "Feature record national attribute field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FFPC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FFPC",
                     "Feature record to feature object pointer control field",
                     "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "FFUI", "b11" );
    poFDefn->AddSubfield( "FFIX", "b12" );
    poFDefn->AddSubfield( "NFPT", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FFPT field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FFPT", "Feature record to feature object pointer field",
                     "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "LNAM", "B(64)" );
    poFDefn->AddSubfield( "RIND", "b11" );
    poFDefn->AddSubfield( "COMT", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FSPC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FSPC",
                     "Feature record to spatial record pointer control field",
                     "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "FSUI", "b11" );
    poFDefn->AddSubfield( "FSIX", "b12" );
    poFDefn->AddSubfield( "NSPT", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FSPT field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FSPT", "Feature record to spatial record pointer field",
                     "*", dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "NAME", "B(40)" );
    poFDefn->AddSubfield( "ORNT", "b11" );
    poFDefn->AddSubfield( "USAG", "b11" );
    poFDefn->AddSubfield( "MASK", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    if( !poModule->Create( pszFilename ) )
    {
        delete poModule;
        poModule = nullptr;
        return false;
    }

    return true;
}

/************************************************************************/
/*                             WriteDSID()                              */
/************************************************************************/

bool S57Writer::WriteDSID( int nEXPP, int nINTU,
                           const char *pszDSNM, const char *pszEDTN,
                           const char *pszUPDN, const char *pszUADT,
                           const char *pszISDT, const char *pszSTED,
                           int nAGEN, const char *pszCOMT,
                           int nAALL,
                           int nNALL,
                           int nNOMR, int nNOGR, int nNOLR, int nNOIN,
                           int nNOCN, int nNOED )

{
/* -------------------------------------------------------------------- */
/*      Default values.                                                 */
/* -------------------------------------------------------------------- */
    if( pszDSNM == nullptr )
        pszDSNM = "";
    if( pszEDTN == nullptr )
        pszEDTN = "2";
    if( pszUPDN == nullptr )
        pszUPDN = "0";
    if( pszISDT == nullptr )
        pszISDT = "20030801";
    if( pszUADT == nullptr )
        pszUADT = pszISDT;
    if( pszSTED == nullptr )
        pszSTED = "03.1";
    if( pszCOMT == nullptr )
        pszCOMT = "";

/* -------------------------------------------------------------------- */
/*      Add the DSID field.                                             */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();

    // DDFField *poField =
    poRec->AddField( poModule->FindFieldDefn( "DSID" ) );

    poRec->SetIntSubfield   ( "DSID", 0, "RCNM", 0, 10 );
    poRec->SetIntSubfield   ( "DSID", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "EXPP", 0, nEXPP );
    poRec->SetIntSubfield   ( "DSID", 0, "INTU", 0, nINTU );
    poRec->SetStringSubfield( "DSID", 0, "DSNM", 0, pszDSNM );
    poRec->SetStringSubfield( "DSID", 0, "EDTN", 0, pszEDTN );
    poRec->SetStringSubfield( "DSID", 0, "UPDN", 0, pszUPDN );
    poRec->SetStringSubfield( "DSID", 0, "UADT", 0, pszUADT );
    poRec->SetStringSubfield( "DSID", 0, "ISDT", 0, pszISDT );
    poRec->SetStringSubfield( "DSID", 0, "STED", 0, pszSTED );
    poRec->SetIntSubfield   ( "DSID", 0, "PRSP", 0, 1 );
    poRec->SetStringSubfield( "DSID", 0, "PSDN", 0, "" );
    poRec->SetStringSubfield( "DSID", 0, "PRED", 0, "2.0" );
    poRec->SetIntSubfield   ( "DSID", 0, "PROF", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "AGEN", 0, nAGEN );
    poRec->SetStringSubfield( "DSID", 0, "COMT", 0, pszCOMT );

/* -------------------------------------------------------------------- */
/*      Add the DSSI record.  Eventually we will need to return and     */
/*      correct these when we are finished writing.                     */
/* -------------------------------------------------------------------- */
    /* poField = */ poRec->AddField( poModule->FindFieldDefn( "DSSI" ) );

    poRec->SetIntSubfield   ( "DSSI", 0, "DSTR", 0, 2 ); // "Chain node"
    poRec->SetIntSubfield   ( "DSSI", 0, "AALL", 0, nAALL );
    poRec->SetIntSubfield   ( "DSSI", 0, "NALL", 0, nNALL );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOMR", 0, nNOMR ); // Meta records
    // Cartographic records are not permitted in ENC.
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCR", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOGR", 0, nNOGR ); // Geo records
    // Collection records.
    poRec->SetIntSubfield   ( "DSSI", 0, "NOLR", 0, nNOLR );
    // Isolated node records.
    poRec->SetIntSubfield   ( "DSSI", 0, "NOIN", 0, nNOIN );
    // Connected node records.
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCN", 0, nNOCN );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOED", 0, nNOED ); // Edge records
    // Face are not permitted in chain node structure.
    poRec->SetIntSubfield   ( "DSSI", 0, "NOFA", 0, 0 );

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return true;
}

/************************************************************************/
/*                             WriteDSPM()                              */
/************************************************************************/

bool S57Writer::WriteDSPM( int nHDAT, int nVDAT, int nSDAT, int nCSCL,
                           int nCOMF, int nSOMF )

{
    m_nCOMF = nCOMF;
    m_nSOMF = nSOMF;

/* -------------------------------------------------------------------- */
/*      Add the DSID field.                                             */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();

    // DDFField *poField =
    poRec->AddField( poModule->FindFieldDefn( "DSPM" ) );

    poRec->SetIntSubfield   ( "DSPM", 0, "RCNM", 0, 20 );
    poRec->SetIntSubfield   ( "DSPM", 0, "RCID", 0, 1 );
    // Must be 2 for ENC.
    poRec->SetIntSubfield   ( "DSPM", 0, "HDAT", 0, nHDAT );
    poRec->SetIntSubfield   ( "DSPM", 0, "VDAT", 0, nVDAT );
    poRec->SetIntSubfield   ( "DSPM", 0, "SDAT", 0, nSDAT );
    poRec->SetIntSubfield   ( "DSPM", 0, "CSCL", 0, nCSCL );
    poRec->SetIntSubfield   ( "DSPM", 0, "DUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "HUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "PUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "COUN", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "COMF", 0, nCOMF );
    poRec->SetIntSubfield   ( "DSPM", 0, "SOMF", 0, nSOMF );

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return true;
}

/************************************************************************/
/*                             MakeRecord()                             */
/*                                                                      */
/*      Create a new empty record, and append a 0001 field with a       */
/*      properly set record index in it.                                */
/************************************************************************/

DDFRecord *S57Writer::MakeRecord()

{
    unsigned char abyData[2] = {
        static_cast<unsigned char>( nNext0001Index % 256 ),
        static_cast<unsigned char>( nNext0001Index / 256 )
    };

    DDFRecord *poRec = new DDFRecord( poModule );
    DDFField *poField = poRec->AddField( poModule->FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, (const char *) abyData, 2 );

    nNext0001Index++;

    return poRec;
}

/************************************************************************/
/*                           WriteGeometry()                            */
/************************************************************************/

bool S57Writer::WriteGeometry( DDFRecord *poRec, int nVertCount,
                               const double *padfX, const double *padfY, const double *padfZ )

{
    const char *pszFieldName = "SG2D";

    if( padfZ != nullptr )
        pszFieldName = "SG3D";

    DDFField *poField =
        poRec->AddField( poModule->FindFieldDefn( pszFieldName ) );

    const int nRawDataSize = padfZ
        ? 12 * nVertCount :
        8 * nVertCount;

    unsigned char *pabyRawData =
        static_cast<unsigned char *>( CPLMalloc(nRawDataSize) );

    for( int i = 0; i < nVertCount; i++ )
    {
        const GInt32 nXCOO = CPL_LSBWORD32(
            static_cast<GInt32>( floor(padfX[i] * m_nCOMF + 0.5)) );
        const GInt32 nYCOO = CPL_LSBWORD32(
            static_cast<GInt32>( floor(padfY[i] * m_nCOMF + 0.5)) );

        if( padfZ == nullptr )
        {
            memcpy( pabyRawData + i * 8, &nYCOO, 4 );
            memcpy( pabyRawData + i * 8 + 4, &nXCOO, 4 );
        }
        else
        {
            const GInt32 nVE3D = CPL_LSBWORD32(
                static_cast<GInt32>( floor( padfZ[i] * m_nSOMF + 0.5 )) );
            memcpy( pabyRawData + i * 12, &nYCOO, 4 );
            memcpy( pabyRawData + i * 12 + 4, &nXCOO, 4 );
            memcpy( pabyRawData + i * 12 + 8, &nVE3D, 4 );
        }
    }

    const bool nSuccess = CPL_TO_BOOL(poRec->SetFieldRaw(
        poField, 0,
        reinterpret_cast<const char *>( pabyRawData ), nRawDataSize ));

    CPLFree( pabyRawData );

    return nSuccess;
}

/************************************************************************/
/*                           WritePrimitive()                           */
/************************************************************************/

bool S57Writer::WritePrimitive( OGRFeature *poFeature )

{
    DDFRecord *poRec = MakeRecord();
    const OGRGeometry *poGeom = poFeature->GetGeometryRef();

/* -------------------------------------------------------------------- */
/*      Add the VRID field.                                             */
/* -------------------------------------------------------------------- */

    // DDFField *poField =
    poRec->AddField( poModule->FindFieldDefn( "VRID" ) );

    poRec->SetIntSubfield   ( "VRID", 0, "RCNM", 0,
                              poFeature->GetFieldAsInteger( "RCNM") );
    poRec->SetIntSubfield   ( "VRID", 0, "RCID", 0,
                              poFeature->GetFieldAsInteger( "RCID") );
    poRec->SetIntSubfield   ( "VRID", 0, "RVER", 0, 1 );
    poRec->SetIntSubfield   ( "VRID", 0, "RUIN", 0, 1 );

/* -------------------------------------------------------------------- */
/*      Handle simple point.                                            */
/* -------------------------------------------------------------------- */
    if( poGeom != nullptr && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        const OGRPoint *poPoint = poGeom->toPoint();

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VI
                   || poFeature->GetFieldAsInteger( "RCNM") == RCNM_VC );

        const double adfX[1] = { poPoint->getX() };
        const double adfY[1] = { poPoint->getY() };
        const double adfZ[1] = { poPoint->getZ() };

        if( adfZ[0] == 0.0 )
            WriteGeometry( poRec, 1, &adfX[0], &adfY[0], nullptr );
        else
            WriteGeometry( poRec, 1, &adfX[0], &adfY[0], &adfZ[0] );
    }

/* -------------------------------------------------------------------- */
/*      For multipoints we assuming SOUNDG, and write out as SG3D.      */
/* -------------------------------------------------------------------- */
    else if( poGeom != nullptr
             && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
    {
        const OGRMultiPoint *poMP = poGeom->toMultiPoint();
        const int nVCount = poMP->getNumGeometries();

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VI
                   || poFeature->GetFieldAsInteger( "RCNM") == RCNM_VC );

        double *padfX = (double *) CPLMalloc(sizeof(double) * nVCount);
        double *padfY = (double *) CPLMalloc(sizeof(double) * nVCount);
        double *padfZ = (double *) CPLMalloc(sizeof(double) * nVCount);

        for( int i = 0; i < nVCount; i++ )
        {
            const OGRPoint *poPoint = poMP->getGeometryRef( i );
            padfX[i] = poPoint->getX();
            padfY[i] = poPoint->getY();
            padfZ[i] = poPoint->getZ();
        }

        WriteGeometry( poRec, nVCount, padfX, padfY, padfZ );

        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );
    }

/* -------------------------------------------------------------------- */
/*      Handle LINESTRINGs (edge) geometry.                             */
/* -------------------------------------------------------------------- */
    else if( poGeom != nullptr
             && wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        const OGRLineString *poLS = poGeom->toLineString();
        const int nVCount = poLS->getNumPoints();

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VE );

        double *padfX = (double *) CPLMalloc(sizeof(double) * nVCount);
        double *padfY = (double *) CPLMalloc(sizeof(double) * nVCount);

        for( int i = 0; i < nVCount; i++ )
        {
            padfX[i] = poLS->getX(i);
            padfY[i] = poLS->getY(i);
        }

        if (nVCount)
            WriteGeometry( poRec, nVCount, padfX, padfY, nullptr );

        CPLFree( padfX );
        CPLFree( padfY );
    }

/* -------------------------------------------------------------------- */
/*      edge node linkages.                                             */
/* -------------------------------------------------------------------- */
    if( poFeature->GetDefnRef()->GetFieldIndex( "NAME_RCNM_0" ) >= 0 )
    {
        CPLAssert( poFeature->GetFieldAsInteger( "NAME_RCNM_0") == RCNM_VC );

        // DDFField *poField =
        poRec->AddField( poModule->FindFieldDefn( "VRPT" ) );

        const int nRCID0 = poFeature->GetFieldAsInteger( "NAME_RCID_0");
        char szName0[5] = {
            RCNM_VC,
            static_cast<char>(nRCID0 & 0xff),
            static_cast<char>((nRCID0 & 0xff00) >> 8),
            static_cast<char>((nRCID0 & 0xff0000) >> 16),
            static_cast<char>((nRCID0 & 0xff000000) >> 24)
        };

        poRec->SetStringSubfield( "VRPT", 0, "NAME", 0, szName0, 5 );
        poRec->SetIntSubfield   ( "VRPT", 0, "ORNT", 0,
                                  poFeature->GetFieldAsInteger( "ORNT_0") );
        poRec->SetIntSubfield   ( "VRPT", 0, "USAG", 0,
                                  poFeature->GetFieldAsInteger( "USAG_0") );
        poRec->SetIntSubfield   ( "VRPT", 0, "TOPI", 0,
                                  poFeature->GetFieldAsInteger( "TOPI_0") );
        poRec->SetIntSubfield   ( "VRPT", 0, "MASK", 0,
                                  poFeature->GetFieldAsInteger( "MASK_0") );

        const int nRCID1 = poFeature->GetFieldAsInteger( "NAME_RCID_1");
        const char szName1[5] = {
            RCNM_VC,
            static_cast<char>(nRCID1 & 0xff),
            static_cast<char>((nRCID1 & 0xff00) >> 8),
            static_cast<char>((nRCID1 & 0xff0000) >> 16),
            static_cast<char>((nRCID1 & 0xff000000) >> 24)
        };

        poRec->SetStringSubfield( "VRPT", 0, "NAME", 1, szName1, 5 );
        poRec->SetIntSubfield   ( "VRPT", 0, "ORNT", 1,
                                  poFeature->GetFieldAsInteger( "ORNT_1") );
        poRec->SetIntSubfield   ( "VRPT", 0, "USAG", 1,
                                  poFeature->GetFieldAsInteger( "USAG_1") );
        poRec->SetIntSubfield   ( "VRPT", 0, "TOPI", 1,
                                  poFeature->GetFieldAsInteger( "TOPI_1") );
        poRec->SetIntSubfield   ( "VRPT", 0, "MASK", 1,
                                  poFeature->GetFieldAsInteger( "MASK_1") );
    }

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return true;
}

/************************************************************************/
/*                             GetHEXChar()                             */
/************************************************************************/

static char GetHEXChar( const char *pszSrcHEXString )

{
    if( pszSrcHEXString[0] == '\0' || pszSrcHEXString[1] == '\0' )
        return (char) 0;

    int nResult = 0;

    if( pszSrcHEXString[0] >= '0' && pszSrcHEXString[0] <= '9' )
        nResult += (pszSrcHEXString[0] - '0') * 16;
    else if( pszSrcHEXString[0] >= 'a' && pszSrcHEXString[0] <= 'f' )
        nResult += (pszSrcHEXString[0] - 'a' + 10) * 16;
    else if( pszSrcHEXString[0] >= 'A' && pszSrcHEXString[0] <= 'F' )
        nResult += (pszSrcHEXString[0] - 'A' + 10) * 16;

    if( pszSrcHEXString[1] >= '0' && pszSrcHEXString[1] <= '9' )
        nResult += pszSrcHEXString[1] - '0';
    else if( pszSrcHEXString[1] >= 'a' && pszSrcHEXString[1] <= 'f' )
        nResult += pszSrcHEXString[1] - 'a' + 10;
    else if( pszSrcHEXString[1] >= 'A' && pszSrcHEXString[1] <= 'F' )
        nResult += pszSrcHEXString[1] - 'A' + 10;

    return (char) nResult;
}

/************************************************************************/
/*                        WriteCompleteFeature()                        */
/************************************************************************/

bool S57Writer::WriteCompleteFeature( OGRFeature *poFeature )

{
    OGRFeatureDefn *poFDefn = poFeature->GetDefnRef();

/* -------------------------------------------------------------------- */
/*      We handle primitives in a separate method.                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(poFDefn->GetName(),OGRN_VI)
        || EQUAL(poFDefn->GetName(),OGRN_VC)
        || EQUAL(poFDefn->GetName(),OGRN_VE) )
        return WritePrimitive( poFeature );

/* -------------------------------------------------------------------- */
/*      Create the record.                                              */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();

/* -------------------------------------------------------------------- */
/*      Add the FRID.                                                   */
/* -------------------------------------------------------------------- */
    // DDFField *poField =
    poRec->AddField( poModule->FindFieldDefn( "FRID" ) );

    poRec->SetIntSubfield   ( "FRID", 0, "RCNM", 0, 100 );
    poRec->SetIntSubfield   ( "FRID", 0, "RCID", 0,
                              poFeature->GetFieldAsInteger( "RCID" ) );
    poRec->SetIntSubfield   ( "FRID", 0, "PRIM", 0,
                              poFeature->GetFieldAsInteger( "PRIM" ) );
    poRec->SetIntSubfield   ( "FRID", 0, "GRUP", 0,
                              poFeature->GetFieldAsInteger( "GRUP") );
    poRec->SetIntSubfield   ( "FRID", 0, "OBJL", 0,
                              poFeature->GetFieldAsInteger( "OBJL") );
    poRec->SetIntSubfield   ( "FRID", 0, "RVER", 0, 1 ); /* always new insert*/
    poRec->SetIntSubfield   ( "FRID", 0, "RUIN", 0, 1 );

/* -------------------------------------------------------------------- */
/*      Add the FOID                                                    */
/* -------------------------------------------------------------------- */
    /*poField = */poRec->AddField( poModule->FindFieldDefn( "FOID" ) );

    poRec->SetIntSubfield   ( "FOID", 0, "AGEN", 0,
                              poFeature->GetFieldAsInteger( "AGEN") );
    poRec->SetIntSubfield   ( "FOID", 0, "FIDN", 0,
                              poFeature->GetFieldAsInteger( "FIDN") );
    poRec->SetIntSubfield   ( "FOID", 0, "FIDS", 0,
                              poFeature->GetFieldAsInteger( "FIDS") );

/* -------------------------------------------------------------------- */
/*      ATTF support.                                                   */
/* -------------------------------------------------------------------- */

    if( poRegistrar != nullptr
        && poClassContentExplorer->SelectClass( poFeature->GetDefnRef()->GetName() )
        && !WriteATTF( poRec, poFeature ) )
    {
        delete poRec;
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Add the FSPT if needed.                                         */
/* -------------------------------------------------------------------- */
    if( poFeature->IsFieldSetAndNotNull( poFeature->GetFieldIndex("NAME_RCNM") ) )
    {
        int nItemCount = 0;

        const int *panRCNM =
            poFeature->GetFieldAsIntegerList( "NAME_RCNM", &nItemCount );
        const int *panRCID =
            poFeature->GetFieldAsIntegerList( "NAME_RCID", &nItemCount );
        const int *panORNT =
            poFeature->GetFieldAsIntegerList( "ORNT", &nItemCount );
        const int *panUSAG =
            poFeature->GetFieldAsIntegerList( "USAG", &nItemCount );
        const int *panMASK =
            poFeature->GetFieldAsIntegerList( "MASK", &nItemCount );

        // cppcheck-suppress duplicateExpression
        CPLAssert( sizeof(int) == sizeof(GInt32) );

        const int nRawDataSize = nItemCount * 8;
        unsigned char *pabyRawData = (unsigned char *) CPLMalloc(nRawDataSize);

        for( int i = 0; i < nItemCount; i++ )
        {
            GInt32 nRCID = CPL_LSBWORD32(panRCID[i]);

            pabyRawData[i*8 + 0] = (GByte) panRCNM[i];
            memcpy( pabyRawData + i*8 + 1, &nRCID, 4 );
            pabyRawData[i*8 + 5] = (GByte) panORNT[i];
            pabyRawData[i*8 + 6] = (GByte) panUSAG[i];
            pabyRawData[i*8 + 7] = (GByte) panMASK[i];
        }

        DDFField* poField = poRec->AddField( poModule->FindFieldDefn( "FSPT" ) );
        poRec->SetFieldRaw( poField, 0,
                            (const char *) pabyRawData, nRawDataSize );
        CPLFree( pabyRawData );
    }

/* -------------------------------------------------------------------- */
/*      Add the FFPT if needed.                                         */
/* -------------------------------------------------------------------- */
    char **papszLNAM_REFS = poFeature->GetFieldAsStringList( "LNAM_REFS" );

    if( CSLCount(papszLNAM_REFS) > 0 )
    {
        int i, nRefCount = CSLCount(papszLNAM_REFS);
        const int *panRIND =
            poFeature->GetFieldAsIntegerList( "FFPT_RIND", nullptr );

        poRec->AddField( poModule->FindFieldDefn( "FFPT" ) );

        for( i = 0; i < nRefCount; i++ )
        {
            char szLNAM[9];

            if( strlen(papszLNAM_REFS[i]) < 16 )
                continue;

            // AGEN
            szLNAM[1] = GetHEXChar( papszLNAM_REFS[i] + 0 );
            szLNAM[0] = GetHEXChar( papszLNAM_REFS[i] + 2 );

            // FIDN
            szLNAM[5] = GetHEXChar( papszLNAM_REFS[i] + 4 );
            szLNAM[4] = GetHEXChar( papszLNAM_REFS[i] + 6 );
            szLNAM[3] = GetHEXChar( papszLNAM_REFS[i] + 8 );
            szLNAM[2] = GetHEXChar( papszLNAM_REFS[i] + 10 );

            // FIDS
            szLNAM[7] = GetHEXChar( papszLNAM_REFS[i] + 12 );
            szLNAM[6] = GetHEXChar( papszLNAM_REFS[i] + 14 );

            szLNAM[8] = '\0';

            poRec->SetStringSubfield( "FFPT", 0, "LNAM", i,
                                      (char *) szLNAM, 8 );
            poRec->SetIntSubfield( "FFPT", 0, "RIND", i,
                                   panRIND[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return true;
}

/************************************************************************/
/*                           SetClassBased()                            */
/************************************************************************/

void S57Writer::SetClassBased( S57ClassRegistrar * poReg,
                               S57ClassContentExplorer* poClassContentExplorerIn )

{
    poRegistrar = poReg;
    poClassContentExplorer = poClassContentExplorerIn;
}

/************************************************************************/
/*                             WriteATTF()                              */
/************************************************************************/

bool S57Writer::WriteATTF( DDFRecord *poRec, OGRFeature *poFeature )
{
    CPLAssert( poRegistrar != nullptr );

/* -------------------------------------------------------------------- */
/*      Loop over all attributes.                                       */
/* -------------------------------------------------------------------- */
    int nRawSize = 0;
    int nACount = 0;
    char achRawData[5000] = {};

    char **papszAttrList = poClassContentExplorer->GetAttributeList(nullptr);

    for( int iAttr = 0; papszAttrList[iAttr] != nullptr; iAttr++ )
    {
        const int iField = poFeature->GetFieldIndex( papszAttrList[iAttr] );
        if( iField < 0 )
            continue;

        const OGRFieldType eFldType =
            poFeature->GetDefnRef()->GetFieldDefn(iField)->GetType();

        if( !poFeature->IsFieldSetAndNotNull( iField ) )
            continue;

        const int nATTLInt = poRegistrar->FindAttrByAcronym( papszAttrList[iAttr] );
        if( nATTLInt == -1 )
            continue;

        GUInt16 nATTL = (GUInt16)nATTLInt;
        CPL_LSBPTR16( &nATTL );
        memcpy( achRawData + nRawSize, &nATTL, 2 );
        nRawSize += 2;

        CPLString osATVL;
        if( eFldType == OFTStringList )
        {
            const char* const* papszTokens = poFeature->GetFieldAsStringList(iField);
            for( auto papszIter = papszTokens; papszIter && *papszIter; ++papszIter )
            {
                if( !osATVL.empty() )
                    osATVL += ',';
                osATVL += *papszIter;
            }
        }
        else
        {
            osATVL = poFeature->GetFieldAsString( iField );
        }

        // Special hack to handle special "empty" marker in integer fields.
        if( (eFldType == OFTInteger || eFldType == OFTReal) &&
            atoi(osATVL) == EMPTY_NUMBER_MARKER  )
            osATVL.clear();

        // Watch for really long data.
        if( osATVL.size() + nRawSize + 10 > sizeof(achRawData) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Too much ATTF data for fixed buffer size." );
            return false;
        }

        // copy data into record buffer.
        if( !osATVL.empty() )
        {
            memcpy( achRawData + nRawSize, osATVL.data(), osATVL.size() );
            nRawSize += static_cast<int>(osATVL.size());
        }
        achRawData[nRawSize++] = DDF_UNIT_TERMINATOR;

        nACount++;
    }

/* -------------------------------------------------------------------- */
/*      If we got no attributes, return without adding ATTF.            */
/* -------------------------------------------------------------------- */
    if( nACount == 0 )
        return true;

/* -------------------------------------------------------------------- */
/*      Write the new field value.                                      */
/* -------------------------------------------------------------------- */
    DDFField *poField = poRec->AddField( poModule->FindFieldDefn( "ATTF" ) );

    return CPL_TO_BOOL(poRec->SetFieldRaw( poField, 0, achRawData, nRawSize ));
}
