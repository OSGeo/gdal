
all: 	ellipse.eps ellipse.png gridding.eps gridding.png

figures: figures.mp
	mpost $<

%.dvi: %.tex figures
	latex $<

%.eps: 	%.dvi
	dvips -E -o $@ $<

%.png:	%.eps
	gs -q -dSAFER -dBATCH -dNOPAUSE -r100 -dTextAlphaBits=4 -dGraphicsAlphaBits=4 -dEPSCrop -sDEVICE=pnggray -sOutputFile=$@ $<

clean:
	rm -f *.mpx *.1 *.2 *.log *.dvi *.eps *.png
