#******************************************************************************
#  $Id$
# 
#  Project:  OSR (OGRSpatialReference/CoordinateTransform) Python Interface
#  Purpose:  OSR Shadow Class Implementations
#  Author:   Frank Warmerdam, warmerda@home.com
# 
#******************************************************************************
#  Copyright (c) 2000, Frank Warmerdam
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
# Revision 1.1  2000/03/22 01:10:49  warmerda
# New
#
#

import _gdal

class SpatialReference:

    def __init__(self,obj=None):
        if obj is None:
            self._o = _gdal.OSRNewSpatialReference( "" )
        else:
            self._o = obj
            _gdal.OSRReference( self._o )

    def __del__(self):
        if _gdal.OSRDereference( self._o ) == 0:
            _gdal.OSRDestroySpatialReference( self._o )

    def ImportFromWkt( self, wkt ):
        return _gdal.OSRImportFromWkt( self._o, wkt )

    def ExportToWkt(self):
        return _gdal.OSRExportToWkt( self._o )

    def ImportFromEPSG(self,code):
        return _gdal.OSRImportFromEPSG( self._o, code )

    def IsGeographic(self):
        return _gdal.OSRIsGeographic( self._o )
    
    def IsProjected(self):
        return _gdal.OSRIsProjected( self._o )

    def GetAttrValue(self, name, child = 0):
        return _gdal.OSRGetAttrValue(self, name, child)
    
    def SetAttrValue(self, name, value):
        return _gdal.OSRSetAttrValue(self, name, value)

    def SetWellKnownGeogCS(self, name):
        return _gdal.OSRSetWellKnownGeogCS(self._o, name)
    
    def SetProjCS(self, name = "unnamed" ):
        return _gdal.OSRSetProjCS(self._o, name)
    
    def IsSameGeogCS(self, other):
        return _gdal.OSRIsSameGeogCS(self._o, other._o)

    def IsSame(self, other):
        return _gdal.OSRIsSame(self._o, other._o)

    def SetLinearUnits(self, units_name, to_meters ):
        return _gdal.OSRSetLinearUnits( self._o, units_name, to_meters )

    def SetUTM(self, zone, is_north = 1):
        return _gdal.OSRSetUTM(self._o, zone, is_north )

    
class CoordinateTransformation:
    
    def __init__(self,source,target):
        self._o = _gdal.OCTNewCoordinateTransformation( source._o, target._o )


    def __del__(self):
        if self._o: 
            _gdal.OCTDestroyCoordinateTransformation( self._o )

    def TransformPoint(self, x, y, z = 0):
        points = [(x,y,z),]
        points_ret = self.TransformPoints(points)
        return points_ret[0]

    def TransformPoints(self, points):
        print points
        return _gdal.OCTTransform(self._o, points)
