#!/bin/sh
#
# Author: Mateusz Loskot, mateusz@loskot
# Revision: $Revision$
# Date: $Date$
#
# The script purpose is to reset svn:keywords property for files
# identified as using keywords.
#
# Set to 1 to print detailed listing of touched files and set keywords
VERBOSE=0

if test ! $# = 1; then
    echo "Usage: `basename $0` <source path>"
    exit 1
fi

SVNSRCPATH=${1}
if test ! -d ${SVNSRCPATH}; then
    echo "*** Given source location is not valid directory '${SVNSRCPATH}'"
    exit 1
fi

SVN=`which svn`
FIND=`which find`
GREP=`which grep`

if test ! -x ${SVN}; then
    echo "Can not find Subversion program"
    exit 1
fi

if test ! -x ${FIND}; then
    echo "Can not locate find program"
    exit 1
fi

if test ! -x ${GREP}; then
    echo "Can not locate grep program"
    exit 1
fi

echo "Entering '${SVNSRCPATH}'"
echo "Setting svn:keywords property\c"

${FIND} ${SVNSRCPATH} \( -path '*/.svn' \) -prune -o -type f -print | while read file ; do

    plist=""
    for p in Author Date Id Rev ; do
        if ${GREP} -q '\$'${p}'' "${file}" ; then 
            plist="${p} $plist"
        fi
    done

    if [ "$plist" != "" ] ; then 
        ${SVN} propset svn:keywords "${plist%% }" ${file} > /dev/null 2>&1
        if test ${VERBOSE} = 1; then
            echo "${SVN} propset svn:keywords "${plist%% }" ${file} "
        else
            echo ".\c"
        fi
    fi
done

echo "done."
