/*
	Copyright 2019 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <mutex>
#include "gui.h"
#include "osd.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_if.h"
#include "hw/maple/maple_devs.h"
#include "hw/naomi/naomi_cart.h"
#include "imgui/imgui.h"
#include "gles/imgui_impl_opengl3.h"
#include "imgui/roboto_medium.h"
#include "network/naomi_network.h"
#include "wsi/context.h"
#include "input/gamepad_device.h"
#include "gui_util.h"
#include "gui_android.h"
#include "game_scanner.h"
#include "version.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"

static bool game_started;

extern u8 kb_shift[MAPLE_PORTS]; // shift keys pressed (bitmask)
extern u8 kb_key[MAPLE_PORTS][6];		// normal keys pressed

int screen_dpi = 96;
int insetLeft, insetRight, insetTop, insetBottom;

static bool inited = false;
float scaling = 1;
GuiState gui_state = GuiState::Main;
static bool commandLineStart;
static u32 mouseButtons;
static int mouseX, mouseY;
static float mouseWheel;
static std::string error_msg;
static std::string osd_message;
static double osd_message_end;
static std::mutex osd_message_mutex;

static int map_system = 0;
static void display_vmus();
static void reset_vmus();
static void term_vmus();
static void displayCrosshairs();

GameScanner scanner;

static void emuEventCallback(Event event)
{
	switch (event)
	{
	case Event::Resume:
		game_started = true;
		break;
	case Event::Start:
		GamepadDevice::load_system_mappings();
		if (settings.platform.system == DC_PLATFORM_NAOMI)
			SetNaomiNetworkConfig(-1);
		if (config::AutoLoadState && settings.imgread.ImagePath[0] != '\0')
			dc_loadstate(config::SavestateSlot);
		break;
	case Event::Terminate:
		if (config::AutoSaveState && settings.imgread.ImagePath[0] != '\0')
			dc_savestate(config::SavestateSlot);
		break;
	default:
		break;
	}
}

void gui_init()
{
	if (inited)
		return;
	inited = true;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.Fonts->AddFontFromFileTTF("/storage/.config/emulationstation/themes/Crystal/_inc/fonts/OpenSans-Light.ttf", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

	io.IniFilename = NULL;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

	io.KeyMap[ImGuiKey_Tab] = 0x2B;
	io.KeyMap[ImGuiKey_LeftArrow] = 0x50;
	io.KeyMap[ImGuiKey_RightArrow] = 0x4F;
	io.KeyMap[ImGuiKey_UpArrow] = 0x52;
	io.KeyMap[ImGuiKey_DownArrow] = 0x51;
	io.KeyMap[ImGuiKey_PageUp] = 0x4B;
	io.KeyMap[ImGuiKey_PageDown] = 0x4E;
	io.KeyMap[ImGuiKey_Home] = 0x4A;
	io.KeyMap[ImGuiKey_End] = 0x4D;
	io.KeyMap[ImGuiKey_Insert] = 0x49;
	io.KeyMap[ImGuiKey_Delete] = 0x4C;
	io.KeyMap[ImGuiKey_Backspace] = 0x2A;
	io.KeyMap[ImGuiKey_Space] = 0x2C;
	io.KeyMap[ImGuiKey_Enter] = 0x28;
	io.KeyMap[ImGuiKey_Escape] = 0x29;
	io.KeyMap[ImGuiKey_A] = 0x04;
	io.KeyMap[ImGuiKey_C] = 0x06;
	io.KeyMap[ImGuiKey_V] = 0x19;
	io.KeyMap[ImGuiKey_X] = 0x1B;
	io.KeyMap[ImGuiKey_Y] = 0x1C;
	io.KeyMap[ImGuiKey_Z] = 0x1D;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    ImGui::GetStyle().TabRounding = 0;
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
    //ImGui::GetStyle().WindowRounding = 0;
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
    ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif

    // Setup Platform/Renderer bindings
    if (config::RendererType.isOpenGL())
    	ImGui_ImplOpenGL3_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
#if !(defined(_WIN32) || defined(__APPLE__) || defined(__SWITCH__)) || defined(TARGET_IPHONE)
    scaling = std::max(1.f, screen_dpi / 100.f * 0.75f);
#endif
    if (scaling > 1)
		ImGui::GetStyle().ScaleAllSizes(scaling);

    static const ImWchar ranges[] =
    {
    	0x0020, 0xFFFF, // All chars
        0,
    };

    io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17.f * scaling, nullptr, ranges);
    ImFontConfig font_cfg;
    font_cfg.MergeMode = true;
#ifdef _WIN32
    u32 cp = GetACP();
    std::string fontDir = std::string(nowide::getenv("SYSTEMROOT")) + "\\Fonts\\";
    switch (cp)
    {
    case 932:	// Japanese
		{
			font_cfg.FontNo = 2;	// UIGothic
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "msgothic.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
			font_cfg.FontNo = 2;	// Meiryo UI
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "Meiryo.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
		}
		break;
    case 949:	// Korean
		{
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Malgun.ttf").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			if (font == nullptr)
			{
				font_cfg.FontNo = 2;	// Dotum
				io.Fonts->AddFontFromFileTTF((fontDir + "Gulim.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			}
		}
    	break;
    case 950:	// Traditional Chinese
		{
			font_cfg.FontNo = 1; // Microsoft JhengHei UI Regular
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Msjh.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
			font_cfg.FontNo = 0;
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "MSJH.ttf").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
		}
    	break;
    case 936:	// Simplified Chinese
		io.Fonts->AddFontFromFileTTF((fontDir + "Simsun.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
    	break;
    default:
    	break;
    }
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
    std::string fontDir = std::string("/System/Library/Fonts/");
    
    extern std::string os_Locale();
    std::string locale = os_Locale();
    
    if (locale.find("ja") == 0)             // Japanese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "ヒラギノ角ゴシック W4.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
    }
    else if (locale.find("ko") == 0)       // Korean
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "AppleSDGothicNeo.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
    }
    else if (locale.find("zh-Hant") == 0)  // Traditional Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
    }
    else if (locale.find("zh-Hans") == 0)  // Simplified Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
    }
#elif defined(__ANDROID__)
    if (getenv("FLYCAST_LOCALE") != nullptr)
    {
    	const ImWchar *glyphRanges = nullptr;
    	std::string locale = getenv("FLYCAST_LOCALE");
        if (locale.find("ja") == 0)				// Japanese
        	glyphRanges = io.Fonts->GetGlyphRangesJapanese();
        else if (locale.find("ko") == 0)		// Korean
        	glyphRanges = io.Fonts->GetGlyphRangesKorean();
        else if (locale.find("zh_TW") == 0
        		|| locale.find("zh_HK") == 0)	// Traditional Chinese
        	glyphRanges = GetGlyphRangesChineseTraditionalOfficial();
        else if (locale.find("zh_CN") == 0)		// Simplified Chinese
        	glyphRanges = GetGlyphRangesChineseSimplifiedOfficial();

        if (glyphRanges != nullptr)
        	io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", 17.f * scaling, &font_cfg, glyphRanges);
    }

    // TODO Linux, iOS, ...
#endif
    INFO_LOG(RENDERER, "Screen DPI is %d, size %d x %d. Scaling by %.2f", screen_dpi, screen_width, screen_height, scaling);

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
    EventManager::listen(Event::Terminate, emuEventCallback);
}

void gui_keyboard_input(u16 wc)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
		io.AddInputCharacter(wc);
}

void gui_keyboard_inputUTF8(const std::string& s)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
		io.AddInputCharactersUTF8(s.c_str());
}

void gui_set_mouse_position(int x, int y)
{
	mouseX = x;
	mouseY = y;
}

void gui_set_mouse_button(int button, bool pressed)
{
	if (pressed)
		mouseButtons |= 1 << button;
	else
		mouseButtons &= ~(1 << button);
}

void gui_set_mouse_wheel(float delta)
{
	mouseWheel += delta;
}

static void ImGui_Impl_NewFrame()
{
	if (config::RendererType.isOpenGL())
		ImGui_ImplOpenGL3_NewFrame();
#ifdef _WIN32
	else if (config::RendererType.isDirectX())
		ImGui_ImplDX9_NewFrame();
#endif
	ImGui::GetIO().DisplaySize.x = screen_width;
	ImGui::GetIO().DisplaySize.y = screen_height;

	ImGuiIO& io = ImGui::GetIO();

	// Read keyboard modifiers inputs
	io.KeyCtrl = 0;
	io.KeyShift = 0;
	io.KeyAlt = false;
	io.KeySuper = false;
	memset(&io.KeysDown[0], 0, sizeof(io.KeysDown));
	for (int port = 0; port < 4; port++)
	{
		io.KeyCtrl |= (kb_shift[port] & (0x01 | 0x10)) != 0;
		io.KeyShift |= (kb_shift[port] & (0x02 | 0x20)) != 0;

		for (int i = 0; i < IM_ARRAYSIZE(kb_key[0]); i++)
			if (kb_key[port][i] != 0)
				io.KeysDown[kb_key[port][i]] = true;
	}
	if (mouseX < 0 || mouseX >= screen_width || mouseY < 0 || mouseY >= screen_height)
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	else
		io.MousePos = ImVec2(mouseX, mouseY);
	static bool delayTouch;
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
	// Delay touch by one frame to allow widgets to be hovered before click
	// This is required for widgets using ImGuiButtonFlags_AllowItemOverlap such as TabItem's
	if (!delayTouch && (mouseButtons & (1 << 0)) != 0 && !io.MouseDown[ImGuiMouseButton_Left])
		delayTouch = true;
	else
		delayTouch = false;
#endif
	if (io.WantCaptureMouse)
	{
		io.MouseWheel = -mouseWheel / 16;
		mouseWheel = 0;
	}
	if (!delayTouch)
		io.MouseDown[ImGuiMouseButton_Left] = (mouseButtons & (1 << 0)) != 0;
	io.MouseDown[ImGuiMouseButton_Right] = (mouseButtons & (1 << 1)) != 0;
	io.MouseDown[ImGuiMouseButton_Middle] = (mouseButtons & (1 << 2)) != 0;
	io.MouseDown[3] = (mouseButtons & (1 << 3)) != 0;

	io.NavInputs[ImGuiNavInput_Activate] = (kcode[0] & DC_BTN_A) == 0;
	io.NavInputs[ImGuiNavInput_Cancel] = (kcode[0] & DC_BTN_B) == 0;
	io.NavInputs[ImGuiNavInput_Input] = (kcode[0] & DC_BTN_X) == 0;
	io.NavInputs[ImGuiNavInput_DpadLeft] = (kcode[0] & DC_DPAD_LEFT) == 0;
	io.NavInputs[ImGuiNavInput_DpadRight] = (kcode[0] & DC_DPAD_RIGHT) == 0;
	io.NavInputs[ImGuiNavInput_DpadUp] = (kcode[0] & DC_DPAD_UP) == 0;
	io.NavInputs[ImGuiNavInput_DpadDown] = (kcode[0] & DC_DPAD_DOWN) == 0;
	io.NavInputs[ImGuiNavInput_LStickLeft] = joyx[0] < 0 ? -(float)joyx[0] / 128 : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickLeft] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickLeft] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickRight] = joyx[0] > 0 ? (float)joyx[0] / 128 : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickRight] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickRight] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickUp] = joyy[0] < 0 ? -(float)joyy[0] / 128.f : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickUp] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickUp] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickDown] = joyy[0] > 0 ? (float)joyy[0] / 128.f : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickDown] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickDown] = 0.f;

	ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
}

void gui_set_insets(int left, int right, int top, int bottom)
{
	insetLeft = left;
	insetRight = right;
	insetTop = top;
	insetBottom = bottom;
}

#if 0
#include "oslib/timeseries.h"
TimeSeries renderTimes;
TimeSeries vblankTimes;

void gui_plot_render_time(int width, int height)
{
	std::vector<float> v = renderTimes.data();
	ImGui::PlotLines("Render Times", v.data(), v.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));
	ImGui::Text("StdDev: %.1f%%", renderTimes.stddev() * 100.f / 0.01666666667f);
	v = vblankTimes.data();
	ImGui::PlotLines("VBlank", v.data(), v.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));
	ImGui::Text("StdDev: %.1f%%", vblankTimes.stddev() * 100.f / 0.01666666667f);
}
#endif

void gui_open_settings()
{
	if (gui_state == GuiState::Closed)
	{
		gui_state = GuiState::Commands;
		HideOSD();
	}
	else if (gui_state == GuiState::VJoyEdit)
	{
		gui_state = GuiState::VJoyEditCommands;
	}
	else if (gui_state == GuiState::Loading)
	{
		dc_cancel_load();
		gui_state = GuiState::Main;
	}
	else if (gui_state == GuiState::Commands)
	{
		gui_state = GuiState::Closed;
		GamepadDevice::load_system_mappings();
		dc_resume();
	}
}

void gui_start_game(const std::string& path)
{
	dc_term_game();
	reset_vmus();

	scanner.stop();
	gui_state = GuiState::Loading;
	static std::string path_copy;
	path_copy = path;	// path may be a local var

	dc_load_game(path.empty() ? NULL : path_copy.c_str());
}

void gui_stop_game(const std::string& message)
{
	if (!commandLineStart)
	{
		// Exit to main menu
		dc_term_game();
		gui_state = GuiState::Main;
		game_started = false;
		settings.imgread.ImagePath[0] = '\0';
		reset_vmus();
		if (!message.empty())
			error_msg = "Flycast已停止.\n\n" + message;
	}
	else
	{
		// Exit emulator
		dc_exit();
	}
}

static void gui_display_commands()
{
	if (dc_is_running())
		dc_stop();

   	display_vmus();

    centerNextWindow();
    ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

    ImGui::Begin("##commands", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (settings.imgread.ImagePath[0] == '\0')
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if (ImGui::Button("即时读档", ImVec2(110 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;
		dc_loadstate(config::SavestateSlot);
	}
	ImGui::SameLine();
	std::string slot = "Slot " + std::to_string((int)config::SavestateSlot + 1);
	if (ImGui::Button(slot.c_str(), ImVec2(80 * scaling - ImGui::GetStyle().FramePadding.x, 50 * scaling)))
		ImGui::OpenPopup("slot_select_popup");
    if (ImGui::BeginPopup("slot_select_popup"))
    {
        for (int i = 0; i < 10; i++)
            if (ImGui::Selectable(std::to_string(i + 1).c_str(), config::SavestateSlot == i, 0,
            		ImVec2(ImGui::CalcTextSize("Slot 8").x, 0))) {
                config::SavestateSlot = i;
                SaveSettings();
            }
        ImGui::EndPopup();
    }
	ImGui::SameLine();
	if (ImGui::Button("即时存档", ImVec2(110 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;
		dc_savestate(config::SavestateSlot);
	}
	if (settings.imgread.ImagePath[0] == '\0')
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	ImGui::Columns(2, "按键", false);
	if (ImGui::Button("设置", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("继续", ImVec2(150 * scaling, 50 * scaling)))
	{
		GamepadDevice::load_system_mappings();
		gui_state = GuiState::Closed;
	}

	ImGui::NextColumn();
	const char *disk_label = libGDR_GetDiscType() == Open ? "插入光盘" : "弹出光盘";
	if (ImGui::Button(disk_label, ImVec2(150 * scaling, 50 * scaling)))
	{
		if (libGDR_GetDiscType() == Open)
		{
			gui_state = GuiState::SelectDisk;
		}
		else
		{
			DiscOpenLid();
			gui_state = GuiState::Closed;
		}
	}
	ImGui::NextColumn();
	if (ImGui::Button("金手指", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Cheats;
	}
	ImGui::Columns(1, nullptr, false);
	if (ImGui::Button("退出", ImVec2(300 * scaling + ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1,
			50 * scaling)))
	{
		gui_stop_game();
	}

	ImGui::End();
}

const char *maple_device_types[] = { "无", "世嘉控制器", "光枪", "键盘", "鼠标", "双截棍", "ASCII 摇杆" };
const char *maple_expansion_device_types[] = { "无", "世嘉 VMU", "Purupuru 震动卡", "麦克风" };

static const char *maple_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaController:
		return maple_device_types[1];
	case MDT_LightGun:
		return maple_device_types[2];
	case MDT_Keyboard:
		return maple_device_types[3];
	case MDT_Mouse:
		return maple_device_types[4];
	case MDT_TwinStick:
		return maple_device_types[5];
	case MDT_AsciiStick:
		return maple_device_types[6];
	case MDT_None:
	default:
		return maple_device_types[0];
	}
}

static MapleDeviceType maple_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaController;
	case 2:
		return MDT_LightGun;
	case 3:
		return MDT_Keyboard;
	case 4:
		return MDT_Mouse;
	case 5:
		return MDT_TwinStick;
	case 6:
		return MDT_AsciiStick;
	case 0:
	default:
		return MDT_None;
	}
}

static const char *maple_expansion_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaVMU:
		return maple_expansion_device_types[1];
	case MDT_PurupuruPack:
		return maple_expansion_device_types[2];
	case MDT_Microphone:
		return maple_expansion_device_types[3];
	case MDT_None:
	default:
		return maple_expansion_device_types[0];
	}
}

const char *maple_ports[] = { "无", "A", "B", "C", "D", "All" };
const DreamcastKey button_keys[] = {
		DC_BTN_START, DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, DC_DPAD_UP, DC_DPAD_DOWN, DC_DPAD_LEFT, DC_DPAD_RIGHT,
		EMU_BTN_MENU, EMU_BTN_ESCAPE, EMU_BTN_FFORWARD, EMU_BTN_TRIGGER_LEFT, EMU_BTN_TRIGGER_RIGHT,
		DC_BTN_C, DC_BTN_D, DC_BTN_Z, DC_DPAD2_UP, DC_DPAD2_DOWN, DC_DPAD2_LEFT, DC_DPAD2_RIGHT,
		DC_BTN_RELOAD,
		EMU_BTN_ANA_UP, EMU_BTN_ANA_DOWN, EMU_BTN_ANA_LEFT, EMU_BTN_ANA_RIGHT
};
const char *button_names[] = {
		"开始", "A", "B", "X", "Y", "方向 上", "方向 下", "方向 左", "方向 右",
		"菜单", "退出", "加速", "左扳机", "右扳机",
		"C", "D", "Z", "右 方向 上", "右 方向 下", "右 方向 左", "右 方向 右",
		"重新加载",
		"左摇杆 上", "左摇杆 下", "左摇杆 左", "左摇杆 右"
};
const char *arcade_button_names[] = {
		"开始", "按键 1", "按键 2", "按键 3", "按键 4", "上", "下", "左", "右",
		"菜单", "退出", "加速", "N/A", "N/A",
		"服务", "投币", "测试", "按键 5", "按键 6", "按键 7", "按键 8",
		"重新加载",
		"不适用", "不适用", "不适用", "不适用"
};
const DreamcastKey axis_keys[] = {
		DC_AXIS_X, DC_AXIS_Y, DC_AXIS_LT, DC_AXIS_RT, DC_AXIS_X2, DC_AXIS_Y2, EMU_AXIS_DPAD1_X, EMU_AXIS_DPAD1_Y,
		EMU_AXIS_DPAD2_X, EMU_AXIS_DPAD2_Y, EMU_AXIS_BTN_START, EMU_AXIS_BTN_A, EMU_AXIS_BTN_B, EMU_AXIS_BTN_X, EMU_AXIS_BTN_Y,
		EMU_AXIS_BTN_C, EMU_AXIS_BTN_D, EMU_AXIS_BTN_Z, EMU_AXIS_DPAD2_UP, EMU_AXIS_DPAD2_DOWN, EMU_AXIS_DPAD2_LEFT, EMU_AXIS_DPAD2_RIGHT
};
const char *axis_names[] = {
		"左轴 X", "左轴 Y", "左扳机", "右扳机", "右轴 X", "右轴 Y", "方向 X", "方向 Y",
		"右方向 X", "右方向 Y", "开始", "A", "B", "X", "Y",
		"C", "D", "Z", "不适用", "不适用", "不适用", "不适用"
};
const char *arcade_axis_names[] = {
		"左轴 X", "左轴 Y", "左扳机", "右扳机", "右轴 X", "右轴 Y", "方向 X", "方向 Y",
		"右方向 X", "右方向 Y", "开始", "按键 1", "按键 2", "按键 3", "按键 4",
		"服务", "投币", "测试", "按键 5", "按键 6", "按键 7", "按键 8"
};
static_assert(ARRAY_SIZE(button_keys) == ARRAY_SIZE(button_names), "invalid size");
static_assert(ARRAY_SIZE(button_keys) == ARRAY_SIZE(arcade_button_names), "invalid size");
static_assert(ARRAY_SIZE(axis_keys) == ARRAY_SIZE(axis_names), "invalid size");
static_assert(ARRAY_SIZE(axis_keys) == ARRAY_SIZE(arcade_axis_names), "invalid size");

static MapleDeviceType maple_expansion_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaVMU;
	case 2:
		return MDT_PurupuruPack;
	case 3:
		return MDT_Microphone;
	case 0:
	default:
		return MDT_None;
	}
}

static std::shared_ptr<GamepadDevice> mapped_device;
static u32 mapped_code;
static double map_start_time;
static bool arcade_button_mode;
static u32 gamepad_port;

static void detect_input_popup(int index, bool analog)
{
	ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal(analog ? "映射轴" : "映射按键", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("等待 %s '%s'...", analog ? "轴" : "按键",
				analog ? arcade_button_mode ? arcade_axis_names[index] : axis_names[index]
						: arcade_button_mode ? arcade_button_names[index] : button_names[index]);
		double now = os_GetSeconds();
		ImGui::Text("超时 %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != (u32)-1)
		{
			std::shared_ptr<InputMapping> input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				if (analog)
				{
					u32 previous_mapping = input_mapping->get_axis_code(gamepad_port, axis_keys[index]);
					bool inverted = false;
					if (previous_mapping != (u32)-1)
						inverted = input_mapping->get_axis_inverted(gamepad_port, previous_mapping);
					// FIXME Allow inverted to be set
					input_mapping->set_axis(gamepad_port, axis_keys[index], mapped_code, inverted);
				}
				else
					input_mapping->set_button(gamepad_port, button_keys[index], mapped_code);
			}
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		else if (now - map_start_time >= 5)
		{
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

static void controller_mapping_popup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	fullScreenWindow(true);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("控制器映射", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float width = (ImGui::GetIO().DisplaySize.x - insetLeft - insetRight - style.ItemSpacing.x) / 2 - style.WindowBorderSize - style.WindowPadding.x;
		const float col_width = (width - style.GrabMinSize - style.ItemSpacing.x
				- (ImGui::CalcTextSize("映射").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)
				- (ImGui::CalcTextSize("移除映射").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)) / 2;

		std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("完成", ImVec2(100 * scaling, 30 * scaling)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping(map_system);
		}
		ImGui::SetItemDefaultFocus();

		if (gamepad->maple_port() == MAPLE_PORTS)
		{
			ImGui::SameLine();
			float w = ImGui::CalcItemWidth();
			ImGui::PushItemWidth(w / 2);
			if (ImGui::BeginCombo("端口", maple_ports[gamepad_port + 1]))
			{
				for (u32 j = 0; j < MAPLE_PORTS; j++)
				{
					bool is_selected = gamepad_port == j;
					if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
						gamepad_port = j;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
		}
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Dreamcast 键位").x
			- ImGui::GetStyle().FramePadding.x * 3.0f - ImGui::GetStyle().ItemSpacing.x * 3.0f);

		ImGui::AlignTextToFramePadding();

		const char* items[] = { "Dreamcast 键位", "街机 键位" };
		static int item_current_map_idx = 0;
		static int last_item_current_map_idx = 2;

		// Here our selection data is an index.

		ImGui::PushItemWidth(ImGui::CalcTextSize("Dreamcast 键位").x + ImGui::GetStyle().ItemSpacing.x * 2.0f * 3);

		ImGui::Combo("", &item_current_map_idx, items, IM_ARRAYSIZE(items));

		if (item_current_map_idx != last_item_current_map_idx)
		{
			gamepad->save_mapping(map_system);
		}

		if (item_current_map_idx == 0)
		{
			arcade_button_mode = false;
			map_system = DC_PLATFORM_DREAMCAST;
		}
		else if (item_current_map_idx == 1)
		{
			arcade_button_mode = true;
			map_system = DC_PLATFORM_NAOMI;
		}

		if (item_current_map_idx != last_item_current_map_idx)
		{
			gamepad->find_mapping(map_system);
			input_mapping = gamepad->get_input_mapping();

			last_item_current_map_idx = item_current_map_idx;
		}

		char key_id[32];
		ImGui::BeginGroup();
		ImGui::Text("  按键  ");

		ImGui::BeginChildFrame(ImGui::GetID("按键"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "bindings", false);
		ImGui::SetColumnWidth(0, col_width);
		ImGui::SetColumnWidth(1, col_width);

		gamepad->find_mapping(map_system);
		for (u32 j = 0; j < ARRAY_SIZE(button_keys); j++)
		{
			sprintf(key_id, "key_id%d", j);
			ImGui::PushID(key_id);

			const char *btn_name = arcade_button_mode ? arcade_button_names[j] : button_names[j];
			const char *game_btn_name = GetCurrentGameButtonName(button_keys[j]);
			if (game_btn_name != nullptr)
				ImGui::Text("%s - %s", btn_name, game_btn_name);
			else
				ImGui::Text("%s", btn_name);

			ImGui::NextColumn();
			u32 code = input_mapping->get_button_code(gamepad_port, button_keys[j]);
			if (code != (u32)-1)
			{
				const char *label = gamepad->get_button_name(code);
				if (label != nullptr)
					ImGui::Text("%s", label);
				else
					ImGui::Text("[%d]", code);
			}
			ImGui::NextColumn();
			if (ImGui::Button("映射"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("映射按键");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detect_btn_input([](u32 code)
						{
							mapped_code = code;
						});
			}
			detect_input_popup(j, false);
			ImGui::SameLine();
			if (ImGui::Button("移除映射"))
			{
				input_mapping = gamepad->get_input_mapping();
				input_mapping->clear_button(gamepad_port, button_keys[j], j);
			}
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::Columns(1, nullptr, false);
	    scrollWhenDraggingOnVoid();
	    windowDragScroll();

		ImGui::EndChildFrame();
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("  轴  ");
		ImGui::BeginChildFrame(ImGui::GetID("模拟"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "anabindings", false);
		ImGui::SetColumnWidth(0, col_width);
		ImGui::SetColumnWidth(1, col_width);

		for (u32 j = 0; j < ARRAY_SIZE(axis_keys); j++)
		{
			sprintf(key_id, "axis_id%d", j);
			ImGui::PushID(key_id);

			const char *axis_name = arcade_button_mode ? arcade_axis_names[j] : axis_names[j];
			const char *game_axis_name = GetCurrentGameAxisName(axis_keys[j]);
			if (game_axis_name != nullptr)
				ImGui::Text("%s - %s", axis_name, game_axis_name);
			else
				ImGui::Text("%s", axis_name);

			ImGui::NextColumn();
			u32 code = input_mapping->get_axis_code(gamepad_port, axis_keys[j]);
			if (code != (u32)-1)
			{
				const char *label = gamepad->get_axis_name(code);
				if (label != nullptr)
					ImGui::Text("%s", label);
				else
					ImGui::Text("[%d]", code);
			}
			ImGui::NextColumn();
			if (ImGui::Button("映射"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("映射轴");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detect_axis_input([](u32 code)
						{
							mapped_code = code;
						});
			}
			detect_input_popup(j, true);
			ImGui::SameLine();
			if (ImGui::Button("移除映射"))
			{
				input_mapping = gamepad->get_input_mapping();
				input_mapping->clear_axis(gamepad_port, axis_keys[j], j);
			}
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::Columns(1, nullptr, false);
	    scrollWhenDraggingOnVoid();
	    windowDragScroll();
		ImGui::EndChildFrame();
		ImGui::EndGroup();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

static void error_popup()
{
	if (!error_msg.empty())
	{
		ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
		ImGui::OpenPopup("错误");
		if (ImGui::BeginPopupModal("错误", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
			ImGui::TextWrapped("%s", error_msg.c_str());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
			float currentwidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
			if (ImGui::Button("确认", ImVec2(80.f * scaling, 0.f)))
			{
				error_msg.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::PopStyleVar();
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
	}
}

static void contentpath_warning_popup()
{
    static bool show_contentpath_selection;

    if (scanner.content_path_looks_incorrect)
    {
        ImGui::OpenPopup("文件位置不正确?");
        if (ImGui::BeginPopupModal("文件位置不正确?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
            ImGui::TextWrapped("  已扫描 %d 个文件夹, 但找不到游戏!  ", scanner.empty_folders_scanned);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
            float currentwidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * scaling);
            if (ImGui::Button("重选", ImVec2(100.f * scaling, 0.f)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                show_contentpath_selection = true;
            }
            
            ImGui::SameLine();
            ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * scaling);
            if (ImGui::Button("取消", ImVec2(100.f * scaling, 0.f)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                scanner.stop();
                config::ContentPath.get().clear();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }
    }
    if (show_contentpath_selection)
    {
        scanner.stop();
        ImGui::OpenPopup("选择目录");
        select_file_popup("选择目录", [](bool cancelled, std::string selection)
        {
            show_contentpath_selection = false;
            if (!cancelled)
            {
            	config::ContentPath.get().clear();
                config::ContentPath.get().push_back(selection);
            }
            scanner.refresh();
        });
    }
}

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

static void gui_display_settings()
{
	static bool maple_devices_changed;

	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("设置", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("完成", ImVec2(100 * scaling, 30 * scaling)))
    {
    	if (game_started)
    		gui_state = GuiState::Commands;
    	else
    		gui_state = GuiState::Main;
    	if (maple_devices_changed)
    	{
    		maple_devices_changed = false;
    		if (game_started && settings.platform.system == DC_PLATFORM_DREAMCAST)
    		{
    			maple_ReconnectDevices();
    			reset_vmus();
    		}
    	}
       	SaveSettings();
    }
	if (game_started)
	{
	    ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, normal_padding.y));
		if (config::Settings::instance().hasPerGameConfig())
		{
			if (ImGui::Button("删除游戏配置", ImVec2(0, 30 * scaling)))
			{
				config::Settings::instance().setPerGameConfig(false);
				config::Settings::instance().load(false);
				loadGameSpecificSettings();
			}
		}
		else
		{
			if (ImGui::Button("设置游戏配置", ImVec2(0, 30 * scaling)))
				config::Settings::instance().setPerGameConfig(true);
		}
	    ImGui::PopStyleVar();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 6 * scaling));		// from 4, 3

    if (ImGui::BeginTabBar("设置", ImGuiTabBarFlags_NoTooltip))
    {
		if (ImGui::BeginTabItem("常规"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			const char *languages[] = { "日语", "英语", "德语", "法语", "西班牙语", "意大利语", "默认" };
			OptionComboBox("语言", config::Language, languages, ARRAY_SIZE(languages),
				"在Dreamcast BIOS中设置的语言");

			const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "默认" };
			OptionComboBox("制式", config::Broadcast, broadcast, ARRAY_SIZE(broadcast),
					"非 VGA 模式下的 TV 制式标准");

			const char *region[] = { "日本", "美国", "欧洲", "默认" };
			OptionComboBox("区域", config::Region, region, ARRAY_SIZE(region),
						"BIOS 区域");

			const char *cable[] = { "VGA", "RGB 分量线", "TV 分量线" };
			if (config::Cable.isReadOnly())
			{
		        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::BeginCombo("缆线", cable[config::Cable == 0 ? 0 : config::Cable - 1], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
				{
					bool is_selected = i == 0 ? config::Cable <= 1 : config::Cable - 1 == i;
					if (ImGui::Selectable(cable[i], &is_selected))
						config::Cable = i == 0 ? 0 : i + 1;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (config::Cable.isReadOnly())
			{
		        ImGui::PopItemFlag();
		        ImGui::PopStyleVar();
			}
            ImGui::SameLine();
            ShowHelpMarker("视频连接类型");

#if !defined(TARGET_IPHONE)
            ImVec2 size;
            size.x = 0.0f;
            size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
            				* (config::ContentPath.get().size() + 1) ;//+ ImGui::GetStyle().FramePadding.y * 2.f;

            if (ImGui::ListBoxHeader("文件位置", size))
            {
            	int to_delete = -1;
                for (u32 i = 0; i < config::ContentPath.get().size(); i++)
                {
                	ImGui::PushID(config::ContentPath.get()[i].c_str());
                    ImGui::AlignTextToFramePadding();
                	ImGui::Text("%s", config::ContentPath.get()[i].c_str());
                	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x);
                	if (ImGui::Button("X"))
                		to_delete = i;
                	ImGui::PopID();
                }
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24 * scaling, 3 * scaling));
                if (ImGui::Button("添加"))
                	ImGui::OpenPopup("选择目录");
                select_file_popup("选择目录", [](bool cancelled, std::string selection)
                		{
                			if (!cancelled)
                			{
                				scanner.stop();
                				config::ContentPath.get().push_back(selection);
                				scanner.refresh();
                			}
                		});
                ImGui::PopStyleVar();
                scrollWhenDraggingOnVoid();

        		ImGui::ListBoxFooter();
            	if (to_delete >= 0)
            	{
            		scanner.stop();
            		config::ContentPath.get().erase(config::ContentPath.get().begin() + to_delete);
        			scanner.refresh();
            	}
            }
            ImGui::SameLine();
            ShowHelpMarker("存放游戏的目录");

#if defined(__linux__) && !defined(__ANDROID__)
            if (ImGui::ListBoxHeader("数据目录", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_data_path("").c_str());
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("包含BIOS文件以及保存的VMU和状态的目录");
#else
            if (ImGui::ListBoxHeader("主目录", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_config_path("").c_str());
#ifdef __ANDROID__
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("变更").x - ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button("变更"))
                	gui_state = GuiState::Onboarding;
#endif
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("保存Flycast配置文件与VMU的目录, BIOS文件应该放入一个名为 \"data\" 的子文件夹中");
#endif // !linux
#endif // !TARGET_IPHONE

			if (OptionCheckbox("隐藏传统 Naomi Roms", config::HideLegacyNaomiRoms,
					"浏览文件时隐藏 .bin .dat .lst文件"))
				scanner.refresh();
	    	ImGui::Text("自动即使读档/存档:");
			OptionCheckbox("读档", config::AutoLoadState,
					"启动时自动读档");
			ImGui::SameLine();
			OptionCheckbox("存档", config::AutoSaveState,
					"退出时自动存档 ");

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("控制器"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			header("物理设备");
		    {
				ImGui::Columns(4, "物理设备", false);
				ImGui::Text("系统");
				ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("系统").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
				ImGui::NextColumn();
				ImGui::Text("名称");
				ImGui::NextColumn();
				ImGui::Text("端口");
				ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("无").x * 1.6f + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight()
					+ ImGui::GetStyle().ItemInnerSpacing.x	+ ImGui::GetStyle().ItemSpacing.x);
				ImGui::NextColumn();
				ImGui::NextColumn();
				for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
				{
					std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
					if (!gamepad)
						continue;
					ImGui::Text("%s", gamepad->api_name().c_str());
					ImGui::NextColumn();
					ImGui::Text("%s", gamepad->name().c_str());
					ImGui::NextColumn();
					char port_name[32];
					sprintf(port_name, "##mapleport%d", i);
					ImGui::PushID(port_name);
					if (ImGui::BeginCombo(port_name, maple_ports[gamepad->maple_port() + 1]))
					{
						for (int j = -1; j < (int)ARRAY_SIZE(maple_ports) - 1; j++)
						{
							bool is_selected = gamepad->maple_port() == j;
							if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
								gamepad->set_maple_port(j);
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}

						ImGui::EndCombo();
					}
					ImGui::NextColumn();
					if (gamepad->remappable() && ImGui::Button("映射"))
					{
						gamepad_port = 0;
						gamepad->verify_or_create_system_mappings();
						ImGui::OpenPopup("控制器映射");
					}

					controller_mapping_popup(gamepad);

#ifdef __ANDROID__
					if (gamepad->is_virtual_gamepad())
					{
						if (ImGui::Button("编辑"))
						{
							vjoy_start_editing();
							gui_state = GuiState::VJoyEdit;
						}
						ImGui::SameLine();
						OptionSlider("触摸", config::VirtualGamepadVibration, 0, 60);
					}
#endif
					ImGui::NextColumn();
					ImGui::PopID();
				}
		    }
	    	ImGui::Columns(1, NULL, false);

	    	ImGui::Spacing();
	    	OptionSlider("鼠标灵敏度", config::MouseSensitivity, 1, 500);
#ifdef _WIN32
	    	OptionCheckbox("使用原始输入", config::UseRawInput, "支持多种指向设备(鼠标,光枪)和键盘");
#endif

			ImGui::Spacing();
			header("Dreamcast 设备");
		    {
				for (int bus = 0; bus < MAPLE_PORTS; bus++)
				{
					ImGui::Text("设备 %c", bus + 'A');
					ImGui::SameLine();
					char device_name[32];
					sprintf(device_name, "##设备%d", bus);
					float w = ImGui::CalcItemWidth() / 3;
					ImGui::PushItemWidth(w);
					if (ImGui::BeginCombo(device_name, maple_device_name(config::MapleMainDevices[bus]), ImGuiComboFlags_None))
					{
						for (int i = 0; i < IM_ARRAYSIZE(maple_device_types); i++)
						{
							bool is_selected = config::MapleMainDevices[bus] == maple_device_type_from_index(i);
							if (ImGui::Selectable(maple_device_types[i], &is_selected))
							{
								config::MapleMainDevices[bus] = maple_device_type_from_index(i);
								maple_devices_changed = true;
							}
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					int port_count = config::MapleMainDevices[bus] == MDT_SegaController ? 2
							: config::MapleMainDevices[bus] == MDT_LightGun || config::MapleMainDevices[bus] == MDT_TwinStick || config::MapleMainDevices[bus] == MDT_AsciiStick ? 1
							: 0;
					for (int port = 0; port < port_count; port++)
					{
						ImGui::SameLine();
						sprintf(device_name, "##设备%d.%d", bus, port + 1);
						ImGui::PushID(device_name);
						if (ImGui::BeginCombo(device_name, maple_expansion_device_name(config::MapleExpansionDevices[bus][port]), ImGuiComboFlags_None))
						{
							for (int i = 0; i < IM_ARRAYSIZE(maple_expansion_device_types); i++)
							{
								bool is_selected = config::MapleExpansionDevices[bus][port] == maple_expansion_device_type_from_index(i);
								if (ImGui::Selectable(maple_expansion_device_types[i], &is_selected))
								{
									config::MapleExpansionDevices[bus][port] = maple_expansion_device_type_from_index(i);
									maple_devices_changed = true;
								}
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						ImGui::PopID();
					}
					if (config::MapleMainDevices[bus] == MDT_LightGun)
					{
						ImGui::SameLine();
						sprintf(device_name, "##设备%d.xhair", bus);
						ImGui::PushID(device_name);
						u32 color = config::CrosshairColor[bus];
						float xhairColor[4] {
							(color & 0xff) / 255.f,
							((color >> 8) & 0xff) / 255.f,
							((color >> 16) & 0xff) / 255.f,
							((color >> 24) & 0xff) / 255.f
						};
						ImGui::ColorEdit4("准心颜色", xhairColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
								| ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel);
						ImGui::SameLine();
						bool enabled = color != 0;
						ImGui::Checkbox("准心", &enabled);
						if (enabled)
						{
							config::CrosshairColor[bus] = (u8)(xhairColor[0] * 255.f)
									| ((u8)(xhairColor[1] * 255.f) << 8)
									| ((u8)(xhairColor[2] * 255.f) << 16)
									| ((u8)(xhairColor[3] * 255.f) << 24);
							if (config::CrosshairColor[bus] == 0)
								config::CrosshairColor[bus] = 0xC0FFFFFF;
						}
						else
						{
							config::CrosshairColor[bus] = 0;
						}
						ImGui::PopID();
					}
					ImGui::PopItemWidth();
				}
		    }

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("视频"))
		{
			int renderApi;
			bool perPixel;
			switch (config::RendererType)
			{
			default:
			case RenderType::OpenGL:
				renderApi = 0;
				perPixel = false;
				break;
			case RenderType::OpenGL_OIT:
				renderApi = 0;
				perPixel = true;
				break;
			case RenderType::Vulkan:
				renderApi = 1;
				perPixel = false;
				break;
			case RenderType::Vulkan_OIT:
				renderApi = 1;
				perPixel = true;
				break;
			case RenderType::DirectX9:
				renderApi = 2;
				perPixel = false;
				break;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
#if !defined(__APPLE__)
			bool has_per_pixel = false;
			if (renderApi == 0)
				has_per_pixel = !theGLContext.IsGLES() && theGLContext.GetMajorVersion() >= 4;
#ifdef USE_VULKAN
			else
				has_per_pixel = VulkanContext::Instance()->SupportsFragmentShaderStoresAndAtomics();
#endif
#else
			bool has_per_pixel = false;
#endif
		    header("透明排序");
		    {
		    	int renderer = perPixel ? 2 : config::PerStripSorting ? 1 : 0;
		    	ImGui::Columns(has_per_pixel ? 3 : 2, "渲染器", false);
		    	ImGui::RadioButton("三角形单位", &renderer, 0);
	            ImGui::SameLine();
	            ShowHelpMarker("对每个三角形的透明多边形进行排序. 速度快但可能会产生图形故障");
            	ImGui::NextColumn();
		    	ImGui::RadioButton("条形单位", &renderer, 1);
	            ImGui::SameLine();
	            ShowHelpMarker("按条形对透明多边形进行排序. 速度快但可能会产生图形故障");
	            if (has_per_pixel)
	            {
	            	ImGui::NextColumn();
	            	ImGui::RadioButton("像素单位", &renderer, 2);
	            	ImGui::SameLine();
	            	ShowHelpMarker("按像素对透明多边形进行排序. 速度慢但准确");
	            }
		    	ImGui::Columns(1, NULL, false);
		    	switch (renderer)
		    	{
		    	case 0:
		    		perPixel = false;
		    		config::PerStripSorting.set(false);
		    		break;
		    	case 1:
		    		perPixel = false;
		    		config::PerStripSorting.set(true);
		    		break;
		    	case 2:
		    		perPixel = true;
		    		break;
		    	}
		    }
	    	ImGui::Spacing();
		    header("渲染选项");
		    {
		    	ImGui::Text("自动跳帧:");
		    	ImGui::Columns(3, "自动跳帧", false);
		    	OptionRadioButton("禁用", config::AutoSkipFrame, 0, "不会跳帧");
            	ImGui::NextColumn();
		    	OptionRadioButton("标准", config::AutoSkipFrame, 1, "当 GPU 和 CPU 都运行缓慢时跳过1帧");
            	ImGui::NextColumn();
		    	OptionRadioButton("最大值", config::AutoSkipFrame, 2, "当 GPU 运行缓慢时跳过1帧");
		    	ImGui::Columns(1, nullptr, false);

		    	OptionCheckbox("阴影", config::ModifierVolumes,
		    			"启用体积编辑，通常用于阴影");
		    	OptionCheckbox("雾化", config::Fog, "启用雾化效果");
		    	OptionCheckbox("宽屏", config::Widescreen,
		    			"在正常的4:3高宽比之外绘制几何图形. 可能会在显示区域产生图形故障");
		    	if (!config::Widescreen)
		    	{
			        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		    	}
		    	ImGui::Indent();
		    	OptionCheckbox("超宽屏", config::SuperWidescreen,
		    			"当屏幕或窗口的高宽比大于16:9时, 使用完全宽度");
		    	ImGui::Unindent();
		    	if (!config::Widescreen)
		    	{
			        ImGui::PopItemFlag();
			        ImGui::PopStyleVar();
		    	}
		    	OptionCheckbox("宽屏游戏作弊", config::WidescreenGameHacks,
		    			"修改游戏使其以16:9变形格式显示, 并使用水平屏幕拉伸. 仅支持一些游戏");
#ifndef TARGET_IPHONE
		    	OptionCheckbox("垂直同步", config::VSync, "同步帧速率与屏幕刷新率. 推荐启用");
#endif
		    	OptionCheckbox("显示 FPS 计数器", config::ShowFPS, "屏幕上显示帧/秒计数器");
		    	OptionCheckbox("游戏中显示 VMU", config::FloatVMUs, "在游戏中显示VMU LCD屏幕");
		    	OptionCheckbox("旋转屏幕 90°", config::Rotate90, "逆时针旋转 90° ");
		    	OptionCheckbox("切换延迟帧", config::DelayFrameSwapping,
		    			"有助于避免屏幕闪烁或视频出现故障. 不建议在低配置平台上使用");
#if defined(USE_VULKAN) || defined(_WIN32)
		    	ImGui::Text("图形 API:");
#if defined(USE_VULKAN) && defined(_WIN32)
	            constexpr u32 columns = 3;
#else
	            constexpr u32 columns = 2;
#endif
	            ImGui::Columns(columns, "图形Api", false);
		    	ImGui::RadioButton("Open GL", &renderApi, 0);
            	ImGui::NextColumn();
#ifdef USE_VULKAN
		    	ImGui::RadioButton("Vulkan", &renderApi, 1);
            	ImGui::NextColumn();
#endif
#ifdef _WIN32
		    	ImGui::RadioButton("DirectX", &renderApi, 2);
            	ImGui::NextColumn();
#endif
		    	ImGui::Columns(1, NULL, false);
#endif

	            const std::array<float, 9> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f };
	            const std::array<std::string, 9> scalingsText{ "半数", "原生", "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5" };
	            std::array<int, scalings.size()> vres;
	            std::array<std::string, scalings.size()> resLabels;
	            u32 selected = 0;
	            for (u32 i = 0; i < scalings.size(); i++)
	            {
	            	vres[i] = scalings[i] * 480;
	            	if (vres[i] == config::RenderResolution)
	            		selected = i;
	            	if (!config::Widescreen)
	            		resLabels[i] = std::to_string((int)(scalings[i] * 640)) + "x" + std::to_string((int)(scalings[i] * 480));
	            	else
	            		resLabels[i] = std::to_string((int)(scalings[i] * 480 * 16 / 9)) + "x" + std::to_string((int)(scalings[i] * 480));
	            	resLabels[i] += " (" + scalingsText[i] + ")";
	            }

                ImGuiStyle& style = ImGui::GetStyle();
                float innerSpacing = style.ItemInnerSpacing.x;
                ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
                if (ImGui::BeginCombo("##分辨率", resLabels[selected].c_str(), ImGuiComboFlags_NoArrowButton))
                {
                	for (u32 i = 0; i < scalings.size(); i++)
                    {
                        bool is_selected = vres[i] == config::RenderResolution;
                        if (ImGui::Selectable(resLabels[i].c_str(), is_selected))
                        	config::RenderResolution = vres[i];
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::SameLine(0, innerSpacing);
                
                if (ImGui::ArrowButton("##降低分辨率", ImGuiDir_Left))
                {
                    if (selected > 0)
                    	config::RenderResolution = vres[selected - 1];
                }
                ImGui::SameLine(0, innerSpacing);
                if (ImGui::ArrowButton("##提高分辨率", ImGuiDir_Right))
                {
                    if (selected < vres.size() - 1)
                    	config::RenderResolution = vres[selected + 1];
                }
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                
                ImGui::Text("内部分辨率");
                ImGui::SameLine();
                ShowHelpMarker("内部渲染分辨率, 越高效果越好. 但对配置的要求也更高");

		    	OptionSlider("水平拉伸", config::ScreenStretching, 100, 150,
		    			"水平拉伸屏幕");
		    	OptionArrowButtons("跳帧", config::SkipFrame, 0, 6,
		    			"两个实际渲染帧之间跳过的帧数");
		    }
	    	ImGui::Spacing();
		    header("纹理渲染");
		    {
		    	OptionCheckbox("复制到 VRAM", config::RenderToTextureBuffer,
		    			"将渲染的纹理复制回 VRAM. 较慢但精确");
		    }
	    	ImGui::Spacing();
		    header("纹理提升");
		    {
#ifndef TARGET_NO_OPENMP
		    	OptionArrowButtons("纹理提升", config::TextureUpscale, 1, 8,
		    			"使用xBRZ算法放大纹理. 仅适用于高配置平台和某些2D游戏");
		    	OptionSlider("提升纹理最大值", config::MaxFilteredTextureSize, 8, 1024,
		    			"大于这个尺寸的纹理将不会被放大");
		    	OptionArrowButtons("最大线程数", config::MaxThreads, 1, 8,
		    			"用于提升纹理最大的线程数. 推荐值: 物理核心数减去1");
#endif
		    	OptionCheckbox("加载自定义纹理", config::CustomTextures,
		    			"从data/textures/<game id>加载自定义/高分辨率纹理");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();

		    switch (renderApi)
		    {
		    case 0:
		    	config::RendererType = perPixel ? RenderType::OpenGL_OIT : RenderType::OpenGL;
		    	break;
		    case 1:
		    	config::RendererType = perPixel ? RenderType::Vulkan_OIT : RenderType::Vulkan;
		    	break;
		    case 2:
		    	config::RendererType = RenderType::DirectX9;
		    	break;
		    }
		}
		if (ImGui::BeginTabItem("音频"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			OptionCheckbox("禁用音频", config::DisableSound, "禁用模拟器音频输出");
			OptionCheckbox("启用 DSP", config::DSPEnabled,
					"启用Dreamcast数字音频处理器. 仅推荐在高配置平台上使用");
			if (OptionSlider("音量级别", config::AudioVolume, 0, 100, "调整模拟器的音频级别"))
			{
				config::AudioVolume.calcDbPower();
			};
#ifdef __ANDROID__
			if (config::AudioBackend.get() == "自动" || config::AudioBackend.get() == "安卓")
				OptionCheckbox("Automatic Latency", config::AutoLatency,
						"自动设置音频延迟. 推荐");
#endif
            if (!config::AutoLatency
            		|| (config::AudioBackend.get() != "自动" && config::AudioBackend.get() != "安卓"))
            {
				int latency = (int)roundf(config::AudioBufferSize * 1000.f / 44100.f);
				ImGui::SliderInt("延迟", &latency, 12, 512, "%d ms");
				config::AudioBufferSize = (int)roundf(latency * 44100.f / 1000.f);
				ImGui::SameLine();
				ShowHelpMarker("设置高音频延迟. 不支持某些音频驱动程序");
            }

			audiobackend_t* backend = nullptr;
			std::string backend_name = config::AudioBackend;
			if (backend_name != "自动")
			{
				backend = GetAudioBackend(config::AudioBackend);
				if (backend != NULL)
					backend_name = backend->slug;
			}

			audiobackend_t* current_backend = backend;
			if (ImGui::BeginCombo("音频驱动", backend_name.c_str(), ImGuiComboFlags_None))
			{
				bool is_selected = (config::AudioBackend.get() == "自动");
				if (ImGui::Selectable("自动 - 自动驱动程序选择", &is_selected))
					config::AudioBackend.set("自动");

				for (u32 i = 0; i < GetAudioBackendCount(); i++)
				{
					backend = GetAudioBackend(i);
					is_selected = (config::AudioBackend.get() == backend->slug);

					if (is_selected)
						current_backend = backend;

					if (ImGui::Selectable((backend->slug + " - " + backend->name).c_str(), &is_selected))
						config::AudioBackend.set(backend->slug);
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ShowHelpMarker("要使用的音频驱动程序");

			if (current_backend != NULL && current_backend->get_options != NULL)
			{
				// get backend specific options
				int option_count;
				audio_option_t* options = current_backend->get_options(&option_count);

				for (int o = 0; o < option_count; o++)
				{
					std::string value = cfgLoadStr(current_backend->slug, options->cfg_name, "");

					if (options->type == integer)
					{
						int val = stoi(value);
						if (ImGui::SliderInt(options->caption.c_str(), &val, options->min_value, options->max_value))
						{
							std::string s = std::to_string(val);
							cfgSaveStr(current_backend->slug, options->cfg_name, s);
						}
					}
					else if (options->type == checkbox)
					{
						bool check = value == "1";
						if (ImGui::Checkbox(options->caption.c_str(), &check))
							cfgSaveStr(current_backend->slug, options->cfg_name,
									check ? "1" : "0");
					}
					else if (options->type == ::list)
					{
						if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
						{
							bool is_selected = false;
							for (const auto& cur : options->list_callback())
							{
								is_selected = value == cur;
								if (ImGui::Selectable(cur.c_str(), &is_selected))
									cfgSaveStr(current_backend->slug, options->cfg_name, cur);

								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					else {
						WARN_LOG(RENDERER, "未知选项");
					}

					options++;
				}
			}

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("高级"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    header("CPU 模式");
		    {
				ImGui::Columns(2, "cpu_模式", false);
				OptionRadioButton("动态重编译器", config::DynarecEnabled, true,
					"使用动态重新编译程序. 大多数情况下建议使用");
				ImGui::NextColumn();
				OptionRadioButton("解释器", config::DynarecEnabled, false,
					"使用解释器. 非常慢, 但在动态重编译器出现问题时可能会有帮助");
				ImGui::Columns(1, NULL, false);
		    }
		    if (config::DynarecEnabled)
		    {
		    	ImGui::Spacing();
		    	header("动态重编译器选项");
		    	OptionCheckbox("安全模式", config::DynarecSafeMode,
		    			"不优化整数算法. 不推荐");
		    	OptionCheckbox("闲置跳过", config::DynarecIdleSkip, "跳过等待循环. 推荐");
		    }
	    	ImGui::Spacing();
		    header("网络");
		    {
		    	OptionCheckbox("模拟宽带适配器", config::EmulateBBA,
		    			"模拟以太网宽带适配器（BBA）而不是调制解调器");
		    	OptionCheckbox("启用 Naomi 网络", config::NetworkEnable,
		    			"启用 Naomi 游戏网络支持");
		    	if (config::NetworkEnable)
		    	{
					OptionCheckbox("作为服务器", config::ActAsServer,
							"为 Naomi 网络游戏创建一个本地服务器");
					if (!config::ActAsServer)
					{
						char server_name[256];
						strcpy(server_name, config::NetworkServer.get().c_str());
						ImGui::InputText("服务器", server_name, sizeof(server_name), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
						ImGui::SameLine();
						ShowHelpMarker("要连接到的服务器. 留空会自动查找服务器");
						config::NetworkServer.set(server_name);
					}
		    	}
		    }
	    	ImGui::Spacing();
		    header("其它");
		    {
		    	OptionCheckbox("HLE BIOS", config::UseReios, "强制使用高级模拟 BIOS");
	            OptionCheckbox("强制使用 Windows CE", config::ForceWindowsCE,
	            		"Windows CE 启用完全 MMU 模拟和额外设置. 除非必要否则不要启用");
#ifndef __ANDROID
	            OptionCheckbox("串行控制台", config::SerialConsole,
	            		"将 Dreamcast 控制台转存为 stdout");
#endif
	            OptionCheckbox("转储纹理", config::DumpTextures,
	            		"将所有纹理转储到 data/texdump/<game id>");

	            bool logToFile = cfgLoadBool("日志", "日志文件", false);
	            bool newLogToFile = logToFile;
				ImGui::Checkbox("日志文件", &newLogToFile);
				if (logToFile != newLogToFile)
				{
					cfgSaveBool("日志", "日志文件", newLogToFile);
					LogManager::Shutdown();
					LogManager::Init();
				}
	            ImGui::SameLine();
	            ShowHelpMarker("将调试信息记录到 flycast.log");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("关于"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    header("Flycast汉化版 by PENG, 严禁将本模拟器挂在任何EMUELEC整合包内 ！！！ ");
		    {
				ImGui::Text("Flycast汉化版 BY PENG");
				ImGui::Text("严禁将本模拟器挂在任何EMUELEC整合包内 ！！！");
				ImGui::Text("构建日期: %s", BUILD_DATE);
		    }
	    	ImGui::Spacing();
		    header("平台");
		    {
		    	ImGui::Text("CPU: %s",
#if HOST_CPU == CPU_X86
					"x86"
#elif HOST_CPU == CPU_ARM
					"ARM"
#elif HOST_CPU == CPU_MIPS
					"MIPS"
#elif HOST_CPU == CPU_X64
					"x86/64"
#elif HOST_CPU == CPU_GENERIC
					"Generic"
#elif HOST_CPU == CPU_ARM64
					"ARM64"
#else
					"Unknown"
#endif
						);
		    	ImGui::Text("操作系统: %s",
#ifdef __ANDROID__
					"Android"
#elif defined(__unix__)
					"Linux"
#elif defined(__APPLE__)
#ifdef TARGET_IPHONE
		    		"iOS"
#else
					"macOS"
#endif
#elif defined(_WIN32)
					"Windows"
#elif defined(__SWITCH__)
					"Switch"
#else
					"未知的"
#endif
						);
#ifdef TARGET_IPHONE
				extern std::string iosJitStatus;
				ImGui::Text("JIT 状态: %s", iosJitStatus.c_str());
#endif
		    }
	    	ImGui::Spacing();
	    	if (config::RendererType.isOpenGL())
	    	{
				header("Open GL");
	    		ImGui::Text("渲染器: %s", (const char *)glGetString(GL_RENDERER));
	    		ImGui::Text("版本: %s", (const char *)glGetString(GL_VERSION));
	    	}
#ifdef USE_VULKAN
	    	else if (config::RendererType.isVulkan())
	    	{
				header("Vulkan");
				std::string name = VulkanContext::Instance()->GetDriverName();
				ImGui::Text("驱动程序名称: %s", name.c_str());
				std::string version = VulkanContext::Instance()->GetDriverVersion();
				ImGui::Text("版本: %s", version.c_str());
	    	}
#endif
#ifdef _WIN32
	    	else if (config::RendererType.isDirectX())
	    	{
				if (ImGui::CollapsingHeader("DirectX", ImGuiTreeNodeFlags_DefaultOpen))
				{
		    		std::string name = theDXContext.getDriverName();
		    		ImGui::Text("驱动程序名称: %s", name.c_str());
		    		std::string version = theDXContext.getDriverVersion();
		    		ImGui::Text("版本: %s", version.c_str());
				}
	    	}
#endif

#ifdef __ANDROID__
		    ImGui::Separator();
		    if (ImGui::Button("发送日志")) {
		    	void android_send_logs();
		    	android_send_logs();
		    }
#endif
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();

    scrollWhenDraggingOnVoid();
    windowDragScroll();
    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_display_notification(const char *msg, int duration)
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	osd_message = msg;
	osd_message_end = os_GetSeconds() + (double)duration / 1000.0;
}

static std::string get_notification()
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	if (!osd_message.empty() && os_GetSeconds() >= osd_message_end)
		osd_message.clear();
	return osd_message;
}

inline static void gui_display_demo()
{
	ImGui::ShowDemoWindow();
}

static void gui_display_content()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("##主要", NULL, ImGuiWindowFlags_NoDecoration);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 8 * scaling));		// from 8, 4
    ImGui::AlignTextToFramePadding();
    ImGui::Indent(10 * scaling);
    ImGui::Text("游戏");
    ImGui::Unindent(10 * scaling);

    static ImGuiTextFilter filter;
#if !defined(__ANDROID__) && !defined(TARGET_IPHONE)
	ImGui::SameLine(0, 32 * scaling);
	filter.Draw("筛选");
#endif
    if (gui_state != GuiState::SelectDisk)
    {
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("设置").x - ImGui::GetStyle().FramePadding.x * 2.0f);
		if (ImGui::Button("设置"))
			gui_state = GuiState::Settings;
    }
    ImGui::PopStyleVar();

    scanner.fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, 0), true);
    {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4

		ImGui::PushID("bios");
		if (ImGui::Selectable("Dreamcast BIOS | PENG 汉化 , 严禁将本模拟器挂在任何EMUELEC整合包内！！！"))
		{
			gui_state = GuiState::Closed;
			gui_start_game("");
		}
		ImGui::PopID();

		{
			scanner.get_mutex().lock();
			for (const auto& game : scanner.get_game_list())
			{
				if (gui_state == GuiState::SelectDisk)
				{
					std::string extension = get_file_extension(game.path);
					if (extension != "gdi" && extension != "chd"
							&& extension != "cdi" && extension != "cue")
						// Only dreamcast disks
						continue;
				}
				if (filter.PassFilter(game.name.c_str()))
				{
					ImGui::PushID(game.path.c_str());
					if (ImGui::Selectable(game.name.c_str()))
					{
						if (gui_state == GuiState::SelectDisk)
						{
							strcpy(settings.imgread.ImagePath, game.path.c_str());
							try {
								DiscSwap();
								gui_state = GuiState::Closed;
							} catch (const FlycastException& e) {
								error_msg = e.what();
							}
						}
						else
						{
							std::string gamePath(game.path);
							scanner.get_mutex().unlock();
							gui_state = GuiState::Closed;
							gui_start_game(gamePath);
							scanner.get_mutex().lock();
							ImGui::PopID();
							break;
						}
					}
					ImGui::PopID();
				}
			}
			scanner.get_mutex().unlock();
		}
        ImGui::PopStyleVar();
    }
    windowDragScroll();
	ImGui::EndChild();
	ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

	error_popup();
    contentpath_warning_popup();
}

static void systemdir_selected_callback(bool cancelled, std::string selection)
{
	if (!cancelled)
	{
		selection += "/";
		set_user_config_dir(selection);
		add_system_data_dir(selection);

		std::string data_path = selection + "data/";
		set_user_data_dir(data_path);
		if (!file_exists(data_path))
		{
			if (!make_directory(data_path))
			{
				WARN_LOG(BOOT, "无法创建 'data' 目录");
				set_user_data_dir(selection);
			}
		}

		if (cfgOpen())
		{
			config::Settings::instance().load(false);
			// Make sure the renderer type doesn't change mid-flight
			config::RendererType = RenderType::OpenGL;
			gui_state = GuiState::Main;
			if (config::ContentPath.get().empty())
			{
				scanner.stop();
				config::ContentPath.get().push_back(selection);
			}
			SaveSettings();
		}
	}
}

static void gui_display_onboarding()
{
	ImGui::OpenPopup("选择系统目录");
	select_file_popup("选择系统目录", &systemdir_selected_callback);
}

static std::future<bool> networkStatus;

static void start_network()
{
	networkStatus = naomiNetwork.startNetworkAsync();
	gui_state = GuiState::NetworkStart;
}

static void gui_network_start()
{
	centerNextWindow();
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

	ImGui::Begin("##网络", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
	ImGui::AlignTextToFramePadding();
	ImGui::SetCursorPosX(20.f * scaling);

	if (networkStatus.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		if (networkStatus.get())
		{
			gui_state = GuiState::Closed;
			ImGui::Text("启动...");
		}
		else
		{
			gui_state = GuiState::Main;
			settings.imgread.ImagePath[0] = '\0';
		}
	}
	else
	{
		ImGui::Text("启动网络...");
		if (config::ActAsServer)
			ImGui::Text("按Start开始游戏.");
	}
	ImGui::Text("%s", get_notification().c_str());

	float currentwidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
	ImGui::SetCursorPosY(126.f * scaling);
	if (ImGui::Button("取消", ImVec2(100.f * scaling, 0.f)))
	{
		naomiNetwork.terminate();
		networkStatus.get();
		gui_state = GuiState::Main;
		settings.imgread.ImagePath[0] = '\0';
	}
	ImGui::PopStyleVar();

	ImGui::End();

	if ((kcode[0] & DC_BTN_START) == 0)
		naomiNetwork.startNow();
}

static void gui_display_loadscreen()
{
	centerNextWindow();
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

    ImGui::Begin("##加载", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosX(20.f * scaling);
	if (dc_is_load_done())
	{
		try {
			dc_get_load_status();
			if (NaomiNetworkSupported())
			{
				start_network();
			}
			else
			{
				gui_state = GuiState::Closed;
				ImGui::Text("启动中...");
			}
		} catch (const FlycastException& ex) {
			ERROR_LOG(BOOT, "%s", ex.what());
			error_msg = ex.what();
#ifdef TEST_AUTOMATION
			die("游戏加载失败");
#endif
			gui_state = GuiState::Main;
			settings.imgread.ImagePath[0] = '\0';
		}
	}
	else
	{
		ImGui::Text("加载中... ");
		ImGui::SameLine();
		ImGui::Text("%s", get_notification().c_str());

		float currentwidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
		ImGui::SetCursorPosY(126.f * scaling);
		if (ImGui::Button("取消", ImVec2(100.f * scaling, 0.f)))
		{
			dc_cancel_load();
			gui_state = GuiState::Main;
		}
	}
	ImGui::PopStyleVar();

    ImGui::End();
}

void gui_display_ui()
{
	if (gui_state == GuiState::Closed || gui_state == GuiState::VJoyEdit)
		return;
	if (gui_state == GuiState::Main)
	{
		std::string game_file = settings.imgread.ImagePath;
		if (!game_file.empty())
		{
#ifndef __ANDROID__
			commandLineStart = true;
#endif
			gui_start_game(game_file);
			return;
		}
	}

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	switch (gui_state)
	{
	case GuiState::Settings:
		gui_display_settings();
		break;
	case GuiState::Commands:
		gui_display_commands();
		break;
	case GuiState::Main:
		//gui_display_demo();
		gui_display_content();
		break;
	case GuiState::Closed:
		break;
	case GuiState::Onboarding:
		gui_display_onboarding();
		break;
	case GuiState::VJoyEdit:
		break;
	case GuiState::VJoyEditCommands:
#ifdef __ANDROID__
		gui_display_vjoy_commands(scaling);
#endif
		break;
	case GuiState::SelectDisk:
		gui_display_content();
		break;
	case GuiState::Loading:
		gui_display_loadscreen();
		break;
	case GuiState::NetworkStart:
		gui_network_start();
		break;
	case GuiState::Cheats:
		gui_cheats();
		break;
	default:
		die("未知的用户界面状态");
		break;
	}
    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData());

	if (gui_state == GuiState::Closed)
		dc_resume();
}

static float LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static std::string getFPSNotification()
{
	if (config::ShowFPS)
	{
		double now = os_GetSeconds();
		if (now - LastFPSTime >= 1.0) {
			fps = (MainFrameCount - lastFrameCount) / (now - LastFPSTime);
			LastFPSTime = now;
			lastFrameCount = MainFrameCount;
		}
		if (fps >= 0.f && fps < 9999.f) {
			char text[32];
			snprintf(text, sizeof(text), "F:%.1f%s", fps, settings.input.fastForwardMode ? " >>" : "");

			return std::string(text);
		}
	}
	return std::string(settings.input.fastForwardMode ? ">>" : "");
}

void gui_display_osd()
{
	if (gui_state == GuiState::VJoyEdit)
		return;
	std::string message = get_notification();
	if (message.empty())
		message = getFPSNotification();

	if (!message.empty() || config::FloatVMUs || crosshairsNeeded())
	{
		ImGui_Impl_NewFrame();
		ImGui::NewFrame();

		if (!message.empty())
		{
			ImGui::SetNextWindowBgAlpha(0);
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 0));

			ImGui::Begin("##osd", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
					| ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
			ImGui::SetWindowFontScale(1.5);
			ImGui::TextColored(ImVec4(1, 1, 0, 0.7), "%s", message.c_str());
			ImGui::End();
		}
		displayCrosshairs();
		if (config::FloatVMUs)
			display_vmus();
//		gui_plot_render_time(screen_width, screen_height);

		ImGui::Render();
		ImGui_impl_RenderDrawData(ImGui::GetDrawData());
	}
}

void gui_open_onboarding()
{
	gui_state = GuiState::Onboarding;
}

void gui_term()
{
	if (inited)
	{
		inited = false;
		term_vmus();
		if (config::RendererType.isOpenGL())
			ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
	    EventManager::unlisten(Event::Resume, emuEventCallback);
	}
}

int msgboxf(const char* text, unsigned int type, ...) {
    va_list args;

    char temp[2048];
    va_start(args, type);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    ERROR_LOG(COMMON, "%s", temp);

    gui_display_notification(temp, 2000);

    return 1;
}

extern bool subfolders_read;

void gui_refresh_files()
{
	scanner.refresh();
	subfolders_read = false;
}

#define VMU_WIDTH (70 * 48 * scaling / 32)
#define VMU_HEIGHT (70 * scaling)
#define VMU_PADDING (8 * scaling)
static ImTextureID vmu_lcd_tex_ids[8];

static ImTextureID crosshairTexId;

static const int vmu_coords[8][2] = {
		{ 0 , 0 },
		{ 0 , 0 },
		{ 1 , 0 },
		{ 1 , 0 },
		{ 0 , 1 },
		{ 0 , 1 },
		{ 1 , 1 },
		{ 1 , 1 },
};

static void display_vmus()
{
	if (!game_started)
		return;
	if (!config::RendererType.isOpenGL())
		return;
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("vmu-window", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
    		| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		if (vmu_lcd_tex_ids[i] != (ImTextureID)0)
			ImGui_ImplOpenGL3_DeleteTexture(vmu_lcd_tex_ids[i]);
		vmu_lcd_tex_ids[i] = ImGui_ImplOpenGL3_CreateVmuTexture(vmu_lcd_data[i]);

	    int x = vmu_coords[i][0];
	    int y = vmu_coords[i][1];
	    ImVec2 pos;
	    if (x == 0)
	    	pos.x = VMU_PADDING;
	    else
	    	pos.x = ImGui::GetIO().DisplaySize.x - VMU_WIDTH - VMU_PADDING;
	    if (y == 0)
	    {
	    	pos.y = VMU_PADDING;
	    	if (i & 1)
	    		pos.y += VMU_HEIGHT + VMU_PADDING;
	    }
	    else
	    {
	    	pos.y = ImGui::GetIO().DisplaySize.y - VMU_HEIGHT - VMU_PADDING;
	    	if (i & 1)
	    		pos.y -= VMU_HEIGHT + VMU_PADDING;
	    }
	    ImVec2 pos_b(pos.x + VMU_WIDTH, pos.y + VMU_HEIGHT);
		ImGui::GetWindowDrawList()->AddImage(vmu_lcd_tex_ids[i], pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0xC0ffffff);
	}
    ImGui::End();
}

std::pair<float, float> getCrosshairPosition(int playerNum)
{
	float fx = mo_x_abs[playerNum];
	float fy = mo_y_abs[playerNum];
	float width = 640.f;
	float height = 480.f;

	if (config::Rotate90)
	{
		float t = fy;
		fy = 639.f - fx;
		fx = t;
		std::swap(width, height);
	}
	float scale = height / screen_height;
	fy /= scale;
	scale /= config::ScreenStretching / 100.f;
	fx = fx / scale + (screen_width - width / scale) / 2.f;

	return std::make_pair(fx, fy);
}

static void displayCrosshairs()
{
	if (!game_started)
		return;
	if (!config::RendererType.isOpenGL())
		return;
	if (!crosshairsNeeded())
		return;

	if (crosshairTexId == ImTextureID())
		crosshairTexId = ImGui_ImplOpenGL3_CreateCrosshairTexture(getCrosshairTextureData());
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("xhair-window", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
    		| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	for (u32 i = 0; i < config::CrosshairColor.size(); i++)
	{
		if (config::CrosshairColor[i] == 0)
			continue;
		if (settings.platform.system == DC_PLATFORM_DREAMCAST && config::MapleMainDevices[i] != MDT_LightGun)
			continue;

		ImVec2 pos;
		std::tie(pos.x, pos.y) = getCrosshairPosition(i);
		pos.x -= (XHAIR_WIDTH * scaling) / 2.f;
		pos.y += (XHAIR_WIDTH * scaling) / 2.f;
		ImVec2 pos_b(pos.x + XHAIR_WIDTH * scaling, pos.y - XHAIR_HEIGHT * scaling);

		ImGui::GetWindowDrawList()->AddImage(crosshairTexId, pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), config::CrosshairColor[i]);
	}
	ImGui::End();
}

static void reset_vmus()
{
	for (u32 i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
		vmu_lcd_status[i] = false;
}

static void term_vmus()
{
	if (!config::RendererType.isOpenGL())
		return;
	for (u32 i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
	{
		if (vmu_lcd_tex_ids[i] != ImTextureID())
		{
			ImGui_ImplOpenGL3_DeleteTexture(vmu_lcd_tex_ids[i]);
			vmu_lcd_tex_ids[i] = ImTextureID();
		}
	}
	if (crosshairTexId != ImTextureID())
	{
		ImGui_ImplOpenGL3_DeleteTexture(crosshairTexId);
		crosshairTexId = ImTextureID();
	}
}
