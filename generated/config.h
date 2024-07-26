#ifdef OHOS_LITE
#include "config_lite.h"
#else
#ifdef TOYBOX_BUILD_ROOT
#include "config_root.h"
#else
#include "config_user.h"
#endif
#endif