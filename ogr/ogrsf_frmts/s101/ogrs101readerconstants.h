/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Reader
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_S101_READER_CONSTANTS_H_INCLUDED
#define OGR_S101_READER_CONSTANTS_H_INCLUDED

#include "ogr_s101.h"

// Refer to:
//
// - S-101 Edition 2.0:
//   https://registry.iho.int/productspec/view.do?idx=214&product_ID=S-101&statusS=5&domainS=ALL&category=product_ID&searchValue=
//
// - S-100 Edition 5.2 Part 10a (ISO/IEC 8211 Encoding):
//   https://iho.int/uploads/user/pubs/standards/s-100/S-100%20Ed%205.2.1_FINAL.pdf
//
// S-101 adds additional constraints over S-100 Edition 5.2 Part 10a

using RecordName = OGRS101Reader::RecordName;
constexpr RecordName RECORD_NAME_DATASET_IDENTIFICATION = 10;  // in DSID field
constexpr RecordName RECORD_NAME_CRS = 15;                     // in CSID field
constexpr RecordName RECORD_NAME_INFORMATION_TYPE = 150;       // in IRID field
constexpr RecordName RECORD_NAME_POINT = 110;                  // in PRID field
constexpr RecordName RECORD_NAME_MULTIPOINT = 115;             // in MRID field
constexpr RecordName RECORD_NAME_CURVE = 120;                  // in CRID field
constexpr RecordName RECORD_NAME_COMPOSITE_CURVE = 125;        // in CCID field
constexpr RecordName RECORD_NAME_SURFACE = 130;                // in SRID field
constexpr RecordName RECORD_NAME_FEATURE_TYPE = 100;           // in FRID field

constexpr const char *_0000_FIELD = "0000";

// clang-format off
// The formatting reflects the (parent, child) hierarchy of fields.
// Cf "B-5 Base Dataset Structure" of S-101 spec
constexpr const char *DSID_FIELD = "DSID";     // Dataset Identification
  constexpr const char *DSSI_FIELD = "DSSI";   // Dataset Structure Information
  constexpr const char *ATCS_FIELD = "ATCS";   // Attribute Codes
  constexpr const char *ITCS_FIELD = "ITCS";   // Information Type Codes
  constexpr const char *FTCS_FIELD = "FTCS";   // Feature Type Codes
  constexpr const char *IACS_FIELD = "IACS";   // Information Association Codes
  constexpr const char *FACS_FIELD = "FACS";   // Feature Association Codes
  constexpr const char *ARCS_FIELD = "ARCS";   // Association Role Codes

constexpr const char *CSID_FIELD = "CSID";     // Coordinate Reference System Record Identifier
  constexpr const char* CRSH_FIELD = "CRSH";   // Coordinate Reference System Header
    constexpr const char* CSAX_FIELD = "CSAX"; // Coordinate System Axes
    constexpr const char* VDAT_FIELD = "VDAT"; // Vertical Datum

constexpr const char *IRID_FIELD = "IRID";     // Information Type Record Identifier
    constexpr const char *NITC_SUBFIELD = "NITC"; // Numeric Information Type Code
  constexpr const char *ATTR_FIELD = "ATTR";   // Attribute field
  constexpr const char *INAS_FIELD = "INAS";   // Information Association

constexpr const char *PRID_FIELD = "PRID";     // Point Record Identifier
  //constexpr const char *INAS_FIELD = "INAS"; // Information Association
  constexpr const char *C2IT_FIELD = "C2IT";   // 2-D Integer Coordinate Tuple
  constexpr const char *C3IT_FIELD = "C3IT";   // 3-D Integer Coordinate Tuple

constexpr const char *MRID_FIELD = "MRID";     // Multi Point Record Identifier field
  //constexpr const char *INAS_FIELD = "INAS"; // Information Association
  constexpr const char *COCC_FIELD = "COCC";   // Coordinate Control
  constexpr const char *C2IL_FIELD = "C2IL";   // 2-D Integer Coordinate List
  constexpr const char *C3IL_FIELD = "C3IL";   // 3-D Integer Coordinate List

constexpr const char *CRID_FIELD = "CRID";     // Curve Record Identifier
  //constexpr const char *INAS_FIELD = "INAS"; // Information Association
  constexpr const char *PTAS_FIELD = "PTAS";   // Point Association
  constexpr const char *SECC_FIELD = "SECC";   // Segment Control field
  constexpr const char *SEGH_FIELD = "SEGH";   // Segment Header field
  // constexpr const char *COCC_FIELD = "COCC";   // Coordinate Control
  // constexpr const char *C2IL_FIELD = "C2IL";   // 2-D Integer Coordinate List

constexpr const char *CCID_FIELD = "CCID";     // Composite Curve Record Identifier
  //constexpr const char *INAS_FIELD = "INAS"; // Information Association
  //constexpr const char *C2IL_FIELD = "C2IL"; // 2-D Integer Coordinate List
  constexpr const char *CCOC_FIELD = "CCOC";   // Curve Component Control
  constexpr const char *CUCO_FIELD = "CUCO";   // Curve Component field

constexpr const char *SRID_FIELD = "SRID";     // Surface Record Identifier
  //constexpr const char *INAS_FIELD = "INAS"; // Information Association
  constexpr const char *RIAS_FIELD = "RIAS";   // Ring Association
    constexpr const char *USAG_SUBFIELD = "USAG";
    constexpr const char *RAUI_SUBFIELD = "RAUI";

constexpr const char *FRID_FIELD = "FRID";     // Feature Type Record Identifier
     constexpr const char *NFTC_SUBFIELD = "NFTC"; // Feature Type Numeric Code
   constexpr const char *FOID_FIELD = "FOID";   // Feature Object Identifier
     constexpr const char *AGEN_SUBFIELD = "AGEN";
     constexpr const char *FIDN_SUBFIELD = "FIDN";
     constexpr const char *FIDS_SUBFIELD = "FIDS";
  //constexpr const char *ATTR_FIELD = "ATTR"; // Attribute field
  //constexpr const char *INAS_FIELD = "INAS"; // Information Association
  constexpr const char *SPAS_FIELD = "SPAS";   // Spatial Association
    constexpr const char *SMIN_SUBFIELD = "SMIN";
    constexpr const char *SMAX_SUBFIELD = "SMAX";
    constexpr const char *SAUI_SUBFIELD = "SAUI";
  constexpr const char *FASC_FIELD = "FASC";   // Feature Association
  constexpr const char *MASK_FIELD = "MASK";   // Masked Spatial Type
    constexpr const char* MIND_SUBFIELD = "MIND";
    constexpr const char* MUIN_SUBFIELD = "MUIN";
// clang-format on

// xxID subfields
constexpr const char *RCNM_SUBFIELD = "RCNM";
constexpr const char *RCID_SUBFIELD = "RCID";
constexpr const char *RUIN_SUBFIELD = "RUIN";

// ATTR subfields
constexpr const char *NATC_SUBFIELD = "NATC";  // Numeric attribute code
                                               // (references ATCS.ATCD)
constexpr const char *ATIX_SUBFIELD = "ATIX";  // Attribute index
constexpr const char *PAIX_SUBFIELD = "PAIX";  // Parent index
constexpr const char *ATIN_SUBFIELD = "ATIN";  // Attribute instruction
constexpr const char *ATVL_SUBFIELD = "ATVL";  // Attribute value

// INAS subfields
constexpr const char *RRNM_SUBFIELD = "RRNM";  // Referenced Record name
constexpr const char *RRID_SUBFIELD = "RRID";  // Referenced Record identifier
constexpr const char *NIAC_SUBFIELD =
    "NIAC";  // Numeric Information Association Code
constexpr const char *NARC_SUBFIELD = "NARC";  // Numeric Association Role Code
constexpr const char *IUIN_SUBFIELD =
    "IUIN";  // Information Association Update Instruction

// FASC subfields
// Numeric Feature Association Code
constexpr const char *NFAC_SUBFIELD = "NFAC";
// Feature Association Update Instruction
constexpr const char *FAUI_SUBFIELD = "FAUI";

// C3IT/C3IL subfields
constexpr const char *VCID_SUBFIELD = "VCID";  // ID of the Vertical CRS

// C2IT/C3IT/C2IL/C3IL subfields
constexpr const char *XCOO_SUBFIELD = "XCOO";  // Coordinate in X axis
constexpr const char *YCOO_SUBFIELD = "YCOO";  // Coordinate in Y axis
constexpr const char *ZCOO_SUBFIELD = "ZCOO";  // Coordinate in Z axis

constexpr const char *RVER_SUBFIELD = "RVER";  // Record version

// Value of RUIN subfield or other update instruction subfields.
constexpr int INSTRUCTION_INSERT = 1;
constexpr int INSTRUCTION_DELETE = 2;
constexpr int INSTRUCTION_UPDATE = 3;

// Orientation subfield used in composite curve and surface records
constexpr const char *ORNT_SUBFIELD = "ORNT";
constexpr int ORNT_FORWARD = 1;
constexpr int ORNT_REVERSE = 2;
constexpr int ORNT_NULL = 255;

// OGR field names
constexpr const char *OGR_FIELD_NAME_RECORD_ID = "recordId";
constexpr const char *OGR_FIELD_NAME_RECORD_VERSION = "recordVersion";
constexpr const char *OGR_FIELD_NAME_INFORMATION_TYPE = "informationType";
constexpr const char *OGR_FIELD_NAME_REF_INFO_RID = "infoAssociationRecordId";
constexpr const char *OGR_FIELD_NAME_REF_FEAT_LAYER_NAME =
    "featureAssociationRefLayerName";
constexpr const char *OGR_FIELD_NAME_REF_FEAT_RID =
    "featureAssociationRefRecordId";
constexpr const char *OGR_FIELD_NAME_NIAC = "infoAssociationCode";
constexpr const char *OGR_FIELD_NAME_NFAC = "featureAssociationCode";
constexpr const char *OGR_FIELD_NAME_NARC = "infoAssociationRoleCode";
constexpr const char *OGR_FIELD_NAME_FEATURE_NARC =
    "featureAssociationRoleCode";
constexpr const char *OGR_FIELD_NAME_AGEN = "producingAgency";
constexpr const char *OGR_FIELD_NAME_FIDN = "featureIdentificationNumber";
constexpr const char *OGR_FIELD_NAME_FIDS = "featureIdentificationSubdivision";
constexpr const char *OGR_FIELD_NAME_GEOMETRY_LAYER_NAME = "geometryLayerName";
constexpr const char *OGR_FIELD_NAME_GEOMETRY_RECORD_ID = "geometryRecordId";
constexpr const char *OGR_FIELD_NAME_GEOMETRY_ORIENTATION =
    "geometryOrientation";
constexpr const char *OGR_FIELD_NAME_SMIN = "scaleMinimum";
constexpr const char *OGR_FIELD_NAME_SMAX = "scaleMaximum";
constexpr const char *OGR_FIELD_NAME_MASK_LAYER_NAME = "maskLayerName";
constexpr const char *OGR_FIELD_NAME_MASK_RECORD_ID = "maskRecordId";
constexpr const char *OGR_FIELD_NAME_MASK_INDICATOR = "maskIndicator";

// OGR layer names
constexpr const char *OGR_LAYER_NAME_CURVE = "Curve";
constexpr const char *OGR_LAYER_NAME_COMPOSITE_CURVE = "CompositeCurve";
constexpr const char *OGR_LAYER_NAME_SURFACE = "Surface";

#endif  // OGR_S101_READER_CONSTANTS_H_INCLUDED
