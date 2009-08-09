/* classes: h_files */

#ifndef SCM_CHARS_H
#define SCM_CHARS_H

/* Copyright (C) 1995,1996,2000,2001,2004, 2006, 2008, 2009 Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */



#include "libguile/__scm.h"
#include "libguile/numbers.h"


/* Immediate Characters
 */
#define SCM_CHARP(x) (SCM_ITAG8(x) == scm_tc8_char)
#define SCM_CHAR(x) ((scm_t_wchar)SCM_ITAG8_DATA(x))

#define SCM_MAKE_CHAR(x)                                              \
  ((x) < 0                                                            \
   ? SCM_MAKE_ITAG8 ((scm_t_bits) (unsigned char) (x), scm_tc8_char)  \
   : SCM_MAKE_ITAG8 ((scm_t_bits) (x), scm_tc8_char))

#define SCM_CODEPOINT_MAX (0x10ffff)
#define SCM_IS_UNICODE_CHAR(c)                                          \
  ((scm_t_wchar) (c) <= 0xd7ff                                          \
   || ((scm_t_wchar) (c) >= 0xe000 && (scm_t_wchar) (c) <= SCM_CODEPOINT_MAX))



SCM_API SCM scm_char_p (SCM x);
SCM_API SCM scm_char_eq_p (SCM x, SCM y);
SCM_API SCM scm_char_less_p (SCM x, SCM y);
SCM_API SCM scm_char_leq_p (SCM x, SCM y);
SCM_API SCM scm_char_gr_p (SCM x, SCM y);
SCM_API SCM scm_char_geq_p (SCM x, SCM y);
SCM_API SCM scm_char_ci_eq_p (SCM x, SCM y);
SCM_API SCM scm_char_ci_less_p (SCM x, SCM y);
SCM_API SCM scm_char_ci_leq_p (SCM x, SCM y);
SCM_API SCM scm_char_ci_gr_p (SCM x, SCM y);
SCM_API SCM scm_char_ci_geq_p (SCM x, SCM y);
SCM_API SCM scm_char_alphabetic_p (SCM chr);
SCM_API SCM scm_char_numeric_p (SCM chr);
SCM_API SCM scm_char_whitespace_p (SCM chr);
SCM_API SCM scm_char_upper_case_p (SCM chr);
SCM_API SCM scm_char_lower_case_p (SCM chr);
SCM_API SCM scm_char_is_both_p (SCM chr);
SCM_API SCM scm_char_to_integer (SCM chr);
SCM_API SCM scm_integer_to_char (SCM n);
SCM_API SCM scm_char_upcase (SCM chr);
SCM_API SCM scm_char_downcase (SCM chr);
SCM_API scm_t_wchar scm_c_upcase (scm_t_wchar c);
SCM_API scm_t_wchar scm_c_downcase (scm_t_wchar c);
SCM_INTERNAL const char *scm_i_charname (SCM chr);
SCM_INTERNAL SCM scm_i_charname_to_char (const char *charname, 
                                         size_t charname_len);
SCM_INTERNAL void scm_init_chars (void);

#endif  /* SCM_CHARS_H */

/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
