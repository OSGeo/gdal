#!/bin/sh
set -e
if git --version >/dev/null 2>/dev/null && grep dev gdal_version.h.in >/dev/null; then
        REV=`git log -1 --format="%h"`
        DATE=`git log -1 --date=format:'%Y%m%d' --format="%ad" 2>/dev/null` || DATE=""
        if git status --porcelain -uno | grep . >/dev/null; then REV="$REV-dirty"; fi
        if test -f gdal_version.h; then
                cp gdal_version.h gdal_version.h.bak
        else
                touch gdal_version.h.bak
        fi
        echo "/* This is a generated file from gdal_version.h.in. DO NOT MODIFY !!!! */" > gdal_version.h.new
        echo "" >> gdal_version.h.new
        cat gdal_version.h.in >> gdal_version.h.new
        sed -i.bak "s/dev/dev\-$REV/" gdal_version.h.new && rm gdal_version.h.new.bak
        if test "$DATE" != ""; then
                sed -i.bak "s/define GDAL_RELEASE_DATE.*/define GDAL_RELEASE_DATE     $DATE/" gdal_version.h.new && rm gdal_version.h.new.bak
        fi
        diff -u gdal_version.h.new gdal_version.h.bak >/dev/null || \
            (echo "Update gdal_version.h"; \
             cp gdal_version.h.new gdal_version.h)
        rm -f gdal_version.h.bak
        rm -f gdal_version.h.new
else
        diff -u gdal_version.h gdal_version.h.in 2>/dev/null >/dev/null || \
            (echo "Update gdal_version.h"; \
             echo "/* This is a generated file from gdal_version.h.in. DO NOT MODIFY !!!! */" > gdal_version.h; \
             echo "" >> gdal_version.h ; \
             cat gdal_version.h.in >> gdal_version.h )
fi
