#
# Makefile for dynamic MPI Mandelbrot program
# 
# @author Martin Ohmann <ohmann@uni-potsdam.de>
#
# "make" or "make all" to make all executables
# "make clean" to remove executables
#

MPICC       = mpicc
CFLAGS      = -O3 -Wall -pedantic -std=c99
LFLAGS      =  

ALL = mandelbrot
HFILES = mandelbrot.h

.PHONY:  all
all:  $(ALL)

%: %.c $(HFILES)
	$(MPICC) -o $@ $(CFLAGS) $< $(LFLAGS)

.PHONY:  clean
clean:
	-rm -f $(ALL)

