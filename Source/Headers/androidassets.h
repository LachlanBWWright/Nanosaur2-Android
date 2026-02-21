// androidassets.h - Android asset extraction

#pragma once

#ifdef __ANDROID__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool AndroidExtractAssets(void);

#ifdef __cplusplus
}
#endif

#endif // __ANDROID__
