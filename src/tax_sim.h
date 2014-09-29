/**
 * Copyright (C) 2013, 2014 Universidad Simón Bolívar
 *
 * Copying: GNU GENERAL PUBLIC LICENSE Version 2
 * @author Guillermo Palma <gpalma@ldc.usb.ve>
 */

#ifndef ___TAX_SIM_H
#define ___TAX_SIM_H

void taxonomic_similarity(struct graph *g, const VEC(long) *v,
                          unsigned n_threads, char **descrptions,
                          enum metric, bool print_lca);

#endif /* ___TAX_SIM_H */
