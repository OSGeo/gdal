include ../../../GDALmake.opt

OBJ	=	ogrdb2cli.o gdaldb2rasterband.o ogrdb2datasourcemd.o ogrdb2driver.o ogrdb2datasource.o ogrdb2layer.o ogrdb2tablelayer.o ogrdb2selectlayer.o

CPPFLAGS	:=	 -iquote .. -iquote ../.. -iquote ../../../frmts/mem $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
