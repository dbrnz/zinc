/*
 * OpenCMISS-Zinc Library Unit Tests
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include <opencmiss/zinc/core.h>
#include <opencmiss/zinc/context.h>
#include <opencmiss/zinc/sceneviewer.h>

#include <opencmiss/zinc/context.hpp>
#include <opencmiss/zinc/light.hpp>
#include <opencmiss/zinc/sceneviewer.hpp>

#include "zinctestsetup.hpp"
#include "zinctestsetupcpp.hpp"

#include "test_resources.h"

TEST(cmzn_sceneviewer_api, destroy_context_before_scene_viewer)
{
	 cmzn_context_id context = cmzn_context_create("test");
	 cmzn_sceneviewermodule_id svModule = cmzn_context_get_sceneviewermodule(context);
	 cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svModule,
	  CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);
	 cmzn_sceneviewermodule_destroy(&svModule);
	 cmzn_context_destroy(&context);
	 cmzn_sceneviewer_destroy(&sv);
}

TEST(cmzn_sceneviewer_api, set_background_invalid_args)
{
	ZincTestSetup z;

	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_background_colour_component_rgb(0, 0.0, 0.0, 0.0));

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_background_colour_rgb(0, 0));

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_background_colour_rgb(sv, 0));

	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_background_colour_component_rgb(sv, -1.0, 0.2, 0.8));

	const double rgb[3] = {-0.3, -1.0, 2.99};
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_background_colour_rgb(sv, rgb));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, set_background_valid_args)
{
	ZincTestSetup z;

	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);

	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_background_colour_component_rgb(sv, 0.5, 0.2, 0.8));

	const double rgb[3] = {0.3, 1.0, 0.99};
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_background_colour_rgb(sv, rgb));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, get_background_rgb_invalid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_background_colour_rgb(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_background_colour_rgb(sv, 0));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, get_background_rgb)
{
	ZincTestSetup z;
	double rgb[3] = {0.0, 0.0, 0.0};

	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_background_colour_component_rgb(sv, 0.5, 0.2, 0.8));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_get_background_colour_rgb(sv, rgb));

	EXPECT_EQ(0.5, rgb[0]);
	EXPECT_EQ(0.2, rgb[1]);
	EXPECT_EQ(0.8, rgb[2]);

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer, lookat_parameters)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	double eyeOut[3], lookatOut[3], upvectorOut[3];
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_get_lookat_parameters(sv, eyeOut, lookatOut, upvectorOut));
	const double eyeIn[] = { -5.0, -5.0, 0.0 };
	const double lookatIn[] = { 2.0, 2.0, 0.0 };
	const double upvectorIn[] = { 0.0, 0.0, 5.0 };
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_lookat_parameters_non_skew(sv, eyeIn, lookatIn, upvectorIn));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_get_lookat_parameters(sv, eyeOut, lookatOut, upvectorOut));
	for (int i = 0; i < 3; ++i)
	{
		ASSERT_DOUBLE_EQ(eyeIn[i], eyeOut[i]);
		ASSERT_DOUBLE_EQ(lookatIn[i], lookatOut[i]);
		ASSERT_DOUBLE_EQ(upvectorIn[i] / 5.0, upvectorOut[i]);
	}
	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(ZincSceneviewer, LookatParameters)
{
	ZincTestSetupCpp z;
	Sceneviewermodule svm = z.context.getSceneviewermodule();
	Sceneviewer sv = svm.createSceneviewer(Sceneviewer::BUFFERING_MODE_DEFAULT, Sceneviewer::STEREO_MODE_DEFAULT);

	double eyeOut[3], lookatOut[3], upvectorOut[3];
	EXPECT_EQ(CMZN_OK, sv.getLookatParameters(eyeOut, lookatOut, upvectorOut));
	const double eyeIn[] = { -5.0, -5.0, 0.0 };
	const double lookatIn[] = { 2.0, 2.0, 0.0 };
	const double upvectorIn[] = { 0.0, 0.0, 5.0 };
	EXPECT_EQ(CMZN_OK, sv.setLookatParametersNonSkew(eyeIn, lookatIn, upvectorIn));
	EXPECT_EQ(CMZN_OK, sv.getLookatParameters(eyeOut, lookatOut, upvectorOut));
	for (int i = 0; i < 3; ++i)
	{
		ASSERT_DOUBLE_EQ(eyeIn[i], eyeOut[i]);
		ASSERT_DOUBLE_EQ(lookatIn[i], lookatOut[i]);
		ASSERT_DOUBLE_EQ(upvectorIn[i] / 5.0, upvectorOut[i]);
	}
}

TEST(cmzn_sceneviewer_api, eye_position_invalid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_eye_position(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_eye_position(sv, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_eye_position(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_eye_position(sv, 0));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, eye_position_valid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	const double eye[] = {3.0, 4.5, 6.7};
	double eyeOut[3];
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_eye_position(sv, eye));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_get_eye_position(sv, eyeOut));
	EXPECT_EQ(eye[0], eyeOut[0]);
	EXPECT_EQ(eye[1], eyeOut[1]);
	EXPECT_EQ(eye[2], eyeOut[2]);

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, lookat_position_invalid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_lookat_position(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_lookat_position(sv, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_lookat_position(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_lookat_position(sv, 0));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, lookat_position_valid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	const double lookat[] = {-2.0, 1.5, 16.7};
	double lookatOut[3];
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_lookat_position(sv, lookat));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_get_lookat_position(sv, lookatOut));
	EXPECT_EQ(lookat[0], lookatOut[0]);
	EXPECT_EQ(lookat[1], lookatOut[1]);
	EXPECT_EQ(lookat[2], lookatOut[2]);

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, up_vector_invalid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_up_vector(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_up_vector(sv, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_up_vector(0, 0));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_get_up_vector(sv, 0));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer_api, up_vector_valid_args)
{
	ZincTestSetup z;
	cmzn_sceneviewermodule_id svm = cmzn_context_get_sceneviewermodule(z.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svm, CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	const double upVector[] = {2.0, 0.0, 0.0};
	double upVectorOut[3];
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_up_vector(sv, upVector));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_get_up_vector(sv, upVectorOut));
	EXPECT_EQ(1.0, upVectorOut[0]);
	EXPECT_EQ(upVector[1], upVectorOut[1]);
	EXPECT_EQ(upVector[2], upVectorOut[2]);

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svm);
}

TEST(cmzn_sceneviewer, get_set)
{
	ZincTestSetup zinc;

	cmzn_sceneviewermodule_id svModule = cmzn_context_get_sceneviewermodule(zinc.context);
	cmzn_sceneviewer_id sv = cmzn_sceneviewermodule_create_sceneviewer(svModule,
		CMZN_SCENEVIEWER_BUFFERING_MODE_DEFAULT, CMZN_SCENEVIEWER_STEREO_MODE_DEFAULT);

	cmzn_sceneviewer_interact_mode interact_mode = cmzn_sceneviewer_get_interact_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_INTERACT_MODE_STANDARD, interact_mode);
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_interact_mode(sv, CMZN_SCENEVIEWER_INTERACT_MODE_2D));
	interact_mode = cmzn_sceneviewer_get_interact_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_INTERACT_MODE_2D, interact_mode);

	cmzn_sceneviewer_viewport_mode viewport_mode = cmzn_sceneviewer_get_viewport_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_VIEWPORT_MODE_RELATIVE, viewport_mode);
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_viewport_mode(sv, CMZN_SCENEVIEWER_VIEWPORT_MODE_ABSOLUTE));
	viewport_mode = cmzn_sceneviewer_get_viewport_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_VIEWPORT_MODE_ABSOLUTE, viewport_mode);

	cmzn_sceneviewer_projection_mode projection_mode = cmzn_sceneviewer_get_projection_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_PROJECTION_MODE_PERSPECTIVE, projection_mode);
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_projection_mode(sv, CMZN_SCENEVIEWER_PROJECTION_MODE_PARALLEL));
	projection_mode = cmzn_sceneviewer_get_projection_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_PROJECTION_MODE_PARALLEL, projection_mode);

	cmzn_sceneviewer_blending_mode blending_mode = cmzn_sceneviewer_get_blending_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_BLENDING_MODE_NORMAL, blending_mode);
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_blending_mode(sv, CMZN_SCENEVIEWER_BLENDING_MODE_NONE));
	blending_mode = cmzn_sceneviewer_get_blending_mode(sv);
	EXPECT_EQ(CMZN_SCENEVIEWER_BLENDING_MODE_NONE, blending_mode);

	double value;
	ASSERT_DOUBLE_EQ(1.2309594173407747, value = cmzn_sceneviewer_get_view_angle(sv));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_view_angle(sv, 0.3));
	ASSERT_DOUBLE_EQ(0.3, value = cmzn_sceneviewer_get_view_angle(sv));

	int number;
	EXPECT_EQ(0, number = cmzn_sceneviewer_get_antialias_sampling(sv));
	EXPECT_EQ(CMZN_ERROR_ARGUMENT, cmzn_sceneviewer_set_antialias_sampling(sv, 3));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_antialias_sampling(sv, 8));
	EXPECT_EQ(8, number = cmzn_sceneviewer_get_antialias_sampling(sv));

	EXPECT_FALSE(cmzn_sceneviewer_get_perturb_lines_flag(sv));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_perturb_lines_flag(sv, true));
	EXPECT_TRUE(cmzn_sceneviewer_get_perturb_lines_flag(sv));

	ASSERT_DOUBLE_EQ(1.0, value = cmzn_sceneviewer_get_translation_rate(sv));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_translation_rate(sv, 2.0));
	ASSERT_DOUBLE_EQ(2.0, value = cmzn_sceneviewer_get_translation_rate(sv));

	ASSERT_DOUBLE_EQ(1.5, value = cmzn_sceneviewer_get_tumble_rate(sv));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_tumble_rate(sv, 3.0));
	ASSERT_DOUBLE_EQ(3.0, value = cmzn_sceneviewer_get_tumble_rate(sv));

	ASSERT_DOUBLE_EQ(1.0, value = cmzn_sceneviewer_get_zoom_rate(sv));
	EXPECT_EQ(CMZN_OK, cmzn_sceneviewer_set_zoom_rate(sv, 4.0));
	ASSERT_DOUBLE_EQ(4.0, value = cmzn_sceneviewer_get_zoom_rate(sv));
	
	ASSERT_DOUBLE_EQ(1000.0, value = cmzn_sceneviewer_get_far_clipping_plane(sv));
	EXPECT_EQ(OK, cmzn_sceneviewer_set_far_clipping_plane(sv, 700.0));
	ASSERT_DOUBLE_EQ(700.0, value = cmzn_sceneviewer_get_far_clipping_plane(sv));

	ASSERT_DOUBLE_EQ(0.1, value = cmzn_sceneviewer_get_near_clipping_plane(sv));
	EXPECT_EQ(OK, cmzn_sceneviewer_set_near_clipping_plane(sv, 100.0));
	ASSERT_DOUBLE_EQ(100.0, value = cmzn_sceneviewer_get_near_clipping_plane(sv));

	cmzn_sceneviewer_destroy(&sv);
	cmzn_sceneviewermodule_destroy(&svModule);
}

TEST(ZincSceneviewer, get_set_light)
{
	ZincTestSetupCpp zinc;

	Sceneviewermodule svModule = zinc.context.getSceneviewermodule();
	EXPECT_TRUE(svModule.isValid());

	Sceneviewer sv = svModule.createSceneviewer(
		Sceneviewer::BUFFERING_MODE_DEFAULT, Sceneviewer::STEREO_MODE_DEFAULT);
	EXPECT_TRUE(sv.isValid());

	EXPECT_FALSE(sv.isLightingLocalViewer());
	EXPECT_EQ(OK, sv.setLightingLocalViewer(true));
	EXPECT_TRUE(sv.isLightingLocalViewer());

	EXPECT_TRUE(sv.isLightingTwoSided());
	EXPECT_EQ(OK, sv.setLightingTwoSided(false));
	EXPECT_FALSE(sv.isLightingTwoSided());

	Lightmodule lm = zinc.context.getLightmodule();
	EXPECT_TRUE(lm.isValid());

	Light defaultLight = lm.getDefaultLight();
	EXPECT_TRUE(defaultLight.isValid());
	EXPECT_TRUE(sv.hasLight(defaultLight));

	Light defaultAmbientLight = lm.getDefaultAmbientLight();
	EXPECT_TRUE(defaultAmbientLight.isValid());
	EXPECT_TRUE(sv.hasLight(defaultAmbientLight));
	EXPECT_EQ(OK, sv.removeLight(defaultAmbientLight));
	EXPECT_FALSE(sv.hasLight(defaultAmbientLight));

	int result = lm.beginChange();
	EXPECT_EQ(OK, result);

	Light light = lm.createLight();
	EXPECT_TRUE(light.isValid());
	EXPECT_EQ(OK, light.setType(Light::TYPE_AMBIENT));

	result = lm.endChange();
	EXPECT_EQ(OK, result);

	EXPECT_FALSE(sv.hasLight(light));
	EXPECT_EQ(OK, sv.addLight(light));
	EXPECT_TRUE(sv.hasLight(light));
	EXPECT_EQ(ERROR_ALREADY_EXISTS, sv.addLight(light));
	EXPECT_EQ(OK, sv.removeLight(light));
	EXPECT_FALSE(sv.hasLight(light));
	EXPECT_EQ(ERROR_NOT_FOUND, sv.removeLight(light));
}

TEST(ZincSceneviewer, description_io)
{
	ZincTestSetupCpp zinc;

	Sceneviewermodule svModule = zinc.context.getSceneviewermodule();
	EXPECT_TRUE(svModule.isValid());
	Sceneviewer sv = svModule.createSceneviewer(
		Sceneviewer::BUFFERING_MODE_DEFAULT, Sceneviewer::STEREO_MODE_DEFAULT);
	EXPECT_TRUE(sv.isValid());

	void *buffer = 0;
	long length;
	FILE * f = fopen (TestResources::getLocation(TestResources::SCENEVIEWER_DESCRIPTION_JSON_RESOURCE), "rb");
	if (f)
	{
	  fseek (f, 0, SEEK_END);
	  length = ftell (f);
	  fseek (f, 0, SEEK_SET);
	  buffer = malloc (length);
	  if (buffer)
	  {
	    fread (buffer, 1, length, f);
	  }
	  fclose (f);
	}

	EXPECT_TRUE(buffer != 0);
	EXPECT_EQ(CMZN_OK, sv.readDescription((char *)buffer));

	free(buffer);

	double valuesOut3[3];
	EXPECT_EQ(CMZN_OK, sv.getBackgroundColourRGB(&valuesOut3[0]));
	EXPECT_DOUBLE_EQ(0.6, valuesOut3[0]);
	EXPECT_DOUBLE_EQ(0.55, valuesOut3[1]);
	EXPECT_DOUBLE_EQ(0.4, valuesOut3[2]);

	EXPECT_EQ(CMZN_OK, sv.getEyePosition(&valuesOut3[0]));
	EXPECT_DOUBLE_EQ(0.0, valuesOut3[0]);
	EXPECT_DOUBLE_EQ(1.0, valuesOut3[1]);
	EXPECT_DOUBLE_EQ(0.0, valuesOut3[2]);

	EXPECT_EQ(CMZN_OK, sv.getLookatPosition(&valuesOut3[0]));
	EXPECT_DOUBLE_EQ(1.0, valuesOut3[0]);
	EXPECT_DOUBLE_EQ(0.0, valuesOut3[1]);
	EXPECT_DOUBLE_EQ(0.0, valuesOut3[2]);

	EXPECT_EQ(CMZN_OK, sv.getUpVector(&valuesOut3[0]));
	EXPECT_DOUBLE_EQ(0.0, valuesOut3[0]);
	EXPECT_DOUBLE_EQ(0.0, valuesOut3[1]);
	EXPECT_DOUBLE_EQ(1.0, valuesOut3[2]);

	EXPECT_TRUE(sv.isLightingLocalViewer());

	EXPECT_FALSE(sv.isLightingTwoSided());

	EXPECT_EQ(Sceneviewer::PROJECTION_MODE_PARALLEL, sv.getProjectionMode());

	EXPECT_EQ(Sceneviewer::TRANSPARENCY_MODE_SLOW, sv.getTransparencyMode());

	double value;
	EXPECT_DOUBLE_EQ(0.3, value = sv.getViewAngle());

	int number;
	EXPECT_EQ(8, number = sv.getAntialiasSampling());

	EXPECT_TRUE(sv.getPerturbLinesFlag());

	EXPECT_DOUBLE_EQ(2.0, value = sv.getTranslationRate());

	EXPECT_DOUBLE_EQ(3.0, value = sv.getTumbleRate());

	EXPECT_DOUBLE_EQ(4.0, value = sv.getZoomRate());

	EXPECT_DOUBLE_EQ(700.0, value = sv.getFarClippingPlane());

	EXPECT_DOUBLE_EQ(100.0, value = sv.getNearClippingPlane());

	char *return_string = sv.writeDescription();
	EXPECT_TRUE(return_string != 0);
	cmzn_deallocate(return_string);
}

TEST(ZincSceneviewer, get_set)
{
	ZincTestSetupCpp zinc;

	Sceneviewermodule svModule = zinc.context.getSceneviewermodule();
	EXPECT_TRUE(svModule.isValid());
	Sceneviewer sv = svModule.createSceneviewer(
		Sceneviewer::BUFFERING_MODE_DEFAULT, Sceneviewer::STEREO_MODE_DEFAULT);
	EXPECT_TRUE(sv.isValid());

	Sceneviewer::ProjectionMode projectionMode = sv.getProjectionMode();
	EXPECT_EQ(Sceneviewer::PROJECTION_MODE_PERSPECTIVE, projectionMode);
	EXPECT_EQ(OK, sv.setProjectionMode(Sceneviewer::PROJECTION_MODE_PARALLEL));
	projectionMode = sv.getProjectionMode();
	EXPECT_EQ(Sceneviewer::PROJECTION_MODE_PARALLEL, projectionMode);

	double value;
	ASSERT_DOUBLE_EQ(1.2309594173407747, value = sv.getViewAngle());
	EXPECT_EQ(OK, sv.setViewAngle(0.3));
	ASSERT_DOUBLE_EQ(0.3, value = sv.getViewAngle());

	int number;
	EXPECT_EQ(0, number = sv.getAntialiasSampling());
	EXPECT_EQ(ERROR_ARGUMENT, sv.setAntialiasSampling(3));
	EXPECT_EQ(OK, sv.setAntialiasSampling(8));
	EXPECT_EQ(8, number = sv.getAntialiasSampling());

	EXPECT_FALSE(sv.getPerturbLinesFlag());
	EXPECT_EQ(OK, sv.setPerturbLinesFlag(true));
	EXPECT_TRUE(sv.getPerturbLinesFlag());

	ASSERT_DOUBLE_EQ(1.0, value = sv.getTranslationRate());
	EXPECT_EQ(OK, sv.setTranslationRate(2.0));
	ASSERT_DOUBLE_EQ(2.0, value = sv.getTranslationRate());

	ASSERT_DOUBLE_EQ(1.5, value = sv.getTumbleRate());
	EXPECT_EQ(OK, sv.setTumbleRate(3.0));
	ASSERT_DOUBLE_EQ(3.0, value = sv.getTumbleRate());

	ASSERT_DOUBLE_EQ(1.0, value = sv.getZoomRate());
	EXPECT_EQ(OK, sv.setZoomRate(4.0));
	ASSERT_DOUBLE_EQ(4.0, value = sv.getZoomRate());

	ASSERT_DOUBLE_EQ(1000.0, value = sv.getFarClippingPlane());
	EXPECT_EQ(OK, sv.setFarClippingPlane(700.0));
	ASSERT_DOUBLE_EQ(700.0, value = sv.getFarClippingPlane());

	ASSERT_DOUBLE_EQ(0.1, value = sv.getNearClippingPlane());
	EXPECT_EQ(OK, sv.setNearClippingPlane(100.0));
	ASSERT_DOUBLE_EQ(100.0, value = sv.getNearClippingPlane());
}

class mySceneviewercallback : public Sceneviewercallback
{
private:

	virtual void operator()(const Sceneviewerevent &sceneviewerevent)
	{
		EXPECT_EQ((Sceneviewerevent::CHANGE_FLAG_REPAINT_REQUIRED  | Sceneviewerevent::CHANGE_FLAG_TRANSFORM),
			sceneviewerevent.getChangeFlags());
	}

public:
	mySceneviewercallback() : Sceneviewercallback()
	{
	}
};

TEST(ZincSceneviewer, callback)
{
	ZincTestSetupCpp zinc;

	Sceneviewermodule svm = zinc.context.getSceneviewermodule();
	Sceneviewer sv = svm.createSceneviewer(Sceneviewer::BUFFERING_MODE_DOUBLE, Sceneviewer::STEREO_MODE_DEFAULT);
	double eyeValuesIn3[] = {-3, 7, 5};
	double lookatValuesIn3[] = {9, -11, 13};
	double upVectorValuesIn3[] = {13, 1, 2.5};
	Sceneviewernotifier sceneviewernotifier = sv.createSceneviewernotifier();
	EXPECT_TRUE(sceneviewernotifier.isValid());
	mySceneviewercallback thisNotifier;
	sceneviewernotifier.setCallback(thisNotifier);
	int result = 0;
	double rate = sv.getTranslationRate();
	EXPECT_EQ(CMZN_OK, result = sv.beginChange());
	EXPECT_EQ(CMZN_OK, result = sv.setTranslationRate(rate * 0.99));
	EXPECT_EQ(CMZN_OK, result = sv.setLookatParametersNonSkew(eyeValuesIn3, lookatValuesIn3, upVectorValuesIn3));
	EXPECT_EQ(CMZN_OK, result = sv.endChange());
	EXPECT_EQ(CMZN_OK, result = sceneviewernotifier.clearCallback());
}

TEST(ZincSceneviewermodule, defaultBackgroundColour)
{
	ZincTestSetupCpp zinc;

	Sceneviewermodule svm = zinc.context.getSceneviewermodule();
	EXPECT_TRUE(svm.isValid());
	const double oldColour[3] = { 0.0, 0.0, 0.0 };
	double colour[3];
	EXPECT_EQ(OK, svm.getDefaultBackgroundColourRGB(colour));
	EXPECT_EQ(oldColour[0], colour[0]);
	EXPECT_EQ(oldColour[1], colour[1]);
	EXPECT_EQ(oldColour[2], colour[2]);
	Sceneviewer sv1 = svm.createSceneviewer(Sceneviewer::BUFFERING_MODE_DOUBLE, Sceneviewer::STEREO_MODE_DEFAULT);
	EXPECT_TRUE(sv1.isValid());
	EXPECT_EQ(OK, sv1.getBackgroundColourRGB(colour));
	EXPECT_EQ(oldColour[0], colour[0]);
	EXPECT_EQ(oldColour[1], colour[1]);
	EXPECT_EQ(oldColour[2], colour[2]);

	const double newColour[3] = { 1.0, 1.0, 1.0 };
	EXPECT_EQ(OK, svm.setDefaultBackgroundColourRGB(newColour));
	EXPECT_EQ(OK, svm.getDefaultBackgroundColourRGB(colour));
	EXPECT_EQ(newColour[0], colour[0]);
	EXPECT_EQ(newColour[1], colour[1]);
	EXPECT_EQ(newColour[2], colour[2]);
	Sceneviewer sv2 = svm.createSceneviewer(Sceneviewer::BUFFERING_MODE_DOUBLE, Sceneviewer::STEREO_MODE_DEFAULT);
	EXPECT_TRUE(sv2.isValid());
	EXPECT_EQ(OK, sv2.getBackgroundColourRGB(colour));
	EXPECT_EQ(newColour[0], colour[0]);
	EXPECT_EQ(newColour[1], colour[1]);
	EXPECT_EQ(newColour[2], colour[2]);
}
