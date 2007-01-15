#!/bin/sh

#
#	Test stream for img2tif
#

IMG_DATA_DIR=/usr2/data/imagine

#
#	Create a local directory to do the test operations in, so it
#	is easy to cleanup.
#
TST_DIR=img2tif_work

if test -d $TST_DIR ; then
  rm -rf $TST_DIR
fi

mkdir $TST_DIR

#
#	Do each of the projection files
#

for IMG in $IMG_DATA_DIR/Eprj/*.img ; do
  BASE=`basename $IMG .img`
  img2tif -i $IMG -o $TST_DIR/$BASE

  if test -r $TST_DIR/$BASE.tif ; then
    tst1tif.sh $TST_DIR/${BASE}.tif $IMG_DATA_DIR/Eprj/${BASE}.tlg
  else
    tst1tif.sh $TST_DIR/${BASE}1.tif $IMG_DATA_DIR/Eprj/${BASE}1.tlg
    tst1tif.sh $TST_DIR/${BASE}2.tif $IMG_DATA_DIR/Eprj/${BASE}2.tlg
    tst1tif.sh $TST_DIR/${BASE}3.tif $IMG_DATA_DIR/Eprj/${BASE}3.tlg
  fi
  rm $TST_DIR/${BASE}*.tif
done

#
#	Do all of irvine2.img, small_tm.img, springs.img and irvlong2.img
#
img2tif -i $IMG_DATA_DIR/irvine2.img -o $TST_DIR/irvine2
for i in 1 2 3 4 5 6 7 8 9 10 11 ; do
    tst1tif.sh $TST_DIR/irvine2${i}.tif $IMG_DATA_DIR/irvine2${i}.tlg
done
rm -f $TST_DIR/*.tif

img2tif -i $IMG_DATA_DIR/small_tm.img -o $TST_DIR/small_tm
for i in 1 2 3 4 5 ; do
    tst1tif.sh $TST_DIR/small_tm${i}.tif $IMG_DATA_DIR/small_tm${i}.tlg
done

img2tif -i $IMG_DATA_DIR/springs1.img -o $TST_DIR/springs1
for i in 1 2 3 ; do
    tst1tif.sh $TST_DIR/springs1${i}.tif $IMG_DATA_DIR/springs1${i}.tlg
done

img2tif -i $IMG_DATA_DIR/irvlong2.img -o $TST_DIR/irvlong2
tst1tif.sh $TST_DIR/irvlong2.tif $IMG_DATA_DIR/irvlong2.tlg


#rn -rf $TST_DIR
