#******************************************************************************
#  $Id$
# 
#  Project:  OpenGIS Simple Features Reference Implementation
#  Purpose:  OGR Python Shadow Class Implementations
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
# 
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#******************************************************************************
# 
# $Log$
# Revision 1.1  2002/09/26 18:14:32  warmerda
# New
#
#

import _gdal

# OGRwkbGeometryType

wkbUnknown = 0
wkbPoint = 1  
wkbLineString = 2
wkbPolygon = 3
wkbMultiPoint = 4
wkbMultiLineString = 5
wkbMultiPolygon = 6
wkbGeometryCollection = 7
wkbNone = 100 
wkbPoint25D = 0x80000001   
wkbLineString25D = 0x80000002
wkbPolygon25D = 0x80000003
wkbMultiPoint25D = 0x80000004
wkbMultiLineString25D = 0x80000005
wkbMultiPolygon25D = 0x80000006
wkbGeometryCollection25D = 0x80000007

# OGRFieldType

OFTInteger = 0
OFTIntegerList= 1
OFTReal = 2
OFTRealList = 3
OFTString = 4
OFTStringList = 5
OFTWideString = 6
OFTWideStringList = 7
OFTBinary = 8

# OGRJustification

OJUndefined = 0
OJLeft = 1
OJRight = 2



def Open( filename, update = 0 ):
    _gdal.OGRRegisterAll()
    
    ds_o = _gdal.OGROpen( filename, update, None )
    if ds_o is None:
        return None
    else:
        return DataSource( ds_o )

def GetDriverCount():
    return _gdal.OGRGetDriverCount()

def GetDriver( driver_index ):
    dr_o = _gdal.OGRGetDriver( driver_index )
    if dr_o is None:
        return None
    else:
        return SFDriver( dr_o )

class DataSource:
    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRDataSource may not be directly instantiated.'
        self._o = obj

    def Destroy(self):
        _gdal.OGR_DS_Destroy( self._o )
        self._o = None

    def GetName(self):
        return _gdal.OGR_DS_GetName( self._o )

    def GetLayerCount(self):
        return _gdal.OGR_DS_GetLayerCount( self._o )

    def GetLayer(self,iLayer=0):
        l_obj = _gdal.OGR_DS_GetLayer( self._o, iLayer)
        if l_obj is not None:
            return Layer( l_obj )
        else:
            return None

    def CreateLayer(self, name, srs = None, geom_type = wkbUnknown,
                    options = [] ):
        raise ValueError, 'Not supported yet'

    def TestCapability( self, cap ):
        return _gdal.OGR_DS_TestCapability( self._o, cap )

    def ExecuteSQL( self, statement, region = None, dialect = "" ):
        l_obj = _gdal.OGR_DS_ExecuteSQL( self._o, statement, region, dialect )
        if l_obj is not None:
            return Layer( l_obj )
        else:
            return None

    def ReleaseResultsSet( self, layer ):
        _gdal.OGR_DS_ReleaseResultsSet( self._o, layer._o )


class Layer:
    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRLayer may not be directly instantiated.'
        self._o = obj

    def SetSpatialFilter( self, geom ):
        _gdal.OGR_L_SetSpatialFilter( self._o, geom._o )

    def GetSpatialFilter( self ):
        geom_o = _gdal.OGR_L_GetSpatialGeometry( self._o )
        if geom_o is None:
            return None
        else:
            return Geometry( _obj = geom_o )

    def SetAttributeFilter( self, where_clause ):
        return _gdal.OGR_L_SetAttributeFilter( self._o, where_clause )
        
    def ResetReading( self ):
        _gdal.OGR_L_ResetReading( self._o )

    def GetName( self ):
        return _gdal.OGR_FD_GetName( _gdal.OGR_L_GetLayerDefn( self._o ) )

    def GetFeature( self, fid ):
        f_o = _gdal.OGR_L_GetFeature( self._o, fid )
        if f_o is None:
            return None
        else:
            return Feature( obj = f_o )

    def GetNextFeature( self ):
        f_o = _gdal.OGR_L_GetNextFeature( self._o )
        if f_o is None:
            return None
        else:
            return Feature( obj = f_o )

    def SetFeature( self, feat ):
        return _gdal.OGR_L_SetFeature( self._o, feat._o )

    def CreateFeature( self, feat ):
        return _gdal.OGR_L_CreateFeature( self._o, feat._o )

    def GetLayerDefn( self ):
        return FeatureDefn( obj = _gdal.OGR_L_GetLayerDefn( self._o ) )

    def GetFeatureCount( self, force = 1 ):
        return _gdal.OGR_L_GetFeatureCount( self._o, force )

    def GetExtent( self, force = 1 ):
        raise ValueError, 'No implemented yet'

    def TestCapability( self, cap ):
        return _gdal.OGR_L_TestCapability( self._o, cap )

    def CreateFeature( self, field_def ):
        return _gdal.OGR_L_CreateField( self._o, field_def._o )

    def StartTransaction( self ):
        return _gdal.OGR_L_StartTransaction( self._o )

    def CommitTransaction( self ):
        return _gdal.OGR_L_CommitTransaction( self._o )

    def RollbackTransaction( self ):
        return _gdal.OGR_L_RollbackTransaction( self._o )

class Feature:
    def __init__(self,feature_def=None,obj=None):
        if feature_def is None and obj is None:
            raise ValueError, 'ogr.Feature() needs an ogr.FeatureDefn.'
        if feature_def is not None and obj is not None:
            raise ValueError, 'ogr.Feature() cannot receive obj and feature_def.'
        if obj is not None:
            self._o = obj
        else:
            self._o = _gdal.OGR_F_Create( feature_def._o )

    def Destroy( self ):
        if self._o is not None:
            _gdal.OGR_F_Destroy( self._o )
        self._o = None

    def GetDefnRef( self ):
        return FeatureDefn( obj = _gdal.OGR_F_GetDefnRef( self._o ) )

    def SetGeometry( self, geom ):
        if geom is None:
            return _gdal.OGR_F_SetGeometry( self._o, None )
        else:
            return _gdal.OGR_F_SetGeometry( self._o, geom._o )

    def SetGeometryDirectly( self, geom ):
        if geom is None:
            return _gdal.OGR_F_SetGeometryDirectly( self._o, None )
        else:
            return _gdal.OGR_F_SetGeometryDirectly( self._o, geom._o )

    def GetGeometryRef( self ):
        geom_o = _gdal.OGR_F_GetGeometryRef( self._o )

        if geom_o is None:
            return None
        else:
            return Geometry( obj = geom_o )

    def Clone( self ):
        return Feature( obj = _gdal.OBJ_F_Clone( self._o ) )

    def Equal( self, other_geom ):
        return _gdal.OGR_F_Equal( self._o, other_geom._o )

    def GetFieldCount( self ):
        return _gdal.OGR_F_GetFieldCount( self._o )

    def GetFieldDefnRef( self, fld_index ):
        return FieldDefn( obj = _gdal.OGR_F_GetFieldDefnRef( self._o,
                                                             fld_index ) )

    def GetFieldIndex( self, name ):
        return _gdal.OGR_F_GetFieldIndex( self._o, name )

    def IsFieldSet( self, fld_index ):
        return _gdal.OGR_F_IsFieldSet( self._o, fld_index )

    def UnsetField( self, fld_index ):
        _gdal.OGR_F_UnsetField( self._o, fld_index )

    def SetField( self, fld_index, value ):
        _gdal.OGR_F_SetFieldAsString( self._o, fld_index, str(value) )

    def GetFieldAsString( self, fld_index ):
        return _gdal.OGR_F_GetFieldAsString( self._o, fld_index )

    def GetField( self, fld_index ):
        raise ValueError, 'No yet implemented.'

    def GetFID( self ):
        return _gdal.OGR_F_GetFID( self._o )

    def SetFID( self, fid ):
        return _gdal.OGR_F_SetFID( self._o, fid )

    def DumpReadable(self):
        _gdal.OGR_F_DumpReadable( self._o, None )
        
    def SetFrom( self, other, be_forgiving = 1 ):
        return _gdal.OGR_F_SetFrom( self._o, other._o, be_forgiving )

    def GetStyleString( self ):
        return _gdal.OGR_F_GetStyleString( self._o )

    def SetStyleString( self, style ):
        _gdal.OGR_F_SetStyleString( self._o, style )
    
class FeatureDefn:

    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRGeometry may not be directly instantiated.'
        self._o = obj

class FieldDefn:
    
    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'ogr.FieldDefn may not be directly instantiated.'
        self._o = obj

        
class Geometry:
    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRGeometry may not be directly instantiated.'
        self._o = obj
    
    
                  
        
