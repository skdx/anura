/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include "controls.hpp"
#include "custom_object.hpp"
#include "player_info.hpp"
#include "variant.hpp"

class Level;

class PlayableCustomObject : public CustomObject
{
public:
	PlayableCustomObject(const CustomObject& obj);
	PlayableCustomObject(const PlayableCustomObject& obj);
	PlayableCustomObject(variant node);

	virtual variant write() const;

	virtual PlayerInfo* isHuman() { return &player_info_; }
	virtual const PlayerInfo* isHuman() const { return &player_info_; }

	void saveGame();
	EntityPtr saveCondition() const { return save_condition_; }

	virtual EntityPtr backup() const;
	virtual EntityPtr clone() const;

	virtual int verticalLook() const { return vertical_look_; }

	virtual bool isActive(const rect& screen_area) const;

	bool canInteract() const { return can_interact_ != 0; }

	int difficulty() const { return difficulty_; }

private:
	bool onPlatform() const;

	int walkUpOrDownStairs() const;

	virtual void process(Level& lvl);
	variant getValue(const std::string& key) const;	
	void setValue(const std::string& key, const variant& value);

	variant getPlayerValueBySlot(int slot) const;
	void setPlayerValueBySlot(int slot, const variant& value);

	PlayerInfo player_info_;

	int difficulty_;

	EntityPtr save_condition_;

	int vertical_look_;

	int underwater_ctrl_x_, underwater_ctrl_y_;

	bool underwater_controls_;

	int can_interact_;
	
	bool reverse_ab_;

	std::unique_ptr<controls::local_controls_lock> control_lock_;

	void operator=(const PlayableCustomObject&);
};
