% ***************************************************************************
% $Id$
%
% Project:  GDAL
% Purpose:  METAPOST sources for the GDAL Grid Tutorial figures.
% Author:   Andrey Kiselev, dron@ak4719.spb.edu
%
% ***************************************************************************
% Copyright (c) 2008,  Andrey Kiselev <dron@ak4719.spb.edu>
%
% Permission is hereby granted, free of charge, to any person obtaining a
% copy of this software and associated documentation files (the "Software"),
% to deal in the Software without restriction, including without limitation
% the rights to use, copy, modify, merge, publish, distribute, sublicense,
% and/or sell copies of the Software, and to permit persons to whom the
% Software is furnished to do so, subject to the following conditions:
%
% The above copyright notice and this permission notice shall be included
% in all copies or substantial portions of the Software.
%
% THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
% OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
% FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
% THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
% LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
% FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
% DEALINGS IN THE SOFTWARE.
% ***************************************************************************

verbatimtex
%&latex
\documentclass{article}
\begin{document}
\large
etex;

%
% Illustration of the search ellipse
%
beginfig(1);
	u := 1mm;
	labeloffset := 1.5u;

	% draw ellipse
	r := 35u;	% Ellipse major radius
	scale := 0.625;
	a := 30;	% Ellipse rotation angle
	z0 = (0,0); z1 = (r,0); z2 = (0,r*scale);
	z0 = .5[z1,z3] = .5[z2,z4];
	draw fullcircle scaled 2r yscaled scale rotated a withpen pencircle scaled 0.6u;
	drawarrow z0..z1 rotated a dashed evenly scaled 1/2u;
	drawarrow z0..z2 rotated a dashed evenly scaled 1/2u;
	draw 0.2[z0,z1]..0.2[z0,z1 rotated .5a]..0.2[z0,z1 rotated a] withpen pencircle;
	draw thelabel.ulft(btex $radius_1$ etex, .5[z0,z1] rotated a);
	draw thelabel.lft(btex $radius_2$ etex, .5[z0,z2] rotated a);
	draw thelabel.rt(btex $angle$ etex, 0.2[z0,z1 rotated .5a]);

	% draw axes
	xr := 40;
	yr := 40;
	z11 = (-xr*u,0);
	z12 = (xr*u,0);
	z13 = (0,-yr*u);
	z14 = (0,yr*u);
	drawarrow z11--z12 withpen pencircle scaled 0.3u;
	drawarrow z13--z14 withpen pencircle scaled 0.3u;
	label.llft(btex $x$ etex, z12);
	label.llft(btex $y$ etex, z14);

	% draw dots
	z22[0]=(-3,-14);
	z22[1]=(-4,-30);
	z22[2]=(8,19);
	z22[3]=(26,35);
	z22[4]=(0,25);
	z22[5]=(9,35);
	z22[6]=(38,9);
	z22[7]=(10,30);
	z22[8]=(-37,2);
	z22[9]=(7,-24);
	z22[10]=(6,31);
	z22[11]=(1,16);
	z22[12]=(-35,29);
	z22[13]=(-31,15);
	z22[14]=(19,9);
	z22[15]=(-23,-22);
	z22[16]=(-23,21);
	z22[17]=(15,21);
	z22[18]=(-35,-29);
	z22[19]=(6,-11);
	for i=0 upto 19:
		drawdot z22[i]*u withpen pencircle scaled 2u;
	endfor;
endfig;

%
% Illustration of scattered data gridding
%
input boxes
beginfig(2);
	u := 1mm;
	xr := 30;
	yr := 30;
	xd := 10;
	yd := 10;

	picture Grid; Grid := nullpicture;
		for i = -xr step xd until xr:
			addto Grid doublepath (-xr*u,i*u)--(xr*u,i*u) withpen pencircle scaled 0.3u;
			addto Grid doublepath (i*u,-yr*u)--(i*u,yr*u) withpen pencircle scaled 0.3u;
 		endfor

	% draw grids
	draw Grid;
	draw Grid shifted (3xr*u,0);
	draw (1.1xr*u,-0.1yr*u)--(1.1xr*u,0.1yr*u)--(1.7xr*u,0.1yr*u)--(1.7xr*u,0.2yr*u)--(1.9xr*u,0)--(1.7xr*u,-0.2yr*u)--(1.7xr*u,-0.1yr*u)--(1.1xr*u,-0.1yr*u) withpen pencircle scaled 0.3u;
	label.rt(btex \large GRIDDING etex, (1.05xr*u, 0));

	% Draw scattered dots
	% Randomly scattered dots generated with the simple Python script:
	%
	% import random
	% for i in range(50):
	% 	print "z[%d]=(%d,%d);" % (i,random.randrange(-30,30),random.randrange(-30,30))

	z[0]=(-23,24);
	z[1]=(-1,3);
	z[2]=(12,-9);
	z[3]=(15,-14);
	z[4]=(18,-30);
	z[5]=(-7,-15);
	z[6]=(-1,-2);
	z[7]=(13,-5);
	z[8]=(-8,-13);
	z[9]=(-10,9);
	z[10]=(-12,7);
	z[11]=(5,-6);
	z[12]=(13,5);
	z[13]=(16,-28);
	z[14]=(8,18);
	z[15]=(1,13);
	z[16]=(-25,-1);
	z[17]=(3,-24);
	z[18]=(20,-29);
	z[19]=(-27,-10);
	z[20]=(22,-27);
	z[21]=(-7,-14);
	z[22]=(-24,19);
	z[23]=(26,-17);
	z[24]=(-17,-12);
	z[25]=(9,-26);
	z[26]=(-5,12);
	z[27]=(23,-22);
	z[28]=(14,5);
	z[29]=(-14,-19);
	z[30]=(29,19);
	z[31]=(16,-13);
	z[32]=(-5,25);
	z[33]=(21,25);
	z[34]=(9,8);
	z[35]=(9,28);
	z[36]=(22,-2);
	z[37]=(-15,13);
	z[38]=(-25,4);
	z[39]=(27,-17);
	z[40]=(-1,-23);
	z[41]=(10,-7);
	z[42]=(1,28);
	z[43]=(5,-19);
	z[44]=(-19,-16);
	z[45]=(-16,-26);
	z[46]=(22,2);
	z[47]=(-16,27);
	z[48]=(-29,25);
	z[49]=(29,26);
	for i=0 upto 49:
		drawdot z[i]*u withpen pencircle scaled 2u;
	endfor;

	for i = -xr step xd until xr:
	 	for j = -yr step yd until yr:
	 		drawdot (i*u,j*u) shifted (3xr*u,0) withpen pencircle scaled 2u;
		endfor;
	endfor
endfig;

 end.

