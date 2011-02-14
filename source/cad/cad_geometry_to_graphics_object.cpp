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

extern "C" {
#include "computed_field/computed_field.h"
#include "general/debug.h"
#include "graphics/auxiliary_graphics_types.h"
#include "graphics/graphics_object.h"
}

#include "graphics/graphics_object.hpp"

#include "cad/cad_geometry_to_graphics_object.h"
#include "cad/computed_field_cad_topology.h"
#include "cad/field_location.hpp"
#include "cad/element_identifier.h"

struct GT_surface *create_surface_from_cad_shape(Cmiss_field_cad_topology_id cad_topology,
	struct Computed_field *coordinate_field, struct Computed_field *data_field, Render_type render_type, Cmiss_cad_surface_identifier surface_index)
{
	struct GT_surface *surface = 0;

	int number_of_derivatives = 2;
	int number_of_components = Computed_field_get_number_of_components(coordinate_field);
	FE_value *values = 0;
	FE_value *derivatives = 0;
	FE_value *data_values = 0;
	ALLOCATE(values, FE_value, number_of_components);
	ALLOCATE(derivatives, FE_value, number_of_derivatives * number_of_components);
	//int surface_count = Cmiss_field_cad_topology_get_surface_count(cad_topology);
	//int surface_point_count_total = 0;
	//for (int i = 0; i < surface_count; i++)
	//{
	//	Cmiss_cad_surface_identifier identifier = i;
	//	printf( "Surface index for surface count %d\n", identifier);
	//	surface_point_count_total += Cmiss_field_cad_topology_get_surface_point_count(cad_topology, identifier);
	//}
	int surface_point_count = Cmiss_field_cad_topology_get_surface_point_count(cad_topology, surface_index);

	Triple *points = 0;
	Triple *normals = 0;
	GTDATA *data = 0;
	int num_data_field_components = 0;
	ALLOCATE(points, Triple, surface_point_count);
	ALLOCATE(normals, Triple, surface_point_count);
	if (data_field && (Computed_field_get_number_of_components(data_field)==3))
	{
		num_data_field_components = 3;
		ALLOCATE(data, GTDATA, 3 * surface_point_count);
		ALLOCATE(data_values, FE_value, num_data_field_components);
	}

	if ( points && normals && values && derivatives &&
		(surface = CREATE(GT_surface)(g_SH_DISCONTINUOUS, render_type, g_TRIANGLE, surface_point_count / 3,
		/*npp*/ 3, points, normals, /*tangent*/ (Triple *)NULL, /*texture*/ (Triple *)NULL,
		num_data_field_components, data)))
	{
		
		GT_surface_set_integer_identifier(surface, surface_index);
		int points_index = 0;
		int return_code = 1;
		DEBUG_PRINT("creating %d surfaces\n", surface_index);
		//for ( int i = 0; i < surface_count && return_code; i++ )
		//{
			//Cmiss_cad_surface_identifier identifier = i;
			int point_count = Cmiss_field_cad_topology_get_surface_point_count(cad_topology, surface_index);
			DEBUG_PRINT("  setting id: %d (surface point count %d)\n", surface_index, point_count);
			DEBUG_PRINT( "  surface %d has %d points\n", surface_index+1, point_count );
			for ( int j = 0; j < point_count && return_code; j++ )
			{
				double u = 0.0, v = 0.0;
				Cmiss_cad_surface_point_identifier point_identifier = j;
				return_code = Cmiss_field_cad_topology_get_surface_point_uv_coordinates(cad_topology, surface_index, point_identifier, u, v);
				if (return_code)
				{
					Field_cad_geometry_surface_location loc(cad_topology, surface_index, u, v, 0.0, 2);
					return_code = Computed_field_evaluate_at_location(coordinate_field, loc, values, derivatives);
					if (return_code)
					{
						if (data_field && num_data_field_components)
						{
							return_code = Computed_field_evaluate_at_location(data_field, loc, data_values);
							data[num_data_field_components * points_index + 0] = static_cast<GTDATA>(data_values[0]);
							data[num_data_field_components * points_index + 1] = static_cast<GTDATA>(data_values[1]);
							data[num_data_field_components * points_index + 2] = static_cast<GTDATA>(data_values[2]);
						}

						points[points_index][0] = static_cast<float>(values[0]);
						points[points_index][1] = static_cast<float>(values[1]);
						points[points_index][2] = static_cast<float>(values[2]);
						normals[points_index][0] = static_cast<float>(derivatives[2] * derivatives[5] - derivatives[4] * derivatives[3]);
						normals[points_index][1] = static_cast<float>(derivatives[4] * derivatives[1] - derivatives[0] * derivatives[5]);
						normals[points_index][2] = static_cast<float>(derivatives[0] * derivatives[3] - derivatives[2] * derivatives[1]);
						FE_value magnitude = sqrt(normals[points_index][0] * normals[points_index][0] +
							normals[points_index][1] * normals[points_index][1] +
							normals[points_index][2] * normals[points_index][2]);
						if (magnitude > 0.0)
						{
							normals[points_index][0] /= static_cast<float>(magnitude);
							normals[points_index][1] /= static_cast<float>(magnitude);
							normals[points_index][2] /= static_cast<float>(magnitude);
						}
						points_index++;
					}

				}

			}
		//}
		if (!return_code || (points_index == 0))
		{
			DESTROY(GT_surface)(&surface);
			surface = (struct GT_surface *)NULL;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"create_surface_from_cad_geometry.  Failed to allocate data for surface or create surface");
		if (points)
			DEALLOCATE(points);
		if (normals)
			DEALLOCATE(normals);
	}

	if (values)
		DEALLOCATE(values);
	if (derivatives)
		DEALLOCATE(derivatives);
	if (data_values)
		DEALLOCATE(data_values);

	return surface;
}

struct GT_polyline_vertex_buffers *create_curves_from_cad_shape(Cmiss_field_cad_topology_id cad_topology,
	struct Computed_field *coordinate_field, struct Computed_field *data_field, struct GT_object *graphics_object)
{
	GT_polyline_vertex_buffers *curves = 0;

	/* create the lines for the model */
	if (coordinate_field)
	{
		Graphics_vertex_array* array = GT_object_get_vertex_set(graphics_object);
		int graphics_name = 0;
		array->add_integer_attribute(GRAPHICS_VERTEX_ARRAY_ATTRIBUTE_TYPE_ID,
			1, 1, &graphics_name);

		int number_of_components = Computed_field_get_number_of_components(coordinate_field);
		int num_data_field_components = 0;
		FE_value *values = 0;
		double *data_values = 0;
		ALLOCATE(values, FE_value, number_of_components);
		if (data_field && (Computed_field_get_number_of_components(data_field)==3))
		{
			num_data_field_components = 3;
			ALLOCATE(data_values, double, num_data_field_components);
		}
		int curve_count = Cmiss_field_cad_topology_get_curve_count(cad_topology);
		unsigned int number_of_points = 0;
		unsigned int vertex_start = 0;
		int return_code = 1;
		for ( int i = 0; i < curve_count && return_code; i++ )
		{
			vertex_start = array->get_number_of_vertices(GRAPHICS_VERTEX_ARRAY_ATTRIBUTE_TYPE_POSITION);
			Cmiss_cad_curve_identifier identifier = i;
			number_of_points = Cmiss_field_cad_topology_get_curve_point_count(cad_topology, identifier);
			float *data = 0;
			float *points = 0;
			ALLOCATE(points, float, number_of_points * number_of_components);
			if (data_field && num_data_field_components)
			{
				ALLOCATE(data, float, num_data_field_components * number_of_points);
			}
			for (unsigned int j = 0; j<number_of_points && points && return_code; j++)
			{
				//printf("number of points %d\n", number_of_points);
				double s = 0.0;
				return_code = Cmiss_field_cad_topology_get_curve_point_s_coordinate(cad_topology, identifier, j, s);
				if (return_code)
				{
					Field_cad_geometry_curve_location loc(cad_topology, identifier, s);
					return_code = Computed_field_evaluate_at_location(coordinate_field, loc, values);
					if (data && num_data_field_components && return_code)
					{
						return_code = Computed_field_evaluate_at_location(data_field, loc, data_values);
						data[num_data_field_components * j + 0] = static_cast<float>(data_values[0]);
						data[num_data_field_components * j + 1] = static_cast<float>(data_values[1]);
						data[num_data_field_components * j + 2] = static_cast<float>(data_values[2]);
					}
					points[number_of_components * j + 0] = static_cast<float>(values[0]);
					points[number_of_components * j + 1] = static_cast<float>(values[1]);
					points[number_of_components * j + 2] = static_cast<float>(values[2]);
				}
			}
			if (points && return_code)
			{
				array->add_float_attribute( GRAPHICS_VERTEX_ARRAY_ATTRIBUTE_TYPE_POSITION,
					3, number_of_points, points);
				if (data && num_data_field_components)
				{
					array->add_float_attribute( GRAPHICS_VERTEX_ARRAY_ATTRIBUTE_TYPE_DATA,
						3, number_of_points, data);
				}
				array->add_unsigned_integer_attribute(GRAPHICS_VERTEX_ARRAY_ATTRIBUTE_TYPE_ELEMENT_INDEX_COUNT,
					1, 1, &number_of_points);
				array->add_unsigned_integer_attribute(GRAPHICS_VERTEX_ARRAY_ATTRIBUTE_TYPE_ELEMENT_INDEX_START,
					1, 1, &vertex_start);
			}
			if (points)
				DEALLOCATE(points);
			if (data)
				DEALLOCATE(data);
		}

		if (return_code)
			curves = CREATE(GT_polyline_vertex_buffers)(g_PLAIN, 1);
		if (values)
			DEALLOCATE(values);
		if (data_values)
			DEALLOCATE(data_values);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"create_curve_from_cad_geometry.  Invalid argument");
	}

	return curves;
}

