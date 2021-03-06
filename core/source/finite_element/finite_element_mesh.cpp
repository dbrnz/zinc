/**
 * FILE : finite_element_mesh.cpp
 *
 * Class defining a domain consisting of a set of finite elements.
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "finite_element/finite_element_mesh.hpp"
#include "finite_element/finite_element_nodeset.hpp"
#include "finite_element/finite_element_private.h"
#include "finite_element/finite_element_region_private.h"
#include "general/object.h"
#include "general/debug.h"
#include "general/message.h"
#include "general/mystring.h"

/*
Module types
------------
*/

cmzn_mesh_scale_factor_set::~cmzn_mesh_scale_factor_set()
{
	DEALLOCATE(name);
}

cmzn_mesh_scale_factor_set::cmzn_mesh_scale_factor_set(FE_mesh *fe_meshIn, const char *nameIn) :
	fe_mesh(fe_meshIn),
	name(duplicate_string(nameIn)),
	access_count(1)
{
}

int cmzn_mesh_scale_factor_set::setName(const char *nameIn)
{
	if (nameIn)
	{
		cmzn_mesh_scale_factor_set *existingSet = this->fe_mesh->find_scale_factor_set_by_name(nameIn);
		if (existingSet)
		{
			bool noChange = (existingSet == this);
			cmzn_mesh_scale_factor_set::deaccess(existingSet);
			if (noChange)
			{
				return CMZN_OK;
			}
		}
		else
		{
			// Note: assumes FE_mesh does not store sets in a map
			// Hence can change name in object
			DEALLOCATE(this->name);
			this->name = duplicate_string(nameIn);
			return CMZN_OK;
		}
	}
	return CMZN_ERROR_ARGUMENT;
}

FE_element_template::FE_element_template(FE_mesh *mesh_in, struct FE_element_field_info *element_field_info, FE_element_shape *element_shape_in) :
	cmzn::RefCounted(),
	mesh(mesh_in->access()),
	element_shape(ACCESS(FE_element_shape)(element_shape_in)),
	template_element(create_template_FE_element(element_field_info))
{
}

FE_element_template::FE_element_template(FE_mesh *mesh_in, struct FE_element *element) :
	cmzn::RefCounted(),
	mesh(mesh_in->access()),
	element_shape(ACCESS(FE_element_shape)(get_FE_element_shape(element))),
	template_element(create_FE_element_from_template(DS_LABEL_INDEX_INVALID, element))
{
}

FE_element_template::~FE_element_template()
{
	FE_mesh::deaccess(this->mesh);
	DEACCESS(FE_element)(&(this->template_element));
	if (this->element_shape)
		DEACCESS(FE_element_shape)(&(this->element_shape));
}

DsLabelIndex FE_mesh::ElementShapeFaces::getElementFace(DsLabelIndex elementIndex, int faceNumber)
{
	// could remove following test if good arguments guaranteed
	if ((faceNumber < 0) || (faceNumber >= this->faceCount))
		return CMZN_ERROR_ARGUMENT;
	DsLabelIndex *faces = this->getElementFaces(elementIndex);
	if (!faces)
		return DS_LABEL_INDEX_INVALID;
	return faces[faceNumber];
}

int FE_mesh::ElementShapeFaces::setElementFace(DsLabelIndex elementIndex, int faceNumber, DsLabelIndex faceIndex)
{
	// could remove following test if good arguments guaranteed
	if ((faceNumber < 0) || (faceNumber >= this->faceCount))
		return CMZN_ERROR_ARGUMENT;
	DsLabelIndex *faces = this->getOrCreateElementFaces(elementIndex);
	if (!faces)
		return CMZN_ERROR_MEMORY;
	faces[faceNumber] = faceIndex;
	return CMZN_OK;
}

FE_mesh::FE_mesh(FE_region *fe_regionIn, int dimensionIn) :
	fe_region(fe_regionIn),
	dimension(dimensionIn),
	elementShapeFacesCount(0),
	elementShapeFacesArray(0),
	elementShapeMap(/*blockLengthIn*/1024, /*allocInitValueIn*/0),
	parents(/*blockLengthIn*/128, /*allocInitValueIn*/0),
	element_field_info_list(CREATE(LIST(FE_element_field_info))()),
	parentMesh(0),
	faceMesh(0),
	changeLog(0),
	last_fe_element_field_info(0),
	element_type_node_sequence_list(0),
	definingFaces(false),
	activeElementIterators(0),
	access_count(1)
{
}

FE_mesh::~FE_mesh()
{
	// safely detach from parent/face meshes
	if (this->parentMesh)
		this->parentMesh->setFaceMesh(0);
	if (this->faceMesh)
		this->faceMesh->setParentMesh(0);
	cmzn::Deaccess(this->changeLog);
	this->last_fe_element_field_info = 0;

	// remove pointers to this FE_mesh as destroying
	cmzn_elementiterator *elementIterator = this->activeElementIterators;
	while (elementIterator)
	{
		elementIterator->invalidate();
		elementIterator = elementIterator->nextIterator;
	}

	this->clear();

	// remove pointers to this FE_mesh as destroying
	FOR_EACH_OBJECT_IN_LIST(FE_element_field_info)(
		FE_element_field_info_clear_FE_mesh, (void *)NULL,
		this->element_field_info_list);
	DESTROY(LIST(FE_element_field_info))(&(this->element_field_info_list));

	const size_t size = this->scale_factor_sets.size();
	for (size_t i = 0; i < size; ++i)
	{
		cmzn_mesh_scale_factor_set *scale_factor_set = this->scale_factor_sets[i];
		cmzn_mesh_scale_factor_set::deaccess(scale_factor_set);
	}
}

void FE_mesh::detach_from_FE_region()
{
	this->fe_region = 0;
}

/**
 * Call this to mark element with the supplied change.
 * Notifies change to clients of FE_region.
 */
void FE_mesh::elementChange(DsLabelIndex elementIndex, int change)
{
	if (this->fe_region && this->changeLog)
	{
		this->changeLog->setIndexChange(elementIndex, change);
		this->fe_region->update();
	}
}

/**
 * Call this to mark element with the supplied change, logging field changes
 * from the field_info_element in the fe_region.
 * Notifies change to clients of FE_region.
 * When an element is added or removed, the same element is used for <element> and
 * <field_info_element>. For changes to the contents of <element>, <field_info_element>
 * should contain the changed fields, consistent with merging it into <element>.
 */
void FE_mesh::elementChange(DsLabelIndex elementIndex, int change, FE_element *field_info_element)
{
	if (this->fe_region && this->changeLog)
	{
		this->changeLog->setIndexChange(elementIndex, change);
		// for efficiency, the following marks field changes only if field info changes
		FE_element_log_FE_field_changes(field_info_element, fe_region->fe_field_changes, /*recurseParents*/true);
		this->fe_region->update();
	}
}

/**
 * Records change to element affecting the supplied fields.
 */
void FE_mesh::elementFieldListChange(FE_element *element, int change,
	LIST(FE_field) *changed_fe_field_list)
{
	if (this->fe_region && this->changeLog)
	{
		this->changeLog->setIndexChange(get_FE_element_index(element), change);
		FOR_EACH_OBJECT_IN_LIST(FE_field)(FE_field_log_FE_field_change,
			(void *)this->fe_region->fe_field_changes, changed_fe_field_list);
		this->fe_region->update();
	}
}

/**
 * Call this instead of elementChange when only the identifier has changed.
 */
void FE_mesh::elementIdentifierChange(FE_element *element)
{
	if (this->fe_region && this->changeLog)
	{
		this->changeLog->setIndexChange(get_FE_element_index(element), DS_LABEL_CHANGE_TYPE_IDENTIFIER);
		this->fe_region->update();
	}
}

/**
 * Call this instead of elementChange when exactly one field, <fe_field> of
 * <element> has changed.
 */
void FE_mesh::elementFieldChange(FE_element *element, FE_field *fe_field)
{
	if (this->fe_region && this->changeLog)
	{
		this->changeLog->setIndexChange(get_FE_element_index(element), DS_LABEL_CHANGE_TYPE_RELATED);
		CHANGE_LOG_OBJECT_CHANGE(FE_field)(this->fe_region->fe_field_changes,
			fe_field, CHANGE_LOG_RELATED_OBJECT_CHANGED(FE_field));
		this->fe_region->update();
	}
}

void FE_mesh::elementAddedChange(FE_element *element)
{
	if (this->fe_region && this->changeLog)
	{
		this->changeLog->setIndexChange(get_FE_element_index(element), DS_LABEL_CHANGE_TYPE_ADD);
		// for efficiency, the following marks field changes only if field info changes
		FE_element_log_FE_field_changes(element, fe_region->fe_field_changes, /*recurseParents*/true);
		this->fe_region->update();
	}
}

void FE_mesh::elementRemovedChange(FE_element *element)
{
	if (this->fe_region)
	{
		this->changeLog->setIndexChange(get_FE_element_index(element), DS_LABEL_CHANGE_TYPE_REMOVE);
		// for efficiency, the following marks field changes only if field info changes
		FE_element_log_FE_field_changes(element, fe_region->fe_field_changes, /*recurseParents*/true);
		this->fe_region->update();
	}
}

// Only to be called by FE_region_clear, or when all elements removed to reset data structures
void FE_mesh::clear()
{
	FE_region_begin_change(this->fe_region);

	if (0 < this->labels.getSize())
	{
		const DsLabelIndex indexLimit = this->labels.getIndexSize();
		if (this->parentMesh)
		{
			// fast cleanup of dynamically allocated parent arrays
			for (DsLabelIndex index = 0; index < indexLimit; ++index)
			{
				DsLabelIndex **parentsArrayAddress = this->parents.getAddress(index);
				if (parentsArrayAddress && *parentsArrayAddress)
					delete[] *parentsArrayAddress;
			}
		}
		FE_element *element;
		for (DsLabelIndex index = 0; index < indexLimit; ++index)
		{
			if (this->fe_elements.getValue(index, element) && (element))
			{
				// must invalidate elements since client or nodal element:xi fields may still hold them
				// BUT! Don't invalidate elements that have been merged into another region
				if (FE_element_get_FE_mesh(element) == this)
					FE_element_invalidate(element);
				DEACCESS(FE_element)(&element);
			}
		}
	}
	this->fe_elements.clear();

	for (unsigned int i = 0; i < this->elementShapeFacesCount; ++i)
		delete this->elementShapeFacesArray[i];
	delete[] this->elementShapeFacesArray;
	this->elementShapeFacesCount = 0;
	this->elementShapeFacesArray = 0;
	this->elementShapeMap.clear();
	// dynamic parent arrays have been freed above
	this->parents.clear();

	this->labels.clear();

	FE_region_end_change(this->fe_region);
}

void FE_mesh::createChangeLog()
{
	cmzn::Deaccess(this->changeLog);
	this->changeLog = DsLabelsChangeLog::create(&this->labels);
	if (!this->changeLog)
		display_message(ERROR_MESSAGE, "FE_mesh::createChangeLog.  Failed to create changes object");
	this->last_fe_element_field_info = 0;
}

DsLabelsChangeLog *FE_mesh::extractChangeLog()
{
	DsLabelsChangeLog *returnChangeLog = cmzn::Access(this->changeLog);
	this->createChangeLog();
	return returnChangeLog;
}

/**
 * Set the element shape for the element at index.
 * @param index  The index of the element in the mesh
 * @param element_shape  The shape to set; must be of same dimension as mesh.
 * @return  ElementShapeFaces for element with index, or 0 if failed.
 */
FE_mesh::ElementShapeFaces *FE_mesh::setElementShape(DsLabelIndex elementIndex, FE_element_shape *element_shape)
{
	if ((elementIndex < 0) || (get_FE_element_shape_dimension(element_shape) != this->dimension))
		return 0;
	ElementShapeFaces *currentElementShapeFaces = this->getElementShapeFaces(elementIndex);
	if (currentElementShapeFaces)
	{
		if (currentElementShapeFaces->getShape() == element_shape)
			return currentElementShapeFaces;
		// should check usage/efficiency for multiple changes, ensure element_shape is not degenerate.
		if (this->parentMesh)
			this->clearElementParents(elementIndex);
		if (this->faceMesh)
			this->clearElementFaces(elementIndex);
	}
	unsigned int shapeIndex = 0;
	while ((shapeIndex < this->elementShapeFacesCount) &&
			(this->elementShapeFacesArray[shapeIndex]->getShape() != element_shape))
		++shapeIndex;
	if (shapeIndex == this->elementShapeFacesCount)
	{
		if (1 == this->elementShapeFacesCount)
		{
			// must now store per-element shape
			if (!this->elementShapeMap.setValues(0, this->labels.getIndexSize() - 1, 0))
			{
				display_message(ERROR_MESSAGE, "FE_mesh::setElementShape  Failed to make per-element shape map");
				return 0;
			}
		}
		ElementShapeFaces *newElementShapeFaces = new ElementShapeFaces(&this->labels, element_shape);
		if (!newElementShapeFaces)
			return 0;
		ElementShapeFaces **tempElementShapeFaces = new ElementShapeFaces*[this->elementShapeFacesCount + 1];
		if (!tempElementShapeFaces)
		{
			delete newElementShapeFaces;
			return 0;
		}
		for (unsigned int i = 0; i < this->elementShapeFacesCount; ++i)
			tempElementShapeFaces[i] = this->elementShapeFacesArray[i];
		tempElementShapeFaces[shapeIndex] = newElementShapeFaces;
		delete[] this->elementShapeFacesArray;
		this->elementShapeFacesArray = tempElementShapeFaces;
		++this->elementShapeFacesCount;
	}
	if ((this->elementShapeFacesCount > 1) &&
			(!this->elementShapeMap.setValue(elementIndex, shapeIndex)))
		return 0;
	// No change message here, assume done by callers
	//this->elementChange(elementIndex, DS_LABEL_CHANGE_TYPE_DEFINITION);
	return this->elementShapeFacesArray[shapeIndex];
}

bool FE_mesh::setElementShapeFromTemplate(DsLabelIndex elementIndex, FE_element_template &element_template)
{
	// GRC make more efficient by caching shapeIndex
	return (0 != this->setElementShape(elementIndex, element_template.get_element_shape()));
}

/**
 * Returns a struct FE_element_field_info for the supplied <fe_element_field_list>.
 * The mesh maintains an internal list of these structures so they can be
 * shared between elements.
 * If <element_field_list> is omitted, an empty list is assumed.
 */
struct FE_element_field_info *FE_mesh::get_FE_element_field_info(
	struct LIST(FE_element_field) *fe_element_field_list)
{
	struct FE_element_field_info *fe_element_field_info = 0;
	struct FE_element_field_info *existing_fe_element_field_info;
	if (fe_element_field_list)
	{
		existing_fe_element_field_info =
			FIRST_OBJECT_IN_LIST_THAT(FE_element_field_info)(
				FE_element_field_info_has_matching_FE_element_field_list,
				(void *)fe_element_field_list,
				this->element_field_info_list);
	}
	else
	{
		existing_fe_element_field_info =
			FIRST_OBJECT_IN_LIST_THAT(FE_element_field_info)(
				FE_element_field_info_has_empty_FE_element_field_list, (void *)NULL,
				this->element_field_info_list);
	}
	if (existing_fe_element_field_info)
	{
		fe_element_field_info = existing_fe_element_field_info;
	}
	else
	{
		fe_element_field_info =
			CREATE(FE_element_field_info)(this, fe_element_field_list);
		if (fe_element_field_info)
		{
			if (!ADD_OBJECT_TO_LIST(FE_element_field_info)(fe_element_field_info,
				this->element_field_info_list))
			{
				display_message(ERROR_MESSAGE,
					"FE_mesh::get_FE_element_field_info.  Could not add to FE_region");
				DESTROY(FE_element_field_info)(&fe_element_field_info);
				fe_element_field_info = (struct FE_element_field_info *)NULL;
			}
		}
		else
		{
			display_message(ERROR_MESSAGE, "FE_mesh::get_FE_element_field_info.  "
				"Could not create element field information");
		}
	}
	return (fe_element_field_info);
}

/**
 * Returns a clone of <fe_element_field_info> that belongs to this mesh and
 * uses equivalent FE_fields, FE_time_sequences and scale factor sets from it.
 * Used to merge elements from other FE_regions into.
 * It is an error if an equivalent/same name FE_field is not found.
 */
struct FE_element_field_info *FE_mesh::clone_FE_element_field_info(
	struct FE_element_field_info *fe_element_field_info)
{
	FE_element_field_info *clone_fe_element_field_info = 0;
	if (fe_element_field_info)
	{
		struct LIST(FE_element_field) *fe_element_field_list =
			FE_element_field_list_clone_for_FE_region(
				FE_element_field_info_get_element_field_list(fe_element_field_info), this);
		if (fe_element_field_list)
		{
			clone_fe_element_field_info = this->get_FE_element_field_info(fe_element_field_list);
			DESTROY(LIST(FE_element_field))(&fe_element_field_list);
		}
		if (!clone_fe_element_field_info)
		{
			display_message(ERROR_MESSAGE,
				"FE_mesh::clone_FE_element_field_info.  Failed");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"FE_mesh::clone_FE_element_field_info.  Invalid argument(s)");
	}
	return (clone_fe_element_field_info);
}

/**
 * Provided EXCLUSIVELY for the use of DEACCESS and REACCESS functions.
 * Called when the access_count of <fe_element_field_info> drops to 1 so that
 * the mesh can destroy FE_element_field_info not in use.
 */
int FE_mesh::remove_FE_element_field_info(
	struct FE_element_field_info *fe_element_field_info)
{
	if (fe_element_field_info == this->last_fe_element_field_info)
		this->last_fe_element_field_info = 0;
	return REMOVE_OBJECT_FROM_LIST(FE_element_field_info)(
		fe_element_field_info, this->element_field_info_list);
}

struct FE_element_field_info_check_field_node_value_labels_data
{
	struct FE_field *field;
	struct FE_region *target_fe_region;
};

/** @param data_void  Pointer to FE_element_field_info_check_field_node_value_labels_data */
int FE_element_field_info_check_field_node_value_labels_iterator(
	struct FE_element_field_info *element_field_info, void *data_void)
{
	FE_element_field_info_check_field_node_value_labels_data *data =
		static_cast<FE_element_field_info_check_field_node_value_labels_data *>(data_void);
	return FE_element_field_info_check_field_node_value_labels(
		element_field_info, data->field, data->target_fe_region);
}

/**
 * Checks element fields to ensure parameters are mapped by value/derivative
 * type and version, adding if necessary. Fails it not possible to add.
 * @return  Status code.
 */
int FE_mesh::check_field_element_node_value_labels(FE_field *field,
	FE_region *target_fe_region)
{
	if (!field)
		return CMZN_ERROR_ARGUMENT;
	FE_element_field_info_check_field_node_value_labels_data data = { field, target_fe_region };
	if (0 == FOR_EACH_OBJECT_IN_LIST(FE_element_field_info)(
		FE_element_field_info_check_field_node_value_labels_iterator, (void *)&data, this->element_field_info_list))
	{
		char *name;
		GET_NAME(FE_field)(field, &name);
		display_message(ERROR_MESSAGE, "FE_mesh::check_field_element_node_value_labels.  "
			"Field %s element maps cannot be converted to use node value labels", name);
		DEALLOCATE(name);
		return CMZN_ERROR_GENERAL;
	}
	return CMZN_OK;
}

/**
 * Find handle to the mesh scale factor set of the given name, if any.
 * Scale factors are stored in elements under a scale factor set.
 *
 * @param name  The name of the scale factor set. 
 * @return  Handle to the scale factor set, or 0 if none.
 * Up to caller to destroy returned handle.
 */
cmzn_mesh_scale_factor_set *FE_mesh::find_scale_factor_set_by_name(
	const char *name)
{
	if (name)
	{
		const size_t size = this->scale_factor_sets.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (0 == strcmp(this->scale_factor_sets[i]->getName(), name))
			{
				return this->scale_factor_sets[i]->access();
			}
		}
	}
	return 0;
}

/**
 * Create a mesh scale factor set. The new set is given a unique name in the
 * mesh, which can be changed.
 * Scale factors are stored in elements under a scale factor set.
 *
 * @return  Handle to the new scale factor set, or 0 on failure. Up to caller
 * to destroy the returned handle.
 */
cmzn_mesh_scale_factor_set *FE_mesh::create_scale_factor_set()
{
	char tempName[10];
	for (int i = static_cast<int>(this->scale_factor_sets.size()) + 1; ; ++i)
	{
		sprintf(tempName, "temp%d", i);
		cmzn_mesh_scale_factor_set *existingSet = this->find_scale_factor_set_by_name(tempName);
		if (existingSet)
		{
			cmzn_mesh_scale_factor_set::deaccess(existingSet);
		}
		else
		{
			cmzn_mesh_scale_factor_set *scale_factor_set = cmzn_mesh_scale_factor_set::create(this, tempName);
			this->scale_factor_sets.push_back(scale_factor_set);
			return scale_factor_set->access();
		}
	}
	return 0;
}

bool FE_mesh::is_FE_field_in_use(struct FE_field *fe_field)
{
	if (FIRST_OBJECT_IN_LIST_THAT(FE_element_field_info)(
		FE_element_field_info_has_FE_field, (void *)fe_field,
		this->element_field_info_list))
	{
		/* since elements may still exist in the change_log,
		   must now check that no remaining elements use fe_field */
		/* for now, if there are elements then fe_field is in use */
		if (0 < this->getSize())
			return true;
	}
	return false;
}

void FE_mesh::list_btree_statistics()
{
	if (this->labels.getSize() > 0)
	{
		display_message(INFORMATION_MESSAGE, "%d-D elements:\n", this->dimension);
		this->labels.list_storage_details();
	}
}

/** Remove iterator from linked list in this mesh */
void FE_mesh::removeElementIterator(cmzn_elementiterator *iterator)
{
	if (iterator == this->activeElementIterators)
		this->activeElementIterators = iterator->nextIterator;
	else
	{
		cmzn_elementiterator *prevIterator = this->activeElementIterators;
		while (prevIterator && (prevIterator->nextIterator != iterator))
			prevIterator = prevIterator->nextIterator;
		if (prevIterator)
			prevIterator->nextIterator = iterator->nextIterator;
		else
			display_message(ERROR_MESSAGE, "FE_mesh::removeElementIterator.  Iterator not in linked list");
	}
	iterator->nextIterator = 0;
}

/**
 * Create an element iterator object for iterating through the elements of the
 * mesh. The iterator initially points at the position before the first element.
 * @param labelsGroup  Optional group to iterate over.
 * @return  Handle to element_iterator at position before first, or NULL if error.
 */
cmzn_elementiterator *FE_mesh::createElementiterator(DsLabelsGroup *labelsGroup)
{
	DsLabelIterator *labelIterator = labelsGroup ? labelsGroup->createLabelIterator() : this->labels.createLabelIterator();
	if (!labelIterator)
		return 0;
	cmzn_elementiterator *iterator = new cmzn_elementiterator(this, labelIterator);
	if (iterator)
	{
		iterator->nextIterator = this->activeElementIterators;
		this->activeElementIterators = iterator;
	}
	else
		cmzn::Deaccess(labelIterator);
	return iterator;
}

struct FE_element *FE_mesh::get_first_FE_element_that(
	LIST_CONDITIONAL_FUNCTION(FE_element) *conditional_function, void *user_data_void)
{
	DsLabelIterator *iter = this->labels.createLabelIterator();
	if (!iter)
		return 0;
	DsLabelIndex elementIndex;
	FE_element *element = 0;
	while ((elementIndex = iter->nextIndex()) != DS_LABEL_INDEX_INVALID)
	{
		element = this->getElement(elementIndex);
		if (!element)
		{
			display_message(ERROR_MESSAGE, "FE_mesh::for_each_FE_element.  No element at index");
			break;
		}
		if (conditional_function(element, user_data_void))
			break;
	}
	cmzn::Deaccess(iter);
	if (elementIndex >= 0)
		return element;
	return 0;
}

int FE_mesh::for_each_FE_element(
	LIST_ITERATOR_FUNCTION(FE_element) iterator_function, void *user_data_void)
{
	DsLabelIterator *iter = this->labels.createLabelIterator();
	if (!iter)
		return 0;
	int return_code = 1;
	DsLabelIndex elementIndex;
	FE_element *element;
	while ((elementIndex = iter->nextIndex()) != DS_LABEL_INDEX_INVALID)
	{
		element = this->getElement(elementIndex);
		if (!element)
		{
			display_message(ERROR_MESSAGE, "FE_mesh::for_each_FE_element.  No element at index");
			return_code = 0;
			break;
		}
		if (!iterator_function(element, user_data_void))
		{
			return_code = 0;
			break;
		}
	}
	cmzn::Deaccess(iter);
	return return_code;
}

DsLabelsGroup *FE_mesh::createLabelsGroup()
{
	return DsLabelsGroup::create(&this->labels); // GRC dodgy taking address here
}

int FE_mesh::change_FE_element_identifier(struct FE_element *element, int new_identifier)
{
	if ((FE_element_get_FE_mesh(element) == this) && (new_identifier >= 0))
	{
		const DsLabelIndex elementIndex = get_FE_element_index(element);
		const DsLabelIdentifier currentIdentifier = this->getElementIdentifier(elementIndex);
		if (currentIdentifier >= 0)
		{
			int return_code = this->labels.setIdentifier(elementIndex, new_identifier);
			if (return_code == CMZN_OK)
				this->elementIdentifierChange(element);
			else if (return_code == CMZN_ERROR_ALREADY_EXISTS)
				display_message(ERROR_MESSAGE, "FE_mesh::change_FE_element_identifier.  Identifier %d is already used in %d-D mesh",
					new_identifier, this->dimension);
			else
				display_message(ERROR_MESSAGE, "FE_mesh::change_FE_element_identifier.  Failed to set label identifier");
			return return_code;
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"FE_mesh::change_FE_element_identifier.  Element is not in this mesh");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"FE_mesh::change_FE_element_identifier.  Invalid argument(s)");
	}
	return CMZN_ERROR_ARGUMENT;
}

/** Creates a template that is a copy of the existing element */
FE_element_template *FE_mesh::create_FE_element_template(FE_element *element)
{
	if (FE_element_get_FE_mesh(element) != this)
		return 0;
	FE_element_template *element_template = new FE_element_template(this, element);
	return element_template;
}

/** @param element_shape  Element shape, must match mesh dimension */
FE_element_template *FE_mesh::create_FE_element_template(FE_element_shape *element_shape)
{
	if (get_FE_element_shape_dimension(element_shape) != this->dimension)
		return 0;
	FE_element_template *element_template = new FE_element_template(this,
		this->get_FE_element_field_info((struct LIST(FE_element_field) *)NULL), element_shape);
	return element_template;
}

/**
 * Convenience function returning an existing element with the identifier
 * from the mesh, or if none found or if identifier is -1, a new element with
 * with the identifier (or the first available identifier if -1), and with the
 * supplied shape or if none, unspecified shape of the same dimension as the
 * mesh.
 * It is expected that the calling function has wrapped calls to this function
 * with FE_region_begin/end_change.
 * @return  Accessed element, or 0 on error.
 */
struct FE_element *FE_mesh::get_or_create_FE_element_with_identifier(int identifier,
	struct FE_element_shape *element_shape)
{
	struct FE_element *element = 0;
	if ((-1 <= identifier) && ((!element_shape) ||
		(get_FE_element_shape_dimension(element_shape) == this->dimension)))
	{
		if (identifier >= 0)
			element = this->findElementByIdentifier(identifier);
		if (element)
		{
			ACCESS(FE_element)(element);
		}
		else
		{
			FE_element_template *element_template = this->create_FE_element_template(
				element_shape ? element_shape : CREATE(FE_element_shape)(dimension, /*type*/(int *)0, fe_region));
			element = this->create_FE_element(identifier, element_template);
			cmzn::Deaccess(element_template);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"FE_mesh::get_or_create_FE_element_with_identifier.  "
			"Invalid argument(s)");
	}
	return (element);
}

/**
 * Checks the element_template is compatible with mesh & that there is no
 * existing element of supplied identifier, then creates element of that
 * identifier as a copy of element_template and adds it to the mesh.
 *
 * @param identifier  Non-negative integer identifier of new element, or -1 to
 * automatically generate (starting at 1). Fails if supplied identifier already
 * used by an existing element.
 * @return  Accessed element, or 0 on error.
 */
FE_element *FE_mesh::create_FE_element(int identifier, FE_element_template *element_template)
{
	struct FE_element *new_element = 0;
	if ((-1 <= identifier) && element_template)
	{
		if (element_template->mesh == this)
		{
			DsLabelIndex elementIndex = (identifier < 0) ? this->labels.createLabel() : this->labels.createLabel(identifier);
			if (elementIndex >= 0)
			{
				new_element = ::create_FE_element_from_template(elementIndex, element_template->get_template_element());
				if (this->setElementShapeFromTemplate(elementIndex, *element_template) &&
					this->fe_elements.setValue(elementIndex, new_element))
				{
					ACCESS(FE_element)(new_element);
					this->elementAddedChange(new_element);
				}
				else
				{
					display_message(ERROR_MESSAGE, "FE_mesh::create_FE_element.  Failed to add element to list.");
					DEACCESS(FE_element)(&new_element);
					this->labels.removeLabel(elementIndex);
				}
			}
			else
			{
				if (this->labels.findLabelByIdentifier(identifier) >= 0)
					display_message(ERROR_MESSAGE, "FE_mesh::create_FE_element.  Identifier %d is already used in %d-D mesh.",
						identifier, this->dimension);
				else
					display_message(ERROR_MESSAGE, "FE_mesh::create_FE_element.  Could not create label");
			}
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"FE_mesh::create_FE_element.  Element template is incompatible with mesh");
		}
	}
	return (new_element);
}

/**
 * Merge fields and other data from source element into destination.
 * Both elements must be of this mesh.
 * @return  Status code.
 */
int FE_mesh::merge_FE_element_existing(struct FE_element *destination, struct FE_element *source)
{
	if (destination && source)
	{
		if (destination == source)
			return CMZN_OK; // nothing to do; happens when adding faces
		if ((FE_element_get_FE_mesh(destination) == this) &&
			(FE_element_get_FE_mesh(source) == this))
		{
			int return_code = CMZN_OK;
			struct LIST(FE_field) *changed_fe_field_list = CREATE(LIST(FE_field))();
			if (changed_fe_field_list)
			{
				if (::merge_FE_element(destination, source, changed_fe_field_list))
				{
					this->elementFieldListChange(destination,
						DS_LABEL_CHANGE_TYPE_DEFINITION | DS_LABEL_CHANGE_TYPE_RELATED, changed_fe_field_list);
				}
				else
				{
					display_message(ERROR_MESSAGE,
						"FE_mesh::merge_FE_element_existing.  Could not merge into %d-D element %d",
						this->dimension, cmzn_element_get_identifier(destination));
					return_code = CMZN_ERROR_GENERAL;
				}
				DESTROY(LIST(FE_field))(&changed_fe_field_list);
			}
			else
			{
				display_message(ERROR_MESSAGE,
					"FE_mesh::merge_FE_element_existing.  Could not create field list");
				return_code = CMZN_ERROR_GENERAL;
			}
			return return_code;
		}
		else
		{
			display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_existing.  "
				"Source and/or destination elements are not from mesh");
		}
	}
	return CMZN_ERROR_ARGUMENT;
}

int FE_mesh::merge_FE_element_template(struct FE_element *destination, FE_element_template *fe_element_template)
{
	if (fe_element_template)
	{
		const DsLabelIndex elementIndex = get_FE_element_index(destination);
		const bool shapeChange = (!FE_element_shape_is_unspecified(fe_element_template->get_element_shape())) &&
			(this->getElementShape(elementIndex) != fe_element_template->get_element_shape());
		int return_code = CMZN_OK;
		if (shapeChange)
		{
			FE_region_begin_change(this->fe_region);
			// GRC make more efficient by caching shapeIndex for shape:
			if (0 == this->setElementShape(elementIndex, fe_element_template->get_element_shape()))
				return_code = CMZN_ERROR_GENERAL;
		}
		if (CMZN_OK == return_code)
			return_code = this->merge_FE_element_existing(destination, fe_element_template->get_template_element());
		if (shapeChange)
			FE_region_end_change(this->fe_region);
		return return_code;
	}
	return CMZN_ERROR_ARGUMENT;
}

/** Add parent index to end of list of parents for element.
 * Private: assumes both indexes are >= 0 */
int FE_mesh::addElementParent(DsLabelIndex elementIndex, DsLabelIndex parentIndex)
{
	DsLabelIndex *oldParentsArray = 0;
	int parentsCount;
	if (this->parents.getValue(elementIndex, oldParentsArray) && oldParentsArray)
		parentsCount = oldParentsArray[0];
	else
		parentsCount = 0;
	++parentsCount;
	DsLabelIndex *parentsArray = new DsLabelIndex[parentsCount + 1]; // with one extra space for count
	if (!parentsArray)
		return CMZN_ERROR_MEMORY;
	parentsArray[0] = parentsCount;
	if (oldParentsArray)
	{
		for (int i = 1; i < parentsCount; ++i)
			parentsArray[i] = oldParentsArray[i];
		delete[] oldParentsArray;
	}
	parentsArray[parentsCount] = parentIndex;
	if (this->parents.setValue(elementIndex, parentsArray))
		return CMZN_OK;
	return CMZN_ERROR_MEMORY;
}

/** Removes first instance of parent index from list of parents for element.
 * Private: assumes both indexes are >= 0 */
int FE_mesh::removeElementParent(DsLabelIndex elementIndex, DsLabelIndex parentIndex)
{
	DsLabelIndex *parentsArray = 0;
	if (this->parents.getValue(elementIndex, parentsArray) && parentsArray)
	{
		const int parentsCount = parentsArray[0];
		for (int i = 1; i <= parentsCount; ++i)
			if (parentsArray[i] == parentIndex)
			{
				if ((--parentsArray[0]) == 0)
				{
					delete[] parentsArray;
					if (this->parents.setValue(elementIndex, 0))
						return CMZN_OK;
					return CMZN_ERROR_GENERAL;
				}
				for (int j = i; j < parentsCount; ++j)
					parentsArray[j] = parentsArray[j + 1];
				return CMZN_OK;
			}
	}
	return CMZN_ERROR_NOT_FOUND;
}

/** Remove all storage for parents for element. Safe version for continued use of
 * region: removes this element from faces of parents, and notifies of their change.
 * Private: assumes element index is >= 0. Call only if mesh has parent mesh */
void FE_mesh::clearElementParents(DsLabelIndex elementIndex)
{
	// remove element from all parents; mark parent elements as DEFINITION_CHANGED
	int parentsCount;
	const DsLabelIndex *parents;
	while (0 < (parentsCount = this->getElementParents(elementIndex, parents)))
	{
		const int faceNumber = this->parentMesh->getElementFaceNumber(parents[0], elementIndex);
		if (CMZN_OK != this->parentMesh->setElementFace(parents[0], faceNumber, DS_LABEL_INDEX_INVALID))
			return;
		this->parentMesh->elementChange(parents[0], DS_LABEL_CHANGE_TYPE_DEFINITION);
	}
}

/** Clear all faces of element. Remove any face elements without other parents from face mesh.
 * Private: assumes element index is >= 0. Call only if mesh has faceMesh */
void FE_mesh::clearElementFaces(DsLabelIndex elementIndex)
{
	ElementShapeFaces *elementShapeFaces = this->getElementShapeFaces(elementIndex);
	if (!elementShapeFaces)
	{
		display_message(ERROR_MESSAGE, "FE_mesh::clearElementFaces.  Missing ElementShapeFaces");
		return;
	}
	// remove faces used by no other parent elements
	DsLabelIndex *faces = elementShapeFaces->getElementFaces(elementIndex);
	if (!faces)
		return;
	const int faceCount = elementShapeFaces->getFaceCount();
	for (int i = 0; i < faceCount; ++i)
	{
		DsLabelIndex faceIndex = faces[i]; // must put in local variable since cleared by setElementFace
		if (faceIndex >= 0)
		{
			this->setElementFace(elementIndex, i, DS_LABEL_INDEX_INVALID); // could be more efficient; finds faces again
			const DsLabelIndex *parents;
			if (0 == this->faceMesh->getElementParents(faceIndex, parents))
				this->faceMesh->remove_FE_element_private(this->faceMesh->getElement(faceIndex));
		}
	}
}

// set index of face element (from face mesh)
int FE_mesh::setElementFace(DsLabelIndex elementIndex, int faceNumber, DsLabelIndex faceIndex)
{
	if ((elementIndex < 0) || (!this->faceMesh))
		return CMZN_ERROR_ARGUMENT;
	ElementShapeFaces *elementShapeFaces = this->getElementShapeFaces(elementIndex);
	if (!elementShapeFaces)
		return CMZN_ERROR_GENERAL;
	if ((faceNumber < 0) || (faceNumber >= elementShapeFaces->getFaceCount()))
		return CMZN_ERROR_ARGUMENT;
	// could in future handle special case of setting invalid face when no faces currently
	DsLabelIndex *faces = elementShapeFaces->getOrCreateElementFaces(elementIndex);
	if (!faces)
		return CMZN_ERROR_MEMORY;
	const DsLabelIndex oldFaceIndex = faces[faceNumber];
	if (oldFaceIndex != faceIndex)
	{
		faces[faceNumber] = faceIndex;
		if (oldFaceIndex >= 0)
			this->faceMesh->removeElementParent(oldFaceIndex, elementIndex);
		if (faceIndex >= 0)
			return this->faceMesh->addElementParent(faceIndex, elementIndex);
	}
	return CMZN_OK;
}

/** return the face number of faceIndex in elementIndex or -1 if not a face */
int FE_mesh::getElementFaceNumber(DsLabelIndex elementIndex, DsLabelIndex faceIndex)
{
	ElementShapeFaces *elementShapeFaces = this->getElementShapeFaces(elementIndex);
	if (elementShapeFaces)
	{
		const DsLabelIndex *faces = elementShapeFaces->getElementFaces(elementIndex);
		if (faces)
			for (int faceNumber = elementShapeFaces->getFaceCount() - 1; faceNumber >= 0; --faceNumber)
				if (faces[faceNumber] == faceIndex)
					return faceNumber;
	}
	return -1;
}

bool FE_mesh::isElementAncestor(DsLabelIndex elementIndex,
	FE_mesh *descendantMesh, DsLabelIndex descendantIndex)
{
	if ((!descendantMesh) || (descendantIndex < 0))
		return false;
	if (this == descendantMesh)
		return (elementIndex == descendantIndex);
	if (!descendantMesh->parentMesh)
		return false;
	const DsLabelIndex *parents;
	const int parentsCount = descendantMesh->getElementParents(descendantIndex, parents);
	if (0 == parentsCount)
		return false;
	if (descendantMesh->parentMesh == this)
	{
		for (int p = 0; p < parentsCount; ++p)
			if (parents[p] == elementIndex)
				return true;
	}
	else if (descendantMesh->parentMesh->parentMesh == this)
	{
		for (int p = 0; p < parentsCount; ++p)
		{
			const DsLabelIndex *parentsParents;
			const int parentsParentsCount = descendantMesh->parentMesh->getElementParents(parents[p], parentsParents);
			for (int pp = 0; pp < parentsParentsCount; ++pp)
				if (parentsParents[pp] == elementIndex)
					return true;
		}
	}
	return false;
}

bool FE_mesh::isElementExterior(DsLabelIndex elementIndex)
{
	if (!this->parentMesh)
		return false;
	const DsLabelIndex *parents;
	const int parentsCount = this->getElementParents(elementIndex, parents);
	if (0 == parentsCount)
		return false;
	const DsLabelIndex *parentsParents;
	if (1 == parentsCount)
	{
		if ((!this->parentMesh->parentMesh) || (0 == this->parentMesh->getElementParents(parents[0], parentsParents)))
			return true;
	}
	else
	{
		for (int i = 0; i < parentsCount; ++i)
			if (1 == this->parentMesh->getElementParents(parents[i], parentsParents))
				return true;
	}
	return false;
}

DsLabelIndex FE_mesh::getElementParentOnFace(DsLabelIndex elementIndex, cmzn_element_face_type faceType)
{
	if (!this->parentMesh)
		return DS_LABEL_INDEX_INVALID;
	const DsLabelIndex *parents;
	const int parentsCount = this->getElementParents(elementIndex, parents);
	if (0 == parentsCount)
		return DS_LABEL_INDEX_INVALID;
	if ((CMZN_ELEMENT_FACE_TYPE_ANY_FACE == faceType) || (CMZN_ELEMENT_FACE_TYPE_ALL == faceType))
		return parents[0];
	FE_mesh *parentParentMesh = this->parentMesh->parentMesh;
	for (int i = 0; i < parentsCount; ++i)
	{
		const DsLabelIndex *parentsParents;
		int parentsParentsCount;
		if ((parentParentMesh) && (parentsParentsCount = this->parentMesh->getElementParents(parents[i], parentsParents)))
		{
			for (int j = 0; j < parentsParentsCount; ++j)
				if (parentParentMesh->isElementFaceOfType(parentsParents[j], parents[i], faceType))
					return parents[i];
		}
		else
		{
			if (parentMesh->isElementFaceOfType(parents[i], elementIndex, faceType))
				return parents[i];
		}
	}
	return DS_LABEL_INDEX_INVALID;
}

/** return the index of neighbour element on faceNumber, if any. Looks to first parent first
 * Copes with element wrapping around and joining itself; will find the other face.
 * @param newFaceNumber  If neighbour found, this gives the face it is on.
 */
DsLabelIndex FE_mesh::getElementFirstNeighbour(DsLabelIndex elementIndex, int faceNumber, int &newFaceNumber)
{
	ElementShapeFaces *elementShapeFaces;
	const DsLabelIndex *faces;
	DsLabelIndex faceIndex;
	if ((this->faceMesh) && (elementShapeFaces = this->getElementShapeFaces(elementIndex)) &&
		(faces = elementShapeFaces->getElementFaces(elementIndex)) &&
		(0 <= (faceIndex = faces[faceNumber])))
	{
		const DsLabelIndex *parents;
		const int parentsCount = this->faceMesh->getElementParents(faceIndex, parents);
		for (int i = 0; i < parentsCount; ++i)
		{
			if (parents[i] != elementIndex)
			{
				newFaceNumber = this->getElementFaceNumber(parents[i], faceIndex);
				return parents[i];
			}
		}
		if (parentsCount > 1)
		{
			// faceIndex is on more than one face of elementIndex; change to other face number
			for (int i = elementShapeFaces->getFaceCount() - 1; i >= 0; --i)
				if ((i != faceNumber) && (faces[i] == faceIndex))
				{
					newFaceNumber = i;
					return elementIndex;
				}
		}
	}
	return DS_LABEL_INDEX_INVALID;
}

/**
 * Find or create an element in this mesh that can be used on face number of
 * the parent element. The face is added to the parent.
 * The new face element is merged into this mesh, but without adding faces.
 * Must be between calls to begin_define_faces/end_define_faces.
 * Can only match faces correctly for coordinate fields with standard node
 * to element maps and no versions.
 * The element type node sequence list is updated with any new face.
 *
 * @param parentIndex  Index of parent element in parentMesh, to find or create
 * face for.
 * @param faceNumber  Face number on parent, starting at 0.
 * @param faceIndex  On successful return, set to new faceIndex or
 * DS_LABEL_INDEX_INVALID if no face needed (for collapsed element face).
 * @return  Index of new face or DS_LABEL_INDEX_INVALID if failed.
 * @return  CMZN_OK on success, any other error on failure.
 */
int FE_mesh::findOrCreateFace(DsLabelIndex parentIndex, int faceNumber, DsLabelIndex& faceIndex)
{
	faceIndex = DS_LABEL_INDEX_INVALID;
	FE_element *parentElement = this->parentMesh->getElement(parentIndex);
	FE_element_type_node_sequence *element_type_node_sequence =
		CREATE(FE_element_type_node_sequence)(parentElement, faceNumber);
	if (!element_type_node_sequence)
		return CMZN_ERROR_GENERAL;

	int return_code = CMZN_OK;
	ACCESS(FE_element_type_node_sequence)(element_type_node_sequence);
	if (!FE_element_type_node_sequence_is_collapsed(element_type_node_sequence))
	{
		FE_element_type_node_sequence *existing_element_type_node_sequence =
			FE_element_type_node_sequence_list_find_match(
			this->element_type_node_sequence_list, element_type_node_sequence);
		if (existing_element_type_node_sequence)
		{
			FE_element *face = FE_element_type_node_sequence_get_FE_element(existing_element_type_node_sequence);
			faceIndex = get_FE_element_index(face);
			if (faceIndex < 0)
				return_code = CMZN_ERROR_GENERAL;
			else
				return_code = this->parentMesh->setElementFace(parentIndex, faceNumber, faceIndex);
		}
		else
		{
			FE_element_shape *parentShape = this->parentMesh->getElementShape(parentIndex);
			FE_element_shape *faceShape = get_FE_element_shape_of_face(parentShape, faceNumber, this->fe_region);
			if (!faceShape)
				return_code = CMZN_ERROR_GENERAL;
			else
			{
				FE_element *face = this->get_or_create_FE_element_with_identifier(/*identifier*/-1, faceShape);
				if (!face)
					return_code = CMZN_ERROR_GENERAL;
				else
				{
					FE_element_type_node_sequence_set_FE_element(element_type_node_sequence, face);
					faceIndex = get_FE_element_index(face);
					return_code = this->parentMesh->setElementFace(parentIndex, faceNumber, faceIndex);
					if (CMZN_OK == return_code)
					{
						if (!ADD_OBJECT_TO_LIST(FE_element_type_node_sequence)(
								element_type_node_sequence, this->element_type_node_sequence_list))
							return_code = CMZN_ERROR_GENERAL;
					}
					DEACCESS(FE_element)(&face);
				}
			}
		}
	}
	DEACCESS(FE_element_type_node_sequence)(&element_type_node_sequence);
	return return_code;
}

/**
 * Recursively define faces for element, creating and adding them to face
 * mesh if they don't already exist.
 * Always call between FE_region_begin/end_define_faces.
 * Always call between FE_region_begin/end_changes.
 * Function ensures that elements share existing faces and lines in preference to
 * creating new ones if they have matching dimension and nodes.
 * @return  CMZN_OK on success, otherwise any error code.
 */
int FE_mesh::defineElementFaces(DsLabelIndex elementIndex)
{
	if (!(this->faceMesh && this->definingFaces && (elementIndex >= 0)))
		return CMZN_ERROR_ARGUMENT;
	ElementShapeFaces *elementShapeFaces = this->getElementShapeFaces(elementIndex);
	if (!elementShapeFaces)
	{
		display_message(ERROR_MESSAGE, "FE_mesh::defineElementFaces.  Missing ElementShapeFaces");
		return CMZN_ERROR_ARGUMENT;
	}
	const int faceCount = elementShapeFaces->getFaceCount();
	if (0 == faceCount)
		return CMZN_OK;
	DsLabelIndex *faces = elementShapeFaces->getOrCreateElementFaces(elementIndex);
	if (!faces)
		return CMZN_ERROR_GENERAL;
	int return_code = CMZN_OK;
	int newFaceCount = 0;
	for (int faceNumber = 0; faceNumber < faceCount; ++faceNumber)
	{
		DsLabelIndex faceIndex = faces[faceNumber];
		if (faceIndex < 0)
		{
			return_code = this->faceMesh->findOrCreateFace(elementIndex, faceNumber, faceIndex);
			if (CMZN_OK != return_code)
				break;
			if (faceIndex >= 0)
				++newFaceCount;
		}
		if ((this->dimension > 2) && (DS_LABEL_INDEX_INVALID != faceIndex))
		{
			// recursively add faces of faces, whether existing or new
			return_code = this->faceMesh->defineElementFaces(faceIndex);
			if (CMZN_OK != return_code)
				break;
		}
	}
	if (newFaceCount)
		this->elementChange(elementIndex, DS_LABEL_CHANGE_TYPE_DEFINITION, this->getElement(elementIndex));
	if (CMZN_OK != return_code)
		display_message(ERROR_MESSAGE, "FE_mesh::defineElementFaces.  Failed");
	return (return_code);
}

/**
 * Creates a list of FE_element_type_node_sequence, and
 * if mesh dimension < MAXIMUM_ELEMENT_XI_DIMENSIONS fills it with sequences
 * for this element. Fails if any two faces have the same shape and nodes.
 */
int FE_mesh::begin_define_faces()
{
	if (this->element_type_node_sequence_list)
	{
		display_message(ERROR_MESSAGE, "FE_mesh::begin_define_faces.  Already defining faces");
		return CMZN_ERROR_ALREADY_EXISTS;
	}
	this->element_type_node_sequence_list = CREATE(LIST(FE_element_type_node_sequence))();
	if (!this->element_type_node_sequence_list)
	{
		display_message(ERROR_MESSAGE, "FE_mesh::begin_define_faces.  Could not create node sequence list");
		return CMZN_ERROR_MEMORY;
	}
	this->definingFaces = true;
	int return_code = CMZN_OK;
	if (this->dimension < MAXIMUM_ELEMENT_XI_DIMENSIONS)
	{
		cmzn_elementiterator_id iter = this->createElementiterator();
		cmzn_element_id element = 0;
		FE_element_type_node_sequence *element_type_node_sequence;
		while (0 != (element = cmzn_elementiterator_next_non_access(iter)))
		{
			element_type_node_sequence = CREATE(FE_element_type_node_sequence)(element);
			if (!element_type_node_sequence)
			{
				display_message(ERROR_MESSAGE, "FE_mesh::begin_define_faces.  "
					"Could not create FE_element_type_node_sequence for %d-D element %d",
					this->dimension, get_FE_element_identifier(element));
				return_code = CMZN_ERROR_GENERAL;
				break;
			}
			if (!ADD_OBJECT_TO_LIST(FE_element_type_node_sequence)(
				element_type_node_sequence, this->element_type_node_sequence_list))
			{
				display_message(WARNING_MESSAGE, "FE_mesh::begin_define_faces.  "
					"Could not add FE_element_type_node_sequence for %d-D element %d.",
					this->dimension, get_FE_element_identifier(element));
				FE_element_type_node_sequence *existing_element_type_node_sequence =
					FE_element_type_node_sequence_list_find_match(
						this->element_type_node_sequence_list, element_type_node_sequence);
				if (existing_element_type_node_sequence)
				{
					display_message(WARNING_MESSAGE,
						"Reason: Existing %d-D element %d uses same node list, and will be used for face matching.",
						this->dimension, get_FE_element_identifier(
							FE_element_type_node_sequence_get_FE_element(existing_element_type_node_sequence)));
				}
				DESTROY(FE_element_type_node_sequence)(&element_type_node_sequence);
			}
		}
		cmzn_elementiterator_destroy(&iter);
	}
	return return_code;
}

void FE_mesh::end_define_faces()
{
	if (this->element_type_node_sequence_list)
		DESTROY(LIST(FE_element_type_node_sequence))(&this->element_type_node_sequence_list);
	else
		display_message(ERROR_MESSAGE, "FE_mesh::end_define_faces.  Wasn't defining faces");
	this->definingFaces = false;
}

/**
 * Ensures faces of elements in mesh exist in face mesh.
 * Recursively does same for faces in face mesh.
 * Call between begin/end_define_faces and begin/end_change.
 */
int FE_mesh::define_faces()
{
	DsLabelIterator *iter = this->labels.createLabelIterator();
	if (!iter)
		return CMZN_ERROR_GENERAL;
	int return_code = CMZN_OK;
	DsLabelIndex elementIndex;
	while (((elementIndex = iter->nextIndex()) != DS_LABEL_INDEX_INVALID) && (CMZN_OK == return_code))
	{
		return_code = this->defineElementFaces(elementIndex);
	}
	cmzn::Deaccess(iter);
	return return_code;
}

/**
 * Removes <element> and all its faces that are not shared with other elements
 * from fe_region. Should enclose call between FE_region_begin_change and
 * FE_region_end_change to minimise messages.
 * This function is recursive.
 */
int FE_mesh::remove_FE_element_private(struct FE_element *element)
{
	int return_code = 1;
	if (this->containsElement(element))
	{
		const DsLabelIndex elementIndex = get_FE_element_index(element);
		// must notify of change before invalidating element otherwise has no fields
		// assumes within begin/end change
		this->elementRemovedChange(element);
		// clear FE_element entry but deaccess at end of this function
		this->fe_elements.setValue(elementIndex, 0);
		if (this->parentMesh)
			this->clearElementParents(elementIndex);
		if (this->faceMesh)
			this->clearElementFaces(elementIndex);
		FE_element_invalidate(element);
		this->labels.removeLabel(elementIndex);
		DEACCESS(FE_element)(&element);
		if (0 == this->labels.getSize())
			this->clear();
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"FE_mesh::remove_FE_element_private.  Invalid argument(s)");
		return_code = 0;
	}
	return (return_code);
}

/**
 * Removes <element> and all its faces that are not shared with other elements
 * from <fe_region>.
 * FE_region_begin/end_change are called internally to reduce change messages to
 * one per call. User should place calls to the begin/end_change functions
 * around multiple calls to this function.
 * This function is recursive.
 */
int FE_mesh::remove_FE_element(struct FE_element *element)
{
	FE_region_begin_change(this->fe_region);
	int return_code = this->remove_FE_element_private(element);
	FE_region_end_change(this->fe_region);
	return return_code;
}

/**
 * Destroy all the elements in FE_mesh, and all their faces
 * that are not shared with other elements from <fe_region>.
 * Caches changes to ensure only one change message per call.
 */
int FE_mesh::destroyAllElements()
{
	int return_code = CMZN_OK;
	FE_region_begin_change(fe_region);
	// can't use an iterator as invalidated when element removed
	const DsLabelIndex indexLimit = this->labels.getIndexSize();
	FE_element *element;
	const bool contiguous = this->labels.isContiguous();
	for (DsLabelIndex index = 0; index < indexLimit; ++index)
	{
		// must handle holes left in identifier array by deleted elements
		if (contiguous || (DS_LABEL_IDENTIFIER_INVALID != this->getElementIdentifier(index)))
		{
			element = this->getElement(index);
			if (!element)
			{
				display_message(WARNING_MESSAGE, "FE_mesh::destroyAllElements.  No element at index");
				continue;
			}
			if (!this->remove_FE_element_private(element))
			{
				return_code = CMZN_ERROR_GENERAL;
				break;
			}
		}
	}
	FE_region_end_change(fe_region);
	return (return_code);
}

/**
 * Destroy all the elements in labelsGroup, and all their faces
 * that are not shared with other elements from <fe_region>.
 * Caches changes to ensure only one change message per call.
 */
int FE_mesh::destroyElementsInGroup(DsLabelsGroup& labelsGroup)
{
	int return_code = CMZN_OK;
	FE_region_begin_change(this->fe_region);
	// can't use an iterator as invalidated when element removed
	DsLabelIndex index = -1; // DS_LABEL_INDEX_INVALID
	FE_element *element;
	while (labelsGroup.incrementIndex(index))
	{
		element = this->getElement(index);
		if (!element)
		{
			display_message(WARNING_MESSAGE, "FE_mesh::destroyElementsInGroup.  No element at index");
			continue;
		}
		if (!this->remove_FE_element_private(element))
		{
			return_code = CMZN_ERROR_GENERAL;
			break;
		}
	}
	FE_region_end_change(this->fe_region);
	return (return_code);
}

bool FE_mesh::canMerge(FE_mesh &source)
{
	if (source.dimension != this->dimension)
	{
		display_message(ERROR_MESSAGE, "FE_mesh::canMerge.  Source mesh has wrong dimension");
		return false;
	}
	DsLabelIterator *iter = source.labels.createLabelIterator();
	if (!iter)
		return false;
	bool result = true;
	DsLabelIndex sourceIndex;
	while ((sourceIndex = iter->nextIndex()) >= 0)
	{
		const DsLabelIdentifier identifier = source.getElementIdentifier(sourceIndex);
		const DsLabelIndex targetIndex = this->labels.findLabelByIdentifier(identifier);
		const ElementShapeFaces *sourceElementShapeFaces = source.getElementShapeFaces(sourceIndex);
		if (!sourceElementShapeFaces)
		{
			display_message(ERROR_MESSAGE, "FE_mesh::canMerge.  Source %d-D element %d missing ElementShapeFaces",
				this->dimension, identifier);
			result = false;
			break;
		}
		if (FE_element_shape_is_unspecified(sourceElementShapeFaces->getShape()))
		{
			// unspecified shape is used for nodal element:xi values when element is
			// not read in from the same file, but could in future be used for
			// reading field definitions without shape information.
			// Must find a matching global element.
			if (targetIndex < 0)
			{
				display_message(ERROR_MESSAGE, "%d-D element %d is not found in global mesh",
					this->dimension, identifier);
				result = false;
				break;
			}
		}
		else if (targetIndex >= 0)
		{
			const ElementShapeFaces *targetElementShapeFaces = this->getElementShapeFaces(targetIndex);
			if (!targetElementShapeFaces)
			{
				display_message(ERROR_MESSAGE, "FE_mesh::canMerge.  Target %d-D element %d missing ElementShapeFaces",
					this->dimension, identifier);
				result = false;
				break;
			}
			if (sourceElementShapeFaces->getShape() != targetElementShapeFaces->getShape())
			{
				display_message(ERROR_MESSAGE, "FE_mesh::canMerge.  Cannot merge %d-D element %d with different shape",
					this->dimension, identifier);
				result = false;
				break;
			}
			const int faceCount = sourceElementShapeFaces->getFaceCount();
			if (faceCount > 0)
			{
				if (!(source.faceMesh && this->faceMesh))
				{
					display_message(ERROR_MESSAGE, "FE_mesh::canMerge.  %d-D mesh missing face meshes",
						this->dimension);
					result = false;
					break;
				}
				const DsLabelIndex *sourceFaces = sourceElementShapeFaces->getElementFaces(sourceIndex);
				if (sourceFaces)
				{
					const DsLabelIndex *targetFaces = targetElementShapeFaces->getElementFaces(targetIndex);
					if (targetFaces)
					{
						// check faces refer to same element identifier if both specified
						for (int i = 0; i < faceCount; ++i)
						{
							if ((sourceFaces[i] >= 0) && (targetFaces[i] >= 0) &&
								(source.faceMesh->labels.getIdentifier(sourceFaces[i]) != this->faceMesh->labels.getIdentifier(targetFaces[i])))
							{
								result = false;
								break;
							}
						}
						if (!result)
						{
							display_message(ERROR_MESSAGE, "FE_mesh::canMerge.  Source %d-D element %d has different faces",
								this->dimension, identifier);
							break;
						}
					}
				}
			}
		}
	}
	cmzn::Deaccess(iter);
	return result;
}

/**
 * Data for passing to FE_mesh::merge_FE_element_external.
 */
struct FE_mesh::Merge_FE_element_external_data
{
	FE_mesh &source;
	FE_nodeset &fe_nodeset;
	/* use following array and number to build up matching pairs of old element
		 field info what they become in the global_fe_region.
		 Note these are ACCESSed */
	FE_element_field_info **matching_element_field_info;
	int number_of_matching_element_field_info;

	Merge_FE_element_external_data(FE_mesh &sourceIn, FE_nodeset &fe_nodeset_in) :
		source(sourceIn),
		fe_nodeset(fe_nodeset_in),
		matching_element_field_info(0),
		number_of_matching_element_field_info(0)
	{
	}

	~Merge_FE_element_external_data()
	{
		if (this->matching_element_field_info)
		{
			for (int i = 2*this->number_of_matching_element_field_info - 1; 0 <= i; --i)
				DEACCESS(FE_element_field_info)(&(this->matching_element_field_info[i]));
			DEALLOCATE(this->matching_element_field_info);
		}
	}

	FE_element_field_info *get_matching_FE_element_field_info(FE_element_field_info *source_element_field_info)
	{
		// 1. Convert element to use a new FE_element_field_info from this mesh
		// fast path: check if the element_field_info has already been matched
		FE_element_field_info **matching_element_field_info = this->matching_element_field_info;
		for (int i = 0; i < this->number_of_matching_element_field_info; ++i)
		{
			if (*matching_element_field_info == source_element_field_info)
				return *(matching_element_field_info + 1);
			matching_element_field_info += 2;
		}
		return 0;
	}

	/**
	 * Record match between source_element_field_info and target_element_field_info.
	 * @return  True on success.
	 */
	bool add_matching_FE_element_field_info(
		FE_element_field_info *source_element_field_info, FE_element_field_info *target_element_field_info)
	{
		FE_element_field_info **matching_element_field_info;
		if (REALLOCATE(matching_element_field_info,
			this->matching_element_field_info, struct FE_element_field_info *,
			2*(this->number_of_matching_element_field_info + 1)))
		{
			matching_element_field_info[this->number_of_matching_element_field_info*2] =
				ACCESS(FE_element_field_info)(source_element_field_info);
			matching_element_field_info[this->number_of_matching_element_field_info*2 + 1] =
				ACCESS(FE_element_field_info)(target_element_field_info);
			this->matching_element_field_info = matching_element_field_info;
			++(this->number_of_matching_element_field_info);
			return true;
		}
		display_message(ERROR_MESSAGE,
			"FE_mesh::Merge_FE_element_external_data::add_matching_FE_element_field_info.  Failed");
		return false;
	}

};

/**
 * Merge element from another mesh, used when reading models from files into
 * temporary regions.
 * Before merging, substitutes into element an appropriate element field info
 * from this mesh, plus nodes from the corresponding FE_nodeset which have the
 * same identifiers as those currently used. Scale factors and nodes are
 * similarly converted.
 * Since this changes information in the element the caller is required to
 * destroy the source mesh immediately after calling this function on any
 * elements from it. Operations such as findElementByIdentifier will no longer
 * work as the element is given a new index for this mesh.
 */
int FE_mesh::merge_FE_element_external(struct FE_element *element,
	Merge_FE_element_external_data &data)
{
	int return_code = 1;
	struct FE_element_field_info *old_element_field_info;

	FE_element_shape *element_shape = get_FE_element_shape(element);
	if (element_shape && (old_element_field_info = FE_element_get_FE_element_field_info(element)))
	{
		const DsLabelIndex sourceElementIndex = get_FE_element_index(element);
		const DsLabelIdentifier identifier = get_FE_element_identifier(element);
		FE_element *global_element = this->findElementByIdentifier(identifier);
		if (FE_element_shape_is_unspecified(element_shape))
		{
			if (!global_element)
			{
				display_message(ERROR_MESSAGE,
					"FE_mesh::merge_FE_element_external.  No matching embedding element");
				return 0;
			}
			return 1;
		}
		const DsLabelIndex newElementIndex = (global_element) ? get_FE_element_index(global_element) :
			this->labels.createLabel(identifier);
		if (newElementIndex < 0)
		{
			display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Failed to get element label.");
			return 0;
		}
		ElementShapeFaces *elementShapeFaces = (global_element) ? this->getElementShapeFaces(newElementIndex) :
			this->setElementShape(newElementIndex, element_shape);
		if (!elementShapeFaces)
		{
			display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Failed to get ElementShapeFaces");
			return 0;
		}

		return_code = 1;
		// 1. Convert element to use a new FE_element_field_info from this mesh
		FE_element_field_info *element_field_info = data.get_matching_FE_element_field_info(old_element_field_info);
		if (!element_field_info)
		{
			element_field_info = this->clone_FE_element_field_info(old_element_field_info);
			if (element_field_info)
			{
				if (!data.add_matching_FE_element_field_info(old_element_field_info, element_field_info))
				{
					DESTROY(FE_element_field_info)(&element_field_info);
				}
			}
			else
			{
				display_message(ERROR_MESSAGE,
					"FE_mesh::merge_FE_element_external.  Could not clone element_field_info");
			}
		}
		if (element_field_info)
		{
			/* substitute global nodes */
			int number_of_nodes;
			if (get_FE_element_number_of_nodes(element, &number_of_nodes))
			{
				struct FE_node *new_node, *old_node;
				for (int i = 0; i < number_of_nodes; ++i)
				{
					if (get_FE_element_node(element, i, &old_node))
					{
						if (old_node)
						{
							new_node = data.fe_nodeset.findNodeByIdentifier(get_FE_node_identifier(old_node));
							if (!((new_node) && set_FE_element_node(element, i, new_node)))
							{
								return_code = 0;
								break;
							}
						}
					}
					else
					{
						return_code = 0;
						break;
					}
				}
			}
			else
			{
				return_code = 0;
			}
			/* substitute global scale factor set identifiers */
			int number_of_scale_factor_sets = 0;
			if (get_FE_element_number_of_scale_factor_sets(element, &number_of_scale_factor_sets))
			{
				for (int i = 0; (i < number_of_scale_factor_sets) && return_code; ++i)
				{
					cmzn_mesh_scale_factor_set *source_scale_factor_set =
						get_FE_element_scale_factor_set_identifier_at_index(element, i);
					cmzn_mesh_scale_factor_set *global_scale_factor_set = this->find_scale_factor_set_by_name(source_scale_factor_set->getName());
					if (!global_scale_factor_set)
					{
						global_scale_factor_set = this->create_scale_factor_set();
						global_scale_factor_set->setName(source_scale_factor_set->getName());
					}
					set_FE_element_scale_factor_set_identifier_at_index(element, i, global_scale_factor_set);
					cmzn_mesh_scale_factor_set::deaccess(global_scale_factor_set);
				}
			}
			else
			{
				return_code = 0;
			}
			// merge equivalent-identifier faces into global or soon-to-be global target element
			const int faceCount = elementShapeFaces->getFaceCount();
			if (faceCount > 0)
			{
				// only need to merge if source element has faces
				ElementShapeFaces *sourceElementShapeFaces = 0;
				DsLabelIndex *sourceFaces = 0;
				if ((!this->faceMesh) || (!data.source.faceMesh))
				{
					display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Missing face mesh(es)");
					return_code = 0;
				}
				else if ((sourceElementShapeFaces = data.source.getElementShapeFaces(sourceElementIndex)) &&
					(sourceFaces = sourceElementShapeFaces->getElementFaces(sourceElementIndex)))
				{
					for (int i = 0; i < faceCount; ++i)
					{
						if (sourceFaces[i] != DS_LABEL_INDEX_INVALID)
						{
							const DsLabelIdentifier sourceFaceIdentifier = data.source.faceMesh->getElementIdentifier(sourceFaces[i]);
							const DsLabelIndex newFaceIndex = this->faceMesh->labels.findLabelByIdentifier(sourceFaceIdentifier);
							if (newFaceIndex < 0)
							{
								display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Missing global face");
								return_code = 0;
								break;
							}
							if (CMZN_OK != this->setElementFace(newElementIndex, i, newFaceIndex))
							{
								display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Failed to set new face");
								return_code = 0;
							}
						}
					}
				}
			}
			if (return_code)
			{
				if (global_element)
					ACCESS(FE_element_field_info)(old_element_field_info);
				/* substitute the new element field info */
				FE_element_set_FE_element_field_info(element, element_field_info);
				set_FE_element_index(element, newElementIndex);
				if (global_element)
				{
					if (this->merge_FE_element_existing(global_element, element) != CMZN_OK)
						return_code = 0;
					// must restore the previous information for clean-up
					FE_element_set_FE_element_field_info(element, old_element_field_info);
					DEACCESS(FE_element_field_info)(&old_element_field_info);
					set_FE_element_index(element, sourceElementIndex);
				}
				else
				{
					if (this->fe_elements.setValue(newElementIndex, element))
					{
						ACCESS(FE_element)(element);
						this->elementAddedChange(element);
					}
					else
					{
						display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Failed to add element to list.");
						this->labels.removeLabel(newElementIndex);
						return_code = 0;
					}
				}
			}
		}
		else
		{
			return_code = 0;
		}
		if (!return_code)
		{
			display_message(ERROR_MESSAGE, "FE_mesh::merge_FE_element_external.  Failed");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"FE_mesh::merge_FE_element_external.  Invalid argument(s)");
		return_code = 0;
	}
	return (return_code);
}

int FE_mesh::merge(FE_mesh &source)
{
	int return_code = 1;
	if (source.dimension == this->dimension)
	{
		Merge_FE_element_external_data data(source,
			*FE_region_find_FE_nodeset_by_field_domain_type(this->fe_region, CMZN_FIELD_DOMAIN_TYPE_NODES));
		cmzn_elementiterator *iter = source.createElementiterator();
		cmzn_element *element;
		while (0 != (element = cmzn_elementiterator_next_non_access(iter)))
		{
			if (!this->merge_FE_element_external(element, data))
			{
				display_message(ERROR_MESSAGE, "FE_mesh::merge.  Could not merge element");
				return_code = 0;
				break;
			}
		}
		cmzn_elementiterator_destroy(&iter);
	}
	else
	{
		return_code = 0;
	}
	return return_code;
}
