// Single translation unit that compiles the stb_image implementation.
// Every other file includes stb_image.h for declarations only, so there is
// exactly one definition of the symbols at link time.
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"
