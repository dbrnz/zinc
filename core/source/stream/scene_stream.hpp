/***************************************************************************//**
 * FILE : scene_stream.hpp
 *
 * The private interface to cmzn_streaminformation_scene.
 *
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined (CMZN_SCENE_STREAM_HPP)
#define CMZN_SCENE_STREAM_HPP

#include "graphics/scene.h"
#include "zinc/scenefilter.h"
#include "zinc/scenepicker.h"
#include "stream/stream_private.hpp"

struct cmzn_streaminformation_scene : cmzn_streaminformation
{
public:

	cmzn_streaminformation_scene(cmzn_scene_id scene_in) : scene(scene_in),
		scenefilter(0), numberOfTimeSteps(0), initialTime(0.0), finishTime(0.0)
	{
		cmzn_scene_access(scene_in);
		format = CMZN_STREAMINFORMATION_SCENE_EXPORT_FORMAT_INVALID;
		data_type = CMZN_STREAMINFORMATION_SCENE_EXPORT_DATA_TYPE_COLOUR;
	}

	virtual ~cmzn_streaminformation_scene()
	{
		cmzn_scene_destroy(&scene);
		cmzn_scenefilter_destroy(&scenefilter);
	}

	cmzn_scene_id getScene()
	{
		return cmzn_scene_access(scene);
	}

	double getInitialTime()
	{
		return initialTime;
	}

	int setInitialTime(double initialTimeIn)
	{
		initialTime = initialTimeIn;
		return CMZN_OK;
	}

	double getFinishTime()
	{
		return finishTime;
	}

	int setFinishTime(double finishTimeIn)
	{
		finishTime = finishTimeIn;
		return CMZN_OK;
	}

	int getNumberOfResourcesRequired()
	{
		if (format == CMZN_STREAMINFORMATION_SCENE_EXPORT_FORMAT_THREEJS)
			return Scene_get_number_of_graphics_with_type_in_tree(
				scene, scenefilter, CMZN_GRAPHICS_TYPE_SURFACES);
		else
			return 0;
	}

	int getNumberOfTimeSteps()
	{
		return numberOfTimeSteps;
	}

	int setNumberOfTimeSteps(int numberOfTimeStepsIn)
	{
		numberOfTimeSteps = numberOfTimeStepsIn;
		return CMZN_OK;
	}

	cmzn_scenefilter_id getScenefilter()
	{
		if (scenefilter)
			return cmzn_scenefilter_access(scenefilter);
		return 0;
	}

	int setScenefilter(cmzn_scenefilter_id scenefilter_in)
	{
		if (scenefilter)
			cmzn_scenefilter_destroy(&scenefilter);
		scenefilter = cmzn_scenefilter_access(scenefilter_in);
		return CMZN_OK;
	}

	cmzn_streaminformation_scene_export_format getExportFormat()
	{
		return format;
	}

	int setExportFormat(cmzn_streaminformation_scene_export_format formatIn)
	{
		format = formatIn;
		return CMZN_OK;
	}

	cmzn_streaminformation_scene_export_data_type getExportDataType()
	{
		return data_type;
	}

	int setExportDataType(cmzn_streaminformation_scene_export_data_type dataTypeIn)
	{
		data_type = dataTypeIn;
		return CMZN_OK;
	}

private:
	cmzn_scene_id scene;
	cmzn_scenefilter_id scenefilter;
	int numberOfTimeSteps;
	double initialTime, finishTime;
	enum cmzn_streaminformation_scene_export_format format;
	enum cmzn_streaminformation_scene_export_data_type data_type;
};


#endif /* CMZN_REGION_STREAM_HPP */