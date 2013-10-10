#ifndef __ZINCTESTSETUPCPP_HPP__
#define __ZINCTESTSETUPCPP_HPP__

#include <gtest/gtest.h>

#include <zinc/status.hpp>
#include <zinc/context.hpp>
#include <zinc/region.hpp>
#include <zinc/fieldmodule.hpp>
#include <zinc/glyph.hpp>
#include <zinc/graphicsmodule.hpp>
#include <zinc/scene.hpp>

using namespace OpenCMISS::Zinc;

class ZincTestSetupCpp
{
public:
	Context context;
	Region root_region;
	Fieldmodule fm;
	GraphicsModule gm;
	Glyphmodule glyphmodule;
	Scene scene;

	ZincTestSetupCpp() :
		context("test"),
		root_region(context.getDefaultRegion()),
		fm(root_region.getFieldmodule()),
		gm(context.getGraphicsModule()),
		glyphmodule(gm.getGlyphmodule()),
		scene(0)
	{
		scene = gm.getScene(root_region);
		EXPECT_TRUE(fm.isValid());
		EXPECT_TRUE(gm.isValid());
		EXPECT_TRUE(glyphmodule.isValid());
		EXPECT_EQ(OK, glyphmodule.defineStandardGlyphs());
		EXPECT_TRUE(scene.isValid());
	}

	~ZincTestSetupCpp()
	{
	}
};

#endif // __ZINCTESTSETUPCPP_HPP__
