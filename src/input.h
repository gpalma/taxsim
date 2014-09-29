/**
 * Copyright (C) 2013, 2014 Universidad Simón Bolívar
 *
 * Copying: GNU GENERAL PUBLIC LICENSE Version 2
 * @author Guillermo Palma <gpalma@ldc.usb.ve>
 */

#ifndef ___INPUT_H
#define ___INPUT_H

struct input_data {
  struct graph g;
  VEC(long) anntt;
  char **descriptions;
};

struct input_data get_input_ontology_data(const char *graph_filename,
                                          const char *desc_filename,
                                          const char *annt_filename, 
					  bool description);

void free_input_data(struct input_data *in);

#endif /* ___INPUT_H */
