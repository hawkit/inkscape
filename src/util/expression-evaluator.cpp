/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * Original file from libgimpwidgets: gimpeevl.c
 * Copyright (C) 2008 Fredrik Alstromer <roe@excu.se>
 * Copyright (C) 2008 Martin Nordholts <martinn@svn.gnome.org>
 * Modified for Inkscape by Johan Engelen
 * Copyright (C) 2011 Johan Engelen
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "util/expression-evaluator.h"
#include "util/units.h"

#include <string.h>

namespace Inkscape {
namespace Util {

enum
{
  GIMP_EEVL_TOKEN_NUM        = 30000,
  GIMP_EEVL_TOKEN_IDENTIFIER = 30001,

  GIMP_EEVL_TOKEN_ANY        = 40000,

  GIMP_EEVL_TOKEN_END        = 50000
};

typedef int GimpEevlTokenType;


typedef struct
{
  GimpEevlTokenType type;

  union
  {
    gdouble fl;

    struct
    {
      const gchar *c;
      gint         size;
    };

  } value;

} GimpEevlToken;

typedef struct
{
  const gchar             *string;
  GimpEevlUnitResolverProc unit_resolver_proc;
  Unit                    *unit;

  GimpEevlToken            current_token;
  const gchar             *start_of_current_token;
} GimpEevl;

/** Unit Resolver...
 */
bool unitresolverproc  (const gchar* identifier, GimpEevlQuantity *result, Unit* unit)
{
    static UnitTable unit_table;

    if (!unit) {
        result->value = 1;
        result->dimension = 1;
        return true;
    }else if (!identifier) {
        result->value = 1;
        result->dimension = unit->isAbsolute() ? 1 : 0;
        return true;
    } else if (unit_table.hasUnit(identifier)) {
        Unit identifier_unit = unit_table.getUnit(identifier);

        // Catch the case of zero or negative unit factors (error!)
        if (identifier_unit.factor < 0.0000001) {
            return false;
        }

        result->value = unit->factor / identifier_unit.factor;
        result->dimension = identifier_unit.isAbsolute() ? 1 : 0;
        return true;
    } else {
        return false;
    }
}

static void             gimp_eevl_init                     (GimpEevl                 *eva,
                                                            const gchar              *string,
                                                            GimpEevlUnitResolverProc  unit_resolver_proc,
                                                            Unit                     *unit);
static GimpEevlQuantity gimp_eevl_complete                 (GimpEevl                 *eva);
static GimpEevlQuantity gimp_eevl_expression               (GimpEevl                 *eva);
static GimpEevlQuantity gimp_eevl_term                     (GimpEevl                 *eva);
static GimpEevlQuantity gimp_eevl_signed_factor            (GimpEevl                 *eva);
static GimpEevlQuantity gimp_eevl_factor                   (GimpEevl                 *eva);
static gboolean         gimp_eevl_accept                   (GimpEevl                 *eva,
                                                            GimpEevlTokenType         token_type,
                                                            GimpEevlToken            *consumed_token);
static void             gimp_eevl_lex                      (GimpEevl                 *eva);
static void             gimp_eevl_lex_accept_count          (GimpEevl                 *eva,
                                                            gint                      count,
                                                            GimpEevlTokenType         token_type);
static void             gimp_eevl_lex_accept_to             (GimpEevl                 *eva,
                                                            gchar                    *to,
                                                            GimpEevlTokenType         token_type);
static void             gimp_eevl_move_past_whitespace     (GimpEevl                 *eva);
static gboolean         gimp_eevl_unit_identifier_start    (gunichar                  c);
static gboolean         gimp_eevl_unit_identifier_continue (gunichar                  c);
static gint             gimp_eevl_unit_identifier_size     (const gchar              *s,
                                                            gint                      start);
static void             gimp_eevl_expect                   (GimpEevl                 *eva,
                                                            GimpEevlTokenType         token_type,
                                                            GimpEevlToken            *value);
static void             gimp_eevl_error                    (GimpEevl                 *eva,
                                                            const char               *msg);


/**
 * Evaluates the given arithmetic expression, along with an optional dimension
 * analysis, and basic unit conversions.
 *
 * @param string              The NULL-terminated string to be evaluated.
 * @param unit_resolver_proc  Unit resolver callback.
 *
 * All units conversions factors are relative to some implicit
 * base-unit (which in GIMP is inches). This is also the unit of the
 * returned value.
 *
 * Returns: A #GimpEevlQuantity with a value given in the base unit along with
 * the order of the dimension (i.e. if the base unit is inches, a dimension
 * order of two menas in^2).
 *
 * @return Result of evaluation.
 * @throws Inkscape::Util::EvaluatorException There was a parse error.
 **/
GimpEevlQuantity
gimp_eevl_evaluate (const gchar* string, Unit* unit)
{
    if (! g_utf8_validate (string, -1, NULL)) {
        throw EvaluatorException("Invalid UTF8 string", NULL);
    }

    GimpEevl eva;
    gimp_eevl_init (&eva, string, unitresolverproc, unit);

    return gimp_eevl_complete(&eva);
}

static void
gimp_eevl_init (GimpEevl                  *eva,
                const gchar               *string,
                GimpEevlUnitResolverProc   unit_resolver_proc,
                Unit                      *unit)
{
  eva->string              = string;
  eva->unit_resolver_proc  = unit_resolver_proc;
  eva->unit                = unit;

  eva->current_token.type  = GIMP_EEVL_TOKEN_END;

  /* Preload symbol... */
  gimp_eevl_lex (eva);
}

static GimpEevlQuantity
gimp_eevl_complete (GimpEevl *eva)
{
  GimpEevlQuantity result = {0, 0};
  GimpEevlQuantity default_unit_factor;

  /* Empty expression evaluates to 0 */
  if (gimp_eevl_accept (eva, GIMP_EEVL_TOKEN_END, NULL))
    return result;

  result = gimp_eevl_expression (eva);

  /* There should be nothing left to parse by now */
  gimp_eevl_expect (eva, GIMP_EEVL_TOKEN_END, 0);

  eva->unit_resolver_proc (NULL, &default_unit_factor, eva->unit);

  /* Entire expression is dimensionless, apply default unit if
   * applicable
   */
  if (result.dimension == 0 && default_unit_factor.dimension != 0)
    {
      result.value     /= default_unit_factor.value;
      result.dimension  = default_unit_factor.dimension;
    }
  return result;
}

static GimpEevlQuantity
gimp_eevl_expression (GimpEevl *eva)
{
  gboolean         subtract;
  GimpEevlQuantity evaluated_terms;

  evaluated_terms = gimp_eevl_term (eva);

  /* continue evaluating terms, chained with + or -. */
  for (subtract = FALSE;
       gimp_eevl_accept (eva, '+', NULL) ||
       (subtract = gimp_eevl_accept (eva, '-', NULL));
       subtract = FALSE)
    {
      GimpEevlQuantity new_term = gimp_eevl_term (eva);

      /* If dimensions missmatch, attempt default unit assignent */
      if (new_term.dimension != evaluated_terms.dimension)
        {
          GimpEevlQuantity default_unit_factor;

          eva->unit_resolver_proc (NULL,
                                   &default_unit_factor,
                                   eva->unit);

          if (new_term.dimension == 0 &&
              evaluated_terms.dimension == default_unit_factor.dimension)
            {
              new_term.value     /= default_unit_factor.value;
              new_term.dimension  = default_unit_factor.dimension;
            }
          else if (evaluated_terms.dimension == 0 &&
                   new_term.dimension == default_unit_factor.dimension)
            {
              evaluated_terms.value     /= default_unit_factor.value;
              evaluated_terms.dimension  = default_unit_factor.dimension;
            }
          else
            {
              gimp_eevl_error (eva, "Dimension missmatch during addition");
            }
        }

      evaluated_terms.value += (subtract ? -new_term.value : new_term.value);
    }

  return evaluated_terms;
}

static GimpEevlQuantity
gimp_eevl_term (GimpEevl *eva)
{
  gboolean         division;
  GimpEevlQuantity evaluated_signed_factors;

  evaluated_signed_factors = gimp_eevl_signed_factor (eva);

  for (division = FALSE;
       gimp_eevl_accept (eva, '*', NULL) ||
       (division = gimp_eevl_accept (eva, '/', NULL));
       division = FALSE)
    {
      GimpEevlQuantity new_signed_factor = gimp_eevl_signed_factor (eva);

      if (division)
        {
          evaluated_signed_factors.value     /= new_signed_factor.value;
          evaluated_signed_factors.dimension -= new_signed_factor.dimension;

        }
      else
        {
          evaluated_signed_factors.value     *= new_signed_factor.value;
          evaluated_signed_factors.dimension += new_signed_factor.dimension;
        }
    }

  return evaluated_signed_factors;
}

static GimpEevlQuantity
gimp_eevl_signed_factor (GimpEevl *eva)
{
  GimpEevlQuantity result;
  gboolean         negate = FALSE;

  if (! gimp_eevl_accept (eva, '+', NULL))
    negate = gimp_eevl_accept (eva, '-', NULL);

  result = gimp_eevl_factor (eva);

  if (negate) result.value = -result.value;

  return result;
}

static GimpEevlQuantity
gimp_eevl_factor (GimpEevl *eva)
{
  GimpEevlQuantity evaluated_factor = { 0, 0 };
  GimpEevlToken    consumed_token;

  if (gimp_eevl_accept (eva,
                        GIMP_EEVL_TOKEN_NUM,
                        &consumed_token))
    {
      evaluated_factor.value = consumed_token.value.fl;
    }
  else if (gimp_eevl_accept (eva, '(', NULL))
    {
      evaluated_factor = gimp_eevl_expression (eva);
      gimp_eevl_expect (eva, ')', 0);
    }
  else
    {
      gimp_eevl_error (eva, "Expected number or '('");
    }

  if (eva->current_token.type == GIMP_EEVL_TOKEN_IDENTIFIER)
    {
      gchar            *identifier;
      GimpEevlQuantity  result;

      gimp_eevl_accept (eva,
                        GIMP_EEVL_TOKEN_ANY,
                        &consumed_token);

      identifier = g_newa (gchar, consumed_token.value.size + 1);

      strncpy (identifier, consumed_token.value.c, consumed_token.value.size);
      identifier[consumed_token.value.size] = '\0';

      if (eva->unit_resolver_proc (identifier,
                                   &result,
                                   eva->unit))
        {
          evaluated_factor.value      /= result.value;
          evaluated_factor.dimension  += result.dimension;
        }
      else
        {
          gimp_eevl_error (eva, "Unit was not resolved");
        }
    }

  return evaluated_factor;
}

static gboolean
gimp_eevl_accept (GimpEevl          *eva,
                  GimpEevlTokenType  token_type,
                  GimpEevlToken     *consumed_token)
{
  gboolean existed = FALSE;

  if (token_type == eva->current_token.type ||
      token_type == GIMP_EEVL_TOKEN_ANY)
    {
      existed = TRUE;

      if (consumed_token)
        *consumed_token = eva->current_token;

      /* Parse next token */
      gimp_eevl_lex (eva);
    }

  return existed;
}

static void
gimp_eevl_lex (GimpEevl *eva)
{
  const gchar *s;

  gimp_eevl_move_past_whitespace (eva);
  s = eva->string;
  eva->start_of_current_token = s;

  if (! s || s[0] == '\0')
    {
      /* We're all done */
      eva->current_token.type = GIMP_EEVL_TOKEN_END;
    }
  else if (s[0] == '+' || s[0] == '-')
    {
      /* Snatch these before the g_strtod() does, othewise they might
       * be used in a numeric conversion.
       */
      gimp_eevl_lex_accept_count (eva, 1, s[0]);
    }
  else
    
    {
      /* Attempt to parse a numeric value */
      gchar  *endptr = NULL;
      gdouble value      = g_strtod (s, &endptr);

      if (endptr && endptr != s)
        {
          /* A numeric could be parsed, use it */
          eva->current_token.value.fl = value;

          gimp_eevl_lex_accept_to (eva, endptr, GIMP_EEVL_TOKEN_NUM);
        }
      else if (gimp_eevl_unit_identifier_start (s[0]))
        {
          /* Unit identifier */
          eva->current_token.value.c    = s;
          eva->current_token.value.size = gimp_eevl_unit_identifier_size (s, 0);

          gimp_eevl_lex_accept_count (eva,
                                      eva->current_token.value.size,
                                      GIMP_EEVL_TOKEN_IDENTIFIER);
        }
      else
        {
          /* Everything else is a single character token */
          gimp_eevl_lex_accept_count (eva, 1, s[0]);
        }
    }
}

static void
gimp_eevl_lex_accept_count (GimpEevl          *eva,
                            gint               count,
                            GimpEevlTokenType  token_type)
{
  eva->current_token.type  = token_type;
  eva->string             += count;
}

static void
gimp_eevl_lex_accept_to (GimpEevl          *eva,
                         gchar             *to,
                         GimpEevlTokenType  token_type)
{
  eva->current_token.type = token_type;
  eva->string             = to;
}

static void
gimp_eevl_move_past_whitespace (GimpEevl *eva)
{
  if (! eva->string)
    return;

  while (g_ascii_isspace (*eva->string))
    eva->string++;
}

static gboolean
gimp_eevl_unit_identifier_start (gunichar c)
{
  return (g_unichar_isalpha (c) ||
          c == (gunichar) '%'   ||
          c == (gunichar) '\'');
}

static gboolean
gimp_eevl_unit_identifier_continue (gunichar c)
{
  return (gimp_eevl_unit_identifier_start (c) ||
          g_unichar_isdigit (c));
}

/**
 * gimp_eevl_unit_identifier_size:
 * @s:
 * @start:
 *
 * Returns: Size of identifier in bytes (not including NULL
 * terminator).
 **/
static gint
gimp_eevl_unit_identifier_size (const gchar *string,
                                gint         start_offset)
{
  const gchar *start  = g_utf8_offset_to_pointer (string, start_offset);
  const gchar *s      = start;
  gunichar     c      = g_utf8_get_char (s);
  gint         length = 0;

  if (gimp_eevl_unit_identifier_start (c))
    {
      s = g_utf8_next_char (s);
      c = g_utf8_get_char (s);
      length++;

      while (gimp_eevl_unit_identifier_continue (c))
        {
          s = g_utf8_next_char (s);
          c = g_utf8_get_char (s);
          length++;
        }
    }
  
  return g_utf8_offset_to_pointer (start, length) - start;
}

static void
gimp_eevl_expect (GimpEevl          *eva,
                  GimpEevlTokenType  token_type,
                  GimpEevlToken     *value)
{
  if (! gimp_eevl_accept (eva, token_type, value))
    gimp_eevl_error (eva, "Unexpected token");
}

static void
gimp_eevl_error (GimpEevl *eva,
                 const char    *msg)
{
  throw EvaluatorException(msg, eva->start_of_current_token);
}

} // namespace Util
} // namespace Inkscape
