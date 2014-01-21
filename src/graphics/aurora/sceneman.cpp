/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names can be
 * found in the AUTHORS file distributed with this source
 * distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * The Infinity, Aurora, Odyssey, Eclipse and Lycium engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 */

/** @file graphics/aurora/sceneman.cpp
 *  A scene manager.
 */

#include "common/error.h"
#include "common/threads.h"

#include "graphics/util.h"
#include "graphics/meshutil.h"
#include "graphics/cameraman.h"
#include "graphics/renderable.h"

#include "graphics/aurora/sceneman.h"
#include "graphics/aurora/quad.h"
#include "graphics/aurora/cube.h"
#include "graphics/aurora/model.h"
#include "graphics/aurora/model_nwn.h"
#include "graphics/aurora/model_kotor.h"
#include "graphics/aurora/model_nwn2.h"
#include "graphics/aurora/model_witcher.h"

#include "events/requests.h"

DECLARE_SINGLETON(Graphics::Aurora::SceneManager)

namespace Graphics {

namespace Aurora {


SceneManager::SceneManager() : _modelType(kModelTypeNone) {
}

SceneManager::~SceneManager() {
}

void SceneManager::destroy() {
	Common::Singleton<SceneManager>::destroy();
}

void SceneManager::clear() {
	_modelType = kModelTypeNone;
}

void SceneManager::registerModelType(ModelType type) {
	_modelType = type;
}

void SceneManager::destroy(Renderable *r) {
	if (!r)
		return;

	if (!Common::isMainThread()) {
		Events::MainThreadFunctor<void> functor(boost::bind(&SceneManager::destroy, this, r));

		return RequestMan.callInMainThread(functor);
	}

	delete r;
}

Quad *SceneManager::createQuad(const Common::UString &texture, const Common::UString &scene) {
	if (!Common::isMainThread()) {
		Events::MainThreadFunctor<Quad *> functor(boost::bind(&SceneManager::createQuad, this, texture, scene));

		return RequestMan.callInMainThread(functor);
	}

	return new Quad(texture, scene);
}

Cube *SceneManager::createCube(const Common::UString &texture, const Common::UString &scene) {
	if (!Common::isMainThread()) {
		Events::MainThreadFunctor<Cube *> functor(boost::bind(&SceneManager::createCube, this, texture, scene));

		return RequestMan.callInMainThread(functor);
	}

	Cube *cube = 0;
	try {

		cube = new Cube(texture, scene);

	} catch (Common::Exception &e) {
		e.add("Failed to create rotating cube with texture \"%s\"", texture.c_str());
		throw;
	}

	return cube;
}

Model *SceneManager::createModel(const Common::UString &model, const Common::UString &texture, const Common::UString &scene) {
	if (model.empty())
		return 0;

	if (!Common::isMainThread()) {
		Events::MainThreadFunctor<Model *> functor(boost::bind(&SceneManager::createModel, this, model, texture, scene));

		return RequestMan.callInMainThread(functor);
	}

	Model *modelInstance = 0;
	try {

		if      (_modelType == kModelTypeNWN)
			modelInstance = new Model_NWN(model, texture, scene);
		else if (_modelType == kModelTypeKotOR)
			modelInstance = new Model_KotOR(model, false, texture, scene);
		else if (_modelType == kModelTypeKotOR2)
			modelInstance = new Model_KotOR(model, true, texture, scene);
		else if (_modelType == kModelTypeNWN2)
			modelInstance = new Model_NWN2(model, scene);
		else if (_modelType == kModelTypeTheWitcher)
			modelInstance = new Model_Witcher(model, scene);
		else
			throw Common::Exception("No valid model type registered");

	} catch (Common::Exception &e) {
		e.add("Failed to create model \"%s\"", model.c_str());
		throw;
	}

	return modelInstance;
}

Renderable *SceneManager::getRenderableAt(int x, int y, SelectableType type, float &distance) {
	Ogre::SceneManager &scene = getOgreSceneManager();

	const Ogre::Ray mouseRay = CameraMan.castRay(x, y);

	Ogre::RaySceneQuery *query = scene.createRayQuery(mouseRay, (uint32) type);
	query->setSortByDistance(true);

	Ogre::RaySceneQueryResult &results = query->execute();

	Renderable *renderable = 0;
	for (Ogre::RaySceneQueryResult::iterator result = results.begin(); result != results.end(); ++result) {
		// Check that this result has a moveable that contains a visible renderable

		if (!result->movable)
			continue;

		const Ogre::Any &movableRenderable = result->movable->getUserObjectBindings().getUserAny("renderable");
		if (movableRenderable.isEmpty())
			continue;

		Renderable *resultRenderable = Ogre::any_cast<Renderable *>(movableRenderable);
		if (!resultRenderable->isVisible())
			continue;

		renderable = resultRenderable;
		distance   = result->distance;
		break;
	}

	scene.destroyQuery(query);

	return renderable;
}

} // End of namespace Aurora

} // End of namespace Graphics