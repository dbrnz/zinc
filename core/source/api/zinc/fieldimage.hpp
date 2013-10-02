/***************************************************************************//**
 * FILE : fieldimage.hpp
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CMZN_FIELDIMAGE_HPP__
#define CMZN_FIELDIMAGE_HPP__

#include "zinc/fieldimage.h"
#include "zinc/field.hpp"
#include "zinc/fieldmodule.hpp"
#include "zinc/stream.hpp"


namespace OpenCMISS
{
namespace Zinc
{
class StreamInformationImage;

class FieldImage : public Field
{
private:
	// takes ownership of C handle, responsibility for destroying it
	explicit FieldImage(cmzn_field_id field_id) : Field(field_id)
	{	}

	friend FieldImage Fieldmodule::createFieldImage();
	friend FieldImage Fieldmodule::createFieldImageFromSource(Field& sourceField);

	inline cmzn_field_image_id getDerivedId()
	{
		return reinterpret_cast<cmzn_field_image_id>(id);
	}

public:

	FieldImage() : Field(0)
	{ }

	// takes ownership of C handle, responsibility for destroying it
	explicit FieldImage(cmzn_field_image_id field_image_id) :
		Field(reinterpret_cast<cmzn_field_id>(field_image_id))
	{	}

	// casting constructor: must check isValid()
	FieldImage(Field& field) :
		Field(reinterpret_cast<cmzn_field_id>(cmzn_field_cast_image(field.getId())))
	{	}

	enum CombineMode
	{
		COMBINE_INVALID = CMZN_FIELD_IMAGE_COMBINE_INVALID,
		COMBINE_BLEND = CMZN_FIELD_IMAGE_COMBINE_BLEND,
		COMBINE_DECAL = CMZN_FIELD_IMAGE_COMBINE_DECAL,
		COMBINE_MODULATE = CMZN_FIELD_IMAGE_COMBINE_MODULATE,
		COMBINE_ADD = CMZN_FIELD_IMAGE_COMBINE_ADD,
		COMBINE_ADD_SIGNED = CMZN_FIELD_IMAGE_COMBINE_ADD_SIGNED,
		COMBINE_MODULATE_SCALE_4 = CMZN_FIELD_IMAGE_COMBINE_MODULATE_SCALE_4,
		COMBINE_BLEND_SCALE_4 = CMZN_FIELD_IMAGE_COMBINE_BLEND_SCALE_4,
		COMBINE_SUBTRACT = CMZN_FIELD_IMAGE_COMBINE_SUBTRACT,
		COMBINE_ADD_SCALE_4 = CMZN_FIELD_IMAGE_COMBINE_ADD_SCALE_4,
		COMBINE_SUBTRACT_SCALE_4 = CMZN_FIELD_IMAGE_COMBINE_SUBTRACT_SCALE_4,
		COMBINE_INVERT_ADD_SCALE_4 = CMZN_FIELD_IMAGE_COMBINE_INVERT_ADD_SCALE_4,
		COMBINE_INVERT_SUBTRACT_SCALE_4 = CMZN_FIELD_IMAGE_COMBINE_INVERT_SUBTRACT_SCALE_4
	};

	enum FilterMode
	{
		FILTER_INVALID = CMZN_FIELD_IMAGE_FILTER_INVALID,
		FILTER_NEAREST = CMZN_FIELD_IMAGE_FILTER_NEAREST,
		FILTER_LINEAR = CMZN_FIELD_IMAGE_FILTER_LINEAR,
		FILTER_NEAREST_MIPMAP_NEAREST = CMZN_FIELD_IMAGE_FILTER_NEAREST_MIPMAP_NEAREST,
		FILTER_LINEAR_MIPMAP_NEAREST = CMZN_FIELD_IMAGE_FILTER_LINEAR_MIPMAP_NEAREST,
		FILTER_LINEAR_MIPMAP_LINEAR = CMZN_FIELD_IMAGE_FILTER_LINEAR_MIPMAP_LINEAR
	};

	enum HardwareCompressionMode
	{
		HARDWARE_COMPRESSION_MODE_INVALID = CMZN_FIELD_IMAGE_HARDWARE_COMPRESSION_MODE_INVALID,
		HARDWARE_COMPRESSION_MODE_UNCOMPRESSED = CMZN_FIELD_IMAGE_HARDWARE_COMPRESSION_MODE_UNCOMPRESSED,
		HARDWARE_COMPRESSION_MODE_AUTOMATIC = CMZN_FIELD_IMAGE_HARDWARE_COMPRESSION_MODE_AUTOMATIC
		/*!< Allow the hardware to choose the compression */
	};

	enum WrapMode
	{
		WRAP_INVALID = CMZN_FIELD_IMAGE_WRAP_INVALID,
		WRAP_CLAMP = CMZN_FIELD_IMAGE_WRAP_CLAMP,
		WRAP_REPEAT = CMZN_FIELD_IMAGE_WRAP_REPEAT,
		WRAP_EDGE_CLAMP = CMZN_FIELD_IMAGE_WRAP_EDGE_CLAMP,
		WRAP_BORDER_CLAMP= CMZN_FIELD_IMAGE_WRAP_BORDER_CLAMP,
		WRAP_MIRROR_REPEAT = CMZN_FIELD_IMAGE_WRAP_MIRROR_REPEAT
		/*!< Allow the hardware to choose the wrap mode for texture */
	};

	int getWidthInPixels()
	{
		return cmzn_field_image_get_width_in_pixels(getDerivedId());
	}

	int getHeightInPixels()
	{
		return cmzn_field_image_get_height_in_pixels(getDerivedId());
	}

	int getDepthInPixels()
	{
		return cmzn_field_image_get_depth_in_pixels(getDerivedId());
	}

	int getSizeInPixels(int valuesCount, int *valuesOut)
	{
		return cmzn_field_image_get_size_in_pixels(getDerivedId(), valuesCount, valuesOut);
	}

	double getTextureCoordinateWidth()
	{
		return cmzn_field_image_get_texture_coordinate_width(getDerivedId());
	}

	double getTextureCoordinateHeight()
	{
		return cmzn_field_image_get_texture_coordinate_height(getDerivedId());
	}

	double getTextureCoordinateDepth()
	{
		return cmzn_field_image_get_texture_coordinate_depth(getDerivedId());
	}

	int getTextureCoordinateSizes(int valuesCount, double *valuesOut)
	{
		return cmzn_field_image_get_texture_coordinate_sizes(getDerivedId(), valuesCount,
			valuesOut);
	}

	int setTextureCoordinateWidth(double width)
	{
		return cmzn_field_image_set_texture_coordinate_width(getDerivedId(), width);
	}

	int setTextureCoordinateHeight(double height)
	{
		return cmzn_field_image_set_texture_coordinate_height(getDerivedId(), height);
	}

	int setTextureCoordinateDepth(double depth)
	{
		return cmzn_field_image_set_texture_coordinate_depth(getDerivedId(), depth);
	}

	int setTextureCoordinateSizes(int valuesCount, const double *valuesIn)
	{
		return cmzn_field_image_set_texture_coordinate_sizes(getDerivedId(),
			valuesCount, valuesIn);
	}

	int read(StreamInformation& streamInformation)
	{
		return cmzn_field_image_read(getDerivedId(), streamInformation.getId());
	}

	int readFile(const char *fileName)
	{
		return cmzn_field_image_read_file(getDerivedId(), fileName);
	}

	int write(StreamInformation& streamInformation)
	{
		return cmzn_field_image_write(getDerivedId(), streamInformation.getId());
	}

	CombineMode getCombineMode()
	{
		return static_cast<CombineMode>(cmzn_field_image_get_combine_mode(getDerivedId()));
	}

	int setCombineMode(CombineMode combineMode)
	{
		return cmzn_field_image_set_combine_mode(getDerivedId(),
			static_cast<cmzn_field_image_combine_mode>(combineMode));
	}

	Field getDomainField()
	{
		return Field(cmzn_field_image_get_domain_field(getDerivedId()));
	}

	int setDomainField(Field& domainField)
	{
		return cmzn_field_image_set_domain_field(getDerivedId(), domainField.getId());
	}

	HardwareCompressionMode getHardwareCompressionMode()
	{
		return static_cast<HardwareCompressionMode>(
			cmzn_field_image_get_hardware_compression_mode(getDerivedId()));
	}

	int setHardwareCompressionMode(HardwareCompressionMode hardwareCompressionMode)
	{
		return cmzn_field_image_set_hardware_compression_mode(getDerivedId(),
			static_cast<cmzn_field_image_hardware_compression_mode>(hardwareCompressionMode));
	}

	FilterMode getFilterMode()
	{
		return static_cast<FilterMode>(cmzn_field_image_get_filter_mode(getDerivedId()));
	}

	int setFilterMode(FilterMode filterMode)
	{
		return cmzn_field_image_set_filter_mode(getDerivedId(),
			static_cast<cmzn_field_image_filter_mode>(filterMode));
	}

	WrapMode getWrapMode()
	{
		return static_cast<WrapMode>(cmzn_field_image_get_wrap_mode(getDerivedId()));
	}

	int setWrapMode(WrapMode wrapMode)
	{
		return cmzn_field_image_set_wrap_mode(getDerivedId(),
			static_cast<cmzn_field_image_wrap_mode>(wrapMode));
	}

	char *getProperty(const char* property)
	{
		return cmzn_field_image_get_property(getDerivedId(), property);
	}

	StreamInformationImage createStreamInformation();

};

class StreamInformationImage : public StreamInformation
{
private:
	StreamInformationImage(StreamInformation& streamInformation) :
		StreamInformation(streamInformation)
	{ }

	friend StreamInformationImage FieldImage::createStreamInformation();

public:

	// takes ownership of C handle, responsibility for destroying it
	explicit StreamInformationImage(cmzn_stream_information_image_id stream_information_image_id) :
		StreamInformation(reinterpret_cast<cmzn_stream_information_id>(stream_information_image_id))
	{ }

	enum ImageAttribute
	{
		IMAGE_ATTRIBUTE_RAW_WIDTH_PIXELS = CMZN_STREAM_INFORMATION_IMAGE_ATTRIBUTE_RAW_WIDTH_PIXELS,
		IMAGE_ATTRIBUTE_RAW_HEIGHT_PIXELS = CMZN_STREAM_INFORMATION_IMAGE_ATTRIBUTE_RAW_HEIGHT_PIXELS,
		IMAGE_ATTRIBUTE_BITS_PER_COMPONENT = CMZN_STREAM_INFORMATION_IMAGE_ATTRIBUTE_BITS_PER_COMPONENT,
		IMAGE_ATTRIBUTE_COMPRESSION_QUALITY = CMZN_STREAM_INFORMATION_IMAGE_ATTRIBUTE_COMPRESSION_QUALITY
	};

	enum ImageFileFormat
	{
		IMAGE_FILE_FORMAT_INVALID = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_INVALID,
		IMAGE_FILE_FORMAT_BMP = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_BMP,
		IMAGE_FILE_FORMAT_DICOM = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_DICOM,
		IMAGE_FILE_FORMAT_JPG = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_JPG,
		IMAGE_FILE_FORMAT_GIF = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_GIF,
		IMAGE_FILE_FORMAT_PNG = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_PNG,
		IMAGE_FILE_FORMAT_SGI = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_SGI,
		IMAGE_FILE_FORMAT_TIFF = CMZN_STREAM_INFORMATION_IMAGE_FILE_FORMAT_TIFF
	};

	enum ImagePixelFormat
	{
		IMAGE_PIXEL_FORMAT_INVALID = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_INVALID,
		IMAGE_PIXEL_FORMAT_LUMINANCE = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_LUMINANCE,
		IMAGE_PIXEL_FORMAT_LUMINANCE_ALPHA = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_LUMINANCE_ALPHA,
		IMAGE_PIXEL_FORMAT_RGB = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_RGB,
		IMAGE_PIXEL_FORMAT_RGBA = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_RGBA,
		IMAGE_PIXEL_FORMAT_ABGR = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_ABGR,
		IMAGE_PIXEL_FORMAT_BGR = CMZN_STREAM_INFORMATION_IMAGE_PIXEL_FORMAT_BGR
	};

	int setAttributeInteger(ImageAttribute imageAttribute, int value)
	{
		return cmzn_stream_information_image_set_attribute_integer(
			reinterpret_cast<cmzn_stream_information_image_id>(id),
			static_cast<cmzn_stream_information_image_attribute>(imageAttribute), value);
	}

	int setAttributeReal(ImageAttribute imageAttribute, double value)
	{
		return cmzn_stream_information_image_set_attribute_real(
			reinterpret_cast<cmzn_stream_information_image_id>(id),
			static_cast<cmzn_stream_information_image_attribute>(imageAttribute), value);
	}

	int setFileFormat(ImageFileFormat imageFileFormat)
	{
		return cmzn_stream_information_image_set_file_format(
			reinterpret_cast<cmzn_stream_information_image_id>(id),
			static_cast<cmzn_stream_information_image_file_format>(imageFileFormat));
	}

	int setPixelFormat(ImagePixelFormat imagePixelFormat)
	{
		return cmzn_stream_information_image_set_pixel_format(
			reinterpret_cast<cmzn_stream_information_image_id>(id),
			static_cast<cmzn_stream_information_image_pixel_format>(imagePixelFormat));
	}

};

inline StreamInformationImage FieldImage::createStreamInformation()
{
	return StreamInformationImage(
		reinterpret_cast<cmzn_stream_information_image_id>(
			cmzn_field_image_create_stream_information(getDerivedId())));
}

inline FieldImage Fieldmodule::createFieldImage()
{
	return FieldImage(cmzn_fieldmodule_create_field_image(id));
}

inline FieldImage Fieldmodule::createFieldImageFromSource(Field& sourceField)
{
	return FieldImage(cmzn_fieldmodule_create_field_image_from_source(id, sourceField.getId()));
}

} // namespace Zinc
}
#endif
