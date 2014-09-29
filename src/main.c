/**
 * Copyright (C) 2012, 2013, 2014 Universidad Simón Bolívar
 *
 * Copying: GNU GENERAL PUBLIC LICENSE Version 2
 * @author Guillermo Palma <gpalma@ldc.usb.ve>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "types.h"
#include "memory.h"
#include "graph.h"
#include "util.h"
#include "input.h"
#include "tax_sim.h"

#define MIN_ARG      3
#define MAX_THREADS  128

struct global_args {
     char *graph_filename;
     char *desc_filename;
     char *annt_filename;
     unsigned n_threads;
     enum metric d;
     bool description;
     bool lca; 
};

static struct global_args g_args;
static const char *optString = "ldm:t:";

/*********************************
 **  Parse Arguments
 *********************************/

static void display_usage(void)
{
     fatal("Incorrect arguments \n\ttaxsim [-m tax|str|ps] [-t <number of threads>] [-d] [-l] <graph> <terms> <annotations>\n");
}

static void initialize_arguments(void)
{
     g_args.graph_filename = NULL;
     g_args.desc_filename = NULL;
     g_args.annt_filename = NULL;
     g_args.d = DTAX;
     g_args.n_threads = 1;
     g_args.description = false;
     g_args.lca = false;
}

static void print_args(void)
{
     printf("\n*********************\n");
     printf("Parameters:\n");
     if (g_args.d == DTAX) {
	  printf("Metric: d_tax\n");
     } else if (g_args.d == DSTR) {
	  printf("Metric: d^str_tax\n");
     } else if (g_args.d == DPS) {
	  printf("Metric: d_ps\n");
     } else {
	  fatal("Unknown metric");
     }
     printf("Graph: %s\n", g_args.graph_filename);
     printf("Terms description: %s\n", g_args.desc_filename);
     printf("Annotations: %s\n", g_args.annt_filename);
     printf("Number of Threads: %d\n", g_args.n_threads);
     printf("*********************\n");
}

static void parse_args(int argc, char **argv)
{
     int i, opt;

     opt = 0;
     initialize_arguments();
     opt = getopt(argc, argv, optString);
     while(opt != -1) {
	  switch(opt) {
	  case 'l':
	       g_args.lca = true;
	       break;
	  case 'd':
	       g_args.description = true;
	       break;
	  case 'm':
	       if (strcmp(optarg, "tax") == 0) {
		    g_args.d = DTAX;
	       } else if (strcmp(optarg, "str") == 0) {
		    g_args.d = DSTR;
	       } else if (strcmp(optarg, "ps") == 0) {
		    g_args.d = DPS;
	       } else {
		    display_usage();
	       }
	       break;
	  case 't':
	       g_args.n_threads = strtod(optarg, (char **)NULL);
	       if (0 >= g_args.n_threads)
		    fatal("Error, The minimum number of threads allowed is 1");

	       if (MAX_THREADS < g_args.n_threads)
		    fatal("Error, The maximum number of threads allowed is %d", MAX_THREADS);
	       break;
	  case '?':
	       display_usage();
	       break;
	  default:
	       /* You won't actually get here. */
	       fatal("?? getopt returned character code 0%o ??\n", opt);
	  }
	  opt = getopt(argc, argv, optString);
     }
     if ((argc - optind) != MIN_ARG)
	  display_usage();
     i = optind;
     g_args.graph_filename = argv[i++];
     g_args.desc_filename = argv[i++];
     g_args.annt_filename = argv[i];
}

/*********************************
 *********************************
 **
 **       Main section
 **
 *********************************
 **********************************/

int main(int argc, char **argv)
{
     clock_t ti, tf;
     struct input_data in;

     ti = clock();     
     parse_args(argc, argv);
     print_args();

     /* start solver */
     printf("\n**** Solver Begins ****\n");
     in = get_input_ontology_data(g_args.graph_filename,
				  g_args.desc_filename,
				  g_args.annt_filename, 
				  g_args.description);
     taxonomic_similarity(&in.g, &in.anntt,
			  g_args.n_threads,
			  in.descriptions,
			  g_args.d, g_args.lca);
     tf = clock();
     free_input_data(&in);
     printf("\nTotal Time %.3f secs\n", (double)(tf-ti)/CLOCKS_PER_SEC);
     
     return 0;
}
