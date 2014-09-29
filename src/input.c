/**
 * Copyright (C) 2013, 2014 Universidad Simón Bolívar
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

#include "types.h"
#include "graph.h"
#include "memory.h"
#include "hash_map.h"
#include "util.h"
#include "input.h"

#define HASH_SZ   503
#define BUFSZ     128
#define COST      1

struct arc {
  char *from;
  char *to;
  long cost;
};

struct graph_data {
  long n_nodes;
  long n_arcs;
  struct arc *larcs;
};

struct term {
  char *name;
  char *description;
};

struct term_data
{
  long nr;
  struct term *term_array;
};

struct concept {
  long pos;
  struct hash_entry entry;
};

struct char_array {
  unsigned nr;
  unsigned alloc;
  char *str;
};

struct string_array {
  unsigned nr;
  char **strs;
};

enum node {
  ROOT,
  NOROOT
};

struct node_type {
  enum node ntype;
  struct hash_entry entry;
};

/*********************************
 ** String processing
 *********************************/

static inline void add_char(struct char_array *buf, char ch)
{
  unsigned alloc, nr;

  alloc = buf->alloc;
  nr = buf->nr;
  if (nr == alloc) {
    alloc = BUFSZ + alloc;
    buf->str = xrealloc(buf->str, alloc);
    buf->alloc = alloc;
  }
  buf->str[nr] = ch;
  buf->nr++;
}

static inline void init_char_array(struct char_array *buf)
{
  buf->str = xcalloc(BUFSZ, 1);
  buf->alloc = BUFSZ;
  buf->nr = 0;
}

static inline void string_clean(struct char_array *buf)
{
  buf->nr = 0;
}

static void free_array_char(struct char_array *buf)
{
  buf->nr = 0;
  buf->alloc = 0;
  free(buf->str);
}

/*********************************
 ** Graph processing
 *********************************/

static void initialize_graph(struct graph_data *g, long n_nodes, long n_arcs)
{
  g->n_nodes = n_nodes;
  g->n_arcs = n_arcs;

  g->larcs = xcalloc(n_arcs, sizeof(struct arc));
}

static void free_graph_data(struct graph_data *g)
{
  long i;

  for (i = 0; i < g->n_arcs; i++) {
    free(g->larcs[i].from);
    free(g->larcs[i].to);
  }
  free(g->larcs);
  g->n_nodes = 0;
  g->n_arcs = 0;
}

static long graph_loading(struct graph_data *gd, const char *graph_filename)
{
  FILE *f;
  struct char_array buf;
  long cont_arcs, n, l;
  int ch, tok;
  size_t len;

  f = fopen(graph_filename, "r");
  if (!f) {
    fatal("No instance file specified, abort\n");
  }
  n = 0;
  l = 0;
  init_char_array(&buf);
  ch = getc(f);
  errno = 0;
  /* read number of nodes and arcs */
  while((ch != '\n') && (ch != EOF)) {
    if (ch == '\t') {
      add_char(&buf, '\0');
      n = strtol(buf.str, NULL, 10);
      if (errno)
        fatal("Error in the conversion of string to integer\n");
      string_clean(&buf);
    } else {
      add_char(&buf, ch);
    }
    ch = getc(f);
  }
  if (ch != EOF) {
    add_char(&buf, '\0');
    l = strtol(buf.str, NULL, 10);
    if (errno)
      fatal("Error in the conversion of string to integer\n");
  } else {
    fatal("Error reading the graph data file\n");
  }
  string_clean(&buf);
  initialize_graph(gd, n, l);
  /* read graphs arcs */
  ch = getc(f);
  if (ch == EOF) {
    fatal("Error reading the graph data file\n");
  }
  tok = 1;
  cont_arcs = 0;
  while ((ch != EOF) && (cont_arcs < l)) {
    if ((ch != '\t') && (ch != '\n')) {
      add_char(&buf, ch);
    } else {
      add_char(&buf, '\0');
      if (ch == '\t') {
        len = strlen(buf.str) + 1;
        assert(len > 1);
        if (tok == 1) {
          gd->larcs[cont_arcs].from = xmalloc(len);
          strcpy(gd->larcs[cont_arcs].from, buf.str);
        } else if (tok == 2) {
          gd->larcs[cont_arcs].to = xmalloc(len);
          strcpy(gd->larcs[cont_arcs].to, buf.str);
        } else {
          fatal("Error in graph format\n");
        }
        tok++;
      } else {
        assert(ch == '\n');
        if (tok != 3)
          fatal("Error in graph format\n");
        gd->larcs[cont_arcs].cost = strtol(buf.str, NULL, 10);
        if (errno)
          fatal("Error in the conversion of string to integer\n");
        cont_arcs++;
        tok = 1;
      }
      string_clean(&buf);
    }
    ch = getc(f);
  }
  fclose(f);
  free_array_char(&buf);
  if (cont_arcs < l)
    fatal("Incorrect number of arcs\n");
  return n;
}

#ifdef PRGDEBUG
static void print_graph_data(const struct graph_data *gd)
{
  long i;

  printf("Nodes %ld -- Arcs %ld\n", gd->n_nodes, gd->n_arcs);
  for (i = 0; i < gd->n_arcs; i++) {
    printf("%s\t%s\t%ld\n", gd->larcs[i].from, gd->larcs[i].to, gd->larcs[i].cost);
  }
}
#endif

/*********************************
 ** Ontology terms processing
 *********************************/

static void free_terms(struct term *t)
{
  free(t->name);
  free(t->description);
}

static void free_term_data(struct term_data *td)
{
  long i;

  for (i = 0; i < td->nr; i++)
    free_terms(&td->term_array[i]);
  td->nr = 0;
  free(td->term_array);
}

static void initialize_terms(struct term_data *td, long n)
{
  td->nr = n;
  td->term_array = xcalloc(n, sizeof(struct term));
}

static void load_of_terms(struct term_data *td, const char *desc_filename, 
			  bool description)
{
     FILE *f;
     struct char_array buf;
     long n, i;
     int ch, tok;
     size_t len;

     f = fopen(desc_filename, "r");
     if (!f) {
	  fatal("No instance file specified, abort\n");
     }
     n = 0;
     init_char_array(&buf);
     ch = getc(f);
     errno = 0;
     /* read number of terms */
     while((ch != '\n') && (ch != EOF)) {
	  add_char(&buf, ch);
	  ch = getc(f);
     }
     if (ch != EOF) {
	  add_char(&buf, '\0');
	  n = strtol(buf.str, NULL, 10);
	  if (errno)
	       fatal("Error in the conversion of string to integer\n");
     } else {
	  fatal("Error reading the description data file\n");
     }
     string_clean(&buf);
     initialize_terms(td, n);
     /* read the terms */
     ch = getc(f);
     if (ch == EOF) {
	  fatal("Error reading the description data file\n");
     }
     tok = 1;
     i = 0;
     while ((ch != EOF) && (i < n)) {
	  if ((ch != '\t') && (ch != '\n')) {
	       add_char(&buf, ch);
	  } else {
	       add_char(&buf, '\0');
	       if (ch == '\t') {
		    if (tok != 1)
			 fatal("Error in term file format\n");
		    len = strlen(buf.str) + 1;
		    assert(len > 1);
		    td->term_array[i].name = xmalloc(len);
		    strcpy(td->term_array[i].name, buf.str);
		    tok++;
		    if (!description) {
			 td->term_array[i].description = xmalloc(len);
			 strcpy(td->term_array[i].description, buf.str);
		    }
	       } else {
		    assert(ch == '\n');
		    if (tok != 2)
			 fatal("Error in term file format\n");
		    if (description) {
			 len = strlen(buf.str) + 1;
			 assert(len > 1);
			 td->term_array[i].description = xmalloc(len);
			 strcpy(td->term_array[i].description, buf.str);
		    }
		    i++;
		    tok = 1;
	       }
	       string_clean(&buf);
	  }
	  ch = getc(f);
     }
     fclose(f);
     free_array_char(&buf);
}

#ifdef PRGDEBUG
static void print_term_data(const struct term_data *td)
{
  long i;

  printf("\nNumber of terms %ld\n", td->nr);
  printf("Name\tDescriptions\n");
  for (i = 0; i < td->nr; i++) {
    printf("%s\t%s\n", td->term_array[i].name, td->term_array[i].description);
  }
}
#endif

static void initialize_string_array(struct string_array *sa, long n)
{
  sa->nr = n;
  sa->strs = xmalloc(n*sizeof(char *));
}

static void free_string_array(struct string_array *sa)
{
  for (long i = 0; i < sa->nr; i++)
    free(sa->strs[i]);
  free(sa->strs);
  sa->nr = 0;
}

static void annotations_load(struct string_array *sa, const char *annt_filename)
{
  FILE *f;
  char buf[128];
  size_t len;
  long i, n;

  f = fopen(annt_filename, "r");
  if (!f) {
    fatal("No instance file specified, abort\n");
  }
  i = 0;
  if (fgets(buf, sizeof(buf), f) == NULL)
    fatal("Error reading file");
  errno = 0;
  n = strtol(buf, NULL, 10);
  if (errno)
    fatal("Error in the conversion of string to integer\n");
  initialize_string_array(sa, n);
  for (i = 0; i < n; i++) {
    if (fgets(buf, sizeof(buf), f) == NULL)
      fatal("Error reading file");
    len = strlen(buf) - 1;
    if(buf[len] ==  '\n')
      buf[len] = '\0';
    sa->strs[i] = xcalloc(len+1, sizeof(char));
    strcpy(sa->strs[i], buf);
  }
  fclose(f);
}

#ifdef PRGDEBUG
static void print_string_array(struct string_array *sa)
{
  long i;

  printf("\nAnnotations\n");
  for (i = 0; i < sa->nr; i++)
    printf("%s\n", sa->strs[i]);
  printf("\n");
}
#endif

/********************************************************************
 ** Generation of the internal representation of the ontology graph
 ********************************************************************/

VEC(string) get_graph_roots(const struct graph_data *gd)
{
  long i;
  VEC(string) roots;
  struct hash_map root_set;
  struct node_type *item;
  struct hash_entry *hentry;
  struct hlist_node *n;
  size_t len;
  char *term;

  VEC_INIT(string, roots);
  hmap_create(&root_set, HASH_SZ);
  for (i = 0; i < gd->n_arcs; i++) {
    len = strlen(gd->larcs[i].from);
    hentry = hmap_find_member(&root_set, gd->larcs[i].from, len);
    if (hentry == NULL) {
      item = xmalloc(sizeof(struct node_type));
      item->ntype = ROOT;
      if (hmap_add(&root_set, &item->entry, gd->larcs[i].from, len) != 0)
        fatal("Error in the set of roots");
    }
    len = strlen(gd->larcs[i].to);
    hentry = hmap_find_member(&root_set, gd->larcs[i].to, len);
    if (hentry == NULL) {
      item = xmalloc(sizeof(struct node_type));
      item->ntype = NOROOT;
      if (hmap_add(&root_set, &item->entry, gd->larcs[i].to, len) != 0)
        fatal("Error in the set of roots");
    } else {
      item = hash_entry(hentry, struct node_type, entry);
      item->ntype = NOROOT;
    }
  }
  hmap_for_each_safe(hentry, n, &root_set) {
    item = hash_entry(hentry, struct node_type, entry);
    if (item->ntype == ROOT) {
      term = xmalloc(hentry->keylen+1);
      strcpy(term, hentry->key);
      VEC_PUSH(string, roots, term);
    }
    hmap_delete(&root_set, hentry);
    free(item);
  }
  hmap_destroy(&root_set);

  return roots;
}

#ifdef PRGDEBUG
static void print_ontology_roots(const VEC(string) *roots)
{
  unsigned i;
  char *term;

  printf("\nRoots of the ontology\n");
  for (i = 0; i < VEC_SIZE(*roots); i++) {
    term = VEC_GET(*roots, i);
    printf("%s\n", term);
  }
  printf("\n");
}
#endif

static void configure_the_single_root(struct graph_data *gd,
                                      struct term_data *td,
                                      const VEC(string) *roots)
{
  long i, j, alloc, nr;
  char *term, *tmp;
  const char *root_name = "ROOT";
  const char *root_desc = "Ontology Root";

  if (VEC_SIZE(*roots) == 1) {
    term = VEC_GET(*roots, 0);
    if (strcmp(term, td->term_array[0].name) != 0) {
      for (i = 0; i < td->nr; i++) {
        if (strcmp(term, td->term_array[i].name) == 0) {
          tmp = td->term_array[i].name;
          td->term_array[i].name = td->term_array[0].name;
          td->term_array[0].name = tmp;
          tmp = td->term_array[i].description;
          td->term_array[i].description = td->term_array[0].description;
          td->term_array[0].description = tmp;
          break;
        }
      }
    }
  } else {
    nr = td->nr;
    td->term_array = xrealloc(td->term_array, (nr+1)*sizeof(struct term));
    memmove(td->term_array+1, td->term_array, nr*sizeof(struct term));
    td->nr++;
    td->term_array[0].name = xmalloc(strlen(root_name)+1);
    td->term_array[0].description = xmalloc(strlen(root_desc)+1);
    strcpy(td->term_array[0].name, root_name);
    strcpy(td->term_array[0].description, root_desc);
    alloc = gd->n_arcs+VEC_SIZE(*roots);
    gd->larcs = xrealloc(gd->larcs, alloc*sizeof(struct arc));
    for (i = gd->n_arcs, j = 0; i < alloc; i++, j++) {
      gd->larcs[i].from = xmalloc(strlen(root_name)+1);
      strcpy(gd->larcs[i].from, root_name);
      term = VEC_GET(*roots, j);
      gd->larcs[i].to = xmalloc(strlen(term)+1);
      strcpy(gd->larcs[i].to, term);
      gd->larcs[i].cost = COST;
    }
    gd->n_arcs = alloc;
    gd->n_nodes++;
  }
}

#ifdef PRGDEBUG
static void print_hash_term(struct hash_map *term_pos)
{
  struct concept *item;
  struct hash_entry *hentry;

  printf("\nMap term-position\n");
  hmap_for_each(hentry, term_pos) {
    item = hash_entry(hentry, struct concept, entry);
    printf("** %s %ld\n", hentry->key, item->pos);
  }
}
#endif

static void free_map_term_pos(struct hash_map *term_pos)
{
  struct concept *item;
  struct hash_entry *hentry;
  struct hlist_node *n;

  hmap_for_each_safe(hentry, n, term_pos) {
    item = hash_entry(hentry, struct concept, entry);
    hmap_delete(term_pos, hentry);
    free(item);
  }
  hmap_destroy(term_pos);
}

static void map_term_pos(struct hash_map *term_pos, const struct term_data *td)
{
  long i, n;
  struct concept *item;
  size_t len;

  n = td->nr;
  hmap_create(term_pos, n*2);
  for (i = 0; i < n; i++) {
    len = strlen(td->term_array[i].name);
    item = xmalloc(sizeof(struct concept));
    item->pos = i;
    if (hmap_add_if_not_member(term_pos, &item->entry, td->term_array[i].name, len) != NULL)
      fatal("Error, term repeated in the file term-description\n");
  }
}

#ifdef PRGDEBUG
static void print_descriptions(char **desc, long n)
{
  long i;

  printf("\nDescriptions\n");
  for (i = 0; i < n; i++) {
    printf("%s\n", desc[i]);
  }
  printf("********************\n");
}
#endif

static char **get_descriptions(const struct term_data *td)
{
  long i, n;
  size_t len;
  char **desc;

  n = td->nr;
  desc = xcalloc(n, sizeof(char *));
  for (i = 0; i < n; i++) {
    len = strlen(td->term_array[i].description);
    desc[i] = xmalloc(len+1);
    strcpy(desc[i], td->term_array[i].description);
  }
  return desc;
}

#ifdef PRGDEBUG
static void print_annotations(const VEC(long) *annts)
{
  unsigned i;
  long term;

  printf("\nAnnotations\n");
  for (i = 0; i < VEC_SIZE(*annts); i++) {
    term = VEC_GET(*annts, i);
    printf("%ld\n", term);
  }
  printf("\n");
}
#endif

static VEC(long) get_annotations(struct string_array *sa,
                                 struct hash_map *term_pos)
{
  long i, n;
  VEC(long) annts;
  struct concept *item;
  struct hash_entry *hentry;
  size_t len;

  n = sa->nr;
  VEC_INIT_N(long, annts, n);
  for (i = 0; i < n; i++) {
    len = strlen(sa->strs[i]);
    hentry = hmap_find_member(term_pos, sa->strs[i], len);
    if (hentry == NULL)
      fatal("The term %s does not exist in the term list", sa->strs[i]);
    item = hash_entry(hentry, struct concept, entry);
    VEC_PUSH(long, annts, item->pos);
  }
  return annts;
}

static struct graph generate_internal_graph(const struct graph_data *gd,
                                            struct hash_map *term_pos)
{
  long i, from, to;
  struct graph g;
  struct concept *item;
  struct hash_entry *hentry;
  size_t len;

  init_graph(&g, gd->n_nodes);
  for (i = 0; i < gd->n_arcs; i++) {

    /* from node */
    len =  strlen(gd->larcs[i].from);
    hentry = hmap_find_member(term_pos, gd->larcs[i].from, len);
    if (hentry == NULL)
      fatal("Error, the term %s does not exist in the term list", gd->larcs[i].from);
    item = hash_entry(hentry, struct concept, entry);
    from = item->pos;

    /* to node */
    len =  strlen(gd->larcs[i].to);
    hentry = hmap_find_member(term_pos, gd->larcs[i].to, len);
    if (hentry == NULL)
      fatal("Error; the term %s does not exist in the term list", gd->larcs[i].to);
    item = hash_entry(hentry, struct concept, entry);
    to = item->pos;

    /* add to the graph */
    add_arc_to_graph(&g, i, from, to, gd->larcs[i].cost);
  }
  assert(g.n_edges == gd->n_arcs);
  assert(g.n_nodes == term_pos->fill);
  return g;
}

static void free_roots(VEC(string) *roots)
{
  unsigned i;

  for (i = 0; i < VEC_SIZE(*roots); i++)
     free(VEC_GET(*roots, i));
  VEC_DESTROY(*roots);
}

/*********************************
 ** Ontology Data
 *********************************/

void free_input_data(struct input_data *in)
{
  long i, n;

  n = in->g.n_nodes;
  free_graph(&in->g);
  for (i = 0; i < n; i++) {
    free(in->descriptions[i]);
  }
  free(in->descriptions);
  VEC_DESTROY(in->anntt);
}

struct input_data get_input_ontology_data(const char *graph_filename,
                                          const char *desc_filename,
                                          const char *annt_filename, 
					  bool description)
{
  long n_nodes;
  struct input_data in;
  struct graph_data gd;
  struct term_data td;
  struct string_array sa1;
  VEC(string) roots;
  struct hash_map term_pos;

  n_nodes = graph_loading(&gd, graph_filename);
  load_of_terms(&td, desc_filename, description);
  if (td.nr != n_nodes)
    fatal("Number of nodes of the graph is diferent to the number of terms");
  annotations_load(&sa1, annt_filename);
  roots = get_graph_roots(&gd);
  configure_the_single_root(&gd, &td, &roots);
  map_term_pos(&term_pos, &td);
  in.descriptions = get_descriptions(&td);
  in.anntt = get_annotations(&sa1, &term_pos);
  in.g = generate_internal_graph(&gd, &term_pos);
#ifdef PRGDEBUG
  print_graph_data(&gd);
  print_term_data(&td);
  print_string_array(&sa1);
  print_ontology_roots(&roots);
  print_term_data(&td);
  print_graph_data(&gd);
  print_descriptions(in.descriptions, td.nr);
  print_hash_term(&term_pos);
  print_annotations(&in.anntt1);
  print_annotations(&in.anntt2);
  print_graph(&in.g);
#endif
  free_roots(&roots);
  free_string_array(&sa1);
  free_graph_data(&gd);
  free_term_data(&td);
  free_map_term_pos(&term_pos);

  return in;
}
