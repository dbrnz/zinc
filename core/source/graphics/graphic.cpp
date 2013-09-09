/**
 * FILE : cmiss_graphic.cpp
 *
 * Implementation of graphic conversion object.
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <string>

#include "zinc/zincconfigure.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "zinc/status.h"
#include "zinc/element.h"
#include "zinc/glyph.h"
#include "zinc/graphic.h"
#include "zinc/font.h"
#include "zinc/graphicsfilter.h"
#include "zinc/graphicsmaterial.h"
#include "zinc/fieldsubobjectgroup.h"
#include "zinc/node.h"
#include "general/debug.h"
#include "general/enumerator_private.hpp"
#include "general/indexed_list_stl_private.hpp"
#include "general/compare.h"
#include "general/multi_range.h"
#include "general/mystring.h"
#include "general/object.h"
#include "computed_field/computed_field.h"
#include "computed_field/computed_field_finite_element.h"
#include "computed_field/computed_field_group.h"
#include "computed_field/computed_field_set.h"
#include "computed_field/computed_field_wrappers.h"
#include "finite_element/finite_element.h"
#include "finite_element/finite_element_discretization.h"
#include "finite_element/finite_element_region.h"
#include "finite_element/finite_element_to_graphics_object.h"
#include "finite_element/finite_element_to_iso_lines.h"
#include "finite_element/finite_element_to_iso_surfaces.h"
#include "finite_element/finite_element_to_streamlines.h"
#include "graphics/auxiliary_graphics_types.h"
#include "graphics/font.h"
#include "graphics/glyph.hpp"
#include "graphics/graphics_object.h"
#include "graphics/scene.h"
#include "graphics/scene.h"
#include "graphics/graphic.h"
#include "general/message.h"
#include "general/enumerator_conversion.hpp"
#include "graphics/render_gl.h"
#include "graphics/scene_coordinate_system.hpp"
#include "graphics/tessellation.hpp"
#include "computed_field/computed_field_subobject_group_private.hpp"
#if defined(USE_OPENCASCADE)
#	include "cad/computed_field_cad_geometry.h"
#	include "cad/computed_field_cad_topology.h"
#	include "cad/cad_geometry_to_graphics_object.h"
#endif /* defined(USE_OPENCASCADE) */

struct cmzn_graphic_select_graphics_data
{
	struct FE_region *fe_region;
	struct cmzn_graphic *graphic;
};

enum cmzn_graphic_change
{
	CMZN_GRAPHIC_CHANGE_NONE = 0,
	CMZN_GRAPHIC_CHANGE_REDRAW = 1,          /**< minor change requiring redraw, e.g. visibility flag toggled */
	CMZN_GRAPHIC_CHANGE_RECOMPILE = 2,       /**< graphics display list may need to be recompiled */
	CMZN_GRAPHIC_CHANGE_SELECTION = 3,       /**< change to selected objects */
	CMZN_GRAPHIC_CHANGE_PARTIAL_REBUILD = 4, /**< partial rebuild of graphics object */
	CMZN_GRAPHIC_CHANGE_FULL_REBUILD = 5,    /**< graphics object needs full rebuild */
};

/***************************************************************************//**
 * Call whenever attributes of the graphic have changed to ensure the graphics
 * object is invalidated (if needed) or that the minimum rebuild and redraw is
 * performed.
 */
static int cmzn_graphic_changed(struct cmzn_graphic *graphic,
	enum cmzn_graphic_change change)
{
	int return_code = 1;
	if (graphic)
	{
		switch (change)
		{
		case CMZN_GRAPHIC_CHANGE_REDRAW:
			break;
		case CMZN_GRAPHIC_CHANGE_RECOMPILE:
		case CMZN_GRAPHIC_CHANGE_SELECTION:
			graphic->selected_graphics_changed = 1;
			break;
		case CMZN_GRAPHIC_CHANGE_PARTIAL_REBUILD:
			// partial removal of graphics should have been done by caller
			graphic->graphics_changed = 1;
			break;
		case CMZN_GRAPHIC_CHANGE_FULL_REBUILD:
			graphic->graphics_changed = 1;
			if (graphic->graphics_object)
			{
				DEACCESS(GT_object)(&(graphic->graphics_object));
			}
			break;
		default:
			return_code = 0;
			break;
		}
		if (return_code)
		{
			cmzn_scene_changed(graphic->scene);
		}
	}
	else
	{
		return_code = 0;
	}
	return return_code;
}

PROTOTYPE_ENUMERATOR_STRING_FUNCTION(cmzn_graphic_type)
{
	const char *enumerator_string;

	ENTER(ENUMERATOR_STRING(cmzn_graphic_type));
	switch (enumerator_value)
	{
		case CMZN_GRAPHIC_POINTS:
		{
			enumerator_string = "points";
		} break;
		case CMZN_GRAPHIC_LINES:
		{
			enumerator_string = "lines";
		} break;
		case CMZN_GRAPHIC_SURFACES:
		{
			enumerator_string = "surfaces";
		} break;
		case CMZN_GRAPHIC_CONTOURS:
		{
			enumerator_string = "contours";
		} break;
		case CMZN_GRAPHIC_STREAMLINES:
		{
			enumerator_string = "streamlines";
		} break;
		default:
		{
			enumerator_string = (const char *)NULL;
		} break;
	}
	LEAVE;

	return (enumerator_string);
} /* ENUMERATOR_STRING(cmzn_graphic_type) */

DEFINE_DEFAULT_ENUMERATOR_FUNCTIONS(cmzn_graphic_type)

struct cmzn_graphic *CREATE(cmzn_graphic)(
	enum cmzn_graphic_type graphic_type)
{
	struct cmzn_graphic *graphic;

	ENTER(CREATE(cmzn_graphic));
	if ((CMZN_GRAPHIC_POINTS==graphic_type)||
		(CMZN_GRAPHIC_LINES==graphic_type)||
		(CMZN_GRAPHIC_SURFACES==graphic_type)||
		(CMZN_GRAPHIC_CONTOURS==graphic_type)||
		(CMZN_GRAPHIC_STREAMLINES==graphic_type))
	{
		if (ALLOCATE(graphic,struct cmzn_graphic,1))
		{
			graphic->position=0;
			graphic->scene = NULL;
			graphic->name = (char *)NULL;

			/* geometry settings defaults */
			/* for all graphic types */
			graphic->graphic_type=graphic_type;
			graphic->coordinate_field=(struct Computed_field *)NULL;
			/* For surfaces only at the moment */
			graphic->texture_coordinate_field=(struct Computed_field *)NULL;
			/* for 1-D and 2-D elements only */
			graphic->exterior = false;
			graphic->face=CMZN_ELEMENT_FACE_ALL; /* any face */

			/* line attributes */
			graphic->line_shape = CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE;
			for (int i = 0; i < 2; i++)
			{
				graphic->line_base_size[i] = 0.0;
				graphic->line_scale_factors[i] = 1.0;
			}
			graphic->line_orientation_scale_field = 0;

			/* for contours only */
			graphic->isoscalar_field=(struct Computed_field *)NULL;
			graphic->number_of_isovalues=0;
			graphic->isovalues=(double *)NULL;
			graphic->first_isovalue=0.0;
			graphic->last_isovalue=0.0;
			graphic->decimation_threshold = 0.0;

			/* point attributes */
			graphic->glyph = 0;
			graphic->glyph_repeat_mode = CMZN_GLYPH_REPEAT_NONE;
			for (int i = 0; i < 3; i++)
			{
				graphic->point_offset[i] = 0.0;
				graphic->point_base_size[i] = 0.0;
				graphic->point_scale_factors[i] = 1.0;
				graphic->label_offset[i] = 0.0;
				graphic->label_text[i] = 0;
			}
			graphic->point_orientation_scale_field = 0;
			graphic->signed_scale_field = 0;
			graphic->label_field = 0;
			graphic->label_density_field = 0;

			graphic->subgroup_field=(struct Computed_field *)NULL;
			graphic->select_mode=CMZN_GRAPHIC_SELECT_ON;
			switch (graphic_type)
			{
			case CMZN_GRAPHIC_POINTS:
				graphic->domain_type = CMZN_FIELD_DOMAIN_POINT;
				break;
			case CMZN_GRAPHIC_LINES:
				graphic->domain_type = CMZN_FIELD_DOMAIN_MESH_1D;
				break;
			case CMZN_GRAPHIC_SURFACES:
				graphic->domain_type = CMZN_FIELD_DOMAIN_MESH_2D;
				break;
			default:
				graphic->domain_type = CMZN_FIELD_DOMAIN_MESH_HIGHEST_DIMENSION;
				break;
			}
			// for element sampling: element points, streamlines
			graphic->sample_mode = CMZN_ELEMENT_POINT_SAMPLE_CELL_CENTRES;
			graphic->sample_density_field = 0;
			for (int i = 0; i < 3; i++)
			{
				graphic->sample_location[i] = 0.0;
			}
			// for tessellating and sampling elements
			graphic->tessellation = 0;
			graphic->tessellation_field = 0;
			/* for settings starting in a particular element */
			graphic->seed_element=(struct FE_element *)NULL;
			/* for streamlines only */
			graphic->stream_vector_field=(struct Computed_field *)NULL;
			graphic->streamlines_track_direction = CMZN_GRAPHIC_STREAMLINES_FORWARD_TRACK;
			graphic->streamline_length=1.0;
			graphic->seed_nodeset = (cmzn_nodeset_id)0;
			graphic->seed_node_mesh_location_field = (struct Computed_field *)NULL;
			graphic->overlay_flag = 0;
			graphic->overlay_order = 1;
			graphic->coordinate_system = CMZN_SCENE_COORDINATE_SYSTEM_LOCAL;
			/* appearance settings defaults */
			/* for all graphic types */
			graphic->visibility_flag = true;
			graphic->material=(struct Graphical_material *)NULL;
			graphic->secondary_material=(struct Graphical_material *)NULL;
			graphic->selected_material=(struct Graphical_material *)NULL;
			graphic->data_field=(struct Computed_field *)NULL;
			graphic->spectrum=(struct Spectrum *)NULL;
			graphic->autorange_spectrum_flag = 0;
			/* for glyphsets */
			graphic->font = NULL;
			/* for surface rendering */
			graphic->render_polygon_mode = CMZN_GRAPHIC_RENDER_POLYGON_SHADED;
			/* for streamlines only */
			graphic->streamline_data_type=STREAM_NO_DATA;
			graphic->render_line_width = 1.0;
			graphic->render_point_size = 1.0;

			/* rendering information defaults */
			graphic->graphics_object = (struct GT_object *)NULL;
			graphic->graphics_changed = 1;
			graphic->selected_graphics_changed = 0;
			graphic->time_dependent = 0;

			graphic->access_count=1;
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"CREATE(cmzn_graphic).  Insufficient memory");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"CREATE(cmzn_graphic).  Invalid graphic type");
		graphic=(struct cmzn_graphic *)NULL;
	}
	LEAVE;

	return (graphic);
} /* CREATE(cmzn_graphic) */

int DESTROY(cmzn_graphic)(
	struct cmzn_graphic **cmiss_graphic_address)
{
	int return_code;
	struct cmzn_graphic *graphic;

	ENTER(DESTROY(cmzn_graphic));
	if (cmiss_graphic_address && (graphic= *cmiss_graphic_address))
	{
		if (graphic->name)
		{
			DEALLOCATE(graphic->name);
		}
		if (graphic->graphics_object)
		{
			DEACCESS(GT_object)(&(graphic->graphics_object));
		}
		if (graphic->coordinate_field)
		{
			DEACCESS(Computed_field)(&(graphic->coordinate_field));
		}
		if (graphic->texture_coordinate_field)
		{
			DEACCESS(Computed_field)(&(graphic->texture_coordinate_field));
		}
		cmzn_field_destroy(&(graphic->line_orientation_scale_field));
		if (graphic->isoscalar_field)
		{
			DEACCESS(Computed_field)(&(graphic->isoscalar_field));
		}
		if (graphic->isovalues)
		{
			DEALLOCATE(graphic->isovalues);
		}
		if (graphic->glyph)
		{
			cmzn_glyph_destroy(&(graphic->glyph));
		}
		cmzn_field_destroy(&(graphic->point_orientation_scale_field));
		cmzn_field_destroy(&(graphic->signed_scale_field));
		for (int i = 0; i < 3; i++)
		{
			if (graphic->label_text[i])
			{
				DEALLOCATE(graphic->label_text[i]);
			}
		}
		if (graphic->label_field)
		{
			DEACCESS(Computed_field)(&(graphic->label_field));
		}
		if (graphic->label_density_field)
		{
			DEACCESS(Computed_field)(&(graphic->label_density_field));
		}
		if (graphic->subgroup_field)
		{
			DEACCESS(Computed_field)(&(graphic->subgroup_field));
		}
		cmzn_field_destroy(&(graphic->sample_density_field));
		cmzn_field_destroy(&(graphic->tessellation_field));
		if (graphic->tessellation)
		{
			DEACCESS(cmzn_tessellation)(&(graphic->tessellation));
		}
		if (graphic->stream_vector_field)
		{
			DEACCESS(Computed_field)(&(graphic->stream_vector_field));
		}
		/* appearance graphic */
		if (graphic->material)
		{
			cmzn_graphics_material_destroy(&(graphic->material));
		}
		if (graphic->secondary_material)
		{
			cmzn_graphics_material_destroy(&(graphic->secondary_material));
		}
		if (graphic->selected_material)
		{
			cmzn_graphics_material_destroy(&(graphic->selected_material));
		}
		if (graphic->data_field)
		{
			DEACCESS(Computed_field)(&(graphic->data_field));
		}
		if (graphic->spectrum)
		{
			DEACCESS(Spectrum)(&(graphic->spectrum));
		}
		if (graphic->font)
		{
			DEACCESS(cmzn_font)(&(graphic->font));
		}
		if (graphic->seed_element)
		{
			DEACCESS(FE_element)(&(graphic->seed_element));
		}
		if (graphic->seed_nodeset)
		{
			cmzn_nodeset_destroy(&graphic->seed_nodeset);
		}
		if (graphic->seed_node_mesh_location_field)
		{
			DEACCESS(Computed_field)(&(graphic->seed_node_mesh_location_field));
		}
		DEALLOCATE(*cmiss_graphic_address);
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,"DESTROY(cmzn_graphic_address).  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
}

int cmzn_graphic_get_domain_dimension(struct cmzn_graphic *graphic)
{
	int dimension = -1;
	if (graphic)
	{
		switch (graphic->domain_type)
		{
		case CMZN_FIELD_DOMAIN_POINT:
		case CMZN_FIELD_DOMAIN_NODES:
		case CMZN_FIELD_DOMAIN_DATA:
			dimension = 0;
			break;
		case CMZN_FIELD_DOMAIN_MESH_1D:
			dimension = 1;
			break;
		case CMZN_FIELD_DOMAIN_MESH_2D:
			dimension = 2;
			break;
		case CMZN_FIELD_DOMAIN_MESH_3D:
			dimension = 3;
			break;
		case CMZN_FIELD_DOMAIN_MESH_HIGHEST_DIMENSION:
			dimension = 3;
			if (graphic->scene)
			{
				dimension = FE_region_get_highest_dimension(graphic->scene->fe_region);
				if (0 >= dimension)
					dimension = 3;
			}
			break;
		case CMZN_FIELD_DOMAIN_TYPE_INVALID:
			display_message(ERROR_MESSAGE, "cmzn_graphic_get_domain_dimension.  Unknown graphic type");
			break;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_domain_dimension.  Invalid argument(s)");
	}
	return (dimension);
}

struct cmzn_element_conditional_field_data
{
	cmzn_field_cache_id field_cache;
	cmzn_field_id conditional_field;
};

/** @return true if conditional field evaluates to true in element */
int cmzn_element_conditional_field_is_true(cmzn_element_id element,
	void *conditional_field_data_void)
{
	cmzn_element_conditional_field_data *data =
		reinterpret_cast<cmzn_element_conditional_field_data*>(conditional_field_data_void);
	if (element && data)
	{
		cmzn_field_cache_set_element(data->field_cache, element);
		return cmzn_field_evaluate_boolean(data->conditional_field, data->field_cache);
	}
	return 0;
}

/***************************************************************************//**
 * Converts a finite element into a graphics object with the supplied graphic.
 * @param element  The cmzn_element.
 * @param graphic_to_object_data  Data for converting finite element to graphics.
 * @return return 1 if the element would contribute any graphics generated from the cmzn_graphic
 */
static int FE_element_to_graphics_object(struct FE_element *element,
	cmzn_graphic_to_graphics_object_data *graphic_to_object_data)
{
	FE_value initial_xi[3];
	GLfloat time;
	int element_dimension = 1, element_graphics_name,
		element_selected, i, number_in_xi[MAXIMUM_ELEMENT_XI_DIMENSIONS],
		number_of_xi_points, return_code,
		*top_level_xi_point_numbers,
		use_element_dimension, *use_number_in_xi;
	struct CM_element_information cm;
	struct Element_point_ranges_identifier element_point_ranges_identifier;
	struct FE_element *top_level_element,*use_element;
	struct cmzn_graphic *graphic;
	struct GT_glyph_set *glyph_set;
	struct GT_polyline *polyline;
	struct GT_surface *surface;
	struct Multi_range *ranges;
	FE_value_triple *xi_points = NULL;

	ENTER(FE_element_to_graphics_object);
	if (element && graphic_to_object_data &&
		(NULL != (graphic = graphic_to_object_data->graphic)) &&
		graphic->graphics_object)
	{
		element_dimension = get_FE_element_dimension(element);
		return_code = 1;
		get_FE_element_identifier(element, &cm);
		element_graphics_name = cm.number;
		/* proceed only if graphic uses this element */
		int draw_element = 1;
		cmzn_element_conditional_field_data conditional_field_data = { graphic_to_object_data->field_cache, graphic->subgroup_field };
		if (draw_element)
		{
			int dimension = cmzn_graphic_get_domain_dimension(graphic);
			draw_element = FE_element_meets_topological_criteria(element, dimension,
				graphic->exterior, graphic->face,
				graphic->subgroup_field ? cmzn_element_conditional_field_is_true : 0,
				graphic->subgroup_field ? (void *)&conditional_field_data : 0);
		}
		if (draw_element)
		{
			// FE_element_meets_topological_criteria may have set element in cache, so must set afterwards
			cmzn_field_cache_set_element(graphic_to_object_data->field_cache, element);
			if (graphic->subgroup_field && (graphic_to_object_data->iteration_mesh == graphic_to_object_data->master_mesh))
			{
				draw_element = cmzn_field_evaluate_boolean(graphic->subgroup_field, graphic_to_object_data->field_cache);
			}
		}
		int name_selected = 0;
		if (draw_element)
		{
			if ((CMZN_GRAPHIC_DRAW_SELECTED == graphic->select_mode) ||
				(CMZN_GRAPHIC_DRAW_UNSELECTED == graphic->select_mode))
			{
				if (graphic_to_object_data->selection_group_field)
				{
					name_selected = cmzn_field_evaluate_boolean(graphic_to_object_data->selection_group_field, graphic_to_object_data->field_cache);
				}
				draw_element = ((name_selected && (CMZN_GRAPHIC_DRAW_SELECTED == graphic->select_mode)) ||
					((!name_selected) && (CMZN_GRAPHIC_DRAW_SELECTED != graphic->select_mode)));
			}
		}
		if (draw_element)
		{
			/* determine discretization of element for graphic */
			// copy top_level_number_in_xi since scaled by native_discretization in
			// get_FE_element_discretization
			int top_level_number_in_xi[MAXIMUM_ELEMENT_XI_DIMENSIONS];
			for (int dim = 0; dim < MAXIMUM_ELEMENT_XI_DIMENSIONS; dim++)
			{
				top_level_number_in_xi[dim] = graphic_to_object_data->top_level_number_in_xi[dim];
			}
			top_level_element = (struct FE_element *)NULL;
			struct FE_field *native_discretization_field = 0;
			if (graphic->tessellation_field)
			{
				Computed_field_get_type_finite_element(graphic->tessellation_field, &native_discretization_field);
			}
			if (get_FE_element_discretization(element,
				graphic->subgroup_field ? cmzn_element_conditional_field_is_true : 0,
				graphic->subgroup_field ? (void *)&conditional_field_data : 0,
				graphic->face, native_discretization_field, top_level_number_in_xi,
				&top_level_element, number_in_xi))
			{
				/* g_element scenes use only one time = 0.0. Must take care. */
				time = 0.0;
				switch (graphic->graphic_type)
				{
					case CMZN_GRAPHIC_LINES:
					{
						if (CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE == graphic->line_shape)
						{
							if (graphic_to_object_data->existing_graphics)
							{
								/* So far ignore these */
							}
							if (draw_element)
							{
								return_code = FE_element_add_line_to_vertex_array(
									element, graphic_to_object_data->field_cache,
									GT_object_get_vertex_set(graphic->graphics_object),
									graphic_to_object_data->rc_coordinate_field,
									graphic->data_field,
									graphic_to_object_data->number_of_data_values,
									graphic_to_object_data->data_copy_buffer,
									graphic->texture_coordinate_field,
									number_in_xi[0], top_level_element,
									graphic_to_object_data->time);
							}
						}
						else
						{
							if (graphic_to_object_data->existing_graphics)
							{
								surface = GT_OBJECT_EXTRACT_FIRST_PRIMITIVES_AT_TIME(GT_surface)
									(graphic_to_object_data->existing_graphics, time,
										element_graphics_name);
							}
							else
							{
								surface = (struct GT_surface *)NULL;
							}
							if (draw_element)
							{
								if (surface ||
									(surface = create_cylinder_from_FE_element(element,
										graphic_to_object_data->field_cache,
										graphic_to_object_data->master_mesh,
										graphic_to_object_data->rc_coordinate_field,
										graphic->data_field, graphic->line_base_size,
										graphic->line_scale_factors, graphic->line_orientation_scale_field,
										number_in_xi[0],
										cmzn_tessellation_get_circle_divisions(graphic->tessellation),
										graphic->texture_coordinate_field,
										top_level_element, graphic->render_polygon_mode,
										graphic_to_object_data->time)))
								{
									if (!GT_OBJECT_ADD(GT_surface)(
										graphic->graphics_object, time, surface))
									{
										DESTROY(GT_surface)(&surface);
										return_code = 0;
									}
								}
								else
								{
									return_code = 0;
								}
							}
							else
							{
								if (surface)
								{
									DESTROY(GT_surface)(&surface);
								}
							}
						}
					} break;
					case CMZN_GRAPHIC_SURFACES:
					{
						if (graphic_to_object_data->existing_graphics)
						{
							surface = GT_OBJECT_EXTRACT_FIRST_PRIMITIVES_AT_TIME(GT_surface)
								(graphic_to_object_data->existing_graphics, time,
									element_graphics_name);
						}
						else
						{
							surface = (struct GT_surface *)NULL;
						}
						if (draw_element)
						{
							if (surface ||
								(surface = create_GT_surface_from_FE_element(
									element, graphic_to_object_data->field_cache,
									graphic_to_object_data->master_mesh,
									graphic_to_object_data->rc_coordinate_field,
									graphic->texture_coordinate_field, graphic->data_field,
									number_in_xi[0], number_in_xi[1],
									/*reverse_normals*/0, top_level_element,graphic->render_polygon_mode,
									graphic_to_object_data->time)))
							{
								if (!GT_OBJECT_ADD(GT_surface)(
									graphic->graphics_object, time, surface))
								{
									DESTROY(GT_surface)(&surface);
									return_code = 0;
								}
							}
							else
							{
								return_code = 0;
							}
						}
						else
						{
							if (surface)
							{
								DESTROY(GT_surface)(&surface);
							}
						}
					} break;
					case CMZN_GRAPHIC_CONTOURS:
					{
						switch (GT_object_get_type(graphic->graphics_object))
						{
							case g_SURFACE:
							{
								if (3 == element_dimension)
								{
									if (graphic_to_object_data->existing_graphics)
									{
										surface = GT_OBJECT_EXTRACT_FIRST_PRIMITIVES_AT_TIME(GT_surface)
											(graphic_to_object_data->existing_graphics, time,
												element_graphics_name);
									}
									else
									{
										surface = (struct GT_surface *)NULL;
									}
									if (draw_element)
									{
										if (NULL != surface)
										{
											if (!GT_OBJECT_ADD(GT_surface)(
												graphic->graphics_object, time, surface))
											{
												DESTROY(GT_surface)(&surface);
												return_code = 0;
											}
										}
										else
										{
											return_code = create_iso_surfaces_from_FE_element_new(element,
												graphic_to_object_data->field_cache,
												graphic_to_object_data->master_mesh,
												graphic_to_object_data->time, number_in_xi,
												graphic_to_object_data->iso_surface_specification,
												graphic->graphics_object,
												graphic->render_polygon_mode);
										}
									}
									else
									{
										if (surface)
										{
											DESTROY(GT_surface)(&surface);
										}
									}
								}
							} break;
							case g_POLYLINE:
							{
								if (2 == element_dimension)
								{
									if (graphic_to_object_data->existing_graphics)
									{
										polyline =
											GT_OBJECT_EXTRACT_FIRST_PRIMITIVES_AT_TIME(GT_polyline)
											(graphic_to_object_data->existing_graphics, time,
												element_graphics_name);
									}
									else
									{
										polyline = (struct GT_polyline *)NULL;
									}
									if (draw_element)
									{
										if (polyline)
										{
											if (!GT_OBJECT_ADD(GT_polyline)(
												graphic->graphics_object, time, polyline))
											{
												DESTROY(GT_polyline)(&polyline);
												return_code = 0;
											}
										}
										else
										{
											if (graphic->isovalues)
											{
												for (i = 0 ; i < graphic->number_of_isovalues ; i++)
												{
													return_code = create_iso_lines_from_FE_element(element,
														graphic_to_object_data->field_cache,
														graphic_to_object_data->rc_coordinate_field,
														graphic->isoscalar_field, graphic->isovalues[i],
														graphic->data_field, number_in_xi[0], number_in_xi[1],
														top_level_element, graphic->graphics_object);
												}
											}
											else
											{
												double isovalue_range;
												if (graphic->number_of_isovalues > 1)
												{
													isovalue_range =
														(graphic->last_isovalue - graphic->first_isovalue)
														/ (double)(graphic->number_of_isovalues - 1);
												}
												else
												{
													isovalue_range = 0;
												}
												for (i = 0 ; i < graphic->number_of_isovalues ; i++)
												{
													double isovalue =
														graphic->first_isovalue +
														(double)i * isovalue_range;
													return_code = create_iso_lines_from_FE_element(element,
														graphic_to_object_data->field_cache,
														graphic_to_object_data->rc_coordinate_field,
														graphic->isoscalar_field, isovalue,
														graphic->data_field, number_in_xi[0], number_in_xi[1],
														top_level_element, graphic->graphics_object);
												}
											}
										}
									}
									else
									{
										if (polyline)
										{
											DESTROY(GT_polyline)(&polyline);
										}
									}
								}
							} break;
							default:
							{
								display_message(ERROR_MESSAGE,"FE_element_to_graphics_object.  "
									"Invalid graphic type for contours");
								return_code = 0;
							} break;
						}
					} break;
					case CMZN_GRAPHIC_POINTS:
					{
						cmzn_field_cache_set_time(graphic_to_object_data->field_cache, graphic_to_object_data->time);
						glyph_set = (struct GT_glyph_set *)NULL;
						if (graphic_to_object_data->existing_graphics)
						{
							glyph_set =
								GT_OBJECT_EXTRACT_FIRST_PRIMITIVES_AT_TIME(GT_glyph_set)(
									graphic_to_object_data->existing_graphics, time,
									element_graphics_name);
						}
						if (draw_element)
						{
							if (!glyph_set)
							{
								for (i = 0; i < 3; i++)
								{
									element_point_ranges_identifier.exact_xi[i] =
										graphic->sample_location[i];
								}
								if (FE_element_get_xi_points(element,
									graphic->sample_mode, number_in_xi,
									element_point_ranges_identifier.exact_xi,
									graphic_to_object_data->field_cache,
									graphic_to_object_data->rc_coordinate_field,
									graphic->sample_density_field,
									&number_of_xi_points, &xi_points))
								{
									get_FE_element_identifier(element, &cm);
									element_graphics_name = cm.number;
									top_level_xi_point_numbers = (int *)NULL;
									if (CMZN_ELEMENT_POINT_SAMPLE_CELL_CORNERS ==
										graphic->sample_mode)
									{
										FE_element_convert_xi_points_cell_corners_to_top_level(
											element, top_level_element, top_level_number_in_xi,
											number_of_xi_points, xi_points, &top_level_xi_point_numbers);
									}
									if (top_level_xi_point_numbers)
									{
										/* xi_points have been converted to top-level */
										use_element = top_level_element;
										use_number_in_xi = top_level_number_in_xi;
									}
									else
									{
										use_element = element;
										use_number_in_xi = number_in_xi;
									}
									ranges = (struct Multi_range *)NULL;
									element_point_ranges_identifier.element = use_element;
									element_point_ranges_identifier.top_level_element=
										top_level_element;
									element_point_ranges_identifier.sample_mode =
										graphic->sample_mode;
									use_element_dimension = get_FE_element_dimension(use_element);
									for (i = 0; i < use_element_dimension; i++)
									{
										element_point_ranges_identifier.number_in_xi[i] =
											use_number_in_xi[i];
									}
									element_selected = 0;
									if (graphic_to_object_data->selection_group_field)
									{
										element_selected = cmzn_field_evaluate_boolean(graphic_to_object_data->selection_group_field, graphic_to_object_data->field_cache);
									}
									/* NOT an error if no glyph_set produced == empty selection */
									if ((0 < number_of_xi_points) &&
										NULL != (glyph_set = create_GT_glyph_set_from_FE_element(
											graphic_to_object_data->field_cache,
											use_element, top_level_element,
											graphic_to_object_data->rc_coordinate_field,
											number_of_xi_points, xi_points,
											graphic_to_object_data->glyph_gt_object, graphic->glyph_repeat_mode,
											graphic->point_base_size, graphic->point_offset,
											graphic->point_scale_factors,
											graphic_to_object_data->wrapper_orientation_scale_field,
											graphic->signed_scale_field, graphic->data_field,
											graphic->font, graphic->label_field, graphic->label_offset,
											graphic->label_text,
											graphic->select_mode, element_selected, ranges,
											top_level_xi_point_numbers)))
									{
										/* set auxiliary_object_name for glyph_set to
											 element_graphics_name so we can edit */
										GT_glyph_set_set_auxiliary_integer_identifier(glyph_set,
											element_graphics_name);
									}
									if (top_level_xi_point_numbers)
									{
										DEALLOCATE(top_level_xi_point_numbers);
									}
									DEALLOCATE(xi_points);
								}
								else
								{
									return_code = 0;
								}
							}
							if (glyph_set)
							{
								if (!GT_OBJECT_ADD(GT_glyph_set)(
									graphic->graphics_object,time,glyph_set))
								{
									DESTROY(GT_glyph_set)(&glyph_set);
									return_code = 0;
								}
							}
						}
						else
						{
							if (glyph_set)
							{
								DESTROY(GT_glyph_set)(&glyph_set);
							}
						}
					} break;
					case CMZN_GRAPHIC_STREAMLINES:
					{
						/* use local copy of sample_location since tracking function updates it */
						for (i = 0; i < 3; i++)
						{
							initial_xi[i] =  element_point_ranges_identifier.exact_xi[i] = graphic->sample_location[i];
						}
						if (FE_element_get_xi_points(element,
							graphic->sample_mode, number_in_xi,
							element_point_ranges_identifier.exact_xi,
							graphic_to_object_data->field_cache,
							graphic_to_object_data->rc_coordinate_field,
							graphic->sample_density_field,
							&number_of_xi_points, &xi_points))
						{
							switch (graphic->line_shape)
							{
							case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE:
								{
									for (i = 0; i < number_of_xi_points; i++)
									{
										initial_xi[0] = xi_points[i][0];
										initial_xi[1] = xi_points[i][1];
										initial_xi[2] = xi_points[i][2];
										if (NULL != (polyline = create_GT_polyline_streamline_FE_element(
												element, initial_xi, graphic_to_object_data->field_cache,
												graphic_to_object_data->rc_coordinate_field,
												graphic_to_object_data->wrapper_stream_vector_field,
												static_cast<int>(graphic->streamlines_track_direction == CMZN_GRAPHIC_STREAMLINES_REVERSE_TRACK),
												graphic->streamline_length,
												graphic->streamline_data_type, graphic->data_field,
												graphic_to_object_data->fe_region)))
										{
											if (!GT_OBJECT_ADD(GT_polyline)(graphic->graphics_object,
												time, polyline))
											{
												DESTROY(GT_polyline)(&polyline);
											}
										}
									}
								} break;
							case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_RIBBON:
							case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_CIRCLE_EXTRUSION:
							case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_SQUARE_EXTRUSION:
								{
									for (i = 0; i < number_of_xi_points; i++)
									{
										initial_xi[0] = xi_points[i][0];
										initial_xi[1] = xi_points[i][1];
										initial_xi[2] = xi_points[i][2];
										if (NULL != (surface = create_GT_surface_streamribbon_FE_element(
												element, initial_xi, graphic_to_object_data->field_cache,
												graphic_to_object_data->rc_coordinate_field,
												graphic_to_object_data->wrapper_stream_vector_field,
												static_cast<int>(graphic->streamlines_track_direction == CMZN_GRAPHIC_STREAMLINES_REVERSE_TRACK),
												graphic->streamline_length,
												graphic->line_shape, cmzn_tessellation_get_circle_divisions(graphic->tessellation),
												graphic->line_base_size, graphic->line_scale_factors,
												graphic->line_orientation_scale_field,
												graphic->streamline_data_type, graphic->data_field,
												graphic_to_object_data->fe_region, graphic->render_polygon_mode)))
										{
											if (!GT_OBJECT_ADD(GT_surface)(graphic->graphics_object,
												time, surface))
											{
												DESTROY(GT_surface)(&surface);
											}
										}
									}
								} break;
							case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_INVALID:
								{
									display_message(ERROR_MESSAGE,
										"FE_element_to_graphics_object.  Unknown streamline type");
									return_code = 0;
								} break;
							}
						}
						else
						{
							return_code = 0;
						}
						if (xi_points)
							DEALLOCATE(xi_points);
					} break;
					default:
					{
						display_message(ERROR_MESSAGE,"FE_element_to_graphics_object.  "
							"Unknown element graphic type");
						return_code = 0;
					} break;
				}
			}
			else
			{
				display_message(ERROR_MESSAGE,
					"FE_element_to_graphics_object.  Could not get discretization");
				return_code = 0;
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,"FE_element_to_graphics_object.  "
			"Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* FE_element_to_graphics_object */

/***************************************************************************//**
 * Creates a streamline seeded from the location given by the
 * seed_node_mesh_location_field at the node.
 * @param node  The node to seed streamline from.
 * @param graphic_to_object_data  All other data including graphic.
 * @return  1 if successfully added streamline
 */
static int cmzn_node_to_streamline(struct FE_node *node,
	struct cmzn_graphic_to_graphics_object_data *graphic_to_object_data)
{
	int return_code = 1;

	ENTER(node_to_streamline);
	struct cmzn_graphic *graphic = 0;
	if (node && graphic_to_object_data &&
		(NULL != (graphic = graphic_to_object_data->graphic)) &&
		graphic->graphics_object)
	{
		cmzn_field_cache_set_node(graphic_to_object_data->field_cache, node);
		FE_value xi[MAXIMUM_ELEMENT_XI_DIMENSIONS];
		cmzn_element_id element = cmzn_field_evaluate_mesh_location(
			graphic->seed_node_mesh_location_field, graphic_to_object_data->field_cache,
			MAXIMUM_ELEMENT_XI_DIMENSIONS, xi);
		if (element)
		{
			switch (graphic->line_shape)
			{
			case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE:
				{
					struct GT_polyline *polyline;
					if (NULL != (polyline=create_GT_polyline_streamline_FE_element(element,
							xi, graphic_to_object_data->field_cache,
							graphic_to_object_data->rc_coordinate_field,
							graphic_to_object_data->wrapper_stream_vector_field,
							static_cast<int>(graphic->streamlines_track_direction == CMZN_GRAPHIC_STREAMLINES_REVERSE_TRACK),
							graphic->streamline_length,
							graphic->streamline_data_type, graphic->data_field,
							graphic_to_object_data->fe_region)))
					{
						if (!(return_code=GT_OBJECT_ADD(GT_polyline)(
									graphic->graphics_object,
									/*graphics_object_time*/0,polyline)))
						{
							DESTROY(GT_polyline)(&polyline);
						}
					}
					else
					{
						return_code=0;
					}
				} break;
			case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_RIBBON:
			case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_CIRCLE_EXTRUSION:
			case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_SQUARE_EXTRUSION:
				{
					struct GT_surface *surface;
					if (NULL != (surface=create_GT_surface_streamribbon_FE_element(element,
						xi, graphic_to_object_data->field_cache,
						graphic_to_object_data->rc_coordinate_field,
						graphic_to_object_data->wrapper_stream_vector_field,
						static_cast<int>(graphic->streamlines_track_direction == CMZN_GRAPHIC_STREAMLINES_REVERSE_TRACK),
						graphic->streamline_length,
						graphic->line_shape, cmzn_tessellation_get_circle_divisions(graphic->tessellation),
						graphic->line_base_size, graphic->line_scale_factors,
						graphic->line_orientation_scale_field,
						graphic->streamline_data_type, graphic->data_field,
						graphic_to_object_data->fe_region, graphic->render_polygon_mode)))
					{
						if (!(return_code=GT_OBJECT_ADD(GT_surface)(
							graphic->graphics_object, /*graphics_object_time*/0, surface)))
						{
							DESTROY(GT_surface)(&surface);
						}
					}
					else
					{
						return_code = 0;
					}
				} break;
			case CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_INVALID:
				{
					display_message(ERROR_MESSAGE,
						"cmzn_node_to_streamline.  Unknown streamline type");
					return_code=0;
				} break;
			}
			cmzn_element_destroy(&element);
		}
		else
		{
			return_code=1;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_node_to_streamline.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* node_to_streamline */

int cmzn_graphic_add_to_list(struct cmzn_graphic *graphic,
	int position,struct LIST(cmzn_graphic) *list_of_graphic)
{
	int last_position,return_code;
	struct cmzn_graphic *graphic_in_way;

	ENTER(cmzn_graphic_add_to_list);
	if (graphic&&list_of_graphic&&
		!IS_OBJECT_IN_LIST(cmzn_graphic)(graphic,list_of_graphic))
	{
		return_code=1;
		last_position=NUMBER_IN_LIST(cmzn_graphic)(list_of_graphic);
		if ((1>position)||(position>last_position))
		{
			/* add to end of list */
			position=last_position+1;
		}
		ACCESS(cmzn_graphic)(graphic);
		while (return_code&&graphic)
		{
			graphic->position=position;
			/* is there already a graphic with that position? */
			if (NULL != (graphic_in_way=FIND_BY_IDENTIFIER_IN_LIST(cmzn_graphic,
						position)(position,list_of_graphic)))
			{
				/* remove the old graphic to make way for the new */
				ACCESS(cmzn_graphic)(graphic_in_way);
				REMOVE_OBJECT_FROM_LIST(cmzn_graphic)(
					graphic_in_way,list_of_graphic);
			}
			if (ADD_OBJECT_TO_LIST(cmzn_graphic)(graphic,list_of_graphic))
			{
				DEACCESS(cmzn_graphic)(&graphic);
				/* the old, in-the-way graphic now become the new graphic */
				graphic=graphic_in_way;
				position++;
			}
			else
			{
				DEACCESS(cmzn_graphic)(&graphic);
				if (graphic_in_way)
				{
					DEACCESS(cmzn_graphic)(&graphic_in_way);
				}
				display_message(ERROR_MESSAGE,"cmzn_graphic_add_to_list.  "
					"Could not add graphic - graphic lost");
				return_code=0;
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_add_to_list.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
}

int cmzn_graphic_remove_from_list(struct cmzn_graphic *graphic,
	struct LIST(cmzn_graphic) *list_of_graphic)
{
	int return_code,next_position;

	ENTER(cmzn_graphic_remove_from_list);
	if (graphic&&list_of_graphic)
	{
		if (IS_OBJECT_IN_LIST(cmzn_graphic)(graphic,list_of_graphic))
		{
			next_position=graphic->position+1;
			return_code=REMOVE_OBJECT_FROM_LIST(cmzn_graphic)(
				graphic,list_of_graphic);
			/* decrement position of all remaining graphic */
			while (return_code&&(graphic=FIND_BY_IDENTIFIER_IN_LIST(
				cmzn_graphic,position)(next_position,list_of_graphic)))
			{
				ACCESS(cmzn_graphic)(graphic);
				REMOVE_OBJECT_FROM_LIST(cmzn_graphic)(graphic,list_of_graphic);
				(graphic->position)--;
				if (ADD_OBJECT_TO_LIST(cmzn_graphic)(graphic,list_of_graphic))
				{
					next_position++;
				}
				else
				{
					display_message(ERROR_MESSAGE,
						"cmzn_graphic_remove_from_list.  "
						"Could not readjust positions - graphic lost");
					return_code=0;
				}
				DEACCESS(cmzn_graphic)(&graphic);
			}
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"cmzn_graphic_remove_from_list.  Graphic not in list");
			return_code=0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_remove_from_list.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_remove_from_list */

int cmzn_graphic_modify_in_list(struct cmzn_graphic *graphic,
	struct cmzn_graphic *new_graphic,
	struct LIST(cmzn_graphic) *list_of_graphic)
{
	int return_code,old_position;

	ENTER(cmzn_graphic_modify_in_list);
	if (graphic&&new_graphic&&list_of_graphic)
	{
		if (IS_OBJECT_IN_LIST(cmzn_graphic)(graphic,list_of_graphic))
		{
			/* save the current position */
			old_position=graphic->position;
			return_code=cmzn_graphic_copy_without_graphics_object(graphic,new_graphic);
			graphic->position=old_position;
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"cmzn_graphic_modify_in_list.  graphic not in list");
			return_code=0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_modify_in_list.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_modify_in_list */

DECLARE_OBJECT_FUNCTIONS(cmzn_graphic);

/** functor for ordering cmzn_set<GT_object> by position */
struct cmzn_graphic_compare_position_functor
{
	bool operator() (const cmzn_graphic* graphic1, const cmzn_graphic* graphic2) const
	{
		return (graphic1->position < graphic2->position);
	}
};

typedef cmzn_set<cmzn_graphic *,cmzn_graphic_compare_position_functor> cmzn_set_cmzn_graphic;

DECLARE_INDEXED_LIST_STL_FUNCTIONS(cmzn_graphic)

PROTOTYPE_FIND_BY_IDENTIFIER_IN_LIST_FUNCTION(/*object_type*/cmzn_graphic, /*identifier*/position, /*identifier_type*/int)
{
	if (list)
	{
		CMZN_SET(cmzn_graphic) *cmiss_set = reinterpret_cast<CMZN_SET(cmzn_graphic) *>(list);
		for (CMZN_SET(cmzn_graphic)::iterator iter = cmiss_set->begin(); iter != cmiss_set->end(); ++iter)
		{
			if ((*iter)->position == position)
			{
				return *iter;
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE, "FIND_BY_IDENTIFIER_IN_LIST(cmzn_graphic,position).  Invalid argument");
	}
	return (0);
}

#if defined (USE_OPENCASCADE)

int cmzn_graphic_selects_cad_primitives(struct cmzn_graphic *graphic)
{
	int return_code;

	if (graphic)
	{
		return_code=(CMZN_GRAPHIC_NO_SELECT != graphic->select_mode)&&(
			(CMZN_GRAPHIC_LINES==graphic->graphic_type)||
			(CMZN_GRAPHIC_SURFACES==graphic->graphic_type));
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_selects_cad_primitives.  Invalid argument(s)");
		return_code=0;
	}

	return (return_code);
}

#endif /*defined (USE_OPENCASCADE) */

bool cmzn_graphic_selects_elements(struct cmzn_graphic *graphic)
{
	return (CMZN_GRAPHIC_NO_SELECT != graphic->select_mode) &&
		(0 < cmzn_graphic_get_domain_dimension(graphic));
}

enum cmzn_scene_coordinate_system cmzn_graphic_get_coordinate_system(
	struct cmzn_graphic *graphic)
{
	enum cmzn_scene_coordinate_system coordinate_system;

	ENTER(cmzn_graphic_get_coordinate_system);
	if (graphic)
	{
		coordinate_system=graphic->coordinate_system;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_coordinate_system.  Invalid argument(s)");
		coordinate_system = CMZN_SCENE_COORDINATE_SYSTEM_INVALID;
	}
	LEAVE;

	return (coordinate_system);
}

int cmzn_graphic_set_coordinate_system(
	struct cmzn_graphic *graphic, enum cmzn_scene_coordinate_system coordinate_system)
{
	int return_code = 1;
	ENTER(cmzn_graphic_set_coordinate_system);
	if (graphic)
	{
		if (coordinate_system != graphic->coordinate_system)
		{
			graphic->coordinate_system=coordinate_system;
			if (cmzn_scene_coordinate_system_is_window_relative(coordinate_system))
			{
				graphic->overlay_flag = 1;
				graphic->overlay_order = 1;
			}
			else
			{
				graphic->overlay_flag = 0;
				graphic->overlay_order = 0;
			}
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_REDRAW);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_set_coordinate_system.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
}

enum cmzn_graphic_type cmzn_graphic_get_graphic_type(
	struct cmzn_graphic *graphic)
{
	enum cmzn_graphic_type graphic_type;

	ENTER(cmzn_graphic_get_graphic_type);
	if (graphic)
	{
		graphic_type=graphic->graphic_type;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_graphic_type.  Invalid argument(s)");
		graphic_type = CMZN_GRAPHIC_LINES;
	}
	LEAVE;

	return (graphic_type);
}

int cmzn_graphic_is_graphic_type(struct cmzn_graphic *graphic,
	enum cmzn_graphic_type graphic_type)
{
	int return_code = 0;

	ENTER(cmzn_graphic_is_graphic_type);
	if (graphic)
	{
		if (graphic->graphic_type==graphic_type)
		{
			return_code = 1;
		}
		else
		{
			return_code = 0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_is_graphic_type.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
}

bool cmzn_graphic_get_visibility_flag(struct cmzn_graphic *graphic)
{
	if (graphic)
	{
		return graphic->visibility_flag;
	}
	return false;
}

int cmzn_graphic_set_visibility_flag(struct cmzn_graphic *graphic,
	bool visibility_flag)
{
	if (graphic)
	{
		if (graphic->visibility_flag != visibility_flag)
		{
			graphic->visibility_flag = visibility_flag;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_REDRAW);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_and_scene_visibility_flags_is_set(struct cmzn_graphic *graphic)
{
	int return_code;

	ENTER(cmzn_graphic_and_scene_visibility_flags_set);
	if (graphic)
	{
		if (graphic->visibility_flag && cmzn_scene_is_visible_hierarchical(graphic->scene))
		{
			return_code = 1;
		}
		else
		{
			return_code = 0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_and_scene_visibility_flags_set.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
}

int cmzn_graphic_is_from_region_hierarchical(struct cmzn_graphic *graphic, struct cmzn_region *region)
{
	int return_code = 0;

	ENTER(cmzn_graphic_is_from_region_hierarchical);
	if (graphic && region)
	{
		struct cmzn_region *scene_region = cmzn_scene_get_region(graphic->scene);
		if ((scene_region == region) ||
			(cmzn_region_contains_subregion(region, scene_region)))
		{
			return_code = 1;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_is_from_region_hierarchical.  Invalid argument(s)");
	}

	return (return_code);
}

cmzn_field_id cmzn_graphic_get_coordinate_field(cmzn_graphic_id graphic)
{
	cmzn_field_id coordinate_field = 0;
	if (graphic)
	{
		if (graphic->coordinate_field)
		{
			coordinate_field = ACCESS(Computed_field)(graphic->coordinate_field);
		}
	}
	return (coordinate_field);
}

int cmzn_graphic_set_coordinate_field(cmzn_graphic_id graphic,
	cmzn_field_id coordinate_field)
{
	if (graphic && ((0 == coordinate_field) ||
		(3 >= Computed_field_get_number_of_components(coordinate_field))))
	{
		if (coordinate_field != graphic->coordinate_field)
		{
			REACCESS(Computed_field)(&(graphic->coordinate_field), coordinate_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_get_data_field(cmzn_graphic_id graphic)
{
	cmzn_field_id data_field = 0;
	if (graphic)
	{
		if (graphic->data_field)
		{
			data_field = ACCESS(Computed_field)(graphic->data_field);
		}
	}
	return (data_field);
}

int cmzn_graphic_set_data_field(cmzn_graphic_id graphic,
	cmzn_field_id data_field)
{
	int return_code = 0;
	if (graphic)
	{
		if (data_field != graphic->data_field)
		{
			REACCESS(Computed_field)(&(graphic->data_field), data_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return_code = 1;
	}
	return (return_code);
}

bool cmzn_graphic_is_exterior(cmzn_graphic_id graphic)
{
	if (graphic)
		return graphic->exterior;
	return false;
}

int cmzn_graphic_set_exterior(cmzn_graphic_id graphic, bool exterior)
{
	if (graphic)
	{
		if (exterior != graphic->exterior)
		{
			graphic->exterior = exterior;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

enum cmzn_element_face_type cmzn_graphic_get_face(cmzn_graphic_id graphic)
{
	if (graphic)
	{
		return graphic->face;
	}
	return CMZN_ELEMENT_FACE_INVALID;
}

int cmzn_graphic_set_face(cmzn_graphic_id graphic, enum cmzn_element_face_type face)
{
	if (graphic && (face != CMZN_ELEMENT_FACE_INVALID))
	{
		if (face != graphic->face)
		{
			graphic->face = face;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_update_selected(struct cmzn_graphic *graphic, void *dummy_void)
{
	USE_PARAMETER(dummy_void);
	if (graphic)
	{
		switch (graphic->select_mode)
		{
		case CMZN_GRAPHIC_SELECT_ON:
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_SELECTION);
			break;
		case CMZN_GRAPHIC_NO_SELECT:
			/* nothing to do as selection doesn't affect appearance in this mode */
			break;
		case CMZN_GRAPHIC_DRAW_SELECTED:
		case CMZN_GRAPHIC_DRAW_UNSELECTED:
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
			break;
		default:
			display_message(ERROR_MESSAGE,
				"cmzn_graphic_update_selected.  Unknown select_mode");
			break;
		}
		return 1;
	}
	return 0;
}

/** update 'trivial' attribute glyph graphics object */
void cmzn_graphic_update_graphics_object_trivial_glyph(struct cmzn_graphic *graphic)
{
	if (graphic && graphic->graphics_object &&
		(CMZN_GRAPHIC_POINTS == graphic->graphic_type))
	{
		if (graphic->glyph)
		{
			GT_object *glyph_gt_object = graphic->glyph->getGraphicsObject(graphic->tessellation, graphic->material, graphic->font);
			set_GT_object_glyph(graphic->graphics_object, glyph_gt_object);
			DEACCESS(GT_object)(&glyph_gt_object);
		}
		else
		{
			set_GT_object_glyph(graphic->graphics_object, static_cast<GT_object*>(0));
		}
	}
}

/** replace materials, spectrum and other 'trivial' attributes of existing
  * graphics object so it doesn't need complete rebuilding */
int cmzn_graphic_update_graphics_object_trivial(struct cmzn_graphic *graphic)
{
	int return_code = 0;
	if (graphic && graphic->graphics_object)
	{
		set_GT_object_default_material(graphic->graphics_object,
			graphic->material);
		set_GT_object_secondary_material(graphic->graphics_object,
			graphic->secondary_material);
		set_GT_object_selected_material(graphic->graphics_object,
			graphic->selected_material);
		set_GT_object_Spectrum(graphic->graphics_object, graphic->spectrum);
		if (CMZN_GRAPHIC_POINTS == graphic->graphic_type)
		{
			cmzn_graphic_update_graphics_object_trivial_glyph(graphic);
			set_GT_object_glyph_repeat_mode(graphic->graphics_object, graphic->glyph_repeat_mode);
			Triple base_size, scale_factors, offset, label_offset;
			for (int i = 0; i < 3; ++i)
			{
				base_size[i] = static_cast<GLfloat>(graphic->point_base_size[i]);
				scale_factors[i] = static_cast<GLfloat>(graphic->point_scale_factors[i]);
				offset[i] = static_cast<GLfloat>(graphic->point_offset[i]);
				label_offset[i] = static_cast<GLfloat>(graphic->label_offset[i]);
			}
			set_GT_object_glyph_base_size(graphic->graphics_object, base_size);
			set_GT_object_glyph_scale_factors(graphic->graphics_object, scale_factors);
			set_GT_object_glyph_offset(graphic->graphics_object, offset);
			set_GT_object_font(graphic->graphics_object, graphic->font);
			set_GT_object_glyph_label_offset(graphic->graphics_object, label_offset);
			set_GT_object_glyph_label_text(graphic->graphics_object, graphic->label_text);
		}
		set_GT_object_render_polygon_mode(graphic->graphics_object, graphic->render_polygon_mode);
		set_GT_object_render_line_width(graphic->graphics_object, graphic->render_line_width);
		set_GT_object_render_point_size(graphic->graphics_object, graphic->render_point_size);
		return_code = 1;
	}
	return return_code;
}

cmzn_graphics_material_id cmzn_graphic_get_material(
	cmzn_graphic_id graphic)
{
	if (graphic)
	{
		return ACCESS(Graphical_material)(graphic->material);
	}
	return 0;
}

int cmzn_graphic_set_material(cmzn_graphic_id graphic,
	cmzn_graphics_material_id material)
{
	if (graphic && material)
	{
		if (material != graphic->material)
		{
			REACCESS(Graphical_material)(&(graphic->material), material);
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

struct Graphical_material *cmzn_graphic_get_selected_material(
	struct cmzn_graphic *graphic)
{
	if (graphic)
	{
		return ACCESS(Graphical_material)(graphic->selected_material);
	}
	return 0;
}

int cmzn_graphic_set_selected_material(cmzn_graphic_id graphic,
	cmzn_graphics_material_id selected_material)
{
	if (graphic && selected_material)
	{
		if (selected_material != graphic->selected_material)
		{
			REACCESS(Graphical_material)(&(graphic->selected_material),
				selected_material);
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

char *cmzn_graphic_get_name(cmzn_graphic_id graphic)
{
	char *name = NULL;
	if (graphic && graphic->name)
	{
		name = duplicate_string(graphic->name);
	}

	return name;
}

char *cmzn_graphic_get_name_internal(struct cmzn_graphic *graphic)
{
	char *name = 0;
	if (graphic)
	{
		if (graphic->name)
		{
			name = duplicate_string(graphic->name);
		}
		else
		{
			char temp[30];
			sprintf(temp, "%d", graphic->position);
			name = duplicate_string(temp);
		}
	}
	return name;
}

int cmzn_graphic_set_name(struct cmzn_graphic *graphic, const char *name)
{
	if (graphic)
	{
		if (graphic->name)
		{
			DEALLOCATE(graphic->name);
		}
		if (name)
		{
			graphic->name = duplicate_string(name);
		}
		else
		{
			graphic->name = 0;
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

char *cmzn_graphic_get_summary_string(struct cmzn_graphic *graphic)
{
	if (!graphic)
		return 0;
	char *graphic_string = 0;
	int error = 0;
	char temp_string[100];
	if (graphic->name)
	{
		sprintf(temp_string, "%s. ", graphic->name);
	}
	else
	{
		sprintf(temp_string, "%i. ", graphic->position);
	}
	append_string(&graphic_string, temp_string, &error);
	append_string(&graphic_string,
		ENUMERATOR_STRING(cmzn_graphic_type)(graphic->graphic_type),
		&error);
	append_string(&graphic_string, " ", &error);
	append_string(&graphic_string, ENUMERATOR_STRING(cmzn_field_domain_type)(graphic->domain_type), &error);
	if (graphic->subgroup_field)
	{
		char *name = cmzn_field_get_name(graphic->subgroup_field);
		append_string(&graphic_string, " subgroup ", &error);
		append_string(&graphic_string, name, &error);
		DEALLOCATE(name);
	}
	return graphic_string;
}

char *cmzn_graphic_string(struct cmzn_graphic *graphic,
	enum cmzn_graphic_string_details graphic_detail)
{
	char *graphic_string = NULL,temp_string[100],*name;
	int error,i;

	ENTER(cmzn_graphic_string);
	graphic_string=(char *)NULL;
	if (graphic&&(
		(GRAPHIC_STRING_GEOMETRY==graphic_detail)||
		(GRAPHIC_STRING_COMPLETE==graphic_detail)||
		(GRAPHIC_STRING_COMPLETE_PLUS==graphic_detail)))
	{
		error=0;
		if (GRAPHIC_STRING_COMPLETE_PLUS==graphic_detail)
		{
			if (graphic->name)
			{
				sprintf(temp_string,"%i. (%s) ",graphic->position, graphic->name);
			}
			else
			{
				sprintf(temp_string,"%i. ",graphic->position);
			}
			append_string(&graphic_string,temp_string,&error);
		}

		/* show geometry graphic */
		/* for all graphic types */
		/* write graphic type = "points", "lines" etc. */
		append_string(&graphic_string,
			ENUMERATOR_STRING(cmzn_graphic_type)(graphic->graphic_type),
			&error);
		append_string(&graphic_string, " ", &error);
		append_string(&graphic_string, ENUMERATOR_STRING(cmzn_field_domain_type)(graphic->domain_type), &error);
		if (graphic->name)
		{
			sprintf(temp_string," as %s", graphic->name);
			append_string(&graphic_string,temp_string,&error);
		}
		if (graphic->subgroup_field)
		{
			if (GET_NAME(Computed_field)(graphic->subgroup_field,&name))
			{
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string," subgroup ",&error);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);
			}
		}
		if (graphic->coordinate_field)
		{
			append_string(&graphic_string," coordinate ",&error);
			name=(char *)NULL;
			if (GET_NAME(Computed_field)(graphic->coordinate_field,&name))
			{
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);
			}
			else
			{
				append_string(&graphic_string,"NONE",&error);
			}
		}

		/* for 1-D and 2-D elements only */
		const int domain_dimension = cmzn_graphic_get_domain_dimension(graphic);
		if ((1 == domain_dimension) || (2 == domain_dimension))
		{
			if (graphic->exterior)
			{
				append_string(&graphic_string," exterior",&error);
			}
			if (CMZN_ELEMENT_FACE_ALL != graphic->face)
			{
				append_string(&graphic_string," face",&error);
				switch (graphic->face)
				{
					case CMZN_ELEMENT_FACE_XI1_0:
					{
						append_string(&graphic_string," xi1_0",&error);
					} break;
					case CMZN_ELEMENT_FACE_XI1_1:
					{
						append_string(&graphic_string," xi1_1",&error);
					} break;
					case CMZN_ELEMENT_FACE_XI2_0:
					{
						append_string(&graphic_string," xi2_0",&error);
					} break;
					case CMZN_ELEMENT_FACE_XI2_1:
					{
						append_string(&graphic_string," xi2_1",&error);
					} break;
					case CMZN_ELEMENT_FACE_XI3_0:
					{
						append_string(&graphic_string," xi3_0",&error);
					} break;
					case CMZN_ELEMENT_FACE_XI3_1:
					{
						append_string(&graphic_string," xi3_1",&error);
					} break;
					default:
					{
						display_message(ERROR_MESSAGE,
							"cmzn_graphic_string.  Invalid face number");
						DEALLOCATE(graphic_string);
						error=1;
					} break;
				}
			}
		}

		append_string(&graphic_string, " tessellation ", &error);
		if (graphic->tessellation)
		{
			name = cmzn_tessellation_get_name(graphic->tessellation);
			/* put quotes around name if it contains special characters */
			make_valid_token(&name);
			append_string(&graphic_string, name, &error);
			DEALLOCATE(name);
		}
		else
		{
			append_string(&graphic_string,"NONE",&error);
		}

		append_string(&graphic_string," ",&error);
		append_string(&graphic_string,
			ENUMERATOR_STRING(cmzn_scene_coordinate_system)(graphic->coordinate_system),&error);

		if ((graphic->render_line_width < 0.99999) || (1.00001 < graphic->render_line_width))
		{
			sprintf(temp_string, " line_width %g", graphic->render_line_width);
			append_string(&graphic_string,temp_string,&error);
		}
		if ((graphic->render_point_size < 0.99999) || (1.00001 < graphic->render_point_size))
		{
			sprintf(temp_string, " point_size %g", graphic->render_point_size);
			append_string(&graphic_string,temp_string,&error);
		}

		/* overlay is temporarily disabled, instead the functionality is replaced
			 by coordinate_system
		if (CMZN_GRAPHIC_STATIC==graphic->graphic_type)
		{
			if (graphic->overlay_flag == 0 )
			{
				append_string(&graphic_string, " no_overlay ",&error);
			}
			else
			{
				sprintf(temp_string, " overlay %d", graphic->overlay_order);
				append_string(&graphic_string,temp_string,&error);
			}
		}
		*/

		if (CMZN_GRAPHIC_CONTOURS == graphic->graphic_type)
		{
			if (graphic->isoscalar_field)
			{
				if (GET_NAME(Computed_field)(graphic->isoscalar_field,&name))
				{
					/* put quotes around name if it contains special characters */
					make_valid_token(&name);
					append_string(&graphic_string," iso_scalar ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
				else
				{
					DEALLOCATE(graphic_string);
					error=1;
				}
			}
			if (graphic->isovalues)
			{
				sprintf(temp_string," iso_values");
				append_string(&graphic_string,temp_string,&error);
				for (i = 0 ; i < graphic->number_of_isovalues ; i++)
				{
					sprintf(temp_string, " %g", graphic->isovalues[i]);
					append_string(&graphic_string,temp_string,&error);
				}
			}
			else
			{
				sprintf(temp_string," range_number_of_iso_values %d",
					graphic->number_of_isovalues);
				append_string(&graphic_string,temp_string,&error);
				sprintf(temp_string," first_iso_value %g",
					graphic->first_isovalue);
				append_string(&graphic_string,temp_string,&error);
				sprintf(temp_string," last_iso_value %g",
					graphic->last_isovalue);
				append_string(&graphic_string,temp_string,&error);
			}
			if (graphic->decimation_threshold > 0.0)
			{
				sprintf(temp_string," decimation_threshold %g",
					graphic->decimation_threshold);
				append_string(&graphic_string,temp_string,&error);
			}
		}

		// line attributes
		if ((graphic->graphic_type == CMZN_GRAPHIC_LINES) ||
			(graphic->graphic_type == CMZN_GRAPHIC_STREAMLINES))
		{
			append_string(&graphic_string," ",&error);
			append_string(&graphic_string,
				ENUMERATOR_STRING(cmzn_graphic_line_attributes_shape)(graphic->line_shape),&error);

			append_string(&graphic_string, " line_base_size ", &error);
			if (graphic->line_base_size[1] == graphic->line_base_size[0])
			{
				sprintf(temp_string, "%g", graphic->line_base_size[0]);
			}
			else
			{
				sprintf(temp_string, "\"%g*%g\"", graphic->line_base_size[0], graphic->line_base_size[1]);
			}
			append_string(&graphic_string, temp_string, &error);

			if (graphic->line_orientation_scale_field)
			{
				name = cmzn_field_get_name(graphic->line_orientation_scale_field);
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string, " line_orientation_scale ", &error);
				append_string(&graphic_string, name, &error);
				DEALLOCATE(name);

				append_string(&graphic_string, " line_scale_factors ", &error);
				if (graphic->line_scale_factors[1] == graphic->line_scale_factors[0])
				{
					sprintf(temp_string,"%g", graphic->line_scale_factors[0]);
				}
				else
				{
					sprintf(temp_string,"\"%g*%g\"", graphic->line_scale_factors[0], graphic->line_scale_factors[1]);
				}
				append_string(&graphic_string,temp_string,&error);
			}
		}

		// point attributes
		if (CMZN_GRAPHIC_POINTS == graphic->graphic_type)
		{
			if (graphic->glyph)
			{
				append_string(&graphic_string," glyph ",&error);
				name = cmzn_glyph_get_name(graphic->glyph);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);

				if (graphic->glyph_repeat_mode != CMZN_GLYPH_REPEAT_NONE)
				{
					append_string(&graphic_string, " ", &error);
					append_string(&graphic_string,
						ENUMERATOR_STRING(cmzn_glyph_repeat_mode)(graphic->glyph_repeat_mode), &error);
				}
				sprintf(temp_string," size \"%g*%g*%g\"",graphic->point_base_size[0],
					graphic->point_base_size[1],graphic->point_base_size[2]);
				append_string(&graphic_string,temp_string,&error);

				sprintf(temp_string," offset %g,%g,%g",
					graphic->point_offset[0], graphic->point_offset[1], graphic->point_offset[2]);

				append_string(&graphic_string,temp_string,&error);
				if (graphic->font)
				{
					append_string(&graphic_string," font ",&error);
					if (GET_NAME(cmzn_font)(graphic->font, &name))
					{
						append_string(&graphic_string,name,&error);
						DEALLOCATE(name);
					}
				}
				if (graphic->label_field)
				{
					name = cmzn_field_get_name(graphic->label_field);
					make_valid_token(&name);
					append_string(&graphic_string," label ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
				const int number_of_glyphs =
					cmzn_glyph_repeat_mode_get_number_of_glyphs(graphic->glyph_repeat_mode);
				int last_glyph_number_with_label_text = -1;
				for (int glyph_number = 0; glyph_number < number_of_glyphs; ++glyph_number)
				{
					if (cmzn_glyph_repeat_mode_glyph_number_has_label(graphic->glyph_repeat_mode, glyph_number) &&
						(0 != graphic->label_text[glyph_number]))
					{
						last_glyph_number_with_label_text = glyph_number;
					}
				}
				if (graphic->label_field || (last_glyph_number_with_label_text >= 0))
				{
					sprintf(temp_string," label_offset \"%g,%g,%g\"",graphic->label_offset[0],
						graphic->label_offset[1],graphic->label_offset[2]);
					append_string(&graphic_string,temp_string,&error);
				}
				if (last_glyph_number_with_label_text >= 0)
				{
					append_string(&graphic_string, " label_text ", &error);
					int number_of_labels = 0;
					for (int glyph_number = 0; glyph_number <= last_glyph_number_with_label_text; ++glyph_number)
					{
						if (cmzn_glyph_repeat_mode_glyph_number_has_label(graphic->glyph_repeat_mode, glyph_number))
						{
							if (number_of_labels > 0)
							{
								append_string(&graphic_string, " & ", &error);
							}
							if (graphic->label_text[number_of_labels])
							{
								char *label_text = duplicate_string(graphic->label_text[number_of_labels]);
								make_valid_token(&label_text);
								append_string(&graphic_string, label_text, &error);
								DEALLOCATE(label_text);
							}
							else
							{
								append_string(&graphic_string, "\"\"", &error);
							}
							++number_of_labels;
						}
					}
				}
				if (graphic->label_density_field)
				{
					if (GET_NAME(Computed_field)(graphic->label_density_field,&name))
					{
						/* put quotes around name if it contains special characters */
						make_valid_token(&name);
						append_string(&graphic_string," ldensity ",&error);
						append_string(&graphic_string,name,&error);
						DEALLOCATE(name);
					}
				}
				if (graphic->point_orientation_scale_field)
				{
					if (GET_NAME(Computed_field)(graphic->point_orientation_scale_field,&name))
					{
						/* put quotes around name if it contains special characters */
						make_valid_token(&name);
						append_string(&graphic_string," orientation ",&error);
						append_string(&graphic_string,name,&error);
						DEALLOCATE(name);
					}
					else
					{
						DEALLOCATE(graphic_string);
						error=1;
					}
				}
				if (graphic->signed_scale_field)
				{
					if (GET_NAME(Computed_field)(graphic->signed_scale_field,&name))
					{
						/* put quotes around name if it contains special characters */
						make_valid_token(&name);
						append_string(&graphic_string," variable_scale ",&error);
						append_string(&graphic_string,name,&error);
						DEALLOCATE(name);
					}
					else
					{
						DEALLOCATE(graphic_string);
						error=1;
					}
				}
				if (graphic->point_orientation_scale_field || graphic->signed_scale_field)
				{
					sprintf(temp_string," scale_factors \"%g*%g*%g\"",
						graphic->point_scale_factors[0],
						graphic->point_scale_factors[1],
						graphic->point_scale_factors[2]);
					append_string(&graphic_string,temp_string,&error);
				}
			}
			else
			{
				append_string(&graphic_string," glyph none",&error);
			}
		}

		/* for sampling points only */
		if ((domain_dimension > 0) && (
			(CMZN_GRAPHIC_POINTS == graphic->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type)))
		{
			append_string(&graphic_string," ",&error);
			append_string(&graphic_string, ENUMERATOR_STRING(cmzn_element_point_sample_mode)(
				graphic->sample_mode), &error);
			if (CMZN_ELEMENT_POINT_SAMPLE_SET_LOCATION != graphic->sample_mode)
			{
				if (CMZN_ELEMENT_POINT_SAMPLE_CELL_POISSON == graphic->sample_mode)
				{
					append_string(&graphic_string, " density ", &error);
					if (graphic->sample_density_field)
					{
						if (GET_NAME(Computed_field)(graphic->sample_density_field,&name))
						{
							/* put quotes around name if it contains special characters */
							make_valid_token(&name);
							append_string(&graphic_string, name, &error);
							DEALLOCATE(name);
						}
						else
						{
							DEALLOCATE(graphic_string);
							error = 1;
						}
					}
					else
					{
						append_string(&graphic_string,"NONE",&error);
					}
				}
			}
		}

		if (domain_dimension > 0)
		{
			if (graphic->tessellation_field)
			{
				append_string(&graphic_string," native_discretization ", &error);
				name = cmzn_field_get_name(graphic->tessellation_field);
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);
			}
		}

		/* for graphic starting in a particular element */
		if (CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type)
		{
			if (graphic->seed_element)
			{
				sprintf(temp_string, " seed_element %d",
					FE_element_get_cm_number(graphic->seed_element));
				append_string(&graphic_string, temp_string, &error);
			}
		}

		/* for graphic requiring an exact xi location */
		if ((domain_dimension > 0) && (
			(CMZN_GRAPHIC_POINTS == graphic->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type)) &&
			(CMZN_ELEMENT_POINT_SAMPLE_SET_LOCATION == graphic->sample_mode))
		{
			sprintf(temp_string," xi %g,%g,%g",
				graphic->sample_location[0],graphic->sample_location[1],graphic->sample_location[2]);
			append_string(&graphic_string,temp_string,&error);
		}

		/* for streamlines only */
		if (CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type)
		{
			if (graphic->stream_vector_field)
			{
				if (GET_NAME(Computed_field)(graphic->stream_vector_field,&name))
				{
					/* put quotes around name if it contains special characters */
					make_valid_token(&name);
					append_string(&graphic_string," vector ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
				else
				{
					DEALLOCATE(graphic_string);
					error=1;
				}
			}
			append_string(&graphic_string, " ", &error);
			append_string(&graphic_string,
				ENUMERATOR_STRING(cmzn_graphic_streamlines_track_direction)(graphic->streamlines_track_direction), &error);
			sprintf(temp_string," length %g ", graphic->streamline_length);
			append_string(&graphic_string,temp_string,&error);
			append_string(&graphic_string,
				ENUMERATOR_STRING(Streamline_data_type)(graphic->streamline_data_type),&error);
			if (graphic->seed_nodeset)
			{
				append_string(&graphic_string, " seed_nodeset ", &error);
				char *nodeset_name = cmzn_nodeset_get_name(graphic->seed_nodeset);
				make_valid_token(&nodeset_name);
				append_string(&graphic_string, nodeset_name, &error);
				DEALLOCATE(nodeset_name);
			}
			if (graphic->seed_node_mesh_location_field)
			{
				if (GET_NAME(Computed_field)(graphic->seed_node_mesh_location_field,&name))
				{
					/* put quotes around name if it contains special characters */
					make_valid_token(&name);
					append_string(&graphic_string," seed_node_mesh_location_field ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
				else
				{
					DEALLOCATE(graphic_string);
					error=1;
				}
			}
		}
		append_string(&graphic_string," ",&error);
		append_string(&graphic_string,
			ENUMERATOR_STRING(cmzn_graphic_select_mode)(graphic->select_mode),&error);

		if ((GRAPHIC_STRING_COMPLETE==graphic_detail)||
			(GRAPHIC_STRING_COMPLETE_PLUS==graphic_detail))
		{
			/* show appearance graphic */
			/* for all graphic types */
			if (!graphic->visibility_flag)
			{
				append_string(&graphic_string," invisible",&error);
			}
			if (graphic->material&&
				GET_NAME(Graphical_material)(graphic->material,&name))
			{
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string," material ",&error);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);
			}
			if (graphic->secondary_material&&
				GET_NAME(Graphical_material)(graphic->secondary_material,&name))
			{
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string," secondary_material ",&error);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);
			}
			if (graphic->texture_coordinate_field)
			{
				if (GET_NAME(Computed_field)(graphic->texture_coordinate_field,&name))
				{
					/* put quotes around name if it contains special characters */
					make_valid_token(&name);
					append_string(&graphic_string," texture_coordinates ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
				else
				{
					DEALLOCATE(graphic_string);
					error=1;
				}
			}
			if (graphic->data_field)
			{
				if (GET_NAME(Computed_field)(graphic->data_field,&name))
				{
					/* put quotes around name if it contains special characters */
					make_valid_token(&name);
					append_string(&graphic_string," data ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
				else
				{
					DEALLOCATE(graphic_string);
					error=1;
				}
				if (graphic->spectrum&&
					GET_NAME(Spectrum)(graphic->spectrum,&name))
				{
					/* put quotes around name if it contains special characters */
					make_valid_token(&name);
					append_string(&graphic_string," spectrum ",&error);
					append_string(&graphic_string,name,&error);
					DEALLOCATE(name);
				}
			}
			if (graphic->selected_material&&
				GET_NAME(Graphical_material)(graphic->selected_material,&name))
			{
				/* put quotes around name if it contains special characters */
				make_valid_token(&name);
				append_string(&graphic_string," selected_material ",&error);
				append_string(&graphic_string,name,&error);
				DEALLOCATE(name);
			}
			/* for surfaces rendering */
			append_string(&graphic_string," ",&error);
			append_string(&graphic_string,
				ENUMERATOR_STRING(cmzn_graphic_render_polygon_mode)(graphic->render_polygon_mode),&error);
		}
		if (error)
		{
			display_message(ERROR_MESSAGE,
				"cmzn_graphic_string.  Error creating string");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_string.  Invalid argument(s)");
	}
	LEAVE;

	return graphic_string;
} /* cmzn_graphic_string */

int cmzn_graphic_to_point_object_at_time(
	struct cmzn_graphic *graphic,
	struct cmzn_graphic_to_graphics_object_data *graphic_to_object_data,
	GLfloat graphics_object_primitive_time)
{
	int return_code = 1;
	struct GT_glyph_set *glyph_set;
	ENTER(cmzn_graphic_to_point_object_at_time);
	if (graphic && graphic_to_object_data)
	{
		cmzn_field_cache_set_time(graphic_to_object_data->field_cache, graphic_to_object_data->time);
		FE_value coordinates[3] = { 0.0, 0.0, 0.0 };
		if (graphic->coordinate_field)
		{
			if (CMZN_OK != cmzn_field_evaluate_real(graphic->coordinate_field, graphic_to_object_data->field_cache, 3, coordinates))
			{
				return 0;
			}
		}
		FE_value a[3], b[3], c[3], size[3];
		FE_value orientationScale[9];
		int orientationScaleComponentCount = 0;
		if (graphic->point_orientation_scale_field)
		{
			orientationScaleComponentCount = cmzn_field_get_number_of_components(graphic->point_orientation_scale_field);
			if (CMZN_OK != cmzn_field_evaluate_real(graphic->point_orientation_scale_field,
				graphic_to_object_data->field_cache, orientationScaleComponentCount, orientationScale))
			{
				display_message(WARNING_MESSAGE, "Orientation scale field not defined at point");
			}
		}
		if (!make_glyph_orientation_scale_axes(orientationScaleComponentCount,
			orientationScale, a, b, c, size))
		{
			display_message(WARNING_MESSAGE, "Invalid orientation scale at point");
		}
		if (graphic->signed_scale_field)
		{
			FE_value signedScale[3];
			if (CMZN_OK == cmzn_field_evaluate_real(graphic->signed_scale_field,
				graphic_to_object_data->field_cache, /*number_of_values*/3, signedScale))
			{
				const int componentCount = cmzn_field_get_number_of_components(graphic->signed_scale_field);
				for (int j = 0; j < componentCount; j++)
				{
					size[j] *= signedScale[j];
				}
			}
			else
			{
				display_message(WARNING_MESSAGE, "Variable/signed scale field not defined at point");
			}
		}
		FE_value *data = 0;
		int dataComponentCount = 0;
		if (graphic->data_field)
		{
			dataComponentCount = cmzn_field_get_number_of_components(graphic->data_field);
			data = new FE_value[dataComponentCount];
			if (CMZN_OK != cmzn_field_evaluate_real(graphic->data_field,
				graphic_to_object_data->field_cache, dataComponentCount, data))
			{
				display_message(WARNING_MESSAGE, "Data field not defined at point");
			}
		}
		char **labels = 0;
		if (graphic->label_field)
		{
			ALLOCATE(labels, char *, 1);
			*labels = cmzn_field_evaluate_string(graphic->label_field, graphic_to_object_data->field_cache);
		}
		GT_object_remove_primitives_at_time(
			graphic->graphics_object, graphics_object_primitive_time,
			(GT_object_primitive_object_name_conditional_function *)NULL,
			(void *)NULL);
		Triple *point_list, *axis1_list, *axis2_list, *axis3_list,
			*scale_list;
		ALLOCATE(point_list, Triple, 1);
		ALLOCATE(axis1_list, Triple, 1);
		ALLOCATE(axis2_list, Triple, 1);
		ALLOCATE(axis3_list, Triple, 1);
		ALLOCATE(scale_list, Triple, 1);
		for (int j = 0; j < 3; j++)
		{
			(*point_list)[j] = static_cast<GLfloat>(coordinates[j]);
			(*axis1_list)[j] = static_cast<GLfloat>(a[j]);
			(*axis2_list)[j] = static_cast<GLfloat>(b[j]);
			(*axis3_list)[j] = static_cast<GLfloat>(c[j]);
			(*scale_list)[j] = static_cast<GLfloat>(size[j]);
		}
		GLfloat *floatData = 0;
		if (data)
		{
			ALLOCATE(floatData, GLfloat, dataComponentCount);
			for (int i = 0; i < dataComponentCount; ++i)
			{
				floatData[i] = static_cast<GLfloat>(data[i]);
			}
		}
		Triple glyph_base_size, glyph_scale_factors, glyph_offset, glyph_label_offset;
		for (int i = 0; i < 3; i++)
		{
			glyph_base_size[i] = static_cast<GLfloat>(graphic->point_base_size[i]);
			glyph_scale_factors[i] = static_cast<GLfloat>(graphic->point_scale_factors[i]);
			glyph_offset[i] = static_cast<GLfloat>(graphic->point_offset[i]);
			glyph_label_offset[i] = static_cast<GLfloat>(graphic->label_offset[i]);
		}
		glyph_set = CREATE(GT_glyph_set)(1,
			point_list, axis1_list, axis2_list, axis3_list, scale_list,
			graphic_to_object_data->glyph_gt_object, graphic->glyph_repeat_mode,
			glyph_base_size, glyph_scale_factors, glyph_offset,
			graphic->font, labels, glyph_label_offset, graphic->label_text, dataComponentCount, floatData,
			/*label_bounds_dimension*/0, /*label_bounds_components*/0, /*label_bounds*/(ZnReal *)NULL,
			/*label_density_list*/(Triple *)NULL, /*object_name*/-1, /*names*/(int *)NULL);
		if (glyph_set)
		{
			if (!GT_OBJECT_ADD(GT_glyph_set)(graphic->graphics_object,
				graphics_object_primitive_time,glyph_set))
			{
				DESTROY(GT_glyph_set)(&glyph_set);
				return_code=0;
			}
		}
		delete[] data;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_to_point_object_at_time.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
}

#if defined (USE_OPENCASCADE)
static int Cad_shape_to_graphics_object(struct Computed_field *field,
	struct cmzn_graphic_to_graphics_object_data *graphic_to_object_data)
{
	int return_code = 0;
	GLfloat time = 0.0;
	struct cmzn_graphic *graphic = graphic_to_object_data->graphic;
	cmzn_field_cad_topology_id cad_topology = cmzn_field_cast_cad_topology(field);

	if (cad_topology)
	{
		switch (graphic->graphic_type)
		{
			case CMZN_GRAPHIC_SURFACES:
			{
				//printf( "Building cad geometry surfaces\n" );
				int surface_count = cmzn_field_cad_topology_get_surface_count(cad_topology);
				if (surface_count > 0)
				{
					return_code = 1;
				}
				for (int i = 0; i < surface_count && return_code; i++)
				{
					cmzn_cad_surface_identifier identifier = i;
					struct GT_surface *surface = create_surface_from_cad_shape(cad_topology, graphic_to_object_data->field_cache, graphic_to_object_data->rc_coordinate_field, graphic->data_field, graphic->render_polygon_mode, identifier);
					if (surface && GT_OBJECT_ADD(GT_surface)(graphic->graphics_object, time, surface))
					{
						//printf( "Surface added to graphics object\n" );
						return_code = 1;
					}
					else
					{
						return_code = 0;
					}
				}
				break;
			}
			case CMZN_GRAPHIC_LINES:
			{
				//struct GT_object *graphics_object = settings->graphics_object;
				/*
				GT_polyline_vertex_buffers *lines =
					CREATE(GT_polyline_vertex_buffers)(
					g_PLAIN);
				*/
				GT_polyline_vertex_buffers *lines = create_curves_from_cad_shape(cad_topology, graphic_to_object_data->field_cache, graphic_to_object_data->rc_coordinate_field, graphic->data_field, graphic->graphics_object);
				if (lines && GT_OBJECT_ADD(GT_polyline_vertex_buffers)(
					graphic->graphics_object, lines))
				{
					//printf("Adding lines for cad shape\n");
					return_code = 1;
				}
				else
				{
					//DESTROY(GT_polyline_vertex_buffers)(&lines);
					return_code = 0;
				}
			} break;
			default:
			{
				display_message(ERROR_MESSAGE,"Cad_geometry_to_graphics_object.  "
					"Can't handle this type of graphic");
				return_code = 0;
			}
		}
		cmzn_field_destroy((cmzn_field_id*)&cad_topology);
	}

	return return_code;
}
#endif /* (USE_OPENCASCADE) */

#if defined (USE_OPENCASCADE)
SubObjectGroupHighlightFunctor *create_highlight_functor_cad_primitive(
	struct Computed_field *group_field, cmzn_field_cad_topology_id cad_topology_domain)
{
	SubObjectGroupHighlightFunctor *highlight_functor = NULL;
	if (group_field)
	{
		cmzn_field_group_id sub_group = cmzn_field_cast_group(group_field);

		//cmzn_field_id cad_primitive_group_field = cmzn_field_group_get_cad_primitive_group(sub_group, cad_topology_domain);
		cmzn_field_id cad_primitive_subgroup_field = cmzn_field_group_get_subobject_group_for_domain(sub_group,
			reinterpret_cast<cmzn_field_id>(cad_topology_domain));
		cmzn_field_cad_primitive_group_template_id cad_primitive_group = NULL;
		if (cad_primitive_subgroup_field)
		{
			cad_primitive_group =
				cmzn_field_cast_cad_primitive_group_template(cad_primitive_subgroup_field);
			cmzn_field_destroy(&cad_primitive_subgroup_field);
			if (cad_primitive_group)
			{
				Computed_field_sub_group_object<cmzn_cad_identifier_id> *group_core =
					Computed_field_sub_group_object_core_cast<cmzn_cad_identifier_id,
					cmzn_field_cad_primitive_group_template_id>(cad_primitive_group);
				highlight_functor =
					new SubObjectGroupHighlightFunctor(group_core,
					&Computed_field_subobject_group::isIdentifierInList);
				cmzn_field_id temporary_handle =
					reinterpret_cast<Computed_field *>(cad_primitive_group);
				cmzn_field_destroy(&temporary_handle);
			}
		}
		if (sub_group)
		{
			cmzn_field_group_destroy(&sub_group);
		}
	}

	return (highlight_functor);
}
#endif /* defined (USE_OPENCASCADE) */

SubObjectGroupHighlightFunctor *create_highlight_functor_element(
	struct Computed_field *group_field, cmzn_mesh_id mesh)
{
  SubObjectGroupHighlightFunctor *highlight_functor = NULL;
  if (group_field)
  {
	cmzn_field_group_id sub_group = cmzn_field_cast_group(group_field);
	  if (cmzn_field_group_contains_local_region(sub_group))
	  {
		highlight_functor =	new SubObjectGroupHighlightFunctor(NULL, NULL);
		highlight_functor->setContainsAll(1);
	  }
	  else
	  {
		cmzn_field_element_group_id element_group = cmzn_field_group_get_element_group(sub_group, mesh);
			if (element_group)
			{
				Computed_field_element_group *group_core =
					Computed_field_element_group_core_cast(element_group);
				highlight_functor =
					new SubObjectGroupHighlightFunctor(group_core,
					&Computed_field_subobject_group::isIdentifierInList);
				cmzn_field_element_group_destroy(&element_group);
		}
		}
	if (sub_group)
	{
	  cmzn_field_group_destroy(&sub_group);
	}
  }

  return (highlight_functor);
}

SubObjectGroupHighlightFunctor *create_highlight_functor_nodeset(
	struct Computed_field *group_field, cmzn_nodeset_id nodeset)
{
  SubObjectGroupHighlightFunctor *highlight_functor = NULL;
	if (group_field)
	{
	  cmzn_field_group_id sub_group = cmzn_field_cast_group(group_field);
	  if (cmzn_field_group_contains_local_region(sub_group))
	  {
		highlight_functor = new SubObjectGroupHighlightFunctor(NULL, NULL);
		highlight_functor->setContainsAll(1);
	  }
	  else
	  {
		cmzn_field_node_group_id node_group = cmzn_field_group_get_node_group(sub_group, nodeset);
		if (node_group)
		{
			Computed_field_node_group *group_core =
				Computed_field_node_group_core_cast(node_group);
			highlight_functor =	new SubObjectGroupHighlightFunctor(group_core,
				&Computed_field_subobject_group::isIdentifierInList);
			cmzn_field_node_group_destroy(&node_group);
		}
	  }
	if (sub_group)
		{
		cmzn_field_group_destroy(&sub_group);
		}
	}

	return (highlight_functor);
}

int cmzn_graphic_remove_renderer_highlight_functor(struct cmzn_graphic *graphic,
	Render_graphics *renderer)
{
	if (graphic && renderer)
	{
		renderer->set_highlight_functor(NULL);
		return 1;
	}
	return 0;
}

int cmzn_graphic_set_renderer_highlight_functor(struct cmzn_graphic *graphic, Render_graphics *renderer)
{
	int return_code = 0;

		if (graphic && renderer && graphic->scene)
		{
			cmzn_field_id group_field =
				cmzn_scene_get_selection_group_private_for_highlighting(graphic->scene);
			cmzn_field_module_id field_module = NULL;
			if (group_field &&
				(NULL != (field_module = cmzn_field_get_field_module(group_field))))
			{
				if ((CMZN_GRAPHIC_SELECT_ON == graphic->select_mode) ||
					(CMZN_GRAPHIC_DRAW_SELECTED == graphic->select_mode))
				{
					SubObjectGroupHighlightFunctor *functor = 0;
					switch (graphic->domain_type)
					{
						case CMZN_FIELD_DOMAIN_POINT:
						{
							// no functor
						} break;
						case CMZN_FIELD_DOMAIN_DATA:
						case CMZN_FIELD_DOMAIN_NODES:
						{
							cmzn_nodeset_id nodeset =
								cmzn_field_module_find_nodeset_by_domain_type(field_module, graphic->domain_type);
							functor = create_highlight_functor_nodeset(group_field, nodeset);
							cmzn_nodeset_destroy(&nodeset);
						} break;
						case CMZN_FIELD_DOMAIN_MESH_1D:
						case CMZN_FIELD_DOMAIN_MESH_2D:
						case CMZN_FIELD_DOMAIN_MESH_3D:
						case CMZN_FIELD_DOMAIN_MESH_HIGHEST_DIMENSION:
						{
#if defined(USE_OPENCASCADE)
							if (graphic->graphic_type == CMZN_GRAPHIC_SURFACES)
							{
								// test here for domain of object coordinate_field
								// if it is a cad_geometry do something about it
								struct LIST(Computed_field) *domain_field_list = CREATE_LIST(Computed_field)();
								int return_code = Computed_field_get_domain( graphic->coordinate_field, domain_field_list );
								if ( return_code )
								{
									// so test for topology domain
									struct Computed_field *cad_topology_field = FIRST_OBJECT_IN_LIST_THAT(Computed_field)
										( cmzn_field_is_type_cad_topology, (void *)NULL, domain_field_list );
									if ( cad_topology_field )
									{
										cmzn_field_cad_topology_id cad_topology_domain =
											cmzn_field_cast_cad_topology(cad_topology_field);
										functor = create_highlight_functor_cad_primitive(
											group_field, cad_topology_domain);
									}
								}
								if ( domain_field_list )
									DESTROY_LIST(Computed_field)(&domain_field_list);
							}
							if (!functor)
							{
#endif // defined(USE_OPENCASCADE)
							if (graphic->graphic_type != CMZN_GRAPHIC_STREAMLINES)
							{
								int dimension = cmzn_graphic_get_domain_dimension(graphic);
								cmzn_mesh_id temp_mesh = cmzn_field_module_find_mesh_by_dimension(field_module, dimension);
								functor = create_highlight_functor_element(group_field, temp_mesh);
								cmzn_mesh_destroy(&temp_mesh);
							}
#if defined(USE_OPENCASCADE)
							}
#endif // defined(USE_OPENCASCADE)
						} break;
						default:
						{
							display_message(ERROR_MESSAGE,
								"cmzn_graphic_set_renderer_highlight_functor.  Unknown domain type");
						} break;
					}
					if (!(renderer->set_highlight_functor(functor)) && functor)
					{
						delete functor;
					}
				}
				cmzn_field_module_destroy(&field_module);
			}
			return_code = 1;
		}

	return return_code;
}

int cmzn_graphic_get_iteration_domain(cmzn_graphic_id graphic,
	cmzn_graphic_to_graphics_object_data *graphic_to_object_data)
{
	if (!graphic || !graphic_to_object_data)
		return 0;
	graphic_to_object_data->master_mesh = 0;
	graphic_to_object_data->iteration_mesh = 0;
	int dimension = cmzn_graphic_get_domain_dimension(graphic);
	if (dimension > 0)
	{
		graphic_to_object_data->master_mesh =
			cmzn_field_module_find_mesh_by_dimension(graphic_to_object_data->field_module, dimension);
		if (graphic->subgroup_field)
		{
			cmzn_field_group_id group = cmzn_field_cast_group(graphic->subgroup_field);
			if (group)
			{
				cmzn_field_element_group_id element_group = cmzn_field_group_get_element_group(group, graphic_to_object_data->master_mesh);
				if (element_group)
				{
					graphic_to_object_data->iteration_mesh =
						cmzn_mesh_group_base_cast(cmzn_field_element_group_get_mesh(element_group));
					cmzn_field_element_group_destroy(&element_group);
				}
				cmzn_field_group_destroy(&group);
			}
			else
			{
				cmzn_field_element_group_id element_group = cmzn_field_cast_element_group(graphic->subgroup_field);
				if (element_group)
				{
					// check group is for same master mesh
					graphic_to_object_data->iteration_mesh = cmzn_mesh_group_base_cast(cmzn_field_element_group_get_mesh(element_group));
					cmzn_mesh_id temp_master_mesh = cmzn_mesh_get_master(graphic_to_object_data->iteration_mesh);
					if (!cmzn_mesh_match(graphic_to_object_data->master_mesh, temp_master_mesh))
					{
						cmzn_mesh_destroy(&graphic_to_object_data->iteration_mesh);
					}
					cmzn_mesh_destroy(&temp_master_mesh);
					cmzn_field_element_group_destroy(&element_group);
				}
				else
				{
					graphic_to_object_data->iteration_mesh = cmzn_mesh_access(graphic_to_object_data->master_mesh);
				}
			}
		}
		else
		{
			graphic_to_object_data->iteration_mesh = cmzn_mesh_access(graphic_to_object_data->master_mesh);
		}
	}
	return (0 != graphic_to_object_data->iteration_mesh);
}

static char *cmzn_graphic_get_graphics_object_name(cmzn_graphic *graphic, const char *name_prefix)
{
	if (!graphic || !name_prefix)
		return 0;
	int error = 0;
	char *graphics_object_name = 0;
	append_string(&graphics_object_name, name_prefix, &error);
	if (graphic->subgroup_field)
	{
		char *subgroup_name = cmzn_field_get_name(graphic->subgroup_field);
		append_string(&graphics_object_name, subgroup_name, &error);
		append_string(&graphics_object_name, "/", &error);
		DEALLOCATE(subgroup_name);
	}
	append_string(&graphics_object_name, ".", &error);
	char temp[20];
	sprintf(temp, "%d", graphic->position);
	append_string(&graphics_object_name, temp, &error);
	if (graphic->name)
	{
		append_string(&graphics_object_name, "_", &error);
		append_string(&graphics_object_name, graphic->name, &error);
	}
	return graphics_object_name;
}

static int cmzn_mesh_to_graphics(cmzn_mesh_id mesh, cmzn_graphic_to_graphics_object_data *graphic_to_object_data)
{
	int return_code = 1;
	cmzn_element_iterator_id iterator = cmzn_mesh_create_element_iterator(mesh);
	cmzn_element_id element = 0;
	while (0 != (element = cmzn_element_iterator_next_non_access(iterator)))
	{
		if (!FE_element_to_graphics_object(element, graphic_to_object_data))
		{
			return_code = 0;
			break;
		}
	}
	cmzn_element_iterator_destroy(&iterator);
	return return_code;
}

int cmzn_graphic_to_graphics_object(
	struct cmzn_graphic *graphic,void *graphic_to_object_data_void)
{
	char *existing_name, *graphic_string;
	GLfloat time;
	enum GT_object_type graphics_object_type;
	int return_code;
	struct FE_region *fe_region;

	ENTER(cmzn_graphic_to_graphics_object);
	struct cmzn_graphic_to_graphics_object_data *graphic_to_object_data =
		reinterpret_cast<struct cmzn_graphic_to_graphics_object_data *>(graphic_to_object_data_void);
	if (graphic && graphic_to_object_data &&
		(((CMZN_FIELD_DOMAIN_DATA == graphic->domain_type) &&
			(fe_region = graphic_to_object_data->data_fe_region)) ||
			(fe_region = graphic_to_object_data->fe_region)))
	{
		int dimension = cmzn_graphic_get_domain_dimension(graphic);
		/* all primitives added at time 0.0 */
		time = 0.0;
		return_code = 1;
		/* build only if visible... */
		cmzn_graphics_filter_id filter = graphic_to_object_data->graphics_filter;
		/* build only if visible and changed */
		if ((0 == filter) || (cmzn_graphics_filter_evaluate_graphic(filter, graphic)))
		{
			if (graphic->graphics_changed)
			{
				Computed_field *coordinate_field = graphic->coordinate_field;
				if (coordinate_field ||
					(graphic->domain_type == CMZN_FIELD_DOMAIN_POINT))
				{
					/* RC coordinate_field to pass to FE_element_to_graphics_object */
					graphic_to_object_data->rc_coordinate_field = (cmzn_field_id)0;
					graphic_to_object_data->wrapper_orientation_scale_field = (cmzn_field_id)0;
					graphic_to_object_data->wrapper_stream_vector_field = (cmzn_field_id)0;
					if (coordinate_field)
					{
						graphic_to_object_data->rc_coordinate_field =
							Computed_field_begin_wrap_coordinate_field(coordinate_field);
						if (!graphic_to_object_data->rc_coordinate_field)
						{
							display_message(ERROR_MESSAGE,
								"cmzn_graphic_to_graphics_object.  Could not get rc_coordinate_field wrapper");
							return_code = 0;
						}
					}
					if (return_code && graphic->point_orientation_scale_field)
					{
						graphic_to_object_data->wrapper_orientation_scale_field =
							Computed_field_begin_wrap_orientation_scale_field(
								graphic->point_orientation_scale_field, graphic_to_object_data->rc_coordinate_field);
						if (!graphic_to_object_data->wrapper_orientation_scale_field)
						{
							display_message(ERROR_MESSAGE,
								"cmzn_graphic_to_graphics_object.  Could not get orientation_scale_field wrapper");
							return_code = 0;
						}
					}
					if (return_code && graphic->stream_vector_field)
					{
						graphic_to_object_data->wrapper_stream_vector_field =
							Computed_field_begin_wrap_orientation_scale_field(
								graphic->stream_vector_field, graphic_to_object_data->rc_coordinate_field);
						if (!graphic_to_object_data->wrapper_stream_vector_field)
						{
							display_message(ERROR_MESSAGE,
								"cmzn_graphic_to_graphics_object.  Could not get stream_vector_field wrapper");
							return_code = 0;
						}
					}
					if (return_code && graphic->glyph)
					{
						graphic_to_object_data->glyph_gt_object =
							graphic->glyph->getGraphicsObject(graphic->tessellation, graphic->material, graphic->font);
					}
					else
					{
						graphic_to_object_data->glyph_gt_object = 0;
					}
					if (return_code)
					{
#if defined (DEBUG_CODE)
						/*???debug*/
						if ((graphic_string = cmzn_graphic_string(graphic,
							GRAPHIC_STRING_COMPLETE_PLUS)) != NULL)
						{
							printf("> building %s\n", graphic_string);
							DEALLOCATE(graphic_string);
						}
#endif /* defined (DEBUG_CODE) */
						cmzn_graphic_get_top_level_number_in_xi(graphic,
							MAXIMUM_ELEMENT_XI_DIMENSIONS, graphic_to_object_data->top_level_number_in_xi);
						graphic_to_object_data->existing_graphics = 0;
						/* work out the name the graphics object is to have */
						char *graphics_object_name = cmzn_graphic_get_graphics_object_name(graphic, graphic_to_object_data->name_prefix);
						if (graphics_object_name)
						{
							if (graphic->graphics_object)
							{
								/* replace the graphics object name */
								GT_object_set_name(graphic->graphics_object,
									graphics_object_name);
								if (GT_object_has_primitives_at_time(graphic->graphics_object,
									time))
								{
#if defined (DEBUG_CODE)
									/*???debug*/printf("  EDIT EXISTING GRAPHICS!\n");
#endif /* defined (DEBUG_CODE) */
									GET_NAME(GT_object)(graphic->graphics_object, &existing_name);
									graphic_to_object_data->existing_graphics =
										CREATE(GT_object)(existing_name,
											GT_object_get_type(graphic->graphics_object),
											get_GT_object_default_material(graphic->graphics_object));
									DEALLOCATE(existing_name);
									GT_object_transfer_primitives_at_time(
										graphic_to_object_data->existing_graphics,
										graphic->graphics_object, time);
								}
							}
							else
							{
								switch (graphic->graphic_type)
								{
								case CMZN_GRAPHIC_LINES:
								{
									if (CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE == graphic->line_shape)
									{
										graphics_object_type = g_POLYLINE_VERTEX_BUFFERS;
									}
									else
									{
										graphics_object_type = g_SURFACE;
									}
								} break;
								case CMZN_GRAPHIC_SURFACES:
								{
									graphics_object_type = g_SURFACE;
								} break;
								case CMZN_GRAPHIC_CONTOURS:
								{
									switch (dimension)
									{
									case 3:
									{
										graphics_object_type = g_SURFACE; // for new isosurfaces
									} break;
									case 2:
									{
										graphics_object_type = g_POLYLINE;
									} break;
									case 1:
									{
										display_message(ERROR_MESSAGE,
											"cmzn_graphic_to_graphics_object.  "
											"Contours of 1-D elements is not supported");
										return_code = 0;
									} break;
									default:
									{
										display_message(ERROR_MESSAGE,
											"cmzn_graphic_to_graphics_object.  "
											"Invalid dimension for contours");
										return_code = 0;
									} break;
									}
								} break;
								case CMZN_GRAPHIC_POINTS:
								{
									graphics_object_type = g_GLYPH_SET;
								} break;
								case CMZN_GRAPHIC_STREAMLINES:
								{
									if (CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE == graphic->line_shape)
									{
										graphics_object_type = g_POLYLINE;
									}
									else
									{
										graphics_object_type = g_SURFACE;
									}
								} break;
								default:
								{
									display_message(ERROR_MESSAGE,
										"cmzn_graphic_to_graphics_object.  "
										"Unknown graphic type");
									return_code = 0;
								} break;
								}
								if (return_code)
								{
									graphic->graphics_object = CREATE(GT_object)(
										graphics_object_name, graphics_object_type,
										graphic->material);
									set_GT_object_render_line_width(graphic->graphics_object, graphic->render_line_width);
									set_GT_object_render_point_size(graphic->graphics_object, graphic->render_point_size);
									GT_object_set_select_mode(graphic->graphics_object,
										graphic->select_mode);
									if (graphic->secondary_material)
									{
										set_GT_object_secondary_material(graphic->graphics_object,
											graphic->secondary_material);
									}
									if (graphic->selected_material)
									{
										set_GT_object_selected_material(graphic->graphics_object,
											graphic->selected_material);
									}
								}
							}
							DEALLOCATE(graphics_object_name);
						}
						else
						{
							display_message(ERROR_MESSAGE,
								"cmzn_graphic_to_graphics_object.  "
								"Unable to make graphics object name");
							return_code = 0;
						}
						if (graphic->data_field)
						{
							graphic_to_object_data->number_of_data_values =
								Computed_field_get_number_of_components(graphic->data_field);
							ALLOCATE(graphic_to_object_data->data_copy_buffer,
								FE_value, graphic_to_object_data->number_of_data_values);
						}
						if (graphic->graphics_object)
						{
							graphic->selected_graphics_changed=1;
							/* need graphic for FE_element_to_graphics_object routine */
							graphic_to_object_data->graphic=graphic;
							cmzn_graphic_get_iteration_domain(graphic, graphic_to_object_data);
							switch (graphic->graphic_type)
							{
							case CMZN_GRAPHIC_POINTS:
							{
								switch (graphic->domain_type)
								{
								case CMZN_FIELD_DOMAIN_NODES:
								case CMZN_FIELD_DOMAIN_DATA:
								{
									// all nodes are in a single GT_glyph_set, so rebuild all even if
									// editing a single node or element
									GT_object_remove_primitives_at_time(
										graphic->graphics_object, time,
										(GT_object_primitive_object_name_conditional_function *)NULL,
										(void *)NULL);
									cmzn_nodeset_id master_nodeset = cmzn_field_module_find_nodeset_by_domain_type(
										graphic_to_object_data->field_module, graphic->domain_type);
									cmzn_nodeset_id iteration_nodeset = 0;
									if (graphic->subgroup_field)
									{
										cmzn_field_group_id group = cmzn_field_cast_group(graphic->subgroup_field);
										if (group)
										{
											cmzn_field_node_group_id node_group = cmzn_field_group_get_node_group(group, master_nodeset);
											if (node_group)
											{
												iteration_nodeset =
													cmzn_nodeset_group_base_cast(cmzn_field_node_group_get_nodeset(node_group));
												cmzn_field_node_group_destroy(&node_group);
											}
											cmzn_field_group_destroy(&group);
										}
										else
										{
											cmzn_field_node_group_id node_group = cmzn_field_cast_node_group(graphic->subgroup_field);
											if (node_group)
											{
												// check group is for same master nodeset
												iteration_nodeset = cmzn_nodeset_group_base_cast(cmzn_field_node_group_get_nodeset(node_group));
												cmzn_nodeset_id temp_master_nodeset = cmzn_nodeset_get_master(iteration_nodeset);
												if (!cmzn_nodeset_match(master_nodeset, temp_master_nodeset))
												{
													cmzn_nodeset_destroy(&iteration_nodeset);
												}
												cmzn_nodeset_destroy(&temp_master_nodeset);
												cmzn_field_node_group_destroy(&node_group);
											}
											else
											{
												iteration_nodeset = cmzn_nodeset_access(master_nodeset);
											}
										}
									}
									else
									{
										iteration_nodeset = cmzn_nodeset_access(master_nodeset);
									}
									if (iteration_nodeset)
									{
										GT_glyph_set *glyph_set = create_GT_glyph_set_from_nodeset(
											iteration_nodeset, graphic_to_object_data->field_cache,
											graphic_to_object_data->rc_coordinate_field,
											graphic_to_object_data->glyph_gt_object, graphic->glyph_repeat_mode,
											graphic->point_base_size, graphic->point_offset, graphic->point_scale_factors,
											graphic_to_object_data->time,
											graphic_to_object_data->wrapper_orientation_scale_field,
											graphic->signed_scale_field, graphic->data_field,
											graphic->font, graphic->label_field, graphic->label_offset,
											graphic->label_text, graphic->label_density_field,
											(iteration_nodeset == master_nodeset) ? graphic->subgroup_field : 0,
											graphic->select_mode, graphic_to_object_data->selection_group_field);
										/* NOT an error if no glyph_set produced == empty group */
										if (glyph_set)
										{
											if (!GT_OBJECT_ADD(GT_glyph_set)(graphic->graphics_object,
												time,glyph_set))
											{
												DESTROY(GT_glyph_set)(&glyph_set);
												return_code=0;
											}
										}
										cmzn_nodeset_destroy(&iteration_nodeset);
									}
									cmzn_nodeset_destroy(&master_nodeset);
								} break;
								case CMZN_FIELD_DOMAIN_POINT:
								{
									cmzn_graphic_to_point_object_at_time(
										graphic, graphic_to_object_data, /*graphics_object_primitive_time*/time);
								} break;
								default: // ELEMENTS
								{
									if (graphic_to_object_data->iteration_mesh)
									{
										return_code = cmzn_mesh_to_graphics(graphic_to_object_data->iteration_mesh, graphic_to_object_data);
									}
								} break;
								}
							} break;
							case CMZN_GRAPHIC_LINES:
							{
#if defined(USE_OPENCASCADE)
								// test here for domain of rc_coordinate_field
								// if it is a cad_geometry do something about it
								struct LIST(Computed_field) *domain_field_list = CREATE_LIST(Computed_field)();
								int return_code = Computed_field_get_domain( graphic_to_object_data->rc_coordinate_field, domain_field_list );
								if ( return_code )
								{
									// so test for topology domain
									struct Computed_field *cad_topology_field = FIRST_OBJECT_IN_LIST_THAT(Computed_field)
										( cmzn_field_is_type_cad_topology, (void *)NULL, domain_field_list );
									if ( cad_topology_field )
									{
										// if topology domain then draw item at location
										return_code = Cad_shape_to_graphics_object( cad_topology_field, graphic_to_object_data );
										DESTROY_LIST(Computed_field)(&domain_field_list);
										break;
									}
								}
								if ( domain_field_list )
									DESTROY_LIST(Computed_field)(&domain_field_list);
#endif /* defined(USE_OPENCASCADE) */
								if (CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE == graphic->line_shape)
								{
									GT_polyline_vertex_buffers *lines =
										CREATE(GT_polyline_vertex_buffers)(g_PLAIN);
									if (GT_OBJECT_ADD(GT_polyline_vertex_buffers)(
										graphic->graphics_object, lines))
									{
										if (graphic_to_object_data->iteration_mesh)
										{
											return_code = cmzn_mesh_to_graphics(graphic_to_object_data->iteration_mesh, graphic_to_object_data);
										}
									}
									else
									{
										//DESTROY(GT_polyline_vertex_buffers)(&lines);
										return_code = 0;
									}
								}
								else if (graphic_to_object_data->iteration_mesh)
								{
									// cylinders
									return_code = cmzn_mesh_to_graphics(graphic_to_object_data->iteration_mesh, graphic_to_object_data);
								}
							} break;
							case CMZN_GRAPHIC_SURFACES:
							{
								bool cad_surfaces = false;
#if defined(USE_OPENCASCADE)
								{
									// test here for domain of rc_coordinate_field
									// if it is a cad_geometry do something about it
									//if ( is_cad_geometry( settings_to_object_data->rc_coordinate_field->get_domain() ) )
									struct LIST(Computed_field) *domain_field_list = CREATE_LIST(Computed_field)();
									int return_code = Computed_field_get_domain( graphic_to_object_data->rc_coordinate_field, domain_field_list );
									if ( return_code )
									{
										//printf( "got domain of rc_coordinate_field (%d)\n", NUMBER_IN_LIST(Computed_field)(domain_field_list) );
										// so test for topology domain
										struct Computed_field *cad_topology_field = FIRST_OBJECT_IN_LIST_THAT(Computed_field)
											( cmzn_field_is_type_cad_topology, (void *)NULL, domain_field_list );
										if ( cad_topology_field )
										{
											cad_surfaces = true;
											//printf( "hurrah, we have a cad topology domain.\n" );
											// if topology domain then draw item at location
											return_code = Cad_shape_to_graphics_object( cad_topology_field, graphic_to_object_data );
											DESTROY_LIST(Computed_field)(&domain_field_list);
											break;
										}
									}
									if ( domain_field_list )
										DESTROY_LIST(Computed_field)(&domain_field_list);
								}
#endif /* defined(USE_OPENCASCADE) */
								if (!cad_surfaces)
								{
									if (graphic_to_object_data->iteration_mesh)
									{
										return_code = cmzn_mesh_to_graphics(graphic_to_object_data->iteration_mesh, graphic_to_object_data);
									}
								}
							} break;
							case CMZN_GRAPHIC_CONTOURS:
							{
								cmzn_field_cache_set_time(graphic_to_object_data->field_cache, graphic_to_object_data->time);
								if (0 < graphic->number_of_isovalues)
								{
									if (g_SURFACE == GT_object_get_type(graphic->graphics_object))
									{
										graphic_to_object_data->iso_surface_specification =
											Iso_surface_specification_create(
												graphic->number_of_isovalues, graphic->isovalues,
												graphic->first_isovalue, graphic->last_isovalue,
												graphic_to_object_data->rc_coordinate_field,
												graphic->data_field,
												graphic->isoscalar_field,
												graphic->texture_coordinate_field);
									}
									if (graphic_to_object_data->iteration_mesh)
									{
										return_code = cmzn_mesh_to_graphics(graphic_to_object_data->iteration_mesh, graphic_to_object_data);
									}
									if (g_SURFACE == GT_object_get_type(graphic->graphics_object))
									{
										Iso_surface_specification_destroy(&graphic_to_object_data->iso_surface_specification);
										/* Decimate */
										if (graphic->decimation_threshold > 0.0)
										{
											GT_object_decimate_GT_surface(graphic->graphics_object,
												graphic->decimation_threshold);
										}
									}
									// If the isosurface is a volume we can decimate and then normalise,
									// otherwise if it is a polyline representing a isolines, skip over.
									if (g_VOLTEX == GT_object_get_type(graphic->graphics_object))
									{
										/* Decimate */
										if (graphic->decimation_threshold > 0.0)
										{
											GT_object_decimate_GT_voltex(graphic->graphics_object,
												graphic->decimation_threshold);
										}
										/* Normalise normals now that the entire mesh has been calculated */
										GT_object_normalise_GT_voltex_normals(graphic->graphics_object);
									}
								}
							} break;
							case CMZN_GRAPHIC_STREAMLINES:
							{
								cmzn_field_cache_set_time(graphic_to_object_data->field_cache, graphic_to_object_data->time);
								// must always regenerate ALL streamlines since they can cross into other elements
								if (graphic_to_object_data->existing_graphics)
								{
									DEACCESS(GT_object)(
										&(graphic_to_object_data->existing_graphics));
								}
								if (graphic->seed_element)
								{
									return_code = FE_element_to_graphics_object(
										graphic->seed_element, graphic_to_object_data);
								}
								else if (graphic->seed_nodeset &&
									graphic->seed_node_mesh_location_field)
								{
									cmzn_node_iterator_id iterator = cmzn_nodeset_create_node_iterator(graphic->seed_nodeset);
									cmzn_node_id node = 0;
									while (0 != (node = cmzn_node_iterator_next_non_access(iterator)))
									{
										if (!cmzn_node_to_streamline(node, graphic_to_object_data))
										{
											return_code = 0;
											break;
										}
									}
									cmzn_node_iterator_destroy(&iterator);
								}
								else
								{
									if (graphic_to_object_data->iteration_mesh)
									{
										return_code = cmzn_mesh_to_graphics(graphic_to_object_data->iteration_mesh, graphic_to_object_data);
									}
								}
							} break;
							default:
							{
								return_code = 0;
							} break;
							} /* end of switch */
							cmzn_mesh_destroy(&graphic_to_object_data->iteration_mesh);
							cmzn_mesh_destroy(&graphic_to_object_data->master_mesh);
							if (return_code)
							{
								/* set the spectrum in the graphics object - if required */
								if ((graphic->data_field)||
									((CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type) &&
										(STREAM_NO_DATA != graphic->streamline_data_type)))
								{
									set_GT_object_Spectrum(graphic->graphics_object, graphic->spectrum);
								}
								/* mark display list as needing updating */
								graphic->graphics_changed = 0;
								GT_object_changed(graphic->graphics_object);
							}
							else
							{
								graphic_string = cmzn_graphic_string(graphic,
									GRAPHIC_STRING_COMPLETE_PLUS);
								display_message(ERROR_MESSAGE,
									"cmzn_graphic_to_graphics_object.  "
									"Could not build '%s'",graphic_string);
								DEALLOCATE(graphic_string);
								/* set return_code to 1, so rest of graphic can be built */
								return_code = 1;
							}
						}
						else
						{
							display_message(ERROR_MESSAGE,
								"cmzn_graphic_to_graphics_object.  "
								"Could not create graphics object");
							return_code = 0;
						}
						if (graphic_to_object_data->existing_graphics)
						{
							DEACCESS(GT_object)(&(graphic_to_object_data->existing_graphics));
						}
						if (graphic->data_field)
						{
							graphic_to_object_data->number_of_data_values = 0;
							DEALLOCATE(graphic_to_object_data->data_copy_buffer);
						}
					}
					if (graphic_to_object_data->glyph_gt_object)
					{
						DEACCESS(GT_object)(&(graphic_to_object_data->glyph_gt_object));
					}
					if (graphic->stream_vector_field)
					{
						Computed_field_end_wrap(&(graphic_to_object_data->wrapper_stream_vector_field));
					}
					if (graphic->point_orientation_scale_field)
					{
						Computed_field_end_wrap(&(graphic_to_object_data->wrapper_orientation_scale_field));
					}
					if (graphic_to_object_data->rc_coordinate_field)
					{
						Computed_field_end_wrap(&(graphic_to_object_data->rc_coordinate_field));
					}
				}
			}
			if (graphic->selected_graphics_changed)
			{
				if (graphic->graphics_object)
					GT_object_changed(graphic->graphics_object);
				graphic->selected_graphics_changed = 0;
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_to_graphics_object.  Invalid argument(s)");
		return_code = 0;
	}

	LEAVE;

	return (return_code);
}

int cmzn_graphic_compile_visible_graphic(
	struct cmzn_graphic *graphic, void *renderer_void)
{
	int return_code = 1;
	Render_graphics *renderer;

	ENTER(cmzn_graphic_compile_visible_graphic);
	if (graphic && (renderer = static_cast<Render_graphics *>(renderer_void)))
	{
		return_code = 1;
		if (graphic->graphics_object)
		{
			cmzn_graphics_filter_id filter = renderer->getGraphicsFilter();
			if ((0 == filter) || (cmzn_graphics_filter_evaluate_graphic(filter, graphic)))
			{
				cmzn_graphic_set_renderer_highlight_functor(graphic, renderer);
				return_code = renderer->Graphics_object_compile(graphic->graphics_object);
				cmzn_graphic_remove_renderer_highlight_functor(graphic, renderer);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_compile_visible_graphic.  "
			"Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_compile_visible_graphic */

int cmzn_graphic_execute_visible_graphic(
	struct cmzn_graphic *graphic, void *renderer_void)
{
	int return_code = 1;
	Render_graphics *renderer;

	ENTER(cmzn_graphic_execute_visible_graphic);
	if (graphic && (renderer = static_cast<Render_graphics *>
			(renderer_void)))
	{
		return_code = 1;
		if (graphic->graphics_object)
		{
			cmzn_graphics_filter_id filter = renderer->getGraphicsFilter();
			if ((0 == filter) || (cmzn_graphics_filter_evaluate_graphic(filter, graphic)))
			{
				if (renderer->rendering_layer(graphic->overlay_flag))
				{
					if (renderer->begin_coordinate_system(graphic->coordinate_system))
					{
#if defined (OPENGL_API)
						/* use position in list as name for GL picking */
						glLoadName((GLuint)graphic->position);
#endif /* defined (OPENGL_API) */
						return_code = renderer->Graphics_object_execute(graphic->graphics_object);
						renderer->end_coordinate_system(graphic->coordinate_system);
					}
				}
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_execute_visible_graphic.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_execute_visible_graphic */

static int cmzn_graphic_Computed_field_or_ancestor_satisfies_condition(
	struct cmzn_graphic *graphic,
	LIST_CONDITIONAL_FUNCTION(Computed_field) *conditional_function,
	void *user_data)
{
	int return_code;

	ENTER(cmzn_graphic_Computed_field_or_ancestor_satisfies_condition);
	if (graphic && conditional_function)
	{
		return_code = 0;
		/* compare geometry graphic */
		/* for all graphic types */
		if ((graphic->coordinate_field &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->coordinate_field, conditional_function, user_data)) ||
			(graphic->subgroup_field &&
				(Computed_field_or_ancestor_satisfies_condition(
					graphic->subgroup_field, conditional_function, user_data))) ||
			(graphic->tessellation_field && 
				(Computed_field_or_ancestor_satisfies_condition(
					graphic->tessellation_field, conditional_function, user_data))))
		{
			return_code = 1;
		}
		/* currently for surfaces only */
		else if (graphic->texture_coordinate_field &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->texture_coordinate_field, conditional_function, user_data))
		{
			return_code = 1;
		}
		/* line attributes */
		else if (((CMZN_GRAPHIC_LINES==graphic->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type)) &&
			graphic->line_orientation_scale_field &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->line_orientation_scale_field, conditional_function, user_data))
		{
			return_code = 1;
		}
		/* for contours only */
		else if ((CMZN_GRAPHIC_CONTOURS == graphic->graphic_type) &&
			(graphic->isoscalar_field &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->isoscalar_field, conditional_function, user_data)))
		{
			return_code = 1;
		}
		/* point attributes */
		else if ((CMZN_GRAPHIC_POINTS == graphic->graphic_type) &&
			((graphic->point_orientation_scale_field &&
				(Computed_field_or_ancestor_satisfies_condition(
					graphic->point_orientation_scale_field, conditional_function, user_data)))||
			(graphic->signed_scale_field &&
				(Computed_field_or_ancestor_satisfies_condition(
					graphic->signed_scale_field, conditional_function, user_data))) ||
			(graphic->label_field &&
				(Computed_field_or_ancestor_satisfies_condition(
					graphic->label_field, conditional_function, user_data))) ||
			(graphic->label_density_field &&
				(Computed_field_or_ancestor_satisfies_condition(
					graphic->label_density_field, conditional_function, user_data)))))
		{
			return_code = 1;
		}
		/* for graphics using a sampling density field only */
		else if (((CMZN_GRAPHIC_POINTS == graphic->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type)) &&
			(CMZN_ELEMENT_POINT_SAMPLE_CELL_POISSON == graphic->sample_mode) &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->sample_density_field, conditional_function, user_data))
		{
			return_code = 1;
		}
		/* for streamlines only */
		else if ((CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type) &&
			graphic->stream_vector_field &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->stream_vector_field, conditional_function, user_data))
		{
			return_code = 1;
		}
		/* appearance graphic for all graphic types */
		else if (graphic->data_field &&
			Computed_field_or_ancestor_satisfies_condition(
				graphic->data_field, conditional_function, user_data))
		{
			return_code = 1;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_Computed_field_or_ancestor_satisfies_condition.  "
			"Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_Computed_field_or_ancestor_satisfies_condition */

static int cmzn_graphic_uses_changed_FE_field(
	struct cmzn_graphic *graphic,
	struct CHANGE_LOG(FE_field) *fe_field_change_log)
{
	if (graphic && fe_field_change_log)
	{
		return cmzn_graphic_Computed_field_or_ancestor_satisfies_condition(
			graphic, Computed_field_contains_changed_FE_field, (void *)fe_field_change_log);
	}
	return 0;
}

int cmzn_graphic_Computed_field_change(
	struct cmzn_graphic *graphic, void *change_data_void)
{
	int return_code = 1;
	struct cmzn_graphic_Computed_field_change_data *change_data;

	ENTER(cmzn_graphic_Computed_field_change);
	if (graphic && (change_data =
		(struct cmzn_graphic_Computed_field_change_data *)change_data_void))
	{
		if (change_data->changed_field_list && cmzn_graphic_Computed_field_or_ancestor_satisfies_condition(
			graphic, Computed_field_is_in_list, (void *)change_data->changed_field_list))
		{
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		if (change_data->selection_changed && graphic->graphics_object &&
			(CMZN_GRAPHIC_STREAMLINES != graphic->graphic_type))
		{
			cmzn_graphic_update_selected(graphic, (void *)NULL);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_Computed_field_change.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_Computed_field_change */

int cmzn_graphic_get_visible_graphics_object_range(
	struct cmzn_graphic *graphic,void *graphic_range_void)
{
	int return_code = 1;
	struct cmzn_graphic_range *graphic_range =
		(struct cmzn_graphic_range *)graphic_range_void;

	ENTER(cmzn_graphic_get_visible_graphics_object_range);

	if (graphic && graphic_range && graphic_range->graphics_object_range)
	{
		if (graphic->graphics_object &&
			(graphic->coordinate_system == graphic_range->coordinate_system))
		{
			if ((0 == graphic_range->filter) ||
				(cmzn_graphics_filter_evaluate_graphic(graphic_range->filter, graphic)))
			{
				return_code=get_graphics_object_range(graphic->graphics_object,
					(void *)graphic_range->graphics_object_range);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_visible_graphics_object_range.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_get_visible_graphics_object_range */

struct GT_object *cmzn_graphic_get_graphics_object(
	struct cmzn_graphic *graphic)
{
	struct GT_object *graphics_object

	ENTER(cmzn_graphic_get_graphics_object);
	if (graphic)
	{
		graphics_object=graphic->graphics_object;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_graphics_object.  Invalid argument(s)");
		graphics_object=(struct GT_object *)NULL;
	}
	LEAVE;

	return (graphics_object);
} /* cmzn_graphic_get_graphics_object */

enum cmzn_graphic_select_mode cmzn_graphic_get_select_mode(
	cmzn_graphic_id graphic)
{
	if (graphic)
		return graphic->select_mode;
	return CMZN_GRAPHIC_SELECT_MODE_INVALID;
}

int cmzn_graphic_set_select_mode(cmzn_graphic_id graphic,
	enum cmzn_graphic_select_mode select_mode)
{
	if (graphic && (0 != ENUMERATOR_STRING(cmzn_graphic_select_mode)(select_mode)))
	{
		if (select_mode != graphic->select_mode)
		{
			graphic->select_mode = select_mode;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_spectrum_id cmzn_graphic_get_spectrum(cmzn_graphic_id graphic)
{
	cmzn_spectrum_id spectrum = 0;
	if (graphic)
	{
		if (graphic->spectrum)
		{
			spectrum = ACCESS(Spectrum)(graphic->spectrum);
		}
	}
	return spectrum;
}

int cmzn_graphic_set_spectrum(cmzn_graphic_id graphic,
	cmzn_spectrum_id spectrum)
{
	int return_code = 0;
	if (graphic)
	{
		if (spectrum != graphic->spectrum)
		{
			REACCESS(Spectrum)(&(graphic->spectrum), spectrum);
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return_code = 1;
	}
	return return_code;
}

enum Streamline_data_type cmzn_graphic_get_streamline_data_type(
	cmzn_graphic_id graphic)
{
	if (graphic && (CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type))
	{
		return graphic->streamline_data_type;
	}
	return STREAM_DATA_INVALID;
}

int cmzn_graphic_set_streamline_data_type(cmzn_graphic_id graphic,
	enum Streamline_data_type streamline_data_type)
{
	int return_code = 0;
	if (graphic && (CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type))
	{
		if (streamline_data_type != graphic->streamline_data_type)
		{
			graphic->streamline_data_type = streamline_data_type;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return_code = 1;
	}
	return return_code;
}

int cmzn_graphic_copy_without_graphics_object(
	struct cmzn_graphic *destination, struct cmzn_graphic *source)
{
	int return_code;

	ENTER(cmzn_graphic_copy_without_graphics_object);
	if (destination && source && (destination != source))
	{
		return_code = 1;
		destination->position = source->position;

		if (destination->name)
		{
			DEALLOCATE(destination->name);
			destination->name = 0;
		}
		if (source->name)
		{
			destination->name = duplicate_string(source->name);
		}

		/* copy geometry graphic */
		/* for all graphic types */
		destination->graphic_type=source->graphic_type;
		destination->domain_type = source->domain_type;
		destination->coordinate_system=source->coordinate_system;
		REACCESS(Computed_field)(&(destination->coordinate_field),
			source->coordinate_field);
		destination->select_mode=source->select_mode;
		/* for surfaces only at the moment */
		REACCESS(Computed_field)(&(destination->texture_coordinate_field),
			source->texture_coordinate_field);
		/* for 1-D and 2-D elements only */
		destination->exterior=source->exterior;
		destination->face=source->face;
		/* overlay_flag */
		destination->overlay_flag = source->overlay_flag;
		destination->overlay_order = source->overlay_order;

		/* line attributes */
		destination->line_shape = source->line_shape;
		if ((CMZN_GRAPHIC_LINES == source->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES == source->graphic_type))
		{
			REACCESS(Computed_field)(&destination->line_orientation_scale_field,
				source->line_orientation_scale_field);
			for (int i = 0; i < 2; i++)
			{
				destination->line_base_size[i] = source->line_base_size[i];
				destination->line_scale_factors[i] = source->line_scale_factors[i];
			}
		}
		else if (destination->line_orientation_scale_field)
		{
			DEACCESS(Computed_field)(&destination->line_orientation_scale_field);
		}

		cmzn_graphic_contours_id contours_graphic = cmzn_graphic_cast_contours(destination);
		if (contours_graphic)
		{
			cmzn_graphic_contours_set_isoscalar_field(contours_graphic, source->isoscalar_field);
			if (source->isovalues)
			{
				cmzn_graphic_contours_set_list_isovalues(contours_graphic, source->number_of_isovalues,
					source->isovalues);
			}
			else
			{
				cmzn_graphic_contours_set_range_isovalues(contours_graphic, source->number_of_isovalues,
					source->first_isovalue, source->last_isovalue);
			}
			cmzn_graphic_contours_set_decimation_threshold(contours_graphic, source->decimation_threshold);
			cmzn_graphic_contours_destroy(&contours_graphic);
		}
		else
		{
			if (destination->isoscalar_field)
			{
				DEACCESS(Computed_field)(&destination->isoscalar_field);
			}
			if (destination->isovalues)
			{
				DEALLOCATE(destination->isovalues);
				destination->isovalues = 0;
			}
			destination->number_of_isovalues = 0;
		}

		cmzn_graphic_point_attributes_id point_attributes =
			cmzn_graphic_get_point_attributes(destination);
		if (point_attributes)
		{
			cmzn_graphic_point_attributes_set_glyph(point_attributes, reinterpret_cast<cmzn_glyph*>(source->glyph));
			destination->glyph_repeat_mode = source->glyph_repeat_mode;
			for (int i = 0; i < 3; i++)
			{
				destination->point_base_size[i] = source->point_base_size[i];
				destination->point_offset[i] = source->point_offset[i];
				destination->point_scale_factors[i] = source->point_scale_factors[i];
				destination->label_offset[i] = source->label_offset[i];
				if (destination->label_text[i])
				{
					DEALLOCATE(destination->label_text[i]);
					destination->label_text[i] = 0;
				}
				if (source->label_text[i])
				{
					destination->label_text[i] = duplicate_string(source->label_text[i]);
				}
			}
		}
		else
		{
			if (destination->glyph)
			{
				cmzn_glyph_destroy(&(destination->glyph));
			}
		}
		REACCESS(Computed_field)(&(destination->point_orientation_scale_field),
			source->point_orientation_scale_field);
		REACCESS(Computed_field)(&(destination->signed_scale_field), source->signed_scale_field);
		REACCESS(Computed_field)(&(destination->label_field),source->label_field);
		REACCESS(Computed_field)(&(destination->subgroup_field),source->subgroup_field);
		cmzn_graphic_point_attributes_destroy(&point_attributes);

		destination->overlay_flag = source->overlay_flag;
		destination->overlay_order = source->overlay_order;

		// for element sampling: element points, streamlines
		destination->sample_mode=source->sample_mode;
		REACCESS(Computed_field)(&(destination->sample_density_field),
			source->sample_density_field);
		for (int i = 0; i < 3; i++)
		{
			destination->sample_location[i] = source->sample_location[i];
		}

		// for tessellating and sampling elements
		REACCESS(cmzn_tessellation)(&(destination->tessellation),
			source->tessellation);
		REACCESS(Computed_field)(&(destination->tessellation_field),
			source->tessellation_field);
		REACCESS(Computed_field)(&(destination->label_density_field),source->label_density_field);
		/* for graphic starting in a particular element */
		REACCESS(FE_element)(&(destination->seed_element),
			source->seed_element);
		/* for streamlines only */
		REACCESS(Computed_field)(&(destination->stream_vector_field),
			source->stream_vector_field);
		destination->streamlines_track_direction = source->streamlines_track_direction;
		destination->streamline_length=source->streamline_length;
		if (destination->seed_nodeset)
		{
			cmzn_nodeset_destroy(&destination->seed_nodeset);
		}
		if (source->seed_nodeset)
		{
			destination->seed_nodeset = cmzn_nodeset_access(source->seed_nodeset);
		}
		REACCESS(Computed_field)(&(destination->seed_node_mesh_location_field),
			source->seed_node_mesh_location_field);

		/* copy appearance graphic */
		/* for all graphic types */
		destination->visibility_flag = source->visibility_flag;
		destination->render_line_width = source->render_line_width;
		destination->render_point_size = source->render_point_size;
		REACCESS(Graphical_material)(&(destination->material),source->material);
		REACCESS(Graphical_material)(&(destination->secondary_material),
			source->secondary_material);
		cmzn_graphic_set_render_polygon_mode(destination,source->render_polygon_mode);
		REACCESS(Computed_field)(&(destination->data_field), source->data_field);
		REACCESS(Spectrum)(&(destination->spectrum), source->spectrum);
		destination->streamline_data_type = source->streamline_data_type;
		REACCESS(Graphical_material)(&(destination->selected_material),
			source->selected_material);
		destination->autorange_spectrum_flag = source->autorange_spectrum_flag;
		REACCESS(cmzn_font)(&(destination->font), source->font);

		/* ensure destination graphics object is cleared */
		REACCESS(GT_object)(&(destination->graphics_object),
			(struct GT_object *)NULL);
		destination->graphics_changed = 1;
		destination->selected_graphics_changed = 1;

		if (!return_code)
		{
			display_message(ERROR_MESSAGE,"cmzn_graphic_copy_without_graphics_object.  "
				"Error copying graphic");
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,"cmzn_graphic_copy_without_graphics_object.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmiss_graphic_copy_without_graphics_object */

int cmzn_graphic_has_name(struct cmzn_graphic *graphic,
	void *name_void)
{
	char *name, temp_name[30];
	int return_code;

	ENTER(cmzn_graphic_has_name);
	if (graphic && (name=(char *)name_void))
	{
		return_code = 0;
		if (graphic->name)
		{
			return_code=!strcmp(name,graphic->name);
		}
		if (!return_code)
		{
			/* Compare with number if the graphic
			 has no name or the name didn't match */
			sprintf(temp_name, "%d", graphic->position);
			return_code=!strcmp(name,temp_name);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmiss_graphic_has_name.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_has_name */

static int FE_element_as_graphics_name_is_removed_or_modified(
	int graphics_name, void *data_void)
{
	int return_code;
	struct CM_element_information cm;
	struct FE_element *element;
	struct cmzn_graphic_FE_region_change_data *data;

	ENTER(FE_element_as_graphics_name_is_removed_or_modified);
	return_code = 0;
	if (NULL != (data = (struct cmzn_graphic_FE_region_change_data *)data_void))
	{
		cm.number = graphics_name;
		if (data->element_type == 1)
		{
			cm.type = CM_LINE;
		}
		else if (data->element_type == 2)
		{
			cm.type = CM_FACE;
		}
		else
		{
			cm.type = CM_ELEMENT;
		}
		if (NULL != (element = FE_region_get_FE_element_from_identifier_deprecated(data->fe_region,
					&cm)))
		{
			return_code = FE_element_or_parent_changed(element,
				data->fe_element_changes, data->fe_node_changes);
		}
		else
		{
			/* must have been removed or never in FE_region */
			return_code = 1;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"FE_element_as_graphics_name_is_removed_or_modified.  "
			"Invalid argument(s)");
	}
	LEAVE;

	return (return_code);
} /* FE_element_as_graphics_name_is_removed_or_modified */

int cmzn_graphic_FE_region_change(
	struct cmzn_graphic *graphic, void *data_void)
{
	int fe_field_related_object_change, return_code;
	struct cmzn_graphic_FE_region_change_data *data;

	ENTER(cmzn_graphic_FE_region_change);
	if (graphic &&
		(data = (struct cmzn_graphic_FE_region_change_data *)data_void))
	{
		if (graphic->graphics_object)
		{
			// CMZN_FIELD_DOMAIN_DATA is handled by cmzn_graphic_data_FE_region_change
			if (graphic->domain_type == CMZN_FIELD_DOMAIN_NODES)
			{
				/* must always rebuild if identifiers changed */
				if ((data->fe_node_change_summary &
					CHANGE_LOG_OBJECT_IDENTIFIER_CHANGED(FE_node)) ||
					(cmzn_graphic_uses_changed_FE_field(graphic,
						data->fe_field_changes) && (
							(data->fe_field_change_summary & (
								CHANGE_LOG_OBJECT_IDENTIFIER_CHANGED(FE_field) |
								CHANGE_LOG_OBJECT_NOT_IDENTIFIER_CHANGED(FE_field))) ||
							((data->fe_field_change_summary &
								CHANGE_LOG_RELATED_OBJECT_CHANGED(FE_field)) &&
								(0 < data->number_of_fe_node_changes)))))
				{
					/* currently node points are always rebuilt from scratch */
					cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
				}
			}
			else if (0 < cmzn_graphic_get_domain_dimension(graphic))
			{
				fe_field_related_object_change =
					CHANGE_LOG_OBJECT_UNCHANGED(FE_field);
				/* must always rebuild if identifiers changed */
				bool element_identifier_change = false;
				int number_of_element_changes_all_dimensions = 0;
				for (int dim = 0; dim < MAXIMUM_ELEMENT_XI_DIMENSIONS; dim++)
				{
					if (data->fe_element_change_summary[dim] &
						CHANGE_LOG_OBJECT_IDENTIFIER_CHANGED(FE_element))
					{
						element_identifier_change = true;
					}
					number_of_element_changes_all_dimensions +=
						data->number_of_fe_element_changes[dim];
				}
				if (element_identifier_change ||
					(cmzn_graphic_uses_changed_FE_field(graphic,
						data->fe_field_changes) && (
							(data->fe_field_change_summary & (
								CHANGE_LOG_OBJECT_IDENTIFIER_CHANGED(FE_field) |
								CHANGE_LOG_OBJECT_NOT_IDENTIFIER_CHANGED(FE_field))) ||
							(fe_field_related_object_change = (
								(data->fe_field_change_summary &
									CHANGE_LOG_RELATED_OBJECT_CHANGED(FE_field)) && (
								(0 < data->number_of_fe_node_changes) ||
								(0 < number_of_element_changes_all_dimensions)))))))
				{
					if (fe_field_related_object_change && (
						((data->number_of_fe_node_changes*2) <
							FE_region_get_number_of_FE_nodes(data->fe_region)) &&
						((number_of_element_changes_all_dimensions*4) <
							FE_region_get_number_of_FE_elements_all_dimensions(data->fe_region))))
					{
						data->element_type = cmzn_graphic_get_domain_dimension(graphic);
						/* partial rebuild for few node/element field changes */
						GT_object_remove_primitives_at_time(graphic->graphics_object,
							(GLfloat)data->time, FE_element_as_graphics_name_is_removed_or_modified,
							data_void);
						cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_PARTIAL_REBUILD);
					}
					else
					{
						/* full rebuild for changed identifiers, FE_field definition
								changes or many node/element field changes */
						cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
					}
				}
			}
		}
		else
		{
			/* Graphics have definitely changed as they have not been built yet */
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_REDRAW);
		}
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_FE_region_change.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_FE_region_change */

int cmzn_graphic_data_FE_region_change(
	struct cmzn_graphic *graphic, void *data_void)
{
	int return_code;
	struct cmzn_graphic_FE_region_change_data *data;

	ENTER(cmzn_graphic_data_FE_region_change);
	if (graphic &&
		(data = (struct cmzn_graphic_FE_region_change_data *)data_void))
	{
		if (graphic->graphics_object)
		{
			if (graphic->domain_type == CMZN_FIELD_DOMAIN_DATA)
			{
				// must ensure changes to fields on host elements/nodes force
				// data_points to be rebuilt if using embedded fields referencing them:
				if (((0 < data->number_of_fe_node_changes) ||
					(data->fe_field_change_summary & (
						CHANGE_LOG_OBJECT_NOT_IDENTIFIER_CHANGED(FE_field) |
						CHANGE_LOG_RELATED_OBJECT_CHANGED(FE_field)))) &&
					cmzn_graphic_uses_changed_FE_field(graphic,
						data->fe_field_changes))
				{
					cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
				}
			}
		}
		else
		{
			/* Graphics have definitely changed as they have not been built yet */
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_REDRAW);
		}
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_data_FE_region_change.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_data_FE_region_change */

/**
 * cmzn_graphic list conditional function returning 1 iff the two
 * graphic have the same geometry and the same nontrivial appearance
 * characteristics. Trivial appearance characteristics can be updated in the
 * graphics object without recalculating it, and include material, spectrum,
 * glyph scalings etc.
 */
int cmzn_graphic_same_non_trivial(cmzn_graphic *graphic,
	cmzn_graphic *second_graphic)
{
	int i, return_code;

	ENTER(cmzn_graphic_same_non_trivial);
	if (graphic && second_graphic)
	{
		return_code=1;

		/* compare geometry graphic */
		/* for all graphic types */
		if (return_code)
		{
			/* note: different if names are different */
			return_code =
				(graphic->graphic_type == second_graphic->graphic_type) &&
				(graphic->domain_type == second_graphic->domain_type) &&
				(graphic->coordinate_field == second_graphic->coordinate_field) &&
				(graphic->subgroup_field == second_graphic->subgroup_field) &&
				((graphic->name == second_graphic->name) ||
					((graphic->name) && (second_graphic->name) &&
					(0 == strcmp(graphic->name, second_graphic->name)))) &&
				(graphic->select_mode == second_graphic->select_mode);
		}

		const int domain_dimension = cmzn_graphic_get_domain_dimension(graphic);

		/* for 1-D and 2-D elements only */
		if (return_code)
		{
			if ((1 == domain_dimension) || (2 == domain_dimension))
			{
				return_code =
					(graphic->exterior == second_graphic->exterior) &&
					(graphic->face == second_graphic->face);
			}
		}

		/* line attributes */
		if (return_code && (
			(CMZN_GRAPHIC_LINES==graphic->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type)))
		{
			if ((graphic->line_shape != second_graphic->line_shape) ||
				(graphic->line_orientation_scale_field !=
					second_graphic->line_orientation_scale_field))
			{
				return_code = 0;
			}
			else
			{
				for (int i = 0; i < 2; i++)
				{
					if ((graphic->line_base_size[i] != second_graphic->line_base_size[i]) ||
						(graphic->line_scale_factors[i] != second_graphic->line_scale_factors[i]))
					{
						return_code = 0;
					}
				}
			}
		}

		/* for iso_surfaces only */
		if (return_code&&
			(CMZN_GRAPHIC_CONTOURS==graphic->graphic_type))
		{
			return_code=(graphic->number_of_isovalues==
				second_graphic->number_of_isovalues)&&
				(graphic->decimation_threshold==second_graphic->decimation_threshold)&&
				(graphic->isoscalar_field==second_graphic->isoscalar_field);
			if (return_code)
			{
				if (graphic->isovalues)
				{
					if (second_graphic->isovalues)
					{
						i = 0;
						while (return_code && (i < graphic->number_of_isovalues))
						{
							if (graphic->isovalues[i] != second_graphic->isovalues[i])
							{
								return_code = 0;
							}
							i++;
						}
					}
					else
					{
						return_code = 0;
					}
				}
				else
				{
					if (second_graphic->isovalues)
					{
						return_code = 0;
					}
					else
					{
						return_code =
							(graphic->first_isovalue == second_graphic->first_isovalue)
							&& (graphic->last_isovalue == second_graphic->last_isovalue);
					}
				}
			}
		}

		if (return_code && (CMZN_GRAPHIC_POINTS == graphic->graphic_type))
		{
			return_code=
				(graphic->point_orientation_scale_field==
					second_graphic->point_orientation_scale_field)&&
				(graphic->signed_scale_field==
					second_graphic->signed_scale_field)&&
				(graphic->label_field==second_graphic->label_field)&&
				(graphic->label_density_field==second_graphic->label_density_field);
		}

		if (return_code)
		{
			return_code =
				(graphic->tessellation == second_graphic->tessellation) &&
				(graphic->tessellation_field == second_graphic->tessellation_field);
		}

		// for element sampling: element points, streamlines
		if (return_code && (0 < domain_dimension) && (
			(CMZN_GRAPHIC_POINTS == graphic->graphic_type) ||
			(CMZN_GRAPHIC_STREAMLINES == graphic->graphic_type)))
		{
			return_code = (graphic->sample_mode == second_graphic->sample_mode) &&
				((graphic->sample_mode != CMZN_ELEMENT_POINT_SAMPLE_CELL_POISSON) ||
					(graphic->sample_density_field == second_graphic->sample_density_field)) &&
				((graphic->sample_mode != CMZN_ELEMENT_POINT_SAMPLE_SET_LOCATION) ||
					((graphic->sample_location[0] == second_graphic->sample_location[0]) &&
					 (graphic->sample_location[1] == second_graphic->sample_location[1]) &&
					 (graphic->sample_location[2] == second_graphic->sample_location[2])));
		}
		/* for graphic starting in a particular element */
		if (return_code&&(CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type))
		{
			return_code=
				(graphic->seed_element==second_graphic->seed_element);
		}
		/* for streamlines only */
		if (return_code&&(CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type))
		{
			return_code=
				(graphic->stream_vector_field==second_graphic->stream_vector_field)&&
				(graphic->streamlines_track_direction == second_graphic->streamlines_track_direction) &&
				(graphic->streamline_length==second_graphic->streamline_length)&&
				(((graphic->seed_nodeset==0) && (second_graphic->seed_nodeset==0)) ||
					((graphic->seed_nodeset) && (second_graphic->seed_nodeset) &&
						cmzn_nodeset_match(graphic->seed_nodeset, second_graphic->seed_nodeset)))&&
				(graphic->seed_node_mesh_location_field==second_graphic->seed_node_mesh_location_field);
		}

		if (return_code)
		{
			return_code =
				(graphic->data_field==second_graphic->data_field)&&
				(graphic->texture_coordinate_field==second_graphic->texture_coordinate_field)&&
				((CMZN_GRAPHIC_STREAMLINES != graphic->graphic_type) ||
				 (graphic->streamline_data_type==second_graphic->streamline_data_type));
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_same_non_trivial.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
}

/**
 * Same as cmzn_graphic_same_non_trivial except <graphic> must also have
 * a graphics_object. Used for getting graphics objects from previous graphic
 * that are the same except for trivial differences such as the material and
 * spectrum which can be changed in the graphics object to match the new graphic .
 */
int cmzn_graphic_same_non_trivial_with_graphics_object(
	struct cmzn_graphic *graphic,void *second_graphic_void)
{
	int return_code;

	ENTER(cmzn_graphic_same_non_trivial_with_graphics_object);
	if (graphic)
	{
		return_code=graphic->graphics_object&&
			cmzn_graphic_same_non_trivial(graphic,reinterpret_cast<cmzn_graphic*>(second_graphic_void));
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_same_non_trivial_with_graphics_object.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
}

int cmzn_graphic_match(struct cmzn_graphic *graphic1,
	struct cmzn_graphic *graphic2)
{
	int return_code;

	ENTER(cmzn_graphic_match);
	if (graphic1 && graphic2)
	{
		return
			cmzn_graphic_same_non_trivial(graphic1, graphic2) &&
			(graphic1->visibility_flag == graphic2->visibility_flag) &&
			(graphic1->material == graphic2->material) &&
			(graphic1->secondary_material == graphic2->secondary_material) &&
			(graphic1->render_line_width == graphic2->render_line_width) &&
			(graphic1->render_point_size == graphic2->render_point_size) &&
			(graphic1->selected_material == graphic2->selected_material) &&
			(graphic1->spectrum == graphic2->spectrum) &&
			(graphic1->font == graphic2->font) &&
			(graphic1->render_polygon_mode == graphic2->render_polygon_mode) &&
			((CMZN_GRAPHIC_POINTS != graphic1->graphic_type) || (
				(graphic1->glyph == graphic2->glyph) &&
				(graphic1->glyph_repeat_mode == graphic2->glyph_repeat_mode) &&
				(graphic1->point_base_size[0] == graphic2->point_base_size[0]) &&
				(graphic1->point_base_size[1] == graphic2->point_base_size[1]) &&
				(graphic1->point_base_size[2] == graphic2->point_base_size[2]) &&
				(graphic1->point_scale_factors[0] == graphic2->point_scale_factors[0]) &&
				(graphic1->point_scale_factors[1] == graphic2->point_scale_factors[1]) &&
				(graphic1->point_scale_factors[2] == graphic2->point_scale_factors[2]) &&
				(graphic1->point_offset[0] == graphic2->point_offset[0]) &&
				(graphic1->point_offset[1] == graphic2->point_offset[1]) &&
				(graphic1->point_offset[2] == graphic2->point_offset[2]) &&
				(graphic1->label_offset[0] == graphic2->label_offset[0]) &&
				(graphic1->label_offset[1] == graphic2->label_offset[1]) &&
				(graphic1->label_offset[2] == graphic2->label_offset[2]) &&
				labels_match(graphic1->label_text[0], graphic2->label_text[0]) &&
				labels_match(graphic1->label_text[1], graphic2->label_text[1]) &&
				labels_match(graphic1->label_text[2], graphic2->label_text[2])));
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_match.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_match */

int cmzn_graphic_same_name(struct cmzn_graphic *graphic,
	void *name_void)
{
	int return_code = 0;
	char *name;

	if (graphic && graphic->name && (NULL != (name =(char *)name_void)))
	{
		return_code = (0==strcmp(graphic->name, name));
	}

	return (return_code);
}

int cmzn_graphic_list_contents(struct cmzn_graphic *graphic,
	void *list_data_void)
{
	int return_code;
	char *graphic_string,line[40];
	struct cmzn_graphic_list_data *list_data;

	ENTER(cmzn_graphic_list_contents);
	if (graphic&&
		NULL != (list_data=(struct cmzn_graphic_list_data *)list_data_void))
	{
		if (NULL != (graphic_string=cmzn_graphic_string(graphic,
					list_data->graphic_string_detail)))
		{
			if (list_data->line_prefix)
			{
				display_message(INFORMATION_MESSAGE,list_data->line_prefix);
			}
			display_message(INFORMATION_MESSAGE,graphic_string);
			if (list_data->line_suffix)
			{
				display_message(INFORMATION_MESSAGE,list_data->line_suffix);
			}
			/*???RC temp */
			if ((GRAPHIC_STRING_COMPLETE_PLUS==list_data->graphic_string_detail)&&
				(graphic->access_count != 1))
			{
				sprintf(line," (access count = %i)",graphic->access_count);
				display_message(INFORMATION_MESSAGE,line);
			}
			display_message(INFORMATION_MESSAGE,"\n");
			DEALLOCATE(graphic_string);
			return_code=1;
		}
		else
		{
			return_code=0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_list_contents.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

			return (return_code);
} /* cmzn_graphic_list_contents */

int cmzn_graphic_get_position_in_list(
	struct cmzn_graphic *graphic,
	struct LIST(cmzn_graphic) *list_of_graphic)
{
	int position;

	ENTER(cmzn_graphic_get_position_in_list);
	if (graphic&&list_of_graphic)
	{
		if (IS_OBJECT_IN_LIST(cmzn_graphic)(graphic,list_of_graphic))
		{
			position=graphic->position;
		}
		else
		{
			position=0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_position_in_list.  Invalid argument(s)");
		position=0;
	}
	LEAVE;

	return (position);
} /* cmzn_graphic_get_position_in_list */

int cmzn_graphic_copy_and_put_in_list(
	struct cmzn_graphic *graphic,void *list_of_graphic_void)
{
	int return_code;
	struct cmzn_graphic *copy_graphic;
	struct LIST(cmzn_graphic) *list_of_graphic;

	ENTER(cmzn_graphic_copy_and_put_in_list);
	if (graphic&&NULL != (list_of_graphic=
		(struct LIST(cmzn_graphic) *)list_of_graphic_void))
	{
		/* create new graphic to take the copy */
		if (NULL != (copy_graphic=CREATE(cmzn_graphic)(graphic->graphic_type)))
		{
			/* copy and insert in list */
			if (!(return_code=cmzn_graphic_copy_without_graphics_object(
				copy_graphic,graphic)&&
				ADD_OBJECT_TO_LIST(cmzn_graphic)(copy_graphic,
					list_of_graphic)))
			{
				display_message(ERROR_MESSAGE,
					"cmzn_graphic_copy_and_put_in_list.  "
					"Could not put copy in list");
			}
			DEACCESS(cmzn_graphic)(&copy_graphic);
		}
		else
		{
			display_message(ERROR_MESSAGE,
				"cmzn_graphic_copy_and_put_in_list.  Could not create copy");
			return_code=0;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_copy_and_put_in_list.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_copy_and_put_in_list */

int cmzn_graphic_type_matches(struct cmzn_graphic *graphic,
	void *graphic_type_void)
{
	int return_code;

	ENTER(cmzn_graphic_type_matches);
	if (graphic)
	{
		return_code=((void *)graphic->graphic_type == graphic_type_void);
	}
	else
	{
		display_message(ERROR_MESSAGE,"cmzn_graphic_type_matches.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_type_matches */

/***************************************************************************//**
 * If <graphic> does not already have a graphics object, this function attempts
 * to find graphic in <list_of_graphic> which differ only trivially in material,
 * spectrum etc. AND have a graphics object. If such a graphic is found, the
 * graphics_object is moved from the matching graphic and put in <graphic>, while
 * any trivial differences are fixed up in the graphics_obejct.
 */
int cmzn_graphic_extract_graphics_object_from_list(
	struct cmzn_graphic *graphic,void *list_of_graphic_void)
{
	int return_code;
	struct cmzn_graphic *matching_graphic;
	struct LIST(cmzn_graphic) *list_of_graphic;

	ENTER(cmzn_graphic_extract_graphics_object_from_list);
	if (graphic&&(list_of_graphic=
		(struct LIST(cmzn_graphic) *)list_of_graphic_void))
	{
		return_code = 1;
		if (!(graphic->graphics_object))
		{
			if (NULL != (matching_graphic = FIRST_OBJECT_IN_LIST_THAT(cmzn_graphic)(
				cmzn_graphic_same_non_trivial_with_graphics_object,
				(void *)graphic,list_of_graphic)))
			{
				/* make sure graphics_changed and selected_graphics_changed flags
					 are brought across */
				graphic->graphics_object = matching_graphic->graphics_object;
				/* make sure graphic and graphics object have same material and
					 spectrum */
				cmzn_graphic_update_graphics_object_trivial(graphic);
				graphic->graphics_changed = matching_graphic->graphics_changed;
				graphic->selected_graphics_changed =
					matching_graphic->selected_graphics_changed;
				/* reset graphics_object and flags in matching_graphic */
				matching_graphic->graphics_object = (struct GT_object *)NULL;
				//cmzn_graphic_changed(matching_graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_extract_graphics_object_from_list.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_extract_graphics_object_from_list */

enum cmzn_graphic_render_polygon_mode cmzn_graphic_get_render_polygon_mode(
	struct cmzn_graphic *graphic)
{
	if (graphic)
		return graphic->render_polygon_mode;
	return CMZN_GRAPHIC_RENDER_POLYGON_MODE_INVALID;
}

int cmzn_graphic_set_render_polygon_mode(cmzn_graphic_id graphic,
	enum cmzn_graphic_render_polygon_mode render_polygon_mode)
{
	if (graphic && (0 != ENUMERATOR_STRING(cmzn_graphic_render_polygon_mode)(render_polygon_mode)))
	{
		if (graphic->render_polygon_mode != render_polygon_mode)
		{
			graphic->render_polygon_mode = render_polygon_mode;
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_get_subgroup_field(cmzn_graphic_id graphic)
{
	if (graphic && graphic->subgroup_field)
	{
		return ACCESS(Computed_field)(graphic->subgroup_field);
	}
	return 0;
}

int cmzn_graphic_set_subgroup_field(
	struct cmzn_graphic *graphic, struct Computed_field *subgroup_field)
{
	if (graphic && ((0 == subgroup_field) ||
		Computed_field_is_scalar(subgroup_field, (void*)0)))
	{
		if (subgroup_field != graphic->subgroup_field)
		{
			REACCESS(Computed_field)(&(graphic->subgroup_field), subgroup_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_tessellation_id cmzn_graphic_get_tessellation(
	cmzn_graphic_id graphic)
{
	if (graphic && graphic->tessellation)
	{
		return ACCESS(cmzn_tessellation)(graphic->tessellation);
	}
	return 0;
}

int cmzn_graphic_set_tessellation(
	cmzn_graphic_id graphic, struct cmzn_tessellation *tessellation)
{
	if (graphic && tessellation)
	{
		if (tessellation != graphic->tessellation)
		{
			REACCESS(cmzn_tessellation)(&(graphic->tessellation), tessellation);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_get_tessellation_field(
	cmzn_graphic_id graphic)
{
	if (graphic && graphic->tessellation_field)
		return ACCESS(Computed_field)(graphic->tessellation_field);
	return 0;
}

int cmzn_graphic_set_tessellation_field(cmzn_graphic_id graphic,
	cmzn_field_id tessellation_field)
{
	if (graphic)
	{
		if (tessellation_field != graphic->tessellation_field)
		{
			REACCESS(Computed_field)(&(graphic->tessellation_field), tessellation_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_get_top_level_number_in_xi(struct cmzn_graphic *graphic,
	int max_dimensions, int *top_level_number_in_xi)
{
	int return_code = 1;
	if (graphic && (0 < max_dimensions) && top_level_number_in_xi)
	{
		int dim;
		for (dim = 0; dim < max_dimensions; dim++)
		{
			top_level_number_in_xi[dim] = 1;
		}
		if (graphic->tessellation)
		{
			cmzn_tessellation_get_minimum_divisions(graphic->tessellation,
				max_dimensions, top_level_number_in_xi);
			cmzn_field *tessellation_field = graphic->tessellation_field ?
				graphic->tessellation_field : graphic->coordinate_field;
			if (tessellation_field)
			{
				// refine if tessellation field is non-linear
				// first check if its coordinate system is non-linear (cheaper)
				if (((tessellation_field == graphic->coordinate_field) &&
					Coordinate_system_type_is_non_linear(get_coordinate_system_type(
						Computed_field_get_coordinate_system(tessellation_field)))) ||
					Computed_field_is_non_linear(tessellation_field))
				{
					int *refinement_factors = new int[max_dimensions];
					if (cmzn_tessellation_get_refinement_factors(graphic->tessellation,
						max_dimensions, refinement_factors))
					{
						for (dim = 0; dim < max_dimensions; dim++)
						{
							top_level_number_in_xi[dim] *= refinement_factors[dim];
						}
					}
					delete [] refinement_factors;
				}
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_top_level_number_in_xi.  Invalid argument(s)");
		return_code = 0;
	}
	return return_code;
}

struct FE_element *cmzn_graphic_get_seed_element(
	struct cmzn_graphic *graphic)
{
	struct FE_element *seed_element;

	ENTER(cmzn_graphic_get_seed_element);
	if (graphic&&(CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type))
	{
		seed_element=graphic->seed_element;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_get_seed_element.  Invalid argument(s)");
		seed_element=(struct FE_element *)NULL;
	}
	LEAVE;

	return (seed_element);
} /* cmzn_graphic_get_seed_element */

int cmzn_graphic_set_seed_element(struct cmzn_graphic *graphic,
	struct FE_element *seed_element)
/*******************************************************************************
LAST MODIFIED : 19 March 1999

DESCRIPTION :
For graphic starting in a particular element.
==============================================================================*/
{
	int return_code;

	ENTER(cmzn_graphic_set_seed_element);
	if (graphic&&(CMZN_GRAPHIC_STREAMLINES==graphic->graphic_type))
	{
		REACCESS(FE_element)(&graphic->seed_element,seed_element);
		return_code=1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_set_seed_element.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_set_seed_element */

double cmzn_graphic_get_render_line_width(cmzn_graphic_id graphic)
{
	if (graphic)
		return graphic->render_line_width;
	return 0.0;
}

int cmzn_graphic_set_render_line_width(cmzn_graphic_id graphic, double width)
{
	if (graphic && (width > 0.0))
	{
		if (graphic->render_line_width != width)
		{
			graphic->render_line_width = width;
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

double cmzn_graphic_get_render_point_size(cmzn_graphic_id graphic)
{
	if (graphic)
		return graphic->render_point_size;
	return 0.0;
}

int cmzn_graphic_set_render_point_size(cmzn_graphic_id graphic, double size)
{
	if (graphic && (size > 0.0))
	{
		if (graphic->render_point_size != size)
		{
			graphic->render_point_size = size;
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_get_texture_coordinate_field(
	cmzn_graphic_id graphic)
{
	if (graphic && graphic->texture_coordinate_field)
	{
		return ACCESS(Computed_field)(graphic->texture_coordinate_field);
	}
	return 0;
}

int cmzn_graphic_set_texture_coordinate_field(
	cmzn_graphic_id graphic, cmzn_field_id texture_coordinate_field)
{
	if (graphic && ((0 == texture_coordinate_field) ||
		(3 >= Computed_field_get_number_of_components(texture_coordinate_field))))
	{
		if (texture_coordinate_field != graphic->texture_coordinate_field)
		{
			REACCESS(Computed_field)(&graphic->texture_coordinate_field, texture_coordinate_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_time_change(
	struct cmzn_graphic *graphic,void *dummy_void)
{
	int return_code;

	ENTER(cmzn_graphic_time_change);
	USE_PARAMETER(dummy_void);
	if (graphic)
	{
		return_code = 1;
		if (graphic->glyph)
		{
			graphic->glyph->timeChange();
		}
		if (graphic->time_dependent)
		{
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_time_change.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_time_change */

int cmzn_graphic_update_time_behaviour(
	struct cmzn_graphic *graphic, void *update_time_behaviour_void)
{
	int return_code, time_dependent;
	struct cmzn_graphic_update_time_behaviour_data *data;

	ENTER(cmzn_graphic_update_time_behaviour);
	if (graphic && (data =
		(struct cmzn_graphic_update_time_behaviour_data *)
		update_time_behaviour_void))
	{
		return_code = 1;
		time_dependent = 0;
		if (graphic->glyph && graphic->glyph->isTimeVarying())
		{
			time_dependent = 1;
		}
		if (graphic->coordinate_field)
		{
			if (Computed_field_has_multiple_times(graphic->coordinate_field))
			{
				time_dependent = 1;
			}
		}
		else
		{
			if (data->default_coordinate_depends_on_time)
			{
				time_dependent = 1;
			}
		}
		if (graphic->texture_coordinate_field && Computed_field_has_multiple_times(
			graphic->texture_coordinate_field))
		{
			time_dependent = 1;
		}
		if (graphic->line_orientation_scale_field && Computed_field_has_multiple_times(
			graphic->line_orientation_scale_field))
		{
			time_dependent = 1;
		}
		if (graphic->isoscalar_field && Computed_field_has_multiple_times(
			graphic->isoscalar_field))
		{
			time_dependent = 1;
		}
		if (graphic->point_orientation_scale_field &&
			Computed_field_has_multiple_times(graphic->point_orientation_scale_field))
		{
			time_dependent = 1;
		}
		if (graphic->signed_scale_field &&
			Computed_field_has_multiple_times(graphic->signed_scale_field))
		{
			time_dependent = 1;
		}
		if (graphic->label_field &&
			Computed_field_has_multiple_times(graphic->label_field))
		{
			time_dependent = 1;
		}
		if (graphic->label_density_field &&
			Computed_field_has_multiple_times(graphic->label_density_field))
		{
			time_dependent = 1;
		}
		if (graphic->subgroup_field &&
			Computed_field_has_multiple_times(graphic->subgroup_field))
		{
			time_dependent = 1;
		}
		if (graphic->signed_scale_field &&
			Computed_field_has_multiple_times(graphic->signed_scale_field))
		{
			time_dependent = 1;
		}
		if (graphic->stream_vector_field &&
			Computed_field_has_multiple_times(graphic->stream_vector_field))
		{
			time_dependent = 1;
		}
		if (graphic->data_field &&
			Computed_field_has_multiple_times(graphic->data_field))
		{
			time_dependent = 1;
		}
		/* Or any field that is pointed to has multiple times...... */

		graphic->time_dependent = time_dependent;
		if (time_dependent)
		{
			data->time_dependent = time_dependent;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_update_time_behaviour.  Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_update_time_behaviour */

int cmzn_graphic_glyph_change(
	struct cmzn_graphic *graphic, void *manager_message_void)
{
	struct MANAGER_MESSAGE(cmzn_glyph) *manager_message =
		reinterpret_cast<struct MANAGER_MESSAGE(cmzn_glyph) *>(manager_message_void);
	if (graphic && manager_message)
	{
		if (graphic->glyph)
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(cmzn_glyph)(
				manager_message, graphic->glyph);
			if ((change_flags & MANAGER_CHANGE_RESULT(cmzn_glyph)) != 0)
			{
				cmzn_graphic_update_graphics_object_trivial_glyph(graphic);
				cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
			}
		}
		return 1;
	}
	return 0;
}

int cmzn_graphics_material_change(
	struct cmzn_graphic *graphic, void *material_manager_message_void)
{
	int return_code;
	struct MANAGER_MESSAGE(Graphical_material) *manager_message =
		(struct MANAGER_MESSAGE(Graphical_material) *)material_manager_message_void;
	if (graphic && manager_message)
	{
		return_code = 1;
		bool material_change = false;
		if (graphic->material)
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(Graphical_material)(
				manager_message, graphic->material);
			material_change = (change_flags & MANAGER_CHANGE_RESULT(Graphical_material)) != 0;
		}
		if (!material_change && graphic->secondary_material)
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(Graphical_material)(
				manager_message, graphic->secondary_material);
			material_change = (change_flags & MANAGER_CHANGE_RESULT(Graphical_material)) != 0;
		}
		if (!material_change && graphic->selected_material)
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(Graphical_material)(
				manager_message, graphic->selected_material);
			material_change = (change_flags & MANAGER_CHANGE_RESULT(Graphical_material)) != 0;
		}
		if (graphic->glyph)
		{
			graphic->glyph->materialChange(manager_message);
		}
		if (material_change)
		{
			if (graphic->graphics_object)
			{
				GT_object_Graphical_material_change(graphic->graphics_object,
					(struct LIST(Graphical_material) *)NULL);
			}
			/* need a way to tell either graphic is used in any scene or not */
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphics_material_change.  Invalid argument(s)");
		return_code = 0;
	}
	return return_code;
}

int cmzn_graphic_spectrum_change(
	struct cmzn_graphic *graphic, void *spectrum_manager_message_void)
{
	int return_code;
	struct MANAGER_MESSAGE(Spectrum) *manager_message =
		(struct MANAGER_MESSAGE(Spectrum) *)spectrum_manager_message_void;
	if (graphic && manager_message)
	{
		return_code = 1;
		if (graphic->spectrum)
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(Spectrum)(
				manager_message, graphic->spectrum);
			if (change_flags & MANAGER_CHANGE_RESULT(Spectrum))
			{
				if (graphic->graphics_object)
				{
					GT_object_Spectrum_change(graphic->graphics_object,
						(struct LIST(Spectrum) *)NULL);
				}
				/* need a way to tell either graphic is used in any scene or not */
				cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
			}
		}
		/* The material gets it's own notification of the change,
			it should propagate that to the cmzn_graphic */
		struct Spectrum *colour_lookup;
		if (graphic->material && (colour_lookup =
				Graphical_material_get_colour_lookup_spectrum(graphic->material)))
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(Spectrum)(
				manager_message, colour_lookup);
			if (change_flags & MANAGER_CHANGE_RESULT(Spectrum))
			{
				if (graphic->graphics_object)
				{
					GT_object_Graphical_material_change(graphic->graphics_object,
						(struct LIST(Graphical_material) *)NULL);
				}
				/* need a way to tell either graphic is used in any scene or not */
				cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_spectrum_change.  Invalid argument(s)");
		return_code = 0;
	}
	return return_code;
}

int cmzn_graphic_tessellation_change(struct cmzn_graphic *graphic,
	void *tessellation_manager_message_void)
{
	int return_code;
	struct MANAGER_MESSAGE(cmzn_tessellation) *manager_message =
		(struct MANAGER_MESSAGE(cmzn_tessellation) *)tessellation_manager_message_void;
	if (graphic && manager_message)
	{
		return_code = 1;
		if (graphic->tessellation)
		{
			const cmzn_tessellation_change_detail *change_detail = 0;
			int change_flags = cmzn_tessellation_manager_message_get_object_change_and_detail(
				manager_message, graphic->tessellation, &change_detail);
			if (change_flags & MANAGER_CHANGE_RESULT(cmzn_tessellation))
			{
				if (change_detail->isElementDivisionsChanged() &&
					(0 < cmzn_graphic_get_domain_dimension(graphic)))
				{
					cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
				}
				else if (change_detail->isCircleDivisionsChanged())
				{
					if (CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_CIRCLE_EXTRUSION == graphic->line_shape)
					{
						cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
					}
					else if (graphic->glyph && graphic->glyph->usesCircleDivisions())
					{
						cmzn_graphic_update_graphics_object_trivial_glyph(graphic);
						cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
					}
				}
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_tessellation_change.  Invalid argument(s)");
		return_code = 0;
	}
	return return_code;
}

int cmzn_graphic_font_change(struct cmzn_graphic *graphic,
	void *font_manager_message_void)
{
	int return_code;
	struct MANAGER_MESSAGE(cmzn_font) *manager_message =
		(struct MANAGER_MESSAGE(cmzn_font) *)font_manager_message_void;
	if (graphic && manager_message)
	{
		return_code = 1;
		if ((graphic->graphic_type == CMZN_GRAPHIC_POINTS) && (graphic->font))
		{
			int change_flags = MANAGER_MESSAGE_GET_OBJECT_CHANGE(cmzn_font)(
				manager_message, graphic->font);
			if (change_flags & MANAGER_CHANGE_RESULT(cmzn_font))
			{
				bool glyphUsesFont = (0 != graphic->glyph) && graphic->glyph->usesFont();
				if (glyphUsesFont || graphic->label_field || graphic->label_text[0] ||
					graphic->label_text[1] || graphic->label_text[2])
				{
					if (glyphUsesFont)
					{
						graphic->glyph->fontChange();
					}
					if (graphic->graphics_object)
					{
						if (glyphUsesFont)
						{
							cmzn_graphic_update_graphics_object_trivial_glyph(graphic);
						}
						GT_object_changed(graphic->graphics_object);
					}
					cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
				}
			}
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_font_change.  Invalid argument(s)");
		return_code = 0;
	}
	return return_code;
}

int cmzn_graphic_detach_fields(struct cmzn_graphic *graphic, void *dummy_void)
{
	int return_code = 1;
	USE_PARAMETER(dummy_void);

	if (graphic)
	{
		if (graphic->coordinate_field)
		{
			DEACCESS(Computed_field)(&(graphic->coordinate_field));
		}
		if (graphic->texture_coordinate_field)
		{
			DEACCESS(Computed_field)(&(graphic->texture_coordinate_field));
		}
		if (graphic->line_orientation_scale_field)
		{
			DEACCESS(Computed_field)(&(graphic->line_orientation_scale_field));
		}
		if (graphic->isoscalar_field)
		{
			DEACCESS(Computed_field)(&(graphic->isoscalar_field));
		}
		if (graphic->point_orientation_scale_field)
		{
			DEACCESS(Computed_field)(&(graphic->point_orientation_scale_field));
		}
		if (graphic->signed_scale_field)
		{
			DEACCESS(Computed_field)(&(graphic->signed_scale_field));
		}
		if (graphic->label_field)
		{
			DEACCESS(Computed_field)(&(graphic->label_field));
		}
		if (graphic->label_density_field)
		{
			DEACCESS(Computed_field)(&(graphic->label_density_field));
		}
		if (graphic->subgroup_field)
		{
			DEACCESS(Computed_field)(&(graphic->subgroup_field));
		}
		cmzn_field_destroy(&(graphic->sample_density_field));
		cmzn_field_destroy(&(graphic->tessellation_field));
		if (graphic->stream_vector_field)
		{
			DEACCESS(Computed_field)(&(graphic->stream_vector_field));
		}
		if (graphic->data_field)
		{
			DEACCESS(Computed_field)(&(graphic->data_field));
		}
		if (graphic->seed_node_mesh_location_field)
		{
			DEACCESS(Computed_field)(&(graphic->seed_node_mesh_location_field));
		}
	}
	else
	{
		display_message(INFORMATION_MESSAGE,
			"cmzn_graphic_detach_fields.  Invalid argument(s)");
		return_code = 0;
	}

	return return_code;
}

int cmzn_graphic_selected_element_points_change(
	struct cmzn_graphic *graphic,void *dummy_void)
/*******************************************************************************
LAST MODIFIED : 28 February 2000

DESCRIPTION :
Tells <graphic> that if the graphics resulting from it depend on the currently
selected element points, then they should be updated.
Must call cmzn_graphic_to_graphics_object afterwards to complete.
==============================================================================*/
{
	int return_code;

	ENTER(cmzn_graphic_selected_element_points_change);
	USE_PARAMETER(dummy_void);
	if (graphic)
	{
		return_code=1;
		if (graphic->graphics_object&&
			(CMZN_GRAPHIC_POINTS == graphic->graphic_type) &&
			(0 < cmzn_graphic_get_domain_dimension(graphic)))
		{
			cmzn_graphic_update_selected(graphic, (void *)NULL);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"cmzn_graphic_selected_element_points_change.  "
			"Invalid argument(s)");
		return_code=0;
	}
	LEAVE;

	return (return_code);
} /* cmzn_graphic_selected_element_points_change */

struct cmzn_scene *cmzn_graphic_get_scene_private(struct cmzn_graphic *graphic)
{
	if (graphic)
		return graphic->scene;
	return NULL;
}

int cmzn_graphic_set_scene_private(struct cmzn_graphic *graphic,
	struct cmzn_scene *scene)
{
	if (graphic && ((NULL == scene) || (NULL == graphic->scene)))
	{
		graphic->scene = scene;
		return 1;
	}
	else
	{
		display_message(INFORMATION_MESSAGE,
			"cmzn_graphic_set_scene_private.  Invalid argument(s)");
	}
	return 0;
}

int cmzn_graphic_set_scene_for_list_private(struct cmzn_graphic *graphic, void *scene_void)
{
	cmzn_scene *scene = (cmzn_scene *)scene_void;
	int return_code = 0;
	if (graphic && scene)
	{
		if (graphic->scene == scene)
		{
			return_code = 1;
		}
		else
		{
			return_code = cmzn_graphic_set_scene_private(graphic, NULL);
			return_code = cmzn_graphic_set_scene_private(graphic, scene);
		}
	}
	else
	{
		display_message(INFORMATION_MESSAGE,
			"cmzn_graphic_set_scene_for_list_private.  Invalid argument(s)");
	}

	return return_code;
}

cmzn_graphic_id cmzn_graphic_access(cmzn_graphic_id graphic)
{
	if (graphic)
		return (ACCESS(cmzn_graphic)(graphic));
	return 0;
}

int cmzn_graphic_destroy(cmzn_graphic_id *graphic_address)
{
	if (graphic_address)
	{
		DEACCESS(cmzn_graphic)(graphic_address);
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

class cmzn_graphic_type_conversion
{
public:
	static const char *to_string(enum cmzn_graphic_type type)
	{
		const char *enum_string = 0;
		switch (type)
		{
			case CMZN_GRAPHIC_POINTS:
				enum_string = "POINTS";
				break;
			case CMZN_GRAPHIC_LINES:
				enum_string = "LINES";
				break;
			case CMZN_GRAPHIC_SURFACES:
				enum_string = "SURFACES";
				break;
			case CMZN_GRAPHIC_CONTOURS:
				enum_string = "CONTOURS";
				break;
			case CMZN_GRAPHIC_STREAMLINES:
				enum_string = "STREAMLINES";
				break;
		default:
			break;
		}
		return enum_string;
	}
};

enum cmzn_graphic_type cmzn_graphic_type_enum_from_string(const char *string)
{
	return string_to_enum<enum cmzn_graphic_type, cmzn_graphic_type_conversion>(string);
}

char *cmzn_graphic_type_enum_to_string(enum cmzn_graphic_type type)
{
	const char *type_string = cmzn_graphic_type_conversion::to_string(type);
	return (type_string ? duplicate_string(type_string) : 0);
}

class cmzn_graphic_render_polygon_mode_conversion
{
public:
	static const char *to_string(enum cmzn_graphic_render_polygon_mode type)
	{
		const char *enum_string = 0;
		switch (type)
		{
		case CMZN_GRAPHIC_RENDER_POLYGON_SHADED:
			enum_string = "RENDER_POLYGON_SHADED";
			break;
		case CMZN_GRAPHIC_RENDER_POLYGON_WIREFRAME:
			enum_string = "RENDER_POLYGON_WIREFRAME";
			break;
		default:
			break;
		}
		return enum_string;
	}
};

enum cmzn_graphic_render_polygon_mode cmzn_graphic_render_polygon_mode_enum_from_string(
	const char *string)
{
	return string_to_enum<enum cmzn_graphic_render_polygon_mode,
		cmzn_graphic_render_polygon_mode_conversion>(string);
}

char *cmzn_graphic_render_polygon_mode_enum_to_string(
	enum cmzn_graphic_render_polygon_mode type)
{
	const char *type_string = cmzn_graphic_render_polygon_mode_conversion::to_string(type);
	return (type_string ? duplicate_string(type_string) : 0);
}

enum cmzn_field_domain_type cmzn_graphic_get_domain_type(
	cmzn_graphic_id graphic)
{
	if (graphic)
		return graphic->domain_type;
	return CMZN_FIELD_DOMAIN_TYPE_INVALID;
}

int cmzn_graphic_set_domain_type(cmzn_graphic_id graphic,
	enum cmzn_field_domain_type domain_type)
{
	if (graphic && (domain_type != CMZN_FIELD_DOMAIN_TYPE_INVALID) &&
		(graphic->graphic_type != CMZN_GRAPHIC_LINES) &&
		(graphic->graphic_type != CMZN_GRAPHIC_SURFACES) &&
		((graphic->graphic_type == CMZN_GRAPHIC_POINTS) ||
		 ((domain_type != CMZN_FIELD_DOMAIN_POINT) &&
			(domain_type != CMZN_FIELD_DOMAIN_NODES) &&
			(domain_type != CMZN_FIELD_DOMAIN_DATA))))
	{
		graphic->domain_type = domain_type;
		if (domain_type != graphic->domain_type)
		{
			graphic->domain_type = domain_type;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_graphic_contours_id cmzn_graphic_cast_contours(cmzn_graphic_id graphic)
{
	if (graphic && (graphic->graphic_type == CMZN_GRAPHIC_CONTOURS))
	{
		cmzn_graphic_access(graphic);
		return (reinterpret_cast<cmzn_graphic_contours_id>(graphic));
	}
	return 0;
}

int cmzn_graphic_contours_destroy(cmzn_graphic_contours_id *contours_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(contours_address));
}

double cmzn_graphic_contours_get_decimation_threshold(
	cmzn_graphic_contours_id contours_graphic)
{
	if (contours_graphic)
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		return graphic->decimation_threshold;
	}
	return 0;
}

int cmzn_graphic_contours_set_decimation_threshold(
	cmzn_graphic_contours_id contours_graphic, double decimation_threshold)
{
	if (contours_graphic)
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if (decimation_threshold != graphic->decimation_threshold)
		{
			graphic->decimation_threshold = decimation_threshold;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_contours_get_isoscalar_field(
	cmzn_graphic_contours_id contours_graphic)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
	if (graphic && graphic->isoscalar_field)
	{
		return cmzn_field_access(graphic->isoscalar_field);
	}
	return 0;
}

int cmzn_graphic_contours_set_isoscalar_field(
	cmzn_graphic_contours_id contours_graphic,
	cmzn_field_id isoscalar_field)
{
	if (contours_graphic && ((0 == isoscalar_field) ||
		(1 == cmzn_field_get_number_of_components(isoscalar_field))))
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if (isoscalar_field != graphic->isoscalar_field)
		{
			REACCESS(Computed_field)(&(graphic->isoscalar_field), isoscalar_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_contours_get_list_isovalues(
	cmzn_graphic_contours_id contours_graphic, int number_of_isovalues,
	double *isovalues)
{
	if (contours_graphic && ((number_of_isovalues == 0) ||
		((number_of_isovalues > 0) && isovalues)))
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if (graphic->isovalues)
		{
			const int number_to_copy = (number_of_isovalues < graphic->number_of_isovalues) ?
				number_of_isovalues : graphic->number_of_isovalues;
			for (int i = 0 ; i < number_to_copy ; i++)
			{
				isovalues[i] = graphic->isovalues[i];
			}
			return graphic->number_of_isovalues;
		}
	}
	return 0;
}

int cmzn_graphic_contours_set_list_isovalues(
	cmzn_graphic_contours_id contours_graphic, int number_of_isovalues,
	const double *isovalues)
{
	if (contours_graphic && ((number_of_isovalues == 0) ||
		((number_of_isovalues > 0) && isovalues)))
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		bool changed = false;
		if (number_of_isovalues == graphic->number_of_isovalues)
		{
			if (graphic->isovalues)
			{
				for (int i = 0; i < number_of_isovalues; i++)
				{
					if (isovalues[i] != graphic->isovalues[i])
					{
						changed = true;
						break;
					}
				}
			}
			else
			{
				changed = true;
			}
		}
		else
		{
			changed = true;
		}
		if (changed)
		{
			if (0 < number_of_isovalues)
			{
				double *temp_values;
				REALLOCATE(temp_values, graphic->isovalues, double, number_of_isovalues);
				if (!temp_values)
				{
					return CMZN_ERROR_MEMORY;
				}
				graphic->isovalues = temp_values;
				graphic->number_of_isovalues = number_of_isovalues;
				for (int i = 0 ; i < number_of_isovalues ; i++)
				{
					graphic->isovalues[i] = isovalues[i];
				}
			}
			else
			{
				if (graphic->isovalues)
				{
					DEALLOCATE(graphic->isovalues);
					graphic->isovalues = 0;
				}
				graphic->number_of_isovalues = 0;
			}
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

double cmzn_graphic_contours_get_range_first_isovalue(
	cmzn_graphic_contours_id contours_graphic)
{
	if (contours_graphic)
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if (0 == graphic->isovalues)
		{
			return graphic->first_isovalue;
		}
	}
	return 0.0;
}

double cmzn_graphic_contours_get_range_last_isovalue(
	cmzn_graphic_contours_id contours_graphic)
{
	if (contours_graphic)
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if (0 == graphic->isovalues)
		{
			return graphic->last_isovalue;
		}
	}
	return 0.0;
}

int cmzn_graphic_contours_get_range_number_of_isovalues(
	cmzn_graphic_contours_id contours_graphic)
{
	if (contours_graphic)
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if (0 == graphic->isovalues)
		{
			return graphic->number_of_isovalues;
		}
	}
	return 0;
}

int cmzn_graphic_contours_set_range_isovalues(
	cmzn_graphic_contours_id contours_graphic, int number_of_isovalues,
	double first_isovalue, double last_isovalue)
{
	if (contours_graphic && (0 <= number_of_isovalues))
	{
		cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic_id>(contours_graphic);
		if ((number_of_isovalues != graphic->number_of_isovalues) ||
			(0 != graphic->isovalues) || (first_isovalue != graphic->first_isovalue) ||
			(last_isovalue != graphic->last_isovalue))
		{
			if (graphic->isovalues)
			{
				DEALLOCATE(graphic->isovalues);
				graphic->isovalues = 0;
			}
			graphic->number_of_isovalues = number_of_isovalues;
			graphic->first_isovalue = first_isovalue;
			graphic->last_isovalue = last_isovalue;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_graphic_lines_id cmzn_graphic_cast_lines(cmzn_graphic_id graphic)
{
	if (graphic && (graphic->graphic_type == CMZN_GRAPHIC_LINES))
	{
		cmzn_graphic_access(graphic);
		return (reinterpret_cast<cmzn_graphic_lines_id>(graphic));
	}
	return 0;
}

int cmzn_graphic_lines_destroy(cmzn_graphic_lines_id *lines_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(lines_address));
}

cmzn_graphic_points_id cmzn_graphic_cast_points(cmzn_graphic_id graphic)
{
	if (graphic && (graphic->graphic_type == CMZN_GRAPHIC_POINTS))
	{
		cmzn_graphic_access(graphic);
		return (reinterpret_cast<cmzn_graphic_points_id>(graphic));
	}
	return 0;
}

int cmzn_graphic_points_destroy(cmzn_graphic_points_id *points_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(points_address));
}

cmzn_graphic_streamlines_id cmzn_graphic_cast_streamlines(cmzn_graphic_id graphic)
{
	if (graphic && (graphic->graphic_type == CMZN_GRAPHIC_STREAMLINES))
	{
		cmzn_graphic_access(graphic);
		return (reinterpret_cast<cmzn_graphic_streamlines_id>(graphic));
	}
	return 0;
}

int cmzn_graphic_streamlines_destroy(cmzn_graphic_streamlines_id *streamlines_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(streamlines_address));
}

cmzn_field_id cmzn_graphic_streamlines_get_stream_vector_field(
	cmzn_graphic_streamlines_id streamlines_graphic)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(streamlines_graphic);
	if (graphic && (graphic->stream_vector_field))
	{
		return ACCESS(Computed_field)(graphic->stream_vector_field);
	}
	return 0;
}

int cmzn_graphic_streamlines_set_stream_vector_field(
	cmzn_graphic_streamlines_id streamlines_graphic,
	cmzn_field_id stream_vector_field)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(streamlines_graphic);
	if (graphic)
	{
		if (stream_vector_field != graphic->stream_vector_field)
		{
			REACCESS(Computed_field)(&(graphic->stream_vector_field), stream_vector_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

enum cmzn_graphic_streamlines_track_direction
	cmzn_graphic_streamlines_get_track_direction(
		cmzn_graphic_streamlines_id streamlines_graphic)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(streamlines_graphic);
	if (graphic)
		return graphic->streamlines_track_direction;
	return CMZN_GRAPHIC_STREAMLINES_TRACK_DIRECTION_INVALID;
}

int cmzn_graphic_streamlines_set_track_direction(
	cmzn_graphic_streamlines_id streamlines_graphic,
	enum cmzn_graphic_streamlines_track_direction track_direction)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(streamlines_graphic);
	if (graphic && (track_direction != CMZN_GRAPHIC_STREAMLINES_TRACK_DIRECTION_INVALID))
	{
		if (track_direction != graphic->streamlines_track_direction)
		{
			graphic->streamlines_track_direction = track_direction;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

double cmzn_graphic_streamlines_get_track_length(
	cmzn_graphic_streamlines_id streamlines_graphic)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(streamlines_graphic);
	if (graphic)
		return graphic->streamline_length;
	return 0.0;
}

int cmzn_graphic_streamlines_set_track_length(
	cmzn_graphic_streamlines_id streamlines_graphic, double length)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(streamlines_graphic);
	if (graphic && (length >= 0.0))
	{
		if (length != graphic->streamline_length)
		{
			graphic->streamline_length = length;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_graphic_surfaces_id cmzn_graphic_cast_surfaces(cmzn_graphic_id graphic)
{
	if (graphic && (graphic->graphic_type == CMZN_GRAPHIC_SURFACES))
	{
		cmzn_graphic_access(graphic);
		return (reinterpret_cast<cmzn_graphic_surfaces_id>(graphic));
	}
	return 0;
}

int cmzn_graphic_surfaces_destroy(cmzn_graphic_surfaces_id *surfaces_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(surfaces_address));
}

cmzn_graphic_line_attributes_id cmzn_graphic_get_line_attributes(
	cmzn_graphic_id graphic)
{
	if (graphic && (
		(graphic->graphic_type == CMZN_GRAPHIC_LINES) ||
		(graphic->graphic_type == CMZN_GRAPHIC_STREAMLINES)))
	{
		cmzn_graphic_access(graphic);
		return reinterpret_cast<cmzn_graphic_line_attributes_id>(graphic);
	}
	return 0;
}

cmzn_graphic_line_attributes_id cmzn_graphic_line_attributes_access(
	cmzn_graphic_line_attributes_id line_attributes)
{
	cmzn_graphic_access(reinterpret_cast<cmzn_graphic_id>(line_attributes));
	return line_attributes;
}

int cmzn_graphic_line_attributes_destroy(
	cmzn_graphic_line_attributes_id *line_attributes_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(line_attributes_address));
}

int cmzn_graphic_line_attributes_get_base_size(
	cmzn_graphic_line_attributes_id line_attributes, int number,
	double *base_size)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic && (0 < number) && base_size)
	{
		const int count = (number > 2) ? 2 : number;
		for (int i = 0; i < count; ++i)
		{
			base_size[i] = static_cast<double>(graphic->line_base_size[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_line_attributes_set_base_size(
	cmzn_graphic_line_attributes_id line_attributes, int number,
	const double *base_size)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic && (0 < number) && base_size)
	{
		bool changed = false;
		if (graphic->graphic_type == CMZN_GRAPHIC_LINES)
		{
			number = 1; // only equal values supported for lines (cylinders)
		}
		FE_value value;
		for (int i = 0; i < 2; ++i)
		{
			if (i < number)
			{
				value = static_cast<FE_value>(base_size[i]);
			}
			if (graphic->line_base_size[i] != value)
			{
				graphic->line_base_size[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_line_attributes_get_orientation_scale_field(
	cmzn_graphic_line_attributes_id line_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic && (graphic->line_orientation_scale_field))
	{
		return ACCESS(Computed_field)(graphic->line_orientation_scale_field);
	}
	return 0;
}

int cmzn_graphic_line_attributes_set_orientation_scale_field(
	cmzn_graphic_line_attributes_id line_attributes,
	cmzn_field_id orientation_scale_field)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic)
	{
		if (orientation_scale_field != graphic->line_orientation_scale_field)
		{
			REACCESS(Computed_field)(&(graphic->line_orientation_scale_field), orientation_scale_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_line_attributes_get_scale_factors(
	cmzn_graphic_line_attributes_id line_attributes, int number,
	double *scale_factors)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic && (0 < number) && scale_factors)
	{
		const int count = (number > 2) ? 2 : number;
		for (int i = 0; i < count; ++i)
		{
			scale_factors[i] = static_cast<double>(graphic->line_scale_factors[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_line_attributes_set_scale_factors(
	cmzn_graphic_line_attributes_id line_attributes, int number,
	const double *scale_factors)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic && (0 < number) && scale_factors)
	{
		bool changed = false;
		if (graphic->graphic_type == CMZN_GRAPHIC_LINES)
		{
			number = 1; // only equal values supported for lines (cylinders)
		}
		FE_value value;
		for (int i = 0; i < 2; ++i)
		{
			if (i < number)
			{
				value = static_cast<FE_value>(scale_factors[i]);
			}
			if (graphic->line_scale_factors[i] != value)
			{
				graphic->line_scale_factors[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

enum cmzn_graphic_line_attributes_shape cmzn_graphic_line_attributes_get_shape(
	cmzn_graphic_line_attributes_id line_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic)
	{
		return graphic->line_shape;
	}
	return CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_INVALID;
}

int cmzn_graphic_line_attributes_set_shape(
	cmzn_graphic_line_attributes_id line_attributes,
	enum cmzn_graphic_line_attributes_shape shape)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(line_attributes);
	if (graphic && (shape != CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_INVALID) &&
		((graphic->graphic_type == CMZN_GRAPHIC_STREAMLINES) ||
			(shape == CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_LINE) ||
			(shape == CMZN_GRAPHIC_LINE_ATTRIBUTES_SHAPE_CIRCLE_EXTRUSION)))
	{
		if (shape != graphic->line_shape)
		{
			graphic->line_shape = shape;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_graphic_point_attributes_id cmzn_graphic_get_point_attributes(
	cmzn_graphic_id graphic)
{
	if (graphic && (
		(graphic->graphic_type == CMZN_GRAPHIC_POINTS)))
	{
		cmzn_graphic_access(graphic);
		return reinterpret_cast<cmzn_graphic_point_attributes_id>(graphic);
	}
	return 0;
}

cmzn_graphic_point_attributes_id cmzn_graphic_point_attributes_access(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic_access(reinterpret_cast<cmzn_graphic_id>(point_attributes));
	return point_attributes;
}

int cmzn_graphic_point_attributes_destroy(
	cmzn_graphic_point_attributes_id *point_attributes_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(point_attributes_address));
}

int cmzn_graphic_point_attributes_get_base_size(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	double *base_size)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && base_size)
	{
		const int count = (number > 3) ? 3 : number;
		for (int i = 0; i < count; ++i)
		{
			base_size[i] = static_cast<double>(graphic->point_base_size[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_set_base_size(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	const double *base_size)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && base_size)
	{
		bool changed = false;
		FE_value value;
		for (int i = 0; i < 3; ++i)
		{
			if (i < number)
			{
				value = static_cast<FE_value>(base_size[i]);
			}
			if (graphic->point_base_size[i] != value)
			{
				graphic->point_base_size[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_font_id cmzn_graphic_point_attributes_get_font(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (graphic->font))
	{
		return ACCESS(cmzn_font)(graphic->font);
	}
	return 0;
}

int cmzn_graphic_point_attributes_set_font(
	cmzn_graphic_point_attributes_id point_attributes,
	cmzn_font_id font)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic)
	{
		if (font != graphic->font)
		{
			REACCESS(cmzn_font)(&(graphic->font), font);
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_glyph_id cmzn_graphic_point_attributes_get_glyph(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && graphic->glyph)
	{
		return cmzn_glyph_access(graphic->glyph);
	}
	return 0;
}

int cmzn_graphic_point_attributes_set_glyph(
	cmzn_graphic_point_attributes_id point_attributes, cmzn_glyph_id glyph)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic)
	{
		if (glyph != graphic->glyph)
		{
			REACCESS(cmzn_glyph)(&(graphic->glyph), glyph);
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_get_glyph_offset(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	double *offset)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && offset)
	{
		const int count = (number > 3) ? 3 : number;
		for (int i = 0; i < count; ++i)
		{
			offset[i] = static_cast<double>(graphic->point_offset[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_set_glyph_offset(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	const double *offset)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && offset)
	{
		bool changed = false;
		FE_value value = 0.0;
		for (int i = 2; 0 <= i; --i)
		{
			if (i < number)
			{
				value = static_cast<FE_value>(offset[i]);
			}
			if (graphic->point_offset[i] != value)
			{
				graphic->point_offset[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

enum cmzn_glyph_repeat_mode
	cmzn_graphic_point_attributes_get_glyph_repeat_mode(
		cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic)
	{
		return graphic->glyph_repeat_mode;
	}
	return CMZN_GLYPH_REPEAT_MODE_INVALID;
}

int cmzn_graphic_point_attributes_set_glyph_repeat_mode(
	cmzn_graphic_point_attributes_id point_attributes,
	enum cmzn_glyph_repeat_mode glyph_repeat_mode)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (glyph_repeat_mode !=
		CMZN_GLYPH_REPEAT_MODE_INVALID))
	{
		if (glyph_repeat_mode != graphic->glyph_repeat_mode)
		{
			graphic->glyph_repeat_mode = glyph_repeat_mode;
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

enum cmzn_glyph_type cmzn_graphic_point_attributes_get_glyph_type(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic)
	{
		if (graphic->glyph)
		{
			return graphic->glyph->getType();
		}
		else
		{
			return CMZN_GLYPH_NONE;
		}
	}
	return CMZN_GLYPH_TYPE_INVALID;
}

int cmzn_graphic_point_attributes_set_glyph_type(
	cmzn_graphic_point_attributes_id point_attributes,
	enum cmzn_glyph_type glyph_type)
{
	int return_code = CMZN_ERROR_ARGUMENT;
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (CMZN_GLYPH_TYPE_INVALID != glyph_type))
	{
		cmzn_graphics_module_id graphics_module = cmzn_scene_get_graphics_module(graphic->scene);
		cmzn_glyph_module_id glyph_module = cmzn_graphics_module_get_glyph_module(graphics_module);
		cmzn_glyph_id glyph = glyph_module->findGlyphByType(glyph_type);
		if (glyph || (glyph_type == CMZN_GLYPH_NONE))
		{
			return_code = cmzn_graphic_point_attributes_set_glyph(point_attributes, glyph);
		}
		cmzn_glyph_module_destroy(&glyph_module);
		cmzn_graphics_module_destroy(&graphics_module);
	}
	return return_code;
}

cmzn_field_id cmzn_graphic_point_attributes_get_label_field(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (graphic->label_field))
	{
		return ACCESS(Computed_field)(graphic->label_field);
	}
	return 0;
}

int cmzn_graphic_point_attributes_set_label_field(
	cmzn_graphic_point_attributes_id point_attributes, cmzn_field_id label_field)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic)
	{
		if (label_field != graphic->label_field)
		{
			REACCESS(Computed_field)(&(graphic->label_field), label_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_get_label_offset(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	double *label_offset)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && label_offset)
	{
		const int count = (number > 3) ? 3 : number;
		for (int i = 0; i < count; ++i)
		{
			label_offset[i] = static_cast<double>(graphic->label_offset[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_set_label_offset(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	const double *label_offset)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && label_offset)
	{
		bool changed = false;
		FE_value value = 0.0;
		for (int i = 2; 0 <= i; --i)
		{
			if (i < number)
			{
				value = static_cast<FE_value>(label_offset[i]);
			}
			if (graphic->label_offset[i] != value)
			{
				graphic->label_offset[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

char *cmzn_graphic_point_attributes_get_label_text(
	cmzn_graphic_point_attributes_id point_attributes, int label_number)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < label_number) && (label_number <= 3) &&
		(graphic->label_text[label_number - 1]))
	{
		return duplicate_string(graphic->label_text[label_number - 1]);
	}
	return 0;
}

int cmzn_graphic_point_attributes_set_label_text(
	cmzn_graphic_point_attributes_id point_attributes, int label_number,
	const char *label_text)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < label_number) && (label_number <= 3))
	{
		if (!labels_match(label_text, graphic->label_text[label_number - 1]))
		{
			if (graphic->label_text[label_number - 1])
			{
				DEALLOCATE(graphic->label_text[label_number - 1]);
			}
			graphic->label_text[label_number - 1] =
				(label_text && (0 < strlen(label_text))) ? duplicate_string(label_text) : 0;
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_point_attributes_get_orientation_scale_field(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (graphic->point_orientation_scale_field))
	{
		return ACCESS(Computed_field)(graphic->point_orientation_scale_field);
	}
	return 0;
}

int cmzn_graphic_point_attributes_set_orientation_scale_field(
	cmzn_graphic_point_attributes_id point_attributes,
	cmzn_field_id orientation_scale_field)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && ((0 == orientation_scale_field) ||
		Computed_field_is_orientation_scale_capable(orientation_scale_field, (void *)0)))
	{
		if (orientation_scale_field != graphic->point_orientation_scale_field)
		{
			REACCESS(Computed_field)(&(graphic->point_orientation_scale_field), orientation_scale_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_get_scale_factors(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	double *scale_factors)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && scale_factors)
	{
		const int count = (number > 3) ? 3 : number;
		for (int i = 0; i < count; ++i)
		{
			scale_factors[i] = static_cast<double>(graphic->point_scale_factors[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_point_attributes_set_scale_factors(
	cmzn_graphic_point_attributes_id point_attributes, int number,
	const double *scale_factors)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (0 < number) && scale_factors)
	{
		bool changed = false;
		FE_value value;
		for (int i = 0; i < 3; ++i)
		{
			if (i < number)
			{
				value = static_cast<FE_value>(scale_factors[i]);
			}
			if (graphic->point_scale_factors[i] != value)
			{
				graphic->point_scale_factors[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_update_graphics_object_trivial(graphic);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_RECOMPILE);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_field_id cmzn_graphic_point_attributes_get_signed_scale_field(
	cmzn_graphic_point_attributes_id point_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && (graphic->signed_scale_field))
	{
		return ACCESS(Computed_field)(graphic->signed_scale_field);
	}
	return 0;
}

int cmzn_graphic_point_attributes_set_signed_scale_field(
	cmzn_graphic_point_attributes_id point_attributes,
	cmzn_field_id signed_scale_field)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(point_attributes);
	if (graphic && ((!signed_scale_field) ||
		Computed_field_has_up_to_3_numerical_components(signed_scale_field,(void *)NULL)))
	{
		if (signed_scale_field != graphic->signed_scale_field)
		{
			REACCESS(Computed_field)(&(graphic->signed_scale_field), signed_scale_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

cmzn_graphic_sampling_attributes_id cmzn_graphic_get_sampling_attributes(
	cmzn_graphic_id graphic)
{
	if (graphic && ((graphic->graphic_type == CMZN_GRAPHIC_POINTS) ||
		(graphic->graphic_type == CMZN_GRAPHIC_STREAMLINES)))
	{
		cmzn_graphic_access(graphic);
		return reinterpret_cast<cmzn_graphic_sampling_attributes_id>(graphic);
	}
	return 0;
}

cmzn_graphic_sampling_attributes_id cmzn_graphic_sampling_attributes_access(
	cmzn_graphic_sampling_attributes_id sampling_attributes)
{
	cmzn_graphic_access(reinterpret_cast<cmzn_graphic_id>(sampling_attributes));
	return sampling_attributes;
}

int cmzn_graphic_sampling_attributes_destroy(
	cmzn_graphic_sampling_attributes_id *sampling_attributes_address)
{
	return cmzn_graphic_destroy(reinterpret_cast<cmzn_graphic_id *>(sampling_attributes_address));
}

cmzn_field_id cmzn_graphic_sampling_attributes_get_density_field(
	cmzn_graphic_sampling_attributes_id sampling_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(sampling_attributes);
	if (graphic && graphic->sample_density_field)
		return ACCESS(Computed_field)(graphic->sample_density_field);
	return 0;
}

int cmzn_graphic_sampling_attributes_set_density_field(
	cmzn_graphic_sampling_attributes_id sampling_attributes,
	cmzn_field_id sample_density_field)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(sampling_attributes);
	if (graphic && ((!sample_density_field) ||
		Computed_field_is_scalar(sample_density_field, (void *)0)))
	{
		if (sample_density_field != graphic->sample_density_field)
		{
			REACCESS(Computed_field)(&(graphic->sample_density_field), sample_density_field);
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_sampling_attributes_get_location(
	cmzn_graphic_sampling_attributes_id sampling_attributes,
	int valuesCount, double *valuesOut)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(sampling_attributes);
	if (graphic && (0 < valuesCount) && valuesOut)
	{
		const int count = (valuesCount > 3) ? 3 : valuesCount;
		for (int i = 0; i < count; ++i)
		{
			valuesOut[i] = static_cast<double>(graphic->sample_location[i]);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

int cmzn_graphic_sampling_attributes_set_location(
	cmzn_graphic_sampling_attributes_id sampling_attributes,
	int valuesCount, const double *valuesIn)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(sampling_attributes);
	if (graphic && (0 < valuesCount) && valuesIn)
	{
		bool changed = false;
		FE_value value = 0.0;
		for (int i = 2; 0 <= i; --i)
		{
			if (i < valuesCount)
			{
				value = static_cast<FE_value>(valuesIn[i]);
			}
			if (graphic->sample_location[i] != value)
			{
				graphic->sample_location[i] = value;
				changed = true;
			}
		}
		if (changed)
		{
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}

enum cmzn_element_point_sample_mode cmzn_graphic_sampling_attributes_get_mode(
	cmzn_graphic_sampling_attributes_id sampling_attributes)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(sampling_attributes);
	if (graphic)
		return graphic->sample_mode;
	return CMZN_ELEMENT_POINT_SAMPLE_MODE_INVALID;
}

int cmzn_graphic_sampling_attributes_set_mode(
	cmzn_graphic_sampling_attributes_id sampling_attributes,
	enum cmzn_element_point_sample_mode sample_mode)
{
	cmzn_graphic *graphic = reinterpret_cast<cmzn_graphic *>(sampling_attributes);
	if (graphic && (0 != ENUMERATOR_STRING(cmzn_element_point_sample_mode)(sample_mode)))
	{
		if (sample_mode != graphic->sample_mode)
		{
			graphic->sample_mode = sample_mode;
			cmzn_graphic_changed(graphic, CMZN_GRAPHIC_CHANGE_FULL_REBUILD);
		}
		return CMZN_OK;
	}
	return CMZN_ERROR_ARGUMENT;
}
