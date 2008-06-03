#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL
#  Purpose:  Convert GCPs to a point layer.
#  Author:   Howard Butler, hobu.inc@gmail.com
# 
#******************************************************************************
#  Copyright (c) 2008, Howard Butler <hobu.inc@gmail.com>
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


try:
    from osgeo import osr
    from osgeo import ogr
    ogr.UseExceptions()
except ImportError:
    import osr
    import ogr

import sys
import os





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
        self.opts = options
        self.construct_parser()
        self.options, self.args = self.parser.parse_args(args=arguments)

        self.open()

    def open(self):
        self.in_ds = ogr.Open(self.options.input)
        if self.options.layer:
            self.input = self.in_ds.GetLayerByName(self.options.layer)
            if not self.input:
                raise Exception("The layer '%s' was not found" % self.options.layer)
        else:
            self.input = self.in_ds.GetLayer(0)
        
        if self.options.spat:
            self.input.SetSpatialFilterRect(*self.options.spat)
        self.out_drv = ogr.GetDriverByName(self.options.driver)
        
        if self.options.where:
            self.input.SetAttributeFilter(self.options.where)
            
        if not self.out_drv:
            raise Exception("The '%s' driver was not found, did you misspell it or is it not available in this GDAL build?", self.options.driver)
        if not self.out_drv.TestCapability( 'CreateDataSource' ):
            raise Exception("The '%s' driver does not support creating layers, you will have to choose another output driver", self.options.driver)
        
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
        self.output = self.out_ds.CreateLayer(self.options.output)
        print self.input
        print self.output

    def make_fields(self):
        defn = self.input.GetLayerDefn()

        if self.options.fields:
            fields = self.options.fields.split(',')
            for i in range(defn.GetFieldCount()):
                fld = defn.GetField(i)
                for f in fields:
                    if fld.GetName().upper() == f.upper():
                        
            
class Densify(Translator):
    pass

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

if __name__=='__main__':
    main()
