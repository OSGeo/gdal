#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
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

import sys
from subprocess import PIPE, STDOUT, Popen


def showHelp():
    print("Usage : completionFinder.py  output_script")


def processLine(outList, line):

    in_option = False
    item = ""
    last_c = ""
    for c in line:
        if last_c in ("[", "|") and c == "-" and not in_option:
            in_option = True
            item = c
        elif in_option:
            if c not in ("]", "[", " ", "|"):
                item += c
            else:
                in_option = False
                key = item
                item = ""
                # Handle special cases -key={A/B/C}
                if key.count("={") == 1 and key[-1] == "}":
                    pair = key.split("=")
                    choices = ((pair[1])[1:-1]).split("/")
                    for c in choices:
                        outList.append(pair[0] + "=" + c)
                else:
                    pos = key.find("=")
                    if pos > 0:
                        key = key[0:pos]
                    if key != "-" and key not in outList:
                        outList.append(key)
        last_c = c

    return outList


def processTool(toolName):
    if toolName == "gdal_calc.py":
        command = [toolName]
    else:
        command = [toolName, "--help"]

    process = Popen(command, stdout=PIPE, stderr=STDOUT)
    result = process.communicate()[0]
    lines = result.decode("utf-8").split("\n")
    options = []

    for line in lines:
        cleanLine = line.strip("\t\r ")
        options = processLine(options, cleanLine)

    return options


def parseGDALGeneralOptions():
    command = ["gdalinfo", "--help-general"]
    process = Popen(command, stdout=PIPE, stderr=STDOUT)
    result = process.communicate()[0]
    lines = result.decode("utf-8").split("\n")
    index = 0
    options = []
    while index < len(lines):
        cleanLine = lines[index].strip("\t\r ")
        parts = cleanLine.split(":")
        if parts[0].find("--") == 0:
            words = parts[0].split(" ")
            options.append(words[0])
        index += 1
    return options


def parseOGRGeneralOptions():
    command = ["ogr2ogr", "--help-general"]
    process = Popen(command, stdout=PIPE, stderr=STDOUT)
    result = process.communicate()[0]
    lines = result.decode("utf-8").split("\n")
    index = 0
    options = []
    while index < len(lines):
        cleanLine = lines[index].strip("\t\r ")
        parts = cleanLine.split(":")
        if parts[0].find("--") == 0:
            words = parts[0].split(" ")
            options.append(words[0])
        index += 1
    return options


# generate completion script section for :
#   - name : the given gdal/ogr tool
#   - optList : option list


def getCompletionScript(name, optList):
    output = []

    if name.endswith(".py"):
        func_name = name[0:-3]
    else:
        func_name = name

    output.append("_" + func_name + "()\n")
    output.append("{\n")
    output.append("  COMPREPLY=()\n")
    output.append('  cur="${COMP_WORDS[$COMP_CWORD]}"\n')
    output.append('  case "$cur" in\n')
    output.append("    -*)\n")
    optLine = '      key_list="'
    for item in optList:
        optLine += item + " "
    optLine += '"\n'
    output.append(optLine)
    output.append('      if [ "$CURRENT_SHELL" = "bash" ]; then\n')
    output.append(
        '        mapfile -t COMPREPLY < <(compgen -W "$key_list" -- "$cur")\n'
    )
    output.append("      else\n")
    output.append('        COMPREPLY=( "$(compgen -W "$key_list" -- "$cur")" )\n')
    output.append("      fi\n")
    output.append("      return 0\n")
    output.append("      ;;\n")
    output.append("  esac\n")

    # adapt format parsing command : GDAL type of OGR type

    isGdal = True
    if name.startswith("ogr"):
        isGdal = False

    # gdal type
    if isGdal:
        formatParsingCmd = "$tool --formats | tail -n +2 | cut -f 3 -d ' '"
        if ("-ot" in optList) or ("-of" in optList) or ("--format" in optList):
            output.append("  tool=${COMP_WORDS[0]}\n")
            output.append('  prev="${COMP_WORDS[$COMP_CWORD-1]}"\n')
            output.append('  case "$prev" in\n')
            if "-ot" in optList:
                output.append("    -ot)\n")
                output.append(
                    '      key_list="Byte Int16 UInt16 UInt32 Int32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64"\n'
                )
                output.append('      if [ "$CURRENT_SHELL" = "bash" ]; then\n')
                output.append(
                    '        mapfile -t COMPREPLY < <(compgen -W "$key_list" -- "$cur")\n'
                )
                output.append("      else\n")
                output.append(
                    '        COMPREPLY=( "$(compgen -W "$key_list" -- "$cur")" )\n'
                )
                output.append("      fi\n")
                output.append("      ;;\n")
            for arg in ("-f", "-of"):
                if (arg in optList) and isGdal:
                    output.append("    %s)\n" % arg)
                    output.append('      key_list="$( ' + formatParsingCmd + ')"\n')
                    output.append('      if [ "$CURRENT_SHELL" = "bash" ]; then\n')
                    output.append(
                        '        mapfile -t COMPREPLY < <(compgen -W "$key_list" -- "$cur")\n'
                    )
                    output.append("      else\n")
                    output.append(
                        '        COMPREPLY=( "$(compgen -W "$key_list" -- "$cur")" )\n'
                    )
                    output.append("      fi\n")
                    output.append("      ;;\n")
            if ("--format" in optList) and isGdal:
                output.append("    --format)\n")
                output.append('      key_list="$( ' + formatParsingCmd + ')"\n')
                output.append('      if [ "$CURRENT_SHELL" = "bash" ]; then\n')
                output.append(
                    '        mapfile -t COMPREPLY < <(compgen -W "$key_list" -- "$cur")\n'
                )
                output.append("      else\n")
                output.append(
                    '        COMPREPLY=( "$(compgen -W "$key_list" -- "$cur")" )\n'
                )
                output.append("      fi\n")
                output.append("      ;;\n")
            output.append("  esac\n")
    else:
        # ogr type
        formatParsingCmd = (
            "$tool --formats | tail -n +2 | grep -o -E '\"[^\"]+\"' | sed 's/\\ /__/'"
        )
        if "-f" in optList:
            # replace ogrtindex by ogr2ogr to check --formats
            output.append("  tool=${COMP_WORDS[0]/ogrtindex/ogr2ogr}\n")
            output.append('  prev="${COMP_WORDS[$COMP_CWORD-1]}"\n')
            output.append('  case "$prev" in\n')
            for arg in ("-f", "-of"):
                if (arg in optList) and not isGdal:
                    # completion is more tricky here because of spaces
                    output.append("    %s)\n" % arg)
                    output.append('      key_list="$( ' + formatParsingCmd + ')"\n')
                    output.append("      for iter in $key_list; do\n")
                    output.append("        if [[ $iter =~ ^$cur ]]; then\n")
                    output.append('          COMPREPLY+=( "${iter//__/ }" )\n')
                    output.append("        fi\n")
                    output.append("      done\n")
                    output.append("      ;;\n")
            output.append("  esac\n")

    output.append("  return 0\n")
    output.append("}\n")
    output.append("complete -o default -F _" + func_name + " " + name + "\n")
    if name.endswith(".py"):
        output.append("complete -o default -F _" + func_name + " " + name[0:-3] + "\n")

    return output


def main(argv):
    if len(argv) < 2:
        showHelp()
        return 1

    gdaltools = [
        "gdal2tiles.py",
        "gdal2xyz.py",
        "gdaladdo",
        "gdalbuildvrt",
        "gdal_calc.py",
        "gdalcompare.py",
        "gdal-config",
        "gdal_contour",
        "gdaldem",
        "gdal_edit.py",
        "gdalenhance",
        "gdal_fillnodata.py",
        "gdal_grid",
        "gdalinfo",
        "gdallocationinfo",
        "gdalmanage",
        "gdal_merge.py",
        "gdalmove.py",
        "gdal_polygonize.py",
        "gdal_proximity.py",
        "gdal_rasterize",
        "gdal_retile.py",
        "gdal_sieve.py",
        "gdalsrsinfo",
        "gdaltindex",
        "gdaltransform",
        "gdal_translate",
        "gdalwarp",
        "gdal_viewshed",
        "gdal_create",
        "sozip",
        "gdal_footprint",
    ]

    ogrtools = [
        "ogr2ogr",
        "ogrinfo",
        "ogrlineref",
        "ogrtindex",
        "ogrmerge.py",
        "ogr_layer_algebra.py",
    ]

    # parse general options
    generalOptions = parseGDALGeneralOptions()
    generalOGROptions = parseOGRGeneralOptions()

    outFile = argv[1]
    of = open(outFile, "w")
    of.write("# shellcheck shell=bash disable=SC2148\n")
    of.write(
        "# WARNING: File auto-generated by completionFinder.py, do not modify manually\n"
    )
    of.write("""

if ! ps -p $$ >/dev/null 2>/dev/null; then
  # For busybox
  CURRENT_SHELL="bash"
else
  CURRENT_SHELL="$(sh -c 'ps -p $$ -o ppid=' | xargs ps -o comm= -p)"
fi

if [ "$CURRENT_SHELL" = "zsh" ]; then
  autoload -U bashcompinit
  bashcompinit
fi

HAS_GET_COMP_WORDS_BY_REF=no
if [ "$CURRENT_SHELL" = "bash" ]; then
  function_exists() {
      declare -f -F "$1" > /dev/null
      return $?
  }

  # Checks that bash-completion is recent enough
  if function_exists _get_comp_words_by_ref; then
    HAS_GET_COMP_WORDS_BY_REF=yes;
  fi
fi

_gdal()
{
  COMPREPLY=()
  cur="${COMP_WORDS[$COMP_CWORD]}"

  if test "$HAS_GET_COMP_WORDS_BY_REF" = "yes"; then
    local cur prev
    _get_comp_words_by_ref cur prev
    if test "$cur" = ""; then
      extra="last_word_is_complete=true"
    else
      extra="prev=${prev} cur=${cur}"
    fi
    choices=$(gdal completion ${COMP_LINE} ${extra})
  else
    choices=$(gdal completion ${COMP_LINE})
  fi
  if [ "$CURRENT_SHELL" = "bash" ]; then
    if [[ "$cur" == "=" ]]; then
      mapfile -t COMPREPLY < <(compgen -W "$choices" --)
    elif [[ "$cur" == ":" ]]; then
      mapfile -t COMPREPLY < <(compgen -W "$choices" --)
    elif [[ "$cur" == "!" ]]; then
      mapfile -t COMPREPLY < <(compgen -W "$choices" -P "! " --)
    else
      mapfile -t COMPREPLY < <(compgen -W "$choices" -- "$cur")
    fi
    for element in "${COMPREPLY[@]}"; do
      if [[ $element == */ ]]; then
        # Do not add a space if one of the suggestion ends with slash
        compopt -o nospace
        break
      elif [[ $element == *= ]]; then
        # Do not add a space if one of the suggestion ends with equal
        compopt -o nospace
        break
      elif [[ $element == *: ]]; then
        # Do not add a space if one of the suggestion ends with colon
        compopt -o nospace
        break
      fi
    done
  else
    # zsh
    if [ "$cur" = "=" ]; then
      COMPREPLY=( "$(compgen -W "$choices" --)" )
    elif [ "$cur" = ":" ]; then
      COMPREPLY=( "$(compgen -W "$choices" --)" )
    elif [ "$cur" = "!" ]; then
      COMPREPLY=( "$(compgen -W "$choices" -P "! " --)" )
    else
      COMPREPLY=( "$(compgen -W "$choices" -- "$cur")" )
    fi
    for element in "${COMPREPLY[@]}"; do
      if [[ $element == */ ]]; then
        # Do not add a space if one of the suggestion ends with slash
        _COMP_OPTIONS+=(nospace)
        break
      elif [[ $element == *= ]]; then
        # Do not add a space if one of the suggestion ends with equal
        _COMP_OPTIONS+=(nospace)
        break
      elif [[ $element == *: ]]; then
        # Do not add a space if one of the suggestion ends with colon
        _COMP_OPTIONS+=(nospace)
        break
      fi
    done
  fi
}

# nosort requires bash 4.4
complete -o nosort -o default -F _gdal gdal 2>/dev/null || complete -o default -F _gdal gdal

""")

    for name in gdaltools:
        # verbose print
        print(name)
        optList = processTool(name)
        if "--help-general" in optList:
            for item in generalOptions:
                if item not in optList:
                    optList.append(item)
        of.writelines(getCompletionScript(name, optList))
    for name in ogrtools:
        # verbose print
        print(name)
        optList = processTool(name)
        if "--help-general" in optList:
            for item in generalOGROptions:
                if item not in optList:
                    optList.append(item)
        of.writelines(getCompletionScript(name, optList))

    of.close()
    return 0


if __name__ == "__main__":
    main(sys.argv)
