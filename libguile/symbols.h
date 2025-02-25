#ifndef SCM_SYMBOLS_H
#define SCM_SYMBOLS_H

/* Copyright 1995-1998,2000-2001,2003-2004,2006,2008,2010-2011,2018,2022
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



#include <libguile/error.h>
#include <libguile/gc.h>
#include <libguile/snarf.h>
#include <libguile/strings.h>




#define scm_is_symbol(x)            (SCM_HAS_TYP7 (x, scm_tc7_symbol))
#define scm_i_symbol_hash(x)        ((uintptr_t) SCM_CELL_WORD_2 (x))
#define scm_i_symbol_is_interned(x) \
  (!(SCM_CELL_WORD_0 (x) & SCM_I_F_SYMBOL_UNINTERNED))

#define SCM_I_F_SYMBOL_UNINTERNED   0x100

#define SCM_VALIDATE_SYMBOL(pos, str) \
  do { \
    SCM_ASSERT_TYPE (scm_is_symbol (str), str, pos, FUNC_NAME, "symbol"); \
  } while (0)




#ifdef SCM_SUPPORT_STATIC_ALLOCATION

# define SCM_SYMBOL(c_name, scheme_name)				\
SCM_SNARF_HERE(								\
  SCM_IMMUTABLE_STRING (scm_i_paste (c_name, _string), scheme_name);	\
  static SCM c_name)							\
SCM_SNARF_INIT(								\
  c_name = scm_string_to_symbol (scm_i_paste (c_name, _string))		\
)

# define SCM_GLOBAL_SYMBOL(c_name, scheme_name)				\
SCM_SNARF_HERE(								\
  SCM_IMMUTABLE_STRING (scm_i_paste (c_name, _string), scheme_name);	\
  SCM c_name)								\
SCM_SNARF_INIT(								\
  c_name = scm_string_to_symbol (scm_i_paste (c_name, _string))		\
)

#else /* !SCM_SUPPORT_STATIC_ALLOCATION */

# define SCM_SYMBOL(c_name, scheme_name)				\
SCM_SNARF_HERE(static SCM c_name)					\
SCM_SNARF_INIT(c_name = scm_from_utf8_symbol (scheme_name))

# define SCM_GLOBAL_SYMBOL(c_name, scheme_name)				\
SCM_SNARF_HERE(SCM c_name)						\
SCM_SNARF_INIT(c_name = scm_from_utf8_symbol (scheme_name))

#endif /* !SCM_SUPPORT_STATIC_ALLOCATION */



/* Older spellings; don't use in new code.
 */
#define SCM_SYMBOLP(x)			(scm_is_symbol (x))
#define SCM_SYMBOL_HASH(x)		(scm_i_symbol_hash (x))
#define SCM_SYMBOL_INTERNED_P(x)	(scm_i_symbol_is_interned (x))



#ifdef GUILE_DEBUG
SCM_API SCM scm_sys_symbols (void);
#endif

SCM_API SCM scm_symbol_p (SCM x);
SCM_API SCM scm_symbol_interned_p (SCM sym);
SCM_API SCM scm_make_symbol (SCM name);
SCM_API SCM scm_symbol_to_string (SCM s);
SCM_API SCM scm_string_to_symbol (SCM s);
SCM_API SCM scm_string_ci_to_symbol (SCM s);

SCM_API SCM scm_symbol_hash (SCM s);
SCM_API SCM scm_gensym (SCM prefix);

/* Use locale encoding for user input, user output, or interacting with
   the C library.  Use latin-1 for ASCII, and for literals in source
   code.  Use UTF-8 for interaction with modern libraries which deal in
   UTF-8.  Otherwise use scm_to_stringn or scm_from_stringn, and
   convert.  */

SCM_API SCM scm_from_locale_symbol (const char *str);
SCM_API SCM scm_from_locale_symboln (const char *str, size_t len);
SCM_API SCM scm_take_locale_symbol (char *sym);
SCM_API SCM scm_take_locale_symboln (char *sym, size_t len);

SCM_API SCM scm_from_latin1_symbol (const char *str);
SCM_API SCM scm_from_latin1_symboln (const char *str, size_t len);
SCM_API SCM scm_take_latin1_symbol (char *sym);
SCM_API SCM scm_take_latin1_symboln (char *sym, size_t len);

SCM_API SCM scm_from_utf8_symbol (const char *str);
SCM_API SCM scm_from_utf8_symboln (const char *str, size_t len);
SCM_API SCM scm_take_utf8_symbol (char *sym);
SCM_API SCM scm_take_utf8_symboln (char *sym, size_t len);

/* internal functions. */

SCM_INTERNAL uintptr_t scm_i_hash_symbol (SCM obj, uintptr_t n,
                                          void *closure);

SCM_INTERNAL void scm_symbols_prehistory (void);
SCM_INTERNAL void scm_init_symbols (void);

#endif  /* SCM_SYMBOLS_H */
