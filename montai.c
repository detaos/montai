/* to compile: gcc -Wall -O3 -lm -lpthread -o montai montai.c */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int verbose     = 0;        /* default to non-verbose output, ie no output while rendering */
int num_threads = 7;        /* default to 7 threads */
int work_units  = 4;        /* default to 4 work units per thread */
int next_unit   = 0;        /* next work unit to be rendered */
int num_units;              /* number of threads * work units */
int svg_unit_height;        /* the height of a single unit from the SVG being rendered */
int svg_final_unit_height;  /* the height of the last unit from the SVG being rendered */
int png_unit_height;        /* the height of a single unit for the PNG being rendered */
int png_final_unit_height;  /* the height of the last unit for the PNG being rendered */
int svg_x = 0, svg_y = 0;   /* default to rendering from the origin */
int svg_width, svg_height;  /* svg dimensions to be read from inkscape OR from cli */
int png_width, png_height;  /* png export dimensions */
const char *program_name = NULL;
const char *svg_path = NULL;
const char *png_path = "output.png";

void print_usage(FILE *stream, int exit_code);
void *render();

int main(int argc, char * argv[]) {
	program_name = argv[0];
	if (argc < 2) print_usage(stderr, EXIT_FAILURE);
	int next_option;
	const char *short_options = "ht:w:d:e:v";
	const struct option long_options[] = {
		{ "help",       0, NULL, 'h' },
		{ "threads",    1, NULL, 't' },
		{ "work_units", 1, NULL, 'w' },
		{ "dimensions", 1, NULL, 'd' },
		{ "eport_size", 1, NULL, 'e' },
		{ "verbose",    0, NULL, 'v' },
		{ NULL,         0, NULL,  0  }
	};
	const char *import_dims = NULL, *export_dims = NULL;
	char cmd[2048];
	do {
		next_option = getopt_long (argc, argv, short_options, long_options, NULL);
		switch (next_option) {
			case 'h': /* -h or --help */
				print_usage(stdout, EXIT_SUCCESS);
				break;
			case 't': { /* -t or --threads, parameter format: %i */
				int arg = atoi(optarg);
				num_threads = arg > 0 ? arg : num_threads; /* number of threads must be greater than zero */
				break;
			}
			case 'w': { /* -w or --work_units, parameter format: %i */
				int arg = atoi(optarg);
				work_units = arg > 0 ? arg : work_units; /* number of work units must be greater than zero */
				break;
			}
			case 'd': { /* -d or --dimensions, parameter format: x0:y0:x1:y1 */
				import_dims = optarg;
				break;
			}
			case 'e': { /* -e or --eport_size, parameter format: w:h */
				export_dims = optarg;
				break;
			}
			case 'v':
				verbose = 1;
				break;
			case '?': /* unkown option given */
				print_usage(stderr, EXIT_FAILURE);
				break;
			case -1: /* no more options to parse */
				break;
			default: /* how did we get here? */
				abort();
		}
	} while (next_option != -1);
	
	if (optind == argc) {
		print_usage(stderr, EXIT_FAILURE);
	} else {
		if (argc - optind == 2) { /* input and output file names specified */
			svg_path = argv[optind];
			png_path = argv[optind + 1];
		} else {                  /* input file name speficied only */
			svg_path = argv[optind];
		}
	}
	
	num_units = num_threads * work_units;
	
	if (import_dims) {
		sscanf(import_dims, "%d:%d:%d:%d", &svg_x, &svg_y, &svg_width, &svg_height);
	} else {           /* query svg width and height from inkscape */
		sprintf(cmd, "inkscape --without-gui --query-width %s > .MONTAI_SVG_WIDTH", svg_path);
		system(cmd);
		FILE *reader = fopen(".MONTAI_SVG_WIDTH", "r");
		float fvalue; /* value read from file */
		int return_value = fscanf(reader, "%f", &fvalue);
		if (!return_value) {
			fprintf(stderr, "Failed to read SVG width.\n");
			exit(EXIT_FAILURE);
		}
		sprintf(cmd, "inkscape --without-gui --query-height %s > .MONTAI_SVG_HEIGHT", svg_path);
		system(cmd);
		svg_width = ceil(fvalue);
		reader = fopen(".MONTAI_SVG_HEIGHT", "r");
		return_value = fscanf(reader, "%f", &fvalue);
		if (!return_value) {
			fprintf(stderr, "Failed to read SVG height.\n");
			exit(EXIT_FAILURE);
		}
		svg_height = ceil(fvalue);
		remove(".MONTAI_SVG_WIDTH");
		remove(".MONTAI_SVG_HEIGHT");
	}
	
	svg_unit_height = (svg_height - svg_y) / num_units;
	svg_final_unit_height = (svg_height - svg_y) - (svg_unit_height * (num_units - 1));
	
	if (export_dims) {
		sscanf(import_dims, "%d:%d", &png_width, &png_height);
	} else {
		png_width = svg_width - svg_x;
		png_height = svg_height - svg_y;
	}
	
	png_unit_height = png_height / num_units;
	png_final_unit_height = png_height - (png_unit_height * (num_units - 1));
	
	if (verbose) {
		printf("Input SVG: %s\n", svg_path);
		printf("Export PNG: %s\n", png_path);
		printf("Threads: %d\n", num_threads);
		printf("Units per Thread: %d\n", work_units);
		printf("Total Work Units: %d\n", num_units);
		printf("SVG Dimensions: %d : %d : %d : %d\n", svg_x, svg_y, svg_width, svg_height);
		printf("SVG Unit Height: %d\n", svg_unit_height);
		printf("SVG Final Unit Height: %d\n", svg_final_unit_height);
		printf("SVG SUM(Unit Heights): %d\n", svg_unit_height * (num_units - 1) + svg_final_unit_height);
		printf("PNG Size: %d x %d\n", png_width, png_height);
		printf("PNG Unit Height: %d\n", png_unit_height);
		printf("PNG Final Unit Height: %d\n", png_final_unit_height);
		printf("PNG SUM(Unit Heights): %d\n", png_unit_height * (num_units - 1) + png_final_unit_height);
	}
	
	int i;
	pthread_t * threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));
	
	for (i = 0; i < num_threads; ++i)
		if (pthread_create(&threads[i], NULL, render, NULL) != 0)
			return EXIT_FAILURE;
	
	for (i = 0; i < num_threads; ++i)
		if (pthread_join(threads[i], NULL) != 0)
			return EXIT_FAILURE;
	
	free(threads);
	
	sprintf(cmd, "perl stitch_pngs.pl -o %s -n %d", png_path, num_units);
	system(cmd);
	sprintf(cmd, "rm %s.*", png_path);
	system(cmd);
	
	return EXIT_SUCCESS;
}

void print_usage(FILE *stream, int exit_code) {
	fprintf (stream, "Usage: %s options [ inputfile outputfile ]\n", program_name);
	fprintf (stream,
			 "  -h, --help                    Display this usage information.\n"
			 "  -t, --threads=t               Number of rendering threads to use.\n"
			 "  -w, --work_units=w            Number of work units per thread.\n"
			 "  -d, --dimensions=x0:y0:x1:y1  SVG input dimensions.\n"
			 "  -e, --export_size=w:h         PNG export dimensions.\n"
			 "  -v, --verbose                 Print verbose messages.\n");
	exit (exit_code);		 
}

void *render() {
	int m_unit, m_svg_y0, m_svg_y1, m_png_unit_height;
	do {
		pthread_mutex_lock(&lock);
		if (next_unit < num_units)
			m_unit = next_unit++;
		else
			m_unit = -1;
		pthread_mutex_unlock(&lock);
		if (m_unit == -1)
			break;
		m_svg_y0 = m_unit * svg_unit_height;
		m_svg_y1 = m_svg_y0 + (m_unit == num_units - 1 ? svg_final_unit_height : svg_unit_height);
		m_png_unit_height = m_unit == num_units - 1 ? png_final_unit_height : png_unit_height;
		if (verbose)
			printf("rendering: SVG(%d:%d:%d:%d) -> PNG(y := %d to %d)\n", svg_x, m_svg_y0, svg_width, m_svg_y1, m_unit * png_unit_height, m_unit * png_unit_height + m_png_unit_height);
		char cmd[2048];
		sprintf(cmd, "inkscape --without-gui --export-png=%s.%d --export-area=%d:%d:%d:%d --export-width=%d --export-height=%d %s", png_path, m_unit, svg_x, m_svg_y0, svg_width, m_svg_y1, png_width, m_png_unit_height, svg_path);
		system(cmd);
	} while (m_unit != -1);
	return NULL;
}
