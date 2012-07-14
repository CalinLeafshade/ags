//
// Implementation from acroom.cpp specific to AGS.Native library
//

// Headers, as they are in acroom.cpp

#include <stdio.h>  // needed for bigend
// MACPORT FIX: endian support
#include "platform/bigend.h"
#include "util/misc.h"
//#include "wgt2allg_nofunc.h"
#include "util/wgt2allg.h"
//#include "acroom_func.h"
#include "ac/roomstruct.h"
//

//=============================================================================
// AGS.Native-specific implementation split out of acroom.cpp
//=============================================================================

bool load_room_is_version_bad(roomstruct *rstruc)
{
  return ((rstruc->wasversion < 15) || (rstruc->wasversion > ROOM_FILE_VERSION));
}
