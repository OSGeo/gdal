#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Project:  S-57 OGR Translator
#  Purpose:  Script to translate s57 .csv files into C code "data" statements.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
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
# 
# $Log$
# Revision 1.1  2001/12/17 22:33:06  warmerda
# New
#

import sys
import os
import string

# -----------------------------------------------------------------------------
#	EscapeLine - escape anything C-problematic in a line.
# -----------------------------------------------------------------------------

def EscapeLine( line ):

    line_out = ''
    for lchar in line:
        if lchar == '"':
            line_out += '\\"'
        else:
            line_out += lchar

    return line_out

# -----------------------------------------------------------------------------
# 

if __name__ != '__main__':
    print 'This module should only be used as a mainline.'
    sys.exit( 1 )

if len(sys.argv) < 2:
    directory = os.environ['S57_CSV']
else:
    directory = sys.argv[1]


print 'char *gpapszS57Classes[] = {'
classes = open( directory + '/s57objectclasses.csv' ).readlines()

for line in classes:
    print '"%s",' % EscapeLine(string.strip(line))
    
print 'NULL };'

print 'char *gpapszS57attributes[] = {'
classes = open( directory + '/s57attributes.csv' ).readlines()

for line in classes:
    print '"%s",' % EscapeLine(string.strip(line))
    
print 'NULL };'

    
