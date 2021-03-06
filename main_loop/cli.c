/** @file cli.c
 * A command-line interface for ADSM.
 *
 * @author Neil Harvey <nharve01@uoguelph.ca><br>
 *   School of Computer Science, University of Guelph<br>
 *   Guelph, ON N1G 2W1<br>
 *   CANADA
 *
 * Copyright &copy; University of Guelph and Colorado State University 2010
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "adsm.h"
#include "general.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>



#define ADSM_CLI_ERROR adsm_cli_error_quark()

GQuark
adsm_cli_error_quark (void)
{
  return g_quark_from_static_string ("ADSM_CLI_ERROR");
}



int
main (int argc, char *argv[])
{
  int verbosity = 0;
  const char *scenario_db_name = NULL;
  const char *output_dir = NULL;
  double fixed_rng_value = -1;
  int seed = -1;
  int starting_iteration_number = -1;
  gboolean dry_run = FALSE;
  gboolean version = FALSE;
  GError *option_error = NULL;
  GOptionContext *context;
  GOptionEntry options[] = {
    { "verbosity", 'V', 0, G_OPTION_ARG_INT, &verbosity, "Message verbosity level (0 = simulation output only, 1 = all debugging output)", NULL },
    { "output-dir", 'o', 0, G_OPTION_ARG_FILENAME, &output_dir, "Output directory", NULL },
    { "fixed-random-value", 'r', 0, G_OPTION_ARG_DOUBLE, &fixed_rng_value, "Fixed number to use instead of random numbers", NULL },
    { "rng-seed", 's', 0, G_OPTION_ARG_INT, &seed, "Seed used to initialize the random number generator", NULL },
    { "iteration-number", 'i', 0, G_OPTION_ARG_INT, &starting_iteration_number, "Number of the first iteration", NULL },
    { "dry-run", 'n', 0, G_OPTION_ARG_NONE, &dry_run, "Check the parameters but do not actually run the simulation", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &version, "Print the version number and exit", NULL },
    { NULL }
  };
  int sqlerr;
  sqlite3 *scenario_db;
  GError *error = NULL;
  int exit_status = EXIT_SUCCESS;

  clear_adsm_fns ();

#ifdef USE_SC_GUILIB
  _scenario.scenarioId = NULL;
  _scenario.description = NULL;
  _scenario.nruns = 0;
  _scenario.random_seed = 0;
  _scenario.start_time = _scenario.end_time = 0;

  _iteration.susceptible_units = NULL;
  _iteration.infectious_units = NULL;
  _iteration._unitsInZones = NULL;
  _iteration.zoneFociCreated = FALSE;
  _iteration.diseaseEndDay = -1;
  _iteration.outbreakEndDay = -1;
  _iteration.first_detection = FALSE;
#endif

  init_MAIN_structs();

  context = g_option_context_new ("- Runs epidemiological simulations for animal populations");
  g_option_context_add_main_entries (context, options, /* translation = */ NULL);
  if (!g_option_context_parse (context, &argc, &argv, &option_error))
    {
      g_set_error (&error, ADSM_CLI_ERROR, 0, "option parsing failed: %s", option_error->message);
    }

  if (!error)
    {
      if (version)
        {
          g_print ("%s\n", PACKAGE_VERSION);
        }
      else
        {
          if (argc >= 2)
            scenario_db_name = argv[1];
          else
            {
              g_set_error (&error, ADSM_CLI_ERROR, 0, "Need name of scenario database");
            }
          g_option_context_free (context);
        
          if (!error)
            {
              /* If an output directory was specified, and that directory does not exist,
               * create the directory. */
              {
                /* There will be a "Map" directory inside the output directory too. We can
                 * make the output directory and the subdirectory inside with one call to
                 * g_mkdir_with_parents. */
                gint errcode;
                gchar *map_output_dir;
                if (output_dir == NULL)
                  map_output_dir = g_strdup ("Map");
                else
                  map_output_dir = g_build_filename (output_dir, "Map", NULL);
                errcode = g_mkdir_with_parents (map_output_dir, S_IRUSR + S_IWUSR + S_IXUSR);
                if (errcode != 0)
                  {
                    g_error ("could not create output directory \"%s\": %s",
                             map_output_dir, strerror(errno));
                  }
                g_free (map_output_dir);
              }
            }
        
          if (!error)
            {
              sqlerr = sqlite3_open_v2 (scenario_db_name, &scenario_db, SQLITE_OPEN_READONLY, NULL);
              if (sqlerr !=  SQLITE_OK)
                {
                  g_error ("Error opening scenario database: %s", sqlite3_errmsg (scenario_db));
                }
            }
        
          if (!error)
            {
              sqlite3_busy_timeout (scenario_db, 30 * 60 * 1000 /* 30 minutes, given in milliseconds */);
        
              run_sim_main (scenario_db,
                            (char *)output_dir,
                            fixed_rng_value,
                            verbosity,
                            seed,
                            starting_iteration_number,
                            dry_run,
                            &error);
        
              /* We don't close the database connection here because run_sim_main
               * closes it as soon as it has finished reading. */
            }
        } /* end of case where we don't just output the version number and exit */
    } /* end of case where command line option parsing succeeded */

  if (error)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      exit_status = EXIT_FAILURE;
    }

  return exit_status;
}

/* end of file cli.c */
