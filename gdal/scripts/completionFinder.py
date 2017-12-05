#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Utilities
# Purpose:  Bash completion script generation
# Author:   Guillaume Pasero <guillaume dot pasero at c dash s dot fr>
#
###############################################################################
# Copyright (c) 2016, Guillaume Pasero <guillaume dot pasero at c dash s dot fr>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import sys, os
import os.path as op

from subprocess import call, Popen, PIPE, STDOUT

def showHelp():
  print "Usage : completionFinder.py  output_script"


def processLine(line):
  outList = []
  lvl1 = line.split(' ')
  lvl2 = []
  for item in lvl1:
    lvl2 += item.split('[')
  lvl3 = []
  for item in lvl2:
    lvl3 += item.split(']')
  lvl4 = []
  for item in lvl3:
    lvl4 += item.split('|')

  for item in lvl4:
    if len(item) >= 2:
      if item[0] == '-':
        key = item
        # Handle special cases -key={A/B/C}
        if (key.count("={") == 1 and key[-1] == '}'):
          pair = key.split("=")
          choices = ((pair[1])[1:-1]).split('/')
          for c in choices:
            outList.append(pair[0]+"="+c)
        else:
          outList.append(key)

  return outList

def processTool(toolName):
  command = [toolName, "-"]

  process = Popen(command,stdout=PIPE,stderr=STDOUT)
  result = process.communicate()[0]
  lines = result.split('\n')
  index = 0

  while index < len(lines):
    if (lines[index].find("Usage:") >= 0):
      break
    index += 1

  options = []

  while index < len(lines):
    cleanLine = lines[index].strip('\t\r ')
    if (len(cleanLine) == 0):
      break;
    options += processLine(cleanLine)
    index += 1

  return options

def parseGDALGeneralOptions():
  command = ["gdalinfo", "--help-general"]
  process = Popen(command,stdout=PIPE,stderr=STDOUT)
  result = process.communicate()[0]
  lines = result.split('\n')
  index = 0
  options = []
  while index < len(lines):
    cleanLine = lines[index].strip('\t\r ')
    parts = cleanLine.split(':')
    if (parts[0].find('--') == 0):
      words = parts[0].split(' ')
      options.append(words[0])
    index += 1
  return options

def parseOGRGeneralOptions():
  command = ["ogr2ogr", "--help-general"]
  process = Popen(command,stdout=PIPE,stderr=STDOUT)
  result = process.communicate()[0]
  lines = result.split('\n')
  index = 0
  options = []
  while index < len(lines):
    cleanLine = lines[index].strip('\t\r ')
    parts = cleanLine.split(':')
    if (parts[0].find('--') == 0):
      words = parts[0].split(' ')
      options.append(words[0])
    index += 1
  return options

# generate completion script section for :
#   - name : the given gdal/ogr tool
#   - optList : option list
def getCompletionScript(name,optList):
  output = []
  output.append("_"+name+"()\n")
  output.append("{\n")
  output.append("  local cur prev\n")
  output.append("  COMPREPLY=()\n")
  output.append("  _get_comp_words_by_ref cur prev\n")
  output.append("  case \"$cur\" in\n")
  output.append("    -*)\n")
  optLine = "      key_list=\""
  for item in optList:
    optLine += item + " "
  optLine += "\"\n"
  output.append(optLine)
  output.append("      COMPREPLY=( $( compgen -W '$key_list' -- $cur) )\n")
  output.append("      return 0\n")
  output.append("      ;;\n")
  output.append("  esac\n")

  # adapt format parsing command : GDAL type of OGR type

  isGdal = True
  if (name.find("ogr") == 0):
    isGdal = False

  # gdal type
  if isGdal:
    formatParsingCmd = "$tool --formats | tail -n +2 | cut -f 3 -d ' '"
    if ("-ot" in optList) or ("-of" in optList) or ("--format" in optList):
      output.append("  tool=${COMP_WORDS[0]}\n")
      output.append("  case \"$prev\" in\n")
      if "-ot" in optList:
        output.append("    -ot)\n")
        output.append("      key_list=\"Byte Int16 UInt16 UInt32 Int32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64\"\n")
        output.append("      COMPREPLY=( $( compgen -W '$key_list' -- $cur) )\n")
        output.append("      ;;\n")
      for arg in ("-f", "-of"):
        if (arg in optList) and isGdal:
            output.append("    %s)\n" % arg)
            output.append("      key_list=\"$( "+formatParsingCmd+")\"\n")
            output.append("      COMPREPLY=( $( compgen -W '$key_list' -- $cur) )\n")
            output.append("      ;;\n")
      if ("--format" in optList) and isGdal:
        output.append("    --format)\n")
        output.append("      key_list=\"$( "+formatParsingCmd+")\"\n")
        output.append("      COMPREPLY=( $( compgen -W '$key_list' -- $cur) )\n")
        output.append("      ;;\n")
      output.append("  esac\n")
  else:
    # ogr type
    formatParsingCmd = "$tool --formats | tail -n +2 | grep -o -E '\"[^\"]+\"' | sed 's/\ /__/'"
    if ("-f" in optList):
      # replace ogrtindex by ogr2ogr to check --formats
      output.append("  tool=${COMP_WORDS[0]/ogrtindex/ogr2ogr}\n")
      output.append("  case \"$prev\" in\n")
      for arg in ("-f", "-of"):
        if (arg in optList) and not isGdal:
            # completion is more tricky here because of spaces
            output.append("    %s)\n" % arg)
            output.append("      key_list=\"$( "+formatParsingCmd+")\"\n")
            output.append("      for iter in $key_list; do\n")
            output.append("        if [[ $iter =~ ^$cur ]]; then\n")
            output.append("          COMPREPLY+=( \"${iter//__/ }\" )\n")
            output.append("        fi\n")
            output.append("      done\n")
            output.append("      ;;\n")
      output.append("  esac\n")

  output.append("  return 0\n")
  output.append("}\n")
  output.append("complete -o default -F _"+name+" "+name+"\n")

  return output

def main(argv):
  if len(argv) < 2 :
    showHelp()
    return 1

  gdaltools = [ "gdal2tiles.py",\
                "gdal2xyz.py",\
                "gdaladdo",\
#                "gdal_auth.py",\
                "gdalbuildvrt",\
                "gdal_calc.py",\
                "gdalchksum.py",\
                "gdalcompare.py",\
                "gdal-config",\
                "gdal_contour",\
                "gdaldem",\
                "gdal_edit.py",\
                "gdalenhance",\
                "gdal_fillnodata.py",\
                "gdal_grid",\
                "gdalident.py",\
                "gdalimport.py",\
                "gdalinfo",\
                "gdallocationinfo",\
                "gdalmanage",\
                "gdal_merge.py",\
                "gdalmove.py",\
                "gdal_polygonize.py",\
                "gdal_proximity.py",\
                "gdal_rasterize",\
                "gdal_retile.py",\
                "gdalserver",\
                "gdal_sieve.py",\
                "gdalsrsinfo",\
                "gdaltindex",\
                "gdaltransform",\
                "gdal_translate",\
                "gdalwarp"]

  ogrtools = [ "ogr2ogr",\
               "ogrinfo",\
               "ogrlineref",\
               "ogrtindex",
               "ogrmerge.py"]

  # parse general options
  generalOptions = parseGDALGeneralOptions()
  generalOGROptions = parseOGRGeneralOptions()

  outFile = argv[1]
  of = open(outFile,'w')
  of.write("# File auto-generated by completionFinder.py, do not modify manually\n")
  of.write("""
function_exists() {
    declare -f -F $1 > /dev/null
    return $?
}

# Checks that bash-completion is recent enough
function_exists _get_comp_words_by_ref || return 0

""")

  for name in gdaltools:
    # verbose print
    print name
    optList = processTool(name)
    if "--help-general" in optList:
      for item in generalOptions:
        if not item in optList:
          optList.append(item)
    of.writelines(getCompletionScript(name,optList))
  for name in ogrtools:
    # verbose print
    print name
    optList = processTool(name)
    if "--help-general" in optList:
      for item in generalOGROptions:
        if not item in optList:
          optList.append(item)
    of.writelines(getCompletionScript(name,optList))

  of.close()
  return 0


if __name__ == "__main__":
  main(sys.argv)

