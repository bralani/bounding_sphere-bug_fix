/*                         S I M U L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/simulate/simulate.c
 *
 * The simulate command.
 *
 * Routines related to performing physics on passed objects only
 *
 * 
 */

#include "common.h"

#ifdef HAVE_BULLET

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "bu.h"
#include "raytrace.h"
#include "../ged_private.h"
#include "simulate.h"


/* The C++ simulation function */
extern int run_simulation(struct simulation_params *sim_params);

/**
 * How to use simulate.Blissfully simple interface, more options will be added soon
 */
static void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str, "Usage: simulate <steps>\n\n");
    bu_vls_printf(str, "Currently this command adds all regions in the model database to a \n\
    	simulation having only gravity as a force. The objects should fall towards the ground plane XY.\n");
    bu_vls_printf(str, "The positions of the regions are set after <steps> number of simulation steps.\n");
    bu_vls_printf(str, "-f <n> <x> <y> <z>\t- Specifies frequency of update(eg 1/60 Hz)(WIP)\n");
    bu_vls_printf(str, "-t <x> <y> <z>\t\t  - Specifies time for which to run(alternative to -n)(WIP)\n");
    return;
}


/**
 * Duplicates the prim/comb passed in dp as new_name
 *
 */
int kill_copy(struct ged *gedp, struct directory *dp, char* new_name)
{
    char *cmd_args[5];
    int rv;

    /* Check if the duplicate already exists, and kill it if so(it maybe of a different shape) */
    if (db_lookup(gedp->ged_wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
        bu_log("kill_copy: WARNING \"%s\" already exists, deleting it\n", new_name);
        cmd_args[0] = "kill";
        cmd_args[1] = new_name;
        cmd_args[2] = (char *)0;

        if(ged_kill(gedp, 2, (const char **)cmd_args) != GED_OK){
            bu_log("kill_copy: could not delete existing \"%s\"\n", new_name);
            return GED_ERROR;
        }
    }

    /* Copy the passed prim/comb */
    cmd_args[0] = "copy";
    cmd_args[1] = dp->d_namep;
    cmd_args[2] = new_name;
    cmd_args[3] = (char *)0;
    rv = ged_copy(gedp, 3, (const char **)cmd_args);
    if (rv != GED_OK){
        bu_log("kill_copy: could not copy \"%s\" to \"%s\"\n", dp->d_namep,
                new_name);
        return GED_ERROR;
    }

    return GED_OK;
}

/**
 * Deletes and recreates the simulation result group
 *
 */
int create_sim_comb(struct ged *gedp, struct simulation_params *sim_params)
{
    char *cmd_args[5];
    int rv;

    /* Check if the duplicate already exists, and kill it if so */
    if (db_lookup(gedp->ged_wdbp->dbip, sim_params->sim_comb_name, LOOKUP_QUIET) != RT_DIR_NULL) {
        bu_log("create_sim_comb: WARNING \"%s\" already exists, deleting it\n", sim_params->sim_comb_name);
        cmd_args[0] = "kill";
        cmd_args[1] = sim_params->sim_comb_name;
        cmd_args[2] = (char *)0;

        if(ged_kill(gedp, 2, (const char **)cmd_args) != GED_OK){
            bu_log("create_sim_comb: could not delete existing \"%s\"\n", sim_params->sim_comb_name);
            return GED_ERROR;
        }
    }

    /* Setup the empty simulation result group */
  	cmd_args[0] = "comb";
  	cmd_args[1] = sim_params->sim_comb_name;
  	cmd_args[2] = (char *)0;
  	rv = ged_comb(gedp, 2, (const char **)cmd_args);
  	if (rv != GED_OK){
  		bu_log("create_sim_comb: Could not create simulation result group \"%s\"\n",
  					sim_params->sim_comb_name);
  		return GED_ERROR;
  	}

    return GED_OK;
}

/**
 * Prints a struct rigid_body for debugging
 */
void print_rigid_body(struct rigid_body *rb)
{
    bu_log("Rigid Body : \"%s\"\n",    rb->rb_namep);
}

/**
 * Add the list of regions in the model to the rigid bodies list in
 * simulation parameters. This function will duplicate the existing regions
 * prefixing "sim_" to the new region and putting them all under a new
 * comb "sim.c". It will skip over any existing regions with "sim_" in the name
 */
int add_regions(struct ged *gedp, struct simulation_params *sim_params)
{
    struct directory *dp, *ndp;
    char *prefixed_name;
    char *prefix = "sim_";
    size_t  prefix_len, prefixed_name_len;
    point_t rpp_min, rpp_max;
    int i;
    struct rigid_body *prev_node = NULL, *current_node;
    char *cmd_args[5];
    int rv;

    create_sim_comb(gedp, sim_params);
    sim_params->num_bodies = 0;



    /* Walk the directory list duplicating all regions only, skip some regions */
    for (i = 0; i < RT_DBNHASH; i++)
        for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            if ( (dp->d_flags & RT_DIR_HIDDEN) ||  /* check for hidden comb/prim */
                !(dp->d_flags & RT_DIR_REGION)     /* check if region */
            )
                continue;

            if (strstr(dp->d_namep, prefix)){
            	bu_vls_printf(gedp->ged_result_str, "add_regions: Skipping \"%s\" due to \"%s\" in name\n",
                        dp->d_namep, prefix);
                continue;
            }

            /* Duplicate the region */
            prefix_len = strlen(prefix);
            prefixed_name_len = strlen(prefix)+strlen(dp->d_namep)+1;
            prefixed_name = (char *)bu_malloc(prefixed_name_len, "Adding sim_ prefix");
            bu_strlcpy(prefixed_name, prefix, prefix_len + 1);
            bu_strlcat(prefixed_name + prefix_len, dp->d_namep, prefixed_name_len - prefix_len);

            kill_copy(gedp, dp, prefixed_name);
            bu_vls_printf(gedp->ged_result_str, "add_regions: Copied \"%s\" to \"%s\"\n", dp->d_namep, prefixed_name);

            /* Get the directory pointer for the object just added */
            if ((ndp=db_lookup(gedp->ged_wdbp->dbip, prefixed_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
            	bu_vls_printf(gedp->ged_result_str, "add_regions: db_lookup(%s) failed", prefixed_name);
                return GED_ERROR;
            }

            /* Get its BB */
            if(rt_bound_internal(gedp->ged_wdbp->dbip, ndp, rpp_min, rpp_max) == 0)
            	bu_vls_printf(gedp->ged_result_str, "add_regions: Got the BB for \"%s\" as \
                        min {%f %f %f} max {%f %f %f}\n", ndp->d_namep,
                        rpp_min[0], rpp_min[1], rpp_min[2],
                        rpp_max[0], rpp_max[1], rpp_max[2]);
            else{
            	bu_vls_printf(gedp->ged_result_str, "add_regions: Could not get the BB\n");
                return GED_ERROR;
            }

            /* Add to simulation list */
            current_node = (struct rigid_body *)bu_malloc(sizeof(struct rigid_body), "rigid_body: current_node");
            current_node->index = sim_params->num_bodies;
            current_node->rb_namep = bu_strdup(prefixed_name);
            current_node->dp = ndp;
            VMOVE(current_node->bb_min, rpp_min);
            VMOVE(current_node->bb_max, rpp_max);
            current_node->next = NULL;

            /* Get BB length, width, height */
            current_node->bb_dims[0] = current_node->bb_max[0] - current_node->bb_min[0];
            current_node->bb_dims[1] = current_node->bb_max[1] - current_node->bb_min[1];
            current_node->bb_dims[2] = current_node->bb_max[2] - current_node->bb_min[2];

            bu_vls_printf(gedp->ged_result_str, "add_regions: Dimensions of this BB : %f %f %f\n",
                    current_node->bb_dims[0], current_node->bb_dims[1], current_node->bb_dims[2]);

            /* Get BB position in 3D space */
            current_node->bb_center[0] = current_node->bb_min[0] + current_node->bb_dims[0]/2;
            current_node->bb_center[1] = current_node->bb_min[1] + current_node->bb_dims[1]/2;
            current_node->bb_center[2] = current_node->bb_min[2] + current_node->bb_dims[2]/2;

            /* Setup the linked list */
            if(prev_node == NULL){ /* first node */
                prev_node = current_node;
                sim_params->head_node = current_node;
            }
            else{                   /* past 1st node now */
                prev_node->next = current_node;
                prev_node = prev_node->next;
            }

            /* Setup the simulation result group union-ing the new objects */
			cmd_args[0] = "comb";
			cmd_args[1] = sim_params->sim_comb_name;
			cmd_args[2] = "u";
			cmd_args[3] = prefixed_name;
			cmd_args[4] = (char *)0;
			rv = ged_comb(gedp, 4, (const char **)cmd_args);
			if (rv != GED_OK){
				bu_log("add_regions: Could not add \"%s\" to simulation result group \"%s\"\n",
						prefixed_name, sim_params->sim_comb_name);
				return GED_ERROR;
			}

            sim_params->num_bodies++;
        }




    /* Show list of objects to be added to the sim : keep for debugging as of now */
    /*    bu_log("add_regions: The following %d regions will participate in the sim : \n", sim_params->num_bodies);
    for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {
        print_rigid_body(current_node);
    }*/

    return GED_OK;

}

/**
 * This function takes the transforms present in the current node and applies them
 * in 3 steps : translate to origin, apply the rotation, then translate to final
 * position with respect to origin(as obtained from physics)
 */
int apply_transforms(struct ged *gedp, struct simulation_params *sim_params)
{
	struct rt_db_internal intern;
	struct rigid_body *current_node;
	mat_t t;

	for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {
		/* Get the internal representation of the object */
		GED_DB_GET_INTERNAL(gedp, &intern, current_node->dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

		/* Translate to origin without any rotation, to apply rotation */
		t[0]  = 1; t[1]  = 0; t[2]  = 0; t[3]  = - (current_node->bb_center[0]);
		t[4]  = 0; t[5]  = 1; t[6]  = 0; t[7]  = - (current_node->bb_center[1]);
		t[8]  = 0; t[9]  = 0; t[10] = 1; t[11] = - (current_node->bb_center[2]);
		t[12] = current_node->t[12]; t[13] = current_node->t[13]; t[14] = current_node->t[14]; t[15] = current_node->t[15];

		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					translating to origin!\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		/* Apply rotation with no translation*/
		t[0]  = current_node->t[0];  t[1]  = current_node->t[1];  t[2]  = current_node->t[2];  t[3]  = 0;
		t[4]  = current_node->t[4];  t[5]  = current_node->t[5];  t[6]  = current_node->t[6];  t[7]  = 0;
		t[8]  = current_node->t[8];  t[9]  = current_node->t[9];  t[10] = current_node->t[10]; t[11] = 0;
		t[12] = current_node->t[12]; t[13] = current_node->t[13]; t[14] = current_node->t[14]; t[15] = current_node->t[15];

		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					applying rotation\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		/* Translate again without any rotation, to apply final position */
		t[0]  = 1; t[1]  = 0; t[2]  = 0; t[3]  = current_node->t[3];
		t[4]  = 0; t[5]  = 1; t[6]  = 0; t[7]  = current_node->t[7];
		t[8]  = 0; t[9]  = 0; t[10] = 1; t[11] = current_node->t[11];
		t[12] = current_node->t[12]; t[13] = current_node->t[13]; t[14] = current_node->t[14]; t[15] = current_node->t[15];

		if (rt_matrix_transform(&intern, current_node->t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					translating to final position\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		/* Write the modified solid to the db so it can be redrawn at the new position & orientation by Mged */
		if (rt_db_put_internal(current_node->dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: Database write error for '%s', aborting\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}
	}


    return GED_OK;
}


/**
 * The libged physics simulation function :
 * Check flags, adds regions to simulation parameters, runs the simulation
 * applies the transforms, frees memory
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    int rv;
    struct simulation_params sim_params;
    static const char *sim_comb_name = "sim.c";
    static const char *ground_plane_name = "sim_gp.r";
    struct rigid_body *current_node, *next_node;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Must be wanting help */
    if (argc == 1) {
        print_usage(gedp->ged_result_str);
        return GED_HELP;
    }

    if (argc < 2) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s <steps>", argv[0]);
        return GED_ERROR;
    }
    

    /* Make a list containing the bb and existing transforms of all the objects in the model
     * which will participate in the simulation
     */
    sim_params.duration  = atoi(argv[1]);
    sim_params.result_str = gedp->ged_result_str;
    sim_params.sim_comb_name = bu_strdup(sim_comb_name);
    sim_params.ground_plane_name = bu_strdup(ground_plane_name);
    rv = add_regions(gedp, &sim_params);
    if (rv != GED_OK){
        bu_vls_printf(gedp->ged_result_str, "%s: Error while adding objects\n", argv[0]);
        return GED_ERROR;
    }


    /* Run the physics simulation  */
    rv = run_simulation(&sim_params);
    if (rv != GED_OK){
		bu_vls_printf(gedp->ged_result_str, "%s: Error while running the simulation\n", argv[0]);
		return GED_ERROR;
	}

    /* Apply transforms on the participating objects */
    rv = apply_transforms(gedp, &sim_params);
	if (rv != GED_OK){
		bu_vls_printf(gedp->ged_result_str, "%s: Error while applying transforms\n", argv[0]);
		return GED_ERROR;
	}

	/* Free memory in rigid_body list */
	for (current_node = sim_params.head_node; current_node != NULL; ) {
		next_node = current_node->next;
		bu_free(current_node, "simulate : current_node");
		current_node = next_node;
	}

    bu_vls_printf(gedp->ged_result_str, "%s: The simulation result is in group : %s\n", argv[0], sim_comb_name);
                                                     
    /* Draw the result : inserting it in argv[1] will cause it to be automatically drawn in the cmd_wrapper */
    argv[1] = sim_comb_name;
    argv[2] = (char *)0;

    return GED_OK;
}


#else

#include "../ged_private.h"

/**
 * Dummy simulation function in case no physics library found
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%s : This command is disabled due to the absence of a physics library",
            argv[0]);
    return GED_ERROR;
}

#endif






/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
