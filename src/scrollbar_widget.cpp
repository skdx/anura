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

#include "image_widget.hpp"
#include "input.hpp"
#include "scrollbar_widget.hpp"
#include "widget_factory.hpp"

namespace gui 
{
	namespace 
	{
		const std::string UpArrow = "scrollbar-vertical-up-arrow";
		const std::string DownArrow = "scrollbar-vertical-down-arrow";
		const std::string VerticalHandle = "scrollbar-vertical-handle-middle";
		const std::string VerticalHandleBot = "scrollbar-vertical-handle-bottom";
		const std::string VerticalHandleTop = "scrollbar-vertical-handle-top";
		const std::string VerticalBackground = "scrollbar-vertical-background";
	}

	ScrollBarWidget::ScrollBarWidget(std::function<void(int)> handler)
		: handler_(handler),
		  up_arrow_(new GuiSectionWidget(UpArrow)),
		  down_arrow_(new GuiSectionWidget(DownArrow)),
		  handle_(new GuiSectionWidget(VerticalHandle)),
		  handle_bot_(new GuiSectionWidget(VerticalHandleBot)),
		  handle_top_(new GuiSectionWidget(VerticalHandleTop)),
		  background_(new GuiSectionWidget(VerticalBackground)),
		  window_pos_(0), 
		  window_size_(0), 
		  range_(0), 
		  step_(0), 
		  arrow_step_(0),
		  dragging_handle_(false),
		  drag_start_(0), 
		  drag_anchor_y_(0)
	{
		setEnvironment();
	}

	ScrollBarWidget::ScrollBarWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		  window_pos_(0), 
		  window_size_(0), 
		  range_(0),
		  step_(0), 
		  arrow_step_(0),
		  dragging_handle_(false), 
		  drag_start_(0), 
		  drag_anchor_y_(0)
	{
		handler_ = std::bind(&ScrollBarWidget::handlerDelegate, this, std::placeholders::_1);
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		ffl_handler_ = getEnvironment()->createFormula(v["on_scroll"]);
	
		up_arrow_ = v.has_key("up_arrow") ? widget_factory::create(v["up_arrow"], e) : new GuiSectionWidget(UpArrow);
		down_arrow_ = v.has_key("down_arrow") ? widget_factory::create(v["down_arrow"], e) : new GuiSectionWidget(DownArrow);
		handle_ = v.has_key("handle") ? widget_factory::create(v["handle"], e) : new GuiSectionWidget(VerticalHandle);
		handle_bot_ = v.has_key("handle_bottom") ? widget_factory::create(v["handle_bottom"], e) : new GuiSectionWidget(VerticalHandleBot);
		handle_top_ = v.has_key("handle_top") ? widget_factory::create(v["handle_top"], e) : new GuiSectionWidget(VerticalHandleTop);
		background_ = v.has_key("background") ? widget_factory::create(v["background"], e) : new GuiSectionWidget(VerticalBackground);
		if(v.has_key("range")) {
			std::vector<int> range = v["range"].as_list_int();
			ASSERT_EQ(range.size(), 2);
			setRange(range[0], range[1]);
		}
	}

	void ScrollBarWidget::handlerDelegate(int yscroll)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable(new MapFormulaCallable(getEnvironment()));
			callable->add("yscroll", variant(yscroll));
			variant value = ffl_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			LOG_INFO("ScrollBarWidget::handler_delegate() called without environment!");
		}
	}

	void ScrollBarWidget::setRange(int total_height, int window_height)
	{
		window_size_ = window_height;
		range_ = total_height;
		if(window_pos_ < 0 || window_pos_ > range_ - window_size_) {
			window_pos_ = range_ - window_size_;
		}
	}

	void ScrollBarWidget::setLoc(int x, int y)
	{
		Widget::setLoc(x, y);
		setDim(width(), height());
	}

	void ScrollBarWidget::setDim(int w, int h)
	{
		w = up_arrow_->width();
		up_arrow_->setLoc(x(), y());
		down_arrow_->setLoc(x(), y() + h - down_arrow_->height());
		background_->setLoc(x(), y() + up_arrow_->height());

		const int bar_height = h - (down_arrow_->height() + up_arrow_->height());
		background_->setDim(background_->width(), bar_height);

		if(range_) {
			handle_->setLoc(x(), y() + up_arrow_->height() + (window_pos_*bar_height)/range_);
			handle_->setDim(handle_->width(), std::max<int>(6, (window_size_*bar_height)/range_));
			handle_top_->setLoc(x(), y()+ up_arrow_->height() + (window_pos_*bar_height)/range_);
			handle_bot_->setLoc(x(), y()+ down_arrow_->height() + (window_pos_*bar_height)/range_ + (window_size_*bar_height)/range_ - handle_bot_->height() +1);
		}

		//TODO:  handle range < heightOfEndcaps
	
		Widget::setDim(w, h);
	}

	void ScrollBarWidget::downButtonPressed()
	{
	}

	void ScrollBarWidget::upButtonPressed()
	{
	}

	void ScrollBarWidget::handleDraw() const
	{
		up_arrow_->draw();
		down_arrow_->draw();
		background_->draw();
		handle_->draw();
		handle_bot_->draw();
		handle_top_->draw();
	}

	void ScrollBarWidget::clipWindowPosition()
	{
		if(window_pos_ < 0) {
			window_pos_ = 0;
		}

		if(window_pos_ > range_ - window_size_) {
			window_pos_ = range_ - window_size_;
		}
	}

	bool ScrollBarWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			return claimed;
		}

		if(event.type == SDL_MOUSEWHEEL) {
			int mx, my;
			input::sdl_get_mouse_state(&mx, &my);
			if(mx < x() || mx > x() + width() 
				|| my < y() || my > y() + height()) {
				return claimed;
			}

			const int start_pos = window_pos_;
			if(event.wheel.y > 0) {
				window_pos_ -= arrow_step_;
			} else {
				window_pos_ += arrow_step_;
			}

			clipWindowPosition();

			if(window_pos_ != start_pos) {
				setDim(width(), height());
				handler_(window_pos_);
			}
			return claimed;
		} else
		if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& e = event.button;
			if(e.x < x() || e.x > x() + width() ||
			   e.y < y() || e.y > y() + height()) {
				return claimed;
			}

			const int start_pos = window_pos_;

			claimed = claimMouseEvents();

			if(e.y < up_arrow_->y() + up_arrow_->height()) {
				//on up arrow
				window_pos_ -= arrow_step_;
				while(arrow_step_ && window_pos_%arrow_step_) {
					//snap to a multiple of the step size.
					++window_pos_;
				}
			} else if(e.y > down_arrow_->y()) {
				//on down arrow
				window_pos_ += arrow_step_;
				while(arrow_step_ && window_pos_%arrow_step_) {
					//snap to a multiple of the step size.
					--window_pos_;
				}
			} else if(e.y < handle_->y()) {
				//page up
				window_pos_ -= window_size_ - arrow_step_;
			} else if(e.y > handle_->y() + handle_->height()) {
				//page down
				window_pos_ += window_size_ - arrow_step_;
			} else {
				//on handle
				dragging_handle_ = true;
				drag_start_ = window_pos_;
				drag_anchor_y_ = e.y;
			}

			LOG_INFO("HANDLE: " << handle_->y() << ", " << handle_->height());

			clipWindowPosition();

			if(window_pos_ != start_pos) {
				setDim(width(), height());
				handler_(window_pos_);
			}

		} else if(event.type == SDL_MOUSEBUTTONUP) {
			dragging_handle_ = false;
		} else if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& e = event.motion;

			int mousex, mousey;
			if(!input::sdl_get_mouse_state(&mousex, &mousey)) {
				dragging_handle_ = false;
			}

			if(dragging_handle_) {
				const int handle_height = height() - up_arrow_->height() - down_arrow_->height();
				const int move = e.y - drag_anchor_y_;
				const int window_move = (move*range_)/handle_height;
				window_pos_ = drag_start_ + window_move;
				if(step_) {
					window_pos_ -= window_pos_%step_;
				}

				clipWindowPosition();

				setDim(width(), height());
				handler_(window_pos_);
			}
		}
		return claimed;
	}

	BEGIN_DEFINE_CALLABLE(ScrollBarWidget, Widget)
		DEFINE_FIELD(range, "[int,int]")
			std::vector<variant> v;
			v.emplace_back(obj.range_);
			v.emplace_back(obj.window_size_);
			return variant(&v);
		DEFINE_SET_FIELD
			obj.setRange(value[0].as_int(), value[1].as_int());

		DEFINE_FIELD(position, "int")
			return variant(obj.window_pos_);
		DEFINE_SET_FIELD
			obj.window_pos_ = value.as_int();
			obj.clipWindowPosition();

		DEFINE_FIELD(up_arrow, "builtin widget")
			return variant(obj.up_arrow_.get());
		DEFINE_SET_FIELD_TYPE("map|builtin widget")
			obj.up_arrow_ = widget_factory::create(value, obj.getEnvironment());

		DEFINE_FIELD(down_arrow, "builtin widget")
			return variant(obj.down_arrow_.get());
		DEFINE_SET_FIELD_TYPE("map|builtin widget")
			obj.down_arrow_ = widget_factory::create(value, obj.getEnvironment());

		DEFINE_FIELD(handle, "builtin widget")
			return variant(obj.handle_.get());
		DEFINE_SET_FIELD_TYPE("map|builtin widget")
			obj.handle_ = widget_factory::create(value, obj.getEnvironment());

		DEFINE_FIELD(handle_bottom, "builtin widget")
			return variant(obj.handle_bot_.get());
		DEFINE_SET_FIELD_TYPE("map|builtin widget")
			obj.handle_bot_ = widget_factory::create(value, obj.getEnvironment());

		DEFINE_FIELD(handle_top, "builtin widget")
			return variant(obj.handle_top_.get());
		DEFINE_SET_FIELD_TYPE("map|builtin widget")
			obj.handle_top_ = widget_factory::create(value, obj.getEnvironment());

		DEFINE_FIELD(background, "builtin widget")
			return variant(obj.background_.get());
		DEFINE_SET_FIELD_TYPE("map|builtin widget")
			obj.background_ = widget_factory::create(value, obj.getEnvironment());

		DEFINE_FIELD(on_scroll, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("string|function")
			obj.ffl_handler_ = obj.getEnvironment()->createFormula(value);
	END_DEFINE_CALLABLE(ScrollBarWidget)
}
