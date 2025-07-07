/******************************************************************************
 *
 * Name:     Relationship.i
 * Project:  GDAL Python Interface
 * Purpose:  SWIG Interface for GDALRelationship class.
 * Author:   Nyall Dawson
 *
 ******************************************************************************
 * Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

//************************************************************************
//
// Define the extensions for Relationship
//
//************************************************************************
#ifndef SWIGCSHARP
typedef int GDALRelationshipCardinality;
typedef int GDALRelationshipType;
#endif /* CSHARP */

%rename (Relationship) GDALRelationshipShadow;

class GDALRelationshipShadow {
private:

public:

%extend {

    GDALRelationshipShadow( const char *name,
                            const char *leftTableName,
                            const char *rightTableName,
                            GDALRelationshipCardinality cardinality) {
        return (GDALRelationshipShadow*)
        GDALRelationshipCreate(name,
                               leftTableName,
                               rightTableName,
                               cardinality);
    }

    ~GDALRelationshipShadow() {
        GDALDestroyRelationship(self);
    }

    const char *GetName() {
        return GDALRelationshipGetName( self );
    }

    GDALRelationshipCardinality GetCardinality() {
        return GDALRelationshipGetCardinality( self );
    }

    const char *GetLeftTableName() {
        return GDALRelationshipGetLeftTableName( self );
    }

    const char *GetRightTableName() {
        return GDALRelationshipGetRightTableName( self );
    }

    const char *GetMappingTableName() {
        return GDALRelationshipGetMappingTableName( self );
    }

    %apply ( tostring argin ) { (const char* pszName) };
    void SetMappingTableName( const char *pszName ) {
        GDALRelationshipSetMappingTableName( self, pszName );
    }
    %clear (const char* pszName );

    %apply (char **CSL) {char **};
        char **GetLeftTableFields() {
           return GDALRelationshipGetLeftTableFields(self);
        }
    %clear char **;

    %apply (char **CSL) {char **};
        char **GetRightTableFields() {
            return GDALRelationshipGetRightTableFields(self);
        }
    %clear char **;

    %apply (char **options) {char **pFields};
        void SetLeftTableFields(char **pFields) {
            GDALRelationshipSetLeftTableFields(self, pFields);
        }
    %clear char**pList;

    %apply (char **options) {char **pFields};
        void SetRightTableFields(char **pFields) {
            GDALRelationshipSetRightTableFields(self, pFields);
        }
    %clear char**pList;

    %apply (char **CSL) {char **};
        char **GetLeftMappingTableFields() {
            return GDALRelationshipGetLeftMappingTableFields(self);
        }
    %clear char **;

    %apply (char **CSL) {char **};
        char **GetRightMappingTableFields() {
            return GDALRelationshipGetRightMappingTableFields(self);
        }
    %clear char **;

    %apply (char **options) {char **pFields};
      void SetLeftMappingTableFields(char **pFields) {
          GDALRelationshipSetLeftMappingTableFields(self, pFields);
      }
    %clear char**pList;

    %apply (char **options) {char **pFields};
      void SetRightMappingTableFields(char **pFields) {
          GDALRelationshipSetRightMappingTableFields(self, pFields);
      }
    %clear char**pList;

    #ifdef SWIGCSHARP
    GDALRelationshipType GetRelationshipType() {
        return GDALRelationshipGetType( self );
    }
    #else
    GDALRelationshipType GetType() {
        return GDALRelationshipGetType( self );
    }
    #endif

    void SetType( GDALRelationshipType type ) {
      return GDALRelationshipSetType( self, type );
    }

    const char *GetForwardPathLabel() {
        return GDALRelationshipGetForwardPathLabel( self );
    }

    %apply ( tostring argin ) { (const char* pszLabel) };
    void SetForwardPathLabel( const char *pszLabel ) {
        GDALRelationshipSetForwardPathLabel( self, pszLabel );
    }
    %clear (const char* pszLabel );

    const char *GetBackwardPathLabel() {
        return GDALRelationshipGetBackwardPathLabel( self );
    }

    %apply ( tostring argin ) { (const char* pszLabel) };
    void SetBackwardPathLabel( const char *pszLabel ) {
        GDALRelationshipSetBackwardPathLabel( self, pszLabel );
    }
    %clear (const char* pszLabel );

    const char *GetRelatedTableType() {
        return GDALRelationshipGetRelatedTableType( self );
    }

    %apply ( tostring argin ) { (const char* pszType) };
    void SetRelatedTableType( const char *pszType ) {
        GDALRelationshipSetRelatedTableType( self, pszType );
    }
    %clear (const char* pszType );

}

};
