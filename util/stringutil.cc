// Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an
// additional permission to link the program and your derivative works
// with the separately licensed software that they have included with
// MySQL.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of <MySQL Product>, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

/**
  @file  stringutil.c
  @brief String utility functions, mostly focused on SQLWCHAR and charset
         manipulations.
*/


#include "stringutil.h"


CHARSET_INFO *utf8_charset_info = NULL;
CHARSET_INFO *utf16_charset_info = NULL;


/**
  Duplicate a SQLCHAR in the specified character set as a SQLWCHAR.

  @param[in]      charset_info  Character set to convert into
  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in bytes) or
                                destination string (in chars)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Pointer to a newly allocated SQLWCHAR, or @c NULL
*/
SQLWCHAR *sqlchar_as_sqlwchar(CHARSET_INFO *charset_info, SQLCHAR *str,
                              SQLINTEGER *len, uint *errors)
{
  SQLCHAR *pos, *str_end;
  SQLWCHAR *out;
  SQLINTEGER i, out_bytes;
  my_bool free_str= 0;

  if (str && *len == SQL_NTS)
  {
    *len= strlen((char *)str);
  }
  if (!str || *len == 0)
  {
    *len= 0;
    return NULL;
  }

  if (!is_utf8_charset(charset_info->number))
  {
    uint32 used_bytes, used_chars;
    size_t u8_max= (*len / charset_info->mbminlen *
                    utf8_charset_info->mbmaxlen + 1);
    SQLCHAR *u8= (SQLCHAR *)myodbc_malloc(u8_max, MYF(0));

    if (!u8)
    {
      *len= -1;
      return NULL;
    }

    *len= copy_and_convert((char *)u8, u8_max, utf8_charset_info,
                           (char *)str, *len, charset_info,
                           &used_bytes, &used_chars, errors);
    str= u8;
    free_str= 1;
  }

  str_end= str + *len;

  out_bytes= (*len + 1) * sizeof(SQLWCHAR);

  out= (SQLWCHAR *)myodbc_malloc(out_bytes, MYF(0));
  if (!out)
  {
    *len= -1;
    return NULL;
  }

  for (pos= str, i= 0; pos < str_end && *pos != 0; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      int consumed= utf8toutf32(pos, (UTF32 *)(out + i++));
      pos+= consumed;
      if (consumed == 0)
      {
        *errors+= 1;
        break;
      }
    }
    else
    {
      UTF32 u32;
      int consumed= utf8toutf32(pos, &u32);
      pos+= consumed;
      if (consumed == 0)
      {
        *errors+= 1;
        break;
      }
      i+= utf32toutf16(u32, (UTF16 *)(out + i));
    }
  }

  *len= i;
  out[i]= 0;

  if (free_str)
  {
    x_free(str);
  }

  return out;
}


/**
  Duplicate a SQLWCHAR as a SQLCHAR in the specified character set.

  @param[in]      charset_info  Character set to convert into
  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlwchar_as_sqlchar(CHARSET_INFO *charset_info, SQLWCHAR *str,
                             SQLINTEGER *len, uint *errors)
{
  SQLWCHAR *str_end;
  SQLCHAR *out;
  SQLINTEGER i, u8_len, out_bytes;
  UTF8 u8[MAX_BYTES_PER_UTF8_CP + 1];
  uint32 used_bytes, used_chars;

  *errors= 0;

  if (is_utf8_charset(charset_info->number))
    return sqlwchar_as_utf8(str, len);

  if (*len == SQL_NTS)
    *len= sqlwcharlen(str);
  if (!str || *len == 0)
  {
    *len= 0;
    return NULL;
  }

  out_bytes= *len * charset_info->mbmaxlen * sizeof(SQLCHAR) + 1;
  out= (SQLCHAR *)myodbc_malloc(out_bytes, MYF(0));
  if (!out)
  {
    *len= -1;
    return NULL;
  }

  str_end= str + *len;

  for (i= 0; str < str_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      u8_len= utf32toutf8((UTF32)*str++, u8);
    }
    else
    {
      UTF32 u32;
      int consumed= utf16toutf32((UTF16 *)str, &u32);
      str+= consumed;
      if (!consumed)
      {
        *errors+= 1;
        break;
      }
      u8_len= utf32toutf8(u32, u8);
    }

    i+= copy_and_convert((char *)out + i, out_bytes - i, charset_info,
                         (char *)u8, u8_len, utf8_charset_info, &used_bytes,
                         &used_chars, errors);
  }

  *len= i;
  out[i]= '\0';
  return out;
}


/**
  A bit extended version of sqlwchar_as_utf8 to be used by it and in other
  places where sqlwchar_as_utf8 could not be used

  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes).
  @param[in]      buff          Buffer to put the result string if it fits
  @param[in]      buff_max      max size(in bytes) of the buff.
  @param[out]     utf8mb4_used  has 4 bytes utf8 characters been used

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlwchar_as_utf8_ext(const SQLWCHAR *str, SQLINTEGER *len,
                              SQLCHAR *buff, uint buff_max, int *utf8mb4_used)
{
  const SQLWCHAR *str_end;
  UTF8 *u8;
  int utf8len, dummy;
  SQLINTEGER i;

  if (!str || *len <= 0)
  {
    *len= 0;
    return buff;
  }

  if (utf8mb4_used == NULL)
  {
    utf8mb4_used= &dummy;
  }

  if (buff == NULL || buff_max < (uint)(*len * MAX_BYTES_PER_UTF8_CP))
  {
    u8= (UTF8 *)myodbc_malloc(sizeof(UTF8) * MAX_BYTES_PER_UTF8_CP * *len + 1,
                        MYF(0));
  }
  else
  {
    u8= buff;
  }

  if (!u8)
  {
    *len= -1;
    return NULL;
  }

  str_end= str + *len;

  if (sizeof(SQLWCHAR) == 4)
  {
    for (i= 0; str < str_end; )
    {
      i+= (utf8len= utf32toutf8((UTF32)*str++, u8 + i));

      /*
        utf8mb4 is a superset of utf8, only supplemental characters
        which require four bytes differs in storage characteristics (length)
        between utf8 and utf8mb4.
      */
      if (utf8len == 4)
      {
        *utf8mb4_used= 1;
      }
    }
  }
  else
  {
    for (i= 0; str < str_end; )
    {
      UTF32 u32;
      int consumed= utf16toutf32((UTF16 *)str, &u32);
      if (!consumed)
      {
        break;
      }

      str+= consumed;

      i+= (utf8len= utf32toutf8(u32, u8 + i));
      /*
        utf8mb4 is a superset of utf8, only supplemental characters
        which require four bytes differs in storage characteristics (length)
        between utf8 and utf8mb4.
      */
      if (utf8len == 4)
      {
        *utf8mb4_used= 1;
      }
    }
  }

  *len= i;
  return u8;
}


/**
  Duplicate a SQLWCHAR as a SQLCHAR encoded as UTF-8.

  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes)

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlwchar_as_utf8(const SQLWCHAR *str, SQLINTEGER *len)
{
  SQLCHAR *res;

  if (*len == SQL_NTS)
  {
    *len= sqlwcharlen(str);
  }

  if (!str || *len <= 0)
  {
    *len= 0;
    return NULL;
  }

  res= sqlwchar_as_utf8_ext(str, len, NULL, 0, NULL);

  /* If we could not allocate memory */
  if (res != NULL)
  {
    res[*len]= '\0';
  }

  return res;
}


SQLCHAR* sqlwchar_as_utf8_simple(SQLWCHAR *s)
{
  SQLINTEGER len= SQL_NTS;
  return sqlwchar_as_utf8(s, &len);
}


/**
  Convert a SQLCHAR encoded as UTF-8 into a SQLWCHAR.

  @param[out]     out           Pointer to SQLWCHAR buffer
  @param[in]      out_max       Length of @c out buffer
  @param[in]      in            Pointer to SQLCHAR string (utf-8 encoded)
  @param[in]      in_len        Length of @c in (in bytes)

  @return  Number of characters stored in the @c out buffer
*/
SQLSMALLINT utf8_as_sqlwchar(SQLWCHAR *out, SQLINTEGER out_max, SQLCHAR *in,
                             SQLINTEGER in_len)
{
  SQLINTEGER i;
  SQLWCHAR *pos, *out_end;

  for (i= 0, pos= out, out_end= out + out_max; i < in_len && pos < out_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      int consumed= utf8toutf32(in + i, (UTF32 *)pos++);
      i+= consumed;
      if (!consumed)
        break;
    }
    else
    {
      UTF32 u32;
      int consumed= utf8toutf32(in + i, &u32);
      i+= consumed;
      if (!consumed)
        break;
      pos+= utf32toutf16(u32, (UTF16 *)pos);
    }
  }

  if (pos)
    *pos= 0;
  return pos - out;
}


/**
  Duplicate a SQLCHAR as a SQLCHAR in the specified character set.

  @param[in]      from_charset  Character set to convert from
  @param[in]      to_charset    Character set to convert into
  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlchar_as_sqlchar(CHARSET_INFO *from_charset,
                            CHARSET_INFO *to_charset,
                            SQLCHAR *str, SQLINTEGER *len, uint *errors)
{
  uint32 used_bytes, used_chars, bytes;
  SQLCHAR *conv;

  if (*len == SQL_NTS)
    *len= strlen((char *)str);

  bytes= (*len / from_charset->mbminlen * to_charset->mbmaxlen);
  conv= (SQLCHAR *)myodbc_malloc(bytes + 1, MYF(0));
  if (!conv)
  {
    *len= -1;
    return NULL;
  }

  *len= copy_and_convert((char *)conv, bytes, to_charset,
                         (char *)str, *len,
                         from_charset, &used_bytes,
                         &used_chars, errors);

  conv[*len]= '\0';

  return conv;
}


/**
  Convert a SQLWCHAR to a SQLCHAR in the specified character set. This
  variation uses a pre-allocated buffer.

  @param[in]      charset_info  Character set to convert into
  @param[out]     out           Pointer to SQLWCHAR buffer
  @param[in]      out_bytes     Length of @c out buffer
  @param[in]      str           String to convert
  @param[in]      len           Length of @c in (in SQLWCHAR's)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Number of characters stored in the @c out buffer
*/
SQLINTEGER sqlwchar_as_sqlchar_buf(CHARSET_INFO *charset_info,
                                   SQLCHAR *out, SQLINTEGER out_bytes,
                                   SQLWCHAR *str, SQLINTEGER len, uint *errors)
{
  SQLWCHAR *str_end;
  SQLINTEGER i, u8_len;
  UTF8 u8[MAX_BYTES_PER_UTF8_CP + 1];
  uint32 used_bytes, used_chars;

  *errors= 0;

  if (len == SQL_NTS)
    len= sqlwcharlen(str);
  if (!str || len == 0)
    return 0;

  str_end= str + len;

  for (i= 0; str < str_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      u8_len= utf32toutf8((UTF32)*str++, u8);
    }
    else
    {
      UTF32 u32;
      int consumed= utf16toutf32((UTF16 *)str, &u32);
      str+= consumed;
      if (!consumed)
      {
        *errors+= 1;
        break;
      }
      u8_len= utf32toutf8(u32, u8);
    }

    i+= copy_and_convert((char *)out + i, out_bytes - i, charset_info,
                         (char *)u8, u8_len, utf8_charset_info, &used_bytes,
                         &used_chars, errors);
  }

  out[i]= '\0';

  return i;
}


/**
  Copy a string from one character set to another. Taken from sql_string.cc
  in the MySQL Server source code, since we don't export this functionality
  in libmysql yet.

  @c to must be at least as big as @c from_length * @c to_cs->mbmaxlen

  @param[in,out] to           Store result here
  @param[in]     to_cs        Character set of result string
  @param[in]     from         Copy from here
  @param[in]     from_length  Length of string in @c from (in bytes)
  @param[in]     from_cs      Character set of string in @c from
  @param[out]    used_bytes   Buffer for returning number of bytes consumed
                              from @c from
  @param[out]    used_chars   Buffer for returning number of chars consumed
                              from @c from
  @param[in,out] errors       Pointer to value where number of errors
                              encountered is added.

  @retval Length of bytes copied to @c to
*/
uint32
copy_and_convert(char *to, uint32 to_length, CHARSET_INFO *to_cs,
                 const char *from, uint32 from_length, CHARSET_INFO *from_cs,
                 uint32 *used_bytes, uint32 *used_chars, uint *errors)
{
  int         from_cnvres, to_cnvres;
  my_wc_t     wc;
  const uchar *from_end= (const uchar*) from+from_length;
  char *to_start= to;
  uchar *to_end= (uchar*) to+to_length;
  auto mb_wc= from_cs->cset->mb_wc;
  auto wc_mb= to_cs->cset->wc_mb;
  uint error_count= 0;

  *used_bytes= *used_chars= 0;

  while (1)
  {
    if ((from_cnvres= (*mb_wc)(from_cs, &wc, (uchar*) from, from_end)) > 0)
      from+= from_cnvres;
    else if (from_cnvres == MY_CS_ILSEQ)
    {
      ++error_count;
      ++from;
      wc= '?';
    }
    else if (from_cnvres > MY_CS_TOOSMALL)
    {
      /*
        A correct multibyte sequence detected
        But it doesn't have Unicode mapping.
      */
      ++error_count;
      from+= (-from_cnvres);
      wc= '?';
    }
    else
      break; /* Not enough characters */

outp:
    if ((to_cnvres= (*wc_mb)(to_cs, wc, (uchar*) to, to_end)) > 0)
      to+= to_cnvres;
    else if (to_cnvres == MY_CS_ILUNI && wc != '?')
    {
      ++error_count;
      wc= '?';
      goto outp;
    }
    else
      break;

    *used_bytes+= from_cnvres;
    *used_chars+= 1;
  }
  if (errors)
    *errors+= error_count;

  return (uint32) (to - to_start);
}




/*
 * Compare two SQLWCHAR strings ignoring case. This is only
 * case-insensitive over the ASCII range of characters.
 *
 * @return 0 if the strings are the same, 1 if they are not.
 */
int sqlwcharcasecmp(const SQLWCHAR *s1, const SQLWCHAR *s2)
{
  SQLWCHAR c1, c2;
  while (*s1 && *s2)
  {
    c1= *s1;
    c2= *s2;
    /* capitalize both strings */
    if (c1 >= 'a')
      c1 -= ('a' - 'A');
    if (c2 >= 'a')
      c2 -= ('a' - 'A');
    if (c1 != c2)
      return 1;
    ++s1;
    ++s2;
  }

  /* one will be null, so both must be */
  return *s1 != *s2;
}


/*
 * Locate a SQLWCHAR in a SQLWCHAR string.
 *
 * @return Position of char if found, otherwise NULL.
 */
const SQLWCHAR *sqlwcharchr(const SQLWCHAR *wstr, SQLWCHAR wchr)
{
  while (*wstr)
  {
    if (*wstr == wchr)
    {
      return wstr;
    }

    ++wstr;
  }

  return NULL;
}


/*
 * Calculate the length of a SQLWCHAR string.
 *
 * @return The number of SQLWCHAR units in the string.
 */
size_t sqlwcharlen(const SQLWCHAR *wstr)
{
  size_t len= 0;
  while (wstr && *wstr++)
    ++len;
  return len;
}


/*
 * Duplicate a SQLWCHAR string. Memory is allocated with myodbc_malloc()
 * and should be freed with my_free() or the x_free() macro.
 *
 * @return A pointer to a new string.
 */
SQLWCHAR *sqlwchardup(const SQLWCHAR *wstr, size_t charlen)
{
  size_t chars= charlen == SQL_NTS ? sqlwcharlen(wstr) : charlen;
  SQLWCHAR *res= (SQLWCHAR *)myodbc_malloc((chars + 1) * sizeof(SQLWCHAR), MYF(0));
  if (!res)
    return NULL;
  memcpy(res, wstr, chars * sizeof(SQLWCHAR));
  res[chars]= 0;
  return res;
}


/*
 * Convert a SQLWCHAR string to a long integer.
 *
 * @return The integer result of the conversion or 0 if the
 *         string could not be parsed.
 */
unsigned long sqlwchartoul(const SQLWCHAR *wstr, const SQLWCHAR **endptr){
  unsigned long res= 0;
  SQLWCHAR c;

  if (!wstr)
    return 0;

  while (c= *wstr)
  {
    if (c < '0' || c > '9')
      break;
    res*= 10;
    res+= c - '0';
    ++wstr;
  }

  if (endptr)
    *endptr= wstr;

  return res;
}


/*
 * Convert a long integer to a SQLWCHAR string.
 */
void sqlwcharfromul(SQLWCHAR *wstr, unsigned long v)
{
  int chars;
  unsigned long v1;
  for(chars= 0, v1= v; v1 > 0; ++chars, v1 /= 10);
  wstr[chars]= (SQLWCHAR)0;
  for(v1= v; v1 > 0; v1 /= 10)
    wstr[--chars]= (SQLWCHAR)('0' + (v1 % 10));
}


/*
 * Concatenate two strings. This differs from the convential
 * strncat() in that the parameter n is reduced by the number
 * of characters used.
 *
 * Returns the number of characters copied.
 */
size_t sqlwcharncat2(SQLWCHAR *dest, const SQLWCHAR *src, size_t *n)
{
  SQLWCHAR *orig_dest;
  if (!n || !*n)
    return 0;
  orig_dest= (dest += sqlwcharlen(dest));
  while (*src && *n && (*n)--)
    *dest++= *src++;
  if (*n)
    *dest= 0;
  else
    *(dest - 1)= 0;
  return dest - orig_dest;
}


/*
 * Copy up to 'n' characters (including NULL) from src to dest.
 */
SQLWCHAR *sqlwcharncpy(SQLWCHAR *dest, const SQLWCHAR *src, size_t n)
{
  if (!dest || !src)
    return NULL;
  while (*src && n--)
    *dest++= *src++;
  if (n)
    *dest= 0;
  else
    *(dest - 1)= 0;
  return dest;
}


/* Converts all characters in string to lowercase */
char * myodbc_strlwr(char *target, size_t len)
{
  unsigned char *c= (unsigned char *)target;
  if (-1 == len)
    len= (int)strlen(target);

  while (len-- > 0)
  {
    *c= tolower(*c);
    ++c;
  }

  return target;
}

struct CHARSET_COLLATION_INFO charset_collation_info[] = {
  {0, "NONE", "NONE", 0},
  {1, "big5", "big5_chinese_ci", 2},
  {2, "latin2", "latin2_czech_cs", 1},
  {3, "dec8", "dec8_swedish_ci", 1},
  {4, "cp850", "cp850_general_ci", 1},
  {5, "latin1", "latin1_german1_ci", 1},
  {6, "hp8", "hp8_english_ci", 1},
  {7, "koi8r", "koi8r_general_ci", 1},
  {8, "latin1", "latin1_swedish_ci", 1},
  {9, "latin2", "latin2_general_ci", 1},
  {10, "swe7", "swe7_swedish_ci", 1},
  {11, "ascii", "ascii_general_ci", 1},
  {12, "ujis", "ujis_japanese_ci", 3},
  {13, "sjis", "sjis_japanese_ci", 2},
  {14, "cp1251", "cp1251_bulgarian_ci", 1},
  {15, "latin1", "latin1_danish_ci", 1},
  {16, "hebrew", "hebrew_general_ci", 1},
  {17, "NONE", "NONE", 0},
  {18, "tis620", "tis620_thai_ci", 1},
  {19, "euckr", "euckr_korean_ci", 2},
  {20, "latin7", "latin7_estonian_cs", 1},
  {21, "latin2", "latin2_hungarian_ci", 1},
  {22, "koi8u", "koi8u_general_ci", 1},
  {23, "cp1251", "cp1251_ukrainian_ci", 1},
  {24, "gb2312", "gb2312_chinese_ci", 2},
  {25, "greek", "greek_general_ci", 1},
  {26, "cp1250", "cp1250_general_ci", 1},
  {27, "latin2", "latin2_croatian_ci", 1},
  {28, "gbk", "gbk_chinese_ci", 2},
  {29, "cp1257", "cp1257_lithuanian_ci", 1},
  {30, "latin5", "latin5_turkish_ci", 1},
  {31, "latin1", "latin1_german2_ci", 1},
  {32, "armscii8", "armscii8_general_ci", 1},
  {33, "utf8", "utf8_general_ci", 3},
  {34, "cp1250", "cp1250_czech_cs", 1},
  {35, "ucs2", "ucs2_general_ci", 2},
  {36, "cp866", "cp866_general_ci", 1},
  {37, "keybcs2", "keybcs2_general_ci", 1},
  {38, "macce", "macce_general_ci", 1},
  {39, "macroman", "macroman_general_ci", 1},
  {40, "cp852", "cp852_general_ci", 1},
  {41, "latin7", "latin7_general_ci", 1},
  {42, "latin7", "latin7_general_cs", 1},
  {43, "macce", "macce_bin", 1},
  {44, "cp1250", "cp1250_croatian_ci", 1},
  {45, "utf8mb4", "utf8mb4_general_ci", 4},
  {46, "utf8mb4", "utf8mb4_bin", 4},
  {47, "latin1", "latin1_bin", 1},
  {48, "latin1", "latin1_general_ci", 1},
  {49, "latin1", "latin1_general_cs", 1},
  {50, "cp1251", "cp1251_bin", 1},
  {51, "cp1251", "cp1251_general_ci", 1},
  {52, "cp1251", "cp1251_general_cs", 1},
  {53, "macroman", "macroman_bin", 1},
  {54, "utf16", "utf16_general_ci", 4},
  {55, "utf16", "utf16_bin", 4},
  {56, "utf16le", "utf16le_general_ci", 4},
  {57, "cp1256", "cp1256_general_ci", 1},
  {58, "cp1257", "cp1257_bin", 1},
  {59, "cp1257", "cp1257_general_ci", 1},
  {60, "utf32", "utf32_general_ci", 4},
  {61, "utf32", "utf32_bin", 4},
  {62, "utf16le", "utf16le_bin", 4},
  {63, "binary", "binary", 1},
  {64, "armscii8", "armscii8_bin", 1},
  {65, "ascii", "ascii_bin", 1},
  {66, "cp1250", "cp1250_bin", 1},
  {67, "cp1256", "cp1256_bin", 1},
  {68, "cp866", "cp866_bin", 1},
  {69, "dec8", "dec8_bin", 1},
  {70, "greek", "greek_bin", 1},
  {71, "hebrew", "hebrew_bin", 1},
  {72, "hp8", "hp8_bin", 1},
  {73, "keybcs2", "keybcs2_bin", 1},
  {74, "koi8r", "koi8r_bin", 1},
  {75, "koi8u", "koi8u_bin", 1},
  {76, "utf8", "utf8_tolower_ci", 3},
  {77, "latin2", "latin2_bin", 1},
  {78, "latin5", "latin5_bin", 1},
  {79, "latin7", "latin7_bin", 1},
  {80, "cp850", "cp850_bin", 1},
  {81, "cp852", "cp852_bin", 1},
  {82, "swe7", "swe7_bin", 1},
  {83, "utf8", "utf8_bin", 3},
  {84, "big5", "big5_bin", 2},
  {85, "euckr", "euckr_bin", 2},
  {86, "gb2312", "gb2312_bin", 2},
  {87, "gbk", "gbk_bin", 2},
  {88, "sjis", "sjis_bin", 2},
  {89, "tis620", "tis620_bin", 1},
  {90, "ucs2", "ucs2_bin", 2},
  {91, "ujis", "ujis_bin", 3},
  {92, "geostd8", "geostd8_general_ci", 1},
  {93, "geostd8", "geostd8_bin", 1},
  {94, "latin1", "latin1_spanish_ci", 1},
  {95, "cp932", "cp932_japanese_ci", 2},
  {96, "cp932", "cp932_bin", 2},
  {97, "eucjpms", "eucjpms_japanese_ci", 3},
  {98, "eucjpms", "eucjpms_bin", 3},
  {99, "cp1250", "cp1250_polish_ci", 1},
  {100, "NONE", "NONE", 0},
  {101, "utf16", "utf16_unicode_ci", 4},
  {102, "utf16", "utf16_icelandic_ci", 4},
  {103, "utf16", "utf16_latvian_ci", 4},
  {104, "utf16", "utf16_romanian_ci", 4},
  {105, "utf16", "utf16_slovenian_ci", 4},
  {106, "utf16", "utf16_polish_ci", 4},
  {107, "utf16", "utf16_estonian_ci", 4},
  {108, "utf16", "utf16_spanish_ci", 4},
  {109, "utf16", "utf16_swedish_ci", 4},
  {110, "utf16", "utf16_turkish_ci", 4},
  {111, "utf16", "utf16_czech_ci", 4},
  {112, "utf16", "utf16_danish_ci", 4},
  {113, "utf16", "utf16_lithuanian_ci", 4},
  {114, "utf16", "utf16_slovak_ci", 4},
  {115, "utf16", "utf16_spanish2_ci", 4},
  {116, "utf16", "utf16_roman_ci", 4},
  {117, "utf16", "utf16_persian_ci", 4},
  {118, "utf16", "utf16_esperanto_ci", 4},
  {119, "utf16", "utf16_hungarian_ci", 4},
  {120, "utf16", "utf16_sinhala_ci", 4},
  {121, "utf16", "utf16_german2_ci", 4},
  {122, "utf16", "utf16_croatian_ci", 4},
  {123, "utf16", "utf16_unicode_520_ci", 4},
  {124, "utf16", "utf16_vietnamese_ci", 4},
  {125, "NONE", "NONE", 0},
  {126, "NONE", "NONE", 0},
  {127, "NONE", "NONE", 0},
  {128, "ucs2", "ucs2_unicode_ci", 2},
  {129, "ucs2", "ucs2_icelandic_ci", 2},
  {130, "ucs2", "ucs2_latvian_ci", 2},
  {131, "ucs2", "ucs2_romanian_ci", 2},
  {132, "ucs2", "ucs2_slovenian_ci", 2},
  {133, "ucs2", "ucs2_polish_ci", 2},
  {134, "ucs2", "ucs2_estonian_ci", 2},
  {135, "ucs2", "ucs2_spanish_ci", 2},
  {136, "ucs2", "ucs2_swedish_ci", 2},
  {137, "ucs2", "ucs2_turkish_ci", 2},
  {138, "ucs2", "ucs2_czech_ci", 2},
  {139, "ucs2", "ucs2_danish_ci", 2},
  {140, "ucs2", "ucs2_lithuanian_ci", 2},
  {141, "ucs2", "ucs2_slovak_ci", 2},
  {142, "ucs2", "ucs2_spanish2_ci", 2},
  {143, "ucs2", "ucs2_roman_ci", 2},
  {144, "ucs2", "ucs2_persian_ci", 2},
  {145, "ucs2", "ucs2_esperanto_ci", 2},
  {146, "ucs2", "ucs2_hungarian_ci", 2},
  {147, "ucs2", "ucs2_sinhala_ci", 2},
  {148, "ucs2", "ucs2_german2_ci", 2},
  {149, "ucs2", "ucs2_croatian_ci", 2},
  {150, "ucs2", "ucs2_unicode_520_ci", 2},
  {151, "ucs2", "ucs2_vietnamese_ci", 2},
  {152, "NONE", "NONE", 0},
  {153, "NONE", "NONE", 0},
  {154, "NONE", "NONE", 0},
  {155, "NONE", "NONE", 0},
  {156, "NONE", "NONE", 0},
  {157, "NONE", "NONE", 0},
  {158, "NONE", "NONE", 0},
  {159, "ucs2", "ucs2_general_mysql500_ci", 2},
  {160, "utf32", "utf32_unicode_ci", 4},
  {161, "utf32", "utf32_icelandic_ci", 4},
  {162, "utf32", "utf32_latvian_ci", 4},
  {163, "utf32", "utf32_romanian_ci", 4},
  {164, "utf32", "utf32_slovenian_ci", 4},
  {165, "utf32", "utf32_polish_ci", 4},
  {166, "utf32", "utf32_estonian_ci", 4},
  {167, "utf32", "utf32_spanish_ci", 4},
  {168, "utf32", "utf32_swedish_ci", 4},
  {169, "utf32", "utf32_turkish_ci", 4},
  {170, "utf32", "utf32_czech_ci", 4},
  {171, "utf32", "utf32_danish_ci", 4},
  {172, "utf32", "utf32_lithuanian_ci", 4},
  {173, "utf32", "utf32_slovak_ci", 4},
  {174, "utf32", "utf32_spanish2_ci", 4},
  {175, "utf32", "utf32_roman_ci", 4},
  {176, "utf32", "utf32_persian_ci", 4},
  {177, "utf32", "utf32_esperanto_ci", 4},
  {178, "utf32", "utf32_hungarian_ci", 4},
  {179, "utf32", "utf32_sinhala_ci", 4},
  {180, "utf32", "utf32_german2_ci", 4},
  {181, "utf32", "utf32_croatian_ci", 4},
  {182, "utf32", "utf32_unicode_520_ci", 4},
  {183, "utf32", "utf32_vietnamese_ci", 4},
  {184, "NONE", "NONE", 0},
  {185, "NONE", "NONE", 0},
  {186, "NONE", "NONE", 0},
  {187, "NONE", "NONE", 0},
  {188, "NONE", "NONE", 0},
  {189, "NONE", "NONE", 0},
  {190, "NONE", "NONE", 0},
  {191, "NONE", "NONE", 0},
  {192, "utf8", "utf8_unicode_ci", 3},
  {193, "utf8", "utf8_icelandic_ci", 3},
  {194, "utf8", "utf8_latvian_ci", 3},
  {195, "utf8", "utf8_romanian_ci", 3},
  {196, "utf8", "utf8_slovenian_ci", 3},
  {197, "utf8", "utf8_polish_ci", 3},
  {198, "utf8", "utf8_estonian_ci", 3},
  {199, "utf8", "utf8_spanish_ci", 3},
  {200, "utf8", "utf8_swedish_ci", 3},
  {201, "utf8", "utf8_turkish_ci", 3},
  {202, "utf8", "utf8_czech_ci", 3},
  {203, "utf8", "utf8_danish_ci", 3},
  {204, "utf8", "utf8_lithuanian_ci", 3},
  {205, "utf8", "utf8_slovak_ci", 3},
  {206, "utf8", "utf8_spanish2_ci", 3},
  {207, "utf8", "utf8_roman_ci", 3},
  {208, "utf8", "utf8_persian_ci", 3},
  {209, "utf8", "utf8_esperanto_ci", 3},
  {210, "utf8", "utf8_hungarian_ci", 3},
  {211, "utf8", "utf8_sinhala_ci", 3},
  {212, "utf8", "utf8_german2_ci", 3},
  {213, "utf8", "utf8_croatian_ci", 3},
  {214, "utf8", "utf8_unicode_520_ci", 3},
  {215, "utf8", "utf8_vietnamese_ci", 3},
  {216, "NONE", "NONE", 0},
  {217, "NONE", "NONE", 0},
  {218, "NONE", "NONE", 0},
  {219, "NONE", "NONE", 0},
  {220, "NONE", "NONE", 0},
  {221, "NONE", "NONE", 0},
  {222, "NONE", "NONE", 0},
  {223, "utf8", "utf8_general_mysql500_ci", 3},
  {224, "utf8mb4", "utf8mb4_unicode_ci", 4},
  {225, "utf8mb4", "utf8mb4_icelandic_ci", 4},
  {226, "utf8mb4", "utf8mb4_latvian_ci", 4},
  {227, "utf8mb4", "utf8mb4_romanian_ci", 4},
  {228, "utf8mb4", "utf8mb4_slovenian_ci", 4},
  {229, "utf8mb4", "utf8mb4_polish_ci", 4},
  {230, "utf8mb4", "utf8mb4_estonian_ci", 4},
  {231, "utf8mb4", "utf8mb4_spanish_ci", 4},
  {232, "utf8mb4", "utf8mb4_swedish_ci", 4},
  {233, "utf8mb4", "utf8mb4_turkish_ci", 4},
  {234, "utf8mb4", "utf8mb4_czech_ci", 4},
  {235, "utf8mb4", "utf8mb4_danish_ci", 4},
  {236, "utf8mb4", "utf8mb4_lithuanian_ci", 4},
  {237, "utf8mb4", "utf8mb4_slovak_ci", 4},
  {238, "utf8mb4", "utf8mb4_spanish2_ci", 4},
  {239, "utf8mb4", "utf8mb4_roman_ci", 4},
  {240, "utf8mb4", "utf8mb4_persian_ci", 4},
  {241, "utf8mb4", "utf8mb4_esperanto_ci", 4},
  {242, "utf8mb4", "utf8mb4_hungarian_ci", 4},
  {243, "utf8mb4", "utf8mb4_sinhala_ci", 4},
  {244, "utf8mb4", "utf8mb4_german2_ci", 4},
  {245, "utf8mb4", "utf8mb4_croatian_ci", 4},
  {246, "utf8mb4", "utf8mb4_unicode_520_ci", 4},
  {247, "utf8mb4", "utf8mb4_vietnamese_ci", 4},
  {248, "gb18030", "gb18030_chinese_ci", 4},
  {249, "gb18030", "gb18030_bin", 4},
  {250, "gb18030", "gb18030_unicode_520_ci", 4},
  {251, "NONE", "NONE", 0},
  {252, "NONE", "NONE", 0},
  {253, "NONE", "NONE", 0},
  {254, "NONE", "NONE", 0},
  {255, "utf8mb4", "utf8mb4_0900_ai_ci", 4},
  {256, "utf8mb4", "utf8mb4_de_pb_0900_ai_ci", 4},
  {257, "utf8mb4", "utf8mb4_is_0900_ai_ci", 4},
  {258, "utf8mb4", "utf8mb4_lv_0900_ai_ci", 4},
  {259, "utf8mb4", "utf8mb4_ro_0900_ai_ci", 4},
  {260, "utf8mb4", "utf8mb4_sl_0900_ai_ci", 4},
  {261, "utf8mb4", "utf8mb4_pl_0900_ai_ci", 4},
  {262, "utf8mb4", "utf8mb4_et_0900_ai_ci", 4},
  {263, "utf8mb4", "utf8mb4_es_0900_ai_ci", 4},
  {264, "utf8mb4", "utf8mb4_sv_0900_ai_ci", 4},
  {265, "utf8mb4", "utf8mb4_tr_0900_ai_ci", 4},
  {266, "utf8mb4", "utf8mb4_cs_0900_ai_ci", 4},
  {267, "utf8mb4", "utf8mb4_da_0900_ai_ci", 4},
  {268, "utf8mb4", "utf8mb4_lt_0900_ai_ci", 4},
  {269, "utf8mb4", "utf8mb4_sk_0900_ai_ci", 4},
  {270, "utf8mb4", "utf8mb4_es_trad_0900_ai_ci", 4},
  {271, "utf8mb4", "utf8mb4_la_0900_ai_ci", 4},
  {272, "NONE", "NONE", 0},
  {273, "utf8mb4", "utf8mb4_eo_0900_ai_ci", 4},
  {274, "utf8mb4", "utf8mb4_hu_0900_ai_ci", 4},
  {275, "utf8mb4", "utf8mb4_hr_0900_ai_ci", 4},
  {276, "NONE", "NONE", 0},
  {277, "utf8mb4", "utf8mb4_vi_0900_ai_ci", 4},
  {278, "utf8mb4", "utf8mb4_0900_as_cs", 4},
  {279, "utf8mb4", "utf8mb4_de_pb_0900_as_cs", 4},
  {280, "utf8mb4", "utf8mb4_is_0900_as_cs", 4},
  {281, "utf8mb4", "utf8mb4_lv_0900_as_cs", 4},
  {282, "utf8mb4", "utf8mb4_ro_0900_as_cs", 4},
  {283, "utf8mb4", "utf8mb4_sl_0900_as_cs", 4},
  {284, "utf8mb4", "utf8mb4_pl_0900_as_cs", 4},
  {285, "utf8mb4", "utf8mb4_et_0900_as_cs", 4},
  {286, "utf8mb4", "utf8mb4_es_0900_as_cs", 4},
  {287, "utf8mb4", "utf8mb4_sv_0900_as_cs", 4},
  {288, "utf8mb4", "utf8mb4_tr_0900_as_cs", 4},
  {289, "utf8mb4", "utf8mb4_cs_0900_as_cs", 4},
  {290, "utf8mb4", "utf8mb4_da_0900_as_cs", 4},
  {291, "utf8mb4", "utf8mb4_lt_0900_as_cs", 4},
  {292, "utf8mb4", "utf8mb4_sk_0900_as_cs", 4},
  {293, "utf8mb4", "utf8mb4_es_trad_0900_as_cs", 4},
  {294, "utf8mb4", "utf8mb4_la_0900_as_cs", 4},
  {295, "NONE", "NONE", 0},
  {296, "utf8mb4", "utf8mb4_eo_0900_as_cs", 4},
  {297, "utf8mb4", "utf8mb4_hu_0900_as_cs", 4},
  {298, "utf8mb4", "utf8mb4_hr_0900_as_cs", 4},
  {299, "NONE", "NONE", 0},
  {300, "utf8mb4", "utf8mb4_vi_0900_as_cs", 4},
  {301, "NONE", "NONE", 0},
  {302, "NONE", "NONE", 0},
  {303, "utf8mb4", "utf8mb4_ja_0900_as_cs", 4},
  {304, "utf8mb4", "utf8mb4_ja_0900_as_cs_ks", 4},
  {305, "utf8mb4", "utf8mb4_0900_as_ci", 4},
  {306, "utf8mb4", "utf8mb4_ru_0900_ai_ci", 4},
  {307, "utf8mb4", "utf8mb4_ru_0900_as_cs", 4},
  {308, "utf8mb4", "utf8mb4_zh_0900_as_cs", 4},
  {309, "utf8mb4", "utf8mb4_0900_bin", 4},
};


unsigned int get_charset_maxlen(unsigned int num)
{
  if (num < 310)
    return charset_collation_info[num].maxlen;

  return 0;
}

/********************************
This charset mapping function is cut out from server repo: sql_common/client.c
*********************************/

typedef enum my_cs_match_type_enum
{
  /* MySQL and OS charsets are fully compatible */
  my_cs_exact,
  /* MySQL charset is very close to OS charset  */
  my_cs_approx,
  /*
    MySQL knows this charset, but it is not supported as client character set.
  */
  my_cs_unsupp
} my_cs_match_type;


typedef struct str2str_st
{
  const char        *os_name;
  const char        *my_name;
  my_cs_match_type  param;
} MY_CSET_OS_NAME;

static const MY_CSET_OS_NAME charsets[]=
{
#ifdef __WIN__
  {"cp437",          "cp850",    my_cs_approx},
  {"cp850",          "cp850",    my_cs_exact},
  {"cp852",          "cp852",    my_cs_exact},
  {"cp858",          "cp850",    my_cs_approx},
  {"cp866",          "cp866",    my_cs_exact},
  {"cp874",          "tis620",   my_cs_approx},
  {"cp932",          "cp932",    my_cs_exact},
  {"cp936",          "gbk",      my_cs_approx},
  {"cp949",          "euckr",    my_cs_approx},
  {"cp950",          "big5",     my_cs_exact},
  {"cp1200",         "utf16le",  my_cs_unsupp},
  {"cp1201",         "utf16",    my_cs_unsupp},
  {"cp1250",         "cp1250",   my_cs_exact},
  {"cp1251",         "cp1251",   my_cs_exact},
  {"cp1252",         "latin1",   my_cs_exact},
  {"cp1253",         "greek",    my_cs_exact},
  {"cp1254",         "latin5",   my_cs_exact},
  {"cp1255",         "hebrew",   my_cs_approx},
  {"cp1256",         "cp1256",   my_cs_exact},
  {"cp1257",         "cp1257",   my_cs_exact},
  {"cp10000",        "macroman", my_cs_exact},
  {"cp10001",        "sjis",     my_cs_approx},
  {"cp10002",        "big5",     my_cs_approx},
  {"cp10008",        "gb2312",   my_cs_approx},
  {"cp10021",        "tis620",   my_cs_approx},
  {"cp10029",        "macce",    my_cs_exact},
  {"cp12001",        "utf32",    my_cs_unsupp},
  {"cp20107",        "swe7",     my_cs_exact},
  {"cp20127",        "latin1",   my_cs_approx},
  {"cp20866",        "koi8r",    my_cs_exact},
  {"cp20932",        "ujis",     my_cs_exact},
  {"cp20936",        "gb2312",   my_cs_approx},
  {"cp20949",        "euckr",    my_cs_approx},
  {"cp21866",        "koi8u",    my_cs_exact},
  {"cp28591",        "latin1",   my_cs_approx},
  {"cp28592",        "latin2",   my_cs_exact},
  {"cp28597",        "greek",    my_cs_exact},
  {"cp28598",        "hebrew",   my_cs_exact},
  {"cp28599",        "latin5",   my_cs_exact},
  {"cp28603",        "latin7",   my_cs_exact},
#ifdef UNCOMMENT_THIS_WHEN_WL_4579_IS_DONE
  {"cp28605",        "latin9",   my_cs_exact},
#endif
  {"cp38598",        "hebrew",   my_cs_exact},
  {"cp51932",        "ujis",     my_cs_exact},
  {"cp51936",        "gb2312",   my_cs_exact},
  {"cp51949",        "euckr",    my_cs_exact},
  {"cp51950",        "big5",     my_cs_exact},
#ifdef UNCOMMENT_THIS_WHEN_WL_WL_4024_IS_DONE
  {"cp54936",        "gb18030",  my_cs_exact},
#endif
  {"cp65001",        "utf8",     my_cs_exact},

#else /* not Windows */

  {"646",            "latin1",   my_cs_approx}, /* Default on Solaris */
  {"ANSI_X3.4-1968", "latin1",   my_cs_approx},
  {"ansi1251",       "cp1251",   my_cs_exact},
  {"armscii8",       "armscii8", my_cs_exact},
  {"armscii-8",      "armscii8", my_cs_exact},
  {"ASCII",          "latin1",   my_cs_approx},
  {"Big5",           "big5",     my_cs_exact},
  {"cp1251",         "cp1251",   my_cs_exact},
  {"cp1255",         "hebrew",   my_cs_approx},
  {"CP866",          "cp866",    my_cs_exact},
  {"eucCN",          "gb2312",   my_cs_exact},
  {"euc-CN",         "gb2312",   my_cs_exact},
  {"eucJP",          "ujis",     my_cs_exact},
  {"euc-JP",         "ujis",     my_cs_exact},
  {"eucKR",          "euckr",    my_cs_exact},
  {"euc-KR",         "euckr",    my_cs_exact},
#ifdef UNCOMMENT_THIS_WHEN_WL_WL_4024_IS_DONE
  {"gb18030",        "gb18030",  my_cs_exact},
#endif
  {"gb2312",         "gb2312",   my_cs_exact},
  {"gbk",            "gbk",      my_cs_exact},
  {"georgianps",     "geostd8",  my_cs_exact},
  {"georgian-ps",    "geostd8",  my_cs_exact},
  {"IBM-1252",       "cp1252",   my_cs_exact},

  {"iso88591",       "latin1",   my_cs_approx},
  {"ISO_8859-1",     "latin1",   my_cs_approx},
  {"ISO8859-1",      "latin1",   my_cs_approx},
  {"ISO-8859-1",     "latin1",   my_cs_approx},

  {"iso885913",      "latin7",   my_cs_exact},
  {"ISO_8859-13",    "latin7",   my_cs_exact},
  {"ISO8859-13",     "latin7",   my_cs_exact},
  {"ISO-8859-13",    "latin7",   my_cs_exact},

#ifdef UNCOMMENT_THIS_WHEN_WL_4579_IS_DONE
  {"iso885915",      "latin9",   my_cs_exact},
  {"ISO_8859-15",    "latin9",   my_cs_exact},
  {"ISO8859-15",     "latin9",   my_cs_exact},
  {"ISO-8859-15",    "latin9",   my_cs_exact},
#endif

  {"iso88592",       "latin2",   my_cs_exact},
  {"ISO_8859-2",     "latin2",   my_cs_exact},
  {"ISO8859-2",      "latin2",   my_cs_exact},
  {"ISO-8859-2",     "latin2",   my_cs_exact},

  {"iso88597",       "greek",    my_cs_exact},
  {"ISO_8859-7",     "greek",    my_cs_exact},
  {"ISO8859-7",      "greek",    my_cs_exact},
  {"ISO-8859-7",     "greek",    my_cs_exact},

  {"iso88598",       "hebrew",   my_cs_exact},
  {"ISO_8859-8",     "hebrew",   my_cs_exact},
  {"ISO8859-8",      "hebrew",   my_cs_exact},
  {"ISO-8859-8",     "hebrew",   my_cs_exact},

  {"iso88599",       "latin5",   my_cs_exact},
  {"ISO_8859-9",     "latin5",   my_cs_exact},
  {"ISO8859-9",      "latin5",   my_cs_exact},
  {"ISO-8859-9",     "latin5",   my_cs_exact},

  {"koi8r",          "koi8r",    my_cs_exact},
  {"KOI8-R",         "koi8r",    my_cs_exact},
  {"koi8u",          "koi8u",    my_cs_exact},
  {"KOI8-U",         "koi8u",    my_cs_exact},

  {"roman8",         "hp8",      my_cs_exact}, /* Default on HP UX */

  {"Shift_JIS",      "sjis",     my_cs_exact},
  {"SJIS",           "sjis",     my_cs_exact},
  {"shiftjisx0213",  "sjis",     my_cs_exact},

  {"tis620",         "tis620",   my_cs_exact},
  {"tis-620",        "tis620",   my_cs_exact},

  {"ujis",           "ujis",     my_cs_exact},

  {"US-ASCII",       "latin1",   my_cs_approx},

  {"utf8",           "utf8",     my_cs_exact},
  {"utf-8",          "utf8",     my_cs_exact},
#endif
  {NULL,             NULL,       (my_cs_match_type)0}
};

#if(!MYSQLCLIENT_STATIC_LINKING)
const char *
my_os_charset_to_mysql_charset(const char *csname)
{
  const MY_CSET_OS_NAME *csp;
  for (csp= charsets; csp->os_name; ++csp)
  {
    if (!my_strcasecmp(&my_charset_latin1, csp->os_name, csname))
    {
      switch (csp->param)
      {
      case my_cs_exact:
        return csp->my_name;

      case my_cs_approx:
        /*
          Maybe we should print a warning eventually:
          character set correspondence is not exact.
        */
        return csp->my_name;

      default:
        goto def;
      }
    }
  }

def:
  csname= MYSQL_DEFAULT_CHARSET_NAME;

  return csname;
}
#endif

/*
 Converts from wchar_t to SQLWCHAR and copies the result into the provided
 buffer
*/
SQLWCHAR *wchar_t_as_sqlwchar(wchar_t *from, SQLWCHAR *to, size_t len)
{
  /* The function deals mostly with short strings */
  if (len > 1024)
  len= 1024;

  if (sizeof(wchar_t) == sizeof(SQLWCHAR))
  {
    memcpy(to, from, len * sizeof(wchar_t));
    return to;
  }
  else
  {
    size_t i;
    SQLWCHAR *out= to;
    for (i= 0; i < len; i++)
      to+= utf32toutf16((UTF32)from[i], (UTF16 *)to);
    *to= 0;
    return out;
  }
}

char *myodbc_stpmov(char *dst, const char *src)
{
  while ((*dst++ = *src++));
  return dst - 1;
}


char *myodbc_ll2str(longlong val, char *dst, int radix)
{
  char buffer[65];
  char _dig_vec_upper[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char *p;
  long long_val;
  char *dig_vec = _dig_vec_upper;
  ulonglong uval = (ulonglong)val;

  if (radix < 0)
  {
    if (radix < -36 || radix > -2) return (char*)0;
    if (val < 0) {
      *dst++ = '-';
      /* Avoid integer overflow in (-val) for LLONG_MIN (BUG#31799). */
      uval = (ulonglong)0 - uval;
    }
    radix = -radix;
  }
  else
  {
    if (radix > 36 || radix < 2) return (char*)0;
  }
  if (uval == 0)
  {
    *dst++ = '0';
    *dst = '\0';
    return dst;
  }
  p = &buffer[sizeof(buffer)-1];
  *p = '\0';

  while (uval > (ulonglong)LONG_MAX)
  {
    ulonglong quo = uval / (uint)radix;
    uint rem = (uint)(uval - quo* (uint)radix);
    *--p = dig_vec[rem];
    uval = quo;
  }
  long_val = (long)uval;
  while (long_val != 0)
  {
    long quo = long_val / radix;
    *--p = dig_vec[(uchar)(long_val - quo*radix)];
    long_val = quo;
  }
  while ((*dst++ = *p++) != 0);
  return dst - 1;
}

#if(MYSQL8)
/* We need to use qsort with 2 different compare functions */
#ifdef QSORT_EXTRA_CMP_ARGUMENT
#define CMP(A,B) ((*cmp)(cmp_argument,(A),(B)))
#else
#define CMP(A,B) ((*cmp)((A),(B)))
#endif

#define SWAP(A, B, size,swap_ptrs)			\
do {							\
   if (swap_ptrs)					\
   {							\
     char **a = (char**) (A), **b = (char**) (B);  \
     char *tmp = *a; *a++ = *b; *b++ = tmp;		\
   }							\
   else							\
   {							\
     char *a = (A), *b = (B);			\
     char *end= a+size;				\
     do							\
     {							\
       char tmp = *a; *a++ = *b; *b++ = tmp;		\
     } while (a < end);					\
   }							\
} while (0)

/* Put the median in the middle argument */
#define MEDIAN(low, mid, high)				\
{							\
    if (CMP(high,low) < 0)				\
      SWAP(high, low, size, ptr_cmp);			\
    if (CMP(mid, low) < 0)				\
      SWAP(mid, low, size, ptr_cmp);			\
    else if (CMP(high, mid) < 0)			\
      SWAP(mid, high, size, ptr_cmp);			\
}

/* The following node is used to store ranges to avoid recursive calls */

typedef struct st_stack
{
  char *low, *high;
} stack_node;

#define PUSH(LOW,HIGH)  {stack_ptr->low = LOW; stack_ptr++->high = HIGH;}
#define POP(LOW,HIGH)   {LOW = (--stack_ptr)->low; HIGH = stack_ptr->high;}

/* The following stack size is enough for ulong ~0 elements */
#define STACK_SIZE	(8 * sizeof(unsigned long int))
#define THRESHOLD_FOR_INSERT_SORT 10

/****************************************************************************
** 'standard' quicksort with the following extensions:
**
** Can be compiled with the qsort2_cmp compare function
** Store ranges on stack to avoid recursion
** Use insert sort on small ranges
** Optimize for sorting of pointers (used often by MySQL)
** Use median comparison to find partition element
*****************************************************************************/

void myodbc_qsort(void *base_ptr, size_t count, size_t size, qsort_cmp cmp)
{
  char *low, *high, *pivot;
  stack_node stack[STACK_SIZE], *stack_ptr;
  my_bool ptr_cmp;
  /* Handle the simple case first */
  /* This will also make the rest of the code simpler */
  if (count <= 1)
    return;

  low = (char*)base_ptr;
  high = low + size * (count - 1);
  stack_ptr = stack + 1;
  pivot = (char *)my_alloca((int)size);
  ptr_cmp = size == sizeof(char*) && !((low - (char*)0)& (sizeof(char*) - 1));

  /* The following loop sorts elements between high and low */
  do
  {
    char *low_ptr, *high_ptr, *mid;

    count = ((size_t)(high - low) / size) + 1;
    /* If count is small, then an insert sort is faster than qsort */
    if (count < THRESHOLD_FOR_INSERT_SORT)
    {
      for (low_ptr = low + size; low_ptr <= high; low_ptr += size)
      {
        char *ptr;
        for (ptr = low_ptr; ptr > low && CMP(ptr - size, ptr) > 0;
             ptr -= size)
          SWAP(ptr, ptr - size, size, ptr_cmp);
      }
      POP(low, high);
      continue;
    }

    /* Try to find a good middle element */
    mid = low + size * (count >> 1);
    if (count > 40)				/* Must be bigger than 24 */
    {
      size_t step = size* (count / 8);
      MEDIAN(low, low + step, low + step * 2);
      MEDIAN(mid - step, mid, mid + step);
      MEDIAN(high - 2 * step, high - step, high);
      /* Put best median in 'mid' */
      MEDIAN(low + step, mid, high - step);
      low_ptr = low;
      high_ptr = high;
    }
    else
    {
      MEDIAN(low, mid, high);
      /* The low and high argument are already in sorted against 'pivot' */
      low_ptr = low + size;
      high_ptr = high - size;
    }
    memcpy(pivot, mid, size);

    do
    {
      while (CMP(low_ptr, pivot) < 0)
        low_ptr += size;
      while (CMP(pivot, high_ptr) < 0)
        high_ptr -= size;

      if (low_ptr < high_ptr)
      {
        SWAP(low_ptr, high_ptr, size, ptr_cmp);
        low_ptr += size;
        high_ptr -= size;
      }
      else
      {
        if (low_ptr == high_ptr)
        {
          low_ptr += size;
          high_ptr -= size;
        }
        break;
      }
    } while (low_ptr <= high_ptr);

    /*
    Prepare for next iteration.
    Skip partitions of size 1 as these doesn't have to be sorted
    Push the larger partition and sort the smaller one first.
    This ensures that the stack is keept small.
    */

    if ((int)(high_ptr - low) <= 0)
    {
      if ((int)(high - low_ptr) <= 0)
      {
        POP(low, high);			/* Nothing more to sort */
      }
      else
        low = low_ptr;			/* Ignore small left part. */
    }
    else if ((int)(high - low_ptr) <= 0)
      high = high_ptr;			/* Ignore small right part. */
    else if ((high_ptr - low) > (high - low_ptr))
    {
      PUSH(low, high_ptr);		/* Push larger left part */
      low = low_ptr;
    }
    else
    {
      PUSH(low_ptr, high);		/* Push larger right part */
      high = high_ptr;
    }
  } while (stack_ptr > stack);
  return;
}

/*
  Converts integer to its string representation in decimal notation.

  SYNOPSIS
    myodbc_int10_to_str()
      val     - value to convert
      dst     - points to buffer where string representation should be stored
      radix   - flag that shows whenever val should be taken as signed or not

  DESCRIPTION
    This is version of int2str() function which is optimized for normal case
    of radix 10/-10. It takes only sign of radix parameter into account and
    not its absolute value.

  RETURN VALUE
    Pointer to ending NUL character.
*/

char *myodbc_int10_to_str(long int val, char *dst, int radix) {
  char buffer[65];
  char *p;
  long int new_val;
  unsigned long int uval = (unsigned long int)val;

  if (radix < 0) /* -10 */
  {
    if (val < 0) {
      *dst++ = '-';
      /* Avoid integer overflow in (-val) for LLONG_MIN (BUG#31799). */
      uval = (unsigned long int)0 - uval;
    }
  }

  p = &buffer[sizeof(buffer) - 1];
  *p = '\0';
  new_val = (long)(uval / 10);
  *--p = '0' + (char)(uval - (unsigned long)new_val * 10);
  val = new_val;

  while (val != 0) {
    new_val = val / 10;
    *--p = '0' + (char)(val - new_val * 10);
    val = new_val;
  }
  while ((*dst++ = *p++) != 0)
    ;
  return dst - 1;
}


bool myodbc_append_mem_std(std::string &str, const char *append, size_t length)
{
  str.append(append, length);
  return false;
}


bool myodbc_append_os_quoted_std(std::string &str, const char *append, ...) {
#ifdef _WIN32
  const char *quote_str = "\"";
  const uint quote_len = 1;
#else
  const char *quote_str = "\'";
  const uint quote_len = 1;
#endif /* _WIN32 */
  bool ret = true;
  va_list dirty_text;
  str.reserve(str.length() + 128);
  str.append(quote_str, quote_len); /* Leading quote */
  va_start(dirty_text, append);
  while (append != NullS) {
    const char *cur_pos = append;
    const char *next_pos = cur_pos;

    /* Search for quote in each string and replace with escaped quote */
    while (*(next_pos = strcend(cur_pos, quote_str[0])) != '\0') {
      str.append(cur_pos, (uint)(next_pos - cur_pos)).
          append("\\", 1).append(quote_str, quote_len);
      cur_pos = next_pos + 1;
    }
    str.append(cur_pos, (uint)(next_pos - cur_pos));
    append = va_arg(dirty_text, char *);
  }
  va_end(dirty_text);
  str.append(quote_str, quote_len); /* Trailing quote */

  return false;
}




#endif