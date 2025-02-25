/* Copyright 1995-1996,1998,2000-2001,2004,2006,2008-2016,2018-2019
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

#include <alloca.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <uninorm.h>
#include <unistr.h>
#include <uniconv.h>
#include <c-strcase.h>
#include <intprops.h>

#include "chars.h"
#include "deprecation.h"
#include "error.h"
#include "generalized-vectors.h"
#include "gsubr.h"
#include "numbers.h"
#include "pairs.h"
#include "ports-internal.h"
#include "ports.h"
#include "private-options.h"
#include "striconveh.h"
#include "symbols.h"
#include "threads.h"

#include "strings.h"


#ifndef SCM_MAX_ALLOCA
# define SCM_MAX_ALLOCA 4096 /* Max bytes per string to allocate via alloca */
#endif



/* {Strings}
 */

SCM_SYMBOL (sym_UTF_8, "UTF-8");
SCM_SYMBOL (sym_ISO_8859_1, "ISO-8859-1");
SCM_SYMBOL (sym_error, "error");

/* A stringbuf is a linear buffer of characters.  Every string has a
   stringbuf.  Strings may reference just a slice of a stringbuf; that's
   often the case for strings made by the "substring" function.

   Stringbufs may hold either 8-bit characters or 32-bit characters.  In
   either case the characters are Unicode codepoints.  "Narrow"
   stringbufs thus have the ISO-8859-1 (Latin-1) encoding, and "wide"
   stringbufs have the UTF-32 (UCS-4) encoding.

   By default, stringbufs are immutable.  This enables an O(1)
   "substring" operation with no synchronization.  A string-set! will
   first ensure that the string's stringbuf is mutable, copying the
   stringbuf if necessary.  This is therefore a copy-on-write
   representation.  However, taking a substring of a mutable stringbuf
   is an O(n) operation as it has to create a new immutable stringbuf.
   There are also mutation-sharing substrings as well.  */

/* The size in words of the stringbuf header (type tag + size).  */
#define STRINGBUF_HEADER_SIZE   2U

#define STRINGBUF_HEADER_BYTES  (STRINGBUF_HEADER_SIZE * sizeof (SCM))

#define STRINGBUF_F_WIDE        SCM_I_STRINGBUF_F_WIDE
#define STRINGBUF_F_MUTABLE     SCM_I_STRINGBUF_F_MUTABLE

#define STRINGBUF_TAG           scm_tc7_stringbuf
#define STRINGBUF_WIDE(buf)     (SCM_CELL_WORD_0(buf) & STRINGBUF_F_WIDE)
#define STRINGBUF_MUTABLE(buf)  (SCM_CELL_WORD_0(buf) & STRINGBUF_F_MUTABLE)


#define STRINGBUF_SET_MUTABLE(buf) \
  SCM_SET_CELL_WORD_0 (buf, SCM_CELL_WORD_0 (buf) | STRINGBUF_F_MUTABLE)

#define STRINGBUF_CONTENTS(buf) ((void *)				\
                                 SCM_CELL_OBJECT_LOC (buf,		\
						      STRINGBUF_HEADER_SIZE))
#define STRINGBUF_CHARS(buf)    ((unsigned char *) STRINGBUF_CONTENTS (buf))
#define STRINGBUF_WIDE_CHARS(buf) ((scm_t_wchar *) STRINGBUF_CONTENTS (buf))

#define STRINGBUF_LENGTH(buf)   (SCM_CELL_WORD_1 (buf))

#ifdef SCM_STRING_LENGTH_HISTOGRAM
static size_t lenhist[1001];
#endif

/* Make a stringbuf with space for LEN 8-bit Latin-1-encoded
   characters. */
static SCM
make_stringbuf (size_t len)
{
  /* XXX - for the benefit of SCM_STRING_CHARS, SCM_SYMBOL_CHARS and
     scm_i_symbol_chars, all stringbufs are null-terminated.  Once
     SCM_STRING_CHARS and SCM_SYMBOL_CHARS are removed and the code
     has been changed for scm_i_symbol_chars, this null-termination
     can be dropped.
  */

  SCM buf;

#ifdef SCM_STRING_LENGTH_HISTOGRAM
  if (len < 1000)
    lenhist[len]++;
  else
    lenhist[1000]++;
#endif

  /* Make sure that the total allocation size will not overflow size_t,
     with ~30 extra bytes to spare to avoid an overflow within the
     allocator.  */
  if (INT_ADD_OVERFLOW (len, STRINGBUF_HEADER_BYTES + 32))
    scm_num_overflow ("make_stringbuf");

  buf = SCM_PACK_POINTER (scm_gc_malloc_pointerless (STRINGBUF_HEADER_BYTES + len + 1,
					    "string"));

  SCM_SET_CELL_TYPE (buf, STRINGBUF_TAG);
  SCM_SET_CELL_WORD_1 (buf, (scm_t_bits) len);

  STRINGBUF_CHARS (buf)[len] = 0;

  return buf;
}

/* Make a stringbuf with space for LEN 32-bit UCS-4-encoded
   characters.  */
static SCM
make_wide_stringbuf (size_t len)
{
  SCM buf;
  size_t raw_len;

#ifdef SCM_STRING_LENGTH_HISTOGRAM
  if (len < 1000)
    lenhist[len]++;
  else
    lenhist[1000]++;
#endif

  /* Make sure that the total allocation size will not overflow size_t,
     with ~30 extra bytes to spare to avoid an overflow within the
     allocator.  */
  if (len > (((size_t) -(STRINGBUF_HEADER_BYTES + 32 + sizeof (scm_t_wchar)))
             / sizeof (scm_t_wchar)))
    scm_num_overflow ("make_wide_stringbuf");

  raw_len = (len + 1) * sizeof (scm_t_wchar);
  buf = SCM_PACK_POINTER (scm_gc_malloc_pointerless (STRINGBUF_HEADER_BYTES + raw_len,
                                                     "string"));

  SCM_SET_CELL_TYPE (buf, STRINGBUF_TAG | STRINGBUF_F_WIDE);
  SCM_SET_CELL_WORD_1 (buf, (scm_t_bits) len);

  STRINGBUF_WIDE_CHARS (buf)[len] = 0;

  return buf;
}

/* Return a UCS-4-encoded stringbuf containing the (possibly Latin-1-encoded)
   characters from BUF.  */
static SCM
wide_stringbuf (SCM buf)
{
  SCM new_buf;

  if (STRINGBUF_WIDE (buf))
    new_buf = buf;
  else
    {
      size_t i, len;
      scm_t_wchar *mem;

      len = STRINGBUF_LENGTH (buf);

      new_buf = make_wide_stringbuf (len);

      mem = STRINGBUF_WIDE_CHARS (new_buf);
      for (i = 0; i < len; i++)
	mem[i] = (scm_t_wchar) STRINGBUF_CHARS (buf)[i];
      mem[len] = 0;
    }

  return new_buf;
}

/* Return a Latin-1-encoded stringbuf containing the (possibly UCS-4-encoded)
   characters from BUF, if possible.  */
static SCM
narrow_stringbuf (SCM buf)
{
  SCM new_buf;

  if (!STRINGBUF_WIDE (buf))
    new_buf = buf;
  else
    {
      size_t i, len;
      scm_t_wchar *wmem;
      unsigned char *mem;

      len = STRINGBUF_LENGTH (buf);
      wmem = STRINGBUF_WIDE_CHARS (buf);

      for (i = 0; i < len; i++)
	if (wmem[i] > 0xFF)
	  /* BUF cannot be narrowed.  */
	  return buf;

      new_buf = make_stringbuf (len);

      mem = STRINGBUF_CHARS (new_buf);
      for (i = 0; i < len; i++)
	mem[i] = (unsigned char) wmem[i];
      mem[len] = 0;
    }

  return new_buf;
}


/* Copy-on-write strings.
 */

#define STRING_TAG            scm_tc7_string

#define STRING_STRINGBUF(str) (SCM_CELL_OBJECT_1(str))
#define STRING_START(str)     ((size_t)SCM_CELL_WORD_2(str))
#define STRING_LENGTH(str)    ((size_t)SCM_CELL_WORD_3(str))

#define SET_STRING_STRINGBUF(str,buf) (SCM_SET_CELL_OBJECT_1(str,buf))
#define SET_STRING_START(str,start) (SCM_SET_CELL_WORD_2(str,start))

#define IS_STRING(str)        (SCM_HAS_TYP7 (str, STRING_TAG))

/* Read-only strings.
 */

#define RO_STRING_TAG         scm_tc7_ro_string
#define IS_RO_STRING(str)     (SCM_CELL_TYPE(str)==RO_STRING_TAG)

/* Mutation-sharing substrings
 */

#define SH_STRING_TAG       (scm_tc7_string + 0x100)

#define SH_STRING_STRING(sh) (SCM_CELL_OBJECT_1(sh))
/* START and LENGTH as for STRINGs. */

#define IS_SH_STRING(str)   (SCM_CELL_TYPE(str)==SH_STRING_TAG)

void
scm_i_print_stringbuf (SCM exp, SCM port, scm_print_state *pstate) 
{
  SCM str = scm_double_cell (STRING_TAG, SCM_UNPACK(exp), 0,
                             STRINGBUF_LENGTH (exp));
  scm_puts ("#<stringbuf ", port);
  scm_iprin1 (str, port, pstate);
  scm_puts (">", port);
}

SCM scm_nullstr;

static SCM null_stringbuf;

static void
init_null_stringbuf (void)
{
  null_stringbuf = make_stringbuf (0);
}

/* Create a scheme string with space for LEN 8-bit Latin-1-encoded
   characters.  CHARSP, if not NULL, will be set to location of the
   char array.  If READ_ONLY_P, the returned string is read-only;
   otherwise it is writable.  */
SCM
scm_i_make_string (size_t len, char **charsp, int read_only_p)
{
  SCM buf;
  SCM res;

  if (len == 0)
    {
      static scm_i_pthread_once_t once = SCM_I_PTHREAD_ONCE_INIT;
      scm_i_pthread_once (&once, init_null_stringbuf);
      buf = null_stringbuf;
    }
  else
    buf = make_stringbuf (len);

  if (charsp)
    *charsp = (char *) STRINGBUF_CHARS (buf);
  res = scm_double_cell (read_only_p ? RO_STRING_TAG : STRING_TAG,
			 SCM_UNPACK (buf),
			 (scm_t_bits) 0, (scm_t_bits) len);
  return res;
}

/* Create a scheme string with space for LEN 32-bit UCS-4-encoded
   characters.  CHARSP, if not NULL, will be set to location of the
   character array.  If READ_ONLY_P, the returned string is read-only;
   otherwise it is writable.  */
SCM
scm_i_make_wide_string (size_t len, scm_t_wchar **charsp, int read_only_p)
{
  SCM buf = make_wide_stringbuf (len);
  SCM res;
  if (charsp)
    *charsp = STRINGBUF_WIDE_CHARS (buf);
  res = scm_double_cell (read_only_p ? RO_STRING_TAG : STRING_TAG,
			 SCM_UNPACK (buf),
                         (scm_t_bits) 0, (scm_t_bits) len);
  return res;
}

static void
validate_substring_args (SCM str, size_t start, size_t end)
{
  if (!IS_STRING (str))
    scm_wrong_type_arg_msg (NULL, 0, str, "string");
  if (start > STRING_LENGTH (str))
    scm_out_of_range (NULL, scm_from_size_t (start));
  if (end > STRING_LENGTH (str) || end < start)
    scm_out_of_range (NULL, scm_from_size_t (end));
}

static inline void
get_str_buf_start (SCM *str, SCM *buf, size_t *start)
{
  *start = STRING_START (*str);
  if (IS_SH_STRING (*str))
    {
      *str = SH_STRING_STRING (*str);
      *start += STRING_START (*str);
    }
  *buf = STRING_STRINGBUF (*str);
}

static SCM
substring_with_immutable_stringbuf (SCM str, size_t start, size_t end,
                                    int force_copy_p, int read_only_p)
{
  SCM buf;
  size_t str_start, len;
  scm_t_bits tag = read_only_p ? RO_STRING_TAG : STRING_TAG;

  get_str_buf_start (&str, &buf, &str_start);
  len = end - start;
  start += str_start;

  if (len == 0)
    return scm_i_make_string (0, NULL, read_only_p);
  else if (!force_copy_p && SCM_LIKELY (!STRINGBUF_MUTABLE (buf)))
    return scm_double_cell (tag, SCM_UNPACK (buf), start, len);
  else
    {
      SCM new_buf, new_str;

      if (STRINGBUF_WIDE (buf))
        {
          new_buf = make_wide_stringbuf (len);
          u32_cpy ((uint32_t *) STRINGBUF_WIDE_CHARS (new_buf),
                   (uint32_t *) (STRINGBUF_WIDE_CHARS (buf) + start), len);
          new_str = scm_double_cell (tag, SCM_UNPACK (new_buf), 0, len);
          scm_i_try_narrow_string (new_str);
        }
      else
        {
          new_buf = make_stringbuf (len);
          memcpy (STRINGBUF_CHARS (new_buf),
                  STRINGBUF_CHARS (buf) + start, len);
          new_str = scm_double_cell (tag, SCM_UNPACK (new_buf), 0, len);
        }

      return new_str;
    }
}

SCM
scm_i_substring (SCM str, size_t start, size_t end)
{
  return substring_with_immutable_stringbuf (str, start, end, 0, 0);
}

SCM
scm_i_substring_read_only (SCM str, size_t start, size_t end)
{
  return substring_with_immutable_stringbuf (str, start, end, 0, 1);
}

SCM
scm_i_substring_copy (SCM str, size_t start, size_t end)
{
  return substring_with_immutable_stringbuf (str, start, end, 1, 0);
}

static void
scm_i_string_ensure_mutable_x (SCM str)
{
  SCM buf;

  if (IS_SH_STRING (str))
    {
      /* Shared-mutation strings always have mutable stringbufs.  */
      buf = STRING_STRINGBUF (SH_STRING_STRING (str));
      if (!STRINGBUF_MUTABLE (buf))
        abort ();
      return;
    }

  if (IS_RO_STRING (str))
    scm_misc_error (NULL, "string is read-only: ~s", scm_list_1 (str));

  buf = STRING_STRINGBUF (str);

  if (STRINGBUF_MUTABLE (buf))
    return;

  /* Otherwise copy and mark the fresh stringbuf as mutable.  Note that
     we copy the whole stringbuf so that the start/len offsets from the
     original string keep working, so that concurrent accessors on this
     string don't see things in an inconsistent state.  */
  {
    SCM new_buf;
    size_t len = STRINGBUF_LENGTH (buf);

    if (STRINGBUF_WIDE (buf))
      {
        new_buf = make_wide_stringbuf (len);
        u32_cpy ((uint32_t *) STRINGBUF_WIDE_CHARS (new_buf),
                 (uint32_t *) STRINGBUF_WIDE_CHARS (buf), len);
      }
    else
      {
        new_buf = make_stringbuf (len);
        memcpy (STRINGBUF_CHARS (new_buf), STRINGBUF_CHARS (buf), len);
      }

    STRINGBUF_SET_MUTABLE (new_buf);
    SET_STRING_STRINGBUF (str, new_buf);
  }
}

SCM
scm_i_substring_shared (SCM str, size_t start, size_t end)
{
  if (start == 0 && end == STRING_LENGTH (str))
    return str;
  else if (start == end)
    return scm_i_make_string (0, NULL, 0);
  else if (IS_RO_STRING (str))
    return scm_i_substring_read_only (str, start, end);
  else
    {
      size_t len = end - start;
      if (IS_SH_STRING (str))
	{
	  start += STRING_START (str);
	  str = SH_STRING_STRING (str);
	}

      scm_i_string_ensure_mutable_x (str);

      return scm_double_cell (SH_STRING_TAG, SCM_UNPACK(str),
			      (scm_t_bits)start, (scm_t_bits) len);
    }
}

SCM
scm_c_substring (SCM str, size_t start, size_t end)
{
  validate_substring_args (str, start, end);
  return scm_i_substring (str, start, end);
}

SCM
scm_c_substring_read_only (SCM str, size_t start, size_t end)
{
  validate_substring_args (str, start, end);
  return scm_i_substring_read_only (str, start, end);
}

SCM
scm_c_substring_copy (SCM str, size_t start, size_t end)
{
  validate_substring_args (str, start, end);
  return scm_i_substring_copy (str, start, end);
}

SCM
scm_c_substring_shared (SCM str, size_t start, size_t end)
{
  validate_substring_args (str, start, end);
  return scm_i_substring_shared (str, start, end);
}


/* Internal accessors
 */

/* Returns the number of characters in STR.  This may be different
   than the memory size of the string storage.  */
size_t
scm_i_string_length (SCM str)
{
  return STRING_LENGTH (str);
}

int
scm_i_string_is_mutable (SCM str)
{
  return !IS_RO_STRING (str);
}

/* True if the string is 'narrow', meaning it has a 8-bit Latin-1
   encoding.  False if it is 'wide', having a 32-bit UCS-4
   encoding.  */
int
scm_i_is_narrow_string (SCM str)
{
  if (IS_SH_STRING (str))
    str = SH_STRING_STRING (str);

  return !STRINGBUF_WIDE (STRING_STRINGBUF (str));
}

/* Try to coerce a string to be narrow.  It if is narrow already, do
   nothing.  If it is wide, shrink it to narrow if none of its
   characters are above 0xFF.  Return true if the string is narrow or
   was made to be narrow.  */
int
scm_i_try_narrow_string (SCM str)
{
  if (IS_SH_STRING (str))
    str = SH_STRING_STRING (str);

  SET_STRING_STRINGBUF (str, narrow_stringbuf (STRING_STRINGBUF (str)));

  return scm_i_is_narrow_string (str);
}

/* Return a pointer to the raw data of the string, which can be either Latin-1
   or UCS-4 encoded data, depending on `scm_i_is_narrow_string (STR)'.  */
const void *
scm_i_string_data (SCM str)
{
  SCM buf;
  size_t start;
  const char *data;

  get_str_buf_start (&str, &buf, &start);

  data = STRINGBUF_CONTENTS (buf);
  data += start * (scm_i_is_narrow_string (str) ? 1 : 4);

  return data;
}

/* Returns a pointer to the 8-bit Latin-1 encoded character array of
   STR.  */
const char *
scm_i_string_chars (SCM str)
{
  SCM buf;
  size_t start;
  get_str_buf_start (&str, &buf, &start);
  if (scm_i_is_narrow_string (str))
    return (const char *) STRINGBUF_CHARS (buf) + start;
  else
    scm_misc_error (NULL, "Invalid read access of chars of wide string: ~s",
                    scm_list_1 (str));
  return NULL;
}

/* Returns a pointer to the 32-bit UCS-4 encoded character array of
   STR.  */
const scm_t_wchar *
scm_i_string_wide_chars (SCM str)
{
  SCM buf;
  size_t start;

  get_str_buf_start (&str, &buf, &start);
  if (!scm_i_is_narrow_string (str))
    return (const scm_t_wchar *) STRINGBUF_WIDE_CHARS (buf) + start;
  else
    scm_misc_error (NULL, "Invalid read access of chars of narrow string: ~s",
                    scm_list_1 (str));
}

/* If the buffer in ORIG_STR is immutable, copy ORIG_STR's characters to
   a new string buffer, so that it can be modified without modifying
   other strings.  */
SCM
scm_i_string_start_writing (SCM orig_str)
{
  scm_i_string_ensure_mutable_x (orig_str);
  return orig_str;
}

/* Return a pointer to the 8-bit Latin-1 chars of a string.  */
char *
scm_i_string_writable_chars (SCM str)
{
  SCM buf;
  size_t start;

  get_str_buf_start (&str, &buf, &start);
  if (scm_i_is_narrow_string (str))
    return (char *) STRINGBUF_CHARS (buf) + start;
  else
    scm_misc_error (NULL, "Invalid write access of chars of wide string: ~s",
                    scm_list_1 (str));
  return NULL;
}

/* Return a pointer to the UCS-4 codepoints of a string.  */
static scm_t_wchar *
scm_i_string_writable_wide_chars (SCM str)
{
  SCM buf;
  size_t start;

  get_str_buf_start (&str, &buf, &start);
  if (!scm_i_is_narrow_string (str))
    return STRINGBUF_WIDE_CHARS (buf) + start;
  else
    scm_misc_error (NULL, "Invalid write access of chars of narrow string: ~s",
                    scm_list_1 (str));
}

/* Unlock the string mutex that was locked when
   scm_i_string_start_writing was called.  */
void
scm_i_string_stop_writing (void)
{
}

/* Return the Xth character of STR as a UCS-4 codepoint.  */
scm_t_wchar
scm_i_string_ref (SCM str, size_t x)
{
  if (scm_i_is_narrow_string (str))
    return (scm_t_wchar) (unsigned char) (scm_i_string_chars (str)[x]);
  else
    return scm_i_string_wide_chars (str)[x];
}

/* Returns index+1 of the first char in STR that matches C, or
   0 if the char is not found.  */
int
scm_i_string_contains_char (SCM str, char ch)
{
  size_t i;
  size_t len = scm_i_string_length (str);

  i = 0;
  if (scm_i_is_narrow_string (str))
    {
      while (i < len)
        {
          if (scm_i_string_chars (str)[i] == ch)
            return i+1;
          i++;
        }
    }
  else
    {
      while (i < len)
        {
          if (scm_i_string_wide_chars (str)[i] 
              == (unsigned char) ch)
            return i+1;
          i++;
        }
    }
  return 0;
}

int 
scm_i_string_strcmp (SCM sstr, size_t start_x, const char *cstr)
{
  if (scm_i_is_narrow_string (sstr))
    {
      const char *a = scm_i_string_chars (sstr) + start_x;
      const char *b = cstr;
      return strncmp (a, b, strlen(b));
    }
  else
    {
      size_t i;
      const scm_t_wchar *a = scm_i_string_wide_chars (sstr) + start_x;
      const char *b = cstr;
      for (i = 0; i < strlen (b); i++)
        {
          if (a[i] != (unsigned char) b[i])
            return 1;
        }
    }
  return 0;
}

/* Set the Pth character of STR to UCS-4 codepoint CHR. */
void
scm_i_string_set_x (SCM str, size_t p, scm_t_wchar chr)
{
  if (IS_SH_STRING (str))
    {
      p += STRING_START (str);
      str = SH_STRING_STRING (str);
    }

  if (chr > 0xFF && scm_i_is_narrow_string (str))
    SET_STRING_STRINGBUF (str, wide_stringbuf (STRING_STRINGBUF (str)));

  if (scm_i_is_narrow_string (str))
    {
      char *dst = scm_i_string_writable_chars (str);
      dst[p] = chr;
    }
  else
    {
      scm_t_wchar *dst = scm_i_string_writable_wide_chars (str);
      dst[p] = chr;
    }
}


/* Symbols.

   Basic symbol creation and accessing is done here, the rest is in
   symbols.[hc].  This has been done to keep stringbufs and the
   internals of strings and string-like objects confined to this file.
*/

#define SYMBOL_STRINGBUF SCM_CELL_OBJECT_1

SCM
scm_i_make_symbol (SCM name, scm_t_bits flags, uintptr_t hash)
{
  SCM buf, symbol;
  size_t start, length = STRING_LENGTH (name);

  get_str_buf_start (&name, &buf, &start);
  if (SCM_UNLIKELY (STRINGBUF_MUTABLE (buf)
                    || start != 0
                    || STRINGBUF_LENGTH (buf) != length))
    {
      name = scm_i_substring_copy (name, 0, length);
      buf = STRING_STRINGBUF (name);
    }

  symbol = scm_words (scm_tc7_symbol | flags, 3);
  SCM_SET_CELL_WORD_1 (symbol, SCM_UNPACK (buf));
  SCM_SET_CELL_WORD_2 (symbol, hash);

  return symbol;
}

/* Returns the number of characters in SYM.  This may be different
   from the memory size of SYM.  */
size_t
scm_i_symbol_length (SCM sym)
{
  return STRINGBUF_LENGTH (SYMBOL_STRINGBUF (sym));
}

size_t
scm_c_symbol_length (SCM sym)
#define FUNC_NAME "scm_c_symbol_length"
{
  SCM_VALIDATE_SYMBOL (1, sym);

  return STRINGBUF_LENGTH (SYMBOL_STRINGBUF (sym));
}
#undef FUNC_NAME

/* True if the name of SYM is stored as a Latin-1 encoded string.
   False if it is stored as a 32-bit UCS-4-encoded string.  */
int
scm_i_is_narrow_symbol (SCM sym)
{
  SCM buf;

  buf = SYMBOL_STRINGBUF (sym);
  return !STRINGBUF_WIDE (buf);
}

/* Returns a pointer to the 8-bit Latin-1 encoded character array that
   contains the name of SYM.  */
const char *
scm_i_symbol_chars (SCM sym)
{
  SCM buf;

  buf = SYMBOL_STRINGBUF (sym);
  if (!STRINGBUF_WIDE (buf))
    return (const char *) STRINGBUF_CHARS (buf);
  else
    scm_misc_error (NULL, "Invalid access of chars of a wide symbol ~S",
                    scm_list_1 (sym));
}

/* Return a pointer to the 32-bit UCS-4-encoded character array of a
   symbol's name.  */
const scm_t_wchar *
scm_i_symbol_wide_chars (SCM sym)
{
  SCM buf;

  buf = SYMBOL_STRINGBUF (sym);
  if (STRINGBUF_WIDE (buf))
    return (const scm_t_wchar *) STRINGBUF_WIDE_CHARS (buf);
  else
    scm_misc_error (NULL, "Invalid access of chars of a narrow symbol ~S",
                    scm_list_1 (sym));
}

SCM
scm_i_symbol_substring (SCM sym, size_t start, size_t end)
{
  SCM buf = SYMBOL_STRINGBUF (sym);
  return scm_double_cell (RO_STRING_TAG, SCM_UNPACK (buf),
			  (scm_t_bits)start, (scm_t_bits) end - start);
}

/* Returns the Xth character of symbol SYM as a UCS-4 codepoint.  */
scm_t_wchar
scm_i_symbol_ref (SCM sym, size_t x)
{
  if (scm_i_is_narrow_symbol (sym))
    return (scm_t_wchar) (unsigned char) (scm_i_symbol_chars (sym)[x]);
  else
    return scm_i_symbol_wide_chars (sym)[x];
}

/* Debugging
 */

SCM_DEFINE (scm_sys_string_dump, "%string-dump", 1, 0, 0, (SCM str), 
            "Returns an association list containing debugging information\n"
            "for @var{str}. The association list has the following entries."
            "@table @code\n"
            "@item string\n"
            "The string itself.\n"
            "@item start\n"
            "The start index of the string into its stringbuf\n"
            "@item length\n"
            "The length of the string\n"
            "@item shared\n"
            "If this string is a substring, it returns its parent string.\n"
            "Otherwise, it returns @code{#f}\n"
            "@item read-only\n"
            "@code{#t} if the string is read-only\n"
            "@item stringbuf-chars\n"
            "A new string containing this string's stringbuf's characters\n"
            "@item stringbuf-length\n"
            "The number of characters in this stringbuf\n"
            "@item stringbuf-mutable\n"
            "@code{#t} if this stringbuf is mutable\n"
            "@item stringbuf-wide\n"
            "@code{#t} if this stringbuf's characters are stored in a\n"
            "32-bit buffer, or @code{#f} if they are stored in an 8-bit\n"
            "buffer\n"
            "@end table")
#define FUNC_NAME s_scm_sys_string_dump
{
  SCM e1, e2, e3, e4, e5, e6, e7, e8, e9;
  SCM buf;
  SCM_VALIDATE_STRING (1, str);

  /* String info */
  e1 = scm_cons (scm_from_latin1_symbol ("string"),
                 str);
  e2 = scm_cons (scm_from_latin1_symbol ("start"),
                 scm_from_size_t (STRING_START (str)));
  e3 = scm_cons (scm_from_latin1_symbol ("length"),
                 scm_from_size_t (STRING_LENGTH (str)));

  if (IS_SH_STRING (str))
    {
      e4 = scm_cons (scm_from_latin1_symbol ("shared"),
                     SH_STRING_STRING (str));
      buf = STRING_STRINGBUF (SH_STRING_STRING (str));
    }
  else
    {
      e4 = scm_cons (scm_from_latin1_symbol ("shared"),
                     SCM_BOOL_F);
      buf = STRING_STRINGBUF (str);
    }

  if (IS_RO_STRING (str))
    e5 = scm_cons (scm_from_latin1_symbol ("read-only"),
                   SCM_BOOL_T);
  else
    e5 = scm_cons (scm_from_latin1_symbol ("read-only"),
                   SCM_BOOL_F);

  /* Stringbuf info */
  if (!STRINGBUF_WIDE (buf))
    {
      size_t len = STRINGBUF_LENGTH (buf);
      char *cbuf;
      SCM sbc = scm_i_make_string (len, &cbuf, 0);
      memcpy (cbuf, STRINGBUF_CHARS (buf), len);
      e6 = scm_cons (scm_from_latin1_symbol ("stringbuf-chars"),
                     sbc);
    }
  else
    {
      size_t len = STRINGBUF_LENGTH (buf);
      scm_t_wchar *cbuf;
      SCM sbc = scm_i_make_wide_string (len, &cbuf, 0);
      u32_cpy ((uint32_t *) cbuf, 
               (uint32_t *) STRINGBUF_WIDE_CHARS (buf), len);
      e6 = scm_cons (scm_from_latin1_symbol ("stringbuf-chars"),
                     sbc);
    }
  e7 = scm_cons (scm_from_latin1_symbol ("stringbuf-length"), 
                 scm_from_size_t (STRINGBUF_LENGTH (buf)));
  if (STRINGBUF_MUTABLE (buf))
    e8 = scm_cons (scm_from_latin1_symbol ("stringbuf-mutable"),
                   SCM_BOOL_T);
  else
    e8 = scm_cons (scm_from_latin1_symbol ("stringbuf-mutable"),
                   SCM_BOOL_F);
  if (STRINGBUF_WIDE (buf))
    e9 = scm_cons (scm_from_latin1_symbol ("stringbuf-wide"),
		   SCM_BOOL_T);
  else
    e9 = scm_cons (scm_from_latin1_symbol ("stringbuf-wide"),
		   SCM_BOOL_F);

  return scm_list_n (e1, e2, e3, e4, e5, e6, e7, e8, e9, SCM_UNDEFINED);
}
#undef FUNC_NAME

SCM_DEFINE (scm_sys_symbol_dump, "%symbol-dump", 1, 0, 0, (SCM sym),
            "Returns an association list containing debugging information\n"
            "for @var{sym}. The association list has the following entries."
            "@table @code\n"
            "@item symbol\n"
            "The symbol itself\n"
            "@item hash\n"
            "Its hash value\n"
            "@item interned\n"
            "@code{#t} if it is an interned symbol\n"
            "@item stringbuf-chars\n"
            "A new string containing this symbols's stringbuf's characters\n"
            "@item stringbuf-length\n"
            "The number of characters in this stringbuf\n"
            "@item stringbuf-mutable\n"
            "@code{#t} if this stringbuf is mutable\n"
            "@item stringbuf-wide\n"
            "@code{#t} if this stringbuf's characters are stored in a\n"
            "32-bit buffer, or @code{#f} if they are stored in an 8-bit\n"
            "buffer\n"
            "@end table")
#define FUNC_NAME s_scm_sys_symbol_dump
{
  SCM e1, e2, e3, e4, e5, e6, e7;
  SCM buf;
  SCM_VALIDATE_SYMBOL (1, sym);
  e1 = scm_cons (scm_from_latin1_symbol ("symbol"),
                 sym);
  e2 = scm_cons (scm_from_latin1_symbol ("hash"),
                 scm_from_ulong (scm_i_symbol_hash (sym)));
  e3 = scm_cons (scm_from_latin1_symbol ("interned"),
                 scm_symbol_interned_p (sym));
  buf = SYMBOL_STRINGBUF (sym);

  /* Stringbuf info */
  if (!STRINGBUF_WIDE (buf))
    {
      size_t len = STRINGBUF_LENGTH (buf);
      char *cbuf;
      SCM sbc = scm_i_make_string (len, &cbuf, 0);
      memcpy (cbuf, STRINGBUF_CHARS (buf), len);
      e4 = scm_cons (scm_from_latin1_symbol ("stringbuf-chars"),
                     sbc);
    }
  else
    {
      size_t len = STRINGBUF_LENGTH (buf);
      scm_t_wchar *cbuf;
      SCM sbc = scm_i_make_wide_string (len, &cbuf, 0);
      u32_cpy ((uint32_t *) cbuf, 
               (uint32_t *) STRINGBUF_WIDE_CHARS (buf), len);
      e4 = scm_cons (scm_from_latin1_symbol ("stringbuf-chars"),
                     sbc);
    }
  e5 = scm_cons (scm_from_latin1_symbol ("stringbuf-length"), 
                 scm_from_size_t (STRINGBUF_LENGTH (buf)));
  if (STRINGBUF_MUTABLE (buf))
    e6 = scm_cons (scm_from_latin1_symbol ("stringbuf-mutable"),
                   SCM_BOOL_T);
  else
    e6 = scm_cons (scm_from_latin1_symbol ("stringbuf-mutable"),
                   SCM_BOOL_F);
  if (STRINGBUF_WIDE (buf))
    e7 = scm_cons (scm_from_latin1_symbol ("stringbuf-wide"),
                    SCM_BOOL_T);
  else
    e7 = scm_cons (scm_from_latin1_symbol ("stringbuf-wide"),
                    SCM_BOOL_F);
  return scm_list_n (e1, e2, e3, e4, e5, e6, e7, SCM_UNDEFINED);

}
#undef FUNC_NAME

#ifdef SCM_STRING_LENGTH_HISTOGRAM

SCM_DEFINE (scm_sys_stringbuf_hist, "%stringbuf-hist", 0, 0, 0, (void), "")
#define FUNC_NAME s_scm_sys_stringbuf_hist
{
  int i;
  for (i = 0; i < 1000; i++)
    if (lenhist[i])
      fprintf (stderr, " %3d: %u\n", i, lenhist[i]);
  fprintf (stderr, ">999: %u\n", lenhist[1000]);
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

#endif



SCM_DEFINE (scm_string_p, "string?", 1, 0, 0, 
	    (SCM obj),
	    "Return @code{#t} if @var{obj} is a string, else @code{#f}.")
#define FUNC_NAME s_scm_string_p
{
  return scm_from_bool (IS_STRING (obj));
}
#undef FUNC_NAME


SCM_REGISTER_PROC (s_scm_list_to_string, "list->string", 1, 0, 0, scm_string);

SCM_DEFINE (scm_string, "string", 0, 0, 1, 
            (SCM chrs),
	    "@deffnx {Scheme Procedure} list->string chrs\n"
	    "Return a newly allocated string composed of the arguments,\n"
	    "@var{chrs}.")
#define FUNC_NAME s_scm_string
{
  SCM result = SCM_BOOL_F;
  SCM rest;
  size_t len;
  size_t p = 0;
  long i;
  int wide = 0;

  /* Verify that this is a list of chars.  */
  i = scm_ilength (chrs);
  SCM_ASSERT (i >= 0, chrs, SCM_ARG1, FUNC_NAME);

  len = (size_t) i;
  rest = chrs;

  while (len > 0 && scm_is_pair (rest))
    {
      SCM elt = SCM_CAR (rest);
      SCM_VALIDATE_CHAR (SCM_ARGn, elt);
      if (SCM_CHAR (elt) > 0xFF)
        wide = 1;
      rest = SCM_CDR (rest);
      len--;
      scm_remember_upto_here_1 (elt);
    }

  /* Construct a string containing this list of chars.  */
  len = (size_t) i;
  rest = chrs;

  if (wide == 0)
    {
      char *buf;

      result = scm_i_make_string (len, NULL, 0);
      buf = scm_i_string_writable_chars (result);
      while (len > 0 && scm_is_pair (rest))
        {
          SCM elt = SCM_CAR (rest);
          buf[p] = (unsigned char) SCM_CHAR (elt);
          p++;
          rest = SCM_CDR (rest);
          len--;
          scm_remember_upto_here_1 (elt);
        }
    }
  else
    {
      scm_t_wchar *buf;

      result = scm_i_make_wide_string (len, NULL, 0);
      buf = scm_i_string_writable_wide_chars (result);
      while (len > 0 && scm_is_pair (rest))
        {
          SCM elt = SCM_CAR (rest);
          buf[p] = SCM_CHAR (elt);
          p++;
          rest = SCM_CDR (rest);
          len--;
          scm_remember_upto_here_1 (elt);
        }
    }

  if (len > 0)
    scm_misc_error (NULL, "list changed while constructing string", SCM_EOL);
  if (!scm_is_null (rest))
    scm_wrong_type_arg_msg (NULL, 0, chrs, "proper list");

  return result;
}
#undef FUNC_NAME

SCM_DEFINE (scm_make_string, "make-string", 1, 1, 0,
            (SCM k, SCM chr),
	    "Return a newly allocated string of\n"
            "length @var{k}.  If @var{chr} is given, then all elements of\n"
	    "the string are initialized to @var{chr}, otherwise the contents\n"
	    "of the string are all set to @code{#\nul}.")
#define FUNC_NAME s_scm_make_string
{
  SCM ret = scm_c_make_string (scm_to_size_t (k), chr);
  /* Given that make-string is mostly used by Scheme to prepare a
     mutable string buffer, let's go ahead and mark this as mutable to
     avoid a copy when this buffer is next written to.  */
  STRINGBUF_SET_MUTABLE (STRING_STRINGBUF (ret));
  return ret;
}
#undef FUNC_NAME

SCM
scm_c_make_string (size_t len, SCM chr)
#define FUNC_NAME NULL
{
  size_t p;
  char *contents = NULL;
  SCM res = scm_i_make_string (len, &contents, 0);

  /* If no char is given, initialize string contents to NULL.  */
  if (SCM_UNBNDP (chr))
    memset (contents, 0, len);
  else
    {
      SCM_VALIDATE_CHAR (0, chr);
      for (p = 0; p < len; p++)
        scm_i_string_set_x (res, p, SCM_CHAR (chr));
    }

  return res;
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_length, "string-length", 1, 0, 0, 
	    (SCM string),
	    "Return the number of characters in @var{string}.")
#define FUNC_NAME s_scm_string_length
{
  SCM_VALIDATE_STRING (1, string);
  return scm_from_size_t (STRING_LENGTH (string));
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_bytes_per_char, "string-bytes-per-char", 1, 0, 0,
            (SCM string),
            "Return the bytes used to represent a character in @var{string}."
            "This will return 1 or 4.")
#define FUNC_NAME s_scm_string_bytes_per_char
{
  SCM_VALIDATE_STRING (1, string);
  if (!scm_i_is_narrow_string (string))
    return scm_from_int (4);

  return scm_from_int (1);
}
#undef FUNC_NAME

size_t
scm_c_string_length (SCM string)
{
  if (!IS_STRING (string))
    scm_wrong_type_arg_msg (NULL, 0, string, "string");
  return STRING_LENGTH (string);
}

SCM_DEFINE (scm_string_ref, "string-ref", 2, 0, 0,
            (SCM str, SCM k),
            "Return character @var{k} of @var{str} using zero-origin\n"
            "indexing. @var{k} must be a valid index of @var{str}.")
#define FUNC_NAME s_scm_string_ref
{
  size_t len;
  unsigned long idx;

  SCM_VALIDATE_STRING (1, str);

  len = scm_i_string_length (str);
  if (SCM_LIKELY (len > 0))
    idx = scm_to_unsigned_integer (k, 0, len - 1);
  else
    scm_out_of_range (NULL, k);

  if (scm_i_is_narrow_string (str))
    return scm_c_make_char (scm_i_string_chars (str)[idx]);
  else
    return scm_c_make_char (scm_i_string_wide_chars (str)[idx]);
}
#undef FUNC_NAME

SCM
scm_c_string_ref (SCM str, size_t p)
{
  if (p >= scm_i_string_length (str))
    scm_out_of_range (NULL, scm_from_size_t (p));
  if (scm_i_is_narrow_string (str))
    return scm_c_make_char (scm_i_string_chars (str)[p]);
  else
    return scm_c_make_char (scm_i_string_wide_chars (str)[p]);

}

SCM_DEFINE (scm_string_set_x, "string-set!", 3, 0, 0,
            (SCM str, SCM k, SCM chr),
            "Store @var{chr} in element @var{k} of @var{str} and return\n"
            "an unspecified value. @var{k} must be a valid index of\n"
            "@var{str}.")
#define FUNC_NAME s_scm_string_set_x
{
  size_t len;
  unsigned long idx;

  SCM_VALIDATE_STRING (1, str);

  len = scm_i_string_length (str);
  if (SCM_LIKELY (len > 0))
    idx = scm_to_unsigned_integer (k, 0, len - 1);
  else
    scm_out_of_range (NULL, k);

  SCM_VALIDATE_CHAR (3, chr);
  str = scm_i_string_start_writing (str);
  scm_i_string_set_x (str, idx, SCM_CHAR (chr));
  scm_i_string_stop_writing ();

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

void
scm_c_string_set_x (SCM str, size_t p, SCM chr)
{
  if (p >= scm_i_string_length (str))
    scm_out_of_range (NULL, scm_from_size_t (p));
  str = scm_i_string_start_writing (str);
  scm_i_string_set_x (str, p, SCM_CHAR (chr));
  scm_i_string_stop_writing ();
}

SCM_DEFINE (scm_substring, "substring", 2, 1, 0,
	    (SCM str, SCM start, SCM end),
	    "Return a newly allocated string formed from the characters\n"
            "of @var{str} beginning with index @var{start} (inclusive) and\n"
	    "ending with index @var{end} (exclusive).\n"
            "@var{str} must be a string, @var{start} and @var{end} must be\n"
	    "exact integers satisfying:\n\n"
            "0 <= @var{start} <= @var{end} <= (string-length @var{str}).")
#define FUNC_NAME s_scm_substring
{
  size_t len, from, to;

  SCM_VALIDATE_STRING (1, str);
  len = scm_i_string_length (str);
  from = scm_to_unsigned_integer (start, 0, len);
  if (SCM_UNBNDP (end))
    to = len;
  else
    to = scm_to_unsigned_integer (end, from, len);
  return scm_i_substring (str, from, to);
}
#undef FUNC_NAME

SCM_DEFINE (scm_substring_read_only, "substring/read-only", 2, 1, 0,
	    (SCM str, SCM start, SCM end),
	    "Return a newly allocated string formed from the characters\n"
            "of @var{str} beginning with index @var{start} (inclusive) and\n"
	    "ending with index @var{end} (exclusive).\n"
            "@var{str} must be a string, @var{start} and @var{end} must be\n"
	    "exact integers satisfying:\n"
	    "\n"
            "0 <= @var{start} <= @var{end} <= (string-length @var{str}).\n"
	    "\n"
	    "The returned string is read-only.\n")
#define FUNC_NAME s_scm_substring_read_only
{
  size_t len, from, to;

  SCM_VALIDATE_STRING (1, str);
  len = scm_i_string_length (str);
  from = scm_to_unsigned_integer (start, 0, len);
  if (SCM_UNBNDP (end))
    to = len;
  else
    to = scm_to_unsigned_integer (end, from, len);
  return scm_i_substring_read_only (str, from, to);
}
#undef FUNC_NAME

SCM_DEFINE (scm_substring_copy, "substring/copy", 2, 1, 0,
	    (SCM str, SCM start, SCM end),
	    "Return a newly allocated string formed from the characters\n"
            "of @var{str} beginning with index @var{start} (inclusive) and\n"
	    "ending with index @var{end} (exclusive).\n"
            "@var{str} must be a string, @var{start} and @var{end} must be\n"
	    "exact integers satisfying:\n\n"
            "0 <= @var{start} <= @var{end} <= (string-length @var{str}).")
#define FUNC_NAME s_scm_substring_copy
{
  /* For the Scheme version, START is mandatory, but for the C
     version, it is optional.  See scm_string_copy in srfi-13.c for a
     rationale.
  */

  size_t from, to;

  SCM_VALIDATE_STRING (1, str);
  scm_i_get_substring_spec (scm_i_string_length (str),
			    start, &from, end, &to);
  return scm_i_substring_copy (str, from, to);
}
#undef FUNC_NAME

SCM_DEFINE (scm_substring_shared, "substring/shared", 2, 1, 0,
	    (SCM str, SCM start, SCM end),
	    "Return string that indirectly refers to the characters\n"
            "of @var{str} beginning with index @var{start} (inclusive) and\n"
	    "ending with index @var{end} (exclusive).\n"
            "@var{str} must be a string, @var{start} and @var{end} must be\n"
	    "exact integers satisfying:\n\n"
            "0 <= @var{start} <= @var{end} <= (string-length @var{str}).")
#define FUNC_NAME s_scm_substring_shared
{
  size_t len, from, to;

  SCM_VALIDATE_STRING (1, str);
  len = scm_i_string_length (str);
  from = scm_to_unsigned_integer (start, 0, len);
  if (SCM_UNBNDP (end))
    to = len;
  else
    to = scm_to_unsigned_integer (end, from, len);
  return scm_i_substring_shared (str, from, to);
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_append, "string-append", 0, 0, 1, 
            (SCM args),
            "Return a newly allocated string whose characters form the\n"
            "concatenation of the given strings, @var{args}.")
#define FUNC_NAME s_scm_string_append
{
  SCM res;
  size_t total = 0;
  size_t len;
  int wide = 0;
  SCM l, s;
  size_t i;
  union
  {
    char *narrow;
    scm_t_wchar *wide;
  } data;

  SCM_VALIDATE_REST_ARGUMENT (args);
  for (l = args; !scm_is_null (l); l = SCM_CDR (l))
    {
      s = SCM_CAR (l);
      SCM_VALIDATE_STRING (SCM_ARGn, s);
      len = scm_i_string_length (s);
      if (INT_ADD_OVERFLOW (total, len))
        scm_num_overflow (FUNC_NAME);
      total += len;
      if (!scm_i_is_narrow_string (s))
        wide = 1;
    }
  data.narrow = NULL;
  if (!wide)
    res = scm_i_make_string (total, &data.narrow, 0);
  else
    res = scm_i_make_wide_string (total, &data.wide, 0);

  for (l = args; !scm_is_null (l); l = SCM_CDR (l))
    {
      size_t len;
      s = SCM_CAR (l);
      SCM_VALIDATE_STRING (SCM_ARGn, s);
      len = scm_i_string_length (s);
      if (len > total)
        SCM_MISC_ERROR ("list changed during string-append", SCM_EOL);
      if (!wide)
        {
          memcpy (data.narrow, scm_i_string_chars (s), len);
          data.narrow += len;
        }
      else
        {
          if (scm_i_is_narrow_string (s))
            {
              const char *src = scm_i_string_chars (s);
              for (i = 0; i < len; i++)
                data.wide[i] = (unsigned char) src[i];
            }
          else
            u32_cpy ((uint32_t *) data.wide,
                     (uint32_t *) scm_i_string_wide_chars (s), len);
          data.wide += len;
        }
      total -= len;
      scm_remember_upto_here_1 (s);
    }
  if (total != 0)
    SCM_MISC_ERROR ("list changed during string-append", SCM_EOL);
  return res;
}
#undef FUNC_NAME



/* Charset conversion error handling.  */

SCM_SYMBOL (scm_encoding_error_key, "encoding-error");
SCM_SYMBOL (scm_decoding_error_key, "decoding-error");

/* Raise an exception informing that character CHR could not be written
   to PORT in its current encoding.  */
void
scm_encoding_error (const char *subr, int err, const char *message,
		    SCM port, SCM chr)
{
  scm_throw (scm_encoding_error_key,
	     scm_list_n (scm_from_latin1_string (subr),
			 scm_from_latin1_string (message),
			 scm_from_int (err),
			 port, chr,
			 SCM_UNDEFINED));
}

/* Raise an exception informing of an encoding error on PORT.  This
   means that a character could not be written in PORT's encoding.  */
void
scm_decoding_error (const char *subr, int err, const char *message, SCM port)
{
  scm_throw (scm_decoding_error_key,
	     scm_list_n (scm_from_latin1_string (subr),
			 scm_from_latin1_string (message),
			 scm_from_int (err),
			 port,
			 SCM_UNDEFINED));
}


/* String conversion to/from C.  */

static void
decoding_error (const char *func_name, int errno_save,
                const char *str, size_t len)
{
  /* Raise an error and pass the raw C string as a bytevector to the `throw'
     handler.  */
  SCM bv;
  signed char *buf;

  buf = scm_gc_malloc_pointerless (len, "bytevector");
  memcpy (buf, str, len);
  bv = scm_c_take_gc_bytevector (buf, len, SCM_BOOL_F);

  scm_decoding_error (func_name, errno_save,
                      "input locale conversion error", bv);
}

SCM
scm_from_stringn (const char *str, size_t len, const char *encoding,
                  scm_t_string_failed_conversion_handler handler)
{
  size_t u32len, i;
  scm_t_wchar *u32;
  int wide = 0;
  SCM res;

  /* The order of these checks is important. */
  if (!str && len != 0)
    scm_misc_error ("scm_from_stringn", "NULL string pointer", SCM_EOL);
  if (len == (size_t) -1)
    len = strlen (str);

  if (c_strcasecmp (encoding, "ISO-8859-1") == 0 || len == 0)
    return scm_from_latin1_stringn (str, len);
  else if (c_strcasecmp (encoding, "UTF-8") == 0
           && handler == SCM_FAILED_CONVERSION_ERROR)
    return scm_from_utf8_stringn (str, len);

  u32len = 0;
  u32 = (scm_t_wchar *) u32_conv_from_encoding (encoding,
                                                (enum iconv_ilseq_handler)
                                                handler,
                                                str, len,
                                                NULL,
                                                NULL, &u32len);

  if (SCM_UNLIKELY (u32 == NULL))
    decoding_error (__func__, errno, str, len);

  i = 0;
  while (i < u32len)
    if (u32[i++] > 0xFF)
      {
        wide = 1;
        break;
      }

  if (!wide)
    {
      char *dst;
      res = scm_i_make_string (u32len, &dst, 0);
      for (i = 0; i < u32len; i ++)
        dst[i] = (unsigned char) u32[i];
      dst[u32len] = '\0';
    }
  else
    {
      scm_t_wchar *wdst;
      res = scm_i_make_wide_string (u32len, &wdst, 0);
      u32_cpy ((uint32_t *) wdst, (uint32_t *) u32, u32len);
      wdst[u32len] = 0;
    }

  free (u32);
  return res;
}

SCM
scm_from_locale_string (const char *str)
{
  return scm_from_locale_stringn (str, -1);
}

scm_t_string_failed_conversion_handler
scm_i_default_string_failed_conversion_handler (void)
{
  return scm_i_string_failed_conversion_handler
    (scm_i_default_port_conversion_strategy ());
}

SCM
scm_from_locale_stringn (const char *str, size_t len)
{
  return scm_from_stringn (str, len, locale_charset (),
                           scm_i_default_string_failed_conversion_handler ());
}

SCM
scm_from_latin1_string (const char *str)
{
  return scm_from_latin1_stringn (str, -1);
}

SCM
scm_from_latin1_stringn (const char *str, size_t len)
{
  char *buf;
  SCM result;

  if (len == (size_t) -1)
    len = strlen (str);

  /* Make a narrow string and copy STR as is.  */
  result = scm_i_make_string (len, &buf, 0);
  memcpy (buf, str, len);

  return result;
}

SCM
scm_from_utf8_string (const char *str)
{
  return scm_from_utf8_stringn (str, -1);
}

SCM
scm_from_utf8_stringn (const char *str, size_t len)
{
  size_t i, char_len;
  const uint8_t *ustr = (const uint8_t *) str;
  int ascii = 1, narrow = 1;
  SCM res;

  if (len == (size_t) -1)
    len = strlen (str);

  i = 0;
  char_len = 0;

  while (i < len)
    {
      if (ustr[i] <= 127)
        {
          char_len++;
          i++;
        }
      else
        {
          ucs4_t c;
          int nbytes;

          ascii = 0;

          nbytes = u8_mbtoucr (&c, ustr + i, len - i);

          if (nbytes < 0)
            /* Bad UTF-8.  */
            decoding_error (__func__, errno, str, len);

          if (c > 255)
            narrow = 0;
          
          char_len++;
          i += nbytes;
        }
    }
  
  if (ascii)
    {
      char *dst;
      res = scm_i_make_string (char_len, &dst, 0);
      memcpy (dst, str, len);
    }
  else if (narrow)
    {
      char *dst;
      size_t j;
      ucs4_t c;

      res = scm_i_make_string (char_len, &dst, 0);

      for (i = 0, j = 0; i < len; j++)
        {
          i += u8_mbtouc_unsafe (&c, ustr + i, len - i);
          dst[j] = (signed char) c;
        }
    }
  else
    {
      scm_t_wchar *dst;
      size_t j;
      ucs4_t c;

      res = scm_i_make_wide_string (char_len, &dst, 0);

      for (i = 0, j = 0; i < len; j++)
        {
          i += u8_mbtouc_unsafe (&c, ustr + i, len - i);
          dst[j] = c;
        }
    }

  return res;
}

SCM
scm_from_utf32_string (const scm_t_wchar *str)
{
  return scm_from_utf32_stringn (str, -1);
}

SCM
scm_from_utf32_stringn (const scm_t_wchar *str, size_t len)
{
  SCM result;
  scm_t_wchar *buf;

  if (len == (size_t) -1)
    len = u32_strlen ((uint32_t *) str);

  result = scm_i_make_wide_string (len, &buf, 0);
  memcpy (buf, str, len * sizeof (scm_t_wchar));
  scm_i_try_narrow_string (result);

  return result;
}

SCM
scm_from_port_string (const char *str, SCM port)
{
  return scm_from_port_stringn (str, -1, port);
}

SCM
scm_from_port_stringn (const char *str, size_t len, SCM port)
{
  scm_t_port *pt = SCM_PORT (port);

  if (scm_is_eq (pt->encoding, sym_ISO_8859_1))
    return scm_from_latin1_stringn (str, len);
  else if (scm_is_eq (pt->encoding, sym_UTF_8)
           && (scm_is_eq (pt->conversion_strategy, sym_error)
               || (u8_check ((uint8_t *) str, len) == NULL)))
    return scm_from_utf8_stringn (str, len);
  else
    return scm_from_stringn (str, len, scm_i_symbol_chars (pt->encoding),
                             scm_i_string_failed_conversion_handler
                             (SCM_PORT (port)->conversion_strategy));
}

/* Create a new scheme string from the C string STR.  The memory of
   STR may be used directly as storage for the new string.  */
/* FIXME: GC-wise, the only way to use the memory area pointed to by STR
   would be to register a finalizer to eventually free(3) STR, which isn't
   worth it.  Should we just deprecate the `scm_take_' functions?  */
SCM
scm_take_locale_stringn (char *str, size_t len)
{
  SCM res;

  res = scm_from_locale_stringn (str, len);
  free (str);

  return res;
}

SCM
scm_take_locale_string (char *str)
{
  return scm_take_locale_stringn (str, -1);
}

/* Change libunistring escapes (`\uXXXX' and `\UXXXXXXXX') in BUF, a
   *LENP-byte locale-encoded string, to `\xXX', `\uXXXX', or `\UXXXXXX'.
   Set *LENP to the size of the resulting string.

   FIXME: This is a hack we should get rid of.  See
   <http://lists.gnu.org/archive/html/bug-libunistring/2010-09/msg00004.html>
   for details.  */
static void
unistring_escapes_to_guile_escapes (char *buf, size_t *lenp)
{
  char *before, *after;
  size_t i, j;

  before = buf;
  after = buf;
  i = 0;
  j = 0;
  while (i < *lenp)
    {
      if ((i <= *lenp - 6)
          && before[i] == '\\'
          && before[i + 1] == 'u'
          && before[i + 2] == '0' && before[i + 3] == '0')
        {
          /* Convert \u00NN to \xNN */
          after[j] = '\\';
          after[j + 1] = 'x';
          after[j + 2] = tolower ((int) before[i + 4]);
          after[j + 3] = tolower ((int) before[i + 5]);
          i += 6;
          j += 4;
        }
      else if ((i <= *lenp - 10)
               && before[i] == '\\'
               && before[i + 1] == 'U'
               && before[i + 2] == '0' && before[i + 3] == '0')
        {
          /* Convert \U00NNNNNN to \UNNNNNN */
          after[j] = '\\';
          after[j + 1] = 'U';
          after[j + 2] = tolower ((int) before[i + 4]);
          after[j + 3] = tolower ((int) before[i + 5]);
          after[j + 4] = tolower ((int) before[i + 6]);
          after[j + 5] = tolower ((int) before[i + 7]);
          after[j + 6] = tolower ((int) before[i + 8]);
          after[j + 7] = tolower ((int) before[i + 9]);
          i += 10;
          j += 8;
        }
      else
        {
          after[j] = before[i];
          i++;
          j++;
        }
    }
  *lenp = j;
}

/* Change libunistring escapes (`\uXXXX' and `\UXXXXXXXX') in BUF, a
   *LENP-byte locale-encoded string, to `\xXXXX;'.  Set *LEN to the size
   of the resulting string.  BUF must be large enough to handle the
   worst case when `\uXXXX' escapes (6 characters) are replaced by
   `\xXXXX;' (7 characters).  */
static void
unistring_escapes_to_r6rs_escapes (char *buf, size_t *lenp)
{
  char *before, *after;
  int malloc_p;
  size_t i, j;
  /* The worst case is if the input string contains all 4-digit hex escapes.
     "\uXXXX" (six characters) becomes "\xXXXX;" (seven characters) */
  size_t max_out_len = (*lenp * 7) / 6 + 1;
  size_t nzeros, ndigits;

  before = buf;
  malloc_p = (max_out_len > SCM_MAX_ALLOCA);
  after = malloc_p ? malloc (max_out_len) : alloca (max_out_len);
  i = 0;
  j = 0;
  while (i < *lenp)
    {
      if (((i <= *lenp - 6) && before[i] == '\\' && before[i + 1] == 'u')
          || ((i <= *lenp - 10) && before[i] == '\\' && before[i + 1] == 'U'))
        {
          if (before[i + 1] == 'u')
            ndigits = 4;
          else if (before[i + 1] == 'U')
            ndigits = 8;
          else
            abort ();

          /* Add the R6RS hex escape initial sequence.  */
          after[j] = '\\';
          after[j + 1] = 'x';

          /* Move string positions to the start of the hex numbers.  */
          i += 2;
          j += 2;

          /* Find the number of initial zeros in this hex number.  */
          nzeros = 0;
          while (before[i + nzeros] == '0' && nzeros < ndigits)
            nzeros++;

          /* Copy the number, skipping initial zeros, and then move the string
             positions.  */
          if (nzeros == ndigits)
            {
              after[j] = '0';
              i += ndigits;
              j += 1;
            }
          else
            {
              int pos;
              for (pos = 0; pos < ndigits - nzeros; pos++)
                after[j + pos] = tolower ((int) before[i + nzeros + pos]);
              i += ndigits;
              j += (ndigits - nzeros);
            }

          /* Add terminating semicolon.  */
          after[j] = ';';
          j++;
        }
      else
        {
          after[j] = before[i];
          i++;
          j++;
        }
    }
  *lenp = j;
  memcpy (before, after, j);
  if (malloc_p)
    free (after);
}

char *
scm_to_locale_string (SCM str)
{
  return scm_to_locale_stringn (str, NULL);
}

char *
scm_to_locale_stringn (SCM str, size_t *lenp)
{
  return scm_to_stringn (str, lenp,
                         locale_charset (),
                         scm_i_default_string_failed_conversion_handler ());
}

char *
scm_to_latin1_string (SCM str)
{
  return scm_to_latin1_stringn (str, NULL);
}

char *
scm_to_latin1_stringn (SCM str, size_t *lenp)
#define FUNC_NAME "scm_to_latin1_stringn"
{
  char *result;

  SCM_VALIDATE_STRING (1, str);

  if (scm_i_is_narrow_string (str))
    {
      size_t len = scm_i_string_length (str);

      if (lenp)
        *lenp = len;

      result = scm_strndup (scm_i_string_data (str), len);
    }
  else
    result = scm_to_stringn (str, lenp, NULL,
                             SCM_FAILED_CONVERSION_ERROR);

  return result;
}
#undef FUNC_NAME

char *
scm_to_utf8_string (SCM str)
{
  return scm_to_utf8_stringn (str, NULL);
}

static size_t
latin1_u8_strlen (const uint8_t *str, size_t len)
{
  size_t ret, i;
  for (i = 0, ret = 0; i < len; i++)
    ret += (str[i] < 128) ? 1 : 2;
  return ret;
}

static uint8_t*
latin1_to_u8 (const uint8_t *str, size_t latin_len,
              uint8_t *u8_result, size_t *u8_lenp)
{
  size_t i, n;
  size_t u8_len = latin1_u8_strlen (str, latin_len);

  if (!(u8_result && u8_lenp && *u8_lenp > u8_len))
    u8_result = scm_malloc (u8_len + 1);
  if (u8_lenp)
    *u8_lenp = u8_len;

  for (i = 0, n = 0; i < latin_len; i++)
    n += u8_uctomb (u8_result + n, str[i], u8_len - n);
  if (n != u8_len)
    abort ();
  u8_result[n] = 0;

  return u8_result;
}

/* UTF-8 code table

   (Note that this includes code points that are not allowed by Unicode,
    but since this function has no way to report an error, and its
    purpose is to determine the size of destination buffers for
    libunicode conversion functions, we err on the safe side and handle
    everything that libunicode might conceivably handle, now or in the
    future.)

   Char. number range  |        UTF-8 octet sequence
      (hexadecimal)    |              (binary)
   --------------------+------------------------------------------------------
   0000 0000-0000 007F | 0xxxxxxx
   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
   0001 0000-001F FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
   0020 0000-03FF FFFF | 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
   0400 0000-7FFF FFFF | 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
*/

static size_t
u32_u8_length_in_bytes (const uint32_t *str, size_t len)
{
  size_t ret, i;

  for (i = 0, ret = 0; i < len; i++)
    {
      uint32_t c = str[i];

      if (c <= 0x7f)
        ret += 1;
      else if (c <= 0x7ff)
        ret += 2;
      else if (c <= 0xffff)
        ret += 3;
      else if (c <= 0x1fffff)
        ret += 4;
      else if (c <= 0x3ffffff)
        ret += 5;
      else
        ret += 6;
    }

  return ret;
}

static size_t
utf8_length (SCM str)
{
  if (scm_i_is_narrow_string (str))
    return latin1_u8_strlen ((uint8_t *) scm_i_string_chars (str),
                             scm_i_string_length (str));
  else
    return u32_u8_length_in_bytes
      ((uint32_t *) scm_i_string_wide_chars (str),
       scm_i_string_length (str));
}

size_t
scm_c_string_utf8_length (SCM string)
#define FUNC_NAME "scm_c_string_utf8_length"
{
  SCM_VALIDATE_STRING (1, string);
  return utf8_length (string);
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_utf8_length, "string-utf8-length", 1, 0, 0,
	    (SCM string),
	    "Returns the number of bytes in the UTF-8 representation of "
            "@var{string}.")
#define FUNC_NAME s_scm_string_utf8_length
{
  SCM_VALIDATE_STRING (1, string);
  return scm_from_size_t (utf8_length (string));
}
#undef FUNC_NAME

char *
scm_to_utf8_stringn (SCM str, size_t *lenp)
#define FUNC_NAME "scm_to_utf8_stringn"
{
  SCM_VALIDATE_STRING (1, str);

  if (scm_i_is_narrow_string (str))
    return (char *) latin1_to_u8 ((uint8_t *) scm_i_string_chars (str),
                                  scm_i_string_length (str),
                                  NULL, lenp);
  else
    {
      uint32_t *chars = (uint32_t *) scm_i_string_wide_chars (str);
      uint8_t *buf, *ret;
      size_t num_chars = scm_i_string_length (str);
      size_t num_bytes_predicted, num_bytes_actual;

      num_bytes_predicted = u32_u8_length_in_bytes (chars, num_chars);

      if (lenp)
        {
          *lenp = num_bytes_predicted;
          buf = scm_malloc (num_bytes_predicted);
        }
      else
        {
          buf = scm_malloc (num_bytes_predicted + 1);
          buf[num_bytes_predicted] = 0;
        }

      num_bytes_actual = num_bytes_predicted;
      ret = u32_to_u8 (chars, num_chars, buf, &num_bytes_actual);

      if (SCM_LIKELY (ret == buf && num_bytes_actual == num_bytes_predicted))
        return (char *) ret;

      /* An error: a bad codepoint.  */
      {
        int saved_errno = errno;

        free (buf);
        if (!saved_errno)
          abort ();

        scm_decoding_error ("scm_to_utf8_stringn", errno,
                            "invalid codepoint in string", str);

        /* Not reached.  */
        return NULL;
      }
    }
}
#undef FUNC_NAME

scm_t_wchar *
scm_to_utf32_string (SCM str)
{
  return scm_to_utf32_stringn (str, NULL);
}

scm_t_wchar *
scm_to_utf32_stringn (SCM str, size_t *lenp)
#define FUNC_NAME "scm_to_utf32_stringn"
{
  scm_t_wchar *result;

  SCM_VALIDATE_STRING (1, str);

  if (scm_i_is_narrow_string (str))
    {
      uint8_t *codepoints;
      size_t i, len;

      codepoints = (uint8_t*) scm_i_string_chars (str);
      len = scm_i_string_length (str);
      if (lenp)
	*lenp = len;

      result = scm_malloc ((len + 1) * sizeof (scm_t_wchar));
      for (i = 0; i < len; i++)
        result[i] = codepoints[i];
      result[len] = 0;
    }
  else
    {
      size_t len;

      len = scm_i_string_length (str);
      if (lenp)
	*lenp = len;

      result = scm_malloc ((len + 1) * sizeof (scm_t_wchar));
      memcpy (result, scm_i_string_wide_chars (str),
	      len * sizeof (scm_t_wchar));
      result[len] = 0;
    }

  return result;
}
#undef FUNC_NAME

char *
scm_to_port_string (SCM str, SCM port)
{
  return scm_to_port_stringn (str, NULL, port);
}

char *
scm_to_port_stringn (SCM str, size_t *lenp, SCM port)
{
  scm_t_port *pt = SCM_PORT (port);

  if (scm_is_eq (pt->encoding, sym_ISO_8859_1)
      && scm_is_eq (pt->conversion_strategy, sym_error))
    return scm_to_latin1_stringn (str, lenp);
  else if (scm_is_eq (pt->encoding, sym_UTF_8))
    return scm_to_utf8_stringn (str, lenp);
  else
    return scm_to_stringn (str, lenp, scm_i_symbol_chars (pt->encoding),
                           scm_i_string_failed_conversion_handler
                           (SCM_PORT (port)->conversion_strategy));
}

/* Return a malloc(3)-allocated buffer containing the contents of STR encoded
   according to ENCODING.  If LENP is non-NULL, set it to the size in bytes of
   the returned buffer.  If the conversion to ENCODING fails, apply the strategy
   defined by HANDLER.  */
char *
scm_to_stringn (SCM str, size_t *lenp, const char *encoding,
                scm_t_string_failed_conversion_handler handler)
{
  char *buf;
  size_t ilen, len, i;
  int ret;

  if (!scm_is_string (str))
    scm_wrong_type_arg_msg (NULL, 0, str, "string");

  if (encoding == NULL)
    encoding = "ISO-8859-1";

  if (c_strcasecmp (encoding, "UTF-8") == 0)
    /* This is the most common case--e.g., when calling libc bindings
       while using a UTF-8 locale.  */
    return scm_to_utf8_stringn (str, lenp);

  ilen = scm_i_string_length (str);

  if (ilen == 0)
    {
      buf = scm_malloc (1);
      buf[0] = '\0';
      if (lenp)
        *lenp = 0;
      return buf;
    }

  if (lenp == NULL)
    for (i = 0; i < ilen; i++)
      if (scm_i_string_ref (str, i) == '\0')
        scm_misc_error (NULL,
                        "string contains #\\nul character: ~S",
                        scm_list_1 (str));

  if (scm_i_is_narrow_string (str)
      && c_strcasecmp (encoding, "ISO-8859-1") == 0)
    {
      /* If using native Latin-1 encoding, just copy the string
         contents.  */
      if (lenp)
        {
          buf = scm_malloc (ilen);
          memcpy (buf, scm_i_string_chars (str), ilen);
          *lenp = ilen;
          return buf;
        }
      else
        {
          buf = scm_malloc (ilen + 1);
          memcpy (buf, scm_i_string_chars (str), ilen);
          buf[ilen] = '\0';
          return buf;
        }
    }


  buf = NULL;
  len = 0;
  if (scm_i_is_narrow_string (str))
    {
      ret = mem_iconveh (scm_i_string_chars (str), ilen,
                         "ISO-8859-1", encoding,
                         (enum iconv_ilseq_handler) handler, NULL,
                         &buf, &len);

      if (ret != 0)
        scm_encoding_error (__func__, errno,
			    "cannot convert narrow string to output locale",
			    SCM_BOOL_F,
			    /* FIXME: Faulty character unknown.  */
			    SCM_BOOL_F);
    }
  else
    {
      buf = u32_conv_to_encoding (encoding,
                                  (enum iconv_ilseq_handler) handler,
                                  (uint32_t *) scm_i_string_wide_chars (str),
                                  ilen,
                                  NULL,
                                  NULL, &len);
      if (buf == NULL)
        scm_encoding_error (__func__, errno,
			    "cannot convert wide string to output locale",
			    SCM_BOOL_F,
			    /* FIXME: Faulty character unknown.  */
			    SCM_BOOL_F);
    }
  if (handler == SCM_FAILED_CONVERSION_ESCAPE_SEQUENCE)
    {
      if (SCM_R6RS_ESCAPES_P)
	{
	  /* The worst case is if the input string contains all 4-digit
	     hex escapes.  "\uXXXX" (six characters) becomes "\xXXXX;"
	     (seven characters).  Make BUF large enough to hold
	     that.  */
	  buf = scm_realloc (buf, (len * 7) / 6 + 1);
	  unistring_escapes_to_r6rs_escapes (buf, &len);
	}
      else
        unistring_escapes_to_guile_escapes (buf, &len);

      buf = scm_realloc (buf, len);
    }
  if (lenp)
    *lenp = len;
  else
    {
      buf = scm_realloc (buf, len + 1);
      buf[len] = '\0';
    }

  scm_remember_upto_here_1 (str);
  return buf;
}

size_t
scm_to_locale_stringbuf (SCM str, char *buf, size_t max_len)
{
  size_t len, copy_len;
  char *result = NULL;
  if (!scm_is_string (str))
    scm_wrong_type_arg_msg (NULL, 0, str, "string");
  result = scm_to_locale_stringn (str, &len);

  copy_len = (len > max_len) ? max_len : len;
  if (copy_len != 0)
    /* Some users of 'scm_to_locale_stringbuf' may pass NULL for buf
       when max_len is zero, and yet we must avoid passing NULL to
       memcpy to avoid undefined behavior. */
    memcpy (buf, result, copy_len);
  free (result);

  scm_remember_upto_here_1 (str);
  return len;
}


/* Unicode string normalization.  */

/* This function is a partial clone of SCM_STRING_TO_U32_BUF from 
   libguile/i18n.c.  It would be useful to have this factored out into a more
   convenient location, but its use of alloca makes that tricky to do. */

static SCM 
normalize_str (SCM string, uninorm_t form)
{
  SCM ret;
  uint32_t *w_str;
  uint32_t *w_norm_str;
  scm_t_wchar *cbuf;
  int malloc_p;
  size_t norm_len, len = scm_i_string_length (string);
  
  if (scm_i_is_narrow_string (string))
    {
      size_t i, bytes;
      const char *buf = scm_i_string_chars (string);

      bytes = (len + 1) * sizeof (scm_t_wchar);
      malloc_p = (bytes > SCM_MAX_ALLOCA);
      w_str = malloc_p ? malloc (bytes) : alloca (bytes);

      for (i = 0; i < len; i ++)
	w_str[i] = (unsigned char) buf[i];
      w_str[len] = 0;
    }
  else
    {
      malloc_p = 0;
      w_str = (uint32_t *) scm_i_string_wide_chars (string);
    }

  w_norm_str = u32_normalize (form, w_str, len, NULL, &norm_len);

  ret = scm_i_make_wide_string (norm_len, &cbuf, 0);
  u32_cpy ((uint32_t *) cbuf, w_norm_str, norm_len);
  free (w_norm_str);
  if (malloc_p)
    free (w_str);

  scm_i_try_narrow_string (ret);

  return ret;
}

SCM_DEFINE (scm_string_normalize_nfc, "string-normalize-nfc", 1, 0, 0,
	    (SCM string),
	    "Returns the NFC normalized form of @var{string}.")
#define FUNC_NAME s_scm_string_normalize_nfc
{
  SCM_VALIDATE_STRING (1, string);
  return normalize_str (string, UNINORM_NFC);
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_normalize_nfd, "string-normalize-nfd", 1, 0, 0,
	    (SCM string),
	    "Returns the NFD normalized form of @var{string}.")
#define FUNC_NAME s_scm_string_normalize_nfd
{
  SCM_VALIDATE_STRING (1, string);
  return normalize_str (string, UNINORM_NFD);
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_normalize_nfkc, "string-normalize-nfkc", 1, 0, 0,
	    (SCM string),
	    "Returns the NFKC normalized form of @var{string}.")
#define FUNC_NAME s_scm_string_normalize_nfkc
{
  SCM_VALIDATE_STRING (1, string);
  return normalize_str (string, UNINORM_NFKC);
}
#undef FUNC_NAME

SCM_DEFINE (scm_string_normalize_nfkd, "string-normalize-nfkd", 1, 0, 0,
	    (SCM string),
	    "Returns the NFKD normalized form of @var{string}.")
#define FUNC_NAME s_scm_string_normalize_nfkd
{
  SCM_VALIDATE_STRING (1, string);
  return normalize_str (string, UNINORM_NFKD);
}
#undef FUNC_NAME

/* converts C scm_array of strings to SCM scm_list of strings.
   If argc < 0, a null terminated scm_array is assumed.
   The current locale encoding is assumed */
SCM
scm_makfromstrs (int argc, char **argv)
{
  int i = argc;
  SCM lst = SCM_EOL;
  if (0 > i)
    for (i = 0; argv[i]; i++);
  while (i--)
    lst = scm_cons (scm_from_locale_string (argv[i]), lst);
  return lst;
}

/* Return a newly allocated array of char pointers to each of the strings
   in args, with a terminating NULL pointer.  The strings are encoded using
   the current locale. */

char **
scm_i_allocate_string_pointers (SCM list)
#define FUNC_NAME "scm_i_allocate_string_pointers"
{
  char **result;
  int list_len = scm_ilength (list);
  int i;

  if (list_len < 0)
    scm_wrong_type_arg_msg (NULL, 0, list, "proper list");

  result = scm_gc_malloc ((list_len + 1) * sizeof (char *),
			  "string pointers");
  result[list_len] = NULL;

  /* The list might have been modified in another thread, so
     we check LIST before each access.
   */
  for (i = 0; i < list_len && scm_is_pair (list); i++)
    {
      SCM str = SCM_CAR (list);
      size_t len;  /* String length in bytes */
      char *c_str = scm_to_locale_stringn (str, &len);

      /* OPTIMIZE-ME: Right now, scm_to_locale_stringn always uses
	 scm_malloc to allocate the returned string, which must be
	 explicitly deallocated.  This forces us to copy the string a
	 second time into a new buffer.  Ideally there would be variants
	 of scm_to_*_stringn that can return garbage-collected buffers. */

      result[i] = scm_gc_malloc_pointerless (len + 1, "string");
      memcpy (result[i], c_str, len);
      result[i][len] = '\0';
      free (c_str);

      list = SCM_CDR (list);
    }

  return result;
}
#undef FUNC_NAME

void
scm_i_get_substring_spec (size_t len,
			  SCM start, size_t *cstart,
			  SCM end, size_t *cend)
{
  if (SCM_UNBNDP (start))
    *cstart = 0;
  else
    *cstart = scm_to_unsigned_integer (start, 0, len);

  if (SCM_UNBNDP (end))
    *cend = len;
  else
    *cend = scm_to_unsigned_integer (end, *cstart, len);
}
		  
SCM_VECTOR_IMPLEMENTATION (SCM_ARRAY_ELEMENT_TYPE_CHAR, scm_make_string)

void
scm_init_strings ()
{
  scm_nullstr = scm_i_make_string (0, NULL, 0);

#include "strings.x"
}

