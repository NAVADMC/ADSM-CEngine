/** @file parameter.c
 * Functions for retrieving simulation control parameters.
 *
 * @author Neil Harvey <nharve01@uoguelph.ca><br>
 *   School of Computer Science, University of Guelph<br>
 *   Guelph, ON N1G 2W1<br>
 *   CANADA
 * @date March 2003
 *
 * Copyright &copy; University of Guelph, 2003-2009
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "parameter.h"
#include "sqlite3_exec_dict.h"
#include <glib.h>
#include <gsl/gsl_math.h>

#if STDC_HEADERS
#  include <stdlib.h>
#endif

#if HAVE_STRING_H
#  include <string.h>
#endif

#if HAVE_ERRNO_H
#  include <errno.h>
#endif



typedef struct
{
  GArray *x; /* growable array of doubles */
  GArray *y; /* growable array of doubles */
}
PAR_get_relchart_callback_args_t;


  
/**
 * Retrieves a relationship chart parameter from the database. This callback
 * function is called once for each x-y pair (database row) in the chart.
 *
 * @param loc location of a PAR_get_relchart_callback_args_t object into which
 *  to accumulate the x-y pairs.
 * @param dict the SQL query result as a GHashTable in which key = colname,
 *   value = value, both in (char *) format.
 * @return 0
 */
static int
PAR_get_relchart_callback (void *loc, GHashTable *dict)
{
  int ncols;
  PAR_get_relchart_callback_args_t *build;
  char *x_as_text, *y_as_text;
  double x, y;
  
  ncols = g_hash_table_size (dict);  
  g_assert (ncols == 2);
  build = (PAR_get_relchart_callback_args_t *) loc;
  errno = 0;
  x_as_text = g_hash_table_lookup (dict, "x");
  x = strtod (x_as_text, NULL);
  if (errno == ERANGE)
    {
      g_error ("relationship chart point \"%s\" is not a number", x_as_text);
    }
  errno = 0;
  y_as_text = g_hash_table_lookup (dict, "y");
  y = strtod (y_as_text, NULL);
  if (errno == ERANGE)
    {
      g_error ("relationship chart point \"%s\" is not a number", y_as_text);
    }
  g_array_append_val (build->x, x);
  g_array_append_val (build->y, y);
  return 0;
}




/**
 * Retrieves a relationship chart.
 *
 * @param db a parameter database.
 * @param id the database ID of the relationship chart.
 * @return a relationship chart object.
 */
REL_chart_t *
PAR_get_relchart (sqlite3 *db, guint id)
{
  REL_chart_t *chart;
  PAR_get_relchart_callback_args_t build;
  char *query;
  char *sqlerr;
  
  #if DEBUG
    g_debug ("----- ENTER PAR_get_relationship_chart");
  #endif

  build.x = g_array_new (/* zero_terminated = */ FALSE, /* clear = */ FALSE, sizeof (double));
  build.y = g_array_new (/* zero_terminated = */ FALSE, /* clear = */ FALSE, sizeof (double));
  query = g_strdup_printf ("SELECT x,y FROM ScenarioCreator_relationalfunction fn,ScenarioCreator_relationalpoint pt WHERE fn.id=%u AND pt.relational_function_id=fn.id ORDER BY x ASC", id);
  sqlite3_exec_dict (db, query, PAR_get_relchart_callback, &build, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }
  g_free (query);

  chart = REL_new_chart ((double *)(build.x->data), (double *)(build.y->data), build.x->len);
  g_array_free (build.x, /* free_segment = */ TRUE);
  g_array_free (build.y, /* free_segment = */ TRUE);

  #if DEBUG
    g_debug ("----- EXIT PAR_get_relationship_chart");
  #endif

  return chart;
}



typedef struct
{
  sqlite3 *db; /**< A parameter database. */
  PDF_dist_t *dist; /**< A location into which to write the pointer to the
    created object. */
}
PAR_get_PDF_callback_args_t;


  
/**
 * Retrieves a probability distribution function parameter from the database.
 *
 * @param data pointer to a PAR_get_PDF_callback_args_t structure.
 * @param dict the SQL query result as a GHashTable in which key = colname,
 *   value = value, both in (char *) format.
 * @return 0
 */
static int
PAR_get_PDF_callback (void *data, GHashTable *dict)
{
  int ncols;
  PAR_get_PDF_callback_args_t *args;
  PDF_dist_t *dist = NULL;
  char *equation_type;

  ncols = g_hash_table_size (dict);  
  g_assert (ncols == 20);

  args = (PAR_get_PDF_callback_args_t *)data;

  equation_type = g_hash_table_lookup (dict, "equation_type");
  if (strcmp (equation_type, "Beta") == 0)
    {
      double alpha, beta, location, scale;

      errno = 0;
      alpha = strtod (g_hash_table_lookup (dict, "alpha"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      beta = strtod (g_hash_table_lookup (dict, "alpha2"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      location = strtod (g_hash_table_lookup (dict, "min"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      scale = strtod (g_hash_table_lookup (dict, "max"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_beta_dist (alpha, beta, location, scale);
    }
  else if (strcmp (equation_type, "BetaPERT") == 0)
    {
      double min, mode, max;

      errno = 0;
      min = strtod (g_hash_table_lookup (dict, "min"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      mode = strtod (g_hash_table_lookup (dict, "mode"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      max = strtod (g_hash_table_lookup (dict, "max"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_beta_pert_dist (min, mode, max);
    }
  else if (strcmp (equation_type, "Bernoulli") == 0)
    {
      double p;

      errno = 0;
      p = strtod (g_hash_table_lookup (dict, "p"), NULL);
      g_assert (errno != ERANGE);
      g_assert ((0.0 < p) && (p < 1.0));

      dist = PDF_new_bernoulli_dist (p);
    }
  else if (strcmp (equation_type, "Binomial") == 0)
    {
      long tmp;
      unsigned int n;
      double p;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "s"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (tmp > 0);
      g_assert (tmp <= UINT_MAX);
      n = (unsigned int) tmp;

      errno = 0;
      p = strtod (g_hash_table_lookup (dict, "p"), NULL);
      g_assert (errno != ERANGE);
      g_assert ((0.0 < p) && (p < 1.0));

      dist = PDF_new_binomial_dist (n, p);
    }
  else if (strcmp (equation_type, "Discrete Uniform") == 0)
    {
      long tmp;
      int min, max;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "min"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (tmp <= INT_MAX);
      min = (int) tmp;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "max"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (tmp <= INT_MAX);
      max = (int) tmp;

      dist = PDF_new_discrete_uniform_dist (min, max);
    }
  else if (strcmp (equation_type, "Exponential") == 0)
    {
      double mean;

      errno = 0;
      mean = strtod (g_hash_table_lookup (dict, "mean"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_exponential_dist (mean);
    }
  else if (strcmp (equation_type, "Fixed Value") == 0)
    {
      char *mode_as_text;
      double mode;
      errno = 0;
      mode_as_text = g_hash_table_lookup (dict, "mode");
      mode = strtod (mode_as_text, NULL);
      if (errno == ERANGE)
        {
          g_error ("fixed value distribution parameter \"%s\" is not a number", mode_as_text);
        }
      dist = PDF_new_point_dist (mode);
    }
  else if (strcmp (equation_type, "Gamma") == 0)
    {
      double alpha, beta;

      errno = 0;
      alpha = strtod (g_hash_table_lookup (dict, "alpha"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      beta = strtod (g_hash_table_lookup (dict, "beta"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_gamma_dist (alpha, beta);
    }
  else if (strcmp (equation_type, "Gaussian") == 0)
    {
      double mu, sigma;

      errno = 0;
      mu = strtod (g_hash_table_lookup (dict, "mean"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      sigma = strtod (g_hash_table_lookup (dict, "std_dev"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_gaussian_dist (mu, sigma);
    }
  else if (strcmp (equation_type, "Histogram") == 0)
    {
      guint rel_id;
      PAR_get_relchart_callback_args_t build;
      char *query;
      char *sqlerr;
      guint nbins, i;
      gsl_histogram *h;
      double *range;

      errno = 0;
      rel_id = strtol (g_hash_table_lookup (dict, "graph_id"), NULL, /* base */ 10);
      g_assert (errno != ERANGE && errno != EINVAL);  
      build.x = g_array_new (/* zero_terminated = */ FALSE, /* clear = */ FALSE, sizeof (double));
      build.y = g_array_new (/* zero_terminated = */ FALSE, /* clear = */ FALSE, sizeof (double));
      query =
        g_strdup_printf ("SELECT x,y "
                         "FROM ScenarioCreator_relationalfunction fn,ScenarioCreator_relationalpoint pt "
                         "WHERE fn.id=%u "
                         "AND pt.relational_function_id=fn.id "
                         "ORDER BY x",
                         rel_id);
      sqlite3_exec_dict (args->db, query, PAR_get_relchart_callback, &build, &sqlerr);
      if (sqlerr)
        {
          g_error ("%s", sqlerr);
        }
      g_free (query);

      nbins = build.x->len - 1;
      h = gsl_histogram_alloc (nbins);
      /* The x-values read from the database define the range (upper and lower
       * limit) of each bin. */
      range = (double *)(build.x->data);
      gsl_histogram_set_ranges (h, range, nbins + 1);
      /* The y-values read from the database give the amount to add to each bin
       * (the final y-value is ignored). */
      for (i = 0; i < nbins; i++)
        gsl_histogram_accumulate (h, (range[i]+range[i+1])/2.0, g_array_index(build.y, double, i));

      dist = PDF_new_histogram_dist (h);
      gsl_histogram_free (h);   
    }
  else if (strcmp (equation_type, "Hypergeometric") == 0)
    {
      long tmp;
      unsigned int n, d, m;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "n"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (0 <= tmp && tmp <= UINT_MAX);
      n = (unsigned int) tmp;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "d"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (0 <= tmp && tmp <= UINT_MAX);
      d = (unsigned int) tmp;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "m"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (0 <= tmp && tmp <= UINT_MAX);
      m = (unsigned int) tmp;

      /* Parameter restrictions (from Vose) */
      g_assert (0 < n && n <= m);
      g_assert (0 < d && d <= m);
      g_assert (m > 0);
      
      PDF_new_hypergeometric_dist (d, m - d, n);
    }
  else if (strcmp (equation_type, "Inverse Gaussian") == 0)
    {
      double mu, lambda;

      errno = 0;
      mu = strtod (g_hash_table_lookup (dict, "mean"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      lambda = strtod (g_hash_table_lookup (dict, "shape"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_inverse_gaussian_dist (mu, lambda);
    }
  else if (strcmp (equation_type, "Logistic") == 0)
    {
      double location, scale;

      errno = 0;
      location = strtod (g_hash_table_lookup (dict, "location"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      scale = strtod (g_hash_table_lookup (dict, "scale"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_logistic_dist (location, scale);
    }
  else if (strcmp (equation_type, "LogLogistic") == 0)
    {
      double location, scale, shape;

      errno = 0;
      location = strtod (g_hash_table_lookup (dict, "location"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      scale = strtod (g_hash_table_lookup (dict, "scale"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      shape = strtod (g_hash_table_lookup (dict, "shape"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_loglogistic_dist (location, scale, shape);
    }
  else if (strcmp (equation_type, "Lognormal") == 0)
    {
      double mean, std_dev;
      double mean_sq, std_dev_sq;
      double zeta, sigma;

      errno = 0;
      mean = strtod (g_hash_table_lookup (dict, "mean"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      std_dev = strtod (g_hash_table_lookup (dict, "std_dev"), NULL);
      g_assert (errno != ERANGE);
      
      mean_sq = gsl_pow_2 (mean);
      std_dev_sq = gsl_pow_2 (std_dev);
      zeta = log( mean_sq / sqrt(std_dev_sq + mean_sq) );
      sigma = sqrt( log( (std_dev_sq + mean_sq)/mean_sq ) );
      #if DEBUG
        g_debug ("lognormal mean=%g, std_dev=%g -> zeta=%g, sigma=%g",
                 mean, std_dev, zeta, sigma);
      #endif

      dist = PDF_new_lognormal_dist (zeta, sigma);
    }
  else if (strcmp (equation_type, "Negative Binomial") == 0)
    {
      long tmp;
      unsigned int s;
      double p;

      errno = 0;
      tmp = strtol (g_hash_table_lookup (dict, "s"), NULL, 10); /* base 10 */
      g_assert (errno != ERANGE);
      g_assert (0 < tmp && tmp <= UINT_MAX);
      s = (unsigned int) tmp;

      errno = 0;
      p = strtod (g_hash_table_lookup (dict, "p"), NULL);
      g_assert (errno != ERANGE);
      g_assert ((0.0 < p) && (p < 1.0));

      dist = PDF_new_negative_binomial_dist (s, p);
    }
  else if (strcmp (equation_type, "Pareto") == 0)
    {
      double theta, a;

      errno = 0;
      theta = strtod (g_hash_table_lookup (dict, "theta"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      a = strtod (g_hash_table_lookup (dict, "a"), NULL);
      g_assert (errno != ERANGE);

      /* Parameter restrictions (from Vose) */
      g_assert (theta > 0);
      g_assert (a > 0);

      PDF_new_pareto_dist (theta, a);
    }
  else if (strcmp (equation_type, "Pearson 5") == 0)
    {
      double alpha, beta;

      errno = 0;
      alpha = strtod (g_hash_table_lookup (dict, "alpha"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      beta = strtod (g_hash_table_lookup (dict, "beta"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_pearson5_dist (alpha, beta);
    }
  else if (strcmp (equation_type, "Piecewise") == 0)
    {
      guint rel_id;
      PAR_get_relchart_callback_args_t build;
      char *query;
      char *sqlerr;

      errno = 0;
      rel_id = strtol (g_hash_table_lookup (dict, "graph_id"), NULL, /* base */ 10);
      g_assert (errno != ERANGE && errno != EINVAL);  
      build.x = g_array_new (/* zero_terminated = */ FALSE, /* clear = */ FALSE, sizeof (double));
      build.y = g_array_new (/* zero_terminated = */ FALSE, /* clear = */ FALSE, sizeof (double));
      query =
        g_strdup_printf ("SELECT x,y "
                         "FROM ScenarioCreator_relationalfunction fn,ScenarioCreator_relationalpoint pt "
                         "WHERE fn.id=%u "
                         "AND pt.relational_function_id=fn.id "
                         "ORDER BY x",
                         rel_id);
      sqlite3_exec_dict (args->db, query, PAR_get_relchart_callback, &build, &sqlerr);
      if (sqlerr)
        {
          g_error ("%s", sqlerr);
        }
      g_free (query);

      dist = PDF_new_piecewise_dist (build.x->len, (double *)(build.x->data), (double *)(build.y->data));
      g_array_free (build.x, /* free_segment = */ TRUE);
      g_array_free (build.y, /* free_segment = */ TRUE);
    }
  else if (strcmp (equation_type, "Poisson") == 0)
    {
      double mean;

      errno = 0;
      mean = strtod (g_hash_table_lookup (dict, "mean"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_poisson_dist (mean);
    }
  else if (strcmp (equation_type, "Triangular") == 0)
    {
      double a,c,b;

      errno = 0;
      a = strtod (g_hash_table_lookup (dict, "min"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      c = strtod (g_hash_table_lookup (dict, "mode"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      b = strtod (g_hash_table_lookup (dict, "max"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_triangular_dist (a, c, b);
    }
  else if (strcmp (equation_type, "Uniform") == 0)
    {
      double a, b;

      errno = 0;
      a = strtod (g_hash_table_lookup (dict, "min"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      b = strtod (g_hash_table_lookup (dict, "max"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_uniform_dist (a, b);
    }
  else if (strcmp (equation_type, "Weibull") == 0)
    {
      double alpha, beta;

      errno = 0;
      alpha = strtod (g_hash_table_lookup (dict, "alpha"), NULL);
      g_assert (errno != ERANGE);

      errno = 0;
      beta = strtod (g_hash_table_lookup (dict, "beta"), NULL);
      g_assert (errno != ERANGE);

      dist = PDF_new_weibull_dist (alpha, beta);
    }
  else
    {
      g_error ("%s distribution not supported", equation_type);
    }
  args->dist = dist;
  return 0;
}



/**
 * Retrieves a probability distribution function.
 *
 * @param db a parameter database.
 * @param id the database ID of the probability distribution function.
 * @return a probability distribution function object.
 */
PDF_dist_t *
PAR_get_PDF (sqlite3 *db, guint id)
{
  PAR_get_PDF_callback_args_t args;
  char *query;
  char *sqlerr;
#if DEBUG
  g_debug ("----- ENTER PAR_get_PDF");
#endif

  args.db = db;
  args.dist = NULL;
  query = g_strdup_printf ("SELECT equation_type,mean,std_dev,min,mode,max,alpha,alpha2,beta,location,scale,shape,n,p,m,d,theta,a,s,graph_id FROM ScenarioCreator_probabilitydensityfunction WHERE id=%u", id);
  sqlite3_exec_dict (db, query, PAR_get_PDF_callback, &args, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }
  g_free (query);

#if DEBUG
  g_debug ("----- EXIT PAR_get_PDF");
#endif

  return args.dist;
}



/**
 * Retrieves a boolean parameter from the database.
 *
 * @param loc location of a gboolean into which to write the boolean.
 * @param ncols number of columns in the SQL query result.
 * @param values values returned by the SQL query, all in text form.
 * @param colname names of columns in the SQL query result.
 * @return 0
 */
static int
PAR_get_boolean_callback (void *loc, int ncols, char **value, char **colname)
{
  gboolean *result;
  long int tmp;
  
  g_assert (ncols == 1);
  errno = 0;
  tmp = strtol (value[0], NULL, 10);   /* base 10 */
  g_assert (errno != ERANGE && errno != EINVAL);
  g_assert (tmp == 0 || tmp == 1);
  result = (gboolean *)loc;
  *result = (tmp == 1);
  return 0;
}



/**
 * Retrieves a boolean.
 *
 * @param db a parameter database.
 * @param query the query to retrieve the parameter.
 * @return the boolean.
 */
gboolean
PAR_get_boolean (sqlite3 *db, char *query)
{
  gboolean result = FALSE;
  char *sqlerr;

#if DEBUG
  g_debug ("----- ENTER PAR_get_boolean");
#endif

  sqlite3_exec (db, query, PAR_get_boolean_callback, &result, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }

#if DEBUG
  g_debug ("----- EXIT PAR_get_boolean");
#endif

  return result;
}



/**
 * Retrieves a text parameter from the database.
 *
 * @param loc location of a char * into which to write the starting location of
 *   the text.
 * @param ncols number of columns in the SQL query result.
 * @param values values returned by the SQL query, all in text form.
 * @param colname names of columns in the SQL query result.
 * @return 0
 */
static int
PAR_get_text_callback (void *loc, int ncols, char **value, char **colname)
{
  char **result;
  gchar *normalized;
  
  g_assert (ncols == 1);
  normalized = g_utf8_normalize (value[0], -1, G_NORMALIZE_DEFAULT);
  
  result = (char **)loc;
  *result = normalized;
  return 0;
}



/**
 * Retrieves a text value.  The text is copied and must be freed with g_free.
 *
 * @param db a parameter database.
 * @param query the query to retrieve the parameter.
 * @return the text.
 */
char *
PAR_get_text (sqlite3 *db, char *query)
{
  char *text = NULL;
  char *sqlerr;

#if DEBUG
  g_debug ("----- ENTER PAR_get_text");
#endif

  sqlite3_exec (db, query, PAR_get_text_callback, &text, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }

#if DEBUG
  g_debug ("----- EXIT PAR_get_text");
#endif

  return text;
}



/**
 * Retrieves an integer parameter from the database.
 *
 * @param loc location of a gint into which to write the integer.
 * @param ncols number of columns in the SQL query result.
 * @param values values returned by the SQL query, all in text form.
 * @param colname names of columns in the SQL query result.
 * @return 0
 */
static int
PAR_get_int_callback (void *loc, int ncols, char **value, char **colname)
{
  gint *result;
  long int tmp;
  
  g_assert (ncols == 1);
  errno = 0;
  tmp = strtol (value[0], NULL, 10);   /* base 10 */
  g_assert (errno != ERANGE && errno != EINVAL);
  result = (gint *)loc;
  *result = (gint)tmp;
  return 0;
}



/**
 * Retrieves an integer parameter.
 *
 * @param db a parameter database.
 * @param query the query to retrieve the parameter.
 * @return the integer.
 */
gint
PAR_get_int (sqlite3 *db, char *query)
{
  gint result = 0;
  char *sqlerr;

  #if DEBUG
    g_debug ("----- ENTER PAR_get_int");
  #endif

  sqlite3_exec (db, query, PAR_get_int_callback, &result, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }

  #if DEBUG
    g_debug ("----- EXIT PAR_get_int");
  #endif

  return result;
}
/* end of file parameter.c */
