#!/usr/bin/env python

import math
import os
import sys

from osgeo import osr
from osgeo import ogr
ogr.UseExceptions()


class Translator(object):

    def construct_parser(self):
        from optparse import OptionParser, OptionGroup
        usage = "usage: %prog [options] arg"
        parser = OptionParser(usage)
        g = OptionGroup(parser, "Base options", "Basic Translation Options")
        g.add_option("-i", "--input", dest="input",
                          help="OGR input data source", metavar="INPUT")
        g.add_option("-o", "--output", dest='output',
                          help="OGR output data source", metavar="OUTPUT")
        g.add_option("-n", "--nooverwrite",
                          action="store_false", dest="overwrite",
                          help="Do not overwrite the existing output file")

        g.add_option("-f", "--driver", dest='driver',
                          help="OGR output driver.  Defaults to \"ESRI Shapefile\"", metavar="DRIVER")

        g.add_option('-w', "--where", dest="where",
                          help="""SQL attribute filter -- enclose in quotes "PNAME='Cedar River'" """,)
        g.add_option('-s', "--spat", dest="spat",
                          help="""Spatial query extents -- minx miny maxx maxy""",
                          type=float, nargs=4)
        g.add_option('-l', "--layer", dest="layer",
                          help="""The name of the input layer to translate, if not given, the first layer on the data source is used""",)

        g.add_option('-k', "--select", dest="fields",
                          help="""Comma separated list of fields to include -- field1,field2,field3,...,fieldn""",)
        g.add_option('-t', '--target-srs', dest="t_srs",
                          help="""Target SRS -- the spatial reference system to project the data to""")
        g.add_option('-a', '--assign-srs', dest="a_srs",
                          help="""Assign SRS -- the spatial reference to assign to the input data""")
        g.add_option("-q", "--quiet",
                          action="store_false", dest="verbose", default=False,
                          help="don't print status messages to stdout")
        parser.add_option_group(g)

        if self.opts:
            g = OptionGroup(parser, "Special Options", "Special options")
            for o in self.opts:
                g.add_option(o)
            parser.add_option_group(g)

        parser.set_defaults(verbose=True, driver="ESRI Shapefile", overwrite=True)

        self.parser = parser

    def __init__(self, arguments, options=None):
        self.input = None
        self.output = None

        self.opts = options
        self.construct_parser()
        self.options, self.args = self.parser.parse_args(args=arguments)

    def process(self):
        self.open()
        self.make_fields()
        self.translate()

    def open(self):
        if self.options.input:
            self.in_ds = ogr.Open(self.options.input)
        else:
            raise Exception("No input layer was specified")
        if self.options.layer:
            self.input = self.in_ds.GetLayerByName(self.options.layer)
            if not self.input:
                raise Exception("The layer '%s' was not found" % self.options.layer)
        else:
            self.input = self.in_ds.GetLayer(0)

        if self.options.a_srs:
            self.in_srs = osr.SpatialReference()
            self.in_srs.SetFromUserInput(self.options.a_srs)
        else:
            self.in_srs = self.input.GetSpatialRef()

        if self.options.spat:
            self.input.SetSpatialFilterRect(*self.options.spat)

        self.out_drv = ogr.GetDriverByName(self.options.driver)

        if self.options.where:
            self.input.SetAttributeFilter(self.options.where)

        if not self.out_drv:
            raise Exception("The '%s' driver was not found, did you misspell it or is it not available in this GDAL build?", self.options.driver)
        if not self.out_drv.TestCapability( 'CreateDataSource' ):
            raise Exception("The '%s' driver does not support creating layers, you will have to choose another output driver", self.options.driver)
        if not self.options.output:
            raise Exception("No output layer was specified")
        if self.options.driver == 'ESRI Shapefile':
            path, filename = os.path.split(os.path.abspath(self.options.output))
            name, ext = os.path.splitext(filename)
            if self.options.overwrite:
            # special case the Shapefile driver, which behaves specially.
                if os.path.exists(os.path.join(path,name,) +'.shp'):
                    os.remove(os.path.join(path,name,) +'.shp')
                    os.remove(os.path.join(path,name,) +'.shx')
                    os.remove(os.path.join(path,name,) +'.dbf')
            else:
                if os.path.exists(os.path.join(path,name,)+".shp"):
                    raise Exception("The file '%s' already exists, but the overwrite option is not specified" % (os.path.join(path,name,)+".shp"))

        if self.options.overwrite:
            dsco = ('OVERWRITE=YES',)
        else:
            dsco = (),

        self.out_ds = self.out_drv.CreateDataSource( self.options.output, dsco)

        if self.options.t_srs:
            self.out_srs = osr.SpatialReference()
            self.out_srs.SetFromUserInput(self.options.t_srs)
        else:
            self.out_srs = None
        self.output = self.out_ds.CreateLayer(  self.options.output,
                                                geom_type = self.input.GetLayerDefn().GetGeomType(),
                                                srs= self.out_srs)


    def make_fields(self):
        defn = self.input.GetLayerDefn()

        if self.options.fields:
            fields = self.options.fields.split(',')
            for i in range(defn.GetFieldCount()):
                fld = defn.GetFieldDefn(i)
                for f in fields:
                    if fld.GetName().upper() == f.upper():
                        self.output.CreateField(fld)
        else:
            for i in range(defn.GetFieldCount()):
                fld = defn.GetFieldDefn(i)
                self.output.CreateField(fld)

    def translate(self, geometry_callback=None, attribute_callback=None):
        f = self.input.GetNextFeature()
        trans = None
        if self.options.t_srs:
            trans = osr.CoordinateTransformation(self.in_srs, self.out_srs)
        while f:
            geom = f.GetGeometryRef().Clone()

            if trans:
                geom.Transform(trans)

            if geometry_callback:
                geom = geometry_callback(geom)

            f.SetGeometry(geom)
            d = ogr.Feature(feature_def=self.output.GetLayerDefn())
            d.SetFrom(f)
            self.output.CreateFeature(d)
            f = self.input.GetNextFeature()

    def __del__(self):
        if self.output:
            self.output.SyncToDisk()

def radians(degrees):
    return math.pi/180.0*degrees
def degrees(radians):
    return radians*180.0/math.pi

class Densify(Translator):

    def calcpoint(self,x0, x1, y0, y1, d):
        a = x1 - x0
        b = y1 - y0

        if a == 0:
            xn = x1

            if b > 0:
                yn = y0 + d
            else:
                yn = y0 - d
            return (xn, yn)

        theta = degrees(math.atan(abs(b)/abs(a)))

        if a > 0 and b > 0:
            omega = theta
        if a < 0 and b > 0:
            omega = 180 - theta
        if a < 0 and b < 0:
            omega = 180 + theta
        if a > 0 and b < 0:
            omega = 360 - theta

        if b == 0:
            yn = y1
            if a > 0:
                xn = x0 + d
            else:
                xn = x0 - d
        else:
            xn = x0 + d*math.cos(radians(omega))
            yn = y0 + d*math.sin(radians(omega))

        return (xn, yn)

    def distance(self, x0, x1, y0, y1):
        deltax = x0 - x1
        deltay = y0 - y1
        d2 = (deltax)**2 + (deltay)**2
        d = math.sqrt(d2)
        return d

    def densify(self, geometry):
        gtype = geometry.GetGeometryType()
        if  not (gtype == ogr.wkbLineString or gtype == ogr.wkbMultiLineString):
            raise Exception("The densify function only works on linestring or multilinestring geometries")

        g = ogr.Geometry(ogr.wkbLineString)

        # add the first point
        x0 = geometry.GetX(0)
        y0 = geometry.GetY(0)
        g.AddPoint(x0, y0)

        for i in range(1,geometry.GetPointCount()):
            threshold = self.options.distance
            x1 = geometry.GetX(i)
            y1 = geometry.GetY(i)
            if not x0 or not y0:
                raise Exception("First point is null")
            d = self.distance(x0, x1, y0, y1)

            if self.options.remainder.upper() == "UNIFORM":
                if d != 0.0:
                    threshold = float(d)/math.ceil(d/threshold)
                else:
                    # duplicate point... throw it out
                    continue
            if (d > threshold):
                if self.options.remainder.upper() == "UNIFORM":
                    segcount = int(math.ceil(d/threshold))

                    dx = (x1 - x0)/segcount
                    dy = (y1 - y0)/segcount

                    x = x0
                    y = y0
                    for p in range(1,segcount):
                        x = x + dx
                        y = y + dy
                        g.AddPoint(x, y)

                elif self.options.remainder.upper() == "END":
                    segcount = int(math.floor(d/threshold))
                    xa = None
                    ya = None
                    for p in range(1,segcount):
                        if not xa:
                            xn, yn = self.calcpoint(x0,x1,y0,y1,threshold)
                            d = self.distance(x0, xn, y0, yn)
                            xa = xn
                            ya = yn
                            g.AddPoint(xa,ya)
                            continue
                        xn, yn = self.calcpoint(xa, x1, ya, y1, threshold)
                        xa = xn
                        ya = yn
                        g.AddPoint(xa,ya)

                elif self.options.remainder.upper() == "BEGIN":

                    # I think this might put an extra point in at the end of the
                    # first segment
                    segcount = int(math.floor(d/threshold))
                    xa = None
                    ya = None
                    #xb = x0
                    #yb = y0
                    remainder = d % threshold
                    for p in range(segcount):
                        if not xa:
                            xn, yn = self.calcpoint(x0,x1,y0,y1,remainder)

                            d = self.distance(x0, xn, y0, yn)
                            xa = xn
                            ya = yn
                            g.AddPoint(xa,ya)
                            continue
                        xn, yn = self.calcpoint(xa, x1, ya, y1, threshold)
                        xa = xn
                        ya = yn
                        g.AddPoint(xa,ya)

            g.AddPoint(x1,y1)
            x0 = x1
            y0 = y1

        return g

    def process(self):
        self.open()
        self.make_fields()
        self.translate(geometry_callback = self.densify)

def GetLength(geometry):

    def get_distance(x1, y1, x2, y2):
        """Return the euclidean distance between this point and another."""
        import math
        deltax = x1 - x2
        deltay = y1 - y2
        d2 = (deltax**2) + (deltay**2)
        distance = math.sqrt(d2)
        return distance

    def cumulate(single):
        cumulative = 0.0
        g = single
        pt_count = g.GetPointCount()
        x1, y1 = g.GetX(0), g.GetY(0)
        for pi in range(1,pt_count):
             x2, y2 = g.GetX(pi), g.GetY(pi)
             length = get_distance(x1, y1, x2, y2)
             cumulative = cumulative + length
             x1 = x2
             y1 = y2
        return cumulative

    cumulative = 0.0
    geom_count = geometry.GetGeometryCount()

    if geom_count:
        for gi in range(geom_count):
            g = geometry.GetGeometryRef(gi)
            cumulative = cumulative + cumulate(g)
    else:
        cumulative = cumulate(geometry)
    return cumulative


def main():
    import optparse

    options = []
    o = optparse.make_option("-r", "--remainder", dest="remainder",
                         type="choice",default='end',
                          help="""what to do with the remainder -- place it at the beginning,
place it at the end, or evenly distribute it across the segment""",
                          choices=['end','begin','uniform'])
    options.append(o)
    o = optparse.make_option("-d", "--distance", dest='distance', type="float",
                          help="""Threshold distance for point placement.  If the
'uniform' remainder is used, points will be evenly placed
along the segment in a fashion that makes sure they are
no further apart than the threshold.  If 'beg' or 'end'
is chosen, the threshold distance will be used as an absolute value.""",
                          metavar="DISTANCE")
    options.append(o)
    d = Densify(sys.argv[1:], options=options)
    d.process()


if __name__=='__main__':
    main()
