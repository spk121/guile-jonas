/* srfi-1.c --- SRFI-1 procedures for Guile

   Copyright 1995-1997,2000-2003,2005-2006,2008-2011,2013-2014,2018,2020
     Free Software Foundation, Inc.

   This file is part of Guile.

   Guile is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Guile is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Guile.  If not, see
   <https://www.gnu.org/licenses/>.  */




#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdarg.h>

#include "boolean.h"
#include "eq.h"
#include "eval.h"
#include "extensions.h"
#include "gsubr.h"
#include "list.h"
#include "pairs.h"
#include "procs.h"
#include "values.h"
#include "vectors.h"
#include "version.h"

#include "srfi-1.h"


/* The intent of this file was to gradually replace those Scheme
 * procedures in srfi-1.scm that extend core primitive procedures,
 * so that using srfi-1 wouldn't have performance penalties.
 *
 * However, we now prefer to write these procedures in Scheme, let the compiler
 * optimize them, and have the VM execute them efficiently.
 */


void
scm_register_srfi_1 (void)
{
  scm_c_register_extension ("libguile-" SCM_EFFECTIVE_VERSION,
                            "scm_init_srfi_1",
                            (scm_t_extension_init_func)scm_init_srfi_1, NULL);
}

void
scm_init_srfi_1 (void)
{
#ifndef SCM_MAGIC_SNARFER
#include "srfi-1.x"
#endif
}

/* End of srfi-1.c.  */
