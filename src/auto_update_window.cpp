#include <assert.h>
#include <sstream>

#ifdef _MSC_VER
#include <direct.h>
#define chdir _chdir
#define execv _execv
#else
#include <unistd.h>
#endif

#include "auto_update_window.hpp"
#include "filesystem.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "Font.hpp"
#include "Texture.hpp"

namespace 
{
	KRE::TexturePtr render_updater_text(const std::string& str, const KRE::Color& color)
	{
		return KRE::Font::getInstance()->renderText(str, color, 16, true, KRE::Font::get_default_monospace_font());
	}

	class progress_animation
	{
	public:
		static progress_animation& get() {
			static progress_animation instance;
			return instance;
		}

		progress_animation()
		{
			std::string contents = sys::read_file("update/progress.cfg");
			if(contents == "") {
				area_ = rect();
				pad_ = rows_ = cols_ = 0;
				return;
			}

			variant cfg = json::parse(contents, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			area_ = rect(cfg["x"].as_int(), cfg["y"].as_int(), cfg["w"].as_int(), cfg["h"].as_int());
			tex_ = KRE::Texture::createTexture(cfg["image"].as_string());
			pad_ = cfg["pad"].as_int();
			rows_ = cfg["rows"].as_int();
			cols_ = cfg["cols"].as_int();
		}

		KRE::TexturePtr tex() const { return tex_; }
		rect calculate_rect(int ntime) const {
			if(rows_ * cols_ == 0) {
				return area_;
			}
			ntime = ntime % (rows_*cols_);

			int row = ntime / cols_;
			int col = ntime % cols_;

			rect result(area_.x() + (area_.w() + pad_) * col, area_.y() + (area_.h() + pad_) * row, area_.w(), area_.h());
			return result;
		}

	private:
		KRE::TexturePtr tex_;
		rect area_;
		int pad_, rows_, cols_;
	};
}

auto_update_window::auto_update_window() : window_(), nframes_(0), start_time_(SDL_GetTicks()), percent_(0.0)
{
}

auto_update_window::~auto_update_window()
{
}

void auto_update_window::set_progress(float percent)
{
	percent_ = percent;
}

void auto_update_window::set_message(const std::string& str)
{
	message_ = str;
}

void auto_update_window::process()
{
	++nframes_;
	if(window_ == nullptr && SDL_GetTicks() - start_time_ > 2000) {
		manager_.reset(new SDL::SDL());

		variant_builder hints;
		hints.add("renderer", "opengl");
		hints.add("title", "Anura auto-update");
		hints.add("clear_color", "black");

		KRE::WindowManager wm("SDL");
		window_ = wm.createWindow(800, 600, hints.build());

		using namespace KRE;

		// Set the default font to use for rendering. This can of course be overridden when rendering the
		// text to a texture.
		Font::setDefaultFont(module::get_default_font() == "bitmap" 
			? "FreeMono" 
			: module::get_default_font());
		std::map<std::string,std::string> font_paths;
		std::map<std::string,std::string> font_paths2;
		module::get_unique_filenames_under_dir("data/fonts/", &font_paths);
		for(auto& fp : font_paths) {
			font_paths2[module::get_id(fp.first)] = fp.second;
		}
		KRE::Font::setAvailableFonts(font_paths2);
		font_paths.clear();
		font_paths2.clear();
	}
}

void auto_update_window::draw() const
{
	if(window_ == nullptr) {
		return;
	}

	auto canvas = KRE::Canvas::getInstance();

	window_->clear(KRE::ClearFlags::COLOR);

	canvas->drawSolidRect(rect(300, 290, 200, 20), KRE::Color(255, 255, 255, 255));
	canvas->drawSolidRect(rect(303, 292, 194, 16), KRE::Color(0, 0, 0, 255));
	const rect filled_area(303, 292, static_cast<int>(194.0f*percent_), 16);
	canvas->drawSolidRect(filled_area, KRE::Color(255, 255, 255, 255));

	const int bar_point = filled_area.x2();

	const int percent = static_cast<int>(percent_*100.0);
	std::ostringstream percent_stream;
	percent_stream << percent << "%";

	KRE::TexturePtr percent_surf_white(render_updater_text(percent_stream.str(), KRE::Color(255, 255, 255)));
	KRE::TexturePtr percent_surf_black(render_updater_text(percent_stream.str(), KRE::Color(0, 0, 0)));

	if(percent_surf_white != nullptr) {
		rect dest(window_->width()/2 - percent_surf_white->width()/2,
		          window_->height()/2 - percent_surf_white->height()/2,
				  0, 0);
		canvas->blitTexture(percent_surf_white, 0, dest);
	}

	if(percent_surf_black != nullptr) {
		rect dest(window_->width()/2 - percent_surf_black->width()/2,
		          window_->height()/2 - percent_surf_black->height()/2,
				  0, 0);

		if(bar_point > dest.x()) {
			if(bar_point < dest.x() + dest.w()) {
				dest.set_w(bar_point - dest.x());
			}
			canvas->blitTexture(percent_surf_black, 0, dest);
		}
	}

	KRE::TexturePtr message_surf(render_updater_text(message_, KRE::Color(255, 255, 255)));
	if(!message_surf) {
		canvas->blitTexture(message_surf, 0, window_->width()/2 - message_surf->width()/2, 40 + window_->height()/2 - message_surf->height()/2);
	}
	
	progress_animation& anim = progress_animation::get();
	auto anim_tex = anim.tex();
	if(anim_tex != nullptr) {
		rect src = anim.calculate_rect(nframes_);
		rect dest(window_->width()/2 - src.w()/2, window_->height()/2 - src.h()*2, src.w(), src.h());
		canvas->blitTexture(anim_tex, src, 0, dest);
	}

	window_->swap();
}

COMMAND_LINE_UTILITY(update_launcher)
{
	std::deque<std::string> argv(args.begin(), args.end());

#ifdef _MSC_VER
	std::string anura_exe = "anura.exe";
#else
	std::string anura_exe = "./anura";
#endif

	std::string real_anura;
	bool update_anura = true;
	bool update_module = true;
	bool force = false;

	int timeout_ms = 10000;

	while(!argv.empty()) {
		std::string arg = argv.front();
		argv.pop_front();

		std::string arg_name = arg;
		std::string arg_value;
		auto equal_itor = std::find(arg_name.begin(), arg_name.end(), '=');
		if(equal_itor != arg_name.end()) {
			arg_value = std::string(equal_itor+1, arg_name.end());
			arg_name = std::string(arg_name.begin(), equal_itor);
		}

		if(arg_name == "--timeout") {
			ASSERT_LOG(arg_value.empty(), "Unrecognized argument: " << arg);
			timeout_ms = atoi(arg_value.c_str());
		} else if(arg_name == "--args") {
			ASSERT_LOG(arg_value.empty(), "Unrecognized argument: " << arg);
			break;
		} else if(arg_name == "--update_module" || arg_name == "--update-module") {
			if(arg_value == "true" || arg_value == "yes") {
				update_module = true;
			} else if(arg_value == "false" || arg_value == "no") {
				update_module = false;
			} else {
				ASSERT_LOG(false, "Unrecognized argument: " << arg);
			}
		} else if(arg_name == "--update_anura" || arg_name == "--update-anura") {
			if(arg_value == "true" || arg_value == "yes") {
				update_anura = true;
			} else if(arg_value == "false" || arg_value == "no") {
				update_anura = false;
			} else {
				ASSERT_LOG(false, "Unrecognized argument: " << arg);
			}
		} else if(arg_name == "--anura") {
			ASSERT_LOG(arg_value.empty() == false, "--anura requires a value giving the name of the anura module to use");
			real_anura = arg_value;
		} else if(arg_name == "--anura-exe" || arg_name == "--anura_exe") {
			ASSERT_LOG(arg_value.empty() == false, "--anura-exe requires a value giving the name of the anura executable to use");
			anura_exe = arg_value;
		} else if(arg_name == "--force") {
			force = true;
		} else {
			ASSERT_LOG(false, "Unrecognized argument: " << arg);
		}
	}

	ASSERT_LOG(real_anura != "", "Must provide a --anura argument with the name of the anura module to use");

	variant_builder update_info;

	if(update_anura || update_module) {
		boost::intrusive_ptr<module::client> cl, anura_cl;
		
		if(update_module) {
			cl.reset(new module::client);
			cl->install_module(module::get_module_name(), force);
			update_info.add("attempt_module", true);
		}

		if(update_anura) {
			anura_cl.reset(new module::client);
			//anura_cl->set_install_image(true);
			anura_cl->install_module(real_anura, force);
			update_info.add("attempt_anura", true);
		}

		int nbytes_transferred = 0, nbytes_anura_transferred = 0;
		int start_time = profile::get_tick_time();
		int original_start_time = profile::get_tick_time();
		bool timeout = false;
		LOG_INFO("Requesting update to module from server...");
		int nupdate_cycle = 0;

		{
		auto_update_window update_window;
		while(cl || anura_cl) {
			update_window.process();

			int nbytes_obtained = 0;
			int nbytes_needed = 0;

			++nupdate_cycle;

			if(cl) {
				const int transferred = cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += cl->nbytes_total();
				if(transferred != nbytes_transferred) {
					if(nupdate_cycle%10 == 0) {
						LOG_INFO("Transferred " << (transferred/1024) << "/" << (cl->nbytes_total()/1024) << "KB");
					}
					start_time = profile::get_tick_time();
					nbytes_transferred = transferred;
				}
			}

			if(anura_cl) {
				const int transferred = anura_cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += anura_cl->nbytes_total();
				if(transferred != nbytes_anura_transferred) {
					if(nupdate_cycle%10 == 0) {
						LOG_INFO("Transferred " << (transferred/1024) << "/" << (anura_cl->nbytes_total()/1024) << "KB");
					}
					start_time = profile::get_tick_time();
					nbytes_anura_transferred = transferred;
				}
			}

			const int time_taken = profile::get_tick_time() - start_time;
			if(time_taken > timeout_ms) {
				LOG_ERROR("Timed out updating module. Canceling. " << time_taken << "ms vs " << timeout_ms << "ms");
				break;
			}

			char msg[1024];
			sprintf(msg, "Updating Game. Transferred %.02f/%.02fMB", float(nbytes_obtained/(1024.0*1024.0)), float(nbytes_needed/(1024.0*1024.0)));

			update_window.set_message(msg);

			const float ratio = nbytes_needed <= 0 ? 0 : static_cast<float>(nbytes_obtained)/static_cast<float>(nbytes_needed);
			update_window.set_progress(ratio);
			update_window.draw();

			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				if(event.type == SDL_QUIT) {
					cl.reset();
					anura_cl.reset();
					break;
				}
			}

			const int target_end = profile::get_tick_time() + 50;
			while(static_cast<int>(profile::get_tick_time()) < target_end && (cl || anura_cl)) {
				if(cl && !cl->process()) {
					if(cl->error().empty() == false) {
						LOG_ERROR("Error while updating module: " << cl->error().c_str());
						update_info.add("module_error", variant(cl->error()));
					} else {
						update_info.add("complete_module", true);
					}
					cl.reset();
				}

				if(anura_cl && !anura_cl->process()) {
					if(anura_cl->error().empty() == false) {
						LOG_ERROR("Error while updating anura: " << anura_cl->error().c_str());
						update_info.add("anura_error", variant(anura_cl->error()));
					} else {
						update_info.add("complete_anura", true);
					}
					anura_cl.reset();
				}
			}
		}
		}
	}

	std::string update_info_str = "--auto-update-status=" + update_info.build().write_json(false, variant::JSON_COMPLIANT);

	const std::string working_dir = preferences::dlc_path() + "/" + real_anura;
	LOG_INFO("CHANGE DIRECTORY: " << working_dir);
	const int res = chdir(working_dir.c_str());
	ASSERT_LOG(res == 0, "Could not change directory to game working directory: " << working_dir);

	std::vector<char*> anura_args;
	anura_args.push_back(const_cast<char*>(anura_exe.c_str()));

	anura_args.push_back(const_cast<char*>(update_info_str.c_str()));

	for(const std::string& a : argv) {
		anura_args.push_back(const_cast<char*>(a.c_str()));
	}

	std::string command_line;
	for(const char* c : anura_args) {
		command_line += '"' + std::string(c) + "\" ";
	}

	LOG_INFO("EXECUTING: " << command_line);

	anura_args.push_back(nullptr);
	
	execv(anura_args[0], &anura_args[0]);

	const bool has_file = sys::file_exists(anura_exe);

#ifndef _MSC_VER
	if(has_file && !sys::is_file_executable(anura_exe)) {
		sys::set_file_executable(anura_exe);

		execv(anura_args[0], &anura_args[0]);

		if(!sys::is_file_executable(anura_exe)) {
			ASSERT_LOG(false, "Could not execute " << anura_exe << " from " << working_dir << " file does not appear to be executable");
		}
	}
#endif

	ASSERT_LOG(has_file, "Could not execute " << anura_exe << " from " << working_dir << ". The file does not exist. Try re-running the update process.");
	ASSERT_LOG(false, "Could not execute " << anura_exe << " from " << working_dir << ". The file exists and appears to be executable.");
}
