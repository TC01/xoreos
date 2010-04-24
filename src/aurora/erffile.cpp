/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file aurora/erffile.cpp
 *  Handling BioWare's ERFs (encapsulated resource file).
 */

#include "common/stream.h"
#include "common/file.h"
#include "common/util.h"

#include "aurora/erffile.h"
#include "aurora/error.h"
#include "aurora/util.h"

static const uint32 kERFID     = MKID_BE('ERF ');
static const uint32 kMODID     = MKID_BE('MOD ');
static const uint32 kHAKID     = MKID_BE('HAK ');
static const uint32 kSAVID     = MKID_BE('SAV ');
static const uint32 kVersion1  = MKID_BE('V1.0');
static const uint32 kVersion2  = MKID_BE('V2.0');

namespace Aurora {

ERFFile::ERFFile(const Common::UString &fileName) : _fileName(fileName) {
	load();
}

ERFFile::~ERFFile() {
}

void ERFFile::clear() {
	_resources.clear();
}

void ERFFile::load() {
	Common::File erf;
	open(erf);

	readHeader(erf);

	if ((_id != kERFID) && (_id != kMODID) && (_id != kHAKID) && (_id != kSAVID))
		throw Common::Exception("Not an ERF file");

	if ((_version != kVersion1) && (_version != kVersion2))
		throw Common::Exception("Unsupported ERF file version %08X", _version);

	if ((_version == kVersion2) && !_utf16le)
		throw Common::Exception("ERF file version 2.0, but not UTF-16LE");

	try {

		ERFHeader erfHeader;

		readERFHeader  (erf, erfHeader);
		readDescription(erf, erfHeader);
		readResources  (erf, erfHeader);

		if (erf.err())
			throw Common::Exception(Common::kReadError);

	} catch (Common::Exception &e) {
		e.add("Failed reading ERF file");
		throw e;
	}

}

void ERFFile::readERFHeader(Common::SeekableReadStream &erf, ERFHeader &header) {
	uint32 resCount = 0;

	if        (_version == kVersion1) {

		header.langCount = erf.readUint32LE(); // Number of languages for the description
		erf.skip(4);                           // Number of bytes in the description
		resCount         = erf.readUint32LE(); // Number of resources in the ERF

		header.offDescription = erf.readUint32LE();
		header.offKeyList     = erf.readUint32LE();
		header.offResList     = erf.readUint32LE();

		erf.skip(4 + 4); // Build year and day

		header.descriptionID = erf.readUint32LE();

		erf.skip(116); // Reserved

	} else if (_version == kVersion2) {

		header.langCount = 0;                  // No description in ERF V2.0
		resCount         = erf.readUint32LE(); // Number of resources in the ERF

		header.descriptionID  = 0;    // No description in ERF V2.0
		header.offDescription = 0;    // No description in ERF V2.0
		header.offKeyList     = 0;    // No separate key list in ERF V2.0
		header.offResList     = 0x20; // Resource list always starts at 0x20 in ERF V2.0

		erf.skip(4 + 4); // Build year and day
		erf.skip(4);     // Unknown, always 0xFFFFFFFF?
	}

	_resources.resize(resCount);
	_iResources.resize(resCount);
}

void ERFFile::readDescription(Common::SeekableReadStream &erf, const ERFHeader &header) {
	if (_version == kVersion1) {
		if (!erf.seek(header.offDescription))
			throw Common::Exception(Common::kSeekError);

		_description.readLocString(erf, header.descriptionID, header.langCount);
	}
}

void ERFFile::readResources(Common::SeekableReadStream &erf, const ERFHeader &header) {
	if        (_version == kVersion1) {

		readV1KeyList(erf, header); // Read name and type part of the resource list
		readV1ResList(erf, header); // Read offset and size part of the resource list

	} else if (_version == kVersion2) {

		// Read the resource list
		readV2ResList(erf, header);

	}

}

void ERFFile::readV1KeyList(Common::SeekableReadStream &erf, const ERFHeader &header) {
	if (!erf.seek(header.offKeyList))
		throw Common::Exception(Common::kSeekError);

	uint32 index = 0;
	for (ResourceList::iterator res = _resources.begin(); res != _resources.end(); ++index, ++res) {
		res->name.readASCII(erf, 16);
		erf.skip(4); // Resource ID
		res->type = (FileType) erf.readUint16LE();
		erf.skip(2); // Reserved
		res->index = index;
	}
}

void ERFFile::readV1ResList(Common::SeekableReadStream &erf, const ERFHeader &header) {
	if (!erf.seek(header.offResList))
		throw Common::Exception(Common::kSeekError);

	for (IResourceList::iterator res = _iResources.begin(); res != _iResources.end(); ++res) {
		res->offset = erf.readUint32LE();
		res->size   = erf.readUint32LE();
	}
}

void ERFFile::readV2ResList(Common::SeekableReadStream &erf, const ERFHeader &header) {
	if (!erf.seek(header.offResList))
		throw Common::Exception(Common::kSeekError);

	uint32 index = 0;
	ResourceList::iterator   res = _resources.begin();
	IResourceList::iterator iRes = _iResources.begin();
	for (; (res != _resources.end()) && (iRes != _iResources.end()); ++index, ++res, ++iRes) {
		Common::UString name;

		name.readUTF16LE(erf, 32);

		res->name  = setFileType(name, kFileTypeNone);
		res->type  = getFileType(name);
		res->index = index;

		iRes->offset = erf.readUint32LE();
		iRes->size   = erf.readUint32LE();
	}

}

const LocString &ERFFile::getDescription() const {
	return _description;
}

const Archive::ResourceList &ERFFile::getResources() const {
	return _resources;
}

Common::SeekableReadStream *ERFFile::getResource(uint32 index) const {
	if (index >= _iResources.size())
		throw Common::Exception("Resource index out of range (%d/%d)", index, _iResources.size());

	Common::File erf;
	open(erf);

	const IResource &res = _iResources[index];

	if (!erf.seek(res.offset))
		throw Common::Exception(Common::kSeekError);

	Common::SeekableReadStream *resStream = erf.readStream(res.size);

	if (!resStream || (((uint32) resStream->size()) != res.size)) {
		delete resStream;
		throw Common::Exception(Common::kReadError);
	}

	return resStream;
}

void ERFFile::open(Common::File &file) const {
	if (!file.open(_fileName))
		throw Common::Exception(Common::kOpenError);
}

} // End of namespace Aurora
