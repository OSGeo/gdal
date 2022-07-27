/******************************************************************************
 * $Id$
 *
 * Name:     Relationship.i
 * Project:  GDAL Python Interface
 * Purpose:  SWIG Interface for GDALRelationship class.
 * Author:   Nyall Dawson
 *
 ******************************************************************************
 * Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot com>
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
 *****************************************************************************/

//************************************************************************
//
// Define the extensions for Relationship
//
//************************************************************************
#ifndef SWIGCSHARP
typedef int GDALRelationshipCardinality;
typedef int GDALRelationshipType;
#else
%rename (RelationshipCardinality) GDALRelationshipCardinality;
typedef enum {
    /*! One-to-one */ GRC_ONE_TO_ONE,
    /*! One-to-many */ GRC_ONE_TO_MANY,
    /*! Many-to-one */ GRC_MANY_TO_ONE,
    /*! Many-to-many */ GRC_MANY_TO_MANY,
} GDALRelationshipCardinality;

%rename (RelationshipType) GDALRelationshipType;
typedef enum {
    /*! Composite relationship */ GRT_COMPOSITE,
    /*! Association relationship */ GRT_ASSOCIATION,
    /*! Aggregation relationship */ GRT_AGGREGATION
} GDALRelationshipType;

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

    GDALRelationshipType GetType() {
        return GDALRelationshipGetType( self );
    }

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
