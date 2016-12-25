################################################################################
# SociUtilities.cmake - part of CMake configuration of Proj4 library
#
# Based on BoostUtilities.cmake from CMake configuration for Boost
################################################################################
# Copyright (C) 2007 Douglas Gregor <doug.gregor@gmail.com>
# Copyright (C) 2007 Troy Straszheim
# Copyright (C) 2010 Mateusz Loskot <mateusz@loskot.net> 
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt
################################################################################
# Macros in this module:
#
#   list_contains: Determine whether a string value is in a list.
#
#   car: Return the first element in a list
#
#   cdr: Return all but the first element in a list
#
#   parse_arguments: Parse keyword arguments for use in other macros.
#
#   proj_report_directory_property
#
#   proj_target_output_name: 
#
################################################################################

# This utility macro determines whether a particular string value
# occurs within a list of strings:
#
#  list_contains(result string_to_find arg1 arg2 arg3 ... argn)
# 
# This macro sets the variable named by result equal to TRUE if
# string_to_find is found anywhere in the following arguments.
macro(list_contains var value)
  set(${var})
  foreach (value2 ${ARGN})
    if (${value} STREQUAL ${value2})
      set(${var} TRUE)
    endif (${value} STREQUAL ${value2})
  endforeach (value2)
endmacro(list_contains)

# This utility macro extracts the first argument from the list of
# arguments given, and places it into the variable named var.
#
#   car(var arg1 arg2 ...)
macro(car var)
  set(${var} ${ARGV1})
endmacro(car)

# This utility macro extracts all of the arguments given except the
# first, and places them into the variable named var.
#
#   car(var arg1 arg2 ...)
macro(cdr var junk)
  set(${var} ${ARGN})
endmacro(cdr)

# The parse_arguments macro will take the arguments of another macro and
# define several variables. The first argument to parse_arguments is a
# prefix to put on all variables it creates. The second argument is a
# list of names, and the third argument is a list of options. Both of
# these lists should be quoted. The rest of parse_arguments are
# arguments from another macro to be parsed.
# 
#     parse_arguments(prefix arg_names options arg1 arg2...) 
# 
# For each item in options, parse_arguments will create a variable with
# that name, prefixed with prefix_. So, for example, if prefix is
# MY_MACRO and options is OPTION1;OPTION2, then parse_arguments will
# create the variables MY_MACRO_OPTION1 and MY_MACRO_OPTION2. These
# variables will be set to true if the option exists in the command line
# or false otherwise.
# 
# For each item in arg_names, parse_arguments will create a variable
# with that name, prefixed with prefix_. Each variable will be filled
# with the arguments that occur after the given arg_name is encountered
# up to the next arg_name or the end of the arguments. All options are
# removed from these lists. parse_arguments also creates a
# prefix_DEFAULT_ARGS variable containing the list of all arguments up
# to the first arg_name encountered.
macro(parse_arguments prefix arg_names option_names)
  set(DEFAULT_ARGS)
  foreach(arg_name ${arg_names})
    set(${prefix}_${arg_name})
  endforeach(arg_name)
  foreach(option ${option_names})
    set(${prefix}_${option} FALSE)
  endforeach(option)

  set(current_arg_name DEFAULT_ARGS)
  set(current_arg_list)
  foreach(arg ${ARGN})
    list_contains(is_arg_name ${arg} ${arg_names})
    if (is_arg_name)
      set(${prefix}_${current_arg_name} ${current_arg_list})
      set(current_arg_name ${arg})
      set(current_arg_list)
    else (is_arg_name)
      list_contains(is_option ${arg} ${option_names})
      if (is_option)
        set(${prefix}_${arg} TRUE)
      else (is_option)
        set(current_arg_list ${current_arg_list} ${arg})
      endif (is_option)
    endif (is_arg_name)
  endforeach(arg)
  set(${prefix}_${current_arg_name} ${current_arg_list})
endmacro(parse_arguments)

# Perform a reverse topological sort on the given LIST. 
#   
#   topological_sort(my_list "MY_" "_EDGES")
#
# LIST is the name of a variable containing a list of elements to be
# sorted in reverse topological order. Each element in the list has a
# set of outgoing edges (for example, those other list elements that
# it depends on). In the resulting reverse topological ordering
# (written back into the variable named LIST), an element will come
# later in the list than any of the elements that can be reached by
# following its outgoing edges and the outgoing edges of any vertices
# they target, recursively. Thus, if the edges represent dependencies
# on build targets, for example, the reverse topological ordering is
# the order in which one would build those targets.
#
# For each element E in this list, the edges for E are contained in
# the variable named ${PREFIX}${E}${SUFFIX}, where E is the
# upper-cased version of the element in the list. If no such variable
# exists, then it is assumed that there are no edges. For example, if
# my_list contains a, b, and c, one could provide a dependency graph
# using the following variables:
#
#     MY_A_EDGES     b
#     MY_B_EDGES     
#     MY_C_EDGES     a b
#
#  With the involcation of topological_sort shown above and these
#  variables, the resulting reverse topological ordering will be b, a,
#  c.
function(topological_sort LIST PREFIX SUFFIX)
  # Clear the stack and output variable
  set(VERTICES "${${LIST}}")
  set(STACK)
  set(${LIST})

  # Loop over all of the vertices, starting the topological sort from
  # each one.
  foreach(VERTEX ${VERTICES})
    string(TOUPPER ${VERTEX} UPPER_VERTEX)

    # If we haven't already processed this vertex, start a depth-first
    # search from where.
    if (NOT FOUND_${UPPER_VERTEX})
      # Push this vertex onto the stack with all of its outgoing edges
      string(REPLACE ";" " " NEW_ELEMENT 
        "${VERTEX};${${PREFIX}${UPPER_VERTEX}${SUFFIX}}")
      list(APPEND STACK ${NEW_ELEMENT})

      # We've now seen this vertex
      set(FOUND_${UPPER_VERTEX} TRUE)

      # While the depth-first search stack is not empty
      list(LENGTH STACK STACK_LENGTH)
      while(STACK_LENGTH GREATER 0)
        # Remove the vertex and its remaining out-edges from the top
        # of the stack
        list(GET STACK -1 OUT_EDGES)
        list(REMOVE_AT STACK -1)

        # Get the source vertex and the list of out-edges
        separate_arguments(OUT_EDGES)
        list(GET OUT_EDGES 0 SOURCE)
        list(REMOVE_AT OUT_EDGES 0)

        # While there are still out-edges remaining
        list(LENGTH OUT_EDGES OUT_DEGREE)
        while (OUT_DEGREE GREATER 0)
          # Pull off the first outgoing edge
          list(GET OUT_EDGES 0 TARGET)
          list(REMOVE_AT OUT_EDGES 0)

          string(TOUPPER ${TARGET} UPPER_TARGET)
          if (NOT FOUND_${UPPER_TARGET})
            # We have not seen the target before, so we will traverse
            # its outgoing edges before coming back to our
            # source. This is the key to the depth-first traversal.

            # We've now seen this vertex
            set(FOUND_${UPPER_TARGET} TRUE)

            # Push the remaining edges for the current vertex onto the
            # stack
            string(REPLACE ";" " " NEW_ELEMENT 
              "${SOURCE};${OUT_EDGES}")
            list(APPEND STACK ${NEW_ELEMENT})

            # Setup the new source and outgoing edges
            set(SOURCE ${TARGET})
            string(TOUPPER ${SOURCE} UPPER_SOURCE)
            set(OUT_EDGES 
              ${${PREFIX}${UPPER_SOURCE}${SUFFIX}})
          endif(NOT FOUND_${UPPER_TARGET})

          list(LENGTH OUT_EDGES OUT_DEGREE)
        endwhile (OUT_DEGREE GREATER 0)

        # We have finished all of the outgoing edges for
        # SOURCE; add it to the resulting list.
        list(APPEND ${LIST} ${SOURCE})

        # Check the length of the stack
        list(LENGTH STACK STACK_LENGTH)
      endwhile(STACK_LENGTH GREATER 0)
    endif (NOT FOUND_${UPPER_VERTEX})
  endforeach(VERTEX)

  set(${LIST} ${${LIST}} PARENT_SCOPE)
endfunction(topological_sort)

# Small little hack that tweaks a component name (as used for CPack)
# to make sure to avoid certain names that cause problems. Sets the
# variable named varname to the "sanitized" name.
#
# FIXME: This is a complete hack. We probably need to fix the CPack
# generators (NSIS in particular) to get rid of the need for this.
macro(fix_cpack_component_name varname name)
  if (${name} STREQUAL "foreach")
    set(${varname} "boost_foreach")
  else()
    set(${varname} ${name})
  endif()
endmacro()


#
# A big shout out to the cmake gurus @ compiz
#

function (colormsg)
  string (ASCII 27 _escape)
  set(WHITE "29")
  set(GRAY "30")
  set(RED "31")
  set(GREEN "32")
  set(YELLOW "33")
  set(BLUE "34")
  set(MAG "35")
  set(CYAN "36")

  foreach (color WHITE GRAY RED GREEN YELLOW BLUE MAG CYAN)
    set(HI${color} "1\;${${color}}")
    set(LO${color} "2\;${${color}}")
    set(_${color}_ "4\;${${color}}")
    set(_HI${color}_ "1\;4\;${${color}}")
    set(_LO${color}_ "2\;4\;${${color}}")
  endforeach()

  set(str "")
  set(coloron FALSE)
  foreach(arg ${ARGV})
    if (NOT ${${arg}} STREQUAL "")
      if (CMAKE_COLOR_MAKEFILE)
        set(str "${str}${_escape}[${${arg}}m")
        set(coloron TRUE)
      endif()
    else()
      set(str "${str}${arg}")
      if (coloron)
        set(str "${str}${_escape}[0m")
        set(coloron FALSE)
      endif()
      set(str "${str} ")
    endif()
  endforeach()
  message(STATUS ${str})
endfunction()

# colormsg("Colors:"  
#   WHITE "white" GRAY "gray" GREEN "green" 
#   RED "red" YELLOW "yellow" BLUE "blue" MAG "mag" CYAN "cyan" 
#   _WHITE_ "white" _GRAY_ "gray" _GREEN_ "green" 
#   _RED_ "red" _YELLOW_ "yellow" _BLUE_ "blue" _MAG_ "mag" _CYAN_ "cyan" 
#   _HIWHITE_ "white" _HIGRAY_ "gray" _HIGREEN_ "green" 
#   _HIRED_ "red" _HIYELLOW_ "yellow" _HIBLUE_ "blue" _HIMAG_ "mag" _HICYAN_ "cyan" 
#   HIWHITE "white" HIGRAY "gray" HIGREEN "green" 
#   HIRED "red" HIYELLOW "yellow" HIBLUE "blue" HIMAG "mag" HICYAN "cyan" 
#   "right?")

#
#  pretty-prints the value of a variable so that the 
#  equals signs align
#

function(boost_report_value NAME)
  string(LENGTH "${NAME}" varlen)
  # LOG
  #message(STATUS "boost_report_value: NAME=${NAME} (${varlen})")
  #message(STATUS "boost_report_value: \${NAME}=${${NAME}}")
  math(EXPR padding_len 40-${varlen})
  string(SUBSTRING "                                      " 
    0 ${padding_len} varpadding)
  colormsg("${NAME}${varpadding} = ${${NAME}}")
endfunction()

function(trace NAME)
  if(BOOST_CMAKE_TRACE)
    string(LENGTH "${NAME}" varlen)
    math(EXPR padding_len 40-${varlen})
    string(SUBSTRING "........................................"
      0 ${padding_len} varpadding)
    message("${NAME} ${varpadding} ${${NAME}}")
  endif()
endfunction()  

#
#  pretty-prints the value of a variable so that the 
#  equals signs align
#
function(boost_report_pretty PRETTYNAME VARNAME)
  string(LENGTH "${PRETTYNAME}" varlen)
  math(EXPR padding_len 30-${varlen})
  string(SUBSTRING "                                      " 
    0 ${padding_len} varpadding)
  message(STATUS "${PRETTYNAME}${varpadding} = ${${VARNAME}}")
endfunction()

#
#  assert that ARG is actually a library target
#

macro(dependency_check ARG)
  trace(ARG)
  if (NOT "${ARG}" STREQUAL "")
    get_target_property(deptype ${ARG} TYPE)
    if(NOT deptype MATCHES ".*_LIBRARY$")
      set(DEPENDENCY_OKAY FALSE)
      list(APPEND DEPENDENCY_FAILURES ${ARG})
    endif()
  endif()
endmacro()



#
# Pretty-print of given property of current directory.
#
macro(proj_report_directory_property PROPNAME)
  get_directory_property(${PROPNAME} ${PROPNAME})
  boost_report_value(${PROPNAME})
endmacro()

#
# Scans the current directory and returns a list of subdirectories.
# Author: Robert Fleming
# Source: http://www.cmake.org/pipermail/cmake/2008-February/020114.html
#
# Third parameter is 1 if you want relative paths returned.
# Usage: list_subdirectories(the_list_is_returned_here /path/to/project TRUE)
#

macro(list_subdirectories retval curdir return_relative)
  file(GLOB sub-dir RELATIVE ${curdir} *)
  set(list_of_dirs "")
  foreach(dir ${sub-dir})
    if(IS_DIRECTORY ${curdir}/${dir})
      if (${return_relative})
        set(list_of_dirs ${list_of_dirs} ${dir})
      else()
        set(list_of_dirs ${list_of_dirs} ${curdir}/${dir})
      endif()
    endif()
  endforeach()
  set(${retval} ${list_of_dirs})
endmacro()

#
# Generates output name for given target depending on platform and version.
# For instance, on Windows, libraries get ABI version suffix soci_coreXY.{dll|lib}.
#

function(proj_target_output_name TARGET_NAME OUTPUT_NAME)
  if(NOT DEFINED TARGET_NAME)
    message(SEND_ERROR "Error, the variable TARGET_NAME is not defined!")
  endif()

  if(NOT DEFINED ${PROJECT_INTERN_NAME}_VERSION)
    message(SEND_ERROR "Error, the variable ${${PROJECT_INTERN_NAME}_VERSION} is not defined!")
  endif()

  # On Windows, ABI version is specified using binary file name suffix.
  # On Unix, suffix is empty and SOVERSION is used instead.
  if (WIN32)
    string(LENGTH "${${PROJECT_INTERN_NAME}_ABI_VERSION}" abilen)
    if(abilen GREATER 0)
      set(SUFFIX "_${${PROJECT_INTERN_NAME}_ABI_VERSION}")
    endif()
  endif()

  set(${OUTPUT_NAME} ${TARGET_NAME}${SUFFIX} PARENT_SCOPE)
endfunction()


#
# conversion from lla name to lla convert name ( without lla extension)
#

function(proj_lla_output_name LLA_INPUT_NAME LLA_OUTPUT_NAME  )
    get_filename_component(filename ${LLA_INPUT_NAME} NAME_WE)
    get_filename_component(pathname ${LLA_INPUT_NAME} PATH)
    set(${LLA_OUTPUT_NAME} ${pathname}/${filename} PARENT_SCOPE)
        set(${LLA_OUTPUT_NAME} ${pathname}/${filename} PARENT_SCOPE)
endfunction()

function(proj_lla_target_name LLA_INPUT_NAME  LLA_TARGET )
    get_filename_component(filename ${LLA_INPUT_NAME} NAME_WE)
    set(${LLA_TARGET} ${filename} PARENT_SCOPE)
endfunction()

#
# in place conversion of lla file to gsb 
#

function(proj_convert_grid_lla2gsb GRID_DIRECTORY) 
    set(NAD2BIN_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(NAD2BIN_PATH ${NAD2BIN_DIR}/nad2bin${CMAKE_EXECUTABLE_SUFFIX})
    file(TO_NATIVE_PATH ${NAD2BIN_PATH} NAD2BIN_EXE)
    file(GLOB LLA_FILES  ${${GRID_DIRECTORY}}/*.lla)
    foreach(LLA ${LLA_FILES} )
        proj_lla_output_name(${LLA} DEST_FILE)
        file(TO_NATIVE_PATH ${DEST_FILE} DEST)
        proj_lla_target_name(${LLA} LLA_TARGET)
        if(NOT EXISTS ${DEST})
            add_custom_target( ${LLA_TARGET} ALL
               COMMAND ${NAD2BIN_EXE} ${DEST} "<" ${LLA}
               DEPENDS nad2bin )
        endif(NOT EXISTS ${DEST})
    endforeach(LLA)
endfunction()

#
# add lla output list to an existing file list
#

function(proj_append_lla_output_file LLA_INPUT_FILE  FILE_LIST)
     set(LIST_OUT ${${FILE_LIST}})
     foreach(LLA ${${LLA_INPUT_FILE}} )
        proj_lla_output_name(${LLA} DEST_FILE)
        file(TO_NATIVE_PATH ${DEST_FILE} DEST)
        set(LIST_OUT ${LIST_OUT} ${DEST_FILE} )
    endforeach(LLA ${LLA_INPUT_FILE})
    set(${FILE_LIST} ${LIST_OUT} PARENT_SCOPE)
endfunction()

