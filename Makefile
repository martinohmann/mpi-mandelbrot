#
# Dynamic MPI Mandelbrot algorithm
# Copyright (C) 2015  Martin Ohmann <martin@mohmann.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	-rm -f $(ALL) mandelbrot.bmp

