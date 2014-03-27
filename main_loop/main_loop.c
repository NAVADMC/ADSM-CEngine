/** @file main_loop.c
 * A simulator for animal disease outbreaks.
 *
 * @author Neil Harvey <nharve01@uoguelph.ca><br>
 *   School of Computer Science, University of Guelph<br>
 *   Guelph, ON N1G 2W1<br>
 *   CANADA
 * @author Aaron Reeves <Aaron.Reeves@colostate.edu><br>
 *   Animal Population Health Institute<br>
 *   Colorado State University<br>
 *   Fort Collins, CO 80523<br>
 *   USA
 * @author Shaun Case <ShaunCase@colostate.edu><br>
 *   Animal Population Health Institute<br>
 *   College of Veterinary Medicine and Biomedical Sciences<br>
 *   Colorado State University<br>
 *   Fort Collins, CO 80523<br>
 *   USA
 *
 * Copyright &copy; University of Guelph, and Colorado Statue University 2003-2010
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * @todo Create something to auto-generate all the boilerplate code for a new
 *   sub-model.
 */

/** @mainpage
 * A simulator for animal disease outbreaks.
 *
 * Notes to maintainer:
 *
 * The best way to get an overview of this software is to read the summary
 * documentation for the following files:
 * <ul>
 *   <li>
 *     unit.h - Units (herds, flocks, etc.) of animals are the basis of the
 *     simulation.  This file explains what information defines a unit and how
 *     units may be modified.
 *   <li>
 *     module.h - Modules simulate natural processes or human actions that
 *     affect units.  The simulator is modular: a "simulation" is simply the
 *     sum of the actions of whatever modules are running.  This file
 *     explains the interface that all modules must conform to.
 *   <li>
 *     event.h and event_manager.h - Explain the mechanism by which modules
 *     communicate.
 *   <li>
 *     zone.h - Zones are areas established around diseased units.  They can
 *     affect the behaviour of modules.  This file explains how zones work.
 * </ul>
 *
 * A few general notes on the implementation:
 * <ol>
 *   <li>
 *     The MPI implementation simply divides the desired number of
 *     (independent) Monte Carlo runs across the available processors.
 *   <li>
 *     The various <a href="annotated.html">data structures</a> should be
 *     treated as opaque and manipulated \em only through the functions
 *     defined in their header files.
 *     The functions will usually include
 *     <ul>
 *       <li>
 *         \a new, which returns a newly-allocated, initialized object, or NULL
 *         if there was not enough memory to create one.
 *       <li>
 *         \a free, which frees all memory used by an object.  If the function
 *         \em does \em not free all dynamically-allocated parts of the object,
 *         that is noted in the documentation.
 *       <li>
 *         \a to_string, \a printf, and \a fprintf, which exist to aid
 *         debugging.  The print functions \em always build a complete string in
 *         memory, then print it out, to prevent interleaving of output in a
 *         parallel environment.
 *     </ul>
 *   <li>
 *     Some objects (e.g.,
 *     <a href="prob__dist_8h.html">probability distributions</a>,
 *     <a href="event_8h.html">events</a>)
 *     are encapsulated in a supertype,
 *     implemented as a structure containing a type field and a union.
 *   <li>
 *     Basic data structures and message logging facilities from
 *     <a href="http://developer.gnome.org/doc/API/2.0/glib/">GLib</a>
 *     and special functions from the
 *     <a href="http://sources.redhat.com/gsl/ref/gsl-ref_toc.html">GNU Scientific Library</a>
 *     are used extensively.
 *
 *     The message logging facilities define severity levels (message, info,
 *     warning, error).  These are used to provide a "verbosity" level that the
 *     user can set, where
 *     <ul>
 *       <li>
 *         verbosity = 0 means just print the output of the simulation.
 *       <li>
 *         verbosity = 1 means also print detailed information about how
 *         calculations and decisions are being performed.
 *     </ul>
 *
 *     The verbosity is implemented by defining a "silent" log handler that
 *     simply ignores messages.  This means that when running with low
 *     verbosity, the program is wasting time building strings that are never
 *     displayed.  To eliminate this waste, specify "--disable-debug" when
 *     running the configure script.
 *   <li>
 *     It is assumed that text in the input XML may contain accented characters.
 *     The text is converted to ISO-8859-1 inside the simulator.
 *   <li>
 *     Comments are marked up for the auto-documentation tool
 *     <a href="http://www.doxygen.org/">Doxygen</a>, whose lovely output you
 *     are reading right now.
 *   <li>
 *     The testing harness is written with
 *     <a href="http://www.gnu.org/software/dejagnu/">DejaGnu</a>.
 *     DejaGnu is written in <a href="http://expect.nist.gov/">Expect</a>,
 *     which in turn uses <a href="http://www.tcl.tk/">Tcl</a>,
 *     so individual tests are written in Tcl.
 * </ol>
 */

/** @page complexity Complexity
 * We use R-trees for spatial operations.  R-trees are
 * commonly used in geographical databases.  Like an index in a book that lets
 * you quickly locate a particular topic, an R-tree is a spatial index that
 * lets you quickly find objects in a particular rectangular area.
 * If we can <i>quickly</i> locate the subset of units
 * that are close enough for a unit to interact with, we can potentially reduce
 * the number of interactions in medium- and large-scale simulations.
 *
 * We can measure the usefulness of a spatial index by running the following
 * test with and without one:
 * <ol>
 *   <li>Choose a unit at random.
 *   <li>Find all other units of the same production type within <i>d</i> km.
 *   <li>Repeat many times.
 * </ol>
 * Figure 4 shows the results.  The speedup is tremendous when <i>d</i> is
 * small compared to the entire study area, but when <i>d</i> is larger, using
 * the index can hurt performance.  In the figure, the notion of "size compared
 * to the entire study area" is quantified as the ratio of the diameter of the
 * search area to the short side of a minimum-area rectangle drawn around the
 * units (figure 5).
 *
 * @image html rtree_benefit.png "Figure 4. Speedup from using a spatial index."
 *
 * @image html ontario_tiled.png "Figure 5. An example of a minimum-area rectangle around a set of units."
 *
 * This result means that the program should take a "hybrid" approach, using an
 * R-tree index whenever a spatial search over a small area is needed and a
 * na&iuml;ve search through all units when a search over a large area is
 * needed.
 *
 * <b>How narrow a search?</b>
 *
 * There are 4 modules that work with spatial relationships in the model:
 * contact spread, airborne spread, ring vaccination, and ring destruction.
 * The last three work easily with an R-tree search because they have a
 * definite boundary past which they cannot have an effect: the distance at
 * which the probability of airborne spread drops to zero, or the radius of the
 * vaccination ring or the destruction ring.
 *
 * Contact spread lacks a definite boundary.  The model chooses a contact
 * distance <i>d</i>, and the contact goes to the unit whose distance from the
 * source is closest to <i>d</i>.  However, the closest potential recipient can
 * be arbitrarily far away.  So we use the R-tree to quickly search for a
 * recipient close to the source first, and if we do not find one, we fall back
 * to na&iuml;ve search.
 *
 * Figure 6 illustrates another concern when using a limited-area search for
 * contact spread.  If we search only out to distance <i>d</i>, we can miss the
 * correct recipient.  We avoid this problem by searching out to distance
 * 2<i>d</i>.  Figure 7 shows why this works.  View the scenario as a number
 * line where A is at position 0.  We know that <i>m</i> &lt; <i>d</i> (if
 * <i>d</i> &lt;= <i>m</i>, unit B is the correct choice for recipient and we
 * don't have a problem).  Because B is at the same location as A or to the
 * right of A, it follows that <i>x</i> &lt; <i>d</i>.  Given that <i>m</i>
 * &lt; <i>d</i> and <i>x</i> &lt; <i>d</i>, we can conclude that C = <i>m</i>
 * + <i>x</i> &lt; 2<i>d</i>.
 *
 * @image html contact_problem.png "Figure 6. Finding the recipient unit (B or C) that is closest to the distance d from unit A.  The dashed line is midway between B and C.  If we search only out to distance d, we will choose B as the recipient.  We must search further to find the correct recipient, C."
 *
 * @image html contact_proof.png "Figure 7. Searching out to distance 2d.  m is midway between B and C; x is the distance from B to m and from m to C.  See text for a proof that C will always be to the left of 2d."
 */

/** @page licenses Licenses of libraries and components
 * The table below lists terms and conditions on supporting libraries used by
 * this software.
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>GLib</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>generic data structures such as text and lists</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>The GTK+ Team (Peter Matthis et al.)</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       GNU Library General Public License (GNU LGPL)
 *       [<a href="http://www.gnu.org/copyleft/lgpl.html">read text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.gtk.org/">http://www.gtk.org/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>GNU Scientific Library (GSL)</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>probability calculations</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>M. Galassi et al.</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       GNU General Public License (GNU GPL)
 *       [<a href="http://www.gnu.org/copyleft/gpl.html">read text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.gnu.org/software/gsl/">http://www.gnu.org/software/gsl/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>Expat</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>reading XML files</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>James Clark</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       "Permission is hereby granted, free of charge, to ... deal in the
 *       Software without restriction, including without limitation the rights
 *       to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *       sell copies of the Software"
 *       [<a href="expat-COPYING.txt">read full text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://expat.sourceforge.net/">http://expat.sourceforge.net/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>Simple C Expat Wrapper (SCEW)</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>reading XML files</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Aleix Conchillo Flaque</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       GNU Lesser General Public License (GNU LGPL)
 *       [<a href="http://www.gnu.org/copyleft/lgpl.html">read text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.nongnu.org/scew/">http://www.nongnu.org/scew/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>R-tree library</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>spatial indexing</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Antonin Guttman and Daniel Green</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       "This code is placed in the public domain."
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.superliminal.com/sources/sources.htm">http://www.superliminal.com/sources/sources.htm</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>Wild Magic Library</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>geometry calculations and zones</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>David H. Eberly</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       "The Software may be used, edited, modified, copied, and distributed
 *       by you for commercial products provided that such products are not
 *       intended to wrap The Software solely for the purposes of selling it as
 *       if it were your own product.  The intent of this clause is that you
 *       use The Software, in part or in whole, to assist you in building your
 *       own original products."
 *       [<a href="http://www.magic-software.com/License/WildMagic.pdf">read full text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.magic-software.com/SourceCode.html">http://www.magic-software.com/SourceCode.html</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>planar convex hull code</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>automatic tiling</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Ken Clarkson</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       "Permission to use, copy, modify, and distribute this software for any
 *       purpose without fee is hereby granted"
 *       [<a href="2dch.txt">read full text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://cm.bell-labs.com/who/clarkson/">http://cm.bell-labs.com/who/clarkson/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>Shapefile C Library</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>writing ArcView files</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Frank Warmerdam</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       Library GNU Public License (LGPL)
 *       [<a href="http://www.gnu.org/copyleft/lesser.html">read text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://shapelib.maptools.org/">http://shapelib.maptools.org/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>GD graphics library</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>quick rendering of maps</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Thomas Boutell et al.</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       "Permission has been granted to copy, distribute and modify gd in any
 *       context without fee, including a commercial application"
 *       [<a href="http://www.boutell.com/gd/manual2.0.33.html#notice">read full text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.boutell.com/gd/">http://www.boutell.com/gd/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>General Polygon Clipper (gpc)</a></td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>geometry calculations</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Alan Murta</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>"free for non-commercial use"</td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://www.cs.man.ac.uk/aig/staff/alan/software/">http://www.cs.man.ac.uk/aig/staff/alan/software/</a>
 *     </td>
 *   </tr>
 * </table>
 *
 * <table>
 *   <tr>
 *     <td><b>Component:</b></td> <td>PROJ.4 Cartographic Projections library</td>
 *   </tr>
 *   <tr>
 *     <td><b>Used for:</b></td> <td>converting lat/long locations into x-y locations</td>
 *   </tr>
 *   <tr>
 *     <td><b>Author:</b></td> <td>Gerald Evenden et al.</td>
 *   </tr>
 *   <tr>
 *     <td><b>License:</b></td>
 *     <td>
 *       "Permission is hereby granted, free of charge, to any person obtaining
 *       a copy of this software and associated documentation files ... to deal
 *       in the Software without restriction, including without limitation the
 *       rights to use, copy, modify, merge, publish, distribute, sublicense,
 *       and/or sell copies of the Software"
 *       [<a href="http://trac.osgeo.org/proj/wiki/WikiStart#License">read full text</a>]
 *     </td>
 *   </tr>
 *   <tr>
 *     <td><b>Website:</b></td>
 *     <td>
 *       <a href="http://trac.osgeo.org/proj/">http://trac.osgeo.org/proj/</a>
 *     </td>
 *   </tr>
 * </table>
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif
/* #include <sys/times.h> */
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "unit.h"
#include "module_loader.h"
#include "event_manager.h"
#include "reporting.h"
#include "rng.h"
#include "spatial_search_rtree.h"

#ifdef USE_SC_GUILIB
#include "sc_spreadmodel_outputs.h"
#endif

#if HAVE_MPI && !CANCEL_MPI
#  include "mpix.h"
#endif

#if STDC_HEADERS
#  include <string.h>
#endif

#if HAVE_STRINGS_H
#  include <strings.h>
#endif

#if HAVE_MATH_H
#  include <math.h>
#endif

#if HAVE_CTYPE_H
#  include <ctype.h>
#endif

#if HAVE_ERRNO_H
#  include <errno.h>
#endif

/*
main.c needs access to the functions defined in spreadmodel.h,
even when compiled as a *nix executable (in which case,
the functions defined will all be NULL).
*/
#include "spreadmodel.h"

#include "general.h"

#define G_LOG_ALL_LEVELS (G_LOG_LEVEL_CRITICAL | \
                    G_LOG_LEVEL_DEBUG | \
                    G_LOG_LEVEL_ERROR | \
                    G_LOG_LEVEL_INFO | \
                    G_LOG_LEVEL_WARNING )
#define	G_LOG_ALERT_LEVELS (G_LOG_LEVEL_CRITICAL | \
                      G_LOG_LEVEL_ERROR | \
                      G_LOG_LEVEL_WARNING)

extern gboolean use_fixed_poisson;
extern double poisson_fix;



/**
 * A log handler that simply discards messages.  "Info" and "debug" level
 * messages are directed to this at low verbosity levels.
 */
void
silent_log_handler (const gchar * log_domain, GLogLevelFlags log_level,
                    const gchar * message, gpointer user_data)
{
  ;
}



/**
 * Pull apart the program's version number, available in the constant
 * PACKAGE_VERSION, into major, minor, and release numbers.
 */
static void
split_version (const char *version, RPT_reporting_t *split)
{
  gchar **tokens;
  gchar **iter;
  guint i;
  long num;
  
  tokens = g_strsplit (version, ".", 3);
  for (iter = tokens, i = 0; *iter != NULL; iter++, i++)
    {
      num = strtol (*iter, NULL, 10);
      if (i == 0)
        RPT_reporting_set_integer1 (split, num, "major");
      else if (i == 1)
        RPT_reporting_set_integer1 (split, num, "minor");
      else
        RPT_reporting_set_integer1 (split, num, "release");
    }
  g_strfreev (tokens);

  return;	
}

 

/**
 * Create a default map projection to use if no preferred one is supplied.
 *
 * Side effects: after this function runs, the unit list will have a bounding
 * box defined.  The box will be an unoriented rectangle.
 *
 * @image html albers.gif "Albers equal area conic projection.  Public domain image from USGS."
 *
 * @param units the unit list.
 * @return a newly-allocated projPJ object.  projPJ is actually a pointer data
 *   type, but the fact that it is a pointer is "hidden" by a typedef.
 */
projPJ
default_projection (UNT_unit_list_t * units)
{
  unsigned int nunits, i;
  UNT_unit_t *unit;
  double min_lat, min_lon, max_lat, max_lon;
  double lat_range, center_lon, sp1, sp2;
  projPJ projection;
  char *projection_args;
#if DEBUG
  char *s;
#endif

#if DEBUG
  g_debug ("----- ENTER default_projection");
#endif

  /* Get the lat and lon limits, which we will use to set up the projection. */
  nunits = UNT_unit_list_length (units);
  if (nunits == 0)
    min_lat = max_lat = min_lon = max_lon = 0;
  else
    {
      /* Initialize with the position of the first unit. */
      unit = UNT_unit_list_get (units, 0);
      min_lat = max_lat = unit->latitude;
      min_lon = max_lon = unit->longitude;

      for (i = 1; i < nunits; i++)
        {
          unit = UNT_unit_list_get (units, i);
          if (unit->latitude < min_lat)
            min_lat = unit->latitude;
          else if (unit->latitude > max_lat)
            max_lat = unit->latitude;

          if (unit->longitude < min_lon)
            min_lon = unit->longitude;
          else if (unit->longitude > max_lon)
            max_lon = unit->longitude;
        }
    }
  center_lon = (min_lon + max_lon) / 2.0;

  /* If the latitude range is very close to the equator or contains the
   * equator, use a cylindrical equal area projection.  (The Albers equal area
   * conic projection becomes the cylindrical equal area when its parallels are
   * at the equator.) */
  if ((min_lat > -1 && max_lat < 1) || (min_lat * max_lat < 0))
    {
#if DEBUG
      g_debug ("study area near equator, using cylindrical equal area projection");
#endif
      projection_args = g_strdup_printf ("+ellps=WGS84 +units=km +lon_0=%g +lat_0=%g +proj=cea", center_lon, min_lat);
      projection = pj_init_plus (projection_args);
    }
  else
    {
#if DEBUG
      g_debug ("using Albers equal area conic projection");
#endif
      lat_range = max_lat - min_lat;
      sp1 = min_lat + lat_range / 6.0;
      sp2 = max_lat - lat_range / 6.0;
      projection_args =
        g_strdup_printf ("+ellps=WGS84 +units=km +lon_0=%g +proj=aea +lat_0=%g +lat_1=%g +lat_2=%g",
                         center_lon, min_lat, sp1, sp2);
      projection = pj_init_plus (projection_args);
      if (!projection)
        {
          g_error ("could not create map projection object: %s", pj_strerrno(pj_errno));
        }
    }
  g_free (projection_args);
#if DEBUG
  s = pj_get_def (projection, 0);
  g_debug ("projection = %s", s);
#endif

#if DEBUG
  g_debug ("----- EXIT default_projection");
#endif

  return projection;
}



#ifdef USE_SC_GUILIB
DLL_API void
run_sim_main (const char *population_file,
              sqlite3 *parameter_db,
              const char *output_dir, double fixed_rng_value, int verbosity, int seed, char *production_type_file)
#else
DLL_API void
run_sim_main (const char *population_file,
              sqlite3 *parameter_db,
              const char *output_dir, double fixed_rng_value, int verbosity, int seed)
#endif
{
  unsigned int ndays, nruns, day, run;
  double prevalence_num, prevalence_denom;
  RPT_reporting_t *num_units_in_state;
  RPT_reporting_t *num_units_in_state_by_prodtype;
  RPT_reporting_t *num_animals_in_state;
  RPT_reporting_t *num_animals_in_state_by_prodtype;
  RPT_reporting_t *avg_prevalence;
  RPT_reporting_t *last_day_of_disease;
  RPT_reporting_t *last_day_of_outbreak;
  RPT_reporting_t *clock_time;
  RPT_reporting_t *version;
  GPtrArray *reporting_vars;
  int nmodels = 0;
  spreadmodel_model_t **models = NULL;
  spreadmodel_event_manager_t *manager;
  unsigned int nunits;
  UNT_unit_list_t *units;
  UNT_unit_t *unit;
  gsl_rng *gsl_format_rng;
  RAN_gen_t *rng;
  unsigned int nzones;
  ZON_zone_list_t *zones;
  ZON_zone_t *zone;
  int i, j;                     /* loop counters */
  const char *drill_down_list[3] = { NULL, NULL, NULL };
  gboolean active_infections_yesterday, active_infections_today,
    pending_actions, pending_infections, disease_end_recorded,
    stop_on_disease_end, early_exit;
  time_t start_time, finish_time;
  guint exit_conditions = 0;
  double m_total_time, total_processor_time;
  unsigned long total_runs;
  m_total_time = total_processor_time = 0.0;

#ifdef USE_SC_GUILIB
  GPtrArray *production_types;

  production_types = NULL;
  /* _scenario.version defined as 50 chars wide in general.h */
  g_snprintf( _scenario.version, 50, "Version: %s, Spec: %s", current_version(), specification_version() );
#endif

  /* This line prints a Byte Order Mark (BOM) that indicates that the output is
   * in UTF-8.  Not currently used. */
  /*
  if (NULL == spreadmodel_printf)
    g_print ("%s", "\xEF\xBB\xBF");
  */

  /* Set the verbosity level. */
  if (verbosity < 1)
    {
      g_log_set_handler (NULL, G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
      g_log_set_handler ("unit", G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
      g_log_set_handler ("prob_dist", G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
      g_log_set_handler ("rel_chart", G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
      g_log_set_handler ("reporting", G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
      g_log_set_handler ("zone", G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
      g_log_set_handler ("gis", G_LOG_LEVEL_DEBUG, silent_log_handler, NULL);
    }
  #ifdef WIN_DLL
    #if DEBUG
      /* #define G_LOG_DOMAIN "debug_off" to disable logging for selected units,
       * or #define G_LOG_DOMAIN "debug_on" to enable logging only for selected units. */
        
      g_log_set_handler (NULL, G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("unit", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("prob_dist", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("rel_chart", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("reporting", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("zone", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("gis", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("debug_off", G_LOG_ALL_LEVELS, silent_log_handler, NULL);
      g_log_set_handler ("debug_on", G_LOG_ALL_LEVELS, spreadmodel_log_handler, NULL);
      
      g_debug ("This will be the first debugging message reported to the GUI."); 
    #else
      g_log_set_handler (NULL, G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("unit", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("prob_dist", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("rel_chart", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("reporting", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("zone", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("gis", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("debug_off", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL);
      g_log_set_handler ("debug_on", G_LOG_ALERT_LEVELS, spreadmodel_log_handler, NULL); 
    #endif    
  #endif
  
  #if DEBUG
    g_debug ("verbosity = %i", verbosity);
  #endif

#ifdef USE_SC_GUILIB
  if ( NULL != production_type_file )
  {
    production_types = PRT_load_production_type_list ( production_type_file );
  };
#endif
  /* Get the list of units. */
  if (population_file)
    {
#ifdef USE_SC_GUILIB
      units = UNT_load_unit_list ( population_file, production_types );
#else
      units = UNT_load_unit_list ( population_file );
#endif
      nunits = UNT_unit_list_length (units);
    }
  else
    {
      units = NULL;
      nunits = 0;
    }

#if DEBUG
  g_debug ("%i units read", nunits);
#endif
  if (nunits == 0)
    {
      g_error ("no units in file %s", population_file);
    }

  /* Project the unit locations onto a flat map, if they aren't already. */
  if (units->projection == NULL)
    {
      units->projection = default_projection (units);
      UNT_unit_list_project (units, units->projection);
    }
  /* Build a spatial index around the unit locations. */
  units->spatial_index = new_rtree_spatial_search ();
  for (i = 0; i < nunits; i++)
    {
      unit = UNT_unit_list_get (units, i);
      spatial_search_add_point (units->spatial_index, unit->x, unit->y);
    }
  spatial_search_prepare (units->spatial_index);

  /* Initialize the reporting variables, and bundle them together so they can
   * easily be sent to a function for initialization. */
  num_units_in_state = RPT_new_reporting ("tsdU", RPT_group, RPT_never);
  num_units_in_state_by_prodtype =
    RPT_new_reporting ("num-units-in-each-state-by-production-type", RPT_group, RPT_never);
  num_animals_in_state = RPT_new_reporting ("tsdA", RPT_group, RPT_never);
  num_animals_in_state_by_prodtype =
    RPT_new_reporting ("num-animals-in-each-state-by-production-type", RPT_group, RPT_never);
  for (i = 0; i < UNT_NSTATES; i++)
    {
      RPT_reporting_set_integer1 (num_units_in_state, 0, UNT_state_name[i]);
      RPT_reporting_set_integer1 (num_animals_in_state, 0, UNT_state_name[i]);
      drill_down_list[1] = UNT_state_name[i];
      for (j = 0; j < units->production_type_names->len; j++)
        {
          drill_down_list[0] = (char *) g_ptr_array_index (units->production_type_names, j);
          RPT_reporting_set_integer (num_units_in_state_by_prodtype, 0, drill_down_list);
          RPT_reporting_set_integer (num_animals_in_state_by_prodtype, 0, drill_down_list);
        }
    }
  avg_prevalence = RPT_new_reporting ("average-prevalence", RPT_real, RPT_never);
  last_day_of_disease =
    RPT_new_reporting ("diseaseDuration", RPT_integer, RPT_never);
  last_day_of_outbreak =
    RPT_new_reporting ("outbreakDuration", RPT_integer, RPT_never);
  clock_time = RPT_new_reporting ("clock-time", RPT_real, RPT_never);
  version = RPT_new_reporting ("version", RPT_group, RPT_never);
  split_version (PACKAGE_VERSION, version);
  reporting_vars = g_ptr_array_new ();
  g_ptr_array_add (reporting_vars, num_units_in_state);
  g_ptr_array_add (reporting_vars, num_units_in_state_by_prodtype);
  g_ptr_array_add (reporting_vars, num_animals_in_state);
  g_ptr_array_add (reporting_vars, num_animals_in_state_by_prodtype);
  g_ptr_array_add (reporting_vars, avg_prevalence);
  g_ptr_array_add (reporting_vars, last_day_of_disease);
  g_ptr_array_add (reporting_vars, last_day_of_outbreak);
  g_ptr_array_add (reporting_vars, clock_time);
  g_ptr_array_add (reporting_vars, version);

  /* Pre-create a "background" zone. */
  zones = ZON_new_zone_list (nunits);
  zone = ZON_new_zone ("", -1, 0.0);
#ifdef USE_SC_GUILIB
  zone->_unitDays = NULL;
  zone->_animalDays = NULL;
#endif
  ZON_zone_list_append (zones, zone);

  /* Get the simulation parameters. */
  nmodels =
    spreadmodel_load_modules (parameter_db, units, units->projection, zones,
                              &ndays, &nruns, &models, reporting_vars, &exit_conditions );
  nzones = ZON_zone_list_length (zones);

  /* The clock time reporting variable is special -- it can only be reported
   * once (at the end of each simulation) or never. */
  if (clock_time->frequency != RPT_never && clock_time->frequency != RPT_once)
    {
      g_warning ("clock-time cannot be reported %s; it will reported at the end of each simulation",
                 RPT_frequency_name[clock_time->frequency]);
      RPT_reporting_set_frequency (clock_time, RPT_once);
    }

#if HAVE_MPI && !CANCEL_MPI
  /* Increase the number of runs to divide evenly by the number of processors,
   * if necessary. */
  if (nruns % me.np != 0)
    nruns += (me.np - nruns % me.np);
  nruns /= me.np;               /* because it's parallel, wheee! */
  _scenario.nruns = nruns;
#endif

#if DEBUG
  g_debug ("simulation %u days x %u runs", ndays, nruns);
#endif


  /* Initialize the pseudo-random number generator. */
  gsl_rng_env_setup();
  gsl_format_rng = gsl_rng_alloc (gsl_rng_taus2);
#ifdef USE_SC_GUILIB
  if ( _scenario.random_seed == 0 )
    gsl_rng_set (gsl_format_rng, time(NULL));
  else
    gsl_rng_set (gsl_format_rng, _scenario.random_seed );
#elsif HAVE_MPI && !CANCEL_MPI
  if (seed >= 0)
    gsl_rng_set (gsl_format_rng, seed + me.rank);
  else
    gsl_rng_set (gsl_format_rng, time(NULL) + me.rank);
#else
  if (seed >= 0)
    gsl_rng_set (gsl_format_rng, seed);
  else
    gsl_rng_set (gsl_format_rng, time(NULL));
#endif
  rng = RAN_new_generator (gsl_format_rng);
  if (fixed_rng_value >= 0 && fixed_rng_value < 1)
    {
      RAN_fix (rng, fixed_rng_value);
    }

  manager = spreadmodel_new_event_manager (models, nmodels);

  /* Determine whether each iteration should end when the active disease phase ends. */
  stop_on_disease_end = (0 != get_stop_on_disease_end( exit_conditions ) );

  m_total_time = total_processor_time = 0.0;
  total_runs = 0;

#ifdef USE_SC_GUILIB
  sc_sim_start( units, production_types, zones );
#else
  if (NULL != spreadmodel_sim_start)
    spreadmodel_sim_start ();
#endif


/*
#ifdef USE_SC_GUILIB
  write_scenario_SQL();
  write_job_SQL();
  write_production_types_SQL( production_types );
  write_zones_SQL( zones );
  fflush(NULL);
#endif
*/

  if (output_dir != NULL)
    spreadmodel_create_event (manager, EVT_new_output_dir_event((char *)output_dir), units, zones, rng);
  spreadmodel_create_event (manager, EVT_new_declaration_of_outputs_event(reporting_vars), units, zones, rng);

  /* Begin the loop over the specified number of iterations. */
  spreadmodel_create_event (manager, EVT_new_before_any_simulations_event(), units, zones, rng);
  for (run = 0; run < nruns; run++)
    {

#if defined( USE_MPI ) && !CANCEL_MPI
      double m_start_time, m_end_time;
      m_start_time = m_end_time = 0.0;
      m_start_time = MPI_Wtime();
#endif

    _iteration.zoneFociCreated = FALSE;
    _iteration.diseaseEndDay = -1;
    _iteration.outbreakEndDay = -1;
    _iteration.first_detection = FALSE;

      /* Does the GUI user want to stop a simulation in progress? */
      if (NULL != spreadmodel_simulation_stop)
        {
          if (0 != spreadmodel_simulation_stop ())
            break;
        }

      #if DEBUG
        g_debug ("resetting everything before start of simulation");
      #endif

/*
#error TODO:  After removing the infectious_unit updates from the sc_spreadmodel functions make sure to call them from near here if the optimizations have been defined
*/
#ifdef USE_SC_GUILIB
      sc_iteration_start ( production_types, units,  run);
#else
      if (NULL != spreadmodel_iteration_start)
        spreadmodel_iteration_start (run);
#endif

      if ( _iteration.infectious_units != NULL )
      {
        g_hash_table_destroy( _iteration.infectious_units );
        _iteration.infectious_units = NULL;
      };
      _iteration.infectious_units = g_hash_table_new( g_direct_hash, g_direct_equal );

      /* Reset reporting variables. */
      RPT_reporting_set_null (last_day_of_disease, NULL);
      RPT_reporting_set_null (last_day_of_outbreak, NULL);

      /* Reset all models. */
      for (i = 0; i < nmodels; i++)
        models[i]->reset (models[i]);

      /* Reset all zones. */
      ZON_zone_list_reset (zones);

      active_infections_yesterday = TRUE;
      pending_actions = TRUE;
      pending_infections = TRUE;
      disease_end_recorded = FALSE;
      early_exit = FALSE;

      spreadmodel_create_event (manager, EVT_new_before_each_simulation_event(), units, zones, rng);

      /* Run the iteration. */
      start_time = time (NULL);

      /* Begin the loop over the days in an iteration. */
      for (day = 1; (day <= ndays) && (!early_exit); day++)
        {
#if defined( USE_MPI ) && !CANCEL_MPI
          double m_day_start_time = MPI_Wtime();
          double m_day_end_time = m_day_start_time;
#endif
          /* Does the GUI user want to stop a simulation in progress? */
          if (NULL != spreadmodel_simulation_stop)
            {
              /* This check may break the day loop.
               * If necessary, Another check (see above) will break the iteration loop.*/
              if (0 != spreadmodel_simulation_stop ())
                break;
            }

          /* Should the iteration end due to first detection? */
      if ( _iteration.first_detection && (0 != get_stop_on_first_detection( exit_conditions )) )
      break;


      _iteration.current_day = day;
#ifdef USE_SC_GUILIB
          sc_day_start( production_types );
#else
          if (NULL != spreadmodel_day_start)
            spreadmodel_day_start (day);
#endif

#if DEBUG && defined( USE_MPI )
          double m_start_day_time = MPI_Wtime();
#endif
          /* Process changes made to the units and zones on the previous day. */
          spreadmodel_create_event (manager, EVT_new_midnight_event (day), units, zones, rng);

          /* Count the number of units and animals infected, vaccinated, and
           * destroyed, and the number of units and animals in each state. */
          RPT_reporting_zero (num_units_in_state);
          RPT_reporting_zero (num_animals_in_state);
          prevalence_num = prevalence_denom = 0;

          for (i = 0; i < nunits; i++)
            {
              unit = UNT_unit_list_get (units, i);

              RPT_reporting_add_integer1 (num_units_in_state, 1, UNT_state_name[unit->state]);
              RPT_reporting_add_integer1 (num_animals_in_state, unit->size,
                                          UNT_state_name[unit->state]);
              drill_down_list[0] = unit->production_type_name;
              drill_down_list[1] = UNT_state_name[unit->state];
              RPT_reporting_add_integer (num_units_in_state_by_prodtype, 1, drill_down_list);
              RPT_reporting_add_integer (num_animals_in_state_by_prodtype, unit->size,
                                         drill_down_list);

              if (unit->state >= Latent && unit->state <= InfectiousClinical)
                {
                  prevalence_num += unit->size * unit->prevalence;
                  prevalence_denom += unit->size;
                }
              RPT_reporting_set_real (avg_prevalence, (prevalence_denom > 0) ?
                                      prevalence_num / prevalence_denom : 0, NULL);
            }                   /* end loop over units */
          active_infections_today = (
            RPT_reporting_get_integer1 (num_units_in_state, UNT_state_name[Latent]) > 0
            || RPT_reporting_get_integer1 (num_units_in_state, UNT_state_name[InfectiousSubclinical]) > 0
            || RPT_reporting_get_integer1 (num_units_in_state, UNT_state_name[InfectiousClinical]) > 0
          );

          /* Run the models to get today's changes. */
          spreadmodel_create_event (manager, EVT_new_new_day_event (day), units, zones, rng);

          /* Check if the outbreak is over, and if so, whether we can exit this
           * Monte Carlo trial early. */

          /* Check first for active infections... */
          if (active_infections_yesterday)
            {
              if (!active_infections_today)
                {
                  ;
                  #if DEBUG
                    g_debug ("no more active infections");
                  #endif
                }
            }
          else /* there were no active infections yesterday */
            {
              if (active_infections_today)
                {
                  ;
                  #if DEBUG
                    g_debug ("active infections again");
                  #endif
                }
            }

          /* Should the end of the disease phase be recorded? */
          if (!disease_end_recorded && !active_infections_today && !pending_infections)
            {

#ifdef USE_SC_GUILIB
              sc_disease_end( day );
#else
              if (NULL != spreadmodel_disease_end)
                spreadmodel_disease_end (day);
#endif
              RPT_reporting_set_integer (last_day_of_disease, day - 1, NULL);
              disease_end_recorded = TRUE;
            }


          /* Check the early exit conditions.  If the user wants to exit when
           * the active disease phase ends, then active_infections and pending_infections
           * must both be false to exit early.
           *
           * Otherwise, active_infections and pending_actions must both be false
           * to exit early.
           */
          if (stop_on_disease_end)
            {
              if (!active_infections_today && !pending_infections)
                {
                  #if DEBUG
                    g_debug ("can exit early on end of disease phase");
                  #endif
                  early_exit = TRUE;
                }
            }
          else
            {
              if (!active_infections_today && !pending_actions)
                {
                  #if DEBUG
                    g_debug ("can exit early on end of outbreak");
                  #endif
#ifdef USE_SC_GUILIB
          sc_outbreak_end( day );
#else
                  if (NULL != spreadmodel_outbreak_end)
                    spreadmodel_outbreak_end (day);
#endif
                  RPT_reporting_set_integer (last_day_of_outbreak, day - 1, NULL);
                  early_exit = TRUE;
                }
            }
          active_infections_yesterday = active_infections_today;

          spreadmodel_create_event (manager, EVT_new_end_of_day_event (day, early_exit), units, zones, rng);

          /* Next, check for pending actions... */
          pending_actions = FALSE;
          for (i = 0; i < nmodels; i++)
            {
              if (models[i]->has_pending_actions (models[i]))
                {
                  #if DEBUG
                    g_debug ("%s has pending actions", models[i]->name);
                  #endif
                  pending_actions = TRUE;
                  break;
                }
            }


          /* And finally, check for pending infections. */
          pending_infections = FALSE;
          for (i = 0; i < nmodels; i++)
            {
              if (models[i]->has_pending_infections (models[i]))
                {
                  #if DEBUG
                    g_debug ("%s has pending infections", models[i]->name);
                  #endif
                  pending_infections = TRUE;
                  break;
                }
            }


          if (early_exit || day == ndays)
            {
              finish_time = time (NULL);
              RPT_reporting_set_real (clock_time, (double) (finish_time - start_time), NULL);
              spreadmodel_create_event (manager, EVT_new_last_day_event (day), units, zones, rng);
            }
          spreadmodel_create_event (manager, EVT_new_end_of_day2_event (day, early_exit || day == ndays), units, zones, rng);

          if (NULL != spreadmodel_show_all_prevalences)
            {
              char *prev_summary = UNT_unit_list_prevalence_to_string (units, day);
              spreadmodel_show_all_prevalences (prev_summary);
              free (prev_summary);
            }

          if (NULL != spreadmodel_show_all_states)
            {
              char *summary = UNT_unit_list_summary_to_string (units);
              spreadmodel_show_all_states (summary);
              free (summary);
            }

          if (NULL != spreadmodel_set_zone_perimeters)
            spreadmodel_set_zone_perimeters (zones);

#ifdef USE_SC_GUILIB
          sc_day_complete( day, run, production_types, zones );
#else
          if (NULL != spreadmodel_day_complete)
            spreadmodel_day_complete (day);
#endif

#if defined( USE_MPI ) && !CANCEL_MPI
  m_day_end_time = MPI_Wtime();
  g_debug( "%i: Iteration: %i, Day: %i, Day_Time: %g\n", me.rank, run, day, (double)(m_day_end_time - m_day_start_time ) );
#endif
        } /* end loop over days of one Monte Carlo trial */

#ifdef USE_SC_GUILIB
      sc_iteration_complete( zones, units, production_types, run );
#else
      if (NULL != spreadmodel_iteration_complete)
        spreadmodel_iteration_complete (run);
#endif

#if defined( USE_MPI ) && !CANCEL_MPI
  m_end_time = MPI_Wtime();
  m_total_time = (double)((((double)m_end_time - (double)m_start_time)) + (double)m_total_time);
  #ifdef DEBUG
      g_debug("%i - Run: %d timing: startCount %g, endCount %g, totalCount %g MPI Timer Seconds\n", me.rank, me.rank * _scenario.nruns + run + 1, m_start_time, m_end_time, (double)((double)m_end_time - (double)m_start_time));
  #endif
#endif
    }                           /* loop over all Monte Carlo trials */

#ifdef USE_SC_GUILIB

#if defined( USE_MPI ) && !CANCEL_MPI
    if ( me.np > 1 )
    {
      MPI_Reduce( &m_total_time, &total_processor_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD );
      MPI_Reduce( &run, &total_runs, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD );
    }
    else
#endif
    {
      total_processor_time = m_total_time;
      total_runs = run;
    };
#if defined( USE_MPI ) && !CANCEL_MPI
    if ( me.rank == 0 )
#endif
    {
      _scenario.total_processor_time = total_processor_time;
      _scenario.iterations_completed = total_runs;
      sc_sim_complete( -1, units, production_types, zones );
    };
#else
  /* Inform the GUI that the simulation has ended */
  if (NULL != spreadmodel_sim_complete)
    {
      if (-1 == spreadmodel_simulation_stop ())
        {
          /* simulation was interrupted by the user and did not complete. */
          spreadmodel_sim_complete (0);
        }
      else
        {
          /* Simulation ran to completion. */
          spreadmodel_sim_complete (-1);
        }
    }
#endif

  /* Clean up. */
  RPT_free_reporting (num_units_in_state);
  RPT_free_reporting (num_units_in_state_by_prodtype);
  RPT_free_reporting (num_animals_in_state);
  RPT_free_reporting (num_animals_in_state_by_prodtype);
  RPT_free_reporting (avg_prevalence);
  RPT_free_reporting (last_day_of_disease);
  RPT_free_reporting (last_day_of_outbreak);
  RPT_free_reporting (clock_time);
  RPT_free_reporting (version);
  spreadmodel_free_event_manager (manager);
  spreadmodel_unload_modules (nmodels, models);
  RAN_free_generator (rng);
  ZON_free_zone_list (zones);
  UNT_free_unit_list (units);

  return;
}

/* end of file main_loop.c */
