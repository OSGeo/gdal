#!/bin/bash

# requires xsltproc - if you are not on linux or prefer to use another XSLT processor, follow the human-readable instructions

cp ../ogrsosidatatypes.h ../ogrsosidatatypes.h.bak 2>/dev/null || true                     # make a backup of the old header

xsltproc basicelements.xslt Elementdefinisjoner.xml | tail -n +2  > ../ogrsosidatatypes.h  # apply basicelements.xslt, skip XML header
head -n -1 Gruppeelement_sammensetning.xml > both.xml                                      # concatenate both input files into a single file,
tail -n +3 Elementdefinisjoner.xml >> both.xml                                             #   moving all elements into the <dataroot> node
xsltproc groupelements.xslt both.xml | tail -n +2 >> ../ogrsosidatatypes.h                 # apply groupelements.xslt, skip XML header
rm both.xml                                                                                #   and attach to ogrsosidatatypes.h
