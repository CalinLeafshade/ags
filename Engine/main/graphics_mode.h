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
//
//
//=============================================================================
#ifndef __AGS_EE_MAIN__GRAPHICSMODE_H
#define __AGS_EE_MAIN__GRAPHICSMODE_H

#include "gfx/gfxmodelist.h"
#include "util/geometry.h"

enum RenderFramePlacement
{
    kRenderPlaceCenter,
    kRenderPlaceStretch,
    kRenderPlaceStretchProportional,
    kRenderPlaceResizeWindow
};

int graphics_mode_init();

extern Size GameSize;
extern AGS::Engine::DisplayResolution GameResolution;

extern int debug_15bit_mode, debug_24bit_mode;
extern int convert_16bit_bgr;

#endif // __AGS_EE_MAIN__GRAPHICSMODE_H
