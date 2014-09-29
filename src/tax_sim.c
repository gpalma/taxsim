/**
 * Copyright (C) 2013, 2014 Universidad Simón Bolívar
 *
 * Copying: GNU GENERAL PUBLIC LICENSE Version 2
 * @author Guillermo Palma <gpalma@ldc.usb.ve>
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <math.h>

#include "types.h"
#include "graph.h"
#include "memory.h"
#include "util.h"
#include "metric.h"
#include "tax_sim.h"

#define ROOT       0

struct args_metric {
     long start;
     long end;
     struct graph *gm;
};

struct terms_pair {
     long x;
     long y;
     double sim;
     VEC(long) *lca;
};

typedef struct terms_pair terms_pair_s;
DEFINE_VEC(terms_pair_s);

VEC(terms_pair_s) pterms;
static unsigned long n_pairs;
static double (*metricPtr)(const struct graph *g, long x, long y);;
static struct graph *gm;

static void get_pairs_of_terms(const VEC(long) *v)
{
     unsigned long i, j, n;
     terms_pair_s item;

     n = VEC_SIZE(*v);
     n_pairs = (n*(n+1))/2;
     VEC_INIT_N(terms_pair_s, pterms, n_pairs);
     for (i = 0; i < n; i++) {
	  for (j = i; j < n; j++) {
	       item.x = VEC_GET(*v, i);
	       item.y = VEC_GET(*v, j);
	       item.sim = 0.0;
	       item.lca = NULL;
	       VEC_PUSH(terms_pair_s, pterms, item);
	  }
     }
}

static void print_terms_pairs(char **descrptions, bool print_lca)
{
     unsigned long i, j;
     terms_pair_s item;

     if (print_lca) 
	  printf("\nTerm1\tTerm2\tSimilarity\tLCA\n\n");
     else
	  printf("\nTerm1\tTerm2\tSimilarity\n\n");

     for (i = 0; i < VEC_SIZE(pterms); i++) {
	  item = VEC_GET(pterms, i);
	  printf("%s\t%s\t%.5f", descrptions[item.x], descrptions[item.y], item.sim);
	  if (print_lca) {
	       for (j = 0; j < VEC_SIZE(*(item.lca)); j++) {
		    printf("\t%s\t", descrptions[VEC_GET(*(item.lca), j)]);
	       }
	  }
	  printf("\n");
     }
}

static void *calculate_similarity(void *args)
{
     long i, start, end;
     struct args_metric *am;
     terms_pair_s item;

     am = (struct args_metric *)args;
     start = am->start;
     end = am->end;
     for (i = start; i < end; i++) {
	  item = VEC_GET(pterms, i);
	  (VEC_GET(pterms, i)).sim = (*metricPtr)(gm, item.x, item.y);
     }
     return NULL;
}

static long get_max_group_depth(const VEC(long) *v1, char **desc)
{
     long max_depth = 0; /* ROOT depth */
     long max_node = ROOT;
     long i, n, node;
     const long *depth;

     depth = get_nodes_depth();
     n = VEC_SIZE(*v1);
     for (i = 0; i < n; i++) {
	  node = VEC_GET(*v1, i);
	  if (max_depth <  depth[node]) {
	       max_depth = depth[node];
	       max_node = node;
	  }
     }
     printf("The node deepest in the annotations is %s with depth %ld\n",
	    desc[max_node], max_depth);
     return max_depth;
}

static void calculate_lca(void)
{
  unsigned i;
  terms_pair_s item;

  for (i = 0; i < VEC_SIZE(pterms); i++) {
    item = VEC_GET(pterms, i);
    (VEC_GET(pterms, i)).lca = lca_vector(item.x, item.y);
  }
}

void taxonomic_similarity(struct graph *g, const VEC(long) *v, unsigned n_threads,
                          char **descrptions, enum metric d, bool print_lca)
{
     struct args_metric args[n_threads+1];
     pthread_t thread[n_threads];
     pthread_attr_t attr;
     long i, start, step, max_depth;
     int tc;

     gm = g;
     n_pairs = 0;
     get_pairs_of_terms(v);
     init_metric_data(g);

     if (print_lca)
	  calculate_lca();

     if (n_pairs < n_threads)
	  n_threads = n_pairs;

     if (d == DTAX) {
	  metricPtr = &sim_dtax;
     } else if (d == DPS) {
	  metricPtr = &sim_dps;
     } else {
	  max_depth = get_max_group_depth(v, descrptions);
	  set_max_depth(max_depth);
	  metricPtr = &sim_str;
     }
     /* Initialize and set thread detached attribute */
     pthread_attr_init(&attr);
     pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
     step = lrint(n_pairs/n_threads);
     for (i = 0; i < n_threads; i++) {
	  args[i].start = i*step;
	  args[i].end = (i+1)*step;
	  tc = pthread_create(&thread[i], &attr, calculate_similarity, (void *)(&args[i]));
	  if (tc)
	       fatal("ERROR; return code from pthread_create() is %d\n", tc);
     }
     start = n_threads*step;
     if ((unsigned)start < n_pairs) {
	  args[i].start = start;
	  args[i].end = n_pairs;
	  calculate_similarity((void *)(&args[i]));
     }
     /* Free attribute and wait for the other threads */
     pthread_attr_destroy(&attr);
     for(i = 0; i < n_threads; i++) {
	  tc = pthread_join(thread[i], NULL);
	  if (tc)
	       fatal("ERROR; return code from pthread_join() is %d\n", tc);
#ifdef PRGDEBUG
	  printf("Completed join with thread %ld\n",i );
#endif
     }
     print_terms_pairs(descrptions, print_lca);
     free_metric();
     VEC_DESTROY(pterms);
}
