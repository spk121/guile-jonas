/* Copyright 1995-1998,2000-2001,2003-2004,2006,2009,2011,2013,2015,2018,2022,2023
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
#  include <config.h>
#endif

#include <string.h>
#include <unistr.h>

#include "alist.h"
#include "boolean.h"
#include "chars.h"
#include "eval.h"
#include "fluids.h"
#include "gsubr.h"
#include "hash.h"
#include "list.h"
#include "modules.h"
#include "numbers.h"
#include "pairs.h"
#include "private-options.h"
#include "read.h"
#include "smob.h"
#include "srfi-13.h"
#include "strings.h"
#include "strorder.h"
#include "threads.h"
#include "variable.h"
#include "vectors.h"
#include "weak-set.h"

#include "symbols.h"




static SCM symbols;

#ifdef GUILE_DEBUG
SCM_DEFINE (scm_sys_symbols, "%symbols", 0, 0, 0,
	    (),
	    "Return the system symbol obarray.")
#define FUNC_NAME s_scm_sys_symbols
{
  return symbols;
}
#undef FUNC_NAME
#endif



/* {Symbols}
 */

uintptr_t
scm_i_hash_symbol (SCM obj, uintptr_t n, void *closure)
{
  return scm_i_symbol_hash (obj) % n;
}

struct string_lookup_data
{
  SCM string;
  uintptr_t string_hash;
};

static int
string_lookup_predicate_fn (SCM sym, void *closure)
{
  struct string_lookup_data *data = closure;

  if (scm_i_symbol_hash (sym) == data->string_hash
      && scm_i_symbol_length (sym) == scm_i_string_length (data->string))
    {
      size_t n = scm_i_symbol_length (sym);
      while (n--)
        if (scm_i_symbol_ref (sym, n) != scm_i_string_ref (data->string, n))
          return 0;
      return 1;
    }
  else
    return 0;
}

static SCM
lookup_interned_symbol (SCM name, uintptr_t raw_hash)
{
  struct string_lookup_data data;

  data.string = name;
  data.string_hash = raw_hash;
  
  return scm_c_weak_set_lookup (symbols, raw_hash,
                                string_lookup_predicate_fn,
                                &data, SCM_BOOL_F);
}

struct latin1_lookup_data
{
  const char *str;
  size_t len;
  uintptr_t string_hash;
};

static int
latin1_lookup_predicate_fn (SCM sym, void *closure)
{
  struct latin1_lookup_data *data = closure;

  return scm_i_symbol_hash (sym) == data->string_hash
    && scm_i_is_narrow_symbol (sym)
    && scm_i_symbol_length (sym) == data->len
    && strncmp (scm_i_symbol_chars (sym), data->str, data->len) == 0;
}

static SCM
lookup_interned_latin1_symbol (const char *str, size_t len,
                               uintptr_t raw_hash)
{
  struct latin1_lookup_data data;

  data.str = str;
  data.len = len;
  data.string_hash = raw_hash;
  
  return scm_c_weak_set_lookup (symbols, raw_hash,
                                latin1_lookup_predicate_fn,
                                &data, SCM_BOOL_F);
}

struct utf8_lookup_data
{
  const char *str;
  size_t len;
  uintptr_t string_hash;
};

static int
utf8_string_equals_wide_string (const uint8_t *narrow, size_t nlen,
                                const scm_t_wchar *wide, size_t wlen)
{
  size_t byte_idx = 0, char_idx = 0;
  
  while (byte_idx < nlen && char_idx < wlen)
    {
      ucs4_t c;
      int nbytes;

      nbytes = u8_mbtoucr (&c, narrow + byte_idx, nlen - byte_idx);
      if (nbytes == 0)
        break;
      else if (nbytes < 0)
        /* Bad UTF-8.  */
        return 0;
      else if (c != wide[char_idx])
        return 0;

      byte_idx += nbytes;
      char_idx++;
    }

  return byte_idx == nlen && char_idx == wlen;
}

static int
utf8_lookup_predicate_fn (SCM sym, void *closure)
{
  struct utf8_lookup_data *data = closure;

  if (scm_i_symbol_hash (sym) != data->string_hash)
    return 0;
  
  if (scm_i_is_narrow_symbol (sym))
    return (scm_i_symbol_length (sym) == data->len
            && strncmp (scm_i_symbol_chars (sym), data->str, data->len) == 0);
  else
    return utf8_string_equals_wide_string ((const uint8_t *) data->str,
                                           data->len,
                                           scm_i_symbol_wide_chars (sym),
                                           scm_i_symbol_length (sym));
}

static SCM
lookup_interned_utf8_symbol (const char *str, size_t len,
                             uintptr_t raw_hash)
{
  struct utf8_lookup_data data;

  data.str = str;
  data.len = len;
  data.string_hash = raw_hash;
  
  return scm_c_weak_set_lookup (symbols, raw_hash,
                                utf8_lookup_predicate_fn,
                                &data, SCM_BOOL_F);
}

static int
symbol_lookup_predicate_fn (SCM sym, void *closure)
{
  SCM other = SCM_PACK_POINTER (closure);

  if (scm_i_symbol_hash (sym) == scm_i_symbol_hash (other)
      && scm_i_symbol_length (sym) == scm_i_symbol_length (other))
    {
      if (scm_i_is_narrow_symbol (sym))
        return scm_i_is_narrow_symbol (other)
          && (strncmp (scm_i_symbol_chars (sym),
                       scm_i_symbol_chars (other),
                       scm_i_symbol_length (other)) == 0);
      else
        return scm_is_true
          (scm_string_equal_p (scm_symbol_to_string (sym),
                               scm_symbol_to_string (other)));
    }
  return 0;
}
 
static SCM
scm_i_str2symbol (SCM str)
{
  SCM symbol;
  uintptr_t raw_hash = scm_i_string_hash (str);

  symbol = lookup_interned_symbol (str, raw_hash);
  if (scm_is_true (symbol))
    return symbol;
  else
    {
      /* The symbol was not found, create it.  */
      symbol = scm_i_make_symbol (str, 0, raw_hash);

      /* Might return a different symbol, if another one was interned at
         the same time.  */
      return scm_c_weak_set_add_x (symbols, raw_hash,
                                   symbol_lookup_predicate_fn,
                                   SCM_UNPACK_POINTER (symbol), symbol);
    }
}


static SCM
scm_i_str2uninterned_symbol (SCM str)
{
  uintptr_t raw_hash = scm_i_string_hash (str);

  return scm_i_make_symbol (str, SCM_I_F_SYMBOL_UNINTERNED, raw_hash);
}

SCM_DEFINE (scm_symbol_p, "symbol?", 1, 0, 0, 
	    (SCM obj),
	    "Return @code{#t} if @var{obj} is a symbol, otherwise return\n"
	    "@code{#f}.")
#define FUNC_NAME s_scm_symbol_p
{
  return scm_from_bool (scm_is_symbol (obj));
}
#undef FUNC_NAME

SCM_DEFINE (scm_symbol_interned_p, "symbol-interned?", 1, 0, 0, 
	    (SCM symbol),
	    "Return @code{#t} if @var{symbol} is interned, otherwise return\n"
	    "@code{#f}.")
#define FUNC_NAME s_scm_symbol_interned_p
{
  SCM_VALIDATE_SYMBOL (1, symbol);
  return scm_from_bool (scm_i_symbol_is_interned (symbol));
}
#undef FUNC_NAME

SCM_DEFINE (scm_make_symbol, "make-symbol", 1, 0, 0,
	    (SCM name),
	    "Return a new uninterned symbol with the name @var{name}.  " 
	    "The returned symbol is guaranteed to be unique and future "
	    "calls to @code{string->symbol} will not return it.")
#define FUNC_NAME s_scm_make_symbol
{
  SCM_VALIDATE_STRING (1, name);
  return scm_i_str2uninterned_symbol (name);
}
#undef FUNC_NAME

SCM_DEFINE (scm_symbol_to_string, "symbol->string", 1, 0, 0, 
           (SCM s),
	    "Return the name of @var{symbol} as a string.  The resulting\n"
            "string is immutable.")
#define FUNC_NAME s_scm_symbol_to_string
{
  SCM_VALIDATE_SYMBOL (1, s);
  return scm_i_symbol_substring (s, 0, scm_i_symbol_length (s));
}
#undef FUNC_NAME


SCM_DEFINE (scm_string_to_symbol, "string->symbol", 1, 0, 0, 
	    (SCM string),
	    "Return the symbol whose name is @var{string}.")
#define FUNC_NAME s_scm_string_to_symbol
{
  SCM_VALIDATE_STRING (1, string);
  return scm_i_str2symbol (string);
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_ci_to_symbol, "string-ci->symbol", 1, 0, 0,
	    (SCM str),
	    "Return the symbol whose name is @var{str}.  @var{str} is\n"
	    "converted to lowercase before the conversion is done, if Guile\n"
	    "is currently reading symbols case-insensitively.")
#define FUNC_NAME s_scm_string_ci_to_symbol
{
  return scm_string_to_symbol (SCM_CASE_INSENSITIVE_P
			       ? scm_string_downcase(str)
			       : str);
}
#undef FUNC_NAME

/* The default prefix for `gensym'd symbols.  */
static SCM default_gensym_prefix;

#define MAX_PREFIX_LENGTH 30

SCM_DEFINE (scm_gensym, "gensym", 0, 1, 0,
            (SCM prefix),
	    "Create a new symbol with a name constructed from a prefix and\n"
	    "a counter value. The string @var{prefix} can be specified as\n"
	    "an optional argument. Default prefix is @code{ g}.  The counter\n"
	    "is increased by 1 at each call. There is no provision for\n"
	    "resetting the counter.")
#define FUNC_NAME s_scm_gensym
{
  static int gensym_counter = 0;
  
  SCM suffix, name;
  int n, n_digits;
  char buf[SCM_INTBUFLEN];

  if (SCM_UNBNDP (prefix))
    prefix = default_gensym_prefix;

  /* mutex in case another thread looks and incs at the exact same moment */
  scm_i_scm_pthread_mutex_lock (&scm_i_misc_mutex);
  n = gensym_counter++;
  scm_i_pthread_mutex_unlock (&scm_i_misc_mutex);

  n_digits = scm_iint2str (n, 10, buf);
  suffix = scm_from_latin1_stringn (buf, n_digits);
  name = scm_string_append (scm_list_2 (prefix, suffix));
  return scm_string_to_symbol (name);
}
#undef FUNC_NAME

SCM_DEFINE (scm_symbol_hash, "symbol-hash", 1, 0, 0, 
	    (SCM symbol),
	    "Return a hash value for @var{symbol}.")
#define FUNC_NAME s_scm_symbol_hash
{
  SCM_VALIDATE_SYMBOL (1, symbol);
  return scm_from_ulong (scm_i_symbol_hash (symbol));
}
#undef FUNC_NAME

SCM
scm_from_locale_symbol (const char *sym)
{
  return scm_from_locale_symboln (sym, -1);
}

SCM
scm_from_locale_symboln (const char *sym, size_t len)
{
  SCM str = scm_from_locale_stringn (sym, len);
  return scm_i_str2symbol (str);
}

SCM
scm_take_locale_symboln (char *sym, size_t len)
{
  SCM str;

  str = scm_take_locale_stringn (sym, len);
  return scm_i_str2symbol (str);
}

SCM
scm_take_locale_symbol (char *sym)
{
  return scm_take_locale_symboln (sym, (size_t)-1);
}

SCM
scm_from_latin1_symbol (const char *sym)
{
  return scm_from_latin1_symboln (sym, -1);
}

SCM
scm_from_latin1_symboln (const char *sym, size_t len)
{
  uintptr_t hash;
  SCM ret;

  if (len == (size_t) -1)
    len = strlen (sym);
  hash = scm_i_latin1_string_hash (sym, len);

  ret = lookup_interned_latin1_symbol (sym, len, hash);
  if (scm_is_false (ret))
    {
      SCM str = scm_from_latin1_stringn (sym, len);
      ret = scm_i_str2symbol (str);
    }

  return ret;
}

SCM
scm_from_utf8_symbol (const char *sym)
{
  return scm_from_utf8_symboln (sym, -1);
}

SCM
scm_from_utf8_symboln (const char *sym, size_t len)
{
  uintptr_t hash;
  SCM ret;

  if (len == (size_t) -1)
    len = strlen (sym);
  hash = scm_i_utf8_string_hash (sym, len);

  ret = lookup_interned_utf8_symbol (sym, len, hash);
  if (scm_is_false (ret))
    {
      SCM str = scm_from_utf8_stringn (sym, len);
      ret = scm_i_str2symbol (str);
    }

  return ret;
}

void
scm_symbols_prehistory ()
{
  symbols = scm_c_make_weak_set (5000);
}


void
scm_init_symbols ()
{
#include "symbols.x"

  default_gensym_prefix = scm_from_latin1_string (" g");
}
