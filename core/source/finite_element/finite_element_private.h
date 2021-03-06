/*******************************************************************************
FILE : finite_element_private.h

LAST MODIFIED : 30 May 2003

DESCRIPTION :
Private interfaces to finite element objects to be included only by privileged
modules such as finite_element_region.c.
==============================================================================*/
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined (FINITE_ELEMENT_PRIVATE_H)
#define FINITE_ELEMENT_PRIVATE_H

#include "finite_element/finite_element.h"
#include "general/indexed_list_private.h"
#include "general/indexed_list_stl_private.hpp"
#include "general/list.h"
#include "general/object.h"

/*
Global types
------------
*/

struct FE_node_field;
/*******************************************************************************
LAST MODIFIED : 27 February 2003

DESCRIPTION :
Forward declaration since passed to FE_region functions for finding and
reusing FE_node_field_info.
==============================================================================*/

DECLARE_LIST_TYPES(FE_node_field);

struct FE_node_field_info;
/*******************************************************************************
LAST MODIFIED : 4 October 2002

DESCRIPTION :
Structure describing how fields and their values and derivatives are stored in
FE_node structures.
==============================================================================*/

DECLARE_LIST_TYPES(FE_node_field_info);

struct FE_element_field;
/*******************************************************************************
LAST MODIFIED : 27 February 2003

DESCRIPTION :
Forward declaration since passed to FE_region functions for finding and
reusing FE_element_field_info.
==============================================================================*/

DECLARE_LIST_TYPES(FE_element_field);

struct FE_element_field_info;
/*******************************************************************************
LAST MODIFIED : 9 October 2002

DESCRIPTION :
The element fields defined on an element and how to calculate them.
==============================================================================*/

DECLARE_LIST_TYPES(FE_element_field_info);

struct FE_element_type_node_sequence_identifier;
/*******************************************************************************
LAST MODIFIED : 13 February 2003

DESCRIPTION :
See struct FE_element_type_node_sequence.
==============================================================================*/

struct FE_element_type_node_sequence;
/*******************************************************************************
LAST MODIFIED : 13 February 2003

DESCRIPTION :
Structure for storing an element with its identifier being its cm_type and the
number and list - in ascending order - of the nodes referred to by the default
coordinate field of the element. Indexed lists, indexed using function
compare_FE_element_type_node_sequence_identifier ensure that recalling a line or
face with the same nodes is extremely rapid. FE_mesh
uses them to find faces and lines for elements without them, if they exist.
==============================================================================*/

DECLARE_LIST_TYPES(FE_element_type_node_sequence);

/*
Private functions
-----------------
*/

PROTOTYPE_INDEXED_LIST_STL_IDENTIFIER_CHANGE_FUNCTIONS(FE_field,name);

struct FE_field_info *CREATE(FE_field_info)(struct FE_region *fe_region);
/*******************************************************************************
LAST MODIFIED : 2 April 2003

DESCRIPTION :
Creates a struct FE_field_info with a pointer to <fe_region>.
Note:
This should only be called by FE_region functions, and the FE_region must be
its own master. The returned object is owned by the FE_region.
It maintains a non-ACCESSed pointer to its owning FE_region which the FE_region
will clear before it is destroyed. If it becomes necessary to have other owners
of these objects, the common parts of it and FE_region should be extracted to a
common object.
==============================================================================*/

int DESTROY(FE_field_info)(
	struct FE_field_info **fe_field_info_address);
/*******************************************************************************
LAST MODIFIED : 2 April 2003

DESCRIPTION :
Destroys the FE_field_info at *<field_info_address>. Frees the
memory for the information and sets <*field_info_address> to NULL.
==============================================================================*/

PROTOTYPE_OBJECT_FUNCTIONS(FE_field_info);

int FE_field_info_clear_FE_region(struct FE_field_info *field_info);
/*******************************************************************************
LAST MODIFIED : 2 April 2003

DESCRIPTION :
Clears the pointer to FE_region in <field_info>.
Private function only to be called by destroy_FE_region.
==============================================================================*/

int FE_field_set_FE_field_info(struct FE_field *fe_field,
	struct FE_field_info *fe_field_info);
/*******************************************************************************
LAST MODIFIED : 2 April 2003

DESCRIPTION :
Changes the FE_field_info at <fe_field> to <fe_field_info>.
Private function only to be called by FE_region when merging FE_regions!
==============================================================================*/

int FE_field_set_indexer_field(struct FE_field *fe_field,
	struct FE_field *indexer_fe_field);
/*******************************************************************************
LAST MODIFIED : 1 May 2003

DESCRIPTION :
If <fe_field> is already indexed, substitutes <indexer_fe_field>.
Does not change any of the values currently stored in <fe_field>
Used to merge indexed fields into different FE_regions; should not be used for
any other purpose.
==============================================================================*/

int FE_field_log_FE_field_change(struct FE_field *fe_field,
	void *fe_field_change_log_void);
/*******************************************************************************
LAST MODIFIED : 30 May 2003

DESCRIPTION :
Logs the field in <fe_field> as RELATED_OBJECT_CHANGED in the
struct CHANGE_LOG(FE_field) pointed to by <fe_field_change_log_void>.
???RC Later may wish to allow more than just RELATED_OBJECT_CHANGED, or have
separate functions for each type.
==============================================================================*/

PROTOTYPE_LIST_FUNCTIONS(FE_node_field);

PROTOTYPE_FIND_BY_IDENTIFIER_IN_LIST_FUNCTION(FE_node_field,field, \
	struct FE_field *);

struct LIST(FE_node_field) *FE_node_field_list_clone_with_FE_field_list(
	struct LIST(FE_node_field) *node_field_list,
	struct LIST(FE_field) *fe_field_list,
	struct FE_time_sequence_package *fe_time_sequence_package);
/*******************************************************************************
LAST MODIFIED : 24 February 2003

DESCRIPTION :
Returns a new FE_node_field list that is identical to <node_field_list>
except that it references equivalent same-name fields from <fe_field_list> and
uses FE_time_sequences in <fe_time_sequence_package>.
It is an error if an equivalent FE_field is not found.
==============================================================================*/

/**
 * Creates a struct FE_node_field_info with a pointer to <fe_nodeset>
 * and a copy of the <fe_node_field_list>.
 * If <fe_node_field_list> is omitted, an empty list is assumed.
 * Returned object has an access count of 1.
 * Note:
 * This should only be called by FE_nodeset functions. The returned object is
 * added to the list of FE_node_field_info in the FE_nodeset and 'owned by it'.
 * It maintains a non-ACCESSed pointer to its owning FE_nodeset which the
 * FE_nodeset will clear before it is destroyed.
 */
struct FE_node_field_info *CREATE(FE_node_field_info)(
	FE_nodeset *fe_nodeset, struct LIST(FE_node_field) *fe_node_field_list,
	int number_of_values);

PROTOTYPE_OBJECT_FUNCTIONS(FE_node_field_info);

PROTOTYPE_LIST_FUNCTIONS(FE_node_field_info);

/**
 * Clears the pointer to FE_nodeset in <node_field_info>.
 * Private function only to be called by ~FE_nodeset.
 */
int FE_node_field_info_clear_FE_nodeset(
	struct FE_node_field_info *node_field_info, void *dummy_void);

int FE_node_field_info_has_FE_field(
	struct FE_node_field_info *node_field_info, void *fe_field_void);
/*******************************************************************************
LAST MODIFIED : 3 March 2003

DESCRIPTION :
Returns true if <node_field_info> has an node field for <fe_field>.
==============================================================================*/

int FE_node_field_info_has_empty_FE_node_field_list(
	struct FE_node_field_info *node_field_info, void *dummy_void);
/*******************************************************************************
LAST MODIFIED : 14 October 2002

DESCRIPTION :
Returns true if <node_field_info> has no node fields.
==============================================================================*/

int FE_node_field_info_has_FE_field_with_multiple_times(
	struct FE_node_field_info *node_field_info, void *fe_field_void);
/*******************************************************************************
LAST MODIFIED: 26 February 2003

DESCRIPTION:
Returns true if <node_field_info> has a node_field that references
<fe_field_void> and that node_field has multiple times.
==============================================================================*/

int FE_node_field_info_has_matching_FE_node_field_list(
	struct FE_node_field_info *node_field_info, void *node_field_list_void);
/*******************************************************************************
LAST MODIFIED : 14 November 2002

DESCRIPTION :
Returns true if <node_field_info> has a FE_node_field_list containing all the
same FE_node_fields as <node_field_list>.
==============================================================================*/

struct LIST(FE_node_field) *FE_node_field_info_get_node_field_list(
	struct FE_node_field_info *fe_node_field_info);
/*******************************************************************************
LAST MODIFIED : 24 February 2003

DESCRIPTION :
Returns the node field list contained in the <node_field_info>.
==============================================================================*/

int FE_node_field_info_add_node_field(
	struct FE_node_field_info *fe_node_field_info, 
	struct FE_node_field *new_node_field, int new_number_of_values);
/*******************************************************************************
LAST MODIFIED : 24 August 2005

DESCRIPTION :
Adds the <new_node_field> to the list in the <fe_node_field_info> and updates the
<new_number_of_values>.  This should only be done if object requesting the change
is known to be the only object using this field info.
==============================================================================*/

int FE_node_field_info_used_only_once(
	struct FE_node_field_info *fe_node_field_info);
/*******************************************************************************
LAST MODIFIED : 24 August 2005

DESCRIPTION :
Returns 1 if the <node_field_info> access count indicates that it is being used
by only one external object.
==============================================================================*/

int FE_node_field_info_get_number_of_values(
	struct FE_node_field_info *fe_node_field_info);
/*******************************************************************************
LAST MODIFIED : 24 February 2003

DESCRIPTION :
Returns the number of values expected for the <node_field_info>.
==============================================================================*/

int FE_node_field_info_log_FE_field_changes(
	struct FE_node_field_info *fe_node_field_info,
	struct CHANGE_LOG(FE_field) *fe_field_change_log);
/*******************************************************************************
LAST MODIFIED : 14 February 2003

DESCRIPTION :
Marks each FE_field in <fe_node_field_info> as RELATED_OBJECT_CHANGED
in <fe_field_change_log>.
==============================================================================*/

PROTOTYPE_INDEXED_LIST_STL_IDENTIFIER_CHANGE_FUNCTIONS(FE_node,cm_node_identifier);

int set_FE_node_identifier(struct FE_node *node, int identifier);
/*******************************************************************************
LAST MODIFIED : 16 January 2003

DESCRIPTION :
Changes the identifier of <node> to <identifier>.
Caution! Should only call for nodes that are NOT in indexed lists;
Must wrap in LIST_BEGIN_IDENTIFIER_CHANGE/LIST_END_IDENTIFIER_CHANGE to ensure
node is temporarily removed from all the indexed lists it is in and re-added
afterwards. FE_region should be the only object that needs to call this.
==============================================================================*/

struct FE_node_field_info *FE_node_get_FE_node_field_info(struct FE_node *node);
/*******************************************************************************
LAST MODIFIED : 13 February 2003

DESCRIPTION :
Returns the FE_node_field_info from <node>. Must not be modified!
==============================================================================*/

int FE_node_set_FE_node_field_info(struct FE_node *node,
	struct FE_node_field_info *fe_node_field_info);
/*******************************************************************************
LAST MODIFIED : 24 February 2003

DESCRIPTION :
Changes the FE_node_field_info at <node> to <fe_node_field_info>.
Note it is very important that the old and the new FE_node_field_info structures
describe the same data layout in the nodal values_storage!
Private function only to be called by FE_region when merging FE_regions!
==============================================================================*/

/**
 * Merges the fields from <source> into <destination>. Existing fields in the
 * <destination> keep the same node field description as before with new field
 * storage following them. Where existing fields in <destination> are passed in
 * <source>, values from <source> take precedence, but the node field structure
 * remains unchanged.
 * Function is atomic; <destination> is unchanged if <source> cannot be merged.
 */
int merge_FE_node(struct FE_node *destination, struct FE_node *source);

PROTOTYPE_LIST_FUNCTIONS(FE_element_field);

PROTOTYPE_FIND_BY_IDENTIFIER_IN_LIST_FUNCTION(FE_element_field,field, \
	struct FE_field *);

/**
 * Returns a new FE_element_field list that is identical to <element_field_list>
 * except that it references equivalent same-name fields and scale factor sets
 * from <fe_mesh>.
 * It is an error if an equivalent FE_field is not found.
 */
struct LIST(FE_element_field) *FE_element_field_list_clone_for_FE_region(
	struct LIST(FE_element_field) *element_field_list, FE_mesh *fe_mesh);

/**
 * Creates a struct FE_element_field_info with a pointer to <fe_region> and a
 * copy of the <fe_element_field_list>.
 * Fails if more than one FE_element_field in the list references the same field.
 * If <fe_element_field_list> is omitted, an empty list is assumed.
 * Note:
 * This should only be called by FE_region functions, and the FE_region must be
 * its own master. The returned object is added to the list of
 * FE_element_field_info in the FE_region and is therefore owned by the FE_region.
 * It maintains a non-ACCESSed pointer to its owning FE_region which the FE_region
 * will clear before it is destroyed. If it becomes necessary to have other owners
 * of these objects, the common parts of it and FE_region should be extracted to a
 * common object.
 */
struct FE_element_field_info *CREATE(FE_element_field_info)(
	FE_mesh *fe_mesh,
	struct LIST(FE_element_field) *fe_element_field_list);

int DESTROY(FE_element_field_info)(
	struct FE_element_field_info **fe_element_field_info_address);
/*******************************************************************************
LAST MODIFIED : 25 February 2003

DESCRIPTION :
Destroys the FE_element_field_info at *<element_field_info_address>. Frees the
memory for the information and sets <*element_field_info_address> to NULL.
==============================================================================*/

PROTOTYPE_OBJECT_FUNCTIONS(FE_element_field_info);

PROTOTYPE_LIST_FUNCTIONS(FE_element_field_info);

/**
 * Clears the pointer to FE_mesh in <element_field_info>.
 * Private function only to be called by ~FE_mesh.
 */
int FE_element_field_info_clear_FE_mesh(
	struct FE_element_field_info *element_field_info, void *dummy_void);

int FE_element_field_info_has_FE_field(
	struct FE_element_field_info *element_field_info, void *fe_field_void);
/*******************************************************************************
LAST MODIFIED : 3 March 2003

DESCRIPTION :
Returns true if <element_field_info> has an element field for <fe_field>.
==============================================================================*/

int FE_element_field_info_has_empty_FE_element_field_list(
	struct FE_element_field_info *element_field_info, void *dummy_void);
/*******************************************************************************
LAST MODIFIED : 20 February 2003

DESCRIPTION :
Returns true if <element_field_info> has no element fields.
==============================================================================*/

int FE_element_field_info_has_matching_FE_element_field_list(
	struct FE_element_field_info *element_field_info,
	void *element_field_list_void);
/*******************************************************************************
LAST MODIFIED : 20 February 2003

DESCRIPTION :
Returns true if <element_field_info> has a FE_element_field_list containing all
the same FE_element_fields as <element_field_list>.
==============================================================================*/

struct LIST(FE_element_field) *FE_element_field_info_get_element_field_list(
	struct FE_element_field_info *fe_element_field_info);
/*******************************************************************************
LAST MODIFIED : 27 February 2003

DESCRIPTION :
Returns the element field list contained in the <element_field_info>.
==============================================================================*/

int FE_element_field_info_log_FE_field_changes(
	struct FE_element_field_info *fe_element_field_info,
	struct CHANGE_LOG(FE_field) *fe_field_change_log);
/*******************************************************************************
LAST MODIFIED : 14 February 2003

DESCRIPTION :
Marks each FE_field in <fe_element_field_info> as RELATED_OBJECT_CHANGED
in <fe_field_change_log>.
==============================================================================*/

/**
 * Creates and returns a non-global element to be used as a template for
 * creating other elements in the supplied mesh.
 * @param element_field_info  A struct FE_element_field_info linking to the
 * mesh. Should have no fields.
 * @return  Accessed element, or 0 on error. Do not merge into FE_mesh!
 */
struct FE_element *create_template_FE_element(FE_element_field_info *element_field_info);

/**
 * Creates and returns a non-global element with the specified index,
 * that is a copy of the supplied template element i.e. with all fields
 * and values from it.
 * The returned element is ready to be added into the FE_mesh the
 * template element was created by.
 * Shape is not set for new element as mapped in mesh.
 * Faces are not copied from the template element.
 * @param index  Index of element in mesh, or DS_LABEL_INDEX_INVALID if used
 * as a non-global template element i.e. when called from
 * FE_element_template::FE_element_template().
 * @param template_element  Element to copy.
 * @return  Accessed element, or 0 on error.
 */
struct FE_element *create_FE_element_from_template(DsLabelIndex index, struct FE_element *template_element);

/**
 * Clear content of element and disconnect it from owning mesh.
 * Use when removing element from mesh or deleting mesh to safely orphan any
 * externally accessed elements.
 */
void FE_element_invalidate(struct FE_element *element);

/**
 * Marks each FE_field defined in <element> as RELATED_OBJECT_CHANGED in
 * <fe_field_change_log>.
 * @param recurseParents  Set to true to recursively mark parent element fields
 * as changed. This is only needed if element is being added or removed since
 * graphics built on mesh of element dimension may be affected due to using
 * inherited fields.
 */
int FE_element_log_FE_field_changes(struct FE_element *element,
	struct CHANGE_LOG(FE_field) *fe_field_change_log, bool recurseParents);

PROTOTYPE_INDEXED_LIST_STL_IDENTIFIER_CHANGE_FUNCTIONS(FE_element,identifier);

struct FE_element_field_info *FE_element_get_FE_element_field_info(
	struct FE_element *element);
/*******************************************************************************
LAST MODIFIED : 13 February 2003

DESCRIPTION :
Returns the FE_element_field_info from <element>. Must not be modified!
==============================================================================*/

int FE_element_set_FE_element_field_info(struct FE_element *element,
	struct FE_element_field_info *fe_element_field_info);
/*******************************************************************************
LAST MODIFIED : 27 February 2003

DESCRIPTION :
Changes the FE_element_field_info at <element> to <fe_element_field_info>.
Note it is very important that the old and the new FE_element_field_info
structures describe the same data layout in the element information!
Private function only to be called by FE_region when merging FE_regions!
==============================================================================*/

/**
 * Check that each Standard_node_to_element_map used to define field has
 * node_value/version labels as well as offsets into component DOFs and if not
 * find them and check they are consistently used for all elements i.e. that
 * each use of the same array offset in an element refers to the same
 * node_value/version.
 * @param element_field_info  The FE_element_field_info to check against.
 * @param field  The FE_field to check/update.
 * @param target_fe_region  Optional FE_region where nodes and equivalent field
 * of same name can be found. Used if field not defined on node in same region.
 * @return  CMZN_OK if labels are complete and consistent, any other status if
 * failed. Failure can occur if nodal value labels are absent at the nodes, or
 * if the implementation discovers the same element field component uses the
 * same offsets for different nodal labels but is unable to fix it.
 */
int FE_element_field_info_check_field_node_value_labels(
	struct FE_element_field_info *element_field_info, FE_field *field,
	struct FE_region *target_fe_region);

/**
 * Create structure storing an element with its identifier being its cm_type
 * and the number and list - in ascending order - of the nodes referred to by
 * the default coordinate field of the element. Indexed lists, indexed using
 * function compare_FE_element_type_node_sequence_identifier ensure that
 * recalling a line or face with the same nodes is extremely rapid.
 * FE_mesh uses them to find faces and lines for elements without them,
 * if they exist.
 * @param face_number  If non-negative, calculate nodes for face number of
 * element, as if the face element were supplied to this function. If so, the
 * returned structure accesses the supplied element. It is assumed this is
 * only used when creating new faces, and the face should be set as soon as
 * created.
 */
struct FE_element_type_node_sequence *CREATE(FE_element_type_node_sequence)(
	struct FE_element *element, int face_number = -1);

int DESTROY(FE_element_type_node_sequence)(
	struct FE_element_type_node_sequence **element_type_node_sequence_address);
/*******************************************************************************
LAST MODIFIED : 13 February 2003

DESCRIPTION :
Cleans up memory used by the FE_element_type_node_sequence.
==============================================================================*/

int FE_element_type_node_sequence_is_collapsed(
	struct FE_element_type_node_sequence *element_type_node_sequence);
/*******************************************************************************
LAST MODIFIED : 13 February 2003

DESCRIPTION :
Returns true if the <element_type_node_sequence> represents an element that
has collapsed, ie, is a face with <= 2 unique nodes, or a line with 1 unique
node.
???RC Note that repeated nodes in the face/line are not put in the node_numbers
array twice, facilitating the simple logic in this function. If they were, then
this function would have to be modified extensively.
==============================================================================*/

/**
 * Gets the FE_element from the <element_type_node_sequence>.
 */
struct FE_element *FE_element_type_node_sequence_get_FE_element(
	struct FE_element_type_node_sequence *element_type_node_sequence);

/**
 * Sets the FE_element in the <element_type_node_sequence>. Use only to
 * substitute face element when sequence was created from face number of
 * parent element.
 */
void FE_element_type_node_sequence_set_FE_element(
	struct FE_element_type_node_sequence *element_type_node_sequence,
	struct FE_element *element);

/**
 * Returns existing FE_element_type_node_sequence from list with matching
 * identifier to supplied one.
 */
FE_element_type_node_sequence *FE_element_type_node_sequence_list_find_match(
	struct LIST(FE_element_type_node_sequence) *element_type_node_sequence_list,
	FE_element_type_node_sequence *element_type_node_sequence);

PROTOTYPE_OBJECT_FUNCTIONS(FE_element_type_node_sequence);

PROTOTYPE_LIST_FUNCTIONS(FE_element_type_node_sequence);

PROTOTYPE_FIND_BY_IDENTIFIER_IN_LIST_FUNCTION(FE_element_type_node_sequence, \
	identifier, FE_element_type_node_sequence_identifier&);

#endif /* !defined (FINITE_ELEMENT_PRIVATE_H) */
