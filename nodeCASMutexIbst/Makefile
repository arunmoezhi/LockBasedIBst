TBB_INC=${TBBROOT}/include
TBB_LIB=${TBBROOT}/lib/intel64
TBBFLAGS=-I$(TBB_INC) -Wl,-rpath,$(TBB_LIB) -L$(TBB_LIB) -ltbb
GSL_INC=/opt/apps/intel13/gsl/1.15/include
GSL_LIB=/opt/apps/intel13/gsl/1.15/lib
GSLFLAGS=-I$(GSL_INC) -Wl,-rpath,$(GSL_LIB) -L$(GSL_LIB) -lgsl -lgslcblas
JEMALLOC_INC=${WORK}/jemalloc/installpath/include
JEMALLOC_LIB=${WORK}/jemalloc/installpath/lib
JEMALLOCFLAGS=-I$(JEMALLOC_INC) -Wl,-rpath,$(JEMALLOC_LIB) -L$(JEMALLOC_LIB) -ljemalloc
TCMALLOC_INC=${WORK}/tcmalloc/installpath/include
TCMALLOC_LIB=${WORK}/tcmalloc/installpath/lib
TCMALLOCFLAGS=-I$(TCMALLOC_INC) -Wl,-rpath,$(TCMALLOC_LIB) -L$(TCMALLOC_LIB) -ltcmalloc
CC=g++
CFLAGS= -O3 -lrt -lpthread -march=native
SRC= ./src/LockBasedIBst.c ./src/TestLockBasedIBst.c
OBJ = ./bin/LockBasedIBst.o
ibst: ./src/LockBasedIBst.c ./src/TestLockBasedIBst.c
	$(CC) $(CFLAGS) $(TBBFLAGS) $(GSLFLAGS) $(JEMALLOCFLAGS) -o $(OBJ) $(SRC)
clean:
	rm -rf ./bin/*.*
