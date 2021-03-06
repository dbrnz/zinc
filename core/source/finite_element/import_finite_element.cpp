/**
 * FILE : import_finite_element.cpp
 *
 * Functions for importing finite element data from a EX file format.
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <cmath>
#include "opencmiss/zinc/fieldgroup.h"
#include "opencmiss/zinc/fieldmodule.h"
#include "opencmiss/zinc/fieldsubobjectgroup.h"
#include "finite_element/finite_element.h"
#include "finite_element/finite_element_mesh.hpp"
#include "finite_element/finite_element_nodeset.hpp"
#include "finite_element/finite_element_region.h"
#include "finite_element/finite_element_time.h"
#include "finite_element/import_finite_element.h"
#include "general/debug.h"
#include "general/math.h"
#include "general/io_stream.h"
#include "general/mystring.h"
#include "general/message.h"

#include <ctype.h>

/*
Module types
------------
*/

/*
Module functions
----------------
*/

static int read_element_xi_value(struct IO_stream *input_file,
	struct cmzn_region *root_region, struct cmzn_region *current_region,
	struct FE_element **element_address, FE_value *xi)
/*******************************************************************************
LAST MODIFIED : 27 May 2003

DESCRIPTION :
Reads an element:xi position in from the <input_file> in the format:
[REGION_PATH] E<lement>/F<ace>/L<ine> ELEMENT_NUMBER DIMENSION
xi1 xi2... xiDIMENSION
The REGION_PATH is the path relative to the <root_region>, using forward slashes
for separators, to the cmiss_region containing the fe_region the element is in.
Omitting the region path is equivalent to having a "/" and means the fe_region
is in the root_region itself. The REGION_PATH must end in
<xi> should have space for up to MAXIMUM_ELEMENT_XI_DIMENSIONS FE_values.
==============================================================================*/
{
	char *location;
	int dimension, k, return_code;
	struct cmzn_region *region;
	struct FE_element *element;
	struct FE_region *fe_region;

	ENTER(read_element_xi_value);
	if (input_file && root_region && element_address && xi)
	{
		return_code = 1;
		/* determine the region path, element type and element number. First read
			 two strings and determine if the second is a number, in which case the
			 region path is omitted */
		region = (struct cmzn_region *)NULL;
		char *whitespace_string = 0;
		char *first_string = 0;
		char *separator_string = 0;
		char *second_string = 0;
		int identifier;
		IO_stream_read_string(input_file, "[ \n\r\t]", &whitespace_string);
		if (IO_stream_read_string(input_file, "[^ \n\r\t]", &first_string) &&
			IO_stream_read_string(input_file, "[ \n\r\t]", &separator_string) &&
			IO_stream_read_string(input_file, "[^ \n\r\t]", &second_string))
		{
			char *element_type_string = 0;
			/* first determine the element_number, which is in the second_string
				 if the region path has been omitted, otherwise next in the file */
			if (1 == sscanf(second_string, " %d", &identifier))
			{
				// Note default changed from root_region to current_region
				region = current_region;
				element_type_string = first_string;
			}
			else if (1 == IO_stream_scan(input_file, " %d", &identifier))
			{
				if (cmzn_region_get_region_from_path_deprecated(root_region, first_string,
					&region) && region)
				{
					element_type_string = second_string;
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Invalid region path %s in element:xi value.  %s",
						first_string, location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			else
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Missing element number in element:xi value.  %s",
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
			/* determine the element_type. Redundant since elements stored by dimension */
			if (element_type_string)
			{
				if (fuzzy_string_compare(element_type_string, "element"))
				{
				}
				else if (fuzzy_string_compare(element_type_string, "face"))
				{
				}
				else if (fuzzy_string_compare(element_type_string, "line"))
				{
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Unknown element type %s for element_xi value.  %s",
						element_type_string, location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"Missing region path, element type or number in element:xi value.  "
				"%s", location);
			DEALLOCATE(location)
			return_code = 0;
		}
		DEALLOCATE(second_string);
		DEALLOCATE(separator_string);
		DEALLOCATE(first_string);
		DEALLOCATE(whitespace_string);
		if (return_code)
		{
			if (NULL != (fe_region = cmzn_region_get_FE_region(region)))
			{
				element = (struct FE_element *)NULL;
				if ((1 == IO_stream_scan(input_file, " %d", &dimension)) && (0 < dimension))
				{
					/* get existing element and check it has the dimension, or create
						 a dummy element with unspecified shape and the dimension */
					FE_mesh *fe_mesh = FE_region_find_FE_mesh_by_dimension(fe_region, dimension);
					if (fe_mesh && (NULL != (element = fe_mesh->get_or_create_FE_element_with_identifier(
						identifier, static_cast<FE_element_shape *>(0)))))
					{
						*element_address = element;
						DEACCESS(FE_element)(&element);
						/* now read the xi position */
						for (k = 0; (k < dimension) && return_code; k++)
						{
							if (1 == IO_stream_scan(input_file, FE_VALUE_INPUT_STRING, &(xi[k])))
							{
								if (!finite(xi[k]))
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Infinity or NAN xi coordinates read from file.  %s",
										location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Missing %d xi value(s).  %s",
									dimension - k, location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
					}
					else
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE, "read_element_xi_value.  "
							"Could not get or create element.  %s",
							location);
						DEALLOCATE(location);
						return_code = 0;
					}
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE, "Error reading dimension.  %s",
						location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			else
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"cmzn region does not contain a finite element region.  %s", location);
				DEALLOCATE(location);
				return_code = 0;
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_element_xi_value.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* read_element_xi_value */

static int read_string_value(struct IO_stream *input_file, char **string_address)
/*******************************************************************************
LAST MODIFIED : 15 January 2002

DESCRIPTION :
Returns an allocated string with the next contiguous block (length>0) of
characters from input_file not containing whitespace (space, formfeed, newline,
carriage return, tab, vertical tab). If the string begins with EITHER a single
or double quote, ' or ", then the string must end in the same quote mark
followed by whitespace or EOF.
Special characters including the quote marks, $ and backslash must be preceded
by the escape/backslash character in the input file.
==============================================================================*/
{
	char *location, *the_string;
	int allocated_length, length, quote_mark, reading_token, return_code = 0,
		this_char;

	ENTER(read_string_value);
	if (input_file && string_address)
	{
		the_string = (char *)NULL;
		allocated_length = 0; /* including space for \0 at end */
		length = 0;
		/* pass over leading white space */
		while (isspace(this_char = IO_stream_getc(input_file))) {}
		/* determine if string is in quotes and which quote_mark is in use */
		if (((int)'\'' == this_char) || ((int)'\"' == this_char))
		{
			quote_mark = this_char;
			this_char = IO_stream_getc(input_file);
		}
		else
		{
			quote_mark = 0;
		}
		reading_token = 1;
		/* read token until [quote_mark+]EOF/whitespace */
		while (reading_token)
		{
			if (EOF == this_char)
			{
				if (quote_mark)
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"End of file before end quote mark.  %s",
						location);
					DEALLOCATE(location);
					return_code = 0;
				}
				if (!the_string)
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Missing string in input file.  %s",
						location);
					DEALLOCATE(location);
					return_code = 0;
				}
				reading_token = 0;
			}
			else if (!quote_mark && isspace(this_char))
			{
				reading_token = 0;
			}
			else if (quote_mark && ((int)'\\' == this_char))
			{
				this_char = IO_stream_getc(input_file);
				if (!(((int)'\\' == this_char) ||
					((int)'\"' == this_char) ||
					((int)'\'' == this_char) ||
					((int)'$' == this_char)))
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Invalid escape sequence: \\%c.  %s", this_char,
						location);
					DEALLOCATE(the_string);
					DEALLOCATE(location);
					reading_token = 0;
					return_code = 0;
				}
			}
			else if (quote_mark && (quote_mark == this_char))
			{
				this_char = IO_stream_getc(input_file);
				if ((EOF == this_char) || isspace(this_char))
				{
					if (!the_string)
					{
						/* for empty string "" or '' */
						if (ALLOCATE(the_string, char, 1))
						{
							*string_address = the_string;
						}
						else
						{
							display_message(ERROR_MESSAGE,
								"read_string_value.  Not enough memory");
							return_code = 0;
						}
					}
					reading_token = 0;
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Must have white space after end quote.  %s",
						location);
					DEALLOCATE(location);
					DEALLOCATE(the_string);
					reading_token = 0;
					return_code = 0;
				}
			}
			if (reading_token)
			{
				length++;
				/* is the current string big enough (including \0 at end)? */
				if (allocated_length < length + 1)
				{
					allocated_length += 25;
					if (REALLOCATE(*string_address, the_string, char, allocated_length))
					{
						the_string = *string_address;
					}
					else
					{
						display_message(ERROR_MESSAGE,
							"read_string_value.  Not enough memory");
						DEALLOCATE(the_string);
						reading_token = 0;
						return_code = 0;
					}
				}
				if (the_string)
				{
					the_string[length - 1] = (char)this_char;
				}
				this_char = IO_stream_getc(input_file);
			}
		}
		if (the_string)
		{
			the_string[length] = '\0';
			return_code = 1;
		}
		*string_address = the_string;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_string_value.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* read_string_value */

static struct FE_field *read_FE_field(struct IO_stream *input_file,
	struct FE_region *fe_region)
/*******************************************************************************
LAST MODIFIED : 2 May 2003

DESCRIPTION :
Reads a field from its descriptor in <input_file>. Note that the same format is
used for node and element field headers. The field is returned.
The returned field will be "of" <fe_region>, but not in it. This means it has
access to information such as FE_time that is private to <fe_region> and can be
simply merged into it using FE_region_merge_FE_field.
This approach is used because component names are set later and differently
for node and element fields.
==============================================================================*/
{
	char *field_name, *location, *next_block;
	enum CM_field_type cm_field_type;
	enum FE_field_type fe_field_type = UNKNOWN_FE_FIELD;
	enum Value_type value_type = UNKNOWN_VALUE;
	FE_value focus;
	int element_xi_mesh_dimension = 0;
	int number_of_components, number_of_indexed_values, return_code;
	struct Coordinate_system coordinate_system;
	struct FE_field *field, *indexer_field = NULL, *temp_indexer_field;

	ENTER(read_FE_field);
	field = (struct FE_field *)NULL;
	if (input_file && fe_region)
	{
		return_code = 1;
		field_name = (char *)NULL;
		/* read the field information */
		IO_stream_scan(input_file, " %*d) ");
		/* read the field name */
		if (return_code)
		{
			if (IO_stream_read_string(input_file, "[^,]", &field_name))
			{
				IO_stream_scan(input_file, ", ");
				/* remove trailing blanks off field name */
				size_t i = strlen(field_name);
				while ((0 < i) && (isspace(field_name[i - 1])))
					--i;
				field_name[i] = '\0';
				if (0 == i)
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE, "No field name.  %s",
						location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			else
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE, "Missing field name.  %s",
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
		}
		next_block = (char *)NULL;
		if (return_code)
		{
			/* next string required for CM_field_type, below */
			if (!IO_stream_read_string(input_file, "[^,]", &next_block))
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Field '%s' missing CM field type.  %s", field_name,
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
			IO_stream_scan(input_file, ", ");
		}
		/* read the CM_field_type */
		if (return_code && next_block)
		{
			if (!STRING_TO_ENUMERATOR(CM_field_type)(next_block, &cm_field_type))
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Field '%s' has unknown CM field type '%s'.  %s", field_name,
					next_block, location);
				DEALLOCATE(location);
				return_code = 0;
			}
		}
		if (next_block)
		{
			DEALLOCATE(next_block);
			next_block = (char *)NULL;
		}
		/* read the FE_field_information */
		if (return_code)
		{
			/* next string required for value_type, below */
			if (!IO_stream_read_string(input_file, "[^,]", &next_block))
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Field '%s' missing field/value type.  %s", field_name,
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
			IO_stream_scan(input_file, ", ");
		}
		/* read the optional modifier: constant|indexed */
		if (return_code && next_block)
		{
			if (fuzzy_string_compare_same_length(next_block, "constant"))
			{
				fe_field_type = CONSTANT_FE_FIELD;
			}
			else if (fuzzy_string_compare_same_length(next_block, "indexed"))
			{
				fe_field_type = INDEXED_FE_FIELD;
				DEALLOCATE(next_block);
				if ((EOF != IO_stream_scan(input_file, " Index_field = ")) &&
					IO_stream_read_string(input_file, "[^,]", &next_block))
				{
					if (!(indexer_field =
						FE_region_get_FE_field_from_name(fe_region, next_block)))
					{
						/* create and merge an appropriate indexer field */
						temp_indexer_field = CREATE(FE_field)(next_block, fe_region);
						ACCESS(FE_field)(temp_indexer_field);
						if (!(set_FE_field_number_of_components(temp_indexer_field, 1) &&
							set_FE_field_value_type(temp_indexer_field, INT_VALUE) &&
							(indexer_field = FE_region_merge_FE_field(fe_region,
								temp_indexer_field))))
						{
							return_code = 0;
						}
						DEACCESS(FE_field)(&temp_indexer_field);
					}
					if (return_code)
					{
						if (!((1 == IO_stream_scan(input_file, ", #Values=%d",
							&number_of_indexed_values)) && (0 < number_of_indexed_values)))
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Field '%s' missing number of indexed values.  %s",
								field_name, location);
							DEALLOCATE(location);
							return_code = 0;
						}
					}
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Field '%s' missing indexing information.  %s", field_name,
						location);
					DEALLOCATE(location);
					return_code = 0;
				}
				IO_stream_scan(input_file, ", ");
			}
			else
			{
				fe_field_type = GENERAL_FE_FIELD;
			}
			if (GENERAL_FE_FIELD != fe_field_type)
			{
				DEALLOCATE(next_block);
				if (return_code)
				{
					/* next string required for coordinate system or value_type */
					return_code = IO_stream_read_string(input_file, "[^,]", &next_block);
					IO_stream_scan(input_file, ", ");
				}
			}
		}
		else
		{
			if (next_block)
			{
				DEALLOCATE(next_block);
			}
		}
		/* read the coordinate system (optional) */
		coordinate_system.type = NOT_APPLICABLE;
		if (return_code && next_block)
		{
			if (fuzzy_string_compare_same_length(next_block,
				"rectangular cartesian"))
			{
				coordinate_system.type = RECTANGULAR_CARTESIAN;
			}
			else if (fuzzy_string_compare_same_length(next_block,
				"cylindrical polar"))
			{
				coordinate_system.type = CYLINDRICAL_POLAR;
			}
			else if (fuzzy_string_compare_same_length(next_block,
				"spherical polar"))
			{
				coordinate_system.type = SPHERICAL_POLAR;
			}
			else if (fuzzy_string_compare_same_length(next_block,
				"prolate spheroidal"))
			{
				coordinate_system.type = PROLATE_SPHEROIDAL;
				IO_stream_scan(input_file, " focus=");
				if ((1 != IO_stream_scan(input_file, FE_VALUE_INPUT_STRING, &focus)) ||
					(!finite(focus)))
				{
					focus = 1.0;
				}
				coordinate_system.parameters.focus = focus;
				IO_stream_scan(input_file, " ,");
			}
			else if (fuzzy_string_compare_same_length(next_block,
				"oblate spheroidal"))
			{
				coordinate_system.type = OBLATE_SPHEROIDAL;
				IO_stream_scan(input_file," focus=");
				if ((1 != IO_stream_scan(input_file,FE_VALUE_INPUT_STRING, &focus)) ||
					(!finite(focus)))
				{
					focus = 1.0;
				}
				coordinate_system.parameters.focus = focus;
				IO_stream_scan(input_file, " ,");
			}
			else if (fuzzy_string_compare_same_length(next_block,
				"fibre"))
			{
				coordinate_system.type = FIBRE;
				value_type = FE_VALUE_VALUE;
			}
			if (NOT_APPLICABLE != coordinate_system.type)
			{
				DEALLOCATE(next_block);
				if (return_code)
				{
					/* next string required for value_type, below */
					return_code = IO_stream_read_string(input_file, "[^,\n\r]", &next_block);
					IO_stream_scan(input_file, ", ");
				}
			}
		}
		else
		{
			if (next_block)
			{
				DEALLOCATE(next_block);
			}
		}
		/* read the value_type */
		if (return_code && next_block)
		{
			value_type = Value_type_from_string(next_block);
			if (UNKNOWN_VALUE == value_type)
			{
				if (coordinate_system.type != NOT_APPLICABLE)
				{
					/* for backwards compatibility default to FE_VALUE_VALUE if
						coordinate system specified */
					value_type = FE_VALUE_VALUE;
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Field '%s' has unknown value_type %s.  %s", field_name,
						next_block, location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			else
			{
				DEALLOCATE(next_block);
				/* next string required for value_type, below */
				return_code = IO_stream_read_string(input_file,"[^,\n\r]", &next_block);
			}
		}
		else
		{
			if (next_block)
			{
				DEALLOCATE(next_block);
			}
		}
		if (return_code && next_block)
		{
			if (!((1 == sscanf(next_block, " #Components=%d", &number_of_components))
				&& (0 < number_of_components)))
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Field '%s' missing #Components.  %s", field_name,
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
			if (return_code && (ELEMENT_XI_VALUE == value_type))
			{
				char *mesh_dimension_loc = strstr(next_block, "mesh dimension");
				if (mesh_dimension_loc)
				{
					if ((1 != sscanf(mesh_dimension_loc, "mesh dimension=%d", &element_xi_mesh_dimension)) ||
						((0 >= element_xi_mesh_dimension) ||
							(element_xi_mesh_dimension > MAXIMUM_ELEMENT_XI_DIMENSIONS)))
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE,
							"Field '%s' of element_xi value has invalid mesh dimension.  %s",
							field_name, location);
						DEALLOCATE(location);
						return_code = 0;
					}
				}
			}
		}
		if (next_block)
		{
			DEALLOCATE(next_block);
		}
		if (return_code)
		{
			/* create the field */
			field = CREATE(FE_field)(field_name, fe_region);
			if (!set_FE_field_value_type(field, value_type))
			{
				return_code = 0;
			}
			if (0 != element_xi_mesh_dimension)
			{
				if (!FE_field_set_element_xi_mesh_dimension(field, element_xi_mesh_dimension))
				{
					return_code = 0;
				}
			}
			if (!set_FE_field_number_of_components(field, number_of_components))
			{
				return_code = 0;
			}
			if (!(((CONSTANT_FE_FIELD != fe_field_type) ||
				set_FE_field_type_constant(field)) &&
				((GENERAL_FE_FIELD != fe_field_type) ||
					set_FE_field_type_general(field)) &&
				((INDEXED_FE_FIELD != fe_field_type) ||
					set_FE_field_type_indexed(field, indexer_field,
						number_of_indexed_values))))
			{
				return_code = 0;
			}
			if (!set_FE_field_CM_field_type(field, cm_field_type))
			{
				return_code = 0;
			}
			if (!((set_FE_field_coordinate_system(field, &coordinate_system))))
			{
				return_code = 0;
			}
			if (!return_code)
			{
				display_message(ERROR_MESSAGE,
					"read_FE_field.  Could not create field '%s'", field_name);
				if (field)
				{
					DESTROY(FE_field)(&field);
					field = (struct FE_field *)NULL;
				}
			}
		}
		DEALLOCATE(field_name);
	}
	else
	{
		display_message(ERROR_MESSAGE, "read_FE_field.  Invalid argument(s)");
	}
	LEAVE;

	return (field);
} /* read_FE_field */

static int read_FE_field_values(struct IO_stream *input_file,
	struct FE_region *fe_region, struct cmzn_region *root_region,
	struct cmzn_region *current_region,
	struct FE_field_order_info *field_order_info)
/*******************************************************************************
LAST MODIFIED : 29 October 2002

DESCRIPTION :
Reads in from <input_file> the values for the constant and indexed fields in
the <field_order_info>.
==============================================================================*/
{
	char *location, *rest_of_line;
	enum Value_type value_type;
	int i, k, number_of_fields, number_of_values, return_code;
	struct FE_field *field;

	ENTER(read_FE_field_values);
	return_code = 0;
	if (input_file && fe_region && root_region && field_order_info)
	{
		rest_of_line = (char *)NULL;
		IO_stream_read_string(input_file, "[^\n\r]", &rest_of_line);
		return_code = string_matches_without_whitespace(rest_of_line, "alues : ");
		DEALLOCATE(rest_of_line);
		if (return_code)
		{
			return_code = 1;
			number_of_fields =
				get_FE_field_order_info_number_of_fields(field_order_info);
			for (i = 0; (i < number_of_fields) && return_code; i++)
			{
				if (NULL != (field = get_FE_field_order_info_field(field_order_info, i)))
				{
					number_of_values = get_FE_field_number_of_values(field);
					if (0 < number_of_values)
					{
						value_type = get_FE_field_value_type(field);
						switch (value_type)
						{
							case ELEMENT_XI_VALUE:
							{
								FE_value xi[MAXIMUM_ELEMENT_XI_DIMENSIONS];
								struct FE_element *element;

								for (k = 0; (k < number_of_values) && return_code; k++)
								{
									if (!(read_element_xi_value(input_file, root_region,
										current_region, &element, xi) &&
										set_FE_field_element_xi_value(field, k, element, xi)))
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Error reading field element_xi value.  %s",
											location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
							} break;
							case FE_VALUE_VALUE:
							{
								FE_value value;

								for (k = 0;(k < number_of_values) && return_code; k++)
								{
									if (!((1 == IO_stream_scan(input_file, FE_VALUE_INPUT_STRING, &value))
										&& finite(value) &&
										set_FE_field_FE_value_value(field, k, value)))
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Error reading field FE_value.  %s",
											location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
							} break;
							case INT_VALUE:
							{
								int value;

								for (k = 0; (k < number_of_values) && return_code; k++)
								{
									if (!((1 == IO_stream_scan(input_file, "%d", &value)) &&
										set_FE_field_int_value(field, k, value)))
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Error reading field int.  %s",
											location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
							} break;
							case STRING_VALUE:
							{
								char *the_string;

								for (k = 0; (k < number_of_values) && return_code; k++)
								{
									if (read_string_value(input_file, &the_string))
									{
										if (!set_FE_field_string_value(field, k, the_string))
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"read_FE_field_values.  Error setting string.  %s", location);
											DEALLOCATE(location);
											return_code = 0;
										}
										DEALLOCATE(the_string);
									}
									else
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Error reading field string.  %s",
											location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
							} break;
							default:
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Unsupported field value_type %s.  %s",
									Value_type_string(value_type), location);
								DEALLOCATE(location);
								return_code = 0;
							} break;
						}
					}
				}
			}
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"Invalid field 'Values:'.  %s", location);
			DEALLOCATE(location);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_FE_field_values.  Invalid argument(s)");
	}
	LEAVE;

	return (return_code);
} /* read_FE_field_values */

static int read_FE_node_field(struct IO_stream *input_file,
	FE_nodeset *fe_nodeset, struct FE_node *node,
	struct FE_import_time_index *time_index, struct FE_field **field_address)
/*******************************************************************************
LAST MODIFIED : 27 March 2003

DESCRIPTION :
Reads a node field from an <input_file>, adding it to the fields defined at
<node>. <field> is returned.
==============================================================================*/
{
	char *component_name, *location, *rest_of_line;
	enum FE_field_type fe_field_type;
	int component_number, number_of_components,
		number_of_derivatives, number_of_versions, return_code, temp_int;
	struct FE_field *field, *merged_fe_field;
	struct FE_node_field_creator *node_field_creator;
	struct FE_time_sequence *fe_time_sequence;

	ENTER(read_FE_node_field);
	return_code = 0;
	if (field_address)
	{
		*field_address = (struct FE_field *)NULL;
	}
	if (input_file && fe_nodeset && node && field_address)
	{
		if (NULL != (field = read_FE_field(input_file, fe_nodeset->get_FE_region())))
		{
			ACCESS(FE_field)(field);
			merged_fe_field = (struct FE_field *)NULL;
			number_of_components = get_FE_field_number_of_components(field);
			fe_field_type = get_FE_field_FE_field_type(field);
			return_code = 1;
			/* allocate memory for the components */
			component_name = (char *)NULL;
			node_field_creator = CREATE(FE_node_field_creator)(number_of_components);
			/* read the components */
			component_number = 0;
			while (return_code && (component_number < number_of_components))
			{
				IO_stream_scan(input_file, " ");
				/* read the component name */
				if (component_name)
				{
					DEALLOCATE(component_name);
					component_name = (char *)NULL;
				}
				if (IO_stream_read_string(input_file, "[^.]", &component_name))
				{
					/* strip trailing blanks from component name */
					size_t i = strlen(component_name);
					while ((0 < i) && (isspace(component_name[i - 1])))
						--i;
					component_name[i] = '\0';
					return_code = (0 < i) && set_FE_field_component_name(field,
						component_number, component_name);
				}
				if (return_code)
				{
					/* component name is sufficient for non-GENERAL_FE_FIELD */
					if (GENERAL_FE_FIELD == fe_field_type)
					{
						/* ignore value index */
						if ((2 == IO_stream_scan(input_file,
							".  Value index=%d, #Derivatives=%d", &temp_int,
							&number_of_derivatives)) && (0 <= number_of_derivatives))
						{
							/* first number which is the value, is automatically included */
							if (IO_stream_read_string(input_file, "[^\n\r]", &rest_of_line))
							{
								char *derivative_type_name = rest_of_line;
								/* skip leading spaces */
								while (' ' == *derivative_type_name)
									++derivative_type_name;
								if (0 < number_of_derivatives)
								{
									/* derivative names, in brackets () must follow */
									const char *closing_bracket_location = strchr(derivative_type_name, ')');
									if (('(' != *derivative_type_name) || !closing_bracket_location)
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE, "Derivative types missing or invalid for field component %s.%s .  %s",
											get_FE_field_name(field), component_name, location);
										DEALLOCATE(location);
										return_code = 0;
									}
									else
									{
										++derivative_type_name;
										/* skip leading spaces */
										while (' ' == *derivative_type_name)
											++derivative_type_name;
										while (derivative_type_name != closing_bracket_location)
										{
											const char *nodal_value_type_string = derivative_type_name;
											while ((',' != *derivative_type_name) &&
												(' ' != *derivative_type_name) &&
												(')' != *derivative_type_name))
											{
												++derivative_type_name;
											}
											*derivative_type_name = '\0';
											if (derivative_type_name != closing_bracket_location)
											{
												++derivative_type_name;
												while ((',' == *derivative_type_name) || (' ' == *derivative_type_name))
													++derivative_type_name;
											}
											enum FE_nodal_value_type derivative_type;
											if (!STRING_TO_ENUMERATOR(FE_nodal_value_type)(nodal_value_type_string, &derivative_type))
											{
												location = IO_stream_get_location_string(input_file);
												display_message(ERROR_MESSAGE, "Unknown derivative type '%s' for field component %s.%s .  %s",
													nodal_value_type_string, get_FE_field_name(field), component_name, location);
												DEALLOCATE(location);
												return_code = 0;
												break;
											}
											else
											{
												int result = FE_node_field_creator_define_derivative(
													node_field_creator, component_number, derivative_type);
												if (result != CMZN_OK)
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE, "Failed to set derivative type '%s' for field component %s.%s %s.  %s",
														nodal_value_type_string, get_FE_field_name(field), component_name,
														(CMZN_ERROR_ALREADY_EXISTS == result) ? "as already defined" : "", location);
													DEALLOCATE(location);
													return_code = 0;
													break;
												}
											}
										}
										int count = FE_node_field_creator_get_number_of_derivatives(node_field_creator, component_number);
										if (count != number_of_derivatives)
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE, "Missing derivative type(s) for field component %s.%s .  %s",
												get_FE_field_name(field), component_name, location);
											DEALLOCATE(location);
											return_code = 0;
											break;
										}
									}
									++derivative_type_name;
								}
								/* read in the number of versions (if present) */
								if (return_code && (1 == sscanf(derivative_type_name, ", #Versions=%d", &number_of_versions)))
								{
									int result = FE_node_field_creator_define_versions(node_field_creator,
										component_number, number_of_versions);
									if (CMZN_OK != result)
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE, "Invalid #Versions for field component %s.%s .  %s",
											get_FE_field_name(field), component_name, location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
								DEALLOCATE(rest_of_line);
							}
							else
							{
								display_message(ERROR_MESSAGE,
									"read_FE_node_field.  Could not read rest_of_line");
								return_code = 0;
							}
						}
					}
					else
					{
						/* non GENERAL_FE_FIELD */
						/* check there is nothing on remainder of line */
						if (IO_stream_read_string(input_file, "[^\n\r]", &rest_of_line))
						{
							if (!fuzzy_string_compare(rest_of_line, "."))
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Unexpected text on field '%s' component '%s'"
									".  %s: %s", get_FE_field_name(field), component_name,
									location, rest_of_line);
								DEALLOCATE(location);
								return_code = 0;
							}
							DEALLOCATE(rest_of_line);
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Unexpected end of field '%s' component '%s'.  %s",
								get_FE_field_name(field), component_name,
								location);
							DEALLOCATE(location);
							return_code = 0;
						}
					}
					component_number++;
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Error establishing component name.  Line",
						location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			if (return_code)
			{
				/* first try to retrieve matching field from fe_region */
				if (!(merged_fe_field = FE_region_merge_FE_field(fe_nodeset->get_FE_region(), field)))
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,"read_FE_node_field.  "
						"Could not merge field '%s' into finite element region.  %s",
						get_FE_field_name(field), location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			if (return_code)
			{
				/* define merged_fe_field at the node */
				if (time_index)
				{
					if (!(fe_time_sequence = FE_region_get_FE_time_sequence_matching_series(
						fe_nodeset->get_FE_region(), 1, &(time_index->time))))
					{
						display_message(ERROR_MESSAGE,
							"read_FE_node_field.  Could not get time version");
						return_code = 0;
					}
				}
				else
				{
					fe_time_sequence = (struct FE_time_sequence *)NULL;
				}
				if (return_code)
				{
					if (define_FE_field_at_node(node, merged_fe_field, fe_time_sequence,
						node_field_creator))
					{
						*field_address = merged_fe_field;
					}
					else
					{
						display_message(ERROR_MESSAGE,
							"read_FE_node_field.  Could not define field at node");
						return_code = 0;
					}
				}
			}
			DESTROY(FE_node_field_creator)(&node_field_creator);
			if (component_name)
			{
				DEALLOCATE(component_name);
			}
			DEACCESS(FE_field)(&field);
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"read_FE_node_field.  Could not read field");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_FE_node_field.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* read_FE_node_field */

static struct FE_node *read_FE_node_field_info(struct IO_stream *input_file,
	FE_nodeset *fe_nodeset, struct FE_field_order_info **field_order_info,
	struct FE_import_time_index *time_index)
/*******************************************************************************
LAST MODIFIED : 27 February 2003

DESCRIPTION :
Creates a node with the field information read from <input_file>.
Creates, fills in and returns field_order_info.
<*field_order_info> is reallocated here so should be either NULL or returned
from a previous call to this function.
==============================================================================*/
{
	char *location;
	int number_of_fields,return_code,i;
	struct FE_node *node;
	struct FE_field *field;

	ENTER(read_FE_node_field_info);
	node = (struct FE_node *)NULL;
	if (input_file && fe_nodeset && field_order_info)
	{
		if (*field_order_info)
		{
			DESTROY(FE_field_order_info)(field_order_info);
			*field_order_info = (struct FE_field_order_info *)NULL;
		}
		/* create a node to store the field information in */
		if (NULL != (node = CREATE(FE_node)(0, fe_nodeset, (struct FE_node *)NULL)))
		{
			return_code = 1;
			if ((1 == IO_stream_scan(input_file, "Fields=%d", &number_of_fields)) &&
				(0 <= number_of_fields))
			{
				*field_order_info = CREATE(FE_field_order_info)();
				/* read in the node fields */
				for (i = 0; (i < number_of_fields) && return_code; i++)
				{
					field = (struct FE_field *)NULL;
					if (read_FE_node_field(input_file, fe_nodeset, node, time_index,
						&field))
					{
						if (!add_FE_field_order_info_field(*field_order_info, field))
						{
							display_message(ERROR_MESSAGE,
								"read_FE_node_field_info.  Could not add field to list");
							return_code = 0;
						}
					}
					else
					{
						display_message(ERROR_MESSAGE,
							"read_FE_node_field_info.  Could not read node field");
						return_code = 0;
					}
				}
			}
			else
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Error reading number of fields from file.  %s",
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
			if (!return_code)
			{
				DESTROY(FE_node)(&node);
				node = (struct FE_node *)NULL;
			}
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"read_FE_node_field_info.  Could not create node");
			return_code = 0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_FE_node_field_info.  Invalid argument(s)");
	}
	LEAVE;

	return (node);
} /* read_FE_node_field_info */

/***************************************************************************//**
 * Reads in a node from an <input_file>.
 * @param group  Optional group to put node into.
 */
static struct FE_node *read_FE_node(struct IO_stream *input_file,
	struct FE_node *template_node, FE_nodeset *fe_nodeset,
	cmzn_region_id root_region, cmzn_region_id region,
	struct FE_field_order_info *field_order_info,
	struct FE_import_time_index *time_index)
{
	char *location;
	enum Value_type value_type;
	int i, j, k, length, node_number, number_of_components, number_of_fields,
		number_of_values, return_code;
	struct FE_field *field;
	struct FE_node *node;

	ENTER(read_FE_node);
	node = (struct FE_node *)NULL;
	if (input_file && template_node && fe_nodeset && region &&
		field_order_info)
	{
		if (1 == IO_stream_scan(input_file, "ode :%d", &node_number))
		{
			return_code = 1;
			/* create node based on template node; read and fill in contents */
			if (NULL != (node = CREATE(FE_node)(node_number, (FE_nodeset *)0,
				template_node)))
			{
				number_of_fields =
					get_FE_field_order_info_number_of_fields(field_order_info);
				for (i = 0; (i < number_of_fields) && return_code; i++)
				{
					if (NULL != (field = get_FE_field_order_info_field(field_order_info, i)))
					{
						/* only GENERAL_FE_FIELD can store values at nodes */
						if (GENERAL_FE_FIELD == get_FE_field_FE_field_type(field))
						{
							number_of_components = get_FE_field_number_of_components(field);
							number_of_values = 0;
							for (j = 0; j < number_of_components; j++)
							{
								number_of_values +=
									get_FE_node_field_component_number_of_versions(node,field,j)*
									(1+get_FE_node_field_component_number_of_derivatives(node,
									field, j));
							}
							value_type = get_FE_field_value_type(field);
							if (0 < number_of_values)
							{
								switch (value_type)
								{
									case ELEMENT_XI_VALUE:
									{
										FE_value xi[MAXIMUM_ELEMENT_XI_DIMENSIONS];
										struct FE_element *element;

										if (number_of_values == number_of_components)
										{
											for (k = 0; (k < number_of_values) && return_code; k++)
											{
												if (!(read_element_xi_value(input_file, root_region, region,
													&element, xi) && set_FE_nodal_element_xi_value(node,
														field, /*component_number*/k, /*version*/0,
														FE_NODAL_VALUE, element, xi)))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE, "read_FE_node.  "
														"Error getting element_xi value for field '%s'",
														get_FE_field_name(field), location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
										}
										else
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"Derivatives/versions not supported for element_xi"
												".  %s", location);
											DEALLOCATE(location);
											return_code = 0;
										}
									} break;
									case FE_VALUE_VALUE:
									{
										FE_value *values;

										if (ALLOCATE(values, FE_value, number_of_values))
										{
											for (k = 0; (k < number_of_values) && return_code; k++)
											{
												if (1 != IO_stream_scan(input_file, FE_VALUE_INPUT_STRING,
													&(values[k])))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Error reading nodal value from file.  %s",
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
												if (!finite(values[k]))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Infinity or NAN read from node file.  %s",
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
											if (return_code)
											{
												return_code = set_FE_nodal_field_FE_value_values(field, node,
													values, &length, (time_index) ? time_index->time : 0.0);
												if (return_code)
												{
													if (length != number_of_values)
													{
														location = IO_stream_get_location_string(input_file);
														display_message(ERROR_MESSAGE,
															"node %d field '%s' took %d values from %d"
															" expected.  %s", node_number,
															get_FE_field_name(field), length,
															number_of_values, location);
														DEALLOCATE(location);
														return_code = 0;
													}
												}
											}
											DEALLOCATE(values);
										}
										else
										{
											display_message(ERROR_MESSAGE,"read_FE_node.  "
												"Insufficient memory for FE_value_values");
											return_code = 0;
										}
									} break;
									case INT_VALUE:
									{
										int *values;

										if (ALLOCATE(values,int,number_of_values))
										{
											for (k = 0; (k < number_of_values) && return_code; k++)
											{
												if (1 != IO_stream_scan(input_file, "%d", &(values[k])))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Error reading nodal value from file.  %s",
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
											if (return_code)
											{
												return_code = set_FE_nodal_field_int_values(field,
													node, values, &length);
												if (return_code)
												{
													if (length != number_of_values)
													{
														location = IO_stream_get_location_string(input_file);
														display_message(ERROR_MESSAGE,
															"node %d field '%s' took %d values from %d"
															" expected.  %s", node_number,
															get_FE_field_name(field), length,
															number_of_values, location);
														DEALLOCATE(location);
														return_code = 0;
													}
												}
											}
											DEALLOCATE(values);
										}
										else
										{
											display_message(ERROR_MESSAGE,
												"read_FE_node.  Insufficient memory for int_values");
											return_code = 0;
										}
									} break;
									case STRING_VALUE:
									{
										char *the_string;

										if (number_of_values == number_of_components)
										{
											for (k = 0; (k < number_of_values) && return_code; k++)
											{
												if (read_string_value(input_file, &the_string))
												{
													if (!set_FE_nodal_string_value(node, field,
														/*component_number*/k, /*version*/0, FE_NODAL_VALUE,
														the_string))
													{
														location = IO_stream_get_location_string(input_file);
														display_message(ERROR_MESSAGE,"read_FE_node.  "
															"Error setting string value for field '%s'",
															get_FE_field_name(field), location);
														DEALLOCATE(location);
														return_code = 0;
													}
													DEALLOCATE(the_string);
												}
												else
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Error reading string value for field '%s'."
														"  %s", get_FE_field_name(field),
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
										}
										else
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"Derivatives/versions not supported for string."
												"  %s", location);
											DEALLOCATE(location);
											return_code = 0;
										}
									} break;
									default:
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,"Unsupported value_type %s."
											"  %s",Value_type_string(value_type),
											location);
										DEALLOCATE(location);
										return_code = 0;
									} break;
								}
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"No nodal values for field '%s'.  %s",
									get_FE_field_name(field),	location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
					}
					else
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE, "Invalid field #%d.  %s",
							i + 1, location);
						DEALLOCATE(location);
						return_code = 0;
					}
				}
				if (!return_code)
				{
					DESTROY(FE_node)(&node);
				}
			}
			else
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"read_FE_node.  Could not create node.  %s",
					location);
				DEALLOCATE(location);
			}
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"read_FE_node.  Error reading node number from file.  %s", location);
			DEALLOCATE(location);

		}
	}
	else
	{
		display_message(ERROR_MESSAGE, "read_FE_node.  Invalid argument(s)");
	}
	LEAVE;

	return (node);
} /* read_FE_node */

static int read_FE_element_shape(struct IO_stream *input_file,
	struct FE_element_shape **element_shape_address, struct FE_region *fe_region)
/*******************************************************************************
LAST MODIFIED : 7 July 2003

DESCRIPTION :
Reads element shape information from <input_file>.
Note the returned shape will be NULL if the dimension is 0, denoting nodes.
==============================================================================*/
{
	char *end_description,*location,*shape_description_string,*start_description;
	int component,dimension,*first_simplex,i,j,number_of_polygon_vertices,
		previous_component,return_code,*temp_entry,*type,*type_entry,
		xi_number;
	struct FE_element_shape *element_shape;

	ENTER(read_FE_element_shape);
	element_shape = (struct FE_element_shape *)NULL;
	if (input_file && element_shape_address)
	{
		/* file input */
		if ((1 == IO_stream_scan(input_file, "hape.  Dimension=%d", &dimension)) &&
			(0 <= dimension))
		{
			return_code = 1;
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"Error reading element dimension from file.  %s",
				location);
			DEALLOCATE(location);
			return_code = 0;
		}
		if (return_code && (0 < dimension))
		{
			if (ALLOCATE(type, int, (dimension*(dimension + 1))/2))
			{
				IO_stream_scan(input_file,",");
				/* read the shape description string */
				if (IO_stream_read_string(input_file, "[^\n\r]", &shape_description_string))
				{
					if (shape_description_string)
					{
						/* trim the shape description string */
						end_description=shape_description_string+
							(strlen(shape_description_string)-1);
						while ((end_description>shape_description_string)&&
							(' '== *end_description))
						{
							end_description--;
						}
						end_description[1]='\0';
						start_description=shape_description_string;
						while (' '== *start_description)
						{
							start_description++;
						}
						if ('\0'== *start_description)
						{
							DEALLOCATE(shape_description_string);
						}
					}
					if (shape_description_string)
					{
						/* decipher the shape description */
						xi_number=0;
						type_entry=type;
						while (type&&(xi_number<dimension))
						{
							xi_number++;
							if (xi_number<dimension)
							{
								if (NULL != (end_description=strchr(start_description,'*')))
								{
									*end_description='\0';
								}
								else
								{
									DEALLOCATE(type);
								}
							}
							if (type)
							{
								if (0==strncmp(start_description,"line",4))
								{
									start_description += 4;
									*type_entry=LINE_SHAPE;
									while (' '== *start_description)
									{
										start_description++;
									}
									if ('\0'== *start_description)
									{
										type_entry++;
										for (i=dimension-xi_number;i>0;i--)
										{
											*type_entry=0;
											type_entry++;
										}
									}
									else
									{
										DEALLOCATE(type);
									}
								}
								else
								{
									if (0==strncmp(start_description,"polygon",7))
									{
										start_description += 7;
										while (' '== *start_description)
										{
											start_description++;
										}
										if ('\0'== *start_description)
										{
											/* check for link to first polygon coordinate */
											temp_entry=type_entry;
											i=xi_number-1;
											j=dimension-xi_number;
											number_of_polygon_vertices=0;
											while (type&&(i>0))
											{
												j++;
												temp_entry -= j;
												if (*temp_entry)
												{
													if (0<number_of_polygon_vertices)
													{
														DEALLOCATE(type);
													}
													else
													{
														if (!((POLYGON_SHAPE==temp_entry[i-xi_number])&&
															((number_of_polygon_vertices= *temp_entry)>=3)))
														{
															DEALLOCATE(type);
														}
													}
												}
												i--;
											}
											if (type&&(3<=number_of_polygon_vertices))
											{
												*type_entry=POLYGON_SHAPE;
												type_entry++;
												for (i=dimension-xi_number;i>0;i--)
												{
													*type_entry=0;
													type_entry++;
												}
											}
											else
											{
												DEALLOCATE(type);
											}
										}
										else
										{
											/* assign link to second polygon coordinate */
											if ((2==sscanf(start_description,"(%d ;%d )%n",
												&number_of_polygon_vertices,&component,&i))&&
												(3<=number_of_polygon_vertices)&&
												(xi_number<component)&&(component<=dimension)&&
												('\0'==start_description[i]))
											{
												*type_entry=POLYGON_SHAPE;
												type_entry++;
												i=xi_number+1;
												while (i<component)
												{
													*type_entry=0;
													type_entry++;
													i++;
												}
												*type_entry=number_of_polygon_vertices;
												type_entry++;
												while (i<dimension)
												{
													*type_entry=0;
													type_entry++;
													i++;
												}
											}
											else
											{
												DEALLOCATE(type);
											}
										}
									}
									else
									{
										if (0==strncmp(start_description,"simplex",7))
										{
											start_description += 7;
											while (' '== *start_description)
											{
												start_description++;
											}
											if ('\0'== *start_description)
											{
												/* check for link to previous simplex coordinate */
												temp_entry=type_entry;
												i=xi_number-1;
												j=dimension-xi_number;
												first_simplex=(int *)NULL;
												while (type&&(i>0))
												{
													j++;
													temp_entry -= j;
													if (*temp_entry)
													{
														if (SIMPLEX_SHAPE==temp_entry[i-xi_number])
														{
															first_simplex=temp_entry;
														}
														else
														{
															DEALLOCATE(type);
														}
													}
													i--;
												}
												if (type&&first_simplex)
												{
													*type_entry=SIMPLEX_SHAPE;
													type_entry++;
													first_simplex++;
													for (i=dimension-xi_number;i>0;i--)
													{
														*type_entry= *first_simplex;
														type_entry++;
														first_simplex++;
													}
												}
												else
												{
													DEALLOCATE(type);
												}
											}
											else
											{
												/* assign link to succeeding simplex coordinate */
												previous_component=xi_number+1;
												if ((1 == sscanf(start_description, "(%d %n",
													&component, &i)) &&
													(previous_component <= component) &&
													(component <= dimension))
												{
													*type_entry=SIMPLEX_SHAPE;
													type_entry++;
													do
													{
														start_description += i;
														while (previous_component<component)
														{
															*type_entry=0;
															type_entry++;
															previous_component++;
														}
														*type_entry=1;
														type_entry++;
														previous_component++;
													} while ((')'!=start_description[0])&&
														(1==sscanf(start_description,"%*[; ]%d %n",
														&component,&i))&&(previous_component<=component)&&
														(component<=dimension));
													if (')'==start_description[0])
													{
														/* fill rest of shape_type row with zeroes */
														while (previous_component <= dimension)
														{
															*type_entry=0;
															type_entry++;
															previous_component++;
														}
													}
													else
													{
														DEALLOCATE(type);
													}
												}
												else
												{
													DEALLOCATE(type);
												}
											}
										}
										else
										{
											DEALLOCATE(type);
										}
									}
								}
								if (type&&(xi_number<dimension))
								{
									start_description=end_description+1;
								}
							}
						}
						DEALLOCATE(shape_description_string);
					}
					else
					{
						/* retrieve a "square" element of the specified dimension */
						type_entry=type;
						for (i=dimension-1;i>=0;i--)
						{
							*type_entry=LINE_SHAPE;
							type_entry++;
							for (j=i;j>0;j--)
							{
								*type_entry=0;
								type_entry++;
							}
						}
					}
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"Error reading shape description from file.  %s",
						location);
					DEALLOCATE(type);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			else
			{
				display_message(ERROR_MESSAGE,
					"read_FE_element_shape.  Could not allocate shape type");
				return_code = 0;
			}
			if (return_code)
			{
				if (!(element_shape = CREATE(FE_element_shape)(dimension, type, fe_region)))
				{
					display_message(ERROR_MESSAGE,
						"read_FE_element_shape.  Error creating shape");
					return_code = 0;
				}
				DEALLOCATE(type);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_FE_element_shape.  Invalid argument(s)");
		return_code = 0;
	}
	if (element_shape_address)
	{
		*element_shape_address = element_shape;
	}
	LEAVE;

	return (return_code);
} /* read_FE_element_shape */

static struct FE_basis *read_FE_basis(struct IO_stream *input_file,
	struct FE_region *fe_region)
/*******************************************************************************
LAST MODIFIED : 10 November 2004

DESCRIPTION :
Reads a basis description from an <input_file> or the socket (if <input_file> is
NULL).  If the basis does not exist, it is created.  The basis is returned.
Some examples of basis descriptions in an input file are:
1. c.Hermite*c.Hermite*l.Lagrange  This has cubic variation in xi1 and xi2 and
	linear variation in xi3.
2. c.Hermite*l.simplex(3)*l.simplex  This has cubic variation in xi1 and 2-D
	linear simplex variation for xi2 and xi3.
3. polygon(5,3)*l.Lagrange*polygon  This has linear variation in xi2 and a 2-D
	5-gon for xi1 and xi3.
==============================================================================*/
{
	char *basis_description_string,*end_basis_name,*location,*start_basis_name;
	int *basis_type;
	struct FE_basis *basis;

	ENTER(read_FE_basis);
	basis=(struct FE_basis *)NULL;
	if (input_file&&fe_region)
	{
		/* file input */
		/* read the basis type */
		if (IO_stream_read_string(input_file,"[^,]",&basis_description_string))
		{
			start_basis_name=basis_description_string;
			/* skip leading blanks */
			while (' '== *start_basis_name)
			{
				start_basis_name++;
			}
			/* remove trailing blanks */
			end_basis_name=start_basis_name+(strlen(start_basis_name)-1);
			while (' '== *end_basis_name)
			{
				end_basis_name--;
			}
			end_basis_name[1]='\0';
			if (NULL != (basis_type=FE_basis_string_to_type_array(basis_description_string)))
			{
				basis=FE_region_get_FE_basis_matching_basis_type(fe_region,basis_type);
				DEALLOCATE(basis_type);
			}
			else
			{
				location=IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Error converting basis description to type array.  %s",location);
				DEALLOCATE(location);
			}
			DEALLOCATE(basis_description_string);
		}
		else
		{
			location=IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"Error reading basis description from file.  %s",
				location);
			DEALLOCATE(location);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE, "read_FE_basis.  Invalid argument(s)");
	}
	LEAVE;

	return (basis);
} /* read_FE_basis */

static int read_FE_element_field(struct IO_stream *input_file, struct FE_region *fe_region,
	struct FE_element *element, struct FE_field **field_address)
/*******************************************************************************
LAST MODIFIED : 27 October 2004

DESCRIPTION :
Reads an element field from an <input_file>, adding it to the fields defined at
<element>. <field> is returned.
==============================================================================*/
{
	char *component_name, *global_to_element_map_string, *location,
		*modify_function_name, *rest_of_line, test_string[5];
	enum FE_field_type fe_field_type;
	FE_element_field_component_modify modify;
	int component_number, dimension, i, index, j,
		node_index, number_of_components, number_of_nodes, number_of_values,
		number_in_xi, return_code;
	struct FE_basis *basis;
	struct FE_field *field, *merged_fe_field;
	struct FE_element_field_component **component, **components;
	struct Standard_node_to_element_map *standard_node_map;

	ENTER(read_FE_element_field);
	return_code = 0;
	if (field_address)
	{
		*field_address = (struct FE_field *)NULL;
	}
	FE_mesh *fe_mesh = 0;
	if (input_file && fe_region && element && field_address &&
		(0 < (dimension = get_FE_element_dimension(element))) &&
		(fe_mesh = FE_region_find_FE_mesh_by_dimension(fe_region, dimension)))
	{
		if (NULL != (field = read_FE_field(input_file, fe_region)))
		{
			ACCESS(FE_field)(field);
			merged_fe_field = (struct FE_field *)NULL;
			number_of_components = get_FE_field_number_of_components(field);
			fe_field_type = get_FE_field_FE_field_type(field);
			return_code = 1;
			/* allocate memory for the components */
			component_name = (char *)NULL;
			if (ALLOCATE(components, struct FE_element_field_component *,
				number_of_components))
			{
				for (i = 0;i < number_of_components; i++)
				{
					components[i] = (struct FE_element_field_component *)NULL;
				}
			}
			if (components)
			{
				/* read the components */
				component_number = 0;
				number_of_values = 0;
				component = components;
				while (return_code && (component_number < number_of_components))
				{
					IO_stream_scan(input_file, " ");
					/* read the component name */
					if (component_name)
					{
						DEALLOCATE(component_name);
						component_name = (char *)NULL;
					}
					if (IO_stream_read_string(input_file, "[^.]", &component_name))
					{
						/* strip trailing blanks from component name */
						size_t i = strlen(component_name);
						while ((0 < i) && (isspace(component_name[i - 1])))
							--i;
						component_name[i] = '\0';
						return_code = (0 < i) && set_FE_field_component_name(field,
							component_number, component_name);
					}
					if (return_code)
					{
						/* component name is sufficient for non-GENERAL_FE_FIELD */
						if (GENERAL_FE_FIELD == fe_field_type)
						{
							IO_stream_scan(input_file, ". ");
							/* read the basis */
							if (NULL != (basis = read_FE_basis(input_file, fe_region)))
							{
								IO_stream_scan(input_file, ", ");
								/* read the modify function name */
								if (IO_stream_read_string(input_file, "[^,]", &modify_function_name))
								{
									/* determine the modify function */
									if (0 == strcmp("no modify", modify_function_name))
									{
										modify = (FE_element_field_component_modify)NULL;
									}
									else if (0 == strcmp("increasing in xi1",
										modify_function_name))
									{
										modify = theta_increasing_in_xi1;
									}
									else if (0 == strcmp("decreasing in xi1",
										modify_function_name))
									{
										modify = theta_decreasing_in_xi1;
									}
									else if (0 == strcmp("non-increasing in xi1",
										modify_function_name))
									{
										modify = theta_non_increasing_in_xi1;
									}
									else if (0 == strcmp("non-decreasing in xi1",
										modify_function_name))
									{
										modify = theta_non_decreasing_in_xi1;
									}
									else if (0 == strcmp("closest in xi1",
										modify_function_name))
									{
										modify = theta_closest_in_xi1;
									}
									else
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Invalid modify function from file.  %s",
											location);
										DEALLOCATE(location);
										return_code = 0;
									}
									if (return_code)
									{
										IO_stream_scan(input_file, ", ");
										/* read the global to element map type */
										if (IO_stream_read_string(input_file, "[^.]",
											&global_to_element_map_string))
										{
											IO_stream_scan(input_file, ". ");
											/* determine the global to element map type */
											if (0 == strcmp("standard node based",
												global_to_element_map_string))
											{
												/* standard node to element map */
												/* read the number of nodes */
												if ((1 == IO_stream_scan(input_file, "#Nodes=%d",
													&number_of_nodes)) && (0 < number_of_nodes))
												{
													if (NULL != (components[component_number] =
														CREATE(FE_element_field_component)(
															STANDARD_NODE_TO_ELEMENT_MAP,
															number_of_nodes, basis, modify)))
													{
														// set scale factor set only if there are scale factors in use
														bool noScaleFactors = true;
														for (i = 0; (i < number_of_nodes) && return_code; ++i)
														{
															if (2 != IO_stream_scan(input_file, "%d .  #Values=%d",
																&node_index, &number_of_values))
															{
																location = IO_stream_get_location_string(input_file);
																display_message(ERROR_MESSAGE,
																	"Invalid read of node index and #Values.  %s", location);
																DEALLOCATE(location);
																return_code = 0;
																break;
															}
															char *dofMappingTypeString = 0;
															// old EX files use indices into nodal values
															// new EX files use value labels e.g. value d/ds1(2) zero
															bool readValueIndices = false;
															if (IO_stream_read_string(input_file, "[^:]", &dofMappingTypeString))
															{
																if (1 == sscanf(dofMappingTypeString, " Value indice%1[s] ", test_string))
																	readValueIndices = true;
																else if (1 != sscanf(dofMappingTypeString, " Value label%1[s] ", test_string))
																	return_code = 0;
															}
															else
																return_code = 0;
															DEALLOCATE(dofMappingTypeString);
															if (!return_code)
															{
																location = IO_stream_get_location_string(input_file);
																display_message(ERROR_MESSAGE,
																	"Missing \" Value indices: \" or \" Value labels: \" token in file.  %s", location);
																DEALLOCATE(location);
																return_code = 0;
																break;
															}
															if (readValueIndices)
																standard_node_map = Standard_node_to_element_map_create_legacy(node_index - 1, number_of_values);
															else
																standard_node_map = Standard_node_to_element_map_create(node_index - 1, number_of_values);
															if (!standard_node_map)
															{
																location = IO_stream_get_location_string(input_file);
																display_message(ERROR_MESSAGE,
																	"Failed to create standard node to element map from file.  %s", location);
																DEALLOCATE(location);
																return_code = 0;
																break;
															}
															IO_stream_scan(input_file, ": ");
															if (readValueIndices)
															{
																for (j = 0; j < number_of_values; ++j)
																{
																	if (!((1 == IO_stream_scan(input_file, "%d", &index)) &&
																		Standard_node_to_element_map_set_nodal_value_index(
																			standard_node_map, j, index - 1)))
																	{
																		location = IO_stream_get_location_string(input_file);
																		display_message(ERROR_MESSAGE,
																			"Error reading nodal value index from file.  %s", location);
																		DEALLOCATE(location);
																		return_code = 0;
																		break;
																	}
																}
															}
															else // read value labels value type (versions) e.g. value d/ds1(2) d2/ds1ds2
															{
																if (!IO_stream_read_string(input_file, "[^\n\r]", &rest_of_line))
																{
																	location = IO_stream_get_location_string(input_file);
																	display_message(ERROR_MESSAGE, "Missing node value labels.  %s", location);
																	DEALLOCATE(location);
																	return_code = 0;
																	break;
																}
																char *label = rest_of_line;
																j = 0;
																for (j = 0; j < number_of_values; ++j)
																{
																	while (isspace(*label))
																		++label;
																	if ('\0' == *label)
																	{
																		location = IO_stream_get_location_string(input_file);
																		display_message(ERROR_MESSAGE, "Only %d out of %d value labels found.  %s",
																			j, number_of_values, location);
																		DEALLOCATE(location);
																		return_code = 0;
																		break;
																	}
																	const char *valueTypeString = label;
																	bool readVersion = false;
																	while (('\0' != *label) && (!isspace(*label)))
																	{
																		if ('(' == *label)
																		{
																			readVersion = true;
																			break;
																		}
																		++label;
																	}
																	if ('\0' != *label)
																	{
																		*label = '\0';
																		++label;
																	}
																	enum FE_nodal_value_type valueType = FE_NODAL_UNKNOWN;
																	if (!(STRING_TO_ENUMERATOR(FE_nodal_value_type)(valueTypeString, &valueType) &&
																		(FE_NODAL_UNKNOWN != valueType) &&
																		Standard_node_to_element_map_set_nodal_value_type(standard_node_map, j, valueType)))
																	{
																		// the special 'zero' label means parameter=0
																		// stored as default FE_NODAL_UNKNOWN type, so no need to set
																		if (0 != strcmp(valueTypeString, "zero"))
																		{
																			location = IO_stream_get_location_string(input_file);
																			display_message(ERROR_MESSAGE, "Invalid nodal value label '%s'.  %s",
																				valueTypeString, location);
																			DEALLOCATE(location);
																			return_code = 0;
																			break;
																		}
																	}
																	if (readVersion)
																	{
																		const char *versionString = label;
																		while (('\0' != *label) && (')' != *label))
																		{
																			++label;
																		}
																		if (')' == *label)
																		{
																			*label = '\0';
																			++label;
																			int version = 0;
																			if ((1 != sscanf(versionString, "%d", &version)) ||
																				!Standard_node_to_element_map_set_nodal_version(standard_node_map, j, version))
																			{
																				return_code = 0;
																			}
																		}
																		else
																			return_code = 0;
																		if (!return_code)
																		{
																			location = IO_stream_get_location_string(input_file);
																			display_message(ERROR_MESSAGE, "Invalid version number or format.  %s", location);
																			DEALLOCATE(location);
																			return_code = 0;
																			break;
																		}
																	}
																}
																if (return_code)
																{
																	// check for unexpected additional text
																	while ('\0' != *label)
																	{
																		if (!isspace(*label))
																		{
																			location = IO_stream_get_location_string(input_file);
																			display_message(ERROR_MESSAGE, "Unexpected text '%s' after labels.  %s",
																				label, location);
																			DEALLOCATE(location);
																			return_code = 0;
																			break;
																		}
																		++label;
																	}
																}
																DEALLOCATE(rest_of_line);
															}
															if (return_code)
															{
																/* read the scale factor indices */
																/* Use a %1[:] so that a successful read will return 1 */
																if (1 != IO_stream_scan(input_file," Scale factor indices%1[:] ", test_string))
																{
																	display_message(WARNING_MESSAGE,
																		"Truncated read of required \" Scale factor indices: \" token in element file.");
																}
																for (j = 0; (j < number_of_values) &&
																	return_code; j++)
																{
																	if ((1 == IO_stream_scan(input_file, "%d", &index)) &&
																		Standard_node_to_element_map_set_scale_factor_index(
																			standard_node_map, j, index - 1))
																	{
																		if (noScaleFactors && (index > 0))
																		{
																			noScaleFactors = false;
																			char *scale_factor_set_name = FE_basis_get_description_string(basis);
																			cmzn_mesh_scale_factor_set *scale_factor_set =
																				fe_mesh->find_scale_factor_set_by_name(scale_factor_set_name);
																			if (!scale_factor_set)
																			{
																				scale_factor_set = fe_mesh->create_scale_factor_set();
																				scale_factor_set->setName(scale_factor_set_name);
																			}
																			FE_element_field_component_set_scale_factor_set(
																				components[component_number], scale_factor_set);
																			cmzn_mesh_scale_factor_set::deaccess(scale_factor_set);
																			DEALLOCATE(scale_factor_set_name);
																		}
																	}
																	else
																	{
																		location = IO_stream_get_location_string(input_file);
																		display_message(ERROR_MESSAGE,
																			"Error reading scale factor index from "
																			"file.  %s",
																			location);
																		DEALLOCATE(location);
																		return_code = 0;
																	}
																}
																if (return_code)
																{
																	if (!FE_element_field_component_set_standard_node_map(
																		components[component_number],
																		/*node_number*/i, standard_node_map))
																	{
																		location = IO_stream_get_location_string(input_file);
																		display_message(ERROR_MESSAGE,
																			"read_FE_element_field.  Error setting "
																			"standard_node_to_element_map");
																		Standard_node_to_element_map_destroy(&standard_node_map);
																		DEALLOCATE(location);
																		return_code = 0;
																	}
																}
															}
														}
													}
													else
													{
														location = IO_stream_get_location_string(input_file);
														display_message(ERROR_MESSAGE,
															"read_FE_element_field.  "
															"Error creating component from file %s", location);
														DEALLOCATE(location);
														return_code = 0;
													}
												}
												else
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Error reading component number of nodes from file."
														"  %s", location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
											else if (0 == strcmp("general map based",
												global_to_element_map_string))
											{
												/* GENERAL_ELEMENT_MAP */
												// GRC: implement
												location = IO_stream_get_location_string(input_file);
												display_message(ERROR_MESSAGE,
													"Invalid global to element map type from file.  "
													"%s", location);
												DEALLOCATE(location);
												return_code = 0;
											}
											else if (0 == strcmp("grid based",
												global_to_element_map_string))
											{
												/* element grid based */
												if (NULL != (components[component_number] =
													CREATE(FE_element_field_component)(
														ELEMENT_GRID_MAP, 1, basis, modify)))
												{
													/* read number of divisions in each xi direction */
													i = 0;
													while (return_code && (i < dimension))
													{
														if ((2 == IO_stream_scan(input_file, "#xi%d = %d", &j,
															&number_in_xi)) && (j == i + 1))
														{
															if (FE_element_field_component_set_grid_map_number_in_xi(
																components[component_number], i, number_in_xi))
															{
																IO_stream_scan(input_file, " , ");
																i++;
															}
															else
															{
																location = IO_stream_get_location_string(input_file);
																display_message(ERROR_MESSAGE,
																	"Grid basis must be constant for #xi=0, or linear for #xi>0.  %s",
																	location);
																DEALLOCATE(location);
																return_code = 0;
															}
														}
														else
														{
															location = IO_stream_get_location_string(input_file);
															display_message(ERROR_MESSAGE,
																"Error reading #xi%d.  %s", i + 1,
																location);
															DEALLOCATE(location);
															return_code = 0;
														}
													}
													if (return_code)
													{
														FE_element_field_component_set_grid_map_value_index(
															components[component_number], 0);
													}
												}
												else
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"read_FE_element_field.  "
														"Error creating component from file");
													DEALLOCATE(location);
													return_code = 0;
												}
											}
											else
											{
												location = IO_stream_get_location_string(input_file);
												display_message(ERROR_MESSAGE,
													"Invalid global to element map type from file.  "
													"%s", location);
												DEALLOCATE(location);
												return_code = 0;
											}
											DEALLOCATE(global_to_element_map_string);
										}
										else
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"Error reading global to element map type from file.  "
												"%s", location);
											DEALLOCATE(location);
											return_code = 0;
										}
										DEALLOCATE(modify_function_name);
									}
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Error reading modify function name from file.  %s",
										location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"read_FE_element_field.  Invalid basis from file");
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						else
						{
							/* non GENERAL_FE_FIELD */
							/* check there is nothing on remainder of line */
							if (IO_stream_read_string(input_file, "[^\n\r]", &rest_of_line))
							{
								if (fuzzy_string_compare(rest_of_line, "."))
								{
									/* components are all NULL */
									return_code = 1;
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Unexpected text on field '%s' component '%s' line %d: %s",
										get_FE_field_name(field), component_name,
										location, rest_of_line);
									DEALLOCATE(location);
									return_code = 0;
								}
								DEALLOCATE(rest_of_line);
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Unexpected end of field '%s' component '%s' line %d",
									get_FE_field_name(field), component_name,
									location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						component_number++;
						component++;
					}
					else
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE,
							"Error reading component name from file.  %s",
							location);
						DEALLOCATE(location);
						return_code = 0;
					}
				}
			}
			else
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"read_FE_element_field.  Could not allocate component information");
				DEALLOCATE(location);
				return_code = 0;
			}
			if (return_code)
			{
				/* first try to retrieve matching field from fe_region */
				if (!(merged_fe_field = FE_region_merge_FE_field(fe_region, field)))
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,"read_FE_element_field.  "
						"Could not merge field '%s' into finite element region.  %s",
						get_FE_field_name(field), location);
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			if (return_code)
			{
				/* define merged_fe_field in element */
				if (define_FE_field_at_element(element, merged_fe_field, components))
				{
					*field_address = merged_fe_field;
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"read_FE_element_field.  Could not define field at element");
					DEALLOCATE(location);
					return_code = 0;
				}
			}
			if (component_name)
			{
				DEALLOCATE(component_name);
			}
			if (components)
			{
				for (i = 0; i < number_of_components; i++)
				{
					DESTROY(FE_element_field_component)(&(components[i]));
				}
				DEALLOCATE(components);
			}
			DEACCESS(FE_field)(&field);
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"read_FE_element_field.  Could not read field");
			DEALLOCATE(location);
		}
	}
	else
	{
		location = IO_stream_get_location_string(input_file);
		display_message(ERROR_MESSAGE,
			"read_FE_element_field.  Invalid argument(s)");
		DEALLOCATE(location);
	}
	LEAVE;

	return (return_code);
} /* read_FE_element_field */

static FE_element_template *read_FE_element_field_info(
	struct IO_stream *input_file, struct FE_region *fe_region,
	struct FE_element_shape *element_shape,
	struct FE_field_order_info **field_order_info)
/*******************************************************************************
LAST MODIFIED : 5 November 2004

DESCRIPTION :
Creates an element template with <element_shape> and the field information read
in from <input_file>. Note that the following header is required to return an
element template with no fields:
 #Scale factor sets=0
 #Nodes=0
 #Fields=0
It is also possible to have no scale factors and no nodes but a field - this
would be the case for grid-based fields.
Creates, fills in and returns field_order_info.
<*field_order_info> is reallocated here so should be either NULL or returned
from a previous call to this function.
==============================================================================*/
{
	char *location;
	int i,number_of_fields,number_of_nodes,
		number_of_scale_factor_sets, return_code;
	struct FE_field *field;
	FE_mesh *fe_mesh;

	ENTER(read_FE_element_field_info);
	FE_element_template *element_template = 0;
	const int dimension = get_FE_element_shape_dimension(element_shape);
	if (input_file && fe_region && (0 < dimension) && field_order_info &&
		(fe_mesh = FE_region_find_FE_mesh_by_dimension(fe_region, dimension)))
	{
		if (*field_order_info)
		{
			DESTROY(FE_field_order_info)(field_order_info);
			*field_order_info = (struct FE_field_order_info *)NULL;
		}
		/* create the blank element template */
		element_template = fe_mesh->create_FE_element_template(element_shape);
		if (element_template)
		{
			FE_element *template_element = element_template->get_template_element();
			return_code = 1;
			/* read in the scale factor information */
			if (!((1 == IO_stream_scan(input_file, "Scale factor sets=%d ",
				&number_of_scale_factor_sets)) && (0 <= number_of_scale_factor_sets)))
			{
				location = IO_stream_get_location_string(input_file);
				display_message(ERROR_MESSAGE,
					"Error reading #scale sets from file.  %s",
					location);
				DEALLOCATE(location);
				return_code = 0;
			}
			if (return_code)
			{
				int *numbers_in_scale_factor_sets = (int *)0;
				cmzn_mesh_scale_factor_set **scale_factor_set_identifiers = 0;
				/* note can have no scale factor sets */
				if ((0 == number_of_scale_factor_sets) || (
					ALLOCATE(numbers_in_scale_factor_sets, int,
						number_of_scale_factor_sets) &&
					ALLOCATE(scale_factor_set_identifiers, cmzn_mesh_scale_factor_set *,
						number_of_scale_factor_sets)))
				{
					for (i = 0; i < number_of_scale_factor_sets; ++i)
						scale_factor_set_identifiers[i] = 0;
					/* read in the scale factor set information */
					for (i = 0; (i < number_of_scale_factor_sets) && return_code; i++)
					{
						char *scale_factor_set_text = 0;
						if (IO_stream_read_string(input_file, "[^,]", &scale_factor_set_text))
						{
							char *scale_factor_set_name = remove_leading_trailing_blanks(scale_factor_set_text);
							DEALLOCATE(scale_factor_set_text);
							cmzn_mesh_scale_factor_set *scale_factor_set = fe_mesh->find_scale_factor_set_by_name(scale_factor_set_name);
							if (!scale_factor_set)
							{
								scale_factor_set = fe_mesh->create_scale_factor_set();
								scale_factor_set->setName(scale_factor_set_name);
							}
							DEALLOCATE(scale_factor_set_name);
							scale_factor_set_identifiers[i] = scale_factor_set;
							if (!((1 == IO_stream_scan(input_file, ", #Scale factors=%d ",
								&(numbers_in_scale_factor_sets[i]))) &&
								(0 < numbers_in_scale_factor_sets[i])))
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Error reading #Scale factors from file.  %s",
									location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Error reading scale factor set identifier (basis) from file.  "
								"%s", location);
							DEALLOCATE(location);
							return_code = 0;
						}
					}
					/* read in the node information */
					if (!((1 == IO_stream_scan(input_file, "#Nodes=%d ", &number_of_nodes)) &&
						(0 <= number_of_nodes)))
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE,
							"Error reading #Nodes from file.  %s",
							location);
						DEALLOCATE(location);
						return_code = 0;
					}
					/* read in the field information */
					if (!((1 == IO_stream_scan(input_file, "#Fields=%d ", &number_of_fields)) &&
						(0 <= number_of_fields)))
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE,
							"Error reading #fields from file.  %s",
							location);
						DEALLOCATE(location);
						return_code = 0;
					}
					if (return_code && (0 < number_of_fields))
					{
						if (!(set_FE_element_number_of_nodes(template_element, number_of_nodes) &&
							(CMZN_OK == set_FE_element_number_of_scale_factor_sets(template_element,
								number_of_scale_factor_sets, scale_factor_set_identifiers,
								numbers_in_scale_factor_sets))))
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE, "read_FE_element_field_info.  "
								"Error establishing element nodes and scale factor sets");
							DEALLOCATE(location);
							return_code = 0;
						}
					}
					if (return_code)
					{
						*field_order_info = CREATE(FE_field_order_info)();
						/* read in the element fields */
						for (i = 0; (i < number_of_fields) && return_code; i++)
						{
							field = (struct FE_field *)NULL;
							if (read_FE_element_field(input_file, fe_region, template_element, &field))
							{
								if (!add_FE_field_order_info_field(*field_order_info, field))
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"read_FE_element_field_info.  Could not add field to list");
									DEALLOCATE(location);
									return_code = 0;
								}
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"read_FE_element_field_info.  Could not read element field");
								DEALLOCATE(location);
								return_code = 0;
							}
						}
					}
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"read_FE_element_field_info.  Not enough memory");
					DEALLOCATE(location);
					return_code=0;
				}
				if (numbers_in_scale_factor_sets)
				{
					DEALLOCATE(numbers_in_scale_factor_sets);
				}
				if (scale_factor_set_identifiers)
				{
					for (int i = 0; i < number_of_scale_factor_sets; ++i)
						cmzn_mesh_scale_factor_set::deaccess(scale_factor_set_identifiers[i]);
					DEALLOCATE(scale_factor_set_identifiers);
				}
			}
			if (!return_code)
				cmzn::Deaccess(element_template);
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"read_FE_element_field_info.  Could not create element");
			DEALLOCATE(location);
			return_code = 0;
		}
	}
	else
	{
		location = IO_stream_get_location_string(input_file);
		display_message(ERROR_MESSAGE,
			"read_FE_element_field_info.  Invalid argument(s)");
		DEALLOCATE(location);
		return_code = 0;
	}
	LEAVE;

	return (element_template);
} /* read_FE_element_field_info */

/**
 * Returns an element from the element data in the input file.
 * If the element of that identifier already exists, it is returned but
 * parsed data is put into the element_template and the existingElement flag
 * is set which indicates that the caller must merge the element template.
 * If the element is newly created the new data is already in it.
 * This behaviour is interim while face/node/scale factor information is
 * moved from element objects into the mesh.
 * Format:
 * Element: # # #
 * Faces:
 * # # #
 * # # #
 * ...
 * Values:
 * # # # ...
 * Nodes:
 * # # # ...
 * Scale factors:
 * # # # ...
 * If the element_template has nodes, values or scale factors, those
 * sections are mandatory. Faces are optional.
 */
static struct FE_element *read_FE_element(struct IO_stream *input_file,
	FE_element_template *element_template, FE_mesh *fe_mesh,
	FE_nodeset *fe_nodeset, struct FE_field_order_info *field_order_info,
	bool& existingElement)
{
	char *location, test_string[5];
	enum Value_type value_type;
	FE_value scale_factor;
	int face_token_length, i, j, k, node_number, number_of_components,
		number_of_faces, number_of_fields, number_of_nodes, number_of_scale_factors,
		number_of_values, return_code, element_num, face_num, line_num;
	struct FE_field *field;
	struct FE_node *node;

	ENTER(read_FE_element);
	existingElement = false;
	FE_element *return_element = (struct FE_element *)NULL;
	if (input_file && element_template && fe_mesh && field_order_info)
	{
		/* read the element identifier */
		int element_identifier = -1;
		if (3 == IO_stream_scan(input_file, "lement :%d %d %d",
			&element_num, &face_num, &line_num))
		{
			if (element_num)
				element_identifier = element_num;
			else if (face_num)
				element_identifier = face_num;
			else /* line_num */
				element_identifier = line_num;
			return_code = 1;
		}
		else
		{
			location = IO_stream_get_location_string(input_file);
			display_message(ERROR_MESSAGE,
				"Error reading element identifier from file.  %s",
				location);
			DEALLOCATE(location);
			return_code = 0;
		}
		if (return_code)
		{
			return_element = fe_mesh->findElementByIdentifier(element_identifier);
			if (return_element)
			{
				DsLabelIndex index = get_FE_element_index(return_element);
				FE_element_shape *element_shape = fe_mesh->getElementShape(index);
				if (element_shape)
				{
					if (element_shape != element_template->get_element_shape())
					{
						if (FE_element_shape_is_unspecified(element_shape))
						{
							if (!fe_mesh->setElementShape(index, element_template->get_element_shape()))
							{
								display_message(ERROR_MESSAGE, "read_FE_element.  Could not set element shape for %d-D element %d",
									fe_mesh->getDimension(), element_identifier);
								return_element = 0;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE, "read_FE_element.  Inconsistent shape for %d-D element %d. %s",
								fe_mesh->getDimension(), element_identifier, location);
							DEALLOCATE(location);
							return_code = 0;
						}
					}
					if (return_element)
					{
						existingElement = true;
						ACCESS(FE_element)(return_element);
					}
				}
				else
				{
					display_message(ERROR_MESSAGE, "read_FE_element.  Missing element shape for %d-D element %d",
						fe_mesh->getDimension(), element_identifier);
					return_element = 0;
				}
			}
			else
			{
				return_element = fe_mesh->create_FE_element(element_identifier, element_template);
				if (!return_element)
				{
					display_message(ERROR_MESSAGE, "read_FE_element.  Could not create element");
					return_code = 0;
				}
			}
			if (return_element)
			{
				FE_element *element;
				FE_element_shape *element_shape;
				if (existingElement)
				{
					element = element_template->get_template_element();
					element_shape = element_template->get_element_shape();
				}
				else
				{
					element = return_element;
					element_shape = get_FE_element_shape(element);
				}
				number_of_faces = FE_element_shape_get_number_of_faces(element_shape);
				if (element_shape)
				{
					/* if face_token_length > 0, then faces being read */
					face_token_length = 0;
					IO_stream_scan(input_file, " Faces:%n", &face_token_length);
					if (0 < face_token_length)
					{
						FE_mesh *face_mesh = fe_mesh->getFaceMesh();
						if ((!face_mesh) && (0 < number_of_faces))
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE, "read_FE_element.  Faces token without face mesh of dimension %d.  %s",
								fe_mesh->getDimension() - 1, location);
							DEALLOCATE(location);
							return_code = 0;
						}
						int face_identifier;
						FE_element *face_element;
						for (i = 0; (i < number_of_faces) && return_code; i++)
						{
							/* file input */
							if (3 == IO_stream_scan(input_file, "%d %d %d",
								&element_num, &face_num, &line_num))
							{
								if (element_num)
									face_identifier = element_num;
								else if (face_num)
									face_identifier = face_num;
								else /* line_num */
									face_identifier = line_num;
								return_code =1;
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Error reading face identifier from file.  %s",
									location);
								DEALLOCATE(location);
								return_code = 0;
							}
							if (return_code)
							{
								/* face number of 0 means no face. GRC problem since 0 is legal identifier */
								if (0 != face_identifier)
								{
									face_element = face_mesh->findElementByIdentifier(face_identifier);
									if (!face_element)
									{
										// create a face of the expected shape
										FE_element_shape *face_shape = get_FE_element_shape_of_face(element_shape, i, fe_mesh->get_FE_region());
										face_element = face_mesh->get_or_create_FE_element_with_identifier(face_identifier, face_shape);
									}
									face_element = face_mesh->findElementByIdentifier(face_identifier);
									if (face_element)
									{
										// faces go directly in return element; template does not hold them
										if (CMZN_OK != fe_mesh->setElementFace(
											get_FE_element_index(return_element), i, get_FE_element_index(face_element)))
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,"read_FE_element.  "
												"Could not set face %d of %d-D element %d.  %s", i + 1, fe_mesh->getDimension(),
												element_identifier, location);
											DEALLOCATE(location);
											return_code = 0;
										}
									}
									else
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE, "read_FE_element.  Could not find %d-D face element %d.  %s",
											face_mesh->getDimension(), face_identifier, location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
							}
						}
					}
				}
				else
				{
					location = IO_stream_get_location_string(input_file);
					display_message(ERROR_MESSAGE,
						"read_FE_element.  Could not get shape and number of faces of %d-D element %d.  %s",
						fe_mesh->getDimension(), element_identifier, location);
					DEALLOCATE(location);
					return_code = 0;
				}
				/* check whether element has any grid values */
				if (return_code && FE_element_has_values_storage(element))
				{
					/* read the values */
					/* Use a %1[:] so that a successful read will return 1 */
					if (1 != IO_stream_scan(input_file," Values %1[:] ", test_string))
					{
						display_message(WARNING_MESSAGE,
							"Truncated read of required \" Values :\" token in element file.");
					}
					number_of_fields =
						get_FE_field_order_info_number_of_fields(field_order_info);
					for (i = 0; (i < number_of_fields) && return_code; i++)
					{
						if ((field = get_FE_field_order_info_field(field_order_info, i)))
						{
							number_of_components = get_FE_field_number_of_components(field);
							int allocated_number_of_values = 0;
							value_type = get_FE_field_value_type(field);
							switch (value_type)
							{
								case FE_VALUE_VALUE:
								{
									FE_value *values = NULL;
									for (j = 0; (j < number_of_components) && return_code; j++)
									{
										number_of_values = get_FE_element_field_component_number_of_grid_values(element, field, j);
										if (0 < number_of_values)
										{
											if (number_of_values > allocated_number_of_values)
											{
												FE_value *temp_values;
												if (REALLOCATE(temp_values, values, FE_value, number_of_values))
												{
													values = temp_values;
													allocated_number_of_values = number_of_values;
												}
												else
												{
													return_code = 0;
												}
											}
											for (k = 0; (k < number_of_values) && return_code; k++)
											{
												if (1 != IO_stream_scan(input_file, FE_VALUE_INPUT_STRING,
													&(values[k])))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Error reading grid FE_value value from file.  %s",
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
												if (!finite(values[k]))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Infinity or NAN element value read from element file.  %s",
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
											if (return_code)
											{
												if (!set_FE_element_field_component_grid_FE_value_values(
													element, field, /*component_number*/j, values))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"read_FE_element.  Could not set grid FE_value values");
													DEALLOCATE(location);
												}
											}
										}
									}
									DEALLOCATE(values);
								} break;
								case INT_VALUE:
								{
									int *values = NULL;
									for (j = 0; (j < number_of_components) && return_code; j++)
									{
										number_of_values = get_FE_element_field_component_number_of_grid_values(element, field, j);
										if (0 < number_of_values)
										{
											if (number_of_values > allocated_number_of_values)
											{
												int *temp_values;
												if (REALLOCATE(temp_values, values, int, number_of_values))
												{
													values = temp_values;
													allocated_number_of_values = number_of_values;
												}
												else
												{
													return_code = 0;
												}
											}
											for (k = 0; (k < number_of_values) && return_code; k++)
											{
												if (1 != IO_stream_scan(input_file, "%d", &(values[k])))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"Error reading grid int value from file.  %s",
														location);
													DEALLOCATE(location);
													return_code = 0;
												}
											}
											if (return_code)
											{
												if (!set_FE_element_field_component_grid_int_values(
													element, field, /*component_number*/j, values))
												{
													location = IO_stream_get_location_string(input_file);
													display_message(ERROR_MESSAGE,
														"read_FE_element.  Could not set grid int values");
													DEALLOCATE(location);
												}
											}
										}
									}
									DEALLOCATE(values);
								} break;
								default:
								{
									/* no element values for other types */
								} break;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE, "Invalid field #%d.  %s",
								i + 1, location);
							DEALLOCATE(location);
							return_code = 0;
						}
					}
				}
				if (return_code)
				{
					if (get_FE_element_number_of_nodes(element, &number_of_nodes))
					{
						if (0 < number_of_nodes)
						{
							/* read the nodes */
							/* Use a %1[:] so that a successful read will return 1 */
							if (1 != IO_stream_scan(input_file," Nodes%1[:]", test_string))
							{
								location = IO_stream_get_location_string(input_file);
								display_message(WARNING_MESSAGE,
									"Truncated read of required \" Nodes:\" token in element file.  %s", location);
								DEALLOCATE(location);
							}
							for (i = 0; (i < number_of_nodes) && return_code; i++)
							{
								if (1 == IO_stream_scan(input_file, "%d", &node_number))
								{
									/* get or create node with node_number */
									if (NULL != (node = fe_nodeset->get_or_create_FE_node_with_identifier(node_number)))
									{
										if (!set_FE_element_node(element, i, node))
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"read_FE_element.  Could not set node");
											DEALLOCATE(location);
											return_code = 0;
										}
									}
									else
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"read_FE_element.  Could not get or create node");
										DEALLOCATE(location);
										return_code = 0;
									}
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Error reading node number from file.  %s",
										location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
						}
					}
					else
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE,
							"read_FE_element.  Could not get number of nodes for %d-D element %d",
							fe_mesh->getDimension(), element_identifier);
						DEALLOCATE(location);
						return_code = 0;
					}
				}
				if (return_code)
				{
					if (get_FE_element_number_of_scale_factors(element,
						&number_of_scale_factors))
					{
						if (0 < number_of_scale_factors)
						{
							/*???RC scale_factors array in element_info should be private */
							/* read the scale factors */
							/* Use a %1[:] so that a successful read will return 1 */
							if (1 != IO_stream_scan(input_file," Scale factors%1[:]", test_string))
							{
								display_message(WARNING_MESSAGE,
									"Truncated read of required \" Scale factors:\" token in element file.");
							}
							for (i = 0; (i < number_of_scale_factors) && return_code; i++)
							{
								if (1 == IO_stream_scan(input_file,FE_VALUE_INPUT_STRING,
									&scale_factor))
								{
									if (finite(scale_factor))
									{
										if (!set_FE_element_scale_factor(element, i, scale_factor))
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"Error setting scale factor.  %s",
												location);
											DEALLOCATE(location);
											return_code = 0;
										}
									}
									else
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Infinity or NAN scale factor read from element file.  "
											"%s", location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Error reading scale factor from file.  %s",
										location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
						}
					}
					else
					{
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE, "read_FE_element.  "
							"Could not get number of scale factors for %d-D element %d",
							fe_mesh->getDimension(), element_identifier);
						DEALLOCATE(location);
						return_code = 0;
					}
				}
				if (!return_code)
					DEACCESS(FE_element)(&element);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,"read_FE_element.  Invalid argument(s)");
	}
	LEAVE;

	return (return_element);
} /* read_FE_element */

/***************************************************************************//**
 * Reads region, group, field, node and element field data in EX format into
 * the supplied region.
 *
 * It is good practice to read the file into a newly created region and check it
 * can be merged into the global region before doing so, otherwise failure to
 * merge incompatible data will leave the global region in a compromised state.
 * Where objects not within the file are referred to, such as nodes in a pure
 * exelem file or elements in embedded element:xi fields, local objects of the
 * correct type are made as placeholders and all checking is left to the merge.
 * Embedding elements are located by a region path starting at the root region
 * in the file; if no path is supplied they are placed in the root region.
 * If objects are repeated in the file, they are merged correctly.
 *
 * @param root_region  The region into which data is read which will be the
 *   root of a region hierarchy when sub-regions and groups are read in.
 * @param input_file  The stream from which EX data is read.
 * @param time_index  If non NULL then the values in this read are assumed to
 *   belong to the specified time.  This means that the nodal values will be
 *   read into an array and the correct index put into the corresponding time
 *   array.
 * @param use_data  Flag, if set indicates nodes are to be read into separate
 *   data regions, otherwise nodes and elements are read normally.
 * @return  1 on success, 0 on failure.
 */
static int read_exregion_file_private(struct cmzn_region *root_region,
	struct IO_stream *input_file, struct FE_import_time_index *time_index,
	int use_data)
{
	char first_character_in_token, *location,
		*temp_string, test_string[5];
	int input_result, return_code;
	struct FE_element_shape *element_shape;
	struct FE_field_order_info *field_order_info;
	struct FE_node *template_node;

	ENTER(read_exregion_file);
	return_code = 0;
	if (root_region && input_file)
	{
		int use_data_meta_flag = use_data;
		cmzn_region_begin_hierarchical_change(root_region);
		/* region is the same as read_region if reading into a true region,
		 * otherwise it is the parent region of read_region group */
		cmzn_region_id region = cmzn_region_access(root_region);
		cmzn_field_group_id group = 0;
		cmzn_nodeset_group_id nodeset_group = 0;
		cmzn_mesh_group_id mesh_group = 0;
		struct FE_region *fe_region = 0;
		FE_mesh *fe_mesh = 0;
		FE_nodeset *fe_nodeset = 0;
		field_order_info = (struct FE_field_order_info *)NULL;
		template_node = (struct FE_node *)NULL;
		FE_element_template *element_template = 0;
		element_shape = (struct FE_element_shape *)NULL;
		input_result = 1;
		return_code = 1;
		while (return_code && (1 == input_result))
		{
			/* get first character in next token */
			IO_stream_scan(input_file, " ");
			/*???DB.  On the alphas input_result is 0 at the end of file when the
				IO_stream_scans are combined " %c" ? */
			input_result = IO_stream_scan(input_file, "%c", &first_character_in_token);
			if (1 == input_result)
			{
				switch (first_character_in_token)
				{
					case 'R': /* Region : </path> */
					case 'G': /* Group name : <name> */
					{
						use_data_meta_flag = use_data;
						if (group)
						{
							cmzn_field_group_destroy(&group);
						}
						fe_region = (struct FE_region *)NULL;
						fe_mesh = 0;
						fe_nodeset = 0;
						/* Use a %1[:] so that a successful read will return 1 */
						int valid_token = 0;
						if ('R' == first_character_in_token)
						{
							valid_token = IO_stream_scan(input_file,"egion %1[:]", test_string);
						}
						else
						{
							valid_token = IO_stream_scan(input_file,"roup name %1[:]", test_string);
						}
						if (1 != valid_token)
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Truncated \'Region :\' or \'Group name :\' token in EX file.  %s", location);
							DEALLOCATE(location);
							return_code = 0;
						}
						char *rest_of_line = (char *)NULL;
						char *region_path = (char *)NULL;
						if (return_code)
						{
							/* read the region path */
							if (IO_stream_read_string(input_file, "[^\n\r]", &rest_of_line))
							{
								region_path = rest_of_line;
								/* trim leading and trailing white space */
								while ((' ' == *region_path) || ('\t' == *region_path))
								{
									region_path++;
								}
								char *last_character = region_path+(strlen(region_path)-1);
								while ((last_character > region_path) && ((' ' == *last_character) || ('\t' == *last_character)))
								{
									last_character--;
								}
								*(last_character + 1)='\0';
								return_code = 1;
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"Error reading region path or group name from file.  %s", location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						/* get or create region with path, or group with name */
						if (return_code)
						{
							if ('R' == first_character_in_token)
							{
								cmzn_region_destroy(&region);
								if (region_path && (CMZN_REGION_PATH_SEPARATOR_CHAR == region_path[0]))
								{
									region = cmzn_region_find_subregion_at_path(root_region, region_path);
									if (!region)
									{
										region = cmzn_region_create_subregion(root_region, region_path);
									}
									if (!region)
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Could not create region \'%s\'.  %s", region_path, location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Missing \'%c\' at start of region path \'%s\'.  %s",
										CMZN_REGION_PATH_SEPARATOR_CHAR, region_path, location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
							else
							{
								cmzn_fieldmodule_id field_module = cmzn_region_get_fieldmodule(region);
								cmzn_field_id group_field = cmzn_fieldmodule_find_field_by_name(field_module, region_path);
								if (group_field)
								{
									group = cmzn_field_cast_group(group_field);
									if (!group)
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Could not create group \'%s\' as name in use.  %s", region_path, location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
								else
								{
									group_field = cmzn_fieldmodule_create_field_group(field_module);
									cmzn_field_set_managed(group_field, true);
									if (cmzn_field_set_name(group_field, region_path))
									{
										group = cmzn_field_cast_group(group_field);
									}
									else
									{
										location = IO_stream_get_location_string(input_file);
										display_message(ERROR_MESSAGE,
											"Could not create group \'%s\'.  %s", region_path, location);
										DEALLOCATE(location);
										return_code = 0;
									}
								}
								cmzn_field_destroy(&group_field);
								cmzn_fieldmodule_destroy(&field_module);
							}
						}
						region_path = (char *)NULL;
						DEALLOCATE(rest_of_line);

						if (template_node)
						{
							DEACCESS(FE_node)(&template_node);
						}
						cmzn::Deaccess(element_template);
						/* default to reading nodes after region / group token */
						if (element_shape)
						{
							DEACCESS(FE_element_shape)(&element_shape);
						}
						if (field_order_info)
						{
							DESTROY(FE_field_order_info)(&field_order_info);
						}
						if (region)
						{
							fe_region = cmzn_region_get_FE_region(region);
							fe_nodeset = FE_region_find_FE_nodeset_by_field_domain_type(fe_region,
								use_data_meta_flag ? CMZN_FIELD_DOMAIN_TYPE_DATAPOINTS : CMZN_FIELD_DOMAIN_TYPE_NODES);
							template_node = ACCESS(FE_node)(CREATE(FE_node)(1, fe_nodeset, (struct FE_node *)NULL));
							field_order_info = CREATE(FE_field_order_info)();
						}
						cmzn_nodeset_group_destroy(&nodeset_group);
					} break;
					case 'S': /* Shape */
					{
						if (fe_region)
						{
							if (element_shape)
							{
								DEACCESS(FE_element_shape)(&element_shape);
								element_shape = (struct FE_element_shape *)NULL;
							}
							/* clear node and element field information */
							if (template_node)
							{
								DEACCESS(FE_node)(&template_node);
							}
							cmzn::Deaccess(element_template);
							/* read element shape information */
							if (read_FE_element_shape(input_file, &element_shape, fe_region))
							{
								/* nodes have 0 dimensions and thus no element_shape */
								if (element_shape)
								{
									ACCESS(FE_element_shape)(element_shape);
									/* create the initial template element for no fields */
									const int dimension = get_FE_element_shape_dimension(element_shape);
									fe_mesh = FE_region_find_FE_mesh_by_dimension(fe_region, dimension);
									if (fe_mesh)
										element_template = fe_mesh->create_FE_element_template(element_shape);
									use_data_meta_flag = 0;
									// elements have nodes not datapoints:
									fe_nodeset = FE_region_find_FE_nodeset_by_field_domain_type(fe_region,
										CMZN_FIELD_DOMAIN_TYPE_NODES);
									if (!element_template)
									{
										display_message(ERROR_MESSAGE,
											"read_exregion_file_private.  Error creating element template");
										return_code = 0;
									}
								}
								else
								{
									/* create the initial template node for no fields */
									fe_nodeset = FE_region_find_FE_nodeset_by_field_domain_type(fe_region,
										use_data_meta_flag ? CMZN_FIELD_DOMAIN_TYPE_DATAPOINTS : CMZN_FIELD_DOMAIN_TYPE_NODES);
									template_node = ACCESS(FE_node)(CREATE(FE_node)(1, fe_nodeset, (struct FE_node *)NULL));
									ACCESS(FE_node)(template_node);
								}
								/* clear field_order_info */
								if (field_order_info)
								{
									DESTROY(FE_field_order_info)(&field_order_info);
								}
								field_order_info = CREATE(FE_field_order_info)();
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"read_exregion_file_private.  Error reading element shape");
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Region/Group not set before Shape token in file.  %s",
								location);
							DEALLOCATE(location);
							return_code = 0;
						}
						cmzn_mesh_group_destroy(&mesh_group);
					} break;
					case '!': /* !# directive, otherwise ! Comment ignored to end of line */
					{
						char *comment = NULL;
						// directive !#nodeset nodes|datapoints
						// sets which nodeset to read nodes into
						if (1 == IO_stream_scan(input_file,"#nodese%1[t] ",test_string))
						{
							char *nodesetName = 0;
							if (IO_stream_read_string(input_file, "[^,\n\r]", &nodesetName))
							{
								// remove trailing whitespace off name
								size_t i = strlen(nodesetName);
								while ((0 < i) && (isspace(nodesetName[i - 1])))
									--i;
								nodesetName[i] = '\0';
								if (0 == strcmp("datapoints", nodesetName))
									use_data_meta_flag = 1;
								else if (0 == strcmp("nodes", nodesetName))
									use_data_meta_flag = 0;
								else
								{
									display_message(WARNING_MESSAGE, "Unrecognised nodeset name '%s', assuming %s",
										nodesetName, use_data ? "datapoints" : "nodes");
									use_data_meta_flag = use_data;
								}
								DEALLOCATE(nodesetName);
								if (region)
								{
									fe_region = cmzn_region_get_FE_region(region);
									fe_nodeset = FE_region_find_FE_nodeset_by_field_domain_type(fe_region,
										use_data_meta_flag ? CMZN_FIELD_DOMAIN_TYPE_DATAPOINTS : CMZN_FIELD_DOMAIN_TYPE_NODES);
									if (template_node)
										DEACCESS(FE_node)(&template_node);
									template_node = ACCESS(FE_node)(CREATE(FE_node)(1, fe_nodeset, (struct FE_node *)NULL));
									if (field_order_info)
										DESTROY(FE_field_order_info)(&field_order_info);
									field_order_info = CREATE(FE_field_order_info)();
									cmzn_nodeset_group_destroy(&nodeset_group);
								}
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE, "Error reading nodeset name at location %s", location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						// ignore to end of line for comment AND directive, in case we extend directive
						IO_stream_read_string(input_file, "[^\n\r]", &comment);
						DEALLOCATE(comment);
					} break;
					case '#': /* #Scale factor sets, #Nodes, or #Fields */
					{
						if (fe_region)
						{
							/* clear node and element field information */
							if (template_node)
							{
								DEACCESS(FE_node)(&template_node);
							}
							cmzn::Deaccess(element_template);
							if (element_shape)
							{
								/* read new element field information and field_order_info */
								if (NULL == (element_template = read_FE_element_field_info(input_file,
									fe_region, element_shape, &field_order_info)))
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Error reading element field information.  %s", location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
							else
							{
								/* read new node field information and field_order_info */
								if (NULL != (template_node = read_FE_node_field_info(input_file,
									fe_nodeset, &field_order_info, time_index)))
								{
									ACCESS(FE_node)(template_node);
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"Error reading node field information.  %s", location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Region/Group not set before field header tokens in file.  %s",
								location);
							DEALLOCATE(location);
							return_code = 0;
						}
					} break;
					case 'N': /* Node */
					{
						if (fe_region)
						{
							/* ensure we have node field information */
							if (template_node)
							{
								FE_node *tmp_node = read_FE_node(input_file, template_node, fe_nodeset,
									root_region, region, field_order_info, time_index);
								if (tmp_node)
								{
									ACCESS(FE_node)(tmp_node);
									FE_node *node = fe_nodeset->merge_FE_node(tmp_node);
									if (node)
									{
										if (group && (!nodeset_group))
										{
											cmzn_fieldmodule_id field_module = cmzn_region_get_fieldmodule(region);
											cmzn_nodeset_id nodeset = cmzn_fieldmodule_find_nodeset_by_field_domain_type(field_module,
												use_data_meta_flag ? CMZN_FIELD_DOMAIN_TYPE_DATAPOINTS : CMZN_FIELD_DOMAIN_TYPE_NODES);
											cmzn_field_node_group_id node_group = cmzn_field_group_get_field_node_group(group, nodeset);
											if (!node_group)
											{
												node_group = cmzn_field_group_create_field_node_group(group, nodeset);
											}
											nodeset_group = cmzn_field_node_group_get_nodeset_group(node_group);
											cmzn_field_node_group_destroy(&node_group);
											cmzn_nodeset_destroy(&nodeset);
											cmzn_fieldmodule_destroy(&field_module);
										}
										if (nodeset_group)
										{
											cmzn_nodeset_group_add_node(nodeset_group, node);
										}
									}
									else
									{
										display_message(ERROR_MESSAGE,
											"read_exregion_file.  Could not merge node into region");
										return_code = 0;
									}
									DEACCESS(FE_node)(&tmp_node);
								}
								else
								{
									display_message(ERROR_MESSAGE,
										"read_exregion_file.  Error reading node");
									return_code = 0;
								}
							}
							else
							{
								display_message(ERROR_MESSAGE,
									"read_exregion_file.  No current node field info for node");
								return_code = 0;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Region/Group not set before Node token in file.  %s",
								location);
							DEALLOCATE(location);
							return_code = 0;
						}
					} break;
					case 'E': /* Element */
					{
						if (fe_region)
						{
							/* ensure we have element field information */
							if (element_template)
							{
								bool existingElement = false;
								FE_element *element = read_FE_element(input_file, element_template,
									fe_mesh, fe_nodeset, field_order_info, existingElement);
								if (element)
								{
									if (existingElement)
									{
										int result = fe_mesh->merge_FE_element_template(element, element_template);
										if (result != CMZN_OK)
										{
											location = IO_stream_get_location_string(input_file);
											display_message(ERROR_MESSAGE,
												"read_exregion_file.  Failed to merge into existing element.  %s",
												location);
											DEALLOCATE(location);
											return_code = 0;
											DEACCESS(FE_element)(&element);
										}
									}
									if (group && (!mesh_group))
									{
										cmzn_fieldmodule_id field_module = cmzn_region_get_fieldmodule(region);
										cmzn_mesh_id mesh = cmzn_fieldmodule_find_mesh_by_dimension(field_module,
											fe_mesh->getDimension());
										cmzn_field_element_group_id element_group = cmzn_field_group_get_field_element_group(group, mesh);
										if (!element_group)
										{
											element_group = cmzn_field_group_create_field_element_group(group, mesh);
										}
										mesh_group = cmzn_field_element_group_get_mesh_group(element_group);
										cmzn_field_element_group_destroy(&element_group);
										cmzn_mesh_destroy(&mesh);
										cmzn_fieldmodule_destroy(&field_module);
									}
									if (mesh_group)
									{
										cmzn_mesh_group_add_element(mesh_group, element);
									}
									DEACCESS(FE_element)(&element);
								}
								else
								{
									location = IO_stream_get_location_string(input_file);
									display_message(ERROR_MESSAGE,
										"read_exregion_file.  Error reading element.  %s",
										location);
									DEALLOCATE(location);
									return_code = 0;
								}
							}
							else
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,"read_exregion_file_private.  "
									"No current element field info for element.  %s",
									location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"Region/Group not set before Element token in file.  %s",
								location);
							DEALLOCATE(location);
							return_code = 0;
						}
					} break;
					case 'V': /* Values */
					{
						/* read in field values */
						if ((template_node || element_template) && field_order_info)
						{
							if (!read_FE_field_values(input_file, fe_region, root_region,
								region, field_order_info))
							{
								location = IO_stream_get_location_string(input_file);
								display_message(ERROR_MESSAGE,
									"read_exregion_file.  Error reading field values.  %s",
									location);
								DEALLOCATE(location);
								return_code = 0;
							}
						}
						else
						{
							location = IO_stream_get_location_string(input_file);
							display_message(ERROR_MESSAGE,
								"read_exregion_file.  Unexpected V[alues] token in file.  %s",
								location);
							DEALLOCATE(location);
							return_code = 0;
						}
					} break;
					default:
					{
						temp_string = (char *)NULL;
						IO_stream_read_string(input_file, "[^\n\r]", &temp_string);
						location = IO_stream_get_location_string(input_file);
						display_message(ERROR_MESSAGE,
							"Invalid text \'%c%s\' in EX node/element file.  %s",
							first_character_in_token, temp_string ? temp_string : "", location);
						DEALLOCATE(temp_string);
						DEALLOCATE(location);
						return_code = 0;
					} break;
				} /* switch (first_character_in_token) */
			} /* if (1 == input_result) */
		} /* while (return_code && (1 == input_result)) */
		cmzn_nodeset_group_destroy(&nodeset_group);
		cmzn_mesh_group_destroy(&mesh_group);
		cmzn_field_group_destroy(&group);
		if (template_node)
		{
			DEACCESS(FE_node)(&template_node);
		}
		cmzn::Deaccess(element_template);
		if (element_shape)
		{
			DEACCESS(FE_element_shape)(&element_shape);
		}
		if (field_order_info)
		{
			DESTROY(FE_field_order_info)(&field_order_info);
		}
		DEACCESS(cmzn_region)(&region);
		cmzn_region_end_hierarchical_change(root_region);
	}
	else
	{
		location = IO_stream_get_location_string(input_file);
		display_message(ERROR_MESSAGE, "read_exregion_file.  Invalid argument(s)");
		DEALLOCATE(location);
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* read_exregion_file */

/*
Global functions
----------------
*/

int read_exregion_file(struct cmzn_region *region,
	struct IO_stream *input_file, struct FE_import_time_index *time_index)
{
	return read_exregion_file_private(region, input_file, time_index,
		/*use_data*/0);
}

int read_exdata_file(struct cmzn_region *region,
	struct IO_stream *input_file, struct FE_import_time_index *time_index)
{
	return read_exregion_file_private(region, input_file, time_index,
		/*use_data*/1);
}

int read_exregion_file_of_name(struct cmzn_region *region, const char *file_name,
	struct IO_stream_package *io_stream_package,
	struct FE_import_time_index *time_index, int useData,
	enum cmzn_streaminformation_data_compression_type data_compression_type)
/*******************************************************************************
LAST MODIFIED : 23 May 2008

DESCRIPTION :
Version of read_exregion_file that opens and closes file <file_name>.
Up to the calling function to check and merge the returned cmiss_region.
==============================================================================*/
{
	int return_code;
	struct IO_stream *input_file;

	ENTER(read_exregion_file_of_name);
	return_code = 0;
	if (region && file_name)
	{
		input_file = CREATE(IO_stream)(io_stream_package);
		if (IO_stream_open_for_read_compression_specified(input_file, file_name, data_compression_type))
		{
			return_code = read_exregion_file_private(region, input_file, time_index,
				useData);
			IO_stream_close(input_file);
		}
		else
		{
			display_message(ERROR_MESSAGE, "Could not open exregion file: %s",
				file_name);
		}
		DESTROY(IO_stream)(&input_file);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"read_exregion_file_of_name.  Invalid argument(s)");
	}
	LEAVE;

	return (return_code);
} /* read_exregion_file_of_name */
