/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 * \brief Generic geometry attributes built on CustomData.
 */

#pragma once

#include "BLI_sys_types.h"

#include "BKE_customdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CustomData;
struct CustomDataLayer;
struct ID;
struct ReportList;

/* Attribute.domain */
typedef enum AttributeDomain {
  ATTR_DOMAIN_AUTO = -1,    /* Use for nodes to choose automatically based on other data. */
  ATTR_DOMAIN_POINT = 0,    /* Mesh, Curve or Point Cloud Point */
  ATTR_DOMAIN_EDGE = 1,     /* Mesh Edge */
  ATTR_DOMAIN_FACE = 2,     /* Mesh Face */
  ATTR_DOMAIN_CORNER = 3,   /* Mesh Corner */
  ATTR_DOMAIN_CURVE = 4,    /* A single curve in a larger curve data-block */
  ATTR_DOMAIN_INSTANCE = 5, /* Instance */

  ATTR_DOMAIN_NUM
} AttributeDomain;

/* Attributes */

bool BKE_id_attributes_supported(struct ID *id);

struct CustomDataLayer *BKE_id_attribute_new(
    struct ID *id, const char *name, int type, AttributeDomain domain, struct ReportList *reports);
bool BKE_id_attribute_remove(struct ID *id,
                             struct CustomDataLayer *layer,
                             struct ReportList *reports);

struct CustomDataLayer *BKE_id_attribute_find(const struct ID *id,
                                              const char *name,
                                              int type,
                                              AttributeDomain domain);

/* Return an existing CustomDataLayer, or create a new one if none is found for the given
 * parameters and return it. */
struct CustomDataLayer *BKE_id_attribute_ensure(struct ID *id,
                                                const char *name,
                                                const int type,
                                                const AttributeDomain domain,
                                                struct ReportList *reports);

AttributeDomain BKE_id_attribute_domain(struct ID *id, struct CustomDataLayer *layer);
int BKE_id_attribute_data_length(struct ID *id, struct CustomDataLayer *layer);
bool BKE_id_attribute_required(struct ID *id, struct CustomDataLayer *layer);
bool BKE_id_attribute_rename(struct ID *id,
                             struct CustomDataLayer *layer,
                             const char *new_name,
                             struct ReportList *reports);

int BKE_id_attributes_length(struct ID *id, CustomDataMask mask);

struct CustomDataLayer *BKE_id_attributes_active_get(struct ID *id);
void BKE_id_attributes_active_set(struct ID *id, struct CustomDataLayer *layer);
int *BKE_id_attributes_active_index_p(struct ID *id);

CustomData *BKE_id_attributes_iterator_next_domain(struct ID *id, struct CustomDataLayer *layers);

typedef struct DomainInfo {
  CustomData *customdata;
  int length;
} DomainInfo;

void BKE_id_attribute_get_domains(const struct ID *id, DomainInfo info[ATTR_DOMAIN_NUM]);

#ifdef __cplusplus
}
#endif
