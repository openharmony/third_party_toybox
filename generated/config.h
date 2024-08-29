#ifdef OHOS_LITE
#include "config_lite.h"
#else
#ifdef TOYBOX_BUILD_USER
#include "config_user.h"
#else
#include "config_root.h"
#endif
#endif