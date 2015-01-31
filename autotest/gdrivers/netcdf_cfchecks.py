#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NetCDF driver CF compliance.
# Author:   Etienne Tourigny <etourigny.dev at gmail dot com>
# 
###############################################################################
# No copyright in original script...  apparently under BSD licence
# original can be found at http://pypi.python.org/pypi/cfchecker
#
# Modified by Etienne Tourigny for gdal netcdf_cf autotest
# support for automatic version detection with '-v auto'
###############################################################################
# Adapted for numpy/ma/cdms2 by convertcdms.py
#-------------------------------------------------------------
# Name: cfchecks.py
#
# Author: Rosalyn Hatcher - Met Office, UK
#
# Maintainer: Rosalyn Hatcher - NCAS-CMS, Univ. of Reading, UK
#
# Date: September 2011
#
# File Revision: 
#
# CF Checker Version: 2.0.4-gdal
#
#-------------------------------------------------------------
''' cfchecker [-a|--area_types area_types.xml] [-s|--cf_standard_names standard_names.xml] [-u|--udunits udunits.dat] [-v|--version CFVersion] file1 [file2...]

Description:
 The cfchecker checks NetCDF files for compliance to the CF standard.
 
Options:
 -a or --area_types:
       the location of the CF area types table (xml)
       
 -s or --cf_standard_names:
       the location of the CF standard name table (xml)
       
 -u or --udunits:
       the location of the udunits.dat file

 -h or --help: Prints this help text.

 -v or --version: CF version to check against, use auto to auto-detect the file version.

'''

import cdms2 as cdms, re, string, types, numpy.oldnumeric as Numeric, numpy

from cdms2.axis import FileAxis
from cdms2.auxcoord import FileAuxAxis1D

# Use ctypes to interface to the UDUNITS-2 shared library
# The udunits2 library needs to be in a standard path o/w export LD_LIBRARY_PATH
import ctypes
udunits=ctypes.CDLL("libudunits2.so")
 
STANDARDNAME="./cf-standard-name-table.xml"
AREATYPES="./area-type-table.xml"
checkerVersion="2.0.4-gdal"
CFVersions=['CF-1.0','CF-1.1','CF-1.2','CF-1.3','CF-1.4','CF-1.5']
Versions=[1.0,1.1,1.2,1.3,1.4,1.5]

#-----------------------------------------------------------
from xml.sax import ContentHandler
from xml.sax import make_parser
from xml.sax.handler import feature_namespaces


def normalize_whitespace(text):
    "Remove redundant whitespace from a string."
    return ' '.join(text.split())


class ConstructDict(ContentHandler):
    """Parse the xml standard_name table, reading all entries
       into a dictionary; storing standard_name and units.
    """
    def __init__(self):
        self.inUnitsContent = 0
        self.inEntryIdContent = 0
        self.inVersionNoContent = 0
        self.inLastModifiedContent = 0
        self.dict = {}
        
    def startElement(self, name, attrs):
        # If it's an entry element, save the id
        if name == 'entry':
            id = normalize_whitespace(attrs.get('id', ""))
            self.this_id = id

        # If it's the start of a canonical_units element
        elif name == 'canonical_units':
            self.inUnitsContent = 1
            self.units = ""

        elif name == 'alias':
            id = normalize_whitespace(attrs.get('id', ""))
            self.this_id = id

        elif name == 'entry_id':
            self.inEntryIdContent = 1
            self.entry_id = ""

        elif name == 'version_number':
            self.inVersionNoContent = 1
            self.version_number = ""

        elif name == 'last_modified':
            self.inLastModifiedContent = 1
            self.last_modified = ""
            

    def characters(self, ch):
        if self.inUnitsContent:
            self.units = self.units + ch

        elif self.inEntryIdContent:
            self.entry_id = self.entry_id + ch

        elif self.inVersionNoContent:
            self.version_number = self.version_number + ch

        elif self.inLastModifiedContent:
            self.last_modified = self.last_modified + ch

    def endElement(self, name):
        # If it's the end of the canonical_units element, save the units
        if name == 'canonical_units':
            self.inUnitsContent = 0
            self.units = normalize_whitespace(self.units)
            self.dict[self.this_id] = self.units
            
        # If it's the end of the entry_id element, find the units for the self.alias
        elif name == 'entry_id':
            self.inEntryIdContent = 0
            self.entry_id = normalize_whitespace(self.entry_id)
            self.dict[self.this_id] = self.dict[self.entry_id]

        # If it's the end of the version_number element, save it
        elif name == 'version_number':
            self.inVersionNoContent = 0
            self.version_number = normalize_whitespace(self.version_number)

        # If it's the end of the last_modified element, save the last modified date
        elif name == 'last_modified':
            self.inLastModifiedContent = 0
            self.last_modified = normalize_whitespace(self.last_modified)

class ConstructList(ContentHandler):
    """Parse the xml area_type table, reading all area_types 
       into a list.
    """
    def __init__(self):
        self.inVersionNoContent = 0
        self.inLastModifiedContent = 0
        self.list = []
        
    def startElement(self, name, attrs):
        # If it's an entry element, save the id
        if name == 'entry':
            id = normalize_whitespace(attrs.get('id', ""))
            self.list.append(id)

        elif name == 'version_number':
            self.inVersionNoContent = 1
            self.version_number = ""

        elif name == 'date':
            self.inLastModifiedContent = 1
            self.last_modified = ""

    def characters(self, ch):
        if self.inVersionNoContent:
            self.version_number = self.version_number + ch

        elif self.inLastModifiedContent:
            self.last_modified = self.last_modified + ch

    def endElement(self, name):
        # If it's the end of the version_number element, save it
        if name == 'version_number':
            self.inVersionNoContent = 0
            self.version_number = normalize_whitespace(self.version_number)

        # If it's the end of the date element, save the last modified date
        elif name == 'date':
            self.inLastModifiedContent = 0
            self.last_modified = normalize_whitespace(self.last_modified)

            
def chkDerivedName(name):
    """Checks whether name is a derived standard name and adheres
       to the transformation rules. See CF standard names document
       for more information.
    """
    if re.search("^(direction|magnitude|square|divergence)_of_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^rate_of_change_of_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^(grid_)?(northward|southward|eastward|westward)_derivative_of_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^product_of_[a-zA-Z][a-zA-Z0-9_]*_and_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^ratio_of_[a-zA-Z][a-zA-Z0-9_]*_to_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^derivative_of_[a-zA-Z][a-zA-Z0-9_]*_wrt_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^(correlation|covariance)_over_[a-zA-Z][a-zA-Z0-9_]*_of_[a-zA-Z][a-zA-Z0-9_]*_and_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^histogram_over_[a-zA-Z][a-zA-Z0-9_]*_of_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^probability_distribution_over_[a-zA-Z][a-zA-Z0-9_]*_of_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0

    if re.search("^probability_density_function_over_[a-zA-Z][a-zA-Z0-9_]*_of_[a-zA-Z][a-zA-Z0-9_]*$",name):
        return 0
    
    # Not a valid derived name
    return 1


#======================
# Checking class
#======================
class CFChecker:
    
  def __init__(self, uploader=None, useFileName="yes", badc=None, coards=None, cfStandardNamesXML=None, cfAreaTypesXML=None, udunitsDat=None, version=Versions[-1]):
      self.uploader = uploader
      self.useFileName = useFileName
      self.badc = badc
      self.coards = coards
      self.standardNames = cfStandardNamesXML
      self.areaTypes = cfAreaTypesXML
      self.udunits = udunitsDat
      self.version = version
      self.err = 0
      self.warn = 0
      self.info = 0

  def checker(self, file):
    # Set up dictionary of all valid attributes, their type and use
    self.setUpAttributeList()
    fileSuffix = re.compile('^\S+\.nc$')

    lowerVars=[]
    rc=1

    print ""
    if self.uploader:
        realfile = string.split(file,".nc")[0]+".nc"
        print "CHECKING NetCDF FILE:", realfile
    elif self.useFileName=="no":
        print "CHECKING NetCDF FILE"
    else:
        print "CHECKING NetCDF FILE:",file
    print "====================="
    
    # Check for valid filename
    if not fileSuffix.match(file):
        print "ERROR (2.1): Filename must have .nc suffix"
        exit(1)

    # Initialize udunits-2 package
    # (Temporarily ignore messages to std error stream to prevent "Definition override" warnings
    # being dislayed see Trac #50)
    # Use ctypes callback functions to declare ut_error_message_handler (uemh)
    # Don't fully understand why this works!  Solution supplied by ctypes-mailing-list. 19.01.10
    uemh = ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_char_p)
    ut_set_error_message_handler = ctypes.CFUNCTYPE(uemh,uemh)(("ut_set_error_message_handler",udunits))
    ut_write_to_stderr = uemh(("ut_write_to_stderr",udunits))
    ut_ignore = uemh(("ut_ignore",udunits))

    ut_set_error_message_handler(ut_ignore)
                                           
    self.unitSystem=udunits.ut_read_xml(self.udunits)
    if not self.unitSystem:
        exit("Could not read the UDUNITS2 xml database from: %s" % self.udunits)

    ut_set_error_message_handler(ut_write_to_stderr)

    # Read in netCDF file
    try:
        self.f=cdms.open(file,"r")

    except AttributeError:
        print "NetCDF Attribute Error:"
        raise
    except:
        print "\nCould not open file, please check that NetCDF is formatted correctly.\n".upper()
        print "ERRORS detected:",1
        raise
        exit(1)

    #if 'auto' version, check the CF version in the file
    #if none found, use the default
    check_auto = (self.version == 0.0)
    if check_auto:
        self.version = self.getFileCFVersion()
        if self.version == 0.0:
            self.version = Versions[-1]

    # Set up dictionary of standard_names and their assoc. units
    parser = make_parser()
    parser.setFeature(feature_namespaces, 0)
    self.std_name_dh = ConstructDict()
    parser.setContentHandler(self.std_name_dh)
    parser.parse(self.standardNames)

    if self.version >= 1.4:
        # Set up list of valid area_types
        self.area_type_lh = ConstructList()
        parser.setContentHandler(self.area_type_lh)
        parser.parse(self.areaTypes)
    
    print "Using CF Checker Version",checkerVersion
    if check_auto:
        print "Checking againt CF Version",str(self.version),"(auto)"
    else:
        print "Checking againt CF Version",str(self.version)
    print "Using Standard Name Table Version "+self.std_name_dh.version_number+" ("+self.std_name_dh.last_modified+")"
    if self.version >= 1.4:
        print "Using Area Type Table Version "+self.area_type_lh.version_number+" ("+self.area_type_lh.last_modified+")"
    print ""
    
    # Check global attributes
    if not self.chkGlobalAttributes(): rc=0
        
    (coordVars,auxCoordVars,boundsVars,climatologyVars,gridMappingVars)=self.getCoordinateDataVars()
    self.coordVars = coordVars
    self.auxCoordVars = auxCoordVars
    self.boundsVars = boundsVars
    self.climatologyVars = climatologyVars
    self.gridMappingVars = gridMappingVars

    allCoordVars=coordVars[:]
    allCoordVars[len(allCoordVars):]=auxCoordVars[:]

    self.setUpFormulas()
    
    axes=self.f.axes.keys()

    # Check each variable
    for var in self.f._file_.variables.keys():
        print ""
        print "------------------"
        print "Checking variable:",var
        print "------------------"

        if not self.validName(var):
            print "ERROR (2.3): Invalid variable name -",var
            self.err = self.err+1
            rc=0

        # Check to see if a variable with this name already exists (case-insensitive)
        lowerVar=var.lower()
        if lowerVar in lowerVars:
            print "WARNING (2.3): variable clash:-",var
            self.warn = self.warn + 1
        else:
            lowerVars.append(lowerVar)

        if var not in axes:
            # Non-coordinate variable
            if not self.chkDimensions(var,allCoordVars): rc=0
        
        if not self.chkDescription(var): rc=0

        for attribute in self.f[var].attributes.keys():
            if not self.chkAttribute(attribute,var,allCoordVars): rc=0

        if not self.chkUnits(var,allCoordVars): rc=0
        
        if not self.chkValidMinMaxRange(var): rc=0

        if not self.chk_FillValue(var): rc=0

        if not self.chkAxisAttribute(var): rc=0

        if not self.chkPositiveAttribute(var): rc=0

        if not self.chkCellMethods(var): rc=0

        if not self.chkCellMeasures(var): rc=0
        
        if not self.chkFormulaTerms(var,allCoordVars): rc=0

        if not self.chkCompressAttr(var): rc=0

        if not self.chkPackedData(var): rc=0

        if self.version >= 1.3:
            # Additional conformance checks from CF-1.3 onwards
            if not self.chkFlags(var): rc=0

        if var in coordVars:
            if not self.chkMultiDimCoord(var, axes): rc=0
            if not self.chkValuesMonotonic(var): rc=0

        if var in gridMappingVars:
            if not self.chkGridMappingVar(var) : rc=0

        if var in axes:
            # Check var is a FileAxis.  If not then there may be a problem with its declaration.
            # I.e. Multi-dimensional coordinate var with a dimension of the same name
            # or an axis that hasn't been identified through the coordinates attribute
            # CRM035 (17.04.07)
            if not (isinstance(self.f[var], FileAxis) or isinstance(self.f[var], FileAuxAxis1D)):
                print "WARNING (5): Possible incorrect declaration of a coordinate variable."
                self.warn = self.warn+1
            else:    
                if self.f[var].isTime():
                    if not self.chkTimeVariableAttributes(var): rc=0

    print ""
    print "ERRORS detected:",self.err
    print "WARNINGS given:",self.warn
    print "INFORMATION messages:",self.info
    print "rc:",rc

    if self.err:
        # Return number of errors found
        return self.err
    elif self.warn:
        # No errors, but some warnings found
        return -(self.warn)
    else:
        # No errors or warnings - return success!
        return 0


  #-----------------------------
  def setUpAttributeList(self):
  #-----------------------------
      """Set up Dictionary of valid attributes, their corresponding
      Type; S(tring), N(umeric) D(ata variable type)  and Use C(oordinate),
      D(ata non-coordinate) or G(lobal) variable."""
    
      self.AttrList={}
      self.AttrList['add_offset']=['N','D']
      self.AttrList['ancillary_variables']=['S','D']
      self.AttrList['axis']=['S','C']
      self.AttrList['bounds']=['S','C']
      self.AttrList['calendar']=['S','C']
      self.AttrList['cell_measures']=['S','D']
      self.AttrList['cell_methods']=['S','D']
      self.AttrList['climatology']=['S','C']
      self.AttrList['comment']=['S',('G','D')]
      self.AttrList['compress']=['S','C']
      self.AttrList['Conventions']=['S','G']
      self.AttrList['coordinates']=['S','D']
      self.AttrList['_FillValue']=['D','D']

      if self.version >= 1.3:
          self.AttrList['flag_masks']=['D','D']
          
      self.AttrList['flag_meanings']=['S','D']
      self.AttrList['flag_values']=['D','D']
      self.AttrList['formula_terms']=['S','C']
      self.AttrList['grid_mapping']=['S','D']
      self.AttrList['history']=['S','G']
      self.AttrList['institution']=['S',('G','D')]
      self.AttrList['leap_month']=['N','C']
      self.AttrList['leap_year']=['N','C']
      self.AttrList['long_name']=['S',('C','D')]
      self.AttrList['missing_value']=['D','D']
      self.AttrList['month_lengths']=['N','C']
      self.AttrList['positive']=['S','C']
      self.AttrList['references']=['S',('G','D')]
      self.AttrList['scale_factor']=['N','D']
      self.AttrList['source']=['S',('G','D')]
      self.AttrList['standard_error_multiplier']=['N','D']
      self.AttrList['standard_name']=['S',('C','D')]
      self.AttrList['title']=['S','G']
      self.AttrList['units']=['S',('C','D')]
      self.AttrList['valid_max']=['N',('C','D')]
      self.AttrList['valid_min']=['N',('C','D')]
      self.AttrList['valid_range']=['N',('C','D')]
      
      return


  #---------------------------
  def uniqueList(self, list):
  #---------------------------
      """Determine if list has any repeated elements."""
      # Rewrite to allow list to be either a list or a Numeric array
      seen=[]

      for x in list:
          if x in seen:
              return 0
          else:
              seen.append(x)        
      return 1


  #-------------------------
  def isNumeric(self, var):
  #-------------------------
      """Determine if variable is of Numeric data type."""
      types=['i','f','d']
      rc=1 
      if self.f[var].dtype.char not in types:
          rc=0
      return rc

  #-------------------------
  def getStdNameOld(self, var):
  #-------------------------
      """Get standard_name of variable (i.e. just first part of standard_name attribute, without modifier)"""
      attName = 'standard_name'
      attDict = var.attributes
      if attName not in attDict.keys():
          return None
      bits = string.split(attDict[attName])
      if bits:
          return bits[0]
      else:
          return ""
 
  #-------------------------
  def getStdName(self, var):
  #-------------------------
      """Get standard_name of variable.  Return it as 2 parts - the standard name and the modifier, if present."""
      attName = 'standard_name'
      attDict = var.attributes

      if attName not in attDict.keys():
          return None

      bits = string.split(attDict[attName])
      
      if len(bits) == 1:
          # Only standard_name part present
          return (bits[0],"")
      elif len(bits) == 0:
          # Standard Name is blank
          return ("","")
      else:
          # At least 2 elements so return the first 2.  
          # If there are more than 2, which is invalid syntax, this will have been picked up by chkDescription()
          return (bits[0],bits[1])
    
  #--------------------------------------------------
  def getInterpretation(self, units, positive=None):
  #--------------------------------------------------
    """Determine the interpretation (time - T, height or depth - Z,
    latitude - Y or longitude - X) of a dimension."""

    if units in ['level','layer','sigma_level']:
        # Dimensionless vertical coordinate
        return "Z"

    # Parse the string representation of units into its binary representation for use by udunits
    binaryUnit = udunits.ut_parse(self.unitSystem, units, "UT_ASCII")
    if not binaryUnit:
        # Don't print this message out o/w it is repeated for every variable
        # that has this dimension.  CRM033 return "None" instead
        # print "ERROR: Invalid units:",units
        #self.err = self.err+1
        return None

#    print "here"
    
    # Time Coordinate
    # 19.08.10 - Workaround since udunits2 deems a unit without reference time not convertible to a
    # unit with reference time and vice versa
    if udunits.ut_are_convertible(binaryUnit, udunits.ut_parse(self.unitSystem, "second", "UT_ASCII")):
        return "T"
    elif udunits.ut_are_convertible(binaryUnit, udunits.ut_parse(self.unitSystem, "seconds since 1-1-1 0:0:0", "UT_ASCII")):
        return "T"
    
    # Vertical Coordinate
    if positive and re.match('(up|down)',positive,re.I):
        return "Z"

    # Variable is a vertical coordinate if the units are dimensionally
    # equivalent to Pressure
    if udunits.ut_are_convertible(binaryUnit, udunits.ut_parse(self.unitSystem, "Pa", "UT_ASCII")):
        return "Z"
        
    # Latitude Coordinate
    if re.match('(degrees_north|degree_north|degrees_N|degree_N|degreesN|degreeN)',units):
        return "Y"
        
    # Longitude Coordinate
    if re.match('(degrees_east|degree_east|degrees_E|degree_E|degreesE|degreeE)',units):
        return "X"

    # Not possible to deduce interpretation
    return None


  #--------------------------------
  def getCoordinateDataVars(self):
  #--------------------------------
    """Obtain list of coordinate data variables, boundary
    variables, climatology variables and grid_mapping variables."""
    
    variables=self.f.variables.keys()     # List of variables, but doesn't include coord vars
    allVariables=self.f._file_.variables.keys()   # List of all vars, including coord vars
    axes=self.f.axes.keys()
    
    coordVars=[]
    boundaryVars=[]
    climatologyVars=[]
    gridMappingVars=[]
    auxCoordVars=[]

    for var in allVariables:
        if var not in variables:
            # Coordinate variable - 1D & dimension is the same name as the variable
            coordVars.append(var)

## Commented out 21.02.06 - Duplicate code also in method chkDimensions
## Probably can be completely removed.
##         if var not in coordVars:
##             # Non-coordinate variable so check if it has any repeated dimensions
##             dimensions=self.f[var].getAxisIds()
##             dimensions.sort()
##             if not self.uniqueList(dimensions):
##                 print "ERROR: variable has repeated dimensions"
##                 self.err = self.err+1

        #------------------------
        # Auxilliary Coord Checks
        #------------------------
        if self.f[var].attributes.has_key('coordinates'):
            # Check syntax of 'coordinates' attribute
            if not self.parseBlankSeparatedList(self.f[var].attributes['coordinates']):
                print "ERROR (5): Invalid syntax for 'coordinates' attribute in",var
                self.err = self.err+1
            else:
                coordinates=string.split(self.f[var].attributes['coordinates'])
                for dataVar in coordinates:
                    if dataVar in variables:

                        auxCoordVars.append(dataVar)

                        # Is the auxillary coordinate var actually a label?
                        if self.f[dataVar].dtype.char == 'c':
                            # Label variable
                            num_dimensions = len(self.f[dataVar].getAxisIds())
                            if self.version < 1.4:
                                if not num_dimensions == 2:
                                    print "ERROR (6.1): Label variable",dataVar,"must have 2 dimensions only"
                                    self.err = self.err+1

                            if self.version >= 1.4:
                                if num_dimensions != 1 and num_dimensions != 2:
                                    print "ERROR (6.1): Label variable",dataVar,"must have 1 or 2 dimensions, but has",num_dimensions
                                    self.err = self.err+1

                            if num_dimensions == 2:
                                if self.f[dataVar].getAxisIds()[0] not in self.f[var].getAxisIds():
                                    print "ERROR (6.1): Leading dimension of",dataVar,"must match one of those for",var
                                    self.err = self.err+1
                        else:
                            # Not a label variable
                            for dim in self.f[dataVar].getAxisIds():
                                if dim not in self.f[var].getAxisIds():
                                    print "ERROR (5): Dimensions of",dataVar,"must be a subset of dimensions of",var
                                    self.err = self.err+1
                                    break
                    elif dataVar not in allVariables:
                        print "ERROR (5): coordinates attribute referencing non-existent variable:",dataVar
                        self.err = self.err+1

        #-------------------------
        # Boundary Variable Checks
        #-------------------------
        if self.f[var].attributes.has_key('bounds'):
            bounds=self.f[var].attributes['bounds']
            # Check syntax of 'bounds' attribute
            if not re.search("^[a-zA-Z0-9_]*$",bounds):
                print "ERROR (7.1): Invalid syntax for 'bounds' attribute"
                self.err = self.err+1
            else:
                if bounds in variables:
                    boundaryVars.append(bounds)

                    if not self.isNumeric(bounds):
                        print "ERROR (7.1): boundary variable with non-numeric data type"
                        self.err = self.err+1
                    if len(self.f[var].shape) + 1 == len(self.f[bounds].shape):
                        if var in axes:
                            varDimensions=[var]
                        else:
                            varDimensions=self.f[var].getAxisIds()

                        for dim in varDimensions:
                            if dim not in self.f[bounds].getAxisIds():
                                print "ERROR (7.1): Incorrect dimensions for boundary variable:",bounds
                                self.err = self.err+1
                    else:
                        print "ERROR (7.1): Incorrect number of dimensions for boundary variable:",bounds
                        self.err = self.err+1

                    if self.f[bounds].attributes.has_key('units'):
                        if self.f[bounds].attributes['units'] != self.f[var].attributes['units']:
                            print "ERROR (7.1): Boundary var",bounds,"has inconsistent units to",var
                            self.err = self.err+1
                    if self.f[bounds].attributes.has_key('standard_name') and self.f[var].attributes.has_key('standard_name'):
                        if self.f[bounds].attributes['standard_name'] != self.f[var].attributes['standard_name']:
                            print "ERROR (7.1): Boundary var",bounds,"has inconsistent std_name to",var
                            self.err = self.err+1
                else:
                    print "ERROR (7.1): bounds attribute referencing non-existent variable:",bounds
                    self.err = self.err+1
                    
            # Check that points specified by a coordinate or auxilliary coordinate
            # variable should lie within, or on the boundary, of the cells specified by
            # the associated boundary variable.
            if bounds in variables:
                # Is boundary variable 2 dimensional?  If so can check that points
                # lie within, or on the boundary.
                if len(self.f[bounds].getAxisIds()) <= 2:
                    varData=self.f[var].getValue()
                    boundsData=self.f[bounds].getValue()
##                    if len(varData) == 1:

#                    if type(varData) == type(1) or type(varData) == type(1.00) or len(varData) == 1:
                    if isinstance(varData,(int,long,float,numpy.floating)) or len(varData) == 1:
                        # Gone for belts and braces approach here!!
                        # Variable contains only one value
                        # Bounds array will be 1 dimensional
                        if not ((varData <= boundsData[0] and varData >= boundsData[1])
                                or (varData >= boundsData[0] and varData <= boundsData[1])):
                            print "WARNING (7.1): Data for variable",var,"lies outside cell boundaries"
                            self.warn = self.warn+1
                    else:
                        i=0
                        for value in varData:
                            if not ((value <= boundsData[i][0] and value >= boundsData[i][1]) \
                                    or (value >= boundsData[i][0] and value <= boundsData[i][1])):
                                print "WARNING (7.1): Data for variable",var,"lies outside cell boundaries"
                                self.warn = self.warn+1
                                break
                            i=i+1

        #----------------------------
        # Climatology Variable Checks
        #----------------------------
        if self.f[var].attributes.has_key('climatology'):
            climatology=self.f[var].attributes['climatology']
            # Check syntax of 'climatology' attribute
            if not re.search("^[a-zA-Z0-9_]*$",climatology):
                print "ERROR (7.4): Invalid syntax for 'climatology' attribute"
                self.err = self.err+1
            else:
                if climatology in variables:
                    climatologyVars.append(climatology)
                    if not self.isNumeric(climatology):
                        print "ERROR (7.4): climatology variable with non-numeric data type"
                        self.err = self.err+1
                    if self.f[climatology].attributes.has_key('units'):
                        if self.f[climatology].attributes['units'] != self.f[var].attributes['units']:
                            print "ERROR (7.4): Climatology var",climatology,"has inconsistent units to",var
                            self.err = self.err+1
                    if self.f[climatology].attributes.has_key('standard_name'):
                        if self.f[climatology].attributes['standard_name'] != self.f[var].attributes['standard_name']:
                            print "ERROR (7.4): Climatology var",climatology,"has inconsistent std_name to",var
                            self.err = self.err+1
                    if self.f[climatology].attributes.has_key('calendar'):
                        if self.f[climatology].attributes['calendar'] != self.f[var].attributes['calendar']:
                            print "ERROR (7.4): Climatology var",climatology,"has inconsistent calendar to",var
                            self.err = self.err+1
                else:
                    print "ERROR (7.4): climatology attribute referencing non-existent variable"
                    self.err = self.err+1

        #------------------------------------------
        # Is there a grid_mapping variable?
        #------------------------------------------
        if self.f[var].attributes.has_key('grid_mapping'):
            grid_mapping = self.f[var].attributes['grid_mapping']
            # Check syntax of grid_mapping attribute: a string whose value is a single variable name.
            if not re.search("^[a-zA-Z0-9_]*$",grid_mapping):
                print "ERROR (5.6):",var,"- Invalid syntax for 'grid_mapping' attribute"
                self.err = self.err+1
            else:
                if grid_mapping in variables:
                    gridMappingVars.append(grid_mapping)
                else:
                    print "ERROR (5.6): grid_mapping attribute referencing non-existent variable",grid_mapping
                    self.err = self.err+1
                    
    return (coordVars, auxCoordVars, boundaryVars, climatologyVars, gridMappingVars)


  #-------------------------------------
  def chkGridMappingVar(self, varName):
  #-------------------------------------
      """Section 5.6: Grid Mapping Variable Checks"""
      rc=1
      var=self.f[varName]
      
      if var.attributes.has_key('grid_mapping_name'):
          # Check grid_mapping_name is valid
          validNames = ['albers_conical_equal_area','azimuthal_equidistant','lambert_azimuthal_equal_area',
                        'lambert_conformal_conic','polar_stereographic','rotated_latitude_longitude',
                        'stereographic','transverse_mercator']
          
          if self.version >= 1.2:
              # Extra grid_mapping_names at vn1.2
              validNames[len(validNames):] = ['latitude_longitude','vertical_perspective']

          if self.version >= 1.4:
              # Extra grid_mapping_names at vn1.4
              validNames[len(validNames):] = ['lambert_cylindrical_equal_area','mercator','orthographic']
              
          if var.grid_mapping_name not in validNames:
              print "ERROR (5.6): Invalid grid_mapping_name:",var.grid_mapping_name
              self.err = self.err+1
              rc=0
      else:
          print "ERROR (5.6): No grid_mapping_name attribute set"
          self.err = self.err+1
          rc=0
              
      if len(var.getAxisIds()) != 0:
          print "WARNING (5.6): A grid mapping variable should have 0 dimensions"
          self.warn = self.warn+1

      return rc


  #------------------------
  def setUpFormulas(self):
  #------------------------
      """Set up dictionary of all valid formulas"""
      self.formulas={}
      self.alias={}
      self.alias['atmosphere_ln_pressure_coordinate']='atmosphere_ln_pressure_coordinate'
      self.alias['atmosphere_sigma_coordinate']='sigma'
      self.alias['sigma']='sigma'
      self.alias['atmosphere_hybrid_sigma_pressure_coordinate']='hybrid_sigma_pressure'
      self.alias['hybrid_sigma_pressure']='hybrid_sigma_pressure'
      self.alias['atmosphere_hybrid_height_coordinate']='atmosphere_hybrid_height_coordinate'
      self.alias['ocean_sigma_coordinate']='ocean_sigma_coordinate'
      self.alias['ocean_s_coordinate']='ocean_s_coordinate'
      self.alias['ocean_sigma_z_coordinate']='ocean_sigma_z_coordinate'
      self.alias['ocean_double_sigma_coordinate']='ocean_double_sigma_coordinate'
      

      self.formulas['atmosphere_ln_pressure_coordinate']=['p(k)=p0*exp(-lev(k))']
      self.formulas['sigma']=['p(n,k,j,i)=ptop+sigma(k)*(ps(n,j,i)-ptop)']

      self.formulas['hybrid_sigma_pressure']=['p(n,k,j,i)=a(k)*p0+b(k)*ps(n,j,i)'
                                              ,'p(n,k,j,i)=ap(k)+b(k)*ps(n,j,i)']

      self.formulas['atmosphere_hybrid_height_coordinate']=['z(n,k,j,i)=a(k)+b(k)*orog(n,j,i)']

      self.formulas['ocean_sigma_coordinate']=['z(n,k,j,i)=eta(n,j,i)+sigma(k)*(depth(j,i)+eta(n,j,i))']
      
      self.formulas['ocean_s_coordinate']=['z(n,k,j,i)=eta(n,j,i)*(1+s(k))+depth_c*s(k)+(depth(j,i)-depth_c)*C(k)'
                                           ,'C(k)=(1-b)*sinh(a*s(k))/sinh(a)+b*[tanh(a*(s(k)+0.5))/(2*tanh(0.5*a))-0.5]']

      self.formulas['ocean_sigma_z_coordinate']=['z(n,k,j,i)=eta(n,j,i)+sigma(k)*(min(depth_c,depth(j,i))+eta(n,j,i))'
                                                 ,'z(n,k,j,i)=zlev(k)']

      self.formulas['ocean_double_sigma_coordinate']=['z(k,j,i)=sigma(k)*f(j,i)'
                                                      ,'z(k,j,i)=f(j,i)+(sigma(k)-1)*(depth(j,i)-f(j,i))'
                                                      ,'f(j,i)=0.5*(z1+z2)+0.5*(z1-z2)*tanh(2*a/(z1-z2)*(depth(j,i)-href))']

      
  #----------------------------------------
  def parseBlankSeparatedList(self, list):
  #----------------------------------------
      """Parse blank separated list"""
      if re.match("^[a-zA-Z0-9_ ]*$",list):
          return 1
      else:
          return 0


  #-------------------------------------------
  def extendedBlankSeparatedList(self, list):
  #-------------------------------------------
      """Check list is a blank separated list of words containing alphanumeric characters
      plus underscore '_', period '.', plus '+', hyphen '-', or "at" sign '@'."""
      if re.match("^[a-zA-Z0-9_ @\-\+\.]*$",list):
          return 1
      else:
          return 0
         
  
  #------------------------------
  def chkGlobalAttributes(self):
  #------------------------------
    """Check validity of global attributes."""
    rc=1
    if self.f.attributes.has_key('Conventions'):
        conventions = self.f.attributes['Conventions']
        
        if conventions not in CFVersions:
            print "ERROR (2.6.1): This netCDF file does not appear to contain CF Convention data."
            self.err = self.err+1
            rc=0

        if conventions != 'CF-'+str(self.version):
            print "WARNING: Inconsistency - The conventions attribute is set to "+conventions+", but you've requested a validity check against CF",self.version
            self.warn = self.warn+1
            
    else:
        print "WARNING (2.6.1): No 'Conventions' attribute present"
        self.warn = self.warn+1
        rc=1

    for attribute in ['title','history','institution','source','reference','comment']:
        if self.f.attributes.has_key(attribute):
            if type(self.f.attributes[attribute]) != types.StringType:
                print "ERROR (2.6.2): Global attribute",attribute,"must be of type 'String'"
                self.err = self.err+1
    return rc


  #------------------------------
  def getFileCFVersion(self):
  #------------------------------
    """Return CF version of file, used for auto version option. If Conventions is COARDS return CF-1.0, else 0.0"""
    rc = 0.0
    if self.f.attributes.has_key('Conventions'):
        conventions = self.f.attributes['Conventions']
        
        if conventions == 'COARDS':
            print "WARNING: The conventions attribute is set to "+conventions+", assuming CF-1.0"
            rc = 1.0
        elif conventions not in CFVersions:
            rc = 0.0
        else:
            rc = float(conventions[3:])
    else:
        rc = 0.0

    return rc


  #--------------------------
  def validName(self, name):
  #--------------------------
    """ Check for valid name.  They must begin with a
    letter and be composed of letters, digits and underscores."""

    nameSyntax = re.compile('^[a-zA-Z][a-zA-Z0-9_]*$')
    if not nameSyntax.match(name):
        return 0

    return 1


  #---------------------------------------------
  def chkDimensions(self,varName,allcoordVars):
  #---------------------------------------------
    """Check variable has non-repeated dimensions, that
       space/time dimensions are listed in the order T,Z,Y,X
       and that any non space/time dimensions are added to
       the left of the space/time dimensions, unless it
       is a boundary variable or climatology variable, where
       1 trailing dimension is allowed."""

    var=self.f[varName]
    dimensions=var.getAxisIds()
    trailingVars=[]
    
    if len(dimensions) > 1:
        order=['T','Z','Y','X']
        axesFound=[0,0,0,0] # Holding array to record whether a dimension with an axis value has been found.
        i=-1
        lastPos=-1
        #trailing=0   # Flag to indicate trailing dimension
        
        # Flags to hold positions of first space/time dimension and
        # last Non-space/time dimension in variable declaration.
        firstST=-1
        lastNonST=-1
        nonSpaceDimensions=[]

        for dim in dimensions:
            i=i+1
            try:
                if hasattr(self.f[dim],'axis'):
                    pos=order.index(self.f[dim].axis)

                    # Is there already a dimension with this axis attribute specified.
                    if axesFound[pos] == 1:
                        print "ERROR (4): Variable has more than 1 coordinate variable with same axis value"
                        self.err = self.err+1
                    else:
                        axesFound[pos] = 1
                else:
                    # Determine interpretation of variable by units attribute
                    if hasattr(self.f[dim],'positive'):
                        interp=self.getInterpretation(self.f[dim].units,self.f[dim].positive)
                    else:
                        interp=self.getInterpretation(self.f[dim].units)

                    if not interp: raise ValueError
                    pos=order.index(interp)

                if firstST == -1:
                    firstST=pos
            except AttributeError:
                print "ERROR: Problem accessing variable:",dim,"(May not exist in file)."
                self.err = self.err+1
                exit(self.err)
            except ValueError:
                # Dimension is not T,Z,Y or X axis
                nonSpaceDimensions.append(dim)
                trailingVars.append(dim)
                lastNonST=i
            else:
                # Is the dimensional position of this dimension further to the right than the previous dim?
                if pos >= lastPos:
                    lastPos=pos
                    trailingVars=[]
                else:
                    print "WARNING (2.4): space/time dimensions appear in incorrect order"
                    self.warn = self.warn+1

        # As per CRM #022 
        # This check should only be applied for COARDS conformance.
        if self.coards:
            validTrailing=self.boundsVars[:]
            validTrailing[len(validTrailing):]=self.climatologyVars[:]
            if lastNonST > firstST and firstST != -1:
                if len(trailingVars) == 1:
                    if var.id not in validTrailing:
                        print "WARNING (2.4): dimensions",nonSpaceDimensions,"should appear to left of space/time dimensions"
                        self.warn = self.warn+1
                else:
                    print "WARNING (2.4): dimensions",nonSpaceDimensions,"should appear to left of space/time dimensions"
                    self.warn = self.warn+1

                
        dimensions.sort()
        if not self.uniqueList(dimensions):
            print "ERROR (2.4): variable has repeated dimensions"
            self.err = self.err+1

## Removed this check as per emails 11 June 2004 (See CRM #020)
##     # Check all dimensions of data variables have associated coordinate variables
##     for dim in dimensions:
##         if dim not in f._file_.variables.keys() or dim not in allcoordVars:
##             if dim not in trailingVars:
##                 # dim is not a valid trailing dimension. (valid trailing dimensions e.g. for bounds
##                 # vars; do not need to have an associated coordinate variable CF doc 7.1)
##                 print "WARNING: Dimension:",dim,"does not have an associated coordinate variable"
##                 self.warn = self.warn+1


  #-------------------------------------------------------
  def chkAttribute(self, attribute,varName,allCoordVars):
  #-------------------------------------------------------
    """Check the syntax of the attribute name, that the attribute
    is of the correct type and that it is attached to the right
    kind of variable."""
    rc=1
    var=self.f[varName]

    if not self.validName(attribute) and attribute != "_FillValue":
        print "ERROR: Invalid attribute name -",attribute
        self.err = self.err+1
        return 0

    value=var.attributes[attribute]

    #------------------------------------------------------------
    # Attribute of wrong 'type' in the sense numeric/non-numeric
    #------------------------------------------------------------
    if self.AttrList.has_key(attribute):
        # Standard Attribute, therefore check type

        attrType=type(value)

        if attrType == types.StringType:
            attrType='S'
        elif attrType == types.IntType or attrType == types.FloatType:
            attrType='N'
        elif attrType == type(Numeric.array([])):
            attrType='N'
        elif attrType == types.NoneType:
            #attrType=self.AttrList[attribute][0]
            attrType='NoneType'
        else:
            print "Unknown Type for attribute:",attribute,attrType


        # If attrType = 'NoneType' then it has been automatically created e.g. missing_value
        typeError=0
        if attrType != 'NoneType':
            if self.AttrList[attribute][0] == 'D':
                # Special case for 'D' as these attributes will always be caught
                # by one of the above cases.
                # Attributes of type 'D' should be the same type as the data variable
                # they are attached to.
                if attrType == 'S':
                    # Note: A string is an array of chars
                    if var.dtype.char != 'c':
                        typeError=1
                else:
                    # 26.02.10 - CDAT-5.2 - An inconsistency means that determining the type of
                    # a FileAxis or FileVariable is different.  C.Doutriaux will hopefully
                    # make this more uniform (Raised on the cdat mailing list) CF Trac #
                    if varName in self.f.axes.keys():
                        # FileAxis Variable
                        if var.typecode() != var.attributes[attribute].dtype.char:
                            typeError=1
                    else:
                        # FileVariable
                        if var.dtype.char != var.attributes[attribute].dtype.char:
                            typeError=1
                    
            elif self.AttrList[attribute][0] != attrType:
                typeError=1

            if typeError:
                print "ERROR: Attribute",attribute,"of incorrect type"
                self.err = self.err+1
                rc=0
                        
        # Attribute attached to the wrong kind of variable
        uses=self.AttrList[attribute][1]
        usesLen=len(uses)
        i=1
        for use in uses:
            if use == "C" and var.id in allCoordVars:
                # Valid association
                break
            elif use == "D" and var.id not in allCoordVars:
                # Valid association
                break
            elif i == usesLen:
                if attribute == "missing_value":
                    # Special case since missing_value attribute is present for all
                    # variables whether set explicitly or not. Is this a cdms thing?
                    # Using var.missing_value is null then missing_value not set in the file
                    if var.missing_value:
                        print "WARNING: attribute",attribute,"attached to wrong kind of variable"
                        self.warn = self.warn+1
                else:
                    print "INFO: attribute '" + attribute + "' is being used in a non-standard way"
                    self.info = self.info+1
            else:
                i=i+1


        # Check no time variable attributes. E.g. calendar, month_lengths etc.
        TimeAttributes=['calendar','month_lengths','leap_year','leap_month','climatology']
        if attribute in TimeAttributes:

            if var.attributes.has_key('units'):
                varUnits = udunits.ut_parse(self.unitSystem, var.attributes['units'], "UT_ASCII")
                secsSinceEpoch = udunits.ut_parse(self.unitSystem, "seconds since 1970-01-01", "UT_ASCII")
                if not udunits.ut_are_convertible(varUnits, secsSinceEpoch) :
                    print "ERROR (4.4.1): Attribute",attribute,"may only be attached to time coordinate variable"
                    self.err = self.err+1
                    rc=0

                # Free up resources associated with varUnits
                udunits.ut_free(varUnits)
                udunits.ut_free(secsSinceEpoch)
                
            else:        
                print "ERROR (4.4.1): Attribute",attribute,"may only be attached to time coordinate variable"
                self.err = self.err+1
                rc=0
        
    return rc


  #----------------------------------
  def chkCellMethods(self, varName):
  #----------------------------------
    """Checks on cell_methods attribute
    1) Correct syntax
    2) Valid methods
    3) Valid names
    4) No duplicate entries for dimension other than 'time'"""
    # dim1: [dim2: [dim3: ...]] method [ (comment) ]
    # where comment is of the form:  ([interval: value unit [interval: ...] comment:] remainder)
    rc=1
    error = 0  # Flag to indicate validity of cell_methods string syntax
    varDimensions={}
    var=self.f[varName]
    
    if var.attributes.has_key('cell_methods'):
        cellMethods=var.attributes['cell_methods']
        getComments=re.compile(r'\([^)]+\)')

        # Remove comments from the cell_methods string and split at these points
        noComments=getComments.sub('%5A',cellMethods)
        substrings=re.split('%5A',noComments)
        pr=re.compile(r'^\s*(\S+\s*:\s*(\S+\s*:\s*)*(point|sum|maximum|median|mid_range|minimum|mean|mode|standard_deviation|variance)(\s+(over|within)\s+(days|years))?\s*)+$')
        # Validate each substring
        for s in substrings:
            if s:
                if not pr.match(s):
                    strError=s
                    error=1
                    break

                # Validate dim and check that it only appears once unless it is 'time'
                allDims=re.findall(r'\S+\s*:',s)
                for part in allDims:
                    dims=re.split(':',part)
                    for d in dims:
                        if d:
                            if var.getAxisIndex(d) == -1 and not d in self.std_name_dh.dict.keys():
                                print "ERROR (7.3): Invalid 'name' in cell_methods attribute:",d
                                self.err = self.err+1
                                rc=0
                            elif varDimensions.has_key(d) and d != "time":
                                print "ERROR (7.3): Multiple cell_methods entries for dimension:",d
                                self.err = self.err+1
                            else:
                                varDimensions[d]=1

        # Validate the comment if it is standardized
        ### RSH TO DO:  Still need to implement validation of unit in the standardized comment.
        if not error:
            comments=getComments.findall(cellMethods)
            cpr=re.compile(r'^\((interval:\s+\d+\s+(years|months|days|hours|minutes|seconds)\s*)*(comment: .+)?\)')
            for c in comments:
                if re.search(r'^\(\s*interval',c):
                    # Only need to check standardized comments i.e. those beginning (interval ...)
                    if not cpr.match(c):
                        strError=c
                        error=1
                        break

        if error:
            print "ERROR (7.3): Invalid cell_methods syntax: '" + strError + "'"
            self.err = self.err + 1
            rc=0

    return rc


  #----------------------------------
  def isValidUdunitsUnit(self,unit):
  #----------------------------------
      # units must be recognizable by udunits package
      udunitsUnit = udunits.ut_parse(self.unitSystem, unit, "UT_ASCII")
      if udunitsUnit:
          # Valid unit
          rc=1
      else:
          # Invalid unit
          rc=0

      # Free up resources associated with udunitsUnit
      udunits.ut_free(udunitsUnit)
      
      return rc


  #---------------------------------------------------
  def isValidCellMethodTypeValue(self, type, value):
  #---------------------------------------------------
      """ Is <type1> or <type2> in the cell_methods attribute a valid value"""
      rc=1
      # Is it a string-valued aux coord var with standard_name of area_type?
      if value in self.auxCoordVars:
          if self.f[value].dtype.char != 'c':
              rc=0
          elif type == "type2":
              # <type2> has the additional requirement that it is not allowed a leading dimension of more than one
              leadingDim = self.f[value].getAxisIds()[0]
              # Must not be a value of more than one
              if self.f.dimensions[leadingDim] > 1:
                  print "ERROR (7.3):",value,"is not allowed a leading dimension of more than one."
                  self.err = self.err + 1

          if self.f[value].attributes.has_key('standard_name'):
              if self.f[value].attributes['standard_name'] != 'area_type':
                  rc=0
                  
      # Is type a valid area_type according to the area_type table
      elif value not in self.area_type_lh.list:
          rc=0

      return rc


  #----------------------------------
  def chkCellMethods_redefined(self,varName):
  #----------------------------------
    """Checks on cell_methods attribute
       dim1: [dim2: [dim3: ...]] method [where type1 [over type2]] [ (comment) ]
       where comment is of the form:  ([interval: value unit [interval: ...] comment:] remainder)
    """
    
    rc=1
    #error = 0  # Flag to indicate validity of cell_methods string syntax
    varDimensions={}
    var=self.f[varName]
    
    if var.attributes.has_key('cell_methods'):
        cellMethods=var.attributes['cell_methods']

#        cellMethods="lat: area: maximum (interval: 1 hours interval: 3 hours comment: fred)"

        pr1=re.compile(r'^'
                      r'(\s*\S+\s*:\s*(\S+\s*:\s*)*'
                      r'([a-z_]+)'
                      r'(\s+where\s+\S+(\s+over\s+\S+)?)?'
                      r'(\s+(over|within)\s+(days|years))?\s*'
                      r'(\((interval:\s+\d+\s+\S+\s*)*(comment: .+)?.*\))?)'
                      r'+$')
        
        # Validate the entire string
        m = pr1.match(cellMethods)
        if not m:
            print "ERROR (7.3) Invalid syntax for cell_methods attribute"
            self.err = self.err + 1
            rc=0

        # Grab each word-list - dim1: [dim2: [dim3: ...]] method [where type1 [over type2]] [within|over days|years] [(comment)]
        pr2=re.compile(r'(?P<dimensions>\s*\S+\s*:\s*(\S+\s*:\s*)*'
                      r'(?P<method>[a-z_]+)'
                      r'(?:\s+where\s+(?P<type1>\S+)(?:\s+over\s+(?P<type2>\S+))?)?'
                      r'(?:\s+(?:over|within)\s+(?:days|years))?\s*)'
                      r'(?P<comment>\([^)]+\))?')

        substr_iter=pr2.finditer(cellMethods)
        
        # Validate each substring
        for s in substr_iter:
            if not re.match(r'point|sum|maximum|median|mid_range|minimum|mean|mode|standard_deviation|variance',s.group('method')):
                print "ERROR (7.3): Invalid cell_method:",s.group('method')
                self.err = self.err + 1
                rc=0

            if self.version >= 1.4:
                if s.group('type1'):
                    if not self.isValidCellMethodTypeValue('type1', s.group('type1')):
                        print "ERROR (7.3): Invalid type1: '"+s.group('type1')+"' - must be a variable name or valid area_type"
                        self.err = self.err + 1

                if s.group('type2'):
                    if not self.isValidCellMethodTypeValue('type2', s.group('type2')):
                        print "ERROR (7.3): Invalid type2: '"+s.group('type2')+"' - must be a variable name or valid area_type"
                        self.err = self.err + 1
                                                      
            # Validate dim and check that it only appears once unless it is 'time'
            allDims=re.findall(r'\S+\s*:',s.group('dimensions'))
            dc=0          # Number of dims
            
            for part in allDims:
                dims=re.split(':',part)

                for d in dims:
                    if d:
                        dc=dc+1
                        if var.getAxisIndex(d) == -1 and not d in self.std_name_dh.dict.keys():
                            if self.version >= 1.4:
                                # Extra constraints at CF-1.4 and above
                                if d != "area":
                                    print "ERROR (7.3): Invalid 'name' in cell_methods attribute:",d
                                    self.err = self.err+1
                                    rc=0
                            else:
                                print "ERROR (7.3): Invalid 'name' in cell_methods attribute:",d
                                self.err = self.err+1
                                rc=0
                                
                        else:
                            # dim is a variable dimension
                            if varDimensions.has_key(d) and d != "time":
                                print "ERROR (7.3): Multiple cell_methods entries for dimension:",d
                                self.err = self.err+1
                                rc=0
                            else:
                                varDimensions[d]=1
                                
                            if self.version >= 1.4:
                                # If dim is a coordinate variable and cell_method is not 'point' check
                                # if the coordinate variable has either bounds or climatology attributes
                                if d in self.coordVars and s.group('method') != 'point':
                                    if not self.f[d].attributes.has_key('bounds') and not self.f[d].attributes.has_key('climatology'):
                                        print "WARNING (7.3): Coordinate variable",d,"should have bounds or climatology attribute"
                                        self.warn = self.warn + 1
                                                
            # Validate the comment associated with this method, if present
            comment = s.group('comment')
            if comment:
                getIntervals = re.compile(r'(?P<interval>interval:\s+\d+\s+(?P<unit>\S+)\s*)')
                allIntervals = getIntervals.finditer(comment)

                # There must be zero, one or exactly as many interval clauses as there are dims
                i=0   # Number of intervals present
                for m in allIntervals:
                    i=i+1
                    unit=m.group('unit')
                    if not self.isValidUdunitsUnit(unit):
                        print "ERROR (7.3): Invalid unit",unit,"in cell_methods comment"
                        self.err = self.err + 1
                        rc=0

                if i > 1 and i != dc:
                    print "ERROR (7.3): Incorrect number or interval clauses in cell_methods attribute"
                    self.err = self.err + 1
                    rc=0
                    
    return rc


  #----------------------------------
  def chkCellMeasures(self,varName):
  #----------------------------------
    """Checks on cell_measures attribute:
    1) Correct syntax
    2) Reference valid variable
    3) Valid measure"""
    rc=1
    var=self.f[varName]
    
    if var.attributes.has_key('cell_measures'):
        cellMeasures=var.attributes['cell_measures']
        if not re.search("^([a-zA-Z0-9]+: +([a-zA-Z0-9_ ]+:?)*( +[a-zA-Z0-9_]+)?)$",cellMeasures):
            print "ERROR (7.2): Invalid cell_measures syntax"
            self.err = self.err+1
            rc=0
        else:
            # Need to validate the measure + name
            split=string.split(cellMeasures)
            splitIter=iter(split)
            try:
                while 1:
                    measure=splitIter.next()
                    variable=splitIter.next()

                    if variable not in self.f.variables.keys():
                        print "WARNING (7.2): cell_measures referring to variable '"+variable+"' that doesn't exist in this netCDF file."
                        print "INFO (7.2): This is strictly an error if the cell_measures variable is not included in the dataset."
                        self.warn = self.warn+1
                        rc=0
                        
                    else:
                        # Valid variable name in cell_measures so carry on with tests.    
                        if len(self.f[variable].getAxisIds()) > len(var.getAxisIds()):
                            print "ERROR (7.2): Dimensions of",variable,"must be same or a subset of",var.getAxisIds()
                            self.err = self.err+1
                            rc=0
                        else:
                            # If cell_measures variable has more dims than var then this check automatically will fail
                            # Put in else so as not to duplicate ERROR messages.
                            for dim in self.f[variable].getAxisIds():
                                if dim not in var.getAxisIds():
                                    print "ERROR (7.2): Dimensions of",variable,"must be same or a subset of",var.getAxisIds()
                                    self.err = self.err+1
                                    rc=0
                    
                        measure=re.sub(':','',measure)
                        if not re.match("^(area|volume)$",measure):
                            print "ERROR (7.2): Invalid measure in attribute cell_measures"
                            self.err = self.err+1
                            rc=0

                        if measure == "area" and self.f[variable].units != "m2":
                            print "ERROR (7.2): Must have square meters for area measure"
                            self.err = self.err+1
                            rc=0

                        if measure == "volume" and self.f[variable].units != "m3":
                            print "ERROR (7.2): Must have cubic meters for volume measure"
                            self.err = self.err+1
                            rc=0
                        
            except StopIteration:
                pass
            
    return rc


  #----------------------------------
  def chkFormulaTerms(self,varName,allCoordVars):
  #----------------------------------
    """Checks on formula_terms attribute (CF Section 4.3.2):
    formula_terms = var: term var: term ...
    1) No standard_name present
    2) No formula defined for std_name
    3) Invalid formula_terms syntax
    4) Var referenced, not declared"""
    rc=1
    var=self.f[varName]
    
    if var.attributes.has_key('formula_terms'):

        if varName not in allCoordVars:
            print "ERROR (4.3.2): formula_terms attribute only allowed on coordinate variables"
            self.err = self.err+1
            
        # Get standard_name to determine which formula is to be used
        if not var.attributes.has_key('standard_name'):
            print "ERROR (4.3.2): Cannot get formula definition as no standard_name"
            self.err = self.err+1
            # No sense in carrying on as can't validate formula_terms without valid standard name
            return 0


        (stdName,modifier) = self.getStdName(var)
        
        if not self.alias.has_key(stdName):
            print "ERROR (4.3.2): No formula defined for standard name:",stdName
            self.err = self.err+1
            # No formula available so can't validate formula_terms
            return 0

        index=self.alias[stdName]

        formulaTerms=var.attributes['formula_terms']
        if not re.search("^([a-zA-Z0-9_]+: +[a-zA-Z0-9_]+( +)?)*$",formulaTerms):
            print "ERROR (4.3.2): Invalid formula_terms syntax"
            self.err = self.err+1
            rc=0
        else:
            # Need to validate the term & var
            split=string.split(formulaTerms)
            for x in split[:]:
                if not re.search("^[a-zA-Z0-9_]+:$", x):
                    # Variable - should be declared in netCDF file
                    if x not in self.f._file_.variables.keys():
                        print "ERROR (4.3.2):",x,"is not declared as a variable"
                        self.err = self.err+1
                        rc=0       
                else:
                    # Term - Should be present in formula
                    x=re.sub(':','',x)
                    found='false'
                    for formula in self.formulas[index]:
                        if re.search(x,formula):
                            found='true'
                            break

                    if found == 'false':
                        print "ERROR (4.3.2): term",x,"not present in formula"
                        self.err = self.err+1
                        rc=0

    return rc


  #----------------------------------------
  def chkUnits(self,varName,allCoordVars):
  #----------------------------------------
      """Check units attribute"""
      rc=1
      var=self.f[varName]

      if self.badc:
          rc = self.chkBADCUnits(var)
          # If unit is a BADC unit then no need to check via udunits
          if rc:
              return rc

      # Test for blank since coordinate variables have 'units' defined even if not specifically defined in the file
      if var.attributes.has_key('units') and var.attributes['units'] != '':
          # Type of units is a string
          units = var.attributes['units']
          if type(units) != types.StringType:
              print "ERROR (3.1): units attribute must be of type 'String'"
              self.err = self.err+1
              # units not a string so no point carrying out further tests
              return 0
            
          # units - level, layer and sigma_level are deprecated
          if units in ['level','layer','sigma_level']:
              print "WARNING (3.1): units",units,"is deprecated"
              self.warn = self.warn+1
          elif units == 'month':
              print "WARNING (4.4): The unit 'month', defined by udunits to be exactly year/12, should"
              print "         be used with caution."
              self.warn = self.warn+1
          elif units == 'year':
              print "WARNING (4.4): The unit 'year', defined by udunits to be exactly 365.242198781 days,"
              print "         should be used with caution. It is not a calendar year."
          else:
              
              # units must be recognizable by udunits package
              varUnit = udunits.ut_parse(self.unitSystem, units, "UT_ASCII")
              if not varUnit:
                  print "ERROR (3.1): Invalid units: ",units
                  self.err = self.err+1
                  # Invalid units so no point continuing with further unit checks
                  return 0
        
              # units of a variable that specifies a standard_name must
              # be consistent with units given in standard_name table
              if var.attributes.has_key('standard_name'):
                  (stdName,modifier) = self.getStdName(var)

                  # Is the Standard Name modifier number_of_observations being used.
                  if modifier == 'number_of_observations':
                      # Standard Name modifier is number_of_observations therefore units should be "1".  See Appendix C
                      if not units == "1":
                          print "ERROR (3.3): Standard Name modifier 'number_of_observations' present therefore units must be set to 1."
                          self.err = self.err + 1
                  
                  elif stdName in self.std_name_dh.dict.keys():
                      # Get canonical units from standard name table
                      stdNameUnits = self.std_name_dh.dict[stdName]

                      # stdNameUnits is unicode which udunits can't deal with.  Explicity convert it to ASCII
                      stdNameUnits=stdNameUnits.encode('ascii')

                      canonicalUnit = udunits.ut_parse(self.unitSystem, stdNameUnits, "UT_ASCII")

                      # To compare units we need to remove the reference time from the variable units
                      if re.search("since",units):
                          # unit attribute contains a reference time - remove it
                          udunits.ut_free(varUnit)
                          varUnit = udunits.ut_parse(self.unitSystem, units.split()[0], "UT_ASCII")

                      # If variable has cell_methods=variance we need to square standard_name table units
                      if var.attributes.has_key('cell_methods'):
                          # Remove comments from the cell_methods string - no need to search these
                          getComments=re.compile(r'\([^)]+\)')
                          noComments=getComments.sub('%5A',var.attributes['cell_methods'])

                          if re.search(r'(\s+|:)variance',noComments):
                              # Variance method so standard_name units need to be squared.
                              unit1 = udunits.ut_parse(self.unitSystem, stdNameUnits, "UT_ASCII")
                              canonicalUnit = udunits.ut_multiply(unit1,unit1)
                              udunits.ut_free(unit1)

                      if not udunits.ut_are_convertible(varUnit, canonicalUnit):
                          # Conversion unsuccessful
                          print "ERROR (3.1): Units are not consistent with those given in the standard_name table."
                          self.err = self.err+1
                          rc=0

                      # Free resources associated with canonicalUnit
                      udunits.ut_free(canonicalUnit)

              # Free resources associated with udunitsUnit
              udunits.ut_free(varUnit)
              
      else:

          # No units attribute - is this a coordinate variable or
          # dimensionless vertical coordinate var
          if var.id in allCoordVars:
              
              # Label variables do not require units attribute
              if self.f[var.id].typecode() != 'c':
                  if var.attributes.has_key('axis'):
                      if not var.axis == 'Z':
                          print "WARNING (3.1): units attribute should be present"
                          self.warn = self.warn+1
                  elif not hasattr(var,'positive') and not hasattr(var,'formula_terms') and not hasattr(var,'compress'):
                      print "WARNING (3.1): units attribute should be present"
                      self.warn = self.warn+1

          elif var.id not in self.boundsVars and var.id not in self.climatologyVars and var.id not in self.gridMappingVars:
              # Variable is not a boundary or climatology variable

              dimensions = self.f[var.id].getAxisIds()

              if not hasattr(var,'flag_values') and len(dimensions) != 0 and self.f[var.id].typecode() != 'c':
                  # Variable is not a flag variable or a scalar or a label
                  
                  print "INFO (3.1): No units attribute set.  Please consider adding a units attribute for completeness."
                  self.info = self.info+1 

      return rc


  #----------------------------
  def chkBADCUnits(self, var):
  #----------------------------
      """Check units allowed by BADC"""
      units_lines=open("/usr/local/cf-checker/lib/badc_units.txt").readlines()

      # badc_units test case
      #units_lines=open("/home/ros/SRCE_projects/CF_Checker_W/main/Test_Files/badc_units.txt").readlines()
      
      # units must be recognizable by the BADC units file
      for line in units_lines:
          if hasattr(var, 'units') and var.attributes['units'] in string.split(line):
              print "Valid units in BADC list:", var.attributes['units']
              rc=1
              break
          else:
              rc=0
              
      return rc
  

  #---------------------------------------
  def chkValidMinMaxRange(self, varName):
  #---------------------------------------
      """Check that valid_range and valid_min/valid_max are not both specified"""
      var=self.f[varName]
    
      if var.attributes.has_key('valid_range'):
          if var.attributes.has_key('valid_min') or \
             var.attributes.has_key('valid_max'):

              print "ERROR (2.5.1): Illegal use of valid_range and valid_min/valid_max"
              self.err = self.err+1
              return 0

      return 1


  #---------------------------------
  def chk_FillValue(self, varName):
  #---------------------------------
    """Check 1) type of _FillValue
    2) _FillValue lies outside of valid_range
    3) type of missing_value
    4) flag use of missing_value as deprecated"""
    rc=1
    var=self.f[varName]

##    varType=var.dtype.char

    if var.__dict__.has_key('_FillValue'):
        fillValue=var.__dict__['_FillValue']
        
## 05.02.08 No longer needed as this is now detected by chkAttribute as _FillValue
## has an attribute type of 'D'. See Trac #022
##         if varType == 'c' or varType == types.StringType:
##             if type(fillValue) != types.StringType:
##                 print "ERROR (2.5.1): _FillValue of different type to variable"
##                 self.err = self.err+1
##                 rc=0
##         elif varType != fillValue.dtype.char:
##             print "ERROR (2.5.1): _FillValue of different type to variable"
##             self.err = self.err+1
##             rc=0
            
        if var.attributes.has_key('valid_range'):
            # Check _FillValue is outside valid_range
            validRange=var.attributes['valid_range']
            if fillValue > validRange[0] and fillValue < validRange[1]:
                print "WARNING (2.5.1): _FillValue should be outside valid_range"
                self.warn = self.warn+1

        if var.id in self.boundsVars:
            print "WARNING (7.1): Boundary Variable",var.id,"should not have _FillValue attribute"
            self.warn = self.warn+1
        elif var.id in self.climatologyVars:
            print "ERROR (7.4): Climatology Variable",var.id,"must not have _FillValue attribute"
            self.err = self.err+1
            rc=0

    if var.attributes.has_key('missing_value'):
        missingValue=var.attributes['missing_value']

        if missingValue:
            if var.__dict__.has_key('_FillValue'):
                if fillValue != missingValue:
                    # Special case: NaN == NaN is not detected as NaN does not compare equal to anything else
                    if not (numpy.isnan(fillValue) and numpy.isnan(missingValue)):
                        print "WARNING (2.5.1): missing_value and _FillValue set to differing values"

                        self.warn = self.warn+1

## 08.12.10 missing_value is no longer deprecated by the NUG
##            else:
##                # _FillValue not present
##                print "WARNING (2.5.1): Use of 'missing_value' attribute is deprecated"
##                self.warn = self.warn+1
                
## 05.02.08 No longer needed as this is now detected by chkAttribute as missing_value
## has an attribute type of 'D'. See Trac #022
##             typeError = 0
##             if varType == 'c':
##                 if type(missingValue) != types.StringType:
##                     typeError = 1
##             elif varType != missingValue.dtype.char:
##                 typeError = 1
##
##             if typeError:
##                 print "ERROR (2.5.1): missing_value of different type to variable"
##                 self.err = self.err+1
##                 rc=0

            if var.id in self.boundsVars:
                print "WARNING (7.1): Boundary Variable",var.id,"should not have missing_value attribute"
                self.warn = self.warn+1
            elif var.id in self.climatologyVars:
                print "ERROR (7.4): Climatology Variable",var.id,"must not have missing_value attribute"
                self.err = self.err+1
                rc=0
    return rc
        

  #------------------------------------
  def chkAxisAttribute(self, varName):
  #------------------------------------
      """Check validity of axis attribute"""
      var=self.f[varName]
      
      if var.attributes.has_key('axis'):
          if not re.match('^(X|Y|Z|T)$',var.attributes['axis'],re.I):
              print "ERROR (4): Invalid value for axis attribute"
              self.err = self.err+1
              return 0

          if self.version >= 1.1 and varName in self.auxCoordVars:
              print "ERROR (4): Axis attribute is not allowed for auxillary coordinate variables."
              self.err = self.err+1
              return 0
          
          # Check that axis attribute is consistent with the coordinate type
          # deduced from units and positive.
          if hasattr(var,'positive'): 
              interp=self.getInterpretation(var.units,var.positive)
          else:
              interp=self.getInterpretation(var.units)

#          print "interp:",interp
#          print "axis:",var.axis

          if interp != None:
              # It was possible to deduce axis interpretation from units/positive
              if interp != var.axis:
                  print "ERROR (4): axis attribute inconsistent with coordinate type as deduced from units and/or positive"
                  self.err = self.err+1
                  return 0
            
      return 1


  #----------------------------------------
  def chkPositiveAttribute(self, varName):
  #----------------------------------------
      var=self.f[varName]
      if var.attributes.has_key('positive'):
          if not re.match('^(down|up)$',var.attributes['positive'],re.I):
              print "ERROR (4.3): Invalid value for positive attribute"
              self.err = self.err+1
              return 0

      return 1


  #-----------------------------------------
  def chkTimeVariableAttributes(self, varName):
  #-----------------------------------------
    rc=1
    var=self.f[varName]
    
    if var.attributes.has_key('calendar'):
        if not re.match('(gregorian|standard|proleptic_gregorian|noleap|365_day|all_leap|366_day|360_day|julian|none)',
                        var.attributes['calendar'],re.I):
            # Non-standardized calendar so month_lengths should be present
            if not var.attributes.has_key('month_lengths'):
                print "ERROR (4.4.1): Non-standard calendar, so month_lengths attribute must be present"
                self.err = self.err+1
                rc=0
        else:   
            if var.attributes.has_key('month_lengths') or \
               var.attributes.has_key('leap_year') or \
               var.attributes.has_key('leap_month'):
                print "ERROR (4.4.1): The attributes 'month_lengths', 'leap_year' and 'leap_month' must not appear when 'calendar' is present."
                self.err = self.err+1
                rc=0

    if not var.attributes.has_key('calendar') and not var.attributes.has_key('month_lengths'):
        print "WARNING (4.4.1): Use of the calendar and/or month_lengths attributes is recommended for time coordinate variables"
        self.warn = self.warn+1
        rc=0
        
    if var.attributes.has_key('month_lengths'):
        if len(var.attributes['month_lengths']) != 12 and \
           var.attributes['month_lengths'].dtype.char != 'i':
            print "ERROR (4.4.1): Attribute 'month_lengths' should be an integer array of size 12"
            self.err = self.err+1
            rc=0

    if var.attributes.has_key('leap_year'):
        if var.attributes['leap_year'].dtype.char != 'i' and \
           len(var.attributes['leap_year']) != 1:
            print "ERROR (4.4.1): leap_year should be a scalar value"
            self.err = self.err+1
            rc=0

    if var.attributes.has_key('leap_month'):
        if not re.match("^(1|2|3|4|5|6|7|8|9|10|11|12)$",
                        str(var.attributes['leap_month'][0])):
            print "ERROR (4.4.1): leap_month should be between 1 and 12"
            self.err = self.err+1
            rc=0

        if not var.attributes.has_key('leap_year'):
            print "WARNING (4.4.1): leap_month is ignored as leap_year NOT specified"
            self.warn = self.warn+1

    # Time units must contain a reference time
    # To do this; test if the "unit" in question is convertible with a known timestamp "unit".
    varUnits=udunits.ut_parse(self.unitSystem, var.units, "UT_ASCII")
    secsSinceEpoch=udunits.ut_parse(self.unitSystem, "seconds since 1970-01-01", "UT_ASCII")
    
    if not udunits.ut_are_convertible(secsSinceEpoch, varUnits):
        print "ERROR (4.4): Invalid units and/or reference time"
        self.err = self.err+1

    # Free resources used by varUnits and secsSinceEpoch
    udunits.ut_free(varUnits)
    udunits.ut_free(secsSinceEpoch)
        
    return rc

    
  #----------------------------------
  def chkDescription(self, varName):
  #----------------------------------
      """Check 1) standard_name & long_name attributes are present
               2) for a valid standard_name as listed in the standard name table."""
      rc=1
      var=self.f[varName]

      if not var.attributes.has_key('standard_name') and \
         not var.attributes.has_key('long_name'):

          exceptions=self.boundsVars+self.climatologyVars+self.gridMappingVars
          if var.id not in exceptions:
              print "WARNING (3): No standard_name or long_name attribute specified"
              self.warn = self.warn+1
              
      if var.attributes.has_key('standard_name'):
          # Check if valid by the standard_name table and allowed modifiers
          std_name=var.attributes['standard_name']

          # standard_name attribute can comprise a standard_name only or a standard_name
          # followed by a modifier (E.g. atmosphere_cloud_liquid_water_content status_flag)
          std_name_el=string.split(std_name)
          if not std_name_el:
              print "ERROR (3.3): Empty string for 'standard_name' attribute"
              self.err = self.err + 1
              rc=0
              
          elif not self.parseBlankSeparatedList(std_name) or len(std_name_el) > 2:
              print "ERROR (3.3): Invalid syntax for 'standard_name' attribute: '"+std_name+"'"
              self.err = self.err + 1
              rc=0

          else:
              # Validate standard_name
              name=std_name_el[0]
              if not name in self.std_name_dh.dict.keys():
                  if chkDerivedName(name):
                      print "ERROR (3.3): Invalid standard_name:",name
                      self.err = self.err + 1
                      rc=0

              if len(std_name_el) == 2:
                  # Validate modifier
                  modifier=std_name_el[1]
                  if not modifier in ['detection_minimum','number_of_observations','standard_error','status_flag']:
                      print "ERROR (3.3): Invalid standard_name modifier: "+modifier
                      rc=0
                      
      return rc


  #-----------------------------------
  def chkCompressAttr(self, varName):
  #-----------------------------------
    rc=1
    var=self.f[varName]
    if var.attributes.has_key('compress'):
        compress=var.attributes['compress']

        if var.typecode() != 'i':
            print "ERROR (8.2):",var.id,"- compress attribute can only be attached to variable of type int."
            self.err = self.err+1
            return 0
        if not re.search("^[a-zA-Z0-9_ ]*$",compress):
            print "ERROR (8.2): Invalid syntax for 'compress' attribute"
            self.err = self.err+1
            rc=0
        else:
            dimensions=string.split(compress)
            dimProduct=1
            for x in dimensions:
                found='false'
                if x in self.f.axes.keys():
                    # Get product of compressed dimension sizes for use later
                    #dimProduct=dimProduct*self.f.dimensions[x]
                    dimProduct=dimProduct*len(self.f.axes[x])
                    found='true'

                if found != 'true':
                    print "ERROR (8.2): compress attribute naming non-existent dimension: ",x
                    self.err = self.err+1
                    rc=0

            values=var.getValue()
            outOfRange=0
            for val in values[:]:
                if val < 0 or val > dimProduct-1:
                    outOfRange=1
                    break;
                
            if outOfRange:
                print "ERROR (8.2): values of",var.id,"must be in the range 0 to",dimProduct-1
                self.err = self.err+1
    return rc

  #---------------------------------
  def chkPackedData(self, varName):
  #---------------------------------
    rc=1
    var=self.f[varName]
    if var.attributes.has_key('scale_factor') and var.attributes.has_key('add_offset'):
        if var.attributes['scale_factor'].dtype.char != var.attributes['add_offset'].dtype.char:
            print "ERROR (8.1): scale_factor and add_offset must be the same numeric data type"
            self.err = self.err+1
            # No point running rest of packed data tests
            return 0

    if var.attributes.has_key('scale_factor'):
        type=var.attributes['scale_factor'].dtype.char
    elif var.attributes.has_key('add_offset'):
        type=var.attributes['add_offset'].dtype.char
    else:
        # No packed Data attributes present
        return 1

    # One or other attributes present; run remaining checks
    if var.dtype.char != type:
        if type != 'f' and type != 'd':
            print "ERROR (8.1): scale_factor and add_offset must be of type float or double"
            self.err = self.err+1
            rc=0

        if var.dtype.char != 'b' and  var.dtype.char != 'h' and var.dtype.char != 'i':
            print "ERROR (8.1):",var.id,"must be of type byte, short or int"
            self.err = self.err+1
            rc=0

        if type == 'f' and var.dtype.char == 'i':
            print "WARNING (8.1): scale_factor/add_offset are type float, therefore",var.id,"should not be of type int"
            self.warn = self.warn+1
            
    return rc

  #----------------------------
  def chkFlags(self, varName):
  #----------------------------
      var=self.f[varName]
      rc=1
      
      if var.attributes.has_key('flag_meanings'):
          # Flag to indicate whether one of flag_values or flag_masks present
          values_or_masks=0
          meanings = var.attributes['flag_meanings']

#          if not self.parseBlankSeparatedList(meanings):
          if not self.extendedBlankSeparatedList(meanings):
                print "ERROR (3.5): Invalid syntax for 'flag_meanings' attribute"
                self.err = self.err+1
                rc=0
          
          if var.attributes.has_key('flag_values'):
              values_or_masks=1
              values = var.attributes['flag_values']
              
              # If values is a string of chars, split it up into a list of chars
              if type(values) == str:
                  values = values.split()
                  
              retcode = self.equalNumOfValues(values,meanings)
              if retcode == -1:
                  print "ERROR (3.5): Problem in subroutine equalNumOfValues"
                  rc = 0
              elif not retcode:
                  print "ERROR (3.5): Number of flag_values values must equal the number or words/phrases in flag_meanings"
                  self.err = self.err + 1
                  rc = 0
                  
              # flag_values values must be mutually exclusive
              if not self.uniqueList(values):
                  print "ERROR (3.5): flag_values attribute must contain a list of unique values"
                  self.err = self.err + 1
                  rc = 0
                  
          if var.attributes.has_key('flag_masks'):
              values_or_masks=1
              masks = var.attributes['flag_masks']

              retcode = self.equalNumOfValues(masks,meanings)
              if retcode == -1:
                  print "ERROR (3.5): Problem in subroutine equalNumOfValues"
                  rc = 0
              elif not retcode:
                  print "ERROR (3.5): Number of flag_masks values must equal the number or words/phrases in flag_meanings"
                  self.err = self.err + 1
                  rc = 0
                  
              # flag_values values must be non-zero
              for v in masks:
                  if v == 0:
                      print "ERROR (3.5): flag_masks values must be non-zero"
                      self.err = self.err + 1
                      rc = 0
                      
          # Doesn't make sense to do bitwise comparison for char variable
          if var.dtype.char != 'c':
              if var.attributes.has_key('flag_values') and var.attributes.has_key('flag_masks'):
                  # Both flag_values and flag_masks present
                  # Do a bitwise AND of each flag_value and its corresponding flag_mask value,
                  # the result must be equal to the flag_values entry
                  i=0
                  for v in values:
                      bitwise_AND = v & masks[i]

                      if bitwise_AND != v:
                          print "WARNING (3.5): Bitwise AND of flag_value",v,"and corresponding flag_mask",masks[i],"doesn't match flag_value."
                          self.warn = self.warn + 1
                      i=i+1
                 
          if values_or_masks == 0:
              # flag_meanings attribute present, but no flag_values or flag_masks
              print "ERROR (3.5): flag_meanings present, but no flag_values or flag_masks specified"
              self.err = self.err + 1
              rc = 0

          if var.attributes.has_key('flag_values') and not var.attributes.has_key('flag_meanings'):
              print "ERROR (3.5): flag_meanings attribute is missing"
              self.err = self.err + 1
              rc = 0
              
      return rc


  #-----------------------
  def getType(self, arg):
  #-----------------------
      if type(arg) == type(Numeric.array([])):
          return "array"

      elif type(arg) == str:
          return "str"

      elif type(arg) == list:
          return "list"

      else:
          print "<cfchecker> ERROR: Unknown Type in getType("+arg+")"
          return 0
  
  
  
  #----------------------------------------    
  def equalNumOfValues(self, arg1, arg2):
  #----------------------------------------
      """ Check that arg1 and arg2 contain the same number of blank-separated elements."""

      # Determine the type of both arguments.  strings and arrays need to be handled differently
      type_arg1 = self.getType(arg1)
      type_arg2 = self.getType(arg2)
      
      if not type_arg1 or not type_arg2:
          return -1
          
      if type_arg1 == "str":
          len_arg1 = len(arg1.split())
      else:
          len_arg1 = len(arg1)

      if type_arg2 == "str":
          len_arg2 = len(arg2.split())
      else:
          len_arg2 = len(arg2)
      
      
      if len_arg1 != len_arg2:
          return 0

      return 1

      
  #------------------------------------------
  def chkMultiDimCoord(self, varName, axes):
  #------------------------------------------
      """If a coordinate variable is multi-dimensional, then it is recommended
      that the variable name should not match the name of any of its dimensions."""
      var=self.f[varName]
    
      # This is a temporary work around to obtain the dimensions of the coord
      # var.  In CDMS vn4.0 only 1D coord vars will be axis variables; There
      # will be no need to use _obj_. See CRM #011
      if var.id in axes and len(var._obj_.dimensions) > 1:
          # Multi-dimensional coordinate var
          if var.id in var._obj_.dimensions:
              print "WARNING (5): The name of a multi-dimensional coordinate variable"
              print "             should not match the name of any of its dimensions."
              self.warn = self.warn + 1


  #--------------------------------------
  def chkValuesMonotonic(self, varName):
  #--------------------------------------
    """A coordinate variable must have values that are strictly monotonic
    (increasing or decreasing)."""
    #rc=1
    var=self.f[varName]
    values=var.getValue()
    i=0
    for val in values[:]:
        if i == 0:
            # First value - no comparison to do
            i=i+1
            lastVal=val
            continue
        elif i == 1:
            i=i+1
            if val < lastVal:
                # Decreasing sequence
                type='decr'
            elif val > lastVal:
                # Increasing sequence
                type='incr'
            else:
                # Same value - ERROR
                print "ERROR (5): co-ordinate variable '" + var.id + "' not monotonic"
                self.err = self.err+1
                return 1

            lastVal=val
        else:
            i=i+1
            if val < lastVal and type != 'decr':
                # ERROR - should be increasing value
                print "ERROR (5): co-ordinate variable '" + var.id + "' not monotonic"
                self.err = self.err+1
                return 1
            elif val > lastVal and type != 'incr':
                # ERROR - should be decreasing value
                print "ERROR (5): co-ordinate variable '" + var.id + "' not monotonic"
                self.err = self.err+1
                return 1

            lastVal=val


def getargs(arglist):
    
    '''getargs(arglist): parse command line options and environment variables'''

    from getopt import getopt, GetoptError
    from os import environ
    from sys import stderr, exit

    udunitskey='UDUNITS'
    standardnamekey='CF_STANDARD_NAMES'
    areatypeskey='CF_AREA_TYPES'
    # set defaults
    udunits=''
    standardname=STANDARDNAME
    areatypes=AREATYPES
    uploader=None
    useFileName="yes"
    badc=None
    coards=None
    version=Versions[-1]
    
    # set to environment variables
    if environ.has_key(udunitskey):
        udunits=environ[udunitskey]
    if environ.has_key(standardnamekey):
        standardname=environ[standardnamekey]
    if environ.has_key(areatypeskey):
        areatypes=environ[areatypeskey]

    try:
        (opts,args)=getopt(arglist[1:],'a:bchlnu:s:v:',['area_types=','badc','coards','help','uploader','noname','udunits=','cf_standard_names=','version='])
    except GetoptError:
        stderr.write('%s\n'%__doc__)
        exit(1)
    
    for a, v in opts:
        if a in ('-a','--area_types'):
            areatypes=v
            continue
        if a in ('-b','--badc'):
            badc="yes"
            continue
        if a in ('-c','--coards'):
            coards="yes"
            continue
        if a in ('-h','--help'):
            print __doc__
            exit(0)
        if a in ('-l','--uploader'):
            uploader="yes"
            continue
        if a in ('-n','--noname'):
            useFileName="no"
            continue
        if a in ('-u','--udunits'):
            udunits=v
            continue
        if a in ('-s','--cf_standard_names'):
            standardname=v
            continue
        if a in ('-v','--version'):
            if v == 'auto':
                version = 0.0
            else:
                version=float(v)
                if "CF-"+v not in CFVersions:
                    print "WARNING: CF-"+v+" is not a valid CF version."
                    print "Performing check against newest version",CFVersions[-1]
                    version=Versions[-1]
            continue
                    
    if len(args) == 0:
        stderr.write('ERROR in command line\n\nusage:\n%s\n'%__doc__)
        exit(1)

    return (badc,coards,uploader,useFileName,standardname.strip(),areatypes.strip(),udunits.strip(),version,args)


#--------------------------
# Main Program
#--------------------------

if __name__ == '__main__':

    from sys import argv,exit

    (badc,coards,uploader,useFileName,standardName,areaTypes,udunitsDat,version,files)=getargs(argv)
    
    inst = CFChecker(uploader=uploader, useFileName=useFileName, badc=badc, coards=coards, cfStandardNamesXML=standardName, cfAreaTypesXML=areaTypes, udunitsDat=udunitsDat, version=version)
    for file in files:
        rc = inst.checker(file)
        exit (rc)


