/*
 * Dynamic MPI Mandelbrot algorithm
 * Copyright (C) 2015  Martin Ohmann <martin@mohmann.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "mandelbrot.h"

/* 
 * main
 */
int main(int argc, char **argv) 
{
    int proc_count, proc_id, retval;
    mo_opts_t *opts;

    /* initialize MPI */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        eprintf("MPI initialization failed.\n");
        exit(EXIT_FAILURE);
    }
    
    /* get number of avaiable processes */
    MPI_Comm_size(MPI_COMM_WORLD, &proc_count);

    if (proc_count < 2) {
        eprintf("Number of processes must be at least 2.\n");
        finalize_exit(EXIT_FAILURE);
    }

    /* get current process id */
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_id);

    opts = (mo_opts_t *) malloc(sizeof(*opts));

    if (opts == NULL) {
        eprintf("unable to allocate memory for config.\n");
        finalize_exit(EXIT_FAILURE);
    }

    /* parse args and store them in opts */
    retval = parse_args(argc, argv, opts, proc_id, proc_count);
    
    if (retval == EXIT_SUCCESS) {
        /* depending on process id run as master (0) or slave (n) */
        if (proc_id == 0) {
            retval = master_proc(proc_count - 1, opts);
        } else {
            retval = slave_proc(proc_id, opts);
        }
    }

    free(opts);
    MPI_Finalize();

    return retval;
}

/*
 * parse CLI args and store them into opts 
 */
static int parse_args(int argc, char **argv, mo_opts_t *opts, int proc_id, int proc_count) 
{
    /* set default values first */
    opts->max_iterations = MO_MAXITER;
    opts->width = MO_SIZE;
    opts->height = MO_SIZE;
    opts->filename = MO_FILENAME;
    opts->min_color = MO_COLORMIN;
    opts->max_color = MO_COLORMAX;
    opts->color_mask = MO_COLORMASK;
    opts->blocksize = MO_BLOCKSIZE;
    opts->show_progress = MO_PROGRESS;

    double x_offset = 0;
    double y_offset = 0;
    double axis_length = MO_N;

    const char *opt_string = "c:r:n:hb:p:q:m:x:y:a:o:s";

    int optval_int, c, index;
    long optval_long;
    double optval_double;

    /* suppress getopt error output, we handle output */
    opterr = 0;

    /* start parsing args */
    while ((c = getopt(argc, argv, opt_string)) != -1) {
        switch (c) {
            case 'b': /* blocksize */
            case 'c': /* width */
            case 'r': /* height */
            case 'n': /* iterations */
                optval_int = atoi(optarg);

                if (optval_int <= 0) {
                    if (proc_id == 0) {
                        print_usage(argv);
                        eprintf("argument of '-%c' has to be greater than zero.\n", c);
                    }
                    return EXIT_FAILURE;
                }

                if (c == 'c') opts->width = optval_int; else
                if (c == 'r') opts->height = optval_int; else
                if (c == 'n') opts->max_iterations = optval_int; else
                if (c == 'b') opts->blocksize = optval_int;
                break;
            case 'p': /* colormin */
            case 'q': /* colormax */
            case 'm': /* colormask */
                optval_long = strtol(optarg, NULL, 16);

                if (c == 'p') opts->min_color = optval_long; else
                if (c == 'q') opts->max_color = optval_long; else
                if (c == 'm') opts->color_mask = optval_long;
                break;
            case 'x': /* xoffset */
            case 'y': /* yoffset */
            case 'a': /* axis-length */
                optval_double = atof(optarg);

                if (c == 'x') x_offset = optval_double; else
                if (c == 'y') y_offset = optval_double; else
                if (c == 'a') { 
                    if (optval_double == 0) {
                        if (proc_id == 0) {
                            print_usage(argv);
                            eprintf("argument of '-%c' cannot be zero.\n", c);
                        }
                        return EXIT_FAILURE;
                    }
                    axis_length = optval_double;
                }
                break;
            case 'o': /* output */
                opts->filename = optarg;
                break;
            case 's': /* progress */
                opts->show_progress = 1;
                break;
            case 'h': /* help */
                if (proc_id == 0) {
                    print_usage(argv);
                }
                free(opts);
                finalize_exit(EXIT_SUCCESS);
                break;
            case '?': /* unknown opt */
                if (proc_id == 0) {
                    /* hacky fix: get correct index if length of invalid option is > 2 */
                    index = (strncmp(argv[0], argv[optind - 1], sizeof(argv)) == 0) 
                        ? optind 
                        : optind-1;

                    print_usage(argv);
                    eprintf("invalid option '%s'.\n", argv[index]);
                }
                return EXIT_FAILURE;
                break;
            default:
                break;
        }
    }
    
    /* validate blocksize */
    if (opts->height % opts->blocksize != 0) {
        if (proc_id == 0) {
            print_usage(argv);
            eprintf("argument of '-b' has to be a divisor of %d.\n", opts->height);
        }
        return EXIT_FAILURE;
    }

    /* prevent too large blocksizes which will break the block distribution */
    if (opts->blocksize > opts->height/(proc_count - 1)) {
        if (proc_id == 0) {
            print_usage(argv);
            eprintf("argument of '-b' has to be smaller than %d.\n", opts->height/(proc_count-1));
        }
        return EXIT_FAILURE;
    }
    
    /* calculate problem space */
    opts->min_re = x_offset - axis_length;
    opts->max_re = x_offset + axis_length;
    opts->min_im = y_offset - axis_length;
    opts->max_im = y_offset + axis_length;

    /* summarize used options on master before starting computation */
    if (proc_id == 0) {
        if (argc < 2) {
            printf("Note: Program invoked with default options.\n" \
                "      Run '%s -h' for detailed information on available arguments.\n\n", argv[0]);
        }
        print_params(opts, x_offset, y_offset, axis_length);
    }

    return EXIT_SUCCESS;
}

/*
 * display parameters used for computation
 */
static void print_params(mo_opts_t *opts, double x_off, double y_off, double axis_length)
{
    printf("Computation parameters:\n" \
        "    output file              %s\n" \
        "    maximum iterations       %d\n" \
        "    blocksize                %d\n" \
        "    image width              %d\n" \
        "    image height             %d\n" \
        "    minimum color            0x%06lx\n" \
        "    maximum color            0x%06lx\n" \
        "    color mask               0x%06lx\n" \
        "    x-offset                 %g\n" \
        "    y-offset                 %g\n" \
        "    axis length              %g\n" \
        "    coordinate system range  [%g, %g]\n\n",
        opts->filename, opts->max_iterations, opts->blocksize, opts->width, opts->height, 
        opts->min_color, opts->max_color, opts->color_mask, x_off, y_off, axis_length, 
        opts->min_re, opts->max_re);
}

/*
 * display usage 
 */
static void print_usage(char **argv) 
{
    printf("\nDynamic MPI mandelbrot algorithm\n\n" \
        "usage: %s [options]\n\n" \
        "OPTIONS:\n" \
        "    -h                   Shows this help.\n" \
        "    -c {width}           Width of resulting image. Has to be positive integer.\n" \
        "                         (default: %d)\n" \
        "    -r {height}          Height of resulting image. Has to be positive integer.\n" \
        "                         (default: %d)\n" \
        "    -n {iterations}      Maximum number of iterations for each pixel. Has to be\n" \
        "                         positive integer (default: %d)\n" \
        "    -o {filename}        Filename of resulting bitmap. (default: %s)\n" \
        "    -b {blocksize}       Number of rows to be assigned to a slave at once.\n" \
        "                         Has to be smaller than (height/slave-count).\n" \
        "                         Has to be a divisor of height. (default: %d)\n" \
        "    -x {offset}          X-offset from [0,0]. (default: %g)\n" \
        "    -y {offset}          Y-offset from [0,0]. (default: %g)\n" \
        "    -a {length}          Absolute value range of x/y-axis, e.g. if length was 2, \n" \
        "                         displayed x/y-values would range from -1 to 1. \n" \
        "                         If the x/y-offsets are set, axis shifts by those offsets.\n" \
        "                         Negative value inverts axis.\n" \
        "                         Has to be non-zero double value. (default: %g)\n" \
        "    -p {hexnum}          Minimum color of the resulting image. (default: 0x%06lx)\n" \
        "    -q {hexnum}          Maximum color of the resulting image. (default: 0x%06lx)\n" \
        "    -m {hexnum}          Hex mask to manipulate color ranges. (default: 0x%06lx)\n" \
        "    -s                   Print progress of the computation.\n\n",
        argv[0], MO_SIZE, MO_SIZE, MO_MAXITER, MO_FILENAME, MO_BLOCKSIZE, 0.0f, 0.0f, 
        (double) MO_N, (long) MO_COLORMIN, (long) MO_COLORMAX, (long) MO_COLORMASK);
}

/*
 * master process logic
 */
static int master_proc(int slave_count, mo_opts_t *opts) 
{
    int *rows = (int *) malloc(opts->blocksize*sizeof(*rows));
    long *data = (long *) malloc((opts->width + 1)*opts->blocksize*sizeof(*data));
    char *rgb = (char *) malloc(3*opts->width*opts->height*sizeof(*rgb));

    if (rows == NULL || data == NULL || rgb == NULL) {
        eprintf("unable to allocate memory for buffers.\n");
        free(rows); free(data); free(rgb);
        return EXIT_FAILURE;
    }
    
    int proc_id, offset; 
    double start_time, end_time;
    long pixel_color, pixel_pos;
    int current_row = 0;
    int running_tasks = 0;
    int retval = EXIT_SUCCESS; 
    
    MPI_Status status;

    printf("Computation started.\n");
    
    /* get start time */
    start_time = MPI_Wtime();
    
    /* assign each slave initial row(s) */
    for (int p = 0; p < slave_count; ++p) {
        for (int i = 0; i < opts->blocksize; ++i) {
            rows[i] = current_row++;
        }
        MPI_Send(rows, opts->blocksize, MPI_INT, p + 1, MO_CALC, MPI_COMM_WORLD);
        ++running_tasks;
    }

    /* reveice results from slaves until all rows are processed */
    while (running_tasks > 0) {
        MPI_Recv(data, (opts->width + 1)*opts->blocksize, MPI_LONG, MPI_ANY_SOURCE,
                MO_DATA, MPI_COMM_WORLD, &status);

        --running_tasks;
        proc_id = status.MPI_SOURCE;

        /* if there are still rows to be processed, send slave to work again
         * otherwise send him to sleep */
        if (current_row < opts->height) {
            for (int i = 0; i < opts->blocksize; ++i) {
                rows[i] = current_row++;
            }
            MPI_Send(rows, opts->blocksize, MPI_INT, proc_id, MO_CALC, MPI_COMM_WORLD);
            ++running_tasks;
        } else {
            MPI_Send(NULL, 0, MPI_INT, proc_id, MO_STOP, MPI_COMM_WORLD);
        }

        /* store received row(s) in rgb buffer */
        for (int i = 0; i < opts->blocksize; ++i) {
            offset = opts->width*i;

            for (int col = 0; col < opts->width; ++col) {
                pixel_color = data[offset + col + 1] & opts->color_mask;
                pixel_pos = 3*(opts->width*data[offset] + col);

                rgb[pixel_pos] = (char) ((pixel_color >> 16) & 0xFF);
                rgb[pixel_pos + 1] = (char) ((pixel_color >> 8) & 0xFF);
                rgb[pixel_pos + 2] = (char) (pixel_color & 0xFF);
            }
        }

        /* only show progress if option set */
        if (opts->show_progress) {
            static int rows_processed = 0;
            print_progress(rows_processed += opts->blocksize, opts->height);
        }
    }

    /* get end time  */
    end_time = MPI_Wtime();

    /* clear progress bar from stdout */
    if (opts->show_progress) printf("\033[K");

    printf("Finished. Computation finished in %g sec.\n\n", end_time - start_time);

    /* write rgb data to file */
    printf("Creating bitmap image.\n");
    retval = write_bitmap(opts->filename, opts->width, opts->height, rgb);

    if (retval == EXIT_SUCCESS) {
        printf("Finished. Image stored in '%s'.\n", opts->filename);
    } else {
        eprintf("failed to write bitmap to file.\n");
    }
    
    free(rows);
    free(data);
    free(rgb);

    return retval;
}

/*
 * slave process logic
 */
static int slave_proc(int proc_id, mo_opts_t *opts) 
{
    int *rows = (int *) malloc(opts->blocksize*sizeof(*rows));
    long *data = (long *) malloc((opts->width + 1)*opts->blocksize*sizeof(*data));
    mo_scale_t *scale = (mo_scale_t *) malloc(sizeof(*scale));
    
    if (rows == NULL || data == NULL || scale == NULL) {
        free(rows); free(data); free(scale);
        return EXIT_FAILURE;
    }

    long pixel_color;
    int offset;
    
    MPI_Status status;
    
    /* compute factor for color scaling */
    scale->color = (double) (opts->max_color - opts->min_color) / 
        (double) (opts->max_iterations - 1);

    /* compute factors to scale computational region to imagesize */
    scale->re = (double) (opts->max_re - opts->min_re) / (double) opts->width;
    scale->im = (double) (opts->max_im - opts->min_im) / (double) opts->height;
    
    /* receive row(s) and start computation if status is MO_CALC */
    while ((MPI_Recv(rows, opts->blocksize, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD,
            &status) == MPI_SUCCESS) && status.MPI_TAG == MO_CALC) {
        for (int i = 0; i < opts->blocksize; ++i) {
            offset = opts->width*i;
            data[offset] = rows[i];

            /* compute pixel colors using mandelbrot algorithm */
            for (int col = 0; col < opts->width; ++col) {
                pixel_color = mandelbrot(col, rows[i], scale, opts);
                data[offset + col + 1] = pixel_color;
            }
        }

        /* send row(s) to master */
        MPI_Send(data, (opts->width + 1)*opts->blocksize, MPI_LONG, 0, MO_DATA, MPI_COMM_WORLD);
    }

    free(rows);
    free(data);
    free(scale);

    return EXIT_SUCCESS;
}

/*
 * compute pixel color using the mandelbrot algorithm
 */
static long mandelbrot(int col, int row, mo_scale_t *scale, mo_opts_t *opts) 
{
    mo_complex_t a, b;
    a.re = a.im = 0;

    /* scale display coordinates to actual region */
    b.re = opts->min_re + ((double) col*scale->re);
    b.im = opts->min_im + ((double) (opts->height - 1 - row)*scale->im);

    /* calculate z0, z1, until divergence or maximum iterations */
    int n = 0;
    double r2, tmp;

    do  {
        tmp = a.re*a.re - a.im*a.im + b.re;
        a.im = 2*a.re*a.im + b.im;
        a.re = tmp;
        r2 = a.re*a.re + a.im*a.im;
        ++n;
    } while (r2 < MO_THRESHOLD && n < opts->max_iterations);

    /* scale color and return */
    return (long) ((n - 1)*scale->color) + opts->min_color;
}

/*
 * print progress bar 
 */
static inline void print_progress(int rows_processed, int row_count)
{
    int r = row_count/MO_PUPDATE;
    
    /* only update MO_PUPDATE times */
    if (r == 0 || rows_processed % r != 0) return;

    /* calulate ratio and current position */
    float ratio = rows_processed/(float) row_count;
    int pos = ratio*MO_PWIDTH;
 
    /* print percentage and progress bar */
    printf("%3d%% [", (int) (ratio*100));
 
    for (int i = 0; i < pos; ++i) printf("=");
    for (int i = pos; i < MO_PWIDTH; ++i) printf(" ");
 
    /* carriage return to overwrite line on next progress update */
    printf("]\r");
}

/*
 * write bitmap file
 * derived from http://cpansearch.perl.org/src/DHUNT/PDL-Planet-0.05/libimage/bmp.c
 */
static int write_bitmap(const char *filename, int width, int height, char *rgb)
{
    int i, j, pixel_pos;
    int bytes_per_line;
    unsigned char *line;

    FILE *file;
    mo_bmp_header_t bmph;

    /* the length of each line must be a multiple of 4 bytes */
    bytes_per_line = (3*(width + 1)/4)*4;

    bmph.type[0] = 'B';
    bmph.type[1] = 'M';
    bmph.offbits = 54;
    bmph.fsize = bmph.offbits + bytes_per_line*height;
    bmph.reserved = 0;
    bmph.hsize = 40;
    bmph.width = width;
    bmph.height = height;
    bmph.planes = 1;
    bmph.bit_count = 24;
    bmph.compression = 0;
    bmph.size_image = bytes_per_line*height;
    bmph.x_pels_per_meter = 0;
    bmph.y_pels_per_meter = 0;
    bmph.clr_used = 0;       
    bmph.clr_important = 0; 

    file = fopen(filename, "wb");

    if (file == NULL) { 
        eprintf("unable to open file '%s'.\n", filename);
        return EXIT_FAILURE;
    }
   
    /* write header */
    fwrite(&bmph.type, 2, 1, file);
    fwrite(&bmph.fsize, 4, 1, file);
    fwrite(&bmph.reserved, 4, 1, file);
    fwrite(&bmph.offbits, 4, 1, file);
    fwrite(&bmph.hsize, 4, 1, file);
    fwrite(&bmph.width, 4, 1, file);
    fwrite(&bmph.height, 4, 1, file);
    fwrite(&bmph.planes, 2, 1, file);
    fwrite(&bmph.bit_count, 2, 1, file);
    fwrite(&bmph.compression, 4, 1, file);
    fwrite(&bmph.size_image, 4, 1, file);
    fwrite(&bmph.x_pels_per_meter, 4, 1, file);
    fwrite(&bmph.y_pels_per_meter, 4, 1, file);
    fwrite(&bmph.clr_used, 4, 1, file);
    fwrite(&bmph.clr_important, 4, 1, file);
  
    line = (unsigned char *) malloc(bytes_per_line*sizeof(*line));

    if (line == NULL) {
        eprintf("unable to allocate memory for line buffer.\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    /* write rgb data */
    for (i = height - 1; i >= 0; i--) {
        for (j = 0; j < width; j++) {
            pixel_pos = 3*(width*i + j);
            line[3*j] = rgb[pixel_pos + 2];
            line[3*j + 1] = rgb[pixel_pos + 1];
            line[3*j + 2] = rgb[pixel_pos];
        }
        fwrite(line, bytes_per_line, 1, file);
    }

    free(line);
    fclose(file);

    return EXIT_SUCCESS;
}
