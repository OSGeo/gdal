# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# AnsiEscapeSequences
# -------------------
#
# Define ANSI escape sequences for message outputs
#
# ::
#
#     Return colour ansi escape sequence
#
#     get_colour_escape_sequences(<out variable> <SPEC1> [<SPEC2>...])
#
#     Here spec is one of followings;
#     * Font face colours
#       FF_Black, FF_Red, FF_Green, FF_Yellow, FF_Blue, FF_Magenta, FF_Cyan, FF_White
#     * Back ground colours
#       BG_Black, BG_Red, BG_Green, BG_Yellow, BG_Blue, BG_Magenta, BG_Cyan, BG_White
#     * Itensity
#       Bold, Faint, Underline
#
#     * Special Sequence
#       ResetColour
#
get_property(_res GLOBAL PROPERTY AnsiEscapeSequences_colours.keys DEFINED)
if(NOT _res)
    # Colour Map initialization
    define_property(GLOBAL PROPERTY AnsiEscapeSequences_colours.keys
                    BRIEF_DOCS "Key of map for AnsiEscapeSquences module"
                    FULL_DOCS "Key of map for AnsiEscapeSquences module")
    define_property(GLOBAL PROPERTY AnsiEscapeSequences_colours.vals
                    BRIEF_DOCS "Values of map for AnsiEscapeSquences module"
                    FULL_DOCS "Values of map for AnsiEscapeSquences module")
    set_property(GLOBAL PROPERTY AnsiEscapeSequences_colours.keys FF_Black FF_Red FF_Green FF_Yellow FF_Blue FF_Magenta FF_Cyan FF_White Bold Faint BG_Black)
    set_property(GLOBAL PROPERTY AnsiEscapeSequences_colours.vals 30 31 32 33 34 35 36 37 1 2 40)
endif()

function(get_colour_escape_sequence _output)
    string(ASCII 27 Esc)
    if(ARGN STREQUAL ColourReset)
        set(${_output}  "${Esc}[m" PARENT_SCOPE)
        return()
    endif()

    get_property(keys GLOBAL PROPERTY AnsiEscapeSequences_colours.keys)
    get_property(vals GLOBAL PROPERTY AnsiEscapeSequences_colours.vals)
    set(_out)
    foreach(item IN LISTS ARGN)
        list(FIND keys ${item} index)
        if(index GREATER -1)
            list(GET vals ${index} val)
            list(APPEND _out ${val})
        endif()
    endforeach()
    list(LENGTH _out len)
    if(len GREATER 1)
        list(SORT _out)
    endif()
    set(${_output} "${Esc}[${_out}m" PARENT_SCOPE)
endfunction()
