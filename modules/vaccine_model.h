/** @file vaccine_model.h
 *
 * @author Aaron Reeves <Aaron.Reeves@colostate.edu><br>
 *   Animal Population Health Institute<br>
 *   College of Veterinary Medicine and Biomedical Sciences<br>
 *   Colorado State University<br>
 *   Fort Collins, CO 80523<br>
 *   USA
 * @date June 2005
 *
 * Copyright &copy; 2005 Animal Population Health Institute, Colorado State University
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef VACCINE_MODEL_H
#define VACCINE_MODEL_H

adsm_module_t *vaccine_model_new (sqlite3 *, UNT_unit_list_t *,
                                  projPJ, ZON_zone_list_t *, GError **);

#endif
