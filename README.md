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
