/*
 * OpenCMISS-Zinc Library Unit Tests
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include <zinc/status.h>
#include <zinc/core.h>
#include <zinc/spectrum.h>

#include "zinctestsetup.hpp"
#include "zinctestsetupcpp.hpp"
#include "zinc/spectrum.hpp"

TEST(cmzn_spectrummodule_api, valid_args)
{
    ZincTestSetup zinc;

    cmzn_spectrummodule_id sm = cmzn_context_get_spectrummodule(zinc.context);
    EXPECT_NE(static_cast<cmzn_spectrummodule *>(0), sm);

    int result = cmzn_spectrummodule_begin_change(sm);
    EXPECT_EQ(CMZN_OK, result);

    cmzn_spectrum_id spectrum = cmzn_spectrummodule_create_spectrum(sm);
    EXPECT_NE(static_cast<cmzn_spectrum *>(0), spectrum);

    result = cmzn_spectrum_set_name(spectrum, "new_default");
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrummodule_end_change(sm);
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrummodule_set_default_spectrum(sm, spectrum);
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrum_set_managed(spectrum, 1);
    EXPECT_EQ(CMZN_OK, result);

    cmzn_spectrum_destroy(&spectrum);

    spectrum = cmzn_spectrummodule_find_spectrum_by_name(sm, "new_default");
    EXPECT_NE(static_cast<cmzn_spectrum *>(0), spectrum);

    cmzn_spectrum_destroy(&spectrum);

    spectrum = cmzn_spectrummodule_get_default_spectrum(sm);
    EXPECT_NE(static_cast<cmzn_spectrum *>(0), spectrum);

    cmzn_spectrum_destroy(&spectrum);

    cmzn_spectrummodule_destroy(&sm);
}

TEST(cmzn_spectrummodule_api, valid_args_cpp)
{
    ZincTestSetupCpp zinc;

    Spectrummodule sm = zinc.context.getSpectrummodule();
    EXPECT_TRUE(sm.isValid());

    int result = sm.beginChange();
    EXPECT_EQ(CMZN_OK, result);

    Spectrum spectrum = sm.createSpectrum();
    EXPECT_TRUE(spectrum.isValid());

    result = spectrum.setName("new_default");
    EXPECT_EQ(CMZN_OK, result);

    result = sm.endChange();
    EXPECT_EQ(CMZN_OK, result);

    result = sm.setDefaultSpectrum( spectrum);
    EXPECT_EQ(CMZN_OK, result);

    result = spectrum.setManaged(true);
    EXPECT_EQ(CMZN_OK, result);

    spectrum = sm.findSpectrumByName("new_default");
    EXPECT_TRUE(spectrum.isValid());

    spectrum = sm.getDefaultSpectrum();
    EXPECT_TRUE(spectrum.isValid());
}

TEST(cmzn_spectrum_api, valid_args)
{
    ZincTestSetup zinc;

    cmzn_spectrummodule_id sm = cmzn_context_get_spectrummodule(zinc.context);
    EXPECT_NE(static_cast<cmzn_spectrummodule *>(0), sm);

    int result = cmzn_spectrummodule_begin_change(sm);
    EXPECT_EQ(CMZN_OK, result);

    cmzn_spectrum_id spectrum = cmzn_spectrummodule_create_spectrum(sm);
    EXPECT_NE(static_cast<cmzn_spectrum *>(0), spectrum);

    result = cmzn_spectrum_set_name(spectrum, "new_default");
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrummodule_end_change(sm);
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrum_set_managed(spectrum, 1);
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrum_is_managed(spectrum);
    EXPECT_EQ(1, result);

    cmzn_spectrumcomponent_id component1= cmzn_spectrum_create_spectrumcomponent(spectrum);
    EXPECT_NE(static_cast<cmzn_spectrumcomponent *>(0), component1);

    cmzn_spectrumcomponent_id component2= cmzn_spectrum_create_spectrumcomponent(spectrum);
    EXPECT_NE(static_cast<cmzn_spectrumcomponent *>(0), component2);

    cmzn_spectrumcomponent_id component1_clone= cmzn_spectrum_get_first_spectrumcomponent(spectrum);
    EXPECT_EQ(component1_clone, component1);

    cmzn_spectrumcomponent_id component2_clone = cmzn_spectrum_get_next_spectrumcomponent(spectrum, component1_clone);
    EXPECT_EQ(component2_clone, component2);

    cmzn_spectrumcomponent_id component3 = cmzn_spectrum_get_next_spectrumcomponent(spectrum, component2_clone);
    EXPECT_EQ(0, component3);

    cmzn_spectrumcomponent_destroy(&component1_clone);

    cmzn_spectrumcomponent_destroy(&component2_clone);

    component1_clone = cmzn_spectrum_get_previous_spectrumcomponent(spectrum, component2);
    EXPECT_EQ(component1_clone, component1);

    cmzn_spectrumcomponent_destroy(&component1_clone);

    EXPECT_EQ(CMZN_OK, result = cmzn_spectrumcomponent_set_range_maximum(component1, 20.0));

    double double_result = cmzn_spectrumcomponent_get_range_maximum(component1);
    EXPECT_EQ(20.0, double_result);

    EXPECT_EQ(CMZN_OK, result = cmzn_spectrumcomponent_set_active(component1, false));

    bool bool_result = cmzn_spectrumcomponent_is_active(component1);
    EXPECT_FALSE(bool_result);

    EXPECT_EQ(CMZN_OK, result = cmzn_spectrumcomponent_set_colour_reverse(component1, true));

    bool_result = cmzn_spectrumcomponent_is_colour_reverse(component1);
    EXPECT_TRUE(bool_result);

    // check on by default
    bool_result = cmzn_spectrumcomponent_is_extend_above(component1);
    EXPECT_TRUE(bool_result);

    EXPECT_EQ(CMZN_OK, result = cmzn_spectrumcomponent_set_extend_above(
        component1, false));

    bool_result = cmzn_spectrumcomponent_is_extend_above(component1);
    EXPECT_FALSE(bool_result);

    // check on by default
    bool_result = cmzn_spectrumcomponent_is_extend_below(component1);
    EXPECT_TRUE(bool_result);

    EXPECT_EQ(CMZN_OK, result = cmzn_spectrumcomponent_set_extend_below(component1, false));

    bool_result = cmzn_spectrumcomponent_is_extend_below(component1);
    EXPECT_FALSE(bool_result);

    result = cmzn_spectrumcomponent_set_field_component(component1,	2);
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrumcomponent_get_field_component(component1);
    EXPECT_EQ(2, result);

    result = cmzn_spectrumcomponent_set_number_of_bands(component1,	6);
    EXPECT_EQ(CMZN_OK, result);

    result = cmzn_spectrumcomponent_get_number_of_bands(component1);
    EXPECT_EQ(6, result);

    result = cmzn_spectrumcomponent_set_scale_type(component1,
        CMZN_SPECTRUMCOMPONENT_SCALE_TYPE_LOG);
    EXPECT_EQ(CMZN_OK, result);

    enum cmzn_spectrumcomponent_scale_type scale_type =
        cmzn_spectrumcomponent_get_scale_type(component1);
    EXPECT_EQ(CMZN_SPECTRUMCOMPONENT_SCALE_TYPE_LOG, scale_type);

    result = cmzn_spectrumcomponent_set_colour_mapping_type(component1,
        CMZN_SPECTRUMCOMPONENT_COLOUR_MAPPING_TYPE_MONOCHROME);
    EXPECT_EQ(CMZN_OK, result);

    enum cmzn_spectrumcomponent_colour_mapping_type _colour_mapping_type =
        cmzn_spectrumcomponent_get_colour_mapping_type(component1);
    EXPECT_EQ(CMZN_SPECTRUMCOMPONENT_COLOUR_MAPPING_TYPE_MONOCHROME, _colour_mapping_type);

    cmzn_spectrumcomponent_destroy(&component2);

    cmzn_spectrumcomponent_destroy(&component1);

    cmzn_spectrum_destroy(&spectrum);

    cmzn_spectrummodule_destroy(&sm);
}

TEST(cmzn_spectrum_api, valid_args_cpp)
{
    ZincTestSetupCpp zinc;

    Spectrummodule sm = zinc.context.getSpectrummodule();
    EXPECT_TRUE(sm.isValid());

    int result = sm.beginChange();
    EXPECT_EQ(CMZN_OK, result);

    Spectrum spectrum = sm.createSpectrum();
    EXPECT_TRUE(spectrum.isValid());

    result = spectrum.setName("new_default");
    EXPECT_EQ(CMZN_OK, result);

    result = sm.endChange();
    EXPECT_EQ(CMZN_OK, result);

    result = spectrum.setManaged(true);
    EXPECT_EQ(CMZN_OK, result);

    EXPECT_TRUE(spectrum.isManaged());

    Spectrumcomponent component1 = spectrum.createSpectrumcomponent();
    EXPECT_TRUE(component1.isValid());

    Spectrumcomponent component2 = spectrum.createSpectrumcomponent();
    EXPECT_TRUE(component2.isValid());

    Spectrumcomponent component1_clone = spectrum.getFirstSpectrumcomponent();
    EXPECT_EQ(component1_clone.getId(), component1.getId());

    Spectrumcomponent component2_clone = spectrum.getNextSpectrumcomponent(component1_clone);
    EXPECT_EQ(component2_clone.getId(), component2.getId());

    Spectrumcomponent component3 = spectrum.getNextSpectrumcomponent(component2_clone);
    EXPECT_FALSE(component3.isValid());

    component1_clone = spectrum.getPreviousSpectrumcomponent(component2);
    EXPECT_EQ(component1_clone.getId(), component1.getId());

    EXPECT_EQ(CMZN_OK, result = component1.setRangeMaximum(20.0));

    double double_result = component1.getRangeMaximum();
    EXPECT_EQ(20.0, double_result);

    EXPECT_EQ(CMZN_OK, result = component1.setActive(false));

    bool bool_result = component1.isActive();
    EXPECT_FALSE(bool_result);

    result = component1.setColourReverse(true);
    EXPECT_EQ(CMZN_OK, result);

    bool_result = component1.isColourReverse();
    EXPECT_TRUE(bool_result);

    // check on by default
    bool_result = component1.isExtendAbove();
    EXPECT_TRUE(bool_result);
    result = component1.setExtendAbove(false);
    EXPECT_EQ(CMZN_OK, result);
    bool_result = component1.isExtendAbove();
    EXPECT_FALSE(bool_result);

    // check on by default
    bool_result = component1.isExtendBelow();
    EXPECT_TRUE(bool_result);
    result = component1.setExtendBelow(false);
    EXPECT_EQ(CMZN_OK, result);
    bool_result = component1.isExtendBelow();
    EXPECT_FALSE(bool_result);

    result = component1.setFieldComponent(2);
    EXPECT_EQ(CMZN_OK, result);

    result = component1.getFieldComponent();
    EXPECT_EQ(2, result);

    result = component1.setNumberOfBands(6);
    EXPECT_EQ(CMZN_OK, result);

    result = component1.getNumberOfBands();
    EXPECT_EQ(6, result);

    enum Spectrumcomponent::ScaleType scale_type = component1.getScaleType();
    EXPECT_EQ(Spectrumcomponent::SCALE_TYPE_LINEAR, scale_type);

    result = component1.setScaleType(component1.SCALE_TYPE_LOG);
    EXPECT_EQ(CMZN_OK, result);

    scale_type = component1.getScaleType();
    EXPECT_EQ(Spectrumcomponent::SCALE_TYPE_LOG, scale_type);

    enum Spectrumcomponent::ColourMappingType colour_mapping_type = component1.getColourMappingType();
    EXPECT_EQ(Spectrumcomponent::COLOUR_MAPPING_TYPE_RAINBOW, colour_mapping_type);

    result = component1.setColourMappingType(component1.COLOUR_MAPPING_TYPE_MONOCHROME);
    EXPECT_EQ(CMZN_OK, result);

    colour_mapping_type = component1.getColourMappingType();
    EXPECT_EQ(Spectrumcomponent::COLOUR_MAPPING_TYPE_MONOCHROME, colour_mapping_type);

}

TEST(cmzn_spectrum_api, iteration_cpp)
{
    ZincTestSetupCpp zinc;

    Spectrummodule spectrummodule = zinc.context.getSpectrummodule();
    EXPECT_TRUE(spectrummodule.isValid());

    Spectrum xxx = spectrummodule.createSpectrum();
    EXPECT_TRUE(xxx.isValid());
    EXPECT_EQ(CMZN_OK, xxx.setName("xxx"));
    Spectrum zzz = spectrummodule.createSpectrum();
    EXPECT_TRUE(zzz.isValid());
    EXPECT_EQ(CMZN_OK, zzz.setName("zzz"));

    Spectrum aaa = spectrummodule.createSpectrum();
    EXPECT_TRUE(aaa.isValid());
    EXPECT_EQ(CMZN_OK, aaa.setName("aaa"));

    Spectrum defaultSpectrum = spectrummodule.getDefaultSpectrum();
    EXPECT_TRUE(defaultSpectrum.isValid());

    Spectrumiterator iter = spectrummodule.createSpectrumiterator();
    EXPECT_TRUE(iter.isValid());
    Spectrum s;
    EXPECT_EQ(aaa, s = iter.next());
    EXPECT_EQ(defaultSpectrum, s = iter.next());
    EXPECT_EQ(xxx, s = iter.next());
    EXPECT_EQ(zzz, s = iter.next());
    EXPECT_FALSE((s = iter.next()).isValid());
}
