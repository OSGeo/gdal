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
# Revision 1.18  2003/10/09 15:27:53  warmerda
# added OGRLayer::DeleteFeature() support
#
# Revision 1.17  2003/10/06 22:28:54  warmerda
# Added support for GetExtent().
#
# Revision 1.16  2003/09/22 05:48:50  warmerda
# added GML geometry support
#
# Revision 1.15  2003/09/13 04:57:06  warmerda
# fixed up NULL support for spatial references
#
# Revision 1.14  2003/08/27 15:40:06  warmerda
# added OGRSetGenerate_DB2_V72_BYTE_ORDER()
#
# Revision 1.13  2003/06/10 14:43:33  warmerda
# fixed NULL support in GetGeometryRef()
#
# Revision 1.12  2003/04/22 17:28:55  warmerda
# added SyncToDisk, fix DumpReadable
#
# Revision 1.11  2003/04/08 22:13:00  warmerda
# added new entry poins, and listtostringlist support
#
# Revision 1.10  2003/04/03 19:27:55  warmerda
# added nullable string support, fixed ogr.Layer.SetAttributeFilter()
#
# Revision 1.9  2003/03/20 17:53:30  warmerda
# added OGR OpenShared and reference coutnting stuff
#
# Revision 1.8  2003/03/07 16:28:55  warmerda
# lots of 'NULL' fixes
#
# Revision 1.7  2003/03/03 05:16:11  warmerda
# added DeleteLayer and DeleteDataSource methods
#
# Revision 1.6  2003/03/03 02:46:05  warmerda
# added ogr.Driver class
#
# Revision 1.5  2003/01/08 18:15:48  warmerda
# added loads of stuff
#
# Revision 1.4  2003/01/02 21:41:07  warmerda
# added GetField(), and BuildPolygonFromEdges methods
#
# Revision 1.3  2002/10/24 20:37:18  warmerda
# fixed srs support in CreateFrom functions
#
# Revision 1.2  2002/10/24 16:51:17  warmerda
# added lots of OGRGeometryH support
#
# Revision 1.1  2002/09/26 18:14:32  warmerda
# New
#
#

import _gdal
import gdal
import osr

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
wkbLinearRing = 101
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

wkbXDR = 0
wkbNDR = 1

###############################################################################
#     Do this on module instantiation.

_gdal.OGRRegisterAll()
    
#############################################################################
# Various free standing functions.

def Open( filename, update = 0 ):
    
    ds_o = _gdal.OGROpen( filename, update, 'NULL' )
    if ds_o is None or ds_o == 'NULL':
        raise ValueError, 'Unable to open: ' + filename
    else:
        return DataSource( ds_o )

def OpenShared( filename, update = 0 ):
    
    ds_o = _gdal.OGROpenShared( filename, update, 'NULL' )
    if ds_o is None or ds_o == 'NULL':
        raise ValueError, 'Unable to open: ' + filename
    else:
        return DataSource( ds_o )

def GetDriverCount():
    return _gdal.OGRGetDriverCount()

def GetDriver( driver_index ):
    dr_o = _gdal.OGRGetDriver( driver_index )
    if dr_o is None or dr_o == 'NULL':
        raise IndexError
    else:
        return Driver( dr_o )

def GetDriverByName( name ):
    count = GetDriverCount()
    for i in range(count):
        dr = GetDriver( i )
        if dr.GetName() == name:
            return dr

    raise ValueError, 'Unable to find ogr.Driver named "%s".' % name

def GetOpenDSCount():
    return _gdal.OGRGetOpenDSCount()

def GetOpenDS( i ):
    _o = _gdal.OGRGetOpenDS( i )
    if _o is None or _o == 'NULL':
        return None
    else:
        return DataSource( obj = _o )

def SetGenerate_DB2_V72_BYTE_ORDER( flag ):
    return _gdal.OGRSetGenerate_DB2_V72_BYTE_ORDER( flag )

#############################################################################
# OGRSFDriver

class Driver:

    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRDriver may not be directly instantiated.'
        self._o = obj

    def GetName( self ):
        return _gdal.OGR_Dr_GetName( self._o )

    def TestCapability( self, cap ):
        return _gdal.OGR_Dr_TestCapability( self._o, cap )

    def Open( self, filename, update = 0 ):
        ds_o = _gdal.OGR_Dr_Open( self._o, filename, update )
        if ds_o is None or ds_o == 'NULL':
            return None
        else:
            return DataSource( ds_o )

    def CreateDataSource( self, filename, options = [] ):
        md_c = _gdal.ListToStringList( options )
        ds_o = _gdal.OGR_Dr_CreateDataSource( self._o, filename, md_c )
        _gdal.CSLDestroy(md_c)
        
        if ds_o is None or ds_o == 'NULL':
            raise ValueError, _gdal.CPLGetLastErrorMsg()
        else:
            return DataSource( ds_o )

    def CopyDataSource( self, src_ds, filename, options = [] ):
        md_c = _gdal.ListToStringList( options )
        ds_o = _gdal.OGR_Dr_CopyDataSource( self._o, src_ds._o, filename, md_c)
        _gdal.CSLDestroy(md_c)
        
        if ds_o is None or ds_o == 'NULL':
            raise ValueError, _gdal.CPLGetLastErrorMsg()
        else:
            return DataSource( ds_o )

    def DeleteDataSource( self, filename ):
        return _gdal.OGR_Dr_DeleteDataSource( self._o, filename )

#############################################################################
# OGRDataSource

class DataSource:
    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRDataSource may not be directly instantiated.'
        self._o = obj

    def Destroy(self):
        _gdal.OGR_DS_Destroy( self._o )
        self._o = None

    def Release(self):
        _gdal.OGRReleaseDataSource( self._o )
        self._o = None

    def Reference(self):
        return _gdal.OGR_DS_Reference(self._o)
    
    def Dereference(self):
        return _gdal.OGR_DS_Dereference(self._o)

    def GetRefCount(self):
        return _gdal.OGR_DS_GetRefCount(self._o)

    def GetSummaryRefCount(self):
        return _gdal.OGR_DS_GetSummaryRefCount(self._o)

    def GetName(self):
        return _gdal.OGR_DS_GetName( self._o )

    def GetLayerCount(self):
        return _gdal.OGR_DS_GetLayerCount( self._o )

    def GetLayer(self,iLayer=0):
        # If given a layer name, scan for it. 
        if type(iLayer).__name__ == 'str':
            return self.GetLayerByName( iLayer )

        l_obj = _gdal.OGR_DS_GetLayer( self._o, iLayer)
        if l_obj is not None and l_obj != 'NULL':
            return Layer( l_obj )
        else:
            raise IndexError, 'No layer %d on datasource' % iLayer

    def GetLayerByName(self,name):
        # If given a layer name, scan for it. 
        l_obj = _gdal.OGR_DS_GetLayerByName( self._o, name)
        if l_obj is not None and l_obj != 'NULL':
            return Layer( l_obj )
        else:
            raise IndexError, 'No layer %s on datasource' % name 

    def DeleteLayer( self, iLayer ):
        return _gdal.OGR_DS_DeleteLayer( self._o, iLayer )

    def CreateLayer(self, name, srs = None, geom_type = wkbUnknown,
                    options = [] ):
        if srs is None:
            srs_o = 'NULL'
        else:
	    srs_o = srs._o
        md_c = _gdal.ListToStringList( options )
        obj = _gdal.OGR_DS_CreateLayer( self._o, name, srs_o, geom_type, md_c)
        _gdal.CSLDestroy(md_c)
        if obj is None and obj != 'NULL':
            raise ValueError, gdal.GetLastErrorMsg()
        else:
            return Layer( obj = obj )

    def CopyLayer(self, src_layer, new_name, options = [] ):
        md_c = _gdal.ListToStringList( options )
        obj = _gdal.OGR_DS_CopyLayer( self._o, src_layer._o, new_name, md_c)
        _gdal.CSLDestroy(md_c)
        if obj is None and obj != 'NULL':
            raise ValueError, gdal.GetLastErrorMsg()
        else:
            return Layer( obj = obj )

    def TestCapability( self, cap ):
        return _gdal.OGR_DS_TestCapability( self._o, cap )

    def ExecuteSQL( self, statement, region = 'NULL', dialect = "" ):
        l_obj = _gdal.OGR_DS_ExecuteSQL( self._o, statement, region, dialect )
        if l_obj is not None and l_obj != 'NULL':
            return Layer( l_obj )
        else:
            return None

    def ReleaseResultSet( self, layer ):
        _gdal.OGR_DS_ReleaseResultSet( self._o, layer._o )

#############################################################################
# OGRLayer

class Layer:
    def __init__(self,obj=None):
        if obj is None:
            raise ValueError, 'OGRLayer may not be directly instantiated.'
        self._o = obj

    def Reference(self):
        return _gdal.OGR_L_Reference(self._o)
    
    def Dereference(self):
        return _gdal.OGR_L_Dereference(self._o)

    def GetRefCount(self):
        return _gdal.OGR_L_GetRefCount(self._o)

    def SetSpatialFilter( self, geom ):
        if geom is None:
            geom_o = 'NULL'
        else:
            geom_o = geom._o
        _gdal.OGR_L_SetSpatialFilter( self._o, geom_o )

    def GetSpatialFilter( self ):
        geom_o = _gdal.OGR_L_GetSpatialGeometry( self._o )
        if geom_o is None or geom_o == 'NULL':
            return None
        else:
            return Geometry( _obj = geom_o )

    def SetAttributeFilter( self, where_clause = None ):
        filter = gdal.ToNULLableString( where_clause )
        result = _gdal.OGR_L_SetAttributeFilter( self._o, filter )
        gdal.FreeNULLableString(filter)
        return result
        
    def ResetReading( self ):
        _gdal.OGR_L_ResetReading( self._o )

    def GetName( self ):
        return _gdal.OGR_FD_GetName( _gdal.OGR_L_GetLayerDefn( self._o ) )

    def GetFeature( self, fid ):
        f_o = _gdal.OGR_L_GetFeature( self._o, fid )
        if f_o is None or f_o == 'NULL':
            return None
        else:
            return Feature( obj = f_o )

    def GetNextFeature( self ):
        f_o = _gdal.OGR_L_GetNextFeature( self._o )
        if f_o is None or f_o == 'NULL':
            return None
        else:
            return Feature( obj = f_o )

    def SetFeature( self, feat ):
        return _gdal.OGR_L_SetFeature( self._o, feat._o )

    def CreateFeature( self, feat ):
        return _gdal.OGR_L_CreateFeature( self._o, feat._o )

    def DeleteFeature( self, fid ):
        return _gdal.OGR_L_DeleteFeature( self._o, fid )

    def SyncToDisk( self ):
        return _gdal.OGR_L_SyncToDisk( self._o )

    def GetLayerDefn( self ):
        return FeatureDefn( obj = _gdal.OGR_L_GetLayerDefn( self._o ) )

    def GetFeatureCount( self, force = 1 ):
        return _gdal.OGR_L_GetFeatureCount( self._o, force )

    def GetExtent( self, force = 1 ):
        extents = _gdal.ptrcreate( 'double', '', 4 )
        res = _gdal.OGR_L_GetExtent( self._o,
                  _gdal.ptrcast(extents,'OGREnvelope_p'), force )
        if res != 0:
            ret_extents = None
        else:
            ret_extents = ( _gdal.ptrvalue( extents, 0 ),
                            _gdal.ptrvalue( extents, 1 ),
                            _gdal.ptrvalue( extents, 2 ),
                            _gdal.ptrvalue( extents, 3 ) )
        _gdal.ptrfree( extents )
        
        return ret_extents

    def TestCapability( self, cap ):
        return _gdal.OGR_L_TestCapability( self._o, cap )

    def CreateField( self, field_def, approx_ok = 1 ):
        return _gdal.OGR_L_CreateField( self._o, field_def._o, approx_ok )

    def CreateFeature( self, feature ):
        return _gdal.OGR_L_CreateFeature( self._o, feature._o )

    def StartTransaction( self ):
        return _gdal.OGR_L_StartTransaction( self._o )

    def CommitTransaction( self ):
        return _gdal.OGR_L_CommitTransaction( self._o )

    def RollbackTransaction( self ):
        return _gdal.OGR_L_RollbackTransaction( self._o )

    def GetSpatialRef( self ):
        srs_o = _gdal.OGR_L_GetSpatialRef( self._o )
        if srs_o is not None and srs_o != 'NULL':
            return osr.SpatialReference( srs_o )
        else:
            return None

#############################################################################
# OGRFeature

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

        if geom_o is None or geom_o == 'NULL':
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
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        return FieldDefn( obj = _gdal.OGR_F_GetFieldDefnRef( self._o,
                                                             fld_index ) )

    def GetFieldIndex( self, name ):
        return _gdal.OGR_F_GetFieldIndex( self._o, name )

    def IsFieldSet( self, fld_index ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        return _gdal.OGR_F_IsFieldSet( self._o, fld_index )

    def UnsetField( self, fld_index ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        _gdal.OGR_F_UnsetField( self._o, fld_index )

    def SetField( self, fld_index, value ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        _gdal.OGR_F_SetFieldString( self._o, fld_index, str(value) )

    def GetFieldAsString( self, fld_index ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        return _gdal.OGR_F_GetFieldAsString( self._o, fld_index )

    def GetFieldAsInteger( self, fld_index ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        return _gdal.OGR_F_GetFieldAsInteger( self._o, fld_index )

    def GetFieldAsDouble( self, fld_index ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        return _gdal.OGR_F_GetFieldAsDouble( self._o, fld_index )

    def GetField( self, fld_index ):
        if type(fld_index).__name__ == 'str':
            fld_index = self.GetFieldIndex(fld_index)
        return _gdal.OGR_F_GetField( self._o, fld_index )

    def GetFID( self ):
        return _gdal.OGR_F_GetFID( self._o )

    def SetFID( self, fid ):
        return _gdal.OGR_F_SetFID( self._o, fid )

    def DumpReadable(self):
        _gdal.OGR_F_DumpReadable( self._o, 'NULL' )
        
    def SetFrom( self, other, be_forgiving = 1 ):
        return _gdal.OGR_F_SetFrom( self._o, other._o, be_forgiving )

    def GetStyleString( self ):
        return _gdal.OGR_F_GetStyleString( self._o )

    def SetStyleString( self, style ):
        _gdal.OGR_F_SetStyleString( self._o, style )
    
class FeatureDefn:

    def __init__(self,obj=None):
        if obj is None:
            self._o = _gdal.OGR_FD_Create( name )
        else:
            self._o = obj

    def Destroy( self ):
        _gdal.OGR_FD_Destroy( self._o )

    def GetName( self ):
        return _gdal.OGR_FD_GetName( self._o )

    def GetFieldCount( self ):
        return _gdal.OGR_FD_GetFieldCount( self._o )

    def GetFieldDefn( self, i ):
        obj = _gdal.OGR_FD_GetFieldDefn( self._o, i )
        if obj is not None:
            return FieldDefn( obj = obj )
        else:
            return None

    def GetFieldIndex( self, name ):
        return _gdal.OGR_FD_GetFieldIndex( self._o, name )

    def AddFieldDefn( self, field_defn ):
        _gdal.OGR_FD_AddFieldDefn( self._o, field_defn._o )

    def GetGeomType( self ):
        return _gdal.OGR_FD_GetGeomType( self._o )

    def SetGeomType( self, geom_type ):
        _gdal.OGR_FD_SetGeomType( self._o, geom_type )
        
    def Reference( self ):
        return _gdal.OGR_FD_Reference( self._o )
        
    def Dereference( self ):
        return _gdal.OGR_FD_Dereference( self._o )
        
    def GetReferenceCount( self ):
        return _gdal.OGR_FD_GetReferenceCount( self._o )
        
#############################################################################
# OGRFieldDefn

class FieldDefn:
    
    def __init__(self,name='unnamed',field_type=OFTString, obj=None):
        if obj is None:
            self._o = _gdal.OGR_Fld_Create( name, field_type )
        else:
            self._o = obj

    def Destroy( self ):
        _gdal.OGR_Fld_Destroy( self._o )

    def GetName( self ):
        return _gdal.OGR_Fld_GetNameRef( self._o )

    def GetNameRef( self ):
        return _gdal.OGR_Fld_GetName( self._o )

    def SetName( self, name ):
        _gdal.OGR_Fld_SetName( self._o, name )

    def GetType( self ):
        return _gdal.OGR_Fld_GetType( self._o )

    def SetType( self, type ):
        _gdal.OGR_Fld_SetType( self._o, type )

    def GetJustify( self ):
        return _gdal.OGR_Fld_GetJustify( self._o )
    
    def SetJustify( self, justification ):
        _gdal.OGR_Fld_SetJustify( self._o, justification )
    
    def GetWidth( self ):
        return _gdal.OGR_Fld_GetWidth( self._o )
    
    def SetWidth( self, width ):
        _gdal.OGR_Fld_SetWidth( self._o, width )
    
    def GetPrecision( self ):
        return _gdal.OGR_Fld_GetPrecision( self._o )
    
    def SetPrecision( self, precision ):
        _gdal.OGR_Fld_SetPrecision( self._o, precision )
    
#############################################################################
# OGRGeometry

def CreateGeometryFromWkb( bin_string, srs = None ):
    if srs is not None:
        srs_o = srs._o
    else:
        srs_o = ''
    _obj = _gdal.OGR_G_CreateFromWkb( bin_string, srs_o )
    return Geometry( obj = _obj )

def CreateGeometryFromWkt( string, srs = None ):
    if srs is not None:
        srs_o = srs._o
    else:
        srs_o = ''
    _obj = _gdal.OGR_G_CreateFromWkt( string, srs_o )
    return Geometry( obj = _obj )

def CreateGeometryFromGML( string ):
    _obj = _gdal.OGR_G_CreateFromGML( string )
    return Geometry( obj = _obj )

class Geometry:
    def __init__(self, type=None, obj=None):
        if obj is not None:
            self._o = obj
        elif type is not None:
            self._o = _gdal.OGR_G_CreateGeometry( type )
        else:
            raise ValueError, 'OGRGeometry may not be directly instantiated.'

    def Destroy( self ):
        _gdal.OGR_G_DestroyGeometry( self._o )
        self._o = None

    def ExportToWkb( self, byte_order = None ):
        if byte_order is None:
            byte_order = wkbXDR
        return _gdal.OGR_G_ExportToWkb( self._o, byte_order )

    def ExportToWkt( self):
        return _gdal.OGR_G_ExportToWkt( self._o )

    def ExportToGML( self):
        return _gdal.OGR_G_ExportToGML( self._o )

    def GetDimension( self ):
        return _gdal.OGR_G_GetDimension( self._o )
    
    def GetCoordinateDimension( self ):
        return _gdal.OGR_G_GetCoordinateDimension( self._o )
    
    def WkbSize( self ):
        return _gdal.OGR_G_WkbSize( self._o )

    def Clone( self ):
        _obj = _gdal.OGR_G_Clone( self._o )
        if _obj is not None:
            return Geometry( obj = _obj )
        else:
            return None

    def GetGeometryType( self ):
        return _gdal.OGR_G_GetGeometryType( self._o )
    
    def GetGeometryName( self ):
        return _gdal.OGR_G_GetGeometryName( self._o )
    
    def FlattenTo2D( self ):
        _gdal.OGR_G_FlattenTo2D( self._o )
    
    def AssignSpatialReference( self, srs ):
        _gdal.OGR_G_AssignSpatialReference( self._o, srs._o )
    
    def GetSpatialReference( self ):
        srs_o = _gdal.OGR_G_GetSpatialReference( self._o )
        if srs_o is not None and srs_o != 'NULL':
            return osr.SpatialReference( srs_o )
        else:
            return None

    def Transform( self, coord_tran ):
        return _gdal.OGR_G_Transform( self._o, coord_tran._o )
    
    def TransformTo( self, srs_out ):
        return _gdal.OGR_G_Transform( self._o, srs_out._o )
    
    def Intersect( self, other_geom ):
        return _gdal.OGR_G_Intersect( self._o, other_geom._o )

    def Equal( self, other_geom ):
        return _gdal.OGR_G_Equal( self._o, other_geom._o )

    def Empty( self, other_geom ):
        return _gdal.OGR_G_Empty( self._o, other_geom._o )

    def GetPointCount( self):
        return _gdal.OGR_G_GetPointCount( self._o )

    def GetX( self, i ):
        return _gdal.OGR_G_GetX( self._o, i )

    def GetY( self, i ):
        return _gdal.OGR_G_GetY( self._o, i )

    def GetZ( self, i ):
        return _gdal.OGR_G_GetZ( self._o, i )

    def SetPoint( self, i, x, y, z = 0):
        return _gdal.OGR_G_SetPoint( self._o, i, x, y, z )

    def AddPoint( self, x, y, z = 0 ):
        return _gdal.OGR_G_AddPoint( self._o, x, y, z )

    def GetGeometryCount( self ):
        return _gdal.OGR_G_GetGeometryCount( self._o )

    def GetGeometryRef( self, i ):
        geom = _gdal.OGR_G_GetGeometryRef( self._o, i )
        if geom is not None:
            return Geometry( obj = geom )
        else:
            return None
        
    def AddGeometry( self, subgeom ):
        return _gdal.OGR_G_AddGeometry( self._o, subgeom._o )

    def AddGeometryDirectly( self, subgeom ):
        return _gdal.OGR_G_AddGeometryDirectly( self._o, subgeom._o )


def BuildPolygonFromEdges( edges, bBestEffort=0, bAutoClose=0, Tolerance=0 ):
    _o = _gdal.OGRBuildPolygonFromEdges( edges._o, bBestEffort, bAutoClose,
                                         Tolerance )
    if _o is not None:
        return Geometry( obj = _o )
    else:
        return None;



