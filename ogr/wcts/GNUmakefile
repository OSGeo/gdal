GDAL_ROOT	=	../..

#CURL_LIB = -lcurl
#OTHER_OPTS :=	$(OTHER_OPTS) -DHAVE_CURL

OTHER_OPTS :=	$(OTHER_OPTS) -DDISABLE_USER_DEFINED_CRS

include ../../GDALmake.opt

LINCLUDE =	-I../ogrsf_frmts -I.. -I$(GDAL_ROOT)/port $(OTHER_OPTS)

WCTS_OBJ	=	ogrwcts.o
C_WCTS_OBJ	=	wctsclient.o

BIN	=	ogrwcts
C_BIN	=	wctsclient

default:	$(BIN)

all:	default $(C_BIN)

clean:
	rm -f *.o $(BIN) $(C_BIN)

$(BIN):	$(WCTS_OBJ)
	$(CXX) $(LNK_FLAGS) $(WCTS_OBJ) $(CONFIG_LIBS) $(CURL_LIB) -o $(BIN)

$(C_BIN):	$(C_WCTS_OBJ)
	$(CXX) $(LNK_FLAGS) $(C_WCTS_OBJ) $(CONFIG_LIBS) $(CURL_LIB) \
		-o $(C_BIN)

%.o:	%.c
	$(CC) -c $(LINCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

