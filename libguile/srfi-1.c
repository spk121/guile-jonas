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


static SCM
equal_trampoline (SCM proc, SCM arg1, SCM arg2)
{
  return scm_equal_p (arg1, arg2);
}

/* list_copy_part() copies the first COUNT cells of LST, puts the result at
   *dst, and returns the SCM_CDRLOC of the last cell in that new list.

   This function is designed to be careful about LST possibly having changed
   in between the caller deciding what to copy, and the copy actually being
   done here.  The COUNT ensures we terminate if LST has become circular,
   SCM_VALIDATE_CONS guards against a cdr in the list changed to some
   non-pair object.  */

#include <stdio.h>
static SCM *
list_copy_part (SCM lst, int count, SCM *dst)
#define FUNC_NAME "list_copy_part"
{
  SCM c;
  for ( ; count > 0; count--)
    {
      SCM_VALIDATE_CONS (SCM_ARGn, lst);
      c = scm_cons (SCM_CAR (lst), SCM_EOL);
      *dst = c;
      dst = SCM_CDRLOC (c);
      lst = SCM_CDR (lst);
    }
  return dst;
}
#undef FUNC_NAME

SCM_DEFINE (scm_srfi1_delete_duplicates, "delete-duplicates", 1, 1, 0,
	    (SCM lst, SCM pred),
	    "Return a list containing the elements of @var{lst} but without\n"
	    "duplicates.\n"
	    "\n"
	    "When elements are equal, only the first in @var{lst} is\n"
	    "retained.  Equal elements can be anywhere in @var{lst}, they\n"
	    "don't have to be adjacent.  The returned list will have the\n"
	    "retained elements in the same order as they were in @var{lst}.\n"
	    "\n"
	    "Equality is determined by @var{pred}, or @code{equal?} if not\n"
	    "given.  Calls @code{(pred x y)} are made with element @var{x}\n"
	    "being before @var{y} in @var{lst}.  A call is made at most once\n"
	    "for each combination, but the sequence of the calls across the\n"
	    "elements is unspecified.\n"
	    "\n"
	    "@var{lst} is not modified, but the return might share a common\n"
	    "tail with @var{lst}.\n"
	    "\n"
	    "In the worst case, this is an @math{O(N^2)} algorithm because\n"
	    "it must check each element against all those preceding it.  For\n"
	    "long lists it is more efficient to sort and then compare only\n"
	    "adjacent elements.")
#define FUNC_NAME s_scm_srfi1_delete_duplicates
{
  scm_t_trampoline_2 equal_p;
  SCM  ret, *p, keeplst, item, l;
  int  count, i;

  /* ret is the new list constructed.  p is where to append, initially &ret
     then SCM_CDRLOC of the last pair.  lst is advanced as each element is
     considered.

     Elements retained are not immediately appended to ret, instead keeplst
     is the last pair in lst which is to be kept but is not yet copied.
     Initially this is the first pair of lst, since the first element is
     always retained.

     *p is kept set to keeplst, so ret (inclusive) to lst (exclusive) is all
     the elements retained, making the equality search loop easy.

     If an item must be deleted, elements from keeplst (inclusive) to lst
     (exclusive) must be copied and appended to ret.  When there's no more
     deletions, *p is left set to keeplst, so ret shares structure with the
     original lst.  (ret will be the entire original lst if there are no
     deletions.)  */

  /* skip to end if an empty list (or something invalid) */
  ret = SCM_EOL;

  if (SCM_UNBNDP (pred))
    equal_p = equal_trampoline;
  else
    {
      SCM_VALIDATE_PROC (SCM_ARG2, pred);
      equal_p = scm_call_2;
    }

  keeplst = lst;
  count = 0;
  p = &ret;

  for ( ; scm_is_pair (lst); lst = SCM_CDR (lst))
    {
      item = SCM_CAR (lst);

      /* look for item in "ret" list */
      for (l = ret; scm_is_pair (l); l = SCM_CDR (l))
        {
          if (scm_is_true (equal_p (pred, SCM_CAR (l), item)))
            {
              /* "item" is a duplicate, so copy keeplst onto ret */
            duplicate:
              p = list_copy_part (keeplst, count, p);

              keeplst = SCM_CDR (lst);  /* elem after the one deleted */
              count = 0;
              goto next_elem;
            }
        }

      /* look for item in "keeplst" list
         be careful traversing, in case nasty code changed the cdrs */
      for (i = 0,       l = keeplst;
           i < count && scm_is_pair (l);
           i++,         l = SCM_CDR (l))
        if (scm_is_true (equal_p (pred, SCM_CAR (l), item)))
          goto duplicate;

      /* keep this element */
      count++;

    next_elem:
      ;
    }
  SCM_ASSERT_TYPE (SCM_NULL_OR_NIL_P (lst), lst, SCM_ARG1, FUNC_NAME, "list");

  /* share tail of keeplst items */
  *p = keeplst;

  return ret;
}
#undef FUNC_NAME


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
