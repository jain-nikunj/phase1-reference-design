/* 
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */
#include "RadioHardwareConfig.h"
#include "AppManager.h"
#include "PacketStore.hh"

class RadioHardwareConfig;
class AppManager;

typedef struct {
    PacketStore*            ps_ptr;
    RadioHardwareConfig*    rhc_ptr;
    AppManager*             am_ptr;
} callback_userdata_t;
