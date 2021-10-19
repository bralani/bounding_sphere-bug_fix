/*                      M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup librt */
/** @{ */
/** @file librt/material.c
 *
 * Various functions associated with material object database I/O
 *
 * Todo: 
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bn.h"
#include "rt/db4.h"
#include "pc.h"
#include "raytrace.h"


/**
 * Free the storage associated with the rt_db_internal version of
 * material object.
 */
void
rt_material_ifree(struct rt_db_internal *ip)
{
    register struct rt_material_internal *material;

    RT_CK_DB_INTERNAL(ip);
    material = (struct rt_material_internal *)ip->idb_ptr;

    if (material) {
	material->magic = 0;			/* sanity */
	bu_vls_free(&material->name);
	bu_free((void *)material, "material ifree");
    }
    ip->idb_ptr = ((void *)0);	/* sanity */
}


/**
 * Import a material from the database format to the internal format.
 */
int
rt_material_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *UNUSED(mat), const struct db_i *UNUSED(dbip))
{
    struct rt_material_internal *material_ip;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_MATERIAL;
    ip->idb_meth = &OBJ[ID_MATERIAL];
    BU_ALLOC(ip->idb_ptr, struct rt_material_internal);

    material_ip = (struct rt_material_internal *)ip->idb_ptr;
    BU_VLS_INIT(&material_ip->name);
    material_ip->magic = RT_MATERIAL_MAGIC;

    struct bu_vls name = BU_VLS_INIT_ZERO;
    unsigned char *cp = ep->ext_buf;

    // copy name to internal format
    bu_vls_strcat(&name, (char *)cp);
    bu_vls_vlscat(&material_ip->name, &name);
    cp += strlen((const char *)cp) + 1;

    // copy physical properties
    uint32_t size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    struct bu_external physical_ep = BU_EXTERNAL_INIT_ZERO;
    physical_ep.ext_nbytes = size;
    physical_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->physicalProperties, &physical_ep);
    } else {
        bu_avs_init_empty(&material_ip->physicalProperties);
    }
    cp += size;

    // copy mechanical properties
    size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    struct bu_external mechanical_ep = BU_EXTERNAL_INIT_ZERO;
    mechanical_ep.ext_nbytes = size;
    mechanical_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->mechanicalProperties, &mechanical_ep);
    } else {
        bu_avs_init_empty(&material_ip->mechanicalProperties);
    }
    cp += size;

    // copy optical properties
    size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    struct bu_external optical_ep = BU_EXTERNAL_INIT_ZERO;
    optical_ep.ext_nbytes = size;
    optical_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->opticalProperties, &optical_ep);
    } else {
        bu_avs_init_empty(&material_ip->opticalProperties);
    }
    cp += size;

    // copy thermal properties
    size = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    struct bu_external thermal_ep = BU_EXTERNAL_INIT_ZERO;
    thermal_ep.ext_nbytes = size;
    thermal_ep.ext_buf = cp;
    if (size > 0) {
        db5_import_attributes(&material_ip->thermalProperties, &thermal_ep);
    } else {
        bu_avs_init_empty(&material_ip->thermalProperties);
    }
    cp += size;

    return 0;			/* OK */
}


/**
 * Export a material from the internal format to the database format.
 */
int
rt_material_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *UNUSED(dbip))
{
    struct rt_material_internal *material_ip;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_external physical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external mechanical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external optical_ep = BU_EXTERNAL_INIT_ZERO;
    struct bu_external thermal_ep = BU_EXTERNAL_INIT_ZERO;

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_MATERIAL) bu_bomb("rt_material_export() type not ID_MATERIAL");
    material_ip = (struct rt_material_internal *)ip->idb_ptr;

    BU_EXTERNAL_INIT(ep);

    db5_export_attributes(&physical_ep, &material_ip->physicalProperties);
    db5_export_attributes(&mechanical_ep, &material_ip->mechanicalProperties);
    db5_export_attributes(&optical_ep, &material_ip->opticalProperties);
    db5_export_attributes(&thermal_ep, &material_ip->thermalProperties);

    // initialize entire buffer
    ep->ext_nbytes = bu_vls_strlen(&material_ip->name) + 1 + (4 * SIZEOF_NETWORK_LONG) + physical_ep.ext_nbytes + mechanical_ep.ext_nbytes + optical_ep.ext_nbytes + thermal_ep.ext_nbytes;
    // ep->ext_nbytes = bu_vls_strlen(&material_ip->name) + 1 + SIZEOF_NETWORK_LONG + physical_ep.ext_nbytes;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "material external");
    unsigned char *cp = ep->ext_buf;

    // copy over the name to buffer
    bu_vls_vlscat(&name, &material_ip->name);
    bu_strlcpy((char *)cp, bu_vls_cstr(&name), bu_vls_strlen(&name) + 1);
    cp += bu_vls_strlen(&name) + 1;
    bu_vls_free(&name);

    // copy physical properties
    *(uint32_t *)cp = htonl(physical_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, physical_ep.ext_buf, physical_ep.ext_nbytes);
	cp += physical_ep.ext_nbytes;

    // copy mechanical properties
    *(uint32_t *)cp = htonl(mechanical_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, mechanical_ep.ext_buf, mechanical_ep.ext_nbytes);
	cp += mechanical_ep.ext_nbytes;

    // copy optical properties
    *(uint32_t *)cp = htonl(optical_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, optical_ep.ext_buf, optical_ep.ext_nbytes);
	cp += optical_ep.ext_nbytes;

    // copy thermal properties
    *(uint32_t *)cp = htonl(thermal_ep.ext_nbytes);
    cp += SIZEOF_NETWORK_LONG;
    memcpy(cp, thermal_ep.ext_buf, thermal_ep.ext_nbytes);
	cp += thermal_ep.ext_nbytes;

    return 0;	/* OK */
}

/**
 * Make human-readable formatted presentation of this object.  First
 * line describes type of object.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_material_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    register struct rt_material_internal *material_ip = (struct rt_material_internal *)ip->idb_ptr;

    char buf[256];

    RT_CHECK_MATERIAL(material_ip);
    bu_vls_strcat(str, "material (MATERIAL)\n");

    sprintf(buf, "\tName: %s\n", material_ip->name.vls_str);
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tParent: %s\n", material_ip->parent.vls_str);
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tSource: %s\n", material_ip->source.vls_str);
    bu_vls_strcat(str, buf);

    if (!verbose) return 0;

    const char *physicalProperties = bu_avs_get_all(&material_ip->physicalProperties, NULL);
    sprintf(buf, "\tphysicalProperties: %s\n", physicalProperties);
    bu_vls_strcat(str, buf);

    const char *mechanicalProperties = bu_avs_get_all(&material_ip->mechanicalProperties, NULL);
    sprintf(buf, "\tmechanicalProperties: %s\n", mechanicalProperties);
    bu_vls_strcat(str, buf);

    const char *opticalProperties = bu_avs_get_all(&material_ip->opticalProperties, NULL);
    sprintf(buf, "\topticalProperties: %s\n", opticalProperties);
    bu_vls_strcat(str, buf);

    const char *thermalProperties = bu_avs_get_all(&material_ip->thermalProperties, NULL);
    sprintf(buf, "\tthermalProperties: %s\n", thermalProperties);
    bu_vls_strcat(str, buf);

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
