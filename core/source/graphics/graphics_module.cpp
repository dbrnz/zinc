/**
 * FILE : graphics_module.cpp
 */
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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "zinc/glyph.h"
#include "zinc/graphicsmaterial.h"
#include "zinc/graphicsmodule.h"
#include "zinc/scene.h"
#include "general/debug.h"
#include "general/object.h"
#include "general/mystring.h"
#include "graphics/font.h"
#include "graphics/glyph.hpp"
#include "graphics/graphics_object.h"
#include "graphics/material.h"
#include "graphics/scene.h"
#include "graphics/scene_viewer.h"
#include "graphics/spectrum.h"
#include "graphics/graphics_module.h"
#include "graphics/light_model.h"
#include "graphics/graphics_filter.hpp"
#include "graphics/tessellation.hpp"
#include "region/cmiss_region_private.h"
#include "time/time_keeper.hpp"
#include "general/message.h"
#include <list>

struct Cmiss_graphics_module
{
	/* attribute managers and defaults: */
	struct Cmiss_glyph_module *glyph_module;
	struct Cmiss_graphics_material_module *material_module;
	void *material_manager_callback_id;
	struct Light *default_light;
	struct MANAGER(Light) *light_manager;
	Cmiss_spectrum_module_id spectrum_module;
	void *spectrum_manager_callback_id;
	Cmiss_font_module_id font_module;
	void *font_manager_callback_id;
	Cmiss_scene_viewer_module_id scene_viewer_module;
	struct Light_model *default_light_model;
	struct MANAGER(Light_model) *light_model_manager;
	struct Element_point_ranges_selection *element_point_ranges_selection;
	struct Cmiss_time_keeper *default_time_keeper;
	Cmiss_tessellation_module_id tessellation_module;
	struct Cmiss_graphics_filter_module *graphics_filter_module;
	void *tessellation_manager_callback_id;
	int access_count;
	Cmiss_region_id root_region;
	std::list<Cmiss_region*> *member_regions_list;
};

namespace {

/***************************************************************************//**
 * Callback for changes in the material manager.
 * Informs all scenes about the changes.
 */
void Cmiss_graphics_module_material_manager_callback(
	struct MANAGER_MESSAGE(Graphical_material) *message, void *graphics_module_void)
{
	Cmiss_graphics_module *graphics_module = (Cmiss_graphics_module *)graphics_module_void;
	if (message && graphics_module)
	{
		int change_summary = MANAGER_MESSAGE_GET_CHANGE_SUMMARY(Graphical_material)(message);
		if (change_summary & MANAGER_CHANGE_RESULT(Graphical_material))
		{
			std::list<Cmiss_region*>::iterator region_iter;
			for (region_iter = graphics_module->member_regions_list->begin();
				region_iter != graphics_module->member_regions_list->end(); ++region_iter)
			{
				Cmiss_region *region = *region_iter;
				Cmiss_scene *scene = Cmiss_graphics_module_get_scene(graphics_module, region);
				Cmiss_scene_begin_change(scene);
				Cmiss_scene_material_change(scene, message);
				Cmiss_scene_end_change(scene);
				DEACCESS(Cmiss_scene)(&scene);
			}
		}
	}
}

/***************************************************************************//**
 * Callback for changes in the spectrum manager.
 * Informs all scenes about the changes.
 */
void Cmiss_graphics_module_spectrum_manager_callback(
	struct MANAGER_MESSAGE(Spectrum) *message, void *graphics_module_void)
{
	Cmiss_graphics_module *graphics_module = (Cmiss_graphics_module *)graphics_module_void;
	if (message && graphics_module)
	{
		int change_summary = MANAGER_MESSAGE_GET_CHANGE_SUMMARY(Spectrum)(message);
		if (change_summary & MANAGER_CHANGE_RESULT(Spectrum))
		{
			std::list<Cmiss_region*>::iterator region_iter;
			for (region_iter = graphics_module->member_regions_list->begin();
				region_iter != graphics_module->member_regions_list->end(); ++region_iter)
			{
				Cmiss_region *region = *region_iter;
				Cmiss_scene *scene = Cmiss_graphics_module_get_scene(graphics_module, region);
				Cmiss_scene_begin_change(scene);
				Cmiss_scene_spectrum_change(scene, message);
				Cmiss_scene_end_change(scene);
				DEACCESS(Cmiss_scene)(&scene);
			}
		}
	}
}

/***************************************************************************//**
 * Callback for changes in the tessellation manager.
 * Informs all scenes about the changes.
 */
void Cmiss_graphics_module_tessellation_manager_callback(
	struct MANAGER_MESSAGE(Cmiss_tessellation) *message, void *graphics_module_void)
{
	Cmiss_graphics_module *graphics_module = (Cmiss_graphics_module *)graphics_module_void;
	if (message && graphics_module)
	{
		int change_summary = MANAGER_MESSAGE_GET_CHANGE_SUMMARY(Cmiss_tessellation)(message);
		if (change_summary & MANAGER_CHANGE_RESULT(Cmiss_tessellation))
		{
			std::list<Cmiss_region*>::iterator region_iter;
			for (region_iter = graphics_module->member_regions_list->begin();
				region_iter != graphics_module->member_regions_list->end(); ++region_iter)
			{
				Cmiss_region *region = *region_iter;
				Cmiss_scene *scene = Cmiss_graphics_module_get_scene(graphics_module, region);
				Cmiss_scene_begin_change(scene);
				Cmiss_scene_tessellation_change(scene, message);
				Cmiss_scene_end_change(scene);
				DEACCESS(Cmiss_scene)(&scene);
			}
		}
	}
}

void Cmiss_graphics_module_font_manager_callback(
	struct MANAGER_MESSAGE(Cmiss_font) *message, void *graphics_module_void)
{
	Cmiss_graphics_module *graphics_module = (Cmiss_graphics_module *)graphics_module_void;
	if (message && graphics_module)
	{
		int change_summary = MANAGER_MESSAGE_GET_CHANGE_SUMMARY(Cmiss_font)(message);
		if (change_summary & MANAGER_CHANGE_RESULT(Cmiss_font))
		{
			std::list<Cmiss_region*>::iterator region_iter;
			for (region_iter = graphics_module->member_regions_list->begin();
				region_iter != graphics_module->member_regions_list->end(); ++region_iter)
			{
				Cmiss_region *region = *region_iter;
				Cmiss_scene *scene = Cmiss_graphics_module_get_scene(graphics_module, region);
				Cmiss_scene_begin_change(scene);
				Cmiss_scene_font_change(scene, message);
				Cmiss_scene_end_change(scene);
				DEACCESS(Cmiss_scene)(&scene);
			}
		}
	}
}

}

struct Cmiss_graphics_module *Cmiss_graphics_module_create(
	struct Context *context)
{
	struct Cmiss_graphics_module *module;

	ENTER(Cmiss_rendtion_graphics_module_create);
	if (context)
	{
		if (ALLOCATE(module, struct Cmiss_graphics_module, 1))
		{
			module->light_manager = NULL;
			module->material_module = NULL;
			module->glyph_module = Cmiss_glyph_module_create();
			module->default_light = NULL;
			module->default_light_model = NULL;
			module->scene_viewer_module = NULL;
			module->light_manager=CREATE(MANAGER(Light))();
			module->spectrum_module=Cmiss_spectrum_module_create();
			module->graphics_filter_module=Cmiss_graphics_filter_module_create();
			module->font_module = Cmiss_font_module_create();
			module->font_manager_callback_id =
				MANAGER_REGISTER(Cmiss_font)(Cmiss_graphics_module_font_manager_callback,
					(void *)module, Cmiss_font_module_get_manager(module->font_module));
			module->root_region = Cmiss_context_get_default_region(context);
			module->material_module = Cmiss_graphics_material_module_create(
					Cmiss_spectrum_module_get_manager(module->spectrum_module));
			module->material_manager_callback_id =
				MANAGER_REGISTER(Graphical_material)(Cmiss_graphics_module_material_manager_callback,
					(void *)module, Cmiss_graphics_material_module_get_manager(module->material_module));
			module->spectrum_manager_callback_id =
				MANAGER_REGISTER(Spectrum)(Cmiss_graphics_module_spectrum_manager_callback,
					(void *)module, Cmiss_spectrum_module_get_manager(module->spectrum_module));
			module->light_model_manager = CREATE(MANAGER(Light_model)());
			module->element_point_ranges_selection = Cmiss_context_get_element_point_ranges_selection(context);
			module->default_time_keeper = Cmiss_context_get_default_time_keeper(context);
			module->tessellation_module = Cmiss_tessellation_module_create();
			module->member_regions_list = new std::list<Cmiss_region*>;
			module->tessellation_manager_callback_id =
				MANAGER_REGISTER(Cmiss_tessellation)(Cmiss_graphics_module_tessellation_manager_callback,
					(void *)module, Cmiss_tessellation_module_get_manager(module->tessellation_module));
			module->access_count = 1;
		}
		else
		{
			module = (Cmiss_graphics_module *)NULL;
			display_message(ERROR_MESSAGE,
			"Cmiss_rendtion_graphics_module_create. Not enough memory for Cmiss scene graphics module");
		}
	}
	else
	{
		module = (Cmiss_graphics_module *)NULL;
		display_message(ERROR_MESSAGE,"Cmiss_rendtion_graphics_module_create.  Invalid argument(s)");
	}
	LEAVE;

	return (module);
}

struct Cmiss_graphics_material_module *Cmiss_graphics_module_get_material_module(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->material_module)
	{
		return Cmiss_graphics_material_module_access(graphics_module->material_module);
	}

	return 0;
}

struct Cmiss_graphics_module *Cmiss_graphics_module_access(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module)
	{
		graphics_module->access_count++;
	}
	return graphics_module;
}

int Cmiss_graphics_module_remove_member_regions_scene(
	struct Cmiss_graphics_module *graphics_module)
{
	int return_code = 0;

	if (graphics_module && graphics_module->member_regions_list)
	{
		std::list<Cmiss_region*>::iterator pos;
		for (pos = graphics_module->member_regions_list->begin();
				pos != graphics_module->member_regions_list->end(); ++pos)
		{
			// clean up scene between begin/end change so fields and other objects
			// destroyed when scene destroyed do not cause messages to be sent
			Cmiss_region_begin_change(*pos);
			Cmiss_region_deaccess_scene(*pos);
			Cmiss_region_end_change(*pos);
		}
		graphics_module->member_regions_list->clear();
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_remove_member_regions_scene.  "
			"Invalid argument(s)");
	}

	return return_code;
}

int Cmiss_graphics_module_destroy(
	struct Cmiss_graphics_module **graphics_module_address)
{
	int return_code = 0;
	struct Cmiss_graphics_module *graphics_module = NULL;

	if (NULL != (graphics_module = *graphics_module_address))
	{
		graphics_module->access_count--;
		if (0 == graphics_module->access_count)
		{
			if (graphics_module->root_region)
				Cmiss_region_destroy(&graphics_module->root_region);
			MANAGER_DEREGISTER(Graphical_material)(
				graphics_module->material_manager_callback_id,
				Cmiss_graphics_material_module_get_manager(graphics_module->material_module));
			MANAGER_DEREGISTER(Spectrum)(
				graphics_module->spectrum_manager_callback_id, Cmiss_spectrum_module_get_manager(graphics_module->spectrum_module));
			MANAGER_DEREGISTER(Cmiss_tessellation)(
				graphics_module->tessellation_manager_callback_id,
				Cmiss_tessellation_module_get_manager(graphics_module->tessellation_module));
			MANAGER_DEREGISTER(Cmiss_font)(
				graphics_module->font_manager_callback_id,
				Cmiss_font_module_get_manager(graphics_module->font_module));
			/* This will remove all callbacks used by the scene_viewer projection_field callback */
			Cmiss_scene_viewer_module_destroy(&graphics_module->scene_viewer_module);
			if (graphics_module->member_regions_list)
			{
				Cmiss_graphics_module_remove_member_regions_scene(graphics_module);
				delete graphics_module->member_regions_list;
			}
			Cmiss_glyph_module_destroy(&graphics_module->glyph_module);
			if (graphics_module->default_light)
				DEACCESS(Light)(&graphics_module->default_light);
			if (graphics_module->light_manager)
				DESTROY(MANAGER(Light))(&graphics_module->light_manager);
			if (graphics_module->default_light_model)
				DEACCESS(Light_model)(&graphics_module->default_light_model);
			if (graphics_module->light_model_manager)
				DESTROY(MANAGER(Light_model))(&graphics_module->light_model_manager);
			if (graphics_module->spectrum_module)
				Cmiss_spectrum_module_destroy(&graphics_module->spectrum_module);
			if (graphics_module->font_module)
				Cmiss_font_module_destroy(&graphics_module->font_module);
			if (graphics_module->material_module)
				Cmiss_graphics_material_module_destroy(&graphics_module->material_module);
			if (graphics_module->graphics_filter_module)
				Cmiss_graphics_filter_module_destroy(&graphics_module->graphics_filter_module);
			if (graphics_module->default_time_keeper)
				Cmiss_time_keeper_destroy(&graphics_module->default_time_keeper);
			if (graphics_module->tessellation_module)
				Cmiss_tessellation_module_destroy(&graphics_module->tessellation_module);
			DEALLOCATE(*graphics_module_address);
		}
		*graphics_module_address = NULL;
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_destroy.  Missing graphics module");
		return_code = 0;
	}

	return return_code;
}

int Cmiss_graphics_module_create_scene(
	struct Cmiss_graphics_module *graphics_module,
	struct Cmiss_region *cmiss_region)
{
	struct Cmiss_scene *scene;
	int return_code;

	ENTER(Cmiss_region_add_scene);
	if (cmiss_region && graphics_module)
	{
		scene = FIRST_OBJECT_IN_LIST_THAT(ANY_OBJECT(Cmiss_scene))(
			(ANY_OBJECT_CONDITIONAL_FUNCTION(Cmiss_scene) *)NULL, (void *)NULL,
			Cmiss_region_private_get_any_object_list(cmiss_region));
		if (!(scene))
		{
			if (NULL != (scene = Cmiss_scene_create_internal(cmiss_region, graphics_module)))
			{
				Cmiss_scene_set_position(scene, 1);
				return_code = 1;
			}
			else
			{
				return_code = 0;
				display_message(ERROR_MESSAGE,
					"Cmiss_region_add_scene. Cannot create scene for region");
			}
		}
		else
		{
			return_code = 1;
			//ACCESS or not ?
			//ACCESS(Cmiss_scene)(scene);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_region_add_scene. Invalid argument(s).");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
}

struct MANAGER(Light) *Cmiss_graphics_module_get_light_manager(
	struct Cmiss_graphics_module *graphics_module)
{
	struct MANAGER(Light) *light_manager = NULL;
	if (graphics_module)
	{
		light_manager = graphics_module->light_manager;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_light_manager.  Invalid argument(s)");
	}
	return light_manager;
}

struct Light *Cmiss_graphics_module_get_default_light(
	struct Cmiss_graphics_module *graphics_module)
{
	struct Light *light = NULL;
	if (graphics_module && graphics_module->light_manager)
	{
		if (!graphics_module->default_light)
		{
			graphics_module->default_light=CREATE(Light)("default");
			if (graphics_module->default_light)
			{
				GLfloat default_light_direction[3]={0.0,-0.5,-1.0};
				struct Colour default_colour;
				set_Light_type(graphics_module->default_light,INFINITE_LIGHT);
				default_colour.red=1.0;
				default_colour.green=1.0;
				default_colour.blue=1.0;
				set_Light_colour(graphics_module->default_light,&default_colour);

				set_Light_direction(graphics_module->default_light,default_light_direction);
				/*???DB.  Include default as part of manager ? */
				ACCESS(Light)(graphics_module->default_light);
				if (!ADD_OBJECT_TO_MANAGER(Light)(graphics_module->default_light,
						graphics_module->light_manager))
				{
					DEACCESS(Light)(&(graphics_module->default_light));
				}
			}
		}
		if (graphics_module->default_light)
		{
			light = ACCESS(Light)(graphics_module->default_light);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_default_light.  Invalid argument(s)");
	}

	return light;
}

struct MANAGER(Spectrum) *Cmiss_graphics_module_get_spectrum_manager(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->spectrum_module)
	{
		return Cmiss_spectrum_module_get_manager(graphics_module->spectrum_module);
	}

	return NULL;
}

Cmiss_spectrum_module_id Cmiss_graphics_module_get_spectrum_module(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->spectrum_module)
	{
		return Cmiss_spectrum_module_access(graphics_module->spectrum_module);
	}
	return 0;
}

Cmiss_spectrum_id Cmiss_graphics_module_find_spectrum_by_name(
	Cmiss_graphics_module_id graphics_module, const char *name)
{
	Cmiss_spectrum_id spectrum = NULL;

	if (graphics_module && graphics_module->spectrum_module && name)
	{
		return Cmiss_spectrum_module_find_spectrum_by_name(
			graphics_module->spectrum_module, name);
	}
	return 0;

	return spectrum;
}

Cmiss_spectrum_id Cmiss_graphics_module_create_spectrum(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->spectrum_module)
	{
		return Cmiss_spectrum_module_create_spectrum(
			graphics_module->spectrum_module);
	}
	return 0;
}

struct Cmiss_spectrum *Cmiss_graphics_module_get_default_spectrum(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->spectrum_module)
	{
		return Cmiss_spectrum_module_get_default_spectrum(
			graphics_module->spectrum_module);
	}

	return 0;
}

int Cmiss_graphics_module_define_standard_materials(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->material_module)
	{
		return Cmiss_graphics_material_module_define_standard_materials(graphics_module->material_module);
	}
	return 0;
}

struct Cmiss_font *Cmiss_graphics_module_get_default_font(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->font_module)
	{
		return Cmiss_font_module_get_default_font(graphics_module->font_module);
	}
	return 0;
}

Cmiss_font_module_id Cmiss_graphics_module_get_font_module(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->font_module)
	{
		return Cmiss_font_module_access(graphics_module->font_module);
	}
	return 0;
}

Cmiss_font_id Cmiss_graphics_module_find_font_by_name(
	Cmiss_graphics_module_id graphics_module, const char *name)
{
	Cmiss_font_id font = NULL;

	if (graphics_module && graphics_module->font_module && name)
	{
		return Cmiss_font_module_find_font_by_name(graphics_module->font_module, name);
	}
	return 0;

	return font;
}

Cmiss_font_id Cmiss_graphics_module_create_font(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->font_module)
	{
		return Cmiss_font_module_create_font(
			graphics_module->font_module);
	}
	return 0;
}

struct MANAGER(Cmiss_font) *Cmiss_graphics_module_get_font_manager(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->font_module)
	{
		return Cmiss_font_module_get_manager(graphics_module->font_module);
	}
	return 0;
}

Cmiss_glyph_module_id Cmiss_graphics_module_get_glyph_module(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module)
	{
		Cmiss_glyph_module_access(graphics_module->glyph_module);
		return graphics_module->glyph_module;
	}
	return 0;
}

Cmiss_scene_viewer_module_id Cmiss_graphics_module_get_scene_viewer_module(
	Cmiss_graphics_module_id graphics_module)
{
	Cmiss_scene_viewer_module *scene_viewer_module = NULL;
	if (graphics_module)
	{
		if (!graphics_module->scene_viewer_module)
		{
			struct Light *default_light =
				Cmiss_graphics_module_get_default_light(graphics_module);
			struct Light_model *default_light_model =
				Cmiss_graphics_module_get_default_light_model(graphics_module);
			Colour default_background_colour;
			default_background_colour.red = 0.0;
			default_background_colour.green = 0.0;
			default_background_colour.blue = 0.0;
			Cmiss_graphics_filter_module_id filterModule = Cmiss_graphics_module_get_filter_module(graphics_module);
			graphics_module->scene_viewer_module = CREATE(Cmiss_scene_viewer_module)(
				&default_background_colour,
				/* interactive_tool_manager */0,
				Cmiss_graphics_module_get_light_manager(graphics_module), default_light,
				Cmiss_graphics_module_get_light_model_manager(graphics_module), default_light_model,
				filterModule);
			Cmiss_graphics_filter_module_destroy(&filterModule);
			DEACCESS(Light_model)(&default_light_model);
			DEACCESS(Light)(&default_light);
		}
		scene_viewer_module = Cmiss_scene_viewer_module_access(graphics_module->scene_viewer_module);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_default_scene_viewer_module.  "
			"Missing context");
	}
	return scene_viewer_module;
}

struct MANAGER(Light_model) *Cmiss_graphics_module_get_light_model_manager(
	struct Cmiss_graphics_module *graphics_module)
{
	struct MANAGER(Light_model) *light_model_manager = NULL;
	if (graphics_module)
	{
		light_model_manager = graphics_module->light_model_manager;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_light_model_manager.  Invalid argument(s)");
	}

	return light_model_manager;
}

struct Light_model *Cmiss_graphics_module_get_default_light_model(
	struct Cmiss_graphics_module *graphics_module)
{
	struct Light_model *light_model = NULL;

	if (graphics_module)
	{
		if (!graphics_module->default_light_model)
		{
			if (NULL != (graphics_module->default_light_model=CREATE(Light_model)("default")))
			{
				struct Colour ambient_colour;
				ambient_colour.red=0.2f;
				ambient_colour.green=0.2f;
				ambient_colour.blue=0.2f;
				Light_model_set_ambient(graphics_module->default_light_model,&ambient_colour);
				Light_model_set_side_mode(graphics_module->default_light_model,
					LIGHT_MODEL_TWO_SIDED);
				ACCESS(Light_model)(graphics_module->default_light_model);
				if (!ADD_OBJECT_TO_MANAGER(Light_model)(
							graphics_module->default_light_model,graphics_module->light_model_manager))
				{
					DEACCESS(Light_model)(&(graphics_module->default_light_model));
				}
			}
		}
		if (graphics_module->default_light_model)
			light_model = ACCESS(Light_model)(graphics_module->default_light_model);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_default_light_model.  Invalid argument(s)");
	}
	return light_model;
}

Cmiss_tessellation_module_id Cmiss_graphics_module_get_tessellation_module(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->tessellation_module)
	{
		return Cmiss_tessellation_module_access(graphics_module->tessellation_module);
	}
	return 0;
}

struct Cmiss_tessellation* Cmiss_graphics_module_find_tessellation_by_name(
	Cmiss_graphics_module_id graphics_module, const char *name)
{
	if (graphics_module && graphics_module->tessellation_module && name)
	{
		return Cmiss_tessellation_module_find_tessellation_by_name(
			graphics_module->tessellation_module, name);
	}
	return 0;
}

Cmiss_tessellation_id Cmiss_graphics_module_create_tessellation(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->tessellation_module)
	{
		return Cmiss_tessellation_module_create_tessellation(
			graphics_module->tessellation_module);
	}
	return 0;
}

struct Cmiss_time_keeper *Cmiss_graphics_module_get_time_keeper_internal(
	struct Cmiss_graphics_module *module)
{
	struct Cmiss_time_keeper *time_keeper = NULL;

	if (module && module->default_time_keeper)
	{
		time_keeper = Cmiss_time_keeper_access(module->default_time_keeper);
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_time_keeper_internal.  Invalid argument(s)");
	}

	return time_keeper;
}

Cmiss_scene_id Cmiss_graphics_module_get_default_scene(struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module)
	{
		Cmiss_scene_id scene = Cmiss_region_get_scene_internal(graphics_module->root_region);
		if (scene == 0)
		{
			Cmiss_graphics_module_enable_scenes(graphics_module, graphics_module->root_region);
			scene = Cmiss_region_get_scene_internal(graphics_module->root_region);
		}
		return scene;
	}

	return 0;
}

int Cmiss_graphics_module_enable_scenes(
	struct Cmiss_graphics_module *graphics_module,
	struct Cmiss_region *cmiss_region)
{
	int return_code;
	struct Cmiss_region *child_region;

	if (cmiss_region && graphics_module)
	{
		return_code = Cmiss_graphics_module_create_scene(
			graphics_module, cmiss_region);
		if (return_code)
		{
			struct Cmiss_scene *scene =
				Cmiss_region_get_scene_internal(cmiss_region);
			Cmiss_scene_add_callback(scene, Cmiss_scene_update_callback,
				(void *)NULL);
			child_region = Cmiss_region_get_first_child(cmiss_region);
			while (child_region)
			{
				return_code = Cmiss_graphics_module_enable_scenes(
					graphics_module, child_region);
				/* add callback to call from child scene to parent scene */
				struct Cmiss_scene *child;
				if (scene && (NULL != (child = Cmiss_region_get_scene_internal(child_region))))
				{
					Cmiss_scene_add_callback(child,
						Cmiss_scene_notify_parent_scene_callback,
						(void *)cmiss_region);
					DEACCESS(Cmiss_scene)(&child);
				}
				Cmiss_region_reaccess_next_sibling(&child_region);
			}
			DEACCESS(Cmiss_scene)(&scene);
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,"Cmiss_scene.  "
			"Invalid argument(s)");
		return_code=0;
	}

	return (return_code);
}

struct Element_point_ranges_selection *Cmiss_graphics_module_get_element_point_ranges_selection(
	struct Cmiss_graphics_module *graphics_module)
{
	struct Element_point_ranges_selection *element_point_ranges_selection = NULL;

	if (graphics_module && graphics_module->element_point_ranges_selection)
	{
		element_point_ranges_selection = graphics_module->element_point_ranges_selection;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_get_element_point_ranges_selection.  Invalid argument(s)");
	}

	return (element_point_ranges_selection);
}

Cmiss_scene_id Cmiss_graphics_module_get_scene(
	Cmiss_graphics_module_id graphics_module, Cmiss_region_id region)
{
	struct Cmiss_scene *scene = NULL;
	if (graphics_module && region)
	{
		scene = Cmiss_region_get_scene_internal(region);
		if (!scene)
		{
			char *region_path = Cmiss_region_get_path(region);
			display_message(ERROR_MESSAGE,
				"Cmiss_graphics_module_get_scene.  Scene not found for region %s", region_path);
			DEALLOCATE(region_path);
		}
	}
	return scene;
}

Cmiss_graphics_material_id Cmiss_graphics_module_find_material_by_name(
	Cmiss_graphics_module_id graphics_module, const char *name)
{
	if (graphics_module && graphics_module->material_module && name)
	{
		return Cmiss_graphics_material_module_find_material_by_name(
			graphics_module->material_module, name);
	}
	return 0;
}

Cmiss_graphics_material_id Cmiss_graphics_module_create_material(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->material_module)
	{
		return Cmiss_graphics_material_module_create_material(
			graphics_module->material_module);
	}
	return 0;
}

int Cmiss_graphics_module_add_member_region(
	struct Cmiss_graphics_module *graphics_module, struct Cmiss_region *region)
{
	int return_code = 0;

	if (graphics_module && region)
	{
		if (graphics_module->member_regions_list)
		{
			graphics_module->member_regions_list->push_back(region);
			return_code = 1;
		}
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_add_member_region.  Invalid argument(s)");
	}

	return return_code;
}

int Cmiss_graphics_module_remove_member_region(
		struct Cmiss_graphics_module *graphics_module, struct Cmiss_region *region)
{
	int return_code;

	ENTER(Cmiss_graphics_module_remove_member_region);
	if (graphics_module && graphics_module->member_regions_list && region)
	{
		graphics_module->member_regions_list->remove(region);
		return_code = 1;
	}
	else
	{
		display_message(ERROR_MESSAGE,
			"Cmiss_graphics_module_remove_member_region.  Invalid argument(s)");
		return_code = 0;
	}
	LEAVE;

	return (return_code);
}

Cmiss_graphics_filter_module_id Cmiss_graphics_module_get_filter_module(
	struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_access(graphics_module->graphics_filter_module);
	}
	return 0;
}

struct MANAGER(Cmiss_graphics_filter) *Cmiss_graphics_module_get_filter_manager(
		struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_get_manager(graphics_module->graphics_filter_module);
	}
	return 0;
}

Cmiss_graphics_filter *Cmiss_graphics_module_get_default_filter(
		struct Cmiss_graphics_module *graphics_module)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_get_default_filter(
			graphics_module->graphics_filter_module);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_find_filter_by_name(
	Cmiss_graphics_module_id graphics_module, const char *name)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_find_filter_by_name(
			graphics_module->graphics_filter_module, name);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_create_filter_visibility_flags(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_create_filter_visibility_flags(
			graphics_module->graphics_filter_module);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_create_filter_graphic_name(
	Cmiss_graphics_module_id graphics_module, const char *match_name)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_create_filter_graphic_name(
			graphics_module->graphics_filter_module, match_name);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_create_filter_graphic_type(
	Cmiss_graphics_module_id graphics_module, enum Cmiss_graphic_type graphic_type)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_create_filter_graphic_type(
			graphics_module->graphics_filter_module, graphic_type);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_create_filter_region(
	Cmiss_graphics_module_id graphics_module, Cmiss_region_id match_region)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_create_filter_region(
			graphics_module->graphics_filter_module, match_region);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_create_filter_operator_and(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_create_filter_operator_and(
			graphics_module->graphics_filter_module);
	}
	return 0;
}

Cmiss_graphics_filter_id Cmiss_graphics_module_create_filter_operator_or(
	Cmiss_graphics_module_id graphics_module)
{
	if (graphics_module && graphics_module->graphics_filter_module)
	{
		return Cmiss_graphics_filter_module_create_filter_operator_or(
			graphics_module->graphics_filter_module);
	}
	return 0;
}
