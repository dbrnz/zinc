/***************************************************************************//**
 * FILE : fieldgroup.h
 *
 * Implements a zinc field which maintains a group or selection of objects
 * from the region including the region itself, other fields representing domain
 * object groups (e.g. node, element), and related groups from child regions.
 * The field evaluates to 1 (true) at domain locations in the group, and 0
 * elsewhere.
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CMZN_FIELDGROUP_H__
#define CMZN_FIELDGROUP_H__

#include "types/elementid.h"
#include "types/fieldid.h"
#include "types/fieldgroupid.h"
#include "types/fieldmoduleid.h"
#include "types/nodeid.h"
#include "types/regionid.h"
#include "types/fieldsubobjectgroupid.h"

#include "zinc/zincsharedobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * Creates a group field which can contain an arbitrary set of subregions or
 * region subobjects, and works as a boolean-valued field returning 1 on domains
 * in the group, 0 otherwise.
 *
 * @param field_module  Region field module which will own new field.
 * @return  Handle to newly created field.
 */
ZINC_API cmzn_field_id cmzn_fieldmodule_create_field_group(cmzn_fieldmodule_id field_module);

/***************************************************************************//**
 * If the field is of group type, then this function returns the group specific
 * representation, otherwise it returns NULL.
 * Caller is responsible for destroying the returned derived field reference.
 *
 * @param field  The generic field to be cast.
 * @return  Group specific representation if the input field is of this type,
 * otherwise NULL.
 */
ZINC_API cmzn_field_group_id cmzn_field_cast_group(cmzn_field_id field);

/***************************************************************************//**
 * Cast group field back to its base field and return the field.
 * IMPORTANT NOTE: Returned field does not have incremented reference count and
 * must not be destroyed. Use cmzn_field_access() to add a reference if
 * maintaining returned handle beyond the lifetime of the group argument.
 * Use this function to call base-class API, e.g.:
 * cmzn_field_set_name(cmzn_field_group_base_cast(group_field), "bob");
 *
 * @param group  Handle to the group field to cast.
 * @return  Non-accessed handle to the base field or NULL if failed.
 */
ZINC_C_INLINE cmzn_field_id cmzn_field_group_base_cast(cmzn_field_group_id group)
{
	return (cmzn_field_id)(group);
}

/***************************************************************************//**
 * Destroys this reference to the group field (and sets it to NULL).
 * Internally this just decrements the reference count.
 *
 * @param group_address  Address of handle to the group field.
 * @return  Status CMZN_OK if successfully destroyed the group handle,
 * 		any other value on failure.
 */
ZINC_API int cmzn_field_group_destroy(cmzn_field_group_id *group_address);

/***************************************************************************//**
 * Query if this group and all its subregion and sub-object groups are empty.
 *
 * @param group  Handle to group field to query.
 * @return  1 if group and all its subgroups are empty, otherwise 0.
 */
ZINC_API int cmzn_field_group_is_empty(cmzn_field_group_id group);

/***************************************************************************//**
 * Query if this group contains no objects from the local region.
 *
 * @param group  Handle to group field to query.
 * @return  1 if group is empty locally, otherwise 0.
 */
ZINC_API int cmzn_field_group_is_empty_local(cmzn_field_group_id group);

/***************************************************************************//**
 * Remove all objects from this group, clear all its subgroups, and remove &
 * destroy them if possible.
 *
 * @param group  Handle to group field to modify.
 * @return  Status CMZN_OK if group and its child groups cleared successfully,
 * 		any other value on failure.
 */
ZINC_API int cmzn_field_group_clear(cmzn_field_group_id group);

/***************************************************************************//**
 * Remove all local objects from group, but leave subregion subgroups intact.
 *
 * @param group  Handle to group field to modify.
 * @return  Status CMZN_OK if group is successfully cleared locally,
 * 		any other value on failure.
 */
ZINC_API int cmzn_field_group_clear_local(cmzn_field_group_id group);

/***************************************************************************//**
 * Remove and destroy all empty subregion and subobject groups of this group.
 * Empty subgroups in use by other clients may remain after call.
 *
 * @param group  Handle to group field to modify.
 * @return  Status CMZN_OK on success, any other value on failure.
 */
ZINC_API int cmzn_field_group_remove_empty_subgroups(cmzn_field_group_id group);

/***************************************************************************//**
 * Add the local/owning region of this group field to the group, i.e. all local
 * objects/domains. Local sub-object groups are cleared and destroyed.
 * This function is not hierarchical: subregions are not added.
 *
 * @param group  Handle to group field to modify.
 * @return  Status CMZN_OK on success, any other value on failure.
 */
ZINC_API int cmzn_field_group_add_local_region(cmzn_field_group_id group);

/***************************************************************************//**
 * Query if group contains its local/owning region, i.e. all local objects/
 * domains.
 * This function is not hierarchical: subregions are not checked.
 *
 * @param group  Handle to group field to query.
 * @return  1 if this region is in the group, otherwise 0.
 */
ZINC_API int cmzn_field_group_contains_local_region(cmzn_field_group_id group);

/***************************************************************************//**
 * Add the specified region to the group i.e. all its objects/domains.
 * The specified region must be in the tree of this group's local/owning region
 * and not already in the group.
 * This function is not hierarchical: subregions are not added.
 *
 * @param group  Handle to group field to modify.
 * @param region  Handle to region to be added.
 * @return  Status CMZN_OK if successfully add region into group, any other
 * 		value on failure.
 */
ZINC_API int cmzn_field_group_add_region(cmzn_field_group_id group, cmzn_region_id region);

/***************************************************************************//**
 * Remove specified region from group if currently in it.
 * The specified region must be in the tree of this group's local/owning region.
 * This function is not hierarchical: subregions are not removed.
 *
 * @param group  Handle to group field to modify.
 * @param region  Handle to region to be removed.
 * @return  Status CMZN_OK if region successfully removed from group, any other
 * 		value on failure.
 */
ZINC_API int cmzn_field_group_remove_region(cmzn_field_group_id group, cmzn_region_id region);

/***************************************************************************//**
 * Query if specified region is in the group i.e. all its objects/domains.
 * The specified region must be in the tree of this group's local/owning region.
 * This function is not hierarchical: subregions are not checked.
 *
 * @param group  Handle to group field to query.
 * @param region  Handle to region to check.
 * @return  1 if group contains region, otherwise 0.
 */
ZINC_API int cmzn_field_group_contains_region(cmzn_field_group_id group, cmzn_region_id region);

/***************************************************************************//**
 * Create a group field for the specified subregion, include it in the specified
 * group and return a handle to the newly created sub-group field.
 * The specified region must be in the tree of this group's local/owning region
 * and not already in the group.
 * Caller is responsible for destroying the returned group field handle.
 *
 * @param group  Handle to group field to modify.
 * @param subregion  Handle to region to create a subgroup for.
 * @return  Handle to new, empty sub-group field on success, NULL on failure.
 */
ZINC_API cmzn_field_group_id cmzn_field_group_create_subregion_group(
	cmzn_field_group_id group, cmzn_region_id subregion);

/***************************************************************************//**
 * Get the group field for subregion in the specified group if it exists.
 * The specified region must be in the tree of this group's local/owning region.
 * Caller is responsible for destroying the returned group field handle.
 *
 * @param group  Handle to group field to query.
 * @param subregion  Handle to region to get the subgroup for.
 * @return  Handle to sub-group field or NULL if none.
 */
ZINC_API cmzn_field_group_id cmzn_field_group_get_subregion_group(cmzn_field_group_id group,
	cmzn_region_id subregion);

/***************************************************************************//**
 * Create and return a handle to a node group field compatible with the supplied
 * nodeset, i.e. able to contain nodes from its master nodeset. The node group
 * field is registered as a sub-object group for this group.
 * Fails if a compatible node group field already exists.
 * Caller is responsible for destroying the returned field handle.
 *
 * @param group  Handle to group field to modify.
 * @param nodeset  Handle to a nodeset the node group is to be compatible with.
 * If not already a master nodeset, the master is obtained from it.
 * @return  Handle to new node group field, or NULL on failure.
 */
ZINC_API cmzn_field_node_group_id cmzn_field_group_create_node_group(
	cmzn_field_group_id group, cmzn_nodeset_id nodeset);

/***************************************************************************//**
 * Find and return handle to the sub-object node group compatible with the
 * specified nodeset, if one exists for the group.
 * Caller is responsible for destroying the returned field handle.
 *
 * @param group  Handle to group field to query.
 * @param nodeset  Handle to a nodeset the node group is to be compatible with.
 * If not already a master nodeset, the master is obtained from it.
 * @return  Handle to node group field, or NULL if none.
 */
ZINC_API cmzn_field_node_group_id cmzn_field_group_get_node_group(
	cmzn_field_group_id group, cmzn_nodeset_id nodeset);

/***************************************************************************//**
 * Create and return a handle to an element group field compatible with the
 * supplied mesh, i.e. able to contain elements from its master mesh. The
 * element group field is registered as a sub-object group for this group.
 * Fails if a compatible element group field already exists.
 * Caller is responsible for destroying the returned field handle.
 *
 * @param group  Handle to group field to modify.
 * @param mesh  Handle to a mesh the element group is to be compatible with.
 * If not already a master mesh, the master is obtained from it.
 * @return  Handle to new element group field, or NULL on failure.
 */
ZINC_API cmzn_field_element_group_id cmzn_field_group_create_element_group(
	cmzn_field_group_id group, cmzn_mesh_id mesh);

/***************************************************************************//**
 * Find and return handle to the sub-object element group compatible with the
 * specified mesh, if one exists for the group.
 * Caller is responsible for destroying the returned field handle.
 *
 * @param group  Handle to group field to query.
 * @param mesh  Handle to a mesh the element group is to be compatible with.
 * If not already a master mesh, the master is obtained from it.
 * @return  Handle to element group field, or NULL if none.
 */
ZINC_API cmzn_field_element_group_id cmzn_field_group_get_element_group(
	cmzn_field_group_id group, cmzn_mesh_id mesh);

/***************************************************************************//**
 * Get a subgroup of the given group for the specified domain.
 *
 * @param group the group field
 * @param domain the domain field
 * @returns the subgroup field for the specified domain, NULL otherwise
 */
ZINC_API cmzn_field_id cmzn_field_group_get_subobject_group_for_domain(cmzn_field_group_id group, cmzn_field_id domain);

/***************************************************************************//**
 * Return the first non-empty subregion group in the group tree including itself.
 *
 * @param group  the group field
 * @returns  the first non-empty subregion group field, NULL otherwise.
 */
ZINC_API cmzn_field_group_id cmzn_field_group_get_first_non_empty_group(
	cmzn_field_group_id group);

#ifdef __cplusplus
}
#endif

#endif
