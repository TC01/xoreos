/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010-2011 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file graphics/aurora/texture.cpp
 *  A texture as used in the Aurora engines.
 */

#include "common/types.h"
#include "common/util.h"
#include "common/error.h"
#include "common/stream.h"

#include "graphics/aurora/texture.h"

#include "graphics/graphics.h"
#include "graphics/images/txi.h"
#include "graphics/images/decoder.h"
#include "graphics/images/tga.h"
#include "graphics/images/dds.h"
#include "graphics/images/tpc.h"
#include "graphics/images/txb.h"
#include "graphics/images/sbm.h"

#include "events/requests.h"

#include "aurora/resman.h"

using Events::RequestID;

namespace Graphics {

namespace Aurora {

Texture::Texture(const Common::UString &name) : _textureID(0),
	_type(::Aurora::kFileTypeNone), _image(0), _txi(0), _width(0), _height(0) {

	_txi = new TXI();

	load(name);

	addToQueue();
}

Texture::Texture(ImageDecoder *image, const TXI &txi) {
	assert(image);

	_txi = new TXI(txi);

	load(image);

	addToQueue();
}

Texture::Texture(ImageDecoder *image) {
	assert(image);

	_txi = new TXI();

	load(image);

	addToQueue();
}

Texture::~Texture() {
	destroy();

	delete _txi;
	delete _image;
}

TextureID Texture::getID() const {
	// TODO: Just for debugging, take it out eventually :P
	if (_textureID == 0)
		throw Common::Exception("Non-existing texture ID");

	return _textureID;
}

const uint32 Texture::getWidth() const {
	return _width;
}

const uint32 Texture::getHeight() const {
	return _height;
}

void Texture::load(const Common::UString &name) {
	Common::SeekableReadStream *img = ResMan.getResource(::Aurora::kResourceImage, name, &_type);
	if (!img)
		throw Common::Exception("No such image resource \"%s\"", name.c_str());

	_name = name;

	// Loading the different image formats
	if      (_type == ::Aurora::kFileTypeTGA)
		_image = new TGA(img);
	else if (_type == ::Aurora::kFileTypeDDS)
		_image = new DDS(img);
	else if (_type == ::Aurora::kFileTypeTPC)
		_image = new TPC(img);
	else if (_type == ::Aurora::kFileTypeTXB)
		_image = new TXB(img);
	else if (_type == ::Aurora::kFileTypeSBM)
		_image = new SBM(img);
	else {
		delete img;
		throw Common::Exception("Unsupported image resource type %d", (int) _type);
	}

	_image->load();

	// Get the TXI if availabe
	Common::SeekableReadStream *txiStream = ResMan.getResource(name, ::Aurora::kFileTypeTXI);
	if (txiStream) {
		delete _txi;

		try {
			_txi = new TXI(*txiStream);
		} catch (...) {
			warning("Failed loading \"%s\".txi", name.c_str());

			_txi = new TXI();
		}

		delete txiStream;
	}

	RequestMan.dispatchAndForget(RequestMan.rebuild(*this));
}

void Texture::load(ImageDecoder *image) {
	_image = image;

	// If we didn't already, let the image load
	_image->load();

	RequestMan.dispatchAndForget(RequestMan.rebuild(*this));
}

void Texture::doDestroy() {
	if (_textureID == 0)
		return;

	glDeleteTextures(1, &_textureID);

	_textureID = 0;
}

void Texture::doRebuild() {
	if (!_image)
		// No image
		return;

	if (_image->getMipMapCount() < 1)
		throw Common::Exception("Texture has no images");

	// If we didn't find any "loose" TXI resource, look if the image provides TXI data
	if (_txi->isEmpty()) {
		Common::SeekableReadStream *txiStream = _image->getTXI();
		if (txiStream) {
			delete _txi;

			try {
				_txi = new TXI(*txiStream);
			} catch (...) {
				warning("Failed loading TXI in texture \"%s\"", _name.c_str());

				_txi = new TXI();
			}

			delete txiStream;
		}
	}

	// Set dimensions
	_width  = ((const ImageDecoder *) _image)->getMipMap(0).width;
	_height = ((const ImageDecoder *) _image)->getMipMap(0).height;

	// Generate the texture ID
	glGenTextures(1, &_textureID);
	// Bind the texture
	glBindTexture(GL_TEXTURE_2D, _textureID);

	// Texture wrapping
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Filter?
	const TXI::Features &features = _txi->getFeatures();
	if (features.filter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	if (_image->getMipMapCount() == 1) {
		// Texture doesn't specify any mip maps, generate our own

		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 9);
	} else {
		// Texture does specify mip maps, use these

		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, _image->getMipMapCount() - 1);
	}

	// Texture image data
	if (_image->isCompressed()) {
		// Compressed texture data

		for (int i = 0; i < _image->getMipMapCount(); i++) {
			const ImageDecoder::MipMap &mipMap = ((const ImageDecoder *) _image)->getMipMap(i);

			glCompressedTexImage2D(GL_TEXTURE_2D, i, _image->getFormatRaw(),
			                       mipMap.width, mipMap.height, 0,
			                       mipMap.size, mipMap.data);
		}

	} else {
		// Uncompressed texture data

		for (int i = 0; i < _image->getMipMapCount(); i++) {
			const ImageDecoder::MipMap &mipMap = ((const ImageDecoder *) _image)->getMipMap(i);

			glTexImage2D(GL_TEXTURE_2D, i, _image->getFormatRaw(),
			             mipMap.width, mipMap.height, 0, _image->getFormat(),
			             _image->getDataType(), mipMap.data);
		}

	}

}

const TXI &Texture::getTXI() const {
	return *_txi;
}

} // End of namespace Aurora

} // End of namespace Graphics
