/***************************************************************************//**
 * FILE : cmiss_node_private.cpp
 *
 * Implementation of public interface to cmzn_node.
 *
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>
#include "zinc/fieldmodule.h"
#include "zinc/node.h"
#include "zinc/timesequence.h"
#include "general/debug.h"
#include "general/mystring.h"
#include "finite_element/finite_element.h"
#include "finite_element/finite_element_region.h"
#include "computed_field/computed_field_finite_element.h"
#include "node/node_operations.h"
#include "general/message.h"
#include "computed_field/field_module.hpp"
#include "general/enumerator_conversion.hpp"
#include "mesh/cmiss_node_private.hpp"
#include <vector>
#include "computed_field/computed_field_private.hpp"
#include "computed_field/computed_field_subobject_group_private.hpp"

/*
Global types
------------
*/

/*============================================================================*/

namespace {

class cmzn_node_field
{
	FE_field *fe_field;
	FE_node_field_creator *node_field_creator;
	FE_time_sequence *time_sequence;

public:

	cmzn_node_field(FE_field *fe_field) :
		fe_field(ACCESS(FE_field)(fe_field)),
		node_field_creator(
			CREATE(FE_node_field_creator)(get_FE_field_number_of_components(fe_field))),
		time_sequence(NULL)
	{
	}

	~cmzn_node_field()
	{
		if (time_sequence)
			DEACCESS(FE_time_sequence)(&time_sequence);
		DESTROY(FE_node_field_creator)(&node_field_creator);
		DEACCESS(FE_field)(&fe_field);
	}

	int defineDerivative(int component_number, enum FE_nodal_value_type derivative_type)
	{
		int first = 0;
		int limit = get_FE_field_number_of_components(fe_field);
		if ((component_number < -1) || (component_number == 0) || (component_number > limit))
			return 0;
		if (component_number > 0)
		{
			first = component_number - 1;
			limit = component_number;
		}
		int return_code = 1;
		for (int i = first; i < limit; i++)
		{
			if (!FE_node_field_creator_define_derivative(node_field_creator, i, derivative_type))
				return_code = 0;
		}
		return return_code;
	}

	int defineTimeSequence(FE_time_sequence *in_time_sequence)
	{
		return REACCESS(FE_time_sequence)(&time_sequence, in_time_sequence);
	}

	/** note: does not ACCESS */
	cmzn_time_sequence_id getTimeSequence()
	{
		return reinterpret_cast<cmzn_time_sequence_id>(time_sequence);
	}

	int defineVersions(int component_number, int number_of_versions)
	{
		int first = 0;
		int limit = get_FE_field_number_of_components(fe_field);
		if ((component_number < -1) || (component_number == 0) || (component_number > limit))
			return 0;
		if (component_number > 0)
		{
			first = component_number - 1;
			limit = component_number;
		}
		int return_code = 1;
		for (int i = first; i < limit; i++)
		{
			if (!FE_node_field_creator_define_versions(node_field_creator, i, number_of_versions))
				return_code = 0;
		}
		return return_code;
	}

	int defineAtNode(FE_node *node)
	{
		return define_FE_field_at_node(node, fe_field,
			time_sequence, node_field_creator);
	}

	int getNumberOfVersions(int component_number)
	{
		int number_of_components = get_FE_field_number_of_components(fe_field);
		if ((component_number < -1) || (component_number == 0) || (component_number > number_of_components))
			return 0;
		return FE_node_field_creator_get_number_of_versions(node_field_creator, component_number - 1);
	}

	int hasDerivative(int component_number, enum FE_nodal_value_type derivative_type)
	{
		int number_of_components = get_FE_field_number_of_components(fe_field);
		if ((component_number < -1) || (component_number == 0) || (component_number > number_of_components))
			return 0;
		return FE_node_field_creator_has_derivative(node_field_creator, component_number - 1, derivative_type);
	}

	FE_field *getFeField() const { return fe_field; }
};

}

/*============================================================================*/

struct cmzn_nodetemplate
{
private:
	FE_region *fe_region;
	FE_node *template_node;
	std::vector<cmzn_node_field*> fields;
	std::vector<FE_field*> undefine_fields; // ACCESSed
	int access_count;

public:
	cmzn_nodetemplate(FE_region *fe_region) :
		fe_region(ACCESS(FE_region)(fe_region)),
		template_node(NULL),
		access_count(1)
	{
	}

	cmzn_nodetemplate_id access()
	{
		++access_count;
		return this;
	}

	static int deaccess(cmzn_nodetemplate_id &node_template)
	{
		if (!node_template)
			return 0;
		--(node_template->access_count);
		if (node_template->access_count <= 0)
			delete node_template;
		node_template = 0;
		return 1;
	}

	int defineField(cmzn_field_id field)
	{
		if (!checkValidFieldForDefine(field))
			return 0;
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		if (getNodeField(fe_field))
		{
			return 0;
		}
		if (getUndefineNodeField(fe_field))
		{
			return 0;
		}
		clearTemplateNode();
		cmzn_node_field *node_field = createNodeField(fe_field);
		return (node_field != NULL);
	}

	int defineFieldFromNode(cmzn_field_id field, cmzn_node_id node)
	{
		if (!checkValidFieldForDefine(field))
			return 0;
		if (!FE_region_contains_FE_node(fe_region, node))
			return 0;

		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		if (getNodeField(fe_field))
		{
			return 0;
		}
		if (getUndefineNodeField(fe_field))
		{
			return 0;
		}

		const enum FE_nodal_value_type all_fe_nodal_value_types[] = {
			FE_NODAL_VALUE,
			FE_NODAL_D_DS1,
			FE_NODAL_D_DS2,
			FE_NODAL_D_DS3,
			FE_NODAL_D2_DS1DS2,
			FE_NODAL_D2_DS1DS3,
			FE_NODAL_D2_DS2DS3,
			FE_NODAL_D3_DS1DS2DS3
		};
		const int number_of_fe_value_types = sizeof(all_fe_nodal_value_types) / sizeof(enum FE_nodal_value_type);
		clearTemplateNode();
		cmzn_node_field *node_field = createNodeField(fe_field);
		int number_of_components = cmzn_field_get_number_of_components(field);
		for (int component_number = 1; component_number <= number_of_components; ++component_number)
		{
			for (int i = 1; i < number_of_fe_value_types; ++i)
			{
				enum FE_nodal_value_type fe_nodal_value_type = all_fe_nodal_value_types[i];
				if (FE_nodal_value_version_exists(node, fe_field,
					component_number - 1, /*version*/0, fe_nodal_value_type))
				{
					node_field->defineDerivative(component_number, fe_nodal_value_type);
				}
			}
			// versions should be per-nodal-value-type, but are not currently
			int number_of_versions = get_FE_node_field_component_number_of_versions(node, fe_field, component_number - 1);
			if (number_of_versions > 1)
			{
				node_field->defineVersions(component_number, number_of_versions);
			}
		}
		struct FE_time_sequence *time_sequence = get_FE_node_field_FE_time_sequence(node, fe_field);
		if (time_sequence)
		{
			node_field->defineTimeSequence(time_sequence);
		}
		return (node_field != NULL);
	}

	int defineDerivative(cmzn_field_id field, int component_number,
		enum cmzn_node_value_type derivative_type)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		if (!finite_element_field)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_derivative.  Field must be real finite_element type");
			return 0;
		}
		cmzn_field_finite_element_destroy(&finite_element_field);
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		cmzn_node_field *node_field = getNodeField(fe_field);
		if (!node_field)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_derivative.  Field is not defined yet");
			return 0;
		}
		enum FE_nodal_value_type fe_nodal_value_type =
			cmzn_node_value_type_to_FE_nodal_value_type(derivative_type);
		if (FE_NODAL_UNKNOWN == fe_nodal_value_type)
			return 0;
		clearTemplateNode();
		return node_field->defineDerivative(component_number, fe_nodal_value_type);
	}

	int defineTimeSequence(cmzn_field_id field,
		cmzn_time_sequence_id time_sequence)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		if (!finite_element_field)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_time_sequence.  Field must be real finite_element type");
			return 0;
		}
		cmzn_field_finite_element_destroy(&finite_element_field);
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		cmzn_node_field *node_field = getNodeField(fe_field);
		if (!node_field)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_time_sequence.  Field is not defined yet");
			return 0;
		}
		clearTemplateNode();
		return node_field->defineTimeSequence(reinterpret_cast<struct FE_time_sequence *>(time_sequence));
	}

	int defineVersions(cmzn_field_id field, int component_number,
		int number_of_versions)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		if (!finite_element_field)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_versions.  Field must be real finite_element type");
			return 0;
		}
		cmzn_field_finite_element_destroy(&finite_element_field);
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		cmzn_node_field *node_field = getNodeField(fe_field);
		if (!node_field)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_versions.  Field is not defined yet");
			return 0;
		}
		clearTemplateNode();
		return node_field->defineVersions(component_number, number_of_versions);
	}

	int getNumberOfVersions(cmzn_field_id field, int component_number)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		if (!finite_element_field)
			return 0;
		cmzn_field_finite_element_destroy(&finite_element_field);
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		cmzn_node_field *node_field = getNodeField(fe_field);
		if (!node_field)
			return 0;
		return node_field->getNumberOfVersions(component_number);
	}

	cmzn_time_sequence_id getTimeSequence(cmzn_field_id field)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		if (!finite_element_field)
			return 0;
		cmzn_field_finite_element_destroy(&finite_element_field);
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		cmzn_node_field *node_field = getNodeField(fe_field);
		if (!node_field)
			return 0;
		cmzn_time_sequence_id timeSequence = node_field->getTimeSequence();
		if (timeSequence)
		{
			cmzn_time_sequence_access(timeSequence);
		}
		return timeSequence;
	}

	int hasDerivative(cmzn_field_id field, int component_number,
		enum cmzn_node_value_type derivative_type)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		if (!finite_element_field)
			return 0;
		cmzn_field_finite_element_destroy(&finite_element_field);
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		cmzn_node_field *node_field = getNodeField(fe_field);
		if (!node_field)
			return 0;
		enum FE_nodal_value_type fe_nodal_value_type =
			cmzn_node_value_type_to_FE_nodal_value_type(derivative_type);
		if (FE_NODAL_UNKNOWN == fe_nodal_value_type)
			return 0;
		return node_field->hasDerivative(component_number, fe_nodal_value_type);
	}

	int undefineField(cmzn_field_id field)
	{
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		cmzn_field_stored_mesh_location_id stored_mesh_location_field = cmzn_field_cast_stored_mesh_location(field);
		cmzn_field_stored_string_id stored_string_field = cmzn_field_cast_stored_string(field);
		if (!(finite_element_field || stored_mesh_location_field || stored_string_field))
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_undefine_field.  "
				"Field must be finite_element, stored_mesh_location or stored_string type");
			return 0;
		}
		int return_code = 1;
		FE_field *fe_field = NULL;
		Computed_field_get_type_finite_element(field, &fe_field);
		FE_region *compare_fe_region = fe_region;
		if (FE_region_is_data_FE_region(fe_region))
		{
			 FE_region_get_immediate_master_FE_region(fe_region, &compare_fe_region);
		}
		if (FE_field_get_FE_region(fe_field) != compare_fe_region)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_undefine_field.  Field is from another region");
			return_code = 0;
		}
		if (getNodeField(fe_field))
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_undefine_field.  Field is already being defined");
			return_code = 0;
		}
		if (getUndefineNodeField(fe_field))
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_undefine_field.  Field is already being undefined");
			return_code = 0;
		}
		cmzn_field_finite_element_destroy(&finite_element_field);
		cmzn_field_stored_mesh_location_destroy(&stored_mesh_location_field);
		cmzn_field_stored_string_destroy(&stored_string_field);
		if (!return_code)
			return 0;
		clearTemplateNode();
		setUndefineNodeField(fe_field);
		return 1;
	}

	int validate()
	{
		if (template_node)
			return 1;
		template_node = ACCESS(FE_node)(
			CREATE(FE_node)(0, fe_region, (struct FE_node *)NULL));
		for (unsigned int i = 0; i < fields.size(); i++)
		{
			if (!fields[i]->defineAtNode(template_node))
			{
				DEACCESS(FE_node)(&template_node);
				break;
			}
		}
		if (!template_node)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_validate.  Failed to create template node");
			return 0;
		}
		return 1;
	}

	// can be made more efficient
	int mergeIntoNode(cmzn_node_id node)
	{
		int return_code = 1;
		if (validate())
		{
			if (0 < undefine_fields.size())
			{
				for (unsigned int i = 0; i < undefine_fields.size(); i++)
				{
					if (FE_field_is_defined_at_node(undefine_fields[i], node) &&
						!undefine_FE_field_at_node(node, undefine_fields[i]))
					{
						return_code = 0;
						break;
					}
				}
			}
			if ((0 < fields.size() &&
				!FE_region_merge_FE_node_existing(fe_region, node, template_node)))
			{
				return_code = 0;
			}
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"cmzn_node_merge.  Node template is not valid");
			return_code = 0;
		}
		return return_code;
	}

	FE_node *getTemplateNode() { return template_node; }

private:
	~cmzn_nodetemplate()
	{
		for (unsigned int i = 0; i < fields.size(); i++)
		{
			delete fields[i];
		}
		for (unsigned int i = 0; i < undefine_fields.size(); i++)
		{
			DEACCESS(FE_field)(&(undefine_fields[i]));
		}
		REACCESS(FE_node)(&template_node, NULL);
		DEACCESS(FE_region)(&fe_region);
	}

	cmzn_node_field *getNodeField(FE_field *fe_field)
	{
		for (unsigned int i = 0; i < fields.size(); i++)
		{
			if (fields[i]->getFeField() == fe_field)
			{
				return fields[i];
			}
		}
		return NULL;
	}

	/** Must call getNodeField first to confirm not already being defined */
	cmzn_node_field *createNodeField(FE_field *fe_field)
	{
		cmzn_node_field *node_field = new cmzn_node_field(fe_field);
		fields.push_back(node_field);
		return node_field;
	}

	bool getUndefineNodeField(FE_field *fe_field)
	{
		for (unsigned int i = 0; i < undefine_fields.size(); i++)
		{
			if (undefine_fields[i] == fe_field)
			{
				return true;
			}
		}
		return false;
	}

	/** Must call getUndefineNodeField first to confirm not already being undefined */
	void setUndefineNodeField(FE_field *fe_field)
	{
		ACCESS(FE_field)(fe_field);
		undefine_fields.push_back(fe_field);
	}

	void clearTemplateNode()
	{
		REACCESS(FE_node)(&template_node, NULL);
	}

	bool checkValidFieldForDefine(cmzn_field_id field)
	{
		bool result = true;
		cmzn_field_finite_element_id finite_element_field = cmzn_field_cast_finite_element(field);
		cmzn_field_stored_mesh_location_id stored_mesh_location_field = cmzn_field_cast_stored_mesh_location(field);
		cmzn_field_stored_string_id stored_string_field = cmzn_field_cast_stored_string(field);
		if (finite_element_field || stored_mesh_location_field || stored_string_field)
		{
			FE_field *fe_field = 0;
			Computed_field_get_type_finite_element(field, &fe_field);
			FE_region *compare_fe_region = fe_region;
			if (FE_region_is_data_FE_region(fe_region))
			{
				 FE_region_get_immediate_master_FE_region(fe_region, &compare_fe_region);
			}
			if (FE_field_get_FE_region(fe_field) != compare_fe_region)
			{
				display_message(ERROR_MESSAGE,
					"cmzn_nodetemplate_define_field.  "
					"Field is from another region");
				result = false;
			}
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodetemplate_define_field.  "
				"Field must be finite_element, stored_mesh_location or stored_string type");
			result = false;
		}
		cmzn_field_finite_element_destroy(&finite_element_field);
		cmzn_field_stored_mesh_location_destroy(&stored_mesh_location_field);
		cmzn_field_stored_string_destroy(&stored_string_field);
		return result;
	}

	FE_nodal_value_type cmzn_node_value_type_to_FE_nodal_value_type(
		enum cmzn_node_value_type nodal_value_type)
	{
		FE_nodal_value_type fe_nodal_value_type = FE_NODAL_UNKNOWN;
		switch (nodal_value_type)
		{
			case CMZN_NODE_VALUE_TYPE_INVALID:
				fe_nodal_value_type = FE_NODAL_UNKNOWN;
				break;
			case CMZN_NODE_VALUE:
				fe_nodal_value_type = FE_NODAL_VALUE;
				break;
			case CMZN_NODE_D_DS1:
				fe_nodal_value_type = FE_NODAL_D_DS1;
				break;
			case CMZN_NODE_D_DS2:
				fe_nodal_value_type = FE_NODAL_D_DS2;
				break;
			case CMZN_NODE_D_DS3:
				fe_nodal_value_type = FE_NODAL_D_DS3;
				break;
			case CMZN_NODE_D2_DS1DS2:
				fe_nodal_value_type = FE_NODAL_D2_DS1DS2;
				break;
			case CMZN_NODE_D2_DS1DS3:
				fe_nodal_value_type = FE_NODAL_D2_DS1DS3;
				break;
			case CMZN_NODE_D2_DS2DS3:
				fe_nodal_value_type = FE_NODAL_D2_DS2DS3;
				break;
			case CMZN_NODE_D3_DS1DS2DS3:
				fe_nodal_value_type = FE_NODAL_D3_DS1DS2DS3;
				break;
		}
		return fe_nodal_value_type;
	}
};

/*============================================================================*/

struct cmzn_nodeset
{
protected:
	FE_region *fe_region;
	cmzn_field_node_group_id group;
	int access_count;

	cmzn_nodeset(cmzn_field_node_group_id group) :
		fe_region(ACCESS(FE_region)(
			Computed_field_node_group_core_cast(group)->getMasterNodeset()->fe_region)),
		group(group),
		access_count(1)
	{
		// GRC cmzn_field_node_group_access missing:
		cmzn_field_access(cmzn_field_node_group_base_cast(group));
	}

public:
	cmzn_nodeset(FE_region *fe_region_in) :
		fe_region(ACCESS(FE_region)(fe_region_in)),
		group(0),
		access_count(1)
	{
	}

	cmzn_nodeset_id access()
	{
		++access_count;
		return this;
	}

	static int deaccess(cmzn_nodeset_id &nodeset)
	{
		if (!nodeset)
			return 0;
		--(nodeset->access_count);
		if (nodeset->access_count <= 0)
			delete nodeset;
		nodeset = 0;
		return 1;
	}

	int containsNode(cmzn_node_id node)
	{
		if (group)
			return Computed_field_node_group_core_cast(group)->containsObject(node);
		return FE_region_contains_FE_node(fe_region, node);
	}

	cmzn_node_id createNode(int identifier,
		cmzn_nodetemplate_id node_template)
	{
		cmzn_node_id node = 0;
		if (node_template->validate())
		{
			cmzn_node_id template_node = node_template->getTemplateNode();
			node = ACCESS(FE_node)(FE_region_create_FE_node_copy(
				fe_region, identifier, template_node));
			if (group)
				Computed_field_node_group_core_cast(group)->addObject(node);
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"cmzn_nodeset_create_node.  Node template is not valid");
		}
		return node;
	}

	cmzn_nodetemplate_id createNodetemplate()
	{
		FE_region *master_fe_region = fe_region;
		FE_region_get_ultimate_master_FE_region(fe_region, &master_fe_region);
		return new cmzn_nodetemplate(master_fe_region);
	}

	cmzn_nodeiterator_id createIterator()
	{
		if (group)
			return Computed_field_node_group_core_cast(group)->createIterator();
		return FE_region_create_nodeiterator(fe_region);
	}

	int destroyAllNodes()
	{
		return destroyNodesConditional(/*conditional_field*/0);
	}

	int destroyNode(cmzn_node_id node)
	{
		if (containsNode(node))
		{
			FE_region *master_fe_region = fe_region;
			FE_region_get_ultimate_master_FE_region(fe_region, &master_fe_region);
			return FE_region_remove_FE_node(master_fe_region, node);
		}
		return 0;
	}

	int destroyNodesConditional(cmzn_field_id conditional_field)
	{
		struct LIST(FE_node) *node_list = createNodeListWithCondition(conditional_field);
		FE_region *master_fe_region = fe_region;
		FE_region_get_ultimate_master_FE_region(fe_region, &master_fe_region);
		int return_code = FE_region_remove_FE_node_list(master_fe_region, node_list);
		DESTROY(LIST(FE_node))(&node_list);
		return return_code;
	}

	cmzn_node_id findNodeByIdentifier(int identifier) const
	{
		cmzn_node_id node = 0;
		if (group)
		{
			node = Computed_field_node_group_core_cast(group)->findNodeByIdentifier(identifier);
		}
		else
		{
			node = FE_region_get_FE_node_from_identifier(fe_region, identifier);
		}
		if (node)
			ACCESS(FE_node)(node);
		return node;
	}

	FE_region *getFeRegion() const { return fe_region; }

	char *getName()
	{
		char *name = 0;
		if (group)
		{
			name = cmzn_field_get_name(cmzn_field_node_group_base_cast(group));
		}
		else if (FE_region_is_data_FE_region(fe_region))
		{
			name = duplicate_string("datapoints");
		}
		else
		{
			name = duplicate_string("nodes");
		}
		return name;
	}

	cmzn_nodeset_id getMaster()
	{
		if (!isGroup())
			return access();
		FE_region *master_fe_region = fe_region;
		if (FE_region_get_ultimate_master_FE_region(fe_region, &master_fe_region) && master_fe_region)
			return new cmzn_nodeset(master_fe_region);
		return 0;
	}

	int getSize() const
	{
		if (group)
			return Computed_field_node_group_core_cast(group)->getSize();
		return FE_region_get_number_of_FE_nodes(fe_region);
	}

	int isGroup()
	{
		return (0 != group);
	}

	bool match(cmzn_nodeset& other_nodeset)
	{
		return ((fe_region == other_nodeset.fe_region) &&
			(group == other_nodeset.group));
	}

protected:
	~cmzn_nodeset()
	{
		if (group)
			cmzn_field_node_group_destroy(&group);
		DEACCESS(FE_region)(&fe_region);
	}

	struct LIST(FE_node) *createNodeListWithCondition(cmzn_field_id conditional_field)
	{
		cmzn_region_id region = FE_region_get_master_cmzn_region(fe_region);
		cmzn_fieldmodule_id field_module = cmzn_region_get_fieldmodule(region);
		cmzn_fieldcache_id cache = cmzn_fieldmodule_create_fieldcache(field_module);
		cmzn_nodeiterator_id iterator = createIterator();
		cmzn_node_id node = 0;
		struct LIST(FE_node) *node_list = FE_region_create_related_node_list(fe_region);
		while (0 != (node = cmzn_nodeiterator_next_non_access(iterator)))
		{
			cmzn_fieldcache_set_node(cache, node);
			if ((!conditional_field) || cmzn_field_evaluate_boolean(conditional_field, cache))
				ADD_OBJECT_TO_LIST(FE_node)(node, node_list);
		}
		cmzn_nodeiterator_destroy(&iterator);
		cmzn_fieldcache_destroy(&cache);
		cmzn_fieldmodule_destroy(&field_module);
		return node_list;
	}

};

struct cmzn_nodeset_group : public cmzn_nodeset
{
public:

	cmzn_nodeset_group(cmzn_field_node_group_id group) :
		cmzn_nodeset(group)
	{
	}

	int addNode(cmzn_node_id node)
	{
		return Computed_field_node_group_core_cast(group)->addObject(node);
	}

	int removeAllNodes()
	{
		return Computed_field_node_group_core_cast(group)->clear();
	}

	int removeNode(cmzn_node_id node)
	{
		return Computed_field_node_group_core_cast(group)->removeObject(node);
	}

	int removeNodesConditional(cmzn_field_id conditional_field)
	{
		return Computed_field_node_group_core_cast(group)->removeNodesConditional(conditional_field);
	}

	int addElementNodes(cmzn_element_id element)
	{
		return Computed_field_node_group_core_cast(group)->addElementNodes(element);
	}

	int removeElementNodes(cmzn_element_id element)
	{
		return Computed_field_node_group_core_cast(group)->removeElementNodes(element);
	}

};

/*
Global functions
----------------
*/

cmzn_nodeset_id cmzn_fieldmodule_find_nodeset_by_domain_type(
	cmzn_fieldmodule_id field_module, enum cmzn_field_domain_type domain_type)
{
	cmzn_nodeset_id nodeset = 0;
	if (field_module)
	{
		cmzn_region_id region = cmzn_fieldmodule_get_region_internal(field_module);
		FE_region *fe_region = 0;
		if (CMZN_FIELD_DOMAIN_NODES == domain_type)
		{
			fe_region = cmzn_region_get_FE_region(region);
		}
		else if (CMZN_FIELD_DOMAIN_DATA == domain_type)
		{
			fe_region = FE_region_get_data_FE_region(cmzn_region_get_FE_region(region));
		}
		if (fe_region)
		{
			nodeset = new cmzn_nodeset(fe_region);
		}
	}
	return nodeset;
}

cmzn_nodeset_id cmzn_fieldmodule_find_nodeset_by_name(
	cmzn_fieldmodule_id field_module, const char *nodeset_name)
{
	cmzn_nodeset_id nodeset = 0;
	if (field_module && nodeset_name)
	{
		cmzn_field_id field = cmzn_fieldmodule_find_field_by_name(field_module, nodeset_name);
		if (field)
		{
			cmzn_field_node_group_id node_group_field = cmzn_field_cast_node_group(field);
			if (node_group_field)
			{
				nodeset = cmzn_nodeset_group_base_cast(cmzn_field_node_group_get_nodeset(node_group_field));
				cmzn_field_node_group_destroy(&node_group_field);
			}
			cmzn_field_destroy(&field);
		}
		else
		{
			if (0 == strcmp(nodeset_name, "nodes"))
			{
				nodeset = cmzn_fieldmodule_find_nodeset_by_domain_type(field_module, CMZN_FIELD_DOMAIN_NODES);
			}
			else if (0 == strcmp(nodeset_name, "datapoints"))
			{
				nodeset = cmzn_fieldmodule_find_nodeset_by_domain_type(field_module, CMZN_FIELD_DOMAIN_DATA);
			}
		}
	}
	return nodeset;
}

cmzn_nodeset_id cmzn_nodeset_access(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->access();
	return 0;
}

int cmzn_nodeset_destroy(cmzn_nodeset_id *nodeset_address)
{
	if (nodeset_address)
		return cmzn_nodeset::deaccess(*nodeset_address);
	return 0;
}

int cmzn_nodeset_contains_node(cmzn_nodeset_id nodeset, cmzn_node_id node)
{
	if (nodeset && node)
		return nodeset->containsNode(node);
	return 0;
}

cmzn_nodetemplate_id cmzn_nodeset_create_nodetemplate(
	cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->createNodetemplate();
	return 0;
}

cmzn_node_id cmzn_nodeset_create_node(cmzn_nodeset_id nodeset,
	int identifier, cmzn_nodetemplate_id node_template)
{
	if (nodeset && node_template)
		return nodeset->createNode(identifier, node_template);
	return 0;
}

cmzn_nodeiterator_id cmzn_nodeset_create_nodeiterator(
	cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->createIterator();
	return 0;
}

cmzn_node_id cmzn_nodeset_find_node_by_identifier(cmzn_nodeset_id nodeset,
	int identifier)
{
	if (nodeset)
		return nodeset->findNodeByIdentifier(identifier);
	return 0;
}

char *cmzn_nodeset_get_name(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->getName();
	return 0;
}

int cmzn_nodeset_get_size(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->getSize();
	return 0;
}

int cmzn_nodeset_destroy_all_nodes(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		nodeset->destroyAllNodes();
	return 0;
}

int cmzn_nodeset_destroy_node(cmzn_nodeset_id nodeset, cmzn_node_id node)
{
	if (nodeset && node)
		nodeset->destroyNode(node);
	return 0;
}

int cmzn_nodeset_destroy_nodes_conditional(cmzn_nodeset_id nodeset,
	cmzn_field_id conditional_field)
{
	if (nodeset && conditional_field)
		return nodeset->destroyNodesConditional(conditional_field);
	return 0;
}

cmzn_nodeset_id cmzn_nodeset_get_master(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->getMaster();
	return 0;
}

bool cmzn_nodeset_match(cmzn_nodeset_id nodeset1, cmzn_nodeset_id nodeset2)
{
	return (nodeset1 && nodeset2 && nodeset1->match(*nodeset2));
}

cmzn_nodeset_group_id cmzn_nodeset_cast_group(cmzn_nodeset_id nodeset)
{
	if (nodeset && nodeset->isGroup())
		return static_cast<cmzn_nodeset_group_id>(nodeset->access());
	return 0;
}

int cmzn_nodeset_group_destroy(cmzn_nodeset_group_id *nodeset_group_address)
{
	if (nodeset_group_address)
		return cmzn_nodeset::deaccess(*(reinterpret_cast<cmzn_nodeset_id*>(nodeset_group_address)));
	return 0;
}

int cmzn_nodeset_group_add_node(cmzn_nodeset_group_id nodeset_group, cmzn_node_id node)
{
	if (nodeset_group && node)
		return nodeset_group->addNode(node);
	return 0;
}

int cmzn_nodeset_group_remove_all_nodes(cmzn_nodeset_group_id nodeset_group)
{
	if (nodeset_group)
		return nodeset_group->removeAllNodes();
	return 0;
}

int cmzn_nodeset_group_remove_node(cmzn_nodeset_group_id nodeset_group, cmzn_node_id node)
{
	if (nodeset_group && node)
		return nodeset_group->removeNode(node);
	return 0;
}

int cmzn_nodeset_group_remove_nodes_conditional(cmzn_nodeset_group_id nodeset_group,
	cmzn_field_id conditional_field)
{
	if (nodeset_group && conditional_field)
		return nodeset_group->removeNodesConditional(conditional_field);
	return 0;
}

int cmzn_nodeset_group_add_element_nodes(
	cmzn_nodeset_group_id nodeset_group, cmzn_element_id element)
{
	if (nodeset_group && element)
		return nodeset_group->addElementNodes(element);
	return 0;
}

int cmzn_nodeset_group_remove_element_nodes(
	cmzn_nodeset_group_id nodeset_group, cmzn_element_id element)
{
	if (nodeset_group && element)
		return nodeset_group->removeElementNodes(element);
	return 0;
}

cmzn_nodeset_group_id cmzn_field_node_group_get_nodeset(
	cmzn_field_node_group_id node_group)
{
	if (node_group)
		return new cmzn_nodeset_group(node_group);
	return 0;
}

struct LIST(FE_node) *cmzn_nodeset_create_node_list_internal(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return FE_region_create_related_node_list(nodeset->getFeRegion());
	return 0;
}

FE_region *cmzn_nodeset_get_FE_region_internal(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return nodeset->getFeRegion();
	return 0;
}

cmzn_region_id cmzn_nodeset_get_region_internal(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return FE_region_get_cmzn_region(nodeset->getFeRegion());
	return 0;
}

cmzn_region_id cmzn_nodeset_get_master_region_internal(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return FE_region_get_master_cmzn_region(nodeset->getFeRegion());
	return 0;
}

int cmzn_nodeset_is_data_internal(cmzn_nodeset_id nodeset)
{
	if (nodeset)
		return FE_region_is_data_FE_region(nodeset->getFeRegion());
	return 0;
}

cmzn_nodeset_group_id cmzn_fieldmodule_create_field_nodeset_group_from_name_internal(
	cmzn_fieldmodule_id field_module, const char *nodeset_group_name)
{
	cmzn_nodeset_group_id nodeset_group = 0;
	if (field_module && nodeset_group_name)
	{
		cmzn_field_id existing_field = cmzn_fieldmodule_find_field_by_name(field_module, nodeset_group_name);
		if (existing_field)
		{
			cmzn_field_destroy(&existing_field);
		}
		else
		{
			char *group_name = duplicate_string(nodeset_group_name);
			char *nodeset_name = strrchr(group_name, '.');
			if (nodeset_name)
			{
				*nodeset_name = '\0';
				++nodeset_name;
				cmzn_nodeset_id master_nodeset = cmzn_fieldmodule_find_nodeset_by_name(field_module, nodeset_name);
				cmzn_field_id field = cmzn_fieldmodule_find_field_by_name(field_module, group_name);
				cmzn_field_group_id group = cmzn_field_cast_group(field);
				cmzn_field_node_group_id node_group = cmzn_field_group_create_node_group(group, master_nodeset);
				nodeset_group = cmzn_field_node_group_get_nodeset(node_group);
				cmzn_field_node_group_destroy(&node_group);
				cmzn_field_group_destroy(&group);
				cmzn_field_destroy(&field);
				cmzn_nodeset_destroy(&master_nodeset);
			}
			DEALLOCATE(group_name);
		}
	}
	return nodeset_group;
}

cmzn_nodetemplate_id cmzn_nodetemplate_access(cmzn_nodetemplate_id node_template)
{
	if (node_template)
		return node_template->access();
	return 0;
}


int cmzn_nodetemplate_destroy(cmzn_nodetemplate_id *node_template_address)
{
	if (node_template_address)
		return cmzn_nodetemplate::deaccess(*node_template_address);
	return 0;
}

int cmzn_nodetemplate_define_field(cmzn_nodetemplate_id node_template,
	cmzn_field_id field)
{
	if (node_template && field)
	{
		return node_template->defineField(field);
	}
	return 0;
}

int cmzn_nodetemplate_define_field_from_node(
	cmzn_nodetemplate_id node_template, cmzn_field_id field,
	cmzn_node_id node)
{
	if (node_template && field && node)
	{
		return node_template->defineFieldFromNode(field, node);
	}
	return 0;
}

int cmzn_nodetemplate_define_derivative(cmzn_nodetemplate_id node_template,
	cmzn_field_id field, int component_number,
	enum cmzn_node_value_type derivative_type)
{
	if (node_template && field)
	{
		return node_template->defineDerivative(field, component_number, derivative_type);
	}
	return 0;
}

int cmzn_nodetemplate_define_time_sequence(
	cmzn_nodetemplate_id node_template, cmzn_field_id field,
	cmzn_time_sequence_id time_sequence)
{
	if (node_template && field && time_sequence)
	{
		return node_template->defineTimeSequence(field, time_sequence);
	}
	return 0;
}

int cmzn_nodetemplate_define_versions(cmzn_nodetemplate_id node_template,
	cmzn_field_id field, int component_number,
	int number_of_versions)
{
	if (node_template && field)
	{
		return node_template->defineVersions(field, component_number, number_of_versions);
	}
	return 0;
}

int cmzn_nodetemplate_get_number_of_versions(cmzn_nodetemplate_id node_template,
	cmzn_field_id field, int component_number)
{
	if (node_template && field)
	{
		return node_template->getNumberOfVersions(field, component_number);
	}
	return 0;
}

cmzn_time_sequence_id cmzn_nodetemplate_get_time_sequence(
	cmzn_nodetemplate_id node_template, cmzn_field_id field)
{
	if (node_template && field)
	{
		return node_template->getTimeSequence(field);
	}
	return 0;
}

int cmzn_nodetemplate_has_derivative(cmzn_nodetemplate_id node_template,
	cmzn_field_id field, int component_number,
	enum cmzn_node_value_type derivative_type)
{
	if (node_template && field)
	{
		return node_template->hasDerivative(field, component_number, derivative_type);
	}
	return 0;
}

int cmzn_nodetemplate_undefine_field(cmzn_nodetemplate_id node_template,
	cmzn_field_id field)
{
	if (node_template && field)
	{
		return node_template->undefineField(field);
	}
	return 0;
}

cmzn_node_id cmzn_node_access(cmzn_node_id node)
{
	if (node)
		return ACCESS(FE_node)(node);
	return 0;
}

int cmzn_node_destroy(cmzn_node_id *node_address)
{
	return DEACCESS(FE_node)(node_address);
}

int cmzn_node_get_identifier(cmzn_node_id node)
{
	return get_FE_node_identifier(node);
}

int cmzn_node_merge(cmzn_node_id node, cmzn_nodetemplate_id node_template)
{
	if (node && node_template)
		return node_template->mergeIntoNode(node);
	return 0;
}

class cmzn_node_value_type_conversion
{
public:
	static const char *to_string(enum cmzn_node_value_type type)
	{
		const char *enum_string = 0;
		switch (type)
		{
			case CMZN_NODE_VALUE:
				enum_string = "VALUE";
				break;
			case CMZN_NODE_D_DS1:
				enum_string = "D_DS1";
				break;
			case CMZN_NODE_D_DS2:
				enum_string = "D_DS2";
				break;
			case CMZN_NODE_D_DS3:
				enum_string = "D_DS3";
				break;
			case CMZN_NODE_D2_DS1DS2:
				enum_string = "D2_DS1DS2";
				break;
			case CMZN_NODE_D2_DS1DS3:
				enum_string = "_D2_DS1DS3";
				break;
			case CMZN_NODE_D2_DS2DS3:
				enum_string = "D2_DS2DS3";
				break;
			case CMZN_NODE_D3_DS1DS2DS3:
				enum_string = "D3_DS1DS2DS3";
				break;
			default:
				break;
		}
		return enum_string;
	}
};

enum cmzn_node_value_type cmzn_node_value_type_enum_from_string(
	const char *string)
{
	return string_to_enum<enum cmzn_node_value_type,	cmzn_node_value_type_conversion>(string);
}

char *cmzn_node_value_type_enum_to_string(enum cmzn_node_value_type type)
{
	const char *type_string = cmzn_node_value_type_conversion::to_string(type);
	return (type_string ? duplicate_string(type_string) : 0);
}
