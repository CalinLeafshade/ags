//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

//
// Graphics initialization
//

#include "main/mainheader.h"
#include "gfx/ali3d.h"
#include "ac/common.h"
#include "ac/display.h"
#include "ac/draw.h"
#include "ac/gamesetup.h"
#include "ac/gamesetupstruct.h"
#include "ac/walkbehind.h"
#include "debug/debug_log.h"
#include "debug/debugger.h"
#include "debug/out.h"
#include "font/fonts.h"
#include "gui/guiinv.h"
#include "gui/guimain.h"
#include "main/graphics_mode.h"
#include "main/main_allegro.h"
#include "platform/base/agsplatformdriver.h"
#include "gfx/graphicsdriver.h"
#include "gfx/bitmap.h"
#include "util/math.h"
#include "util/string.h"

using AGS::Common::Bitmap;
using AGS::Common::String;
namespace BitmapHelper = AGS::Common::BitmapHelper;
namespace Math = AGS::Common::Math;
namespace Out = AGS::Common::Out;

extern GameSetup usetup;
extern GameSetupStruct game;
extern int proper_exit;
extern GUIMain*guis;
extern int psp_gfx_renderer; // defined in ali3dogl
extern WalkBehindMethodEnum walkBehindMethod;
extern DynamicArray<GUIInv> guiinv;
extern int numguiinv;
extern int current_screen_resolution_multiplier;
extern char force_gfxfilter[50];
extern AGSPlatformDriver *platform;
extern int force_16bit;
extern IGraphicsDriver *gfxDriver;
extern volatile int timerloop;
extern IDriverDependantBitmap *blankImage;
extern IDriverDependantBitmap *blankSidebarImage;
extern Bitmap *_old_screen;
extern int _places_r, _places_g, _places_b;

struct ColorDepthOption
{
    int32_t First;
    int32_t Second;

    ColorDepthOption()
        : First(0)
        , Second(0)
    {
    }

    ColorDepthOption(int32_t first, int32_t second)
        : First(first)
        , Second(second)
    {
    }
};

Size              GameSize;
DisplayResolution GameResolution;

const int MaxSidebordersWidth = 110;
int debug_15bit_mode = 0, debug_24bit_mode = 0;
int convert_16bit_bgr = 0;

int adjust_pixel_size_for_loaded_data(int size, int filever)
{
    if (filever < kGameVersion_310)
    {
        return multiply_up_coordinate(size);
    }
    return size;
}

void adjust_pixel_sizes_for_loaded_data(int *x, int *y, int filever)
{
    x[0] = adjust_pixel_size_for_loaded_data(x[0], filever);
    y[0] = adjust_pixel_size_for_loaded_data(y[0], filever);
}

void adjust_sizes_for_resolution(int filever)
{
    int ee;
    for (ee = 0; ee < game.numcursors; ee++) 
    {
        game.mcurs[ee].hotx = adjust_pixel_size_for_loaded_data(game.mcurs[ee].hotx, filever);
        game.mcurs[ee].hoty = adjust_pixel_size_for_loaded_data(game.mcurs[ee].hoty, filever);
    }

    for (ee = 0; ee < game.numinvitems; ee++) 
    {
        adjust_pixel_sizes_for_loaded_data(&game.invinfo[ee].hotx, &game.invinfo[ee].hoty, filever);
    }

    for (ee = 0; ee < game.numgui; ee++) 
    {
        GUIMain*cgp=&guis[ee];
        adjust_pixel_sizes_for_loaded_data(&cgp->x, &cgp->y, filever);
        if (cgp->wid < 1)
            cgp->wid = 1;
        if (cgp->hit < 1)
            cgp->hit = 1;
        // Temp fix for older games
        if (cgp->wid == usetup.base_width - 1)
            cgp->wid = usetup.base_width;

        adjust_pixel_sizes_for_loaded_data(&cgp->wid, &cgp->hit, filever);

        cgp->popupyp = adjust_pixel_size_for_loaded_data(cgp->popupyp, filever);

        for (int i = 0; i < cgp->numobjs; ++i) 
        {
            adjust_pixel_sizes_for_loaded_data(&cgp->objs[i]->x, &cgp->objs[i]->y, filever);
            adjust_pixel_sizes_for_loaded_data(&cgp->objs[i]->wid, &cgp->objs[i]->hit, filever);
            cgp->objs[i]->activated=0;
        }
    }

    if ((filever >= 37) && (game.options[OPT_NATIVECOORDINATES] == 0) &&
        (game.IsHiRes()))
    {
        // New 3.1 format game file, but with Use Native Coordinates off

        for (ee = 0; ee < game.numcharacters; ee++) 
        {
            game.chars[ee].x /= 2;
            game.chars[ee].y /= 2;
        }

        for (ee = 0; ee < numguiinv; ee++)
        {
            guiinv[ee].itemWidth /= 2;
            guiinv[ee].itemHeight /= 2;
        }
    }

}

int engine_init_gfx_filters(int color_depth);
bool find_nearest_supported_mode(Size &wanted_size, const int color_depth, const Size *ratio_reference = NULL, bool ignore_given_size = false);

bool get_desktop_size_for_windowed_mode(Size &size)
{
    if (get_desktop_resolution(&size.Width, &size.Height) == 0)
    {
        // TODO: a platform-specific way to do this?
        size.Height -= 32; // give some space for window borders
        return true;
    }
    return false;
}

void setup_render_frame(Size &screen_size, Placement &drawing_place)
{
    Size filtered_game_size = GameSize;
    gfxFilter->GetRealResolution(&filtered_game_size.Width, &filtered_game_size.Height);

    if (usetup.drawing_place == kRenderPlaceResizeWindow)
    {
        screen_size = filtered_game_size;
        // We are not allowed to stretch more than user requested (by setting gfx filter)
        drawing_place = kPlaceCenter;
    }
    else
    {
        screen_size = usetup.screen_size;
        // If the configuration did not define proper screen size, use the scaled game size instead
        if (screen_size.Width <= 0)
        {
            screen_size.Width = filtered_game_size.Width;
        }
        if (screen_size.Height <= 0)
        {
            screen_size.Height = filtered_game_size.Height;
        }

        switch (usetup.drawing_place)
        {
        case kRenderPlaceCenter:
            drawing_place = kPlaceCenter;
            break;
        case kRenderPlaceStretchProportional:
            drawing_place = kPlaceStretchProportional;
            break;
        default:
            drawing_place = kPlaceStretch;
            break;
        }
    }
}

void apply_window_aspect_ratio(Size &screen_size, int color_depth)
{
    // Apply extra horizontal and/or vertical borders, referring to the current
    // desktop resolution ratio.
    if (!usetup.windowed && usetup.match_desktop_ratio)
    {
        Size desktop_size;
        if (get_desktop_resolution(&desktop_size.Width, &desktop_size.Height) == 0)
        {
            Size fixed_screen_size = screen_size;
            if (find_nearest_supported_mode(fixed_screen_size, color_depth, &desktop_size))
            {
                screen_size = fixed_screen_size;
            }
        }
        else
        {
            Out::FPrint("Automatic borders disabled (unable to obtain desktop resolution)");
        }
    }
}

int engine_init_screen_settings(Size &screen_size, Placement &drawing_place, ColorDepthOption &color_depths)
{
    Out::FPrint("Initializing screen settings");

    // default shifts for how we store the sprite data

#if defined(PSP_VERSION)
    // PSP: Switch b<>r for 15/16 bit.
    _rgb_r_shift_32 = 16;
    _rgb_g_shift_32 = 8;
    _rgb_b_shift_32 = 0;
    _rgb_b_shift_16 = 11;
    _rgb_g_shift_16 = 5;
    _rgb_r_shift_16 = 0;
    _rgb_b_shift_15 = 10;
    _rgb_g_shift_15 = 5;
    _rgb_r_shift_15 = 0;
#else
    _rgb_r_shift_32 = 16;
    _rgb_g_shift_32 = 8;
    _rgb_b_shift_32 = 0;
    _rgb_r_shift_16 = 11;
    _rgb_g_shift_16 = 5;
    _rgb_b_shift_16 = 0;
    _rgb_r_shift_15 = 10;
    _rgb_g_shift_15 = 5;
    _rgb_b_shift_15 = 0;
#endif

    usetup.base_width = 320;
    usetup.base_height = 200;

    GameResolutionType game_res = game.GetDefaultResolution();
    switch (game_res)
    {
    case kGameResolution_800x600:
    case kGameResolution_1024x768:
        if (game_res >= kGameResolution_1024x768)
        {
            // 1024x768
            usetup.base_width = 512;
            usetup.base_height = 384;
        }
        else
        {
            // 800x600
            usetup.base_width = 400;
            usetup.base_height = 300;
        }
        GameSize.Width = usetup.base_width * 2;
        GameSize.Height = usetup.base_height * 2;
        wtext_multiply = 2;
        break;
    case kGameResolution_640x480:
        GameSize.Width = 640;
        GameSize.Height = 480;
        wtext_multiply = 2;
        break;
    case kGameResolution_640x400:
        GameSize.Width = 640;
        GameSize.Height = 400;
        wtext_multiply = 2;
        break;
    case kGameResolution_320x240:
        GameSize.Width = 320;
        GameSize.Height = 240;
        wtext_multiply = 1;
        break;
    case kGameResolution_320x200:
        GameSize.Width = 320;
        GameSize.Height = 200;
        wtext_multiply = 1;
        break;
    default:
        GameSize.Width = usetup.base_width;
        GameSize.Height = usetup.base_height;
        wtext_multiply = 1;
    }

    // GameResolution Width and Height is now always the same as GameSize;
    // the borders are handled exclusively by graphics driver.
    GameResolution.Width = GameSize.Width;
    GameResolution.Height = GameSize.Height;

    usetup.textheight = wgetfontheight(0) + 1;
    current_screen_resolution_multiplier = GameSize.Width / usetup.base_width;

    if ((game.IsHiRes()) &&
        (game.options[OPT_NATIVECOORDINATES]))
    {
        usetup.base_width *= 2;
        usetup.base_height *= 2;
    }

    // don't allow them to force a 256-col game to hi-color
    if (game.color_depth < 2)
    {
        usetup.force_hicolor_mode = 0;
    }

    color_depths.First = 8;
    color_depths.Second = 8;
    if (debug_15bit_mode)
    {
        color_depths.First = 15;
        color_depths.Second = 15;
    }
    else if (debug_24bit_mode)
    {
        color_depths.First = 24;
        color_depths.Second = 24;
    }
    else if ((game.color_depth == 2) || (force_16bit) || (usetup.force_hicolor_mode))
    {
        color_depths.First = 16;
        color_depths.Second = 15;
    }
    else if (game.color_depth > 2)
    {
        color_depths.First = 32;
        color_depths.Second = 24;
    }

    int res = engine_init_gfx_filters(color_depths.First);
    if (res != RETURN_CONTINUE) {
        return res;
    }

    setup_render_frame(screen_size, drawing_place);
    apply_window_aspect_ratio(screen_size, color_depths.First);
    adjust_sizes_for_resolution(loaded_game_file_version);
    return RETURN_CONTINUE;
}

int initialize_graphics_filter(const char *filterID, const int colDepth)
{
    int idx = 0;
    GFXFilter **filterList;

    if (usetup.gfxDriverID.CompareNoCase("D3D9") == 0)
    {
        filterList = get_d3d_gfx_filter_list(false);
    }
    else
    {
        filterList = get_allegro_gfx_filter_list(false);
    }

    // by default, select No Filter
    gfxFilter = filterList[0];

    GFXFilter *thisFilter = filterList[idx];
    while (thisFilter != NULL) {

        if ((filterID != NULL) &&
            (strcmp(thisFilter->GetFilterID(), filterID) == 0))
            gfxFilter = thisFilter;
        else if (idx > 0)
            delete thisFilter;

        idx++;
        thisFilter = filterList[idx];
    }

    Out::FPrint("Applying scaling filter: %s", gfxFilter->GetFilterID());

    const char *filterError = gfxFilter->Initialize(GameSize.Width, GameSize.Height, colDepth);
    if (filterError != NULL) {
        proper_exit = 1;
        platform->DisplayAlert("Unable to initialize the graphics filter. It returned the following error:\n'%s'\n\nTry running Setup and selecting a different graphics filter.", filterError);
        return -1;
    }

    gfxDriver->SetGraphicsFilter(gfxFilter);
    return 0;
}

void pre_create_gfx_driver(const String &gfx_driver_id)
{
#ifdef WINDOWS_VERSION
    if (gfx_driver_id.CompareNoCase("D3D9") == 0 && (game.color_depth != 1))
    {
        gfxDriver = GetD3DGraphicsDriver(NULL);
        if (!gfxDriver)
        {
            Out::FPrint("Failed to initialize D3D9 driver: %s", get_allegro_error());
        }
    }
    else
#endif
#if defined (IOS_VERSION) || defined(ANDROID_VERSION) || defined(WINDOWS_VERSION)
    if (gfx_driver_id.CompareNoCase("DX5") != 0 && (psp_gfx_renderer > 0) && (game.color_depth != 1))
    {
        gfxDriver = GetOGLGraphicsDriver(NULL);
        if (!gfxDriver)
        {
            Out::FPrint("Failed to initialize OGL driver: %s", get_allegro_error());
        }
    }
#endif

    if (!gfxDriver)
    {
        gfxDriver = GetSoftwareGraphicsDriver(NULL);
    }

    Out::FPrint("Created graphics driver: %s", gfxDriver->GetDriverName());
}

int find_max_supported_uniform_multiplier(const Size &base_size, const int color_depth, int width_range_allowed)
{
    IGfxModeList *modes = gfxDriver->GetSupportedModeList(color_depth);
    if (!modes)
    {
        Out::FPrint("Couldn't get a list of supported resolutions");
        return 0;
    }

    int least_supported_multiplier = 0;
    int mode_count = modes->GetModeCount();
    DisplayResolution mode;
    for (int i = 0; i < mode_count; ++i)
    {
        if (!modes->GetMode(i, mode))
        {
            continue;
        }
        if (mode.ColorDepth != color_depth)
        {
            continue;
        }

        if (mode.Width > base_size.Width &&
            mode.Height > base_size.Height && mode.Height % base_size.Height == 0)
        {
            int multiplier_x = mode.Width / base_size.Width;
            int remainder_x = mode.Width % base_size.Width;
            int multiplier_y = mode.Height / base_size.Height;
            if (multiplier_x == multiplier_y && (remainder_x / multiplier_x <= width_range_allowed) &&
                multiplier_x > least_supported_multiplier)
            {
                least_supported_multiplier = multiplier_x;
            }
        }
    }

    delete modes;

    if (least_supported_multiplier == 0)
    {
        Out::FPrint("Couldn't find acceptable supported resolution");
    }
    return least_supported_multiplier;
}

String get_maximal_supported_scaling_filter(int color_depth)
{
    Out::FPrint("Detecting maximal supported scaling");
    String gfxfilter = "None";

    const int max_scaling = 8; // we support up to x8 scaling now
    // fullscreen mode
    if (usetup.windowed == 0)
    {
        int selected_scaling = find_max_supported_uniform_multiplier(GameSize, color_depth, MaxSidebordersWidth);
        if (selected_scaling > 1)
        {
            selected_scaling = Math::Min(selected_scaling, max_scaling);
            gfxfilter.Format("StdScale%d", selected_scaling);
        }
    }
    // windowed mode
    else
    {
        // Do not try to create windowed mode larger than current desktop resolution
        Size desktop_size;
        if (get_desktop_size_for_windowed_mode(desktop_size))
        {
            int xratio = desktop_size.Width / GameSize.Width;
            int yratio = desktop_size.Height / GameSize.Height;
            int selected_scaling = Math::Min(Math::Min(xratio, yratio), max_scaling);
            gfxfilter.Format("StdScale%d", selected_scaling);
        }
        else
        {
            Out::FPrint("Automatic scaling failed (unable to obtain desktop resolution)");
        }
    }
    return gfxfilter;
}

int engine_init_gfx_filters(int color_depth)
{
    Out::FPrint("Init gfx filters");

    String gfxfilter;

    if (force_gfxfilter[0]) {
        gfxfilter = force_gfxfilter;
    }
    else if (!usetup.gfxFilterID.IsEmpty() && stricmp(usetup.gfxFilterID, "max") != 0) {
        gfxfilter = usetup.gfxFilterID;
    }
#if defined (WINDOWS_VERSION) || defined (LINUX_VERSION)
    else
    {
        gfxfilter = get_maximal_supported_scaling_filter(color_depth);
    }
#endif

    if (initialize_graphics_filter(gfxfilter, color_depth))
    {
        return EXIT_NORMAL;
    }
    return RETURN_CONTINUE;
}

void create_gfx_driver(const String &gfx_driver_id)
{
    Out::FPrint("Init gfx driver");
    pre_create_gfx_driver(gfx_driver_id);
    usetup.gfxDriverID = gfxDriver->GetDriverID();

    gfxDriver->SetCallbackOnInit(GfxDriverOnInitCallback);
    gfxDriver->SetTintMethod(TintReColourise);
}

bool init_gfx_mode(const Size screen_size, const Placement drawing_place, const int color_depth)
{
    Out::FPrint("Trying to set gfx mode to %d x %d (%d-bit) %s",
        screen_size.Width, screen_size.Height, color_depth, usetup.windowed > 0 ? "windowed" : "fullscreen");

    GameResolution.ColorDepth = color_depth;

    if (usetup.refresh >= 50)
    {
        request_refresh_rate(usetup.refresh);
    }

    if (game.color_depth == 1)
    {
        GameResolution.ColorDepth = 8;
    }
    else
    {
        set_color_depth(GameResolution.ColorDepth);
    }

    // Last moment fixups
    Placement using_placement = drawing_place;
    // If the filtered game size appear larger than the window,
    // do not apply a "centered" style, use "proportional stretch" instead
    if (using_placement == kPlaceCenter)
    {
        Size filtered_game_size = GameSize;
        gfxFilter->GetRealResolution(&filtered_game_size.Width, &filtered_game_size.Height);
        if (filtered_game_size.ExceedsByAny(screen_size))
        {
            using_placement = kPlaceStretchProportional;
        }
    }

    bool success =
        gfxDriver->Init(GameSize.Width, GameSize.Height, screen_size.Width, screen_size.Height, using_placement,
                         GameResolution.ColorDepth, usetup.windowed > 0, &timerloop);
    if (success)
    {
        Out::FPrint("Succeeded. Using gfx mode %d x %d (%d-bit) %s",
            screen_size.Width, screen_size.Height, GameResolution.ColorDepth, usetup.windowed > 0 ? "windowed" : "fullscreen");
    }
    else
    {
        Out::FPrint("Failed. %s", get_allegro_error());
    }
    return success;
}

bool find_nearest_supported_mode(Size &wanted_size, const int color_depth, const Size *ratio_reference, bool ignore_given_size)
{
    IGfxModeList *modes = gfxDriver->GetSupportedModeList(color_depth);
    if (!modes)
    {
        Out::FPrint("Couldn't get a list of supported resolutions");
        return false;
    }

    int wanted_ratio = 0;
    if (ratio_reference)
    {
        wanted_ratio = (ratio_reference->Height << 10) / ratio_reference->Width;
    }
    
    int nearest_width = 0;
    int nearest_height = 0;
    int nearest_width_diff = 0;
    int nearest_height_diff = 0;
    int mode_count = modes->GetModeCount();
    DisplayResolution mode;
    for (int i = 0; i < mode_count; ++i)
    {
        if (!modes->GetMode(i, mode))
        {
            continue;
        }
        if (mode.ColorDepth != color_depth)
        {
            continue;
        }

        if (wanted_ratio > 0)
        {
            int mode_ratio = (mode.Height << 10) / mode.Width;
            if (mode_ratio != wanted_ratio)
            {
                continue;
            }
        }
        if (!ignore_given_size && mode.Width == wanted_size.Width && mode.Height == wanted_size.Height)
        {
            return true;
        }
      
        int diff_w = abs(wanted_size.Width - mode.Width);
        int diff_h = abs(wanted_size.Height - mode.Height);
        bool same_diff_w_higher = (diff_w == nearest_width_diff && nearest_width < wanted_size.Width);
        bool same_diff_h_higher = (diff_h == nearest_height_diff && nearest_height < wanted_size.Height);

        if (nearest_width == 0 ||
            (diff_w < nearest_width_diff || same_diff_w_higher) && diff_h <= nearest_height_diff ||
            (diff_h < nearest_height_diff || same_diff_h_higher) && diff_w <= nearest_width_diff)
        {
            nearest_width = mode.Width;
            nearest_width_diff = diff_w;
            nearest_height = mode.Height;
            nearest_height_diff = diff_h;
        }
    }

    delete modes;
    if (nearest_width > 0 && nearest_height > 0)
    {
        wanted_size.Width = nearest_width;
        wanted_size.Height = nearest_height;
        return true;
    }
    Out::FPrint("Couldn't find acceptable supported resolution");
    return false;
}

bool try_init_gfx_mode(const Size screen_size, const Placement drawing_place, const int color_depth)
{
    bool success = init_gfx_mode(screen_size, drawing_place, color_depth);
    if (!success)
    {
        // Try to find nearest compatible mode and init that
        Out::FPrint("Attempting to find nearest supported resolution");
        Size desktop_size;
        Size fixed_screen_size = screen_size;
        bool mode_found = false;

        // Fullscreen mode
        if (usetup.windowed == 0)
        {
            if (usetup.match_desktop_ratio &&
                get_desktop_resolution(&desktop_size.Width, &desktop_size.Height))
            {
                mode_found = find_nearest_supported_mode(fixed_screen_size, color_depth, &desktop_size, true);
            }
            else
            {
                mode_found = find_nearest_supported_mode(fixed_screen_size, color_depth, NULL, true);
            }
        }
        // Windowed mode
        else
        {
            // If windowed mode, make the resolution stay in the generally supported limits
            // TODO: platform/driver specific values?
            const Size minimal_size(128, 128);
            if (!get_desktop_size_for_windowed_mode(desktop_size))
            {
                desktop_size = screen_size;
            }
            if (screen_size.ExceedsByAny(desktop_size) || minimal_size.ExceedsByAny(screen_size))
            {
                fixed_screen_size.Clamp(minimal_size, desktop_size);
                mode_found = true;
            }
        }

        if (mode_found)
        {
            success = init_gfx_mode(fixed_screen_size, drawing_place, color_depth);
        }
    }
    return success;
}

bool switch_to_graphics_mode(const Size init_screen_size, const Placement drawing_place, const ColorDepthOption color_depths) 
{
    bool success = try_init_gfx_mode(init_screen_size, drawing_place, color_depths.First);
    if (!success && color_depths.Second != color_depths.First)
        success = try_init_gfx_mode(init_screen_size, drawing_place, color_depths.Second);
    return success;
}

int engine_init_graphics_mode(const Size screen_size, const Placement drawing_place, const ColorDepthOption color_depths)
{
    Out::FPrint("Switching to graphics mode");

    if (!switch_to_graphics_mode(screen_size, drawing_place, color_depths))
    {
        return EXIT_NORMAL;
    }
    return RETURN_CONTINUE;
}

void CreateBlankImage()
{
    // this is the first time that we try to use the graphics driver,
    // so it's the most likey place for a crash
    try
    {
        Bitmap *blank = BitmapHelper::CreateBitmap(16, 16, GameResolution.ColorDepth);
        blank = gfxDriver->ConvertBitmapToSupportedColourDepth(blank);
        blank->Clear();
        blankImage = gfxDriver->CreateDDBFromBitmap(blank, false, true);
        blankSidebarImage = gfxDriver->CreateDDBFromBitmap(blank, false, true);
        delete blank;
    }
    catch (Ali3DException gfxException)
    {
        quit((char*)gfxException._message);
    }

}

void engine_post_init_gfx_driver()
{
    //screen = _filter->ScreenInitialized(screen, GameResolution.Width, GameResolution.Height);
	_old_screen = BitmapHelper::GetScreenBitmap();

    if (gfxDriver->HasAcceleratedStretchAndFlip()) 
    {
        walkBehindMethod = DrawAsSeparateSprite;

        CreateBlankImage();
    }
}

void engine_prepare_screen()
{
    DisplayResolution disp_res = gfxDriver->GetResolution();
    Out::FPrint("Preparing graphics mode screen");
    Out::FPrint("Screen resolution: %d x %d; game resolution %d x %d", disp_res.Width, disp_res.Height, GameSize.Width, GameSize.Height);

    // Most cards do 5-6-5 RGB, which is the format the files are saved in
    // Some do 5-6-5 BGR, or  6-5-5 RGB, in which case convert the gfx
    if ((GameResolution.ColorDepth == 16) && ((_rgb_b_shift_16 != 0) || (_rgb_r_shift_16 != 11))) {
        convert_16bit_bgr = 1;
        if (_rgb_r_shift_16 == 10) {
            // some very old graphics cards lie about being 16-bit when they
            // are in fact 15-bit ... get around this
            _places_r = 3;
            _places_g = 3;
        }
    }
    if (GameResolution.ColorDepth > 16) {
        // when we're using 32-bit colour, it converts hi-color images
        // the wrong way round - so fix that

#if defined(IOS_VERSION) || defined(ANDROID_VERSION) || defined(PSP_VERSION)
        _rgb_b_shift_16 = 0;
        _rgb_g_shift_16 = 5;
        _rgb_r_shift_16 = 11;

        _rgb_b_shift_15 = 0;
        _rgb_g_shift_15 = 5;
        _rgb_r_shift_15 = 10;

        _rgb_r_shift_32 = 0;
        _rgb_g_shift_32 = 8;
        _rgb_b_shift_32 = 16;
#else
        _rgb_r_shift_16 = 11;
        _rgb_g_shift_16 = 5;
        _rgb_b_shift_16 = 0;
#endif
    }
    else if (GameResolution.ColorDepth == 16) {
        // ensure that any 32-bit graphics displayed are converted
        // properly to the current depth
#if defined(PSP_VERSION)
        _rgb_r_shift_32 = 0;
        _rgb_g_shift_32 = 8;
        _rgb_b_shift_32 = 16;

        _rgb_b_shift_15 = 0;
        _rgb_g_shift_15 = 5;
        _rgb_r_shift_15 = 10;
#else
        _rgb_r_shift_32 = 16;
        _rgb_g_shift_32 = 8;
        _rgb_b_shift_32 = 0;
#endif
    }
    else if (GameResolution.ColorDepth < 16) {
        // ensure that any 32-bit graphics displayed are converted
        // properly to the current depth
#if defined (WINDOWS_VERSION)
        _rgb_r_shift_32 = 16;
        _rgb_g_shift_32 = 8;
        _rgb_b_shift_32 = 0;
#else
        _rgb_r_shift_32 = 0;
        _rgb_g_shift_32 = 8;
        _rgb_b_shift_32 = 16;

        _rgb_b_shift_15 = 0;
        _rgb_g_shift_15 = 5;
        _rgb_r_shift_15 = 10;
#endif
    }
}

void engine_set_gfx_driver_callbacks()
{
    gfxDriver->SetCallbackForPolling(update_polled_stuff_if_runtime);
    gfxDriver->SetCallbackToDrawScreen(draw_screen_callback);
    gfxDriver->SetCallbackForNullSprite(GfxDriverNullSpriteCallback);
}

void engine_set_color_conversions()
{
    Out::FPrint("Initializing colour conversion");

    set_color_conversion(COLORCONV_MOST | COLORCONV_EXPAND_256 | COLORCONV_REDUCE_16_TO_15);
}

int create_gfx_driver_and_init_mode(const String &gfx_driver_id, Size &screen_size, ColorDepthOption &color_depths)
{
    Placement drawing_place;

    create_gfx_driver(gfx_driver_id);
    int res = engine_init_screen_settings(screen_size, drawing_place, color_depths);
    if (res != RETURN_CONTINUE)
    {
        return res;
    }
    res = engine_init_graphics_mode(screen_size, drawing_place, color_depths);
    if (res != RETURN_CONTINUE)
    {
        return res;
    }
    return RETURN_CONTINUE;
}

void display_gfx_mode_error(const Size screen_size, const ColorDepthOption color_depths)
{
    proper_exit=1;
    platform->FinishedUsingGraphicsMode();

    platform->DisplayAlert("There was a problem initializing graphics mode %d x %d (%d-bit).\n"
        "(Problem: '%s')\n"
        "Try to correct the problem, or seek help from the AGS homepage.\n"
        "\nPossible causes:\n* your graphics card drivers do not support this resolution. "
        "Run the game setup program and try the other resolution.\n"
        "* the graphics driver you have selected does not work. Try changing graphics driver.\n"
        "* the graphics filter you have selected does not work. Try another filter.",
        screen_size.Width, screen_size.Height, color_depths.First, get_allegro_error());
}

int graphics_mode_init()
{
    Size screen_size;
    ColorDepthOption color_depths;

    int res = create_gfx_driver_and_init_mode(usetup.gfxDriverID, screen_size, color_depths);
    if (res != RETURN_CONTINUE)
    {
        if (gfxDriver && stricmp(gfxDriver->GetDriverID(), "DX5") != 0)
        {
            graphics_mode_shutdown();
            res = create_gfx_driver_and_init_mode("DX5", screen_size, color_depths);
        }
    }
    if (res != RETURN_CONTINUE)
    {
        display_gfx_mode_error(screen_size, color_depths);
        return res;
    }

    engine_post_init_gfx_driver();
    engine_prepare_screen();
    platform->PostAllegroInit((usetup.windowed > 0) ? true : false);
    engine_set_gfx_driver_callbacks();
    engine_set_color_conversions();
    return RETURN_CONTINUE;
}

void graphics_mode_shutdown()
{
    // Release the display mode (and anything dependant on the window)
    if (gfxDriver != NULL)
    {
        gfxDriver->UnInit();
    }

    // Tell Allegro that we are no longer in graphics mode
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);

    delete gfxDriver;
    gfxDriver = NULL;

    delete gfxFilter;
    gfxFilter = NULL;
}
