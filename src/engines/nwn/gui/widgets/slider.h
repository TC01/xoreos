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

/** @file engines/nwn/gui/widgets/slider.h
 *  A NWN slider widget.
 */

#ifndef ENGINES_NWN_GUI_WIDGETS_SLIDER_H
#define ENGINES_NWN_GUI_WIDGETS_SLIDER_H

#include "engines/nwn/gui/widgets/modelwidget.h"

namespace Ogre {
	class SceneNode;
}

namespace Common {
	class UString;
}

namespace Engines {

class GUI;

namespace NWN {

/** A NWN slider widget. */
class WidgetSlider : public ModelWidget {
public:
	WidgetSlider(::Engines::GUI &gui, const Common::UString &tag,
	             const Common::UString &model);
	~WidgetSlider();

	void setSteps(int steps);

	int getState() const;
	void setState(int state);

	void mouseMove(uint8 state, float x, float y);
	void mouseDown(uint8 state, float x, float y);

private:
	float _position;

	int _steps;
	int _state;

	Ogre::SceneNode *_thumb;

	float _thumbWidth;
	float _thumbHeight;
	float _thumbDepth;

	void changedValue(float x, float y);
	void changePosition(float value);
};

} // End of namespace NWN

} // End of namespace Engines

#endif // ENGINES_NWN_GUI_WIDGETS_SLIDER_H