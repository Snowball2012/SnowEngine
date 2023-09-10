#pragma warning(push, 0)

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>


#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_USE_MINIZ 0
#include <tinyexr/tinyexr.h>