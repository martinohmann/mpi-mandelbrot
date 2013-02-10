mpi-mandelbrot
==============

Dynamic MPI mandelbrot algorithm

Usage
-----

	usage: ./mandelbrot [options]

	OPTIONS:
	    -h                   Shows this help.
	    -c {width}           Width of resulting image. Has to be positive integer.
	                         (default: 1024)
	    -r {height}          Height of resulting image. Has to be positive integer.
	                         (default: 1024)
	    -n {iterations}      Maximum number of iterations for each pixel. Has to be
	                         positive integer (default: 2000)
	    -o {filename}        Filename of resulting bitmap. (default: ./mandelbrot.bmp)
	    -b {blocksize}       Number of rows to be assigned to a slave at once.
	                         Has to be smaller than (height/slave-count).
	                         Has to be a divisor of height. (default: 1)
	    -x {offset}          X-offset from [0,0]. (default: 0)
	    -y {offset}          Y-offset from [0,0]. (default: 0)
	    -a {length}          Absolute value range of x/y-axis, e.g. if length was 2, 
	                         displayed x/y-values would range from -1 to 1. 
	                         If the x/y-offsets are set, axis shifts by those offsets.
	                         Negative value inverts axis.
	                         Has to be non-zero double value. (default: 2)
	    -p {hexnum}          Minimum color of the resulting image. (default: 0x000000)
	    -q {hexnum}          Maximum color of the resulting image. (default: 0xffffff)
	    -m {hexnum}          Hex mask to manipulate color ranges. (default: 0xffffff)
	    -s                   Print progress of the computation.

Usage examples
--------------

Zoomed out

    mpirun -np 4 ./mandelbrot -n 20000 -a 3.5


Inverted axis and blocksize 8

    mpirun -np 4 ./mandelbrot -n 5000 -b 8 -a -2


Shift and zoom

    mpirun -np 4 ./mandelbrot -n 10000 -x -1 -y -1 -a 1


Inverted colors and custom output file

    mpirun -np 4 ./mandelbrot -n 10000 -p 0xffffff -q 0x000000 -o ./output.bmp


Inverted color with color mask

    mpirun -np 4 ./mandelbrot -n 10000 -p 0xffffff -q 0x000000 -m 0x00bb00


Shifted, zoomed in, increased iteration count and custom min-color value 

    mpirun -np 4 ./mandelbrot -n 30000 -x -1 -y -1 -a 1 -p 0x0099ff

The "head" of the mandelbrot "thing": Custom min-color value and filtered blue part

    mpirun -np 4 ./mandelbrot -n 5000 -a 0.5 -x -1 -p 0xbb9955 -m 0xffff00
    
    
Show progress bar on master

	mpirun -np 4 ./mandelbrot -n 100000 -s