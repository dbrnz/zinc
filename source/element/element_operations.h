/*******************************************************************************
FILE : element_operations.h

LAST MODIFIED : 3 March 2003

DESCRIPTION :
FE_element functions that utilise non finite element data structures and
therefore cannot reside in finite element modules.
==============================================================================*/
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is cmgui.
 *
 * The Initial Developer of the Original Code is
 * Auckland Uniservices Ltd, Auckland, New Zealand.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined (ELEMENT_OPERATIONS_H)
#define ELEMENT_OPERATIONS_H

#include "computed_field/computed_field.h"
#include "finite_element/finite_element.h"
#include "finite_element/finite_element_region.h"
#include "general/multi_range.h"
#include "selection/element_selection.h"
#include "selection/element_point_ranges_selection.h"

/*
Global functions
----------------
*/

struct LIST(FE_element) *
	FE_element_list_from_fe_region_selection_ranges_condition(
		struct FE_region *fe_region, enum CM_element_type cm_element_type,
		struct FE_element_selection *element_selection, int selected_flag,
		struct Multi_range *element_ranges,
		struct Computed_field *conditional_field, FE_value time);
/*******************************************************************************
LAST MODIFIED : 3 March 2003

DESCRIPTION :
Creates and returns an element list that is the intersection of:
- all the elements in <fe_region>;
- all elements in the <element_selection> if <selected_flag> is set;
- all elements in the given <element_ranges>, if any.
- all elements for which the <conditional_field> evaluates as "true"
  in its centre at the specified <time>
Up to the calling function to destroy the returned element list.
==============================================================================*/

int FE_region_change_element_identifiers(struct FE_region *fe_region,
	enum CM_element_type cm_type,	int element_offset,
	struct Computed_field *sort_by_field, FE_value time);
/*******************************************************************************
LAST MODIFIED : 16 January 2003

DESCRIPTION :
Changes the identifiers of all elements of <cm_type> in <fe_region>.
If <sort_by_field> is NULL, adds <element_offset> to the identifiers.
If <sort_by_field> is specified, it is evaluated at the centre of all elements
in the group and the elements are sorted by it - changing fastest with the first
component and keeping the current order where the field has the same values.
Checks for and fails if attempting to give any of the elements in <fe_region> an
identifier already used by an element in the same master FE_region.
Calls to this function should be enclosed in FE_region_begin_change/end_change.
Note function avoids iterating through FE_region element lists as this is not
allowed during identifier changes.
==============================================================================*/

/***************************************************************************//**
 * Create an element list with the supplied region, group field at a specific time
 *
 * @param region  The pointer to a region
 * @param element_ranges  Multi_range of elements.
 * @param group_field  Group field of the region
 * @param time  Time of the group field to be evaluated
 * @param use_data  Flag indicating either node or data is in used.
 * @return  Returns element list if successfully create a element list with the given
 *    arguments, otherwise NULL.
 */
struct LIST(FE_element) *
	FE_element_list_from_region_and_selection_group(
		struct Cmiss_region *region, enum CM_element_type cm_element_type,
		struct Multi_range *element_ranges,
		struct Computed_field *group_field,
		struct Computed_field *conditional_field, FE_value time);

#endif /* !defined (ELEMENT_OPERATIONS_H) */
