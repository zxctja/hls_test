#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <cstring>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>


//typedef struct WebPConfig WebPConfig;
typedef struct WebPPicture WebPPicture;   // main structure for I/O
typedef struct WebPAuxStats WebPAuxStats;
//typedef struct WebPMemoryWriter WebPMemoryWriter;

// Color spaces.
typedef enum WebPEncCSP {
  // chroma sampling
  WEBP_YUV420  = 0,        // 4:2:0
  WEBP_YUV420A = 4,        // alpha channel variant
  WEBP_CSP_UV_MASK = 3,    // bit-mask to get the UV sampling factors
  WEBP_CSP_ALPHA_BIT = 4   // bit that is set if alpha is present
} WebPEncCSP;

// Signature for output function. Should return true if writing was successful.
// data/data_size is the segment of data to write, and 'picture' is for
// reference (and so one can make use of picture->custom_ptr).
typedef int (*WebPWriterFunction)(const uint8_t* data, size_t data_size,
                                  const WebPPicture* picture);


// Encoding error conditions.
typedef enum WebPEncodingError {
  VP8_ENC_OK = 0,
  VP8_ENC_ERROR_OUT_OF_MEMORY,            // memory error allocating objects
  VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY,  // memory error while flushing bits
  VP8_ENC_ERROR_NULL_PARAMETER,           // a pointer parameter is NULL
  VP8_ENC_ERROR_INVALID_CONFIGURATION,    // configuration is invalid
  VP8_ENC_ERROR_BAD_DIMENSION,            // picture has invalid width/height
  VP8_ENC_ERROR_PARTITION0_OVERFLOW,      // partition is bigger than 512k
  VP8_ENC_ERROR_PARTITION_OVERFLOW,       // partition is bigger than 16M
  VP8_ENC_ERROR_BAD_WRITE,                // error while flushing bytes
  VP8_ENC_ERROR_FILE_TOO_BIG,             // file is bigger than 4G
  VP8_ENC_ERROR_USER_ABORT,               // abort request by user
  VP8_ENC_ERROR_LAST                      // list terminator. always last.
} WebPEncodingError;

typedef int (*WebPProgressHook)(int percent, const WebPPicture* picture);

struct WebPPicture {
  //   INPUT
  //////////////
  // Main flag for encoder selecting between ARGB or YUV input.
  // It is recommended to use ARGB input (*argb, argb_stride) for lossless
  // compression, and YUV input (*y, *u, *v, etc.) for lossy compression
  // since these are the respective native colorspace for these formats.
  int use_argb;

  // YUV input (mostly used for input to lossy compression)
  WebPEncCSP colorspace;     // colorspace: should be YUV420 for now (=Y'CbCr).
  int width, height;         // dimensions (less or equal to WEBP_MAX_DIMENSION)
  uint8_t *y, *u, *v;        // pointers to luma/chroma planes.
  int y_stride, uv_stride;   // luma/chroma strides.
  uint8_t* a;                // pointer to the alpha plane
  int a_stride;              // stride of the alpha plane
  uint32_t pad1[2];          // padding for later use

  // ARGB input (mostly used for input to lossless compression)
  uint32_t* argb;            // Pointer to argb (32 bit) plane.
  int argb_stride;           // This is stride in pixels units, not bytes.
  uint32_t pad2[3];          // padding for later use

  //   OUTPUT
  ///////////////
  // Byte-emission hook, to store compressed bytes as they are ready.
  WebPWriterFunction writer;  // can be NULL
  void* custom_ptr;           // can be used by the writer.

  // map for extra information (only for lossy compression mode)
  int extra_info_type;    // 1: intra type, 2: segment, 3: quant
                          // 4: intra-16 prediction mode,
                          // 5: chroma prediction mode,
                          // 6: bit cost, 7: distortion
  uint8_t* extra_info;    // if not NULL, points to an array of size
                          // ((width + 15) / 16) * ((height + 15) / 16) that
                          // will be filled with a macroblock map, depending
                          // on extra_info_type.

  //   STATS AND REPORTS
  ///////////////////////////
  // Pointer to side statistics (updated only if not NULL)
  WebPAuxStats* stats;

  // Error code for the latest error encountered during encoding
  WebPEncodingError error_code;

  // If not NULL, report progress during encoding.
  WebPProgressHook progress_hook;

  void* user_data;        // this field is free to be set to any value and
                          // used during callbacks (like progress-report e.g.).

  uint32_t pad3[3];       // padding for later use

  // Unused for now
  uint8_t *pad4, *pad5;
  uint32_t pad6[8];       // padding for later use

  // PRIVATE FIELDS
  ////////////////////
  void* memory_;          // row chunk of memory for yuva planes
  void* memory_argb_;     // and for argb too.
  void* pad7[2];          // padding for later use
};

typedef enum WebPImageHint {
  WEBP_HINT_DEFAULT = 0,  // default preset.
  WEBP_HINT_PICTURE,      // digital picture, like portrait, inner shot
  WEBP_HINT_PHOTO,        // outdoor photograph, with natural lighting
  WEBP_HINT_GRAPH,        // Discrete tone image (graph, map-tile etc).
  WEBP_HINT_LAST
} WebPImageHint;

struct WebPConfig {
  int lossless;           // Lossless encoding (0=lossy(default), 1=lossless).
  float quality;          // between 0 and 100. For lossy, 0 gives the smallest
                          // size and 100 the largest. For lossless, this
                          // parameter is the amount of effort put into the
                          // compression: 0 is the fastest but gives larger
                          // files compared to the slowest, but best, 100.
  int method;             // quality/speed trade-off (0=fast, 6=slower-better)

  WebPImageHint image_hint;  // Hint for image type (lossless only for now).

  int target_size;        // if non-zero, set the desired target size in bytes.
                          // Takes precedence over the 'compression' parameter.
  float target_PSNR;      // if non-zero, specifies the minimal distortion to
                          // try to achieve. Takes precedence over target_size.
  int segments;           // maximum number of segments to use, in [1..4]
  int sns_strength;       // Spatial Noise Shaping. 0=off, 100=maximum.
  int filter_strength;    // range: [0 = off .. 100 = strongest]
  int filter_sharpness;   // range: [0 = off .. 7 = least sharp]
  int filter_type;        // filtering type: 0 = simple, 1 = strong (only used
                          // if filter_strength > 0 or autofilter > 0)
  int autofilter;         // Auto adjust filter's strength [0 = off, 1 = on]
  int alpha_compression;  // Algorithm for encoding the alpha plane (0 = none,
                          // 1 = compressed with WebP lossless). Default is 1.
  int alpha_filtering;    // Predictive filtering method for alpha plane.
                          //  0: none, 1: fast, 2: best. Default if 1.
  int alpha_quality;      // Between 0 (smallest size) and 100 (lossless).
                          // Default is 100.
  int pass;               // number of entropy-analysis passes (in [1..10]).

  int show_compressed;    // if true, export the compressed picture back.
                          // In-loop filtering is not applied.
  int preprocessing;      // preprocessing filter:
                          // 0=none, 1=segment-smooth, 2=pseudo-random dithering
  int partitions;         // log2(number of token partitions) in [0..3]. Default
                          // is set to 0 for easier progressive decoding.
  int partition_limit;    // quality degradation allowed to fit the 512k limit
                          // on prediction modes coding (0: no degradation,
                          // 100: maximum possible degradation).
  int emulate_jpeg_size;  // If true, compression parameters will be remapped
                          // to better match the expected output size from
                          // JPEG compression. Generally, the output size will
                          // be similar but the degradation will be lower.
  int thread_level;       // If non-zero, try and use multi-threaded encoding.
  int low_memory;         // If set, reduce memory usage (but increase CPU use).

  int near_lossless;      // Near lossless encoding [0 = max loss .. 100 = off
                          // (default)].
  int exact;              // if non-zero, preserve the exact RGB values under
                          // transparent area. Otherwise, discard this invisible
                          // RGB information for better compression. The default
                          // value is 0.

  int use_delta_palette;  // reserved for future lossless feature
  int use_sharp_yuv;      // if needed, use sharp (and slow) RGB->YUV conversion

  uint32_t pad[2];        // padding for later use
};

struct WebPAuxStats {
  int coded_size;         // final size

  float PSNR[5];          // peak-signal-to-noise ratio for Y/U/V/All/Alpha
  int block_count[3];     // number of intra4/intra16/skipped macroblocks
  int header_bytes[2];    // approximate number of bytes spent for header
                          // and mode-partition #0
  int residual_bytes[3][4];  // approximate number of bytes spent for
                             // DC/AC/uv coefficients for each (0..3) segments.
  int segment_size[4];    // number of macroblocks in each segments
  int segment_quant[4];   // quantizer values for each segments
  int segment_level[4];   // filtering strength for each segments [0..63]

  int alpha_data_size;    // size of the transparency data
  int layer_data_size;    // size of the enhancement layer data

  // lossless encoder statistics
  uint32_t lossless_features;  // bit0:predictor bit1:cross-color transform
                               // bit2:subtract-green bit3:color indexing
  int histogram_bits;          // number of precision bits of histogram
  int transform_bits;          // precision bits for transform
  int cache_bits;              // number of bits for color cache lookup
  int palette_size;            // number of color in palette, if used
  int lossless_size;           // final lossless size
  int lossless_hdr_size;       // lossless header (transform, huffman etc) size
  int lossless_data_size;      // lossless image data size

  uint32_t pad[2];        // padding for later use
};

struct WebPMemoryWriter {
  uint8_t* mem;       // final buffer (of size 'max_size', larger than 'size').
  size_t   size;      // final size
  size_t   max_size;  // total capacity
  uint32_t pad[1];    // padding for later use
};

typedef struct timeval Stopwatch;

void WebPMemoryWriterInit(WebPMemoryWriter* writer) {
  writer->mem = NULL;
  writer->size = 0;
  writer->max_size = 0;
}

#define WEBP_ABI_IS_INCOMPATIBLE(a, b) (((a) >> 8) != ((b) >> 8))
#define WEBP_ENCODER_ABI_VERSION 0x020e    // MAJOR(8b) + MINOR(8b)

static int DummyWriter(const uint8_t* data, size_t data_size,
                       const WebPPicture* const picture) {
  // The following are to prevent 'unused variable' error message.
  (void)data;
  (void)data_size;
  (void)picture;
  return 1;
}

int WebPEncodingSetError(const WebPPicture* const pic,
                         WebPEncodingError error) {
  assert((int)error < VP8_ENC_ERROR_LAST);
  assert((int)error >= VP8_ENC_OK);
  ((WebPPicture*)pic)->error_code = error;
  return 0;
}

int WebPPictureInitInternal(WebPPicture* picture, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_ENCODER_ABI_VERSION)) {
    return 0;   // caller/system version mismatch!
  }
  if (picture != NULL) {
    memset(picture, 0, sizeof(*picture));
    picture->writer = DummyWriter;
    WebPEncodingSetError(picture, VP8_ENC_OK);
  }
  return 1;
}

static  int WebPPictureInit(WebPPicture* picture) {
  return WebPPictureInitInternal(picture, WEBP_ENCODER_ABI_VERSION);
}

typedef enum WebPPreset {
  WEBP_PRESET_DEFAULT = 0,  // default preset.
  WEBP_PRESET_PICTURE,      // digital picture, like portrait, inner shot
  WEBP_PRESET_PHOTO,        // outdoor photograph, with natural lighting
  WEBP_PRESET_DRAWING,      // hand or line drawing, with high-contrast details
  WEBP_PRESET_ICON,         // small-sized colorful images
  WEBP_PRESET_TEXT          // text-like
} WebPPreset;

int WebPValidateConfig(const WebPConfig* config) {
  if (config == NULL) return 0;
  if (config->quality < 0 || config->quality > 100) return 0;
  if (config->target_size < 0) return 0;
  if (config->target_PSNR < 0) return 0;
  if (config->method < 0 || config->method > 6) return 0;
  if (config->segments < 1 || config->segments > 4) return 0;
  if (config->sns_strength < 0 || config->sns_strength > 100) return 0;
  if (config->filter_strength < 0 || config->filter_strength > 100) return 0;
  if (config->filter_sharpness < 0 || config->filter_sharpness > 7) return 0;
  if (config->filter_type < 0 || config->filter_type > 1) return 0;
  if (config->autofilter < 0 || config->autofilter > 1) return 0;
  if (config->pass < 1 || config->pass > 10) return 0;
  if (config->show_compressed < 0 || config->show_compressed > 1) return 0;
  if (config->preprocessing < 0 || config->preprocessing > 7) return 0;
  if (config->partitions < 0 || config->partitions > 3) return 0;
  if (config->partition_limit < 0 || config->partition_limit > 100) return 0;
  if (config->alpha_compression < 0) return 0;
  if (config->alpha_filtering < 0) return 0;
  if (config->alpha_quality < 0 || config->alpha_quality > 100) return 0;
  if (config->lossless < 0 || config->lossless > 1) return 0;
  if (config->near_lossless < 0 || config->near_lossless > 100) return 0;
  if (config->image_hint >= WEBP_HINT_LAST) return 0;
  if (config->emulate_jpeg_size < 0 || config->emulate_jpeg_size > 1) return 0;
  if (config->thread_level < 0 || config->thread_level > 1) return 0;
  if (config->low_memory < 0 || config->low_memory > 1) return 0;
  if (config->exact < 0 || config->exact > 1) return 0;
  if (config->use_delta_palette < 0 || config->use_delta_palette > 1) {
    return 0;
  }
  if (config->use_sharp_yuv < 0 || config->use_sharp_yuv > 1) return 0;

  return 1;
}

int WebPConfigInitInternal(WebPConfig* config,
                           WebPPreset preset, float quality, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_ENCODER_ABI_VERSION)) {
    return 0;   // caller/system version mismatch!
  }
  if (config == NULL) return 0;

  config->quality = quality;
  config->target_size = 0;
  config->target_PSNR = 0.;
  config->method = 4;
  config->sns_strength = 50;
  config->filter_strength = 60;   // mid-filtering
  config->filter_sharpness = 0;
  config->filter_type = 1;        // default: strong (so U/V is filtered too)
  config->partitions = 0;
  config->segments = 1;
  config->pass = 1;
  config->show_compressed = 0;
  config->preprocessing = 0;
  config->autofilter = 0;
  config->partition_limit = 0;
  config->alpha_compression = 1;
  config->alpha_filtering = 1;
  config->alpha_quality = 100;
  config->lossless = 0;
  config->exact = 0;
  config->image_hint = WEBP_HINT_DEFAULT;
  config->emulate_jpeg_size = 0;
  config->thread_level = 0;
  config->low_memory = 0;
  config->near_lossless = 100;
  config->use_delta_palette = 0;
  config->use_sharp_yuv = 0;

  // TODO(skal): tune.
  switch (preset) {
    case WEBP_PRESET_PICTURE:
      config->sns_strength = 80;
      config->filter_sharpness = 4;
      config->filter_strength = 35;
      config->preprocessing &= ~2;   // no dithering
      break;
    case WEBP_PRESET_PHOTO:
      config->sns_strength = 80;
      config->filter_sharpness = 3;
      config->filter_strength = 30;
      config->preprocessing |= 2;
      break;
    case WEBP_PRESET_DRAWING:
      config->sns_strength = 25;
      config->filter_sharpness = 6;
      config->filter_strength = 10;
      break;
    case WEBP_PRESET_ICON:
      config->sns_strength = 0;
      config->filter_strength = 0;   // disable filtering to retain sharpness
      config->preprocessing &= ~2;   // no dithering
      break;
    case WEBP_PRESET_TEXT:
      config->sns_strength = 0;
      config->filter_strength = 0;   // disable filtering to retain sharpness
      config->preprocessing &= ~2;   // no dithering
      config->segments = 2;
      break;
    case WEBP_PRESET_DEFAULT:
    default:
      break;
  }
  return WebPValidateConfig(config);
}

static  int WebPConfigInit(WebPConfig* config) {
  return WebPConfigInitInternal(config, WEBP_PRESET_DEFAULT, 75.f,
                                WEBP_ENCODER_ABI_VERSION);
}

static void HelpShort(void) {
  printf("Usage:\n\n");
  printf("   cwebp [options] -q quality input.png -o output.webp\n\n");
  printf("where quality is between 0 (poor) to 100 (very good).\n");
  printf("Typical value is around 80.\n\n");
  printf("Try -longhelp for an exhaustive list of advanced options.\n");
}

static void HelpLong(void) {
  printf("Usage:\n");
  printf(" cwebp [-preset <...>] [options] in_file [-o out_file]\n\n");
  printf("If input size (-s) for an image is not specified, it is\n"
         "assumed to be a PNG, JPEG, TIFF or WebP file.\n");
#ifdef HAVE_WINCODEC_H
  printf("Windows builds can take as input any of the files handled by WIC.\n");
#endif
  printf("\nOptions:\n");
  printf("  -h / -help ............. short help\n");
  printf("  -H / -longhelp ......... long help\n");
  printf("  -q <float> ............. quality factor (0:small..100:big), "
         "default=75\n");
  printf("  -alpha_q <int> ......... transparency-compression quality (0..100),"
         "\n                           default=100\n");
  printf("  -preset <string> ....... preset setting, one of:\n");
  printf("                            default, photo, picture,\n");
  printf("                            drawing, icon, text\n");
  printf("     -preset must come first, as it overwrites other parameters\n");
  printf("  -z <int> ............... activates lossless preset with given\n"
         "                           level in [0:fast, ..., 9:slowest]\n");
  printf("\n");
  printf("  -m <int> ............... compression method (0=fast, 6=slowest), "
         "default=4\n");
  printf("  -segments <int> ........ number of segments to use (1..4), "
         "default=4\n");
  printf("  -size <int> ............ target size (in bytes)\n");
  printf("  -psnr <float> .......... target PSNR (in dB. typically: 42)\n");
  printf("\n");
  printf("  -s <int> <int> ......... input size (width x height) for YUV\n");
  printf("  -sns <int> ............. spatial noise shaping (0:off, 100:max), "
         "default=50\n");
  printf("  -f <int> ............... filter strength (0=off..100), "
         "default=60\n");
  printf("  -sharpness <int> ....... "
         "filter sharpness (0:most .. 7:least sharp), default=0\n");
  printf("  -strong ................ use strong filter instead "
                                     "of simple (default)\n");
  printf("  -nostrong .............. use simple filter instead of strong\n");
  printf("  -sharp_yuv ............. use sharper (and slower) RGB->YUV "
                                     "conversion\n");
  printf("  -partition_limit <int> . limit quality to fit the 512k limit on\n");
  printf("                           "
         "the first partition (0=no degradation ... 100=full)\n");
  printf("  -pass <int> ............ analysis pass number (1..10)\n");
  printf("  -crop <x> <y> <w> <h> .. crop picture with the given rectangle\n");
  printf("  -resize <w> <h> ........ resize picture (after any cropping)\n");
  printf("  -mt .................... use multi-threading if available\n");
  printf("  -low_memory ............ reduce memory usage (slower encoding)\n");
  printf("  -map <int> ............. print map of extra info\n");
  printf("  -print_psnr ............ prints averaged PSNR distortion\n");
  printf("  -print_ssim ............ prints averaged SSIM distortion\n");
  printf("  -print_lsim ............ prints local-similarity distortion\n");
  printf("  -d <file.pgm> .......... dump the compressed output (PGM file)\n");
  printf("  -alpha_method <int> .... transparency-compression method (0..1), "
         "default=1\n");
  printf("  -alpha_filter <string> . predictive filtering for alpha plane,\n");
  printf("                           one of: none, fast (default) or best\n");
  printf("  -exact ................. preserve RGB values in transparent area, "
         "default=off\n");
  printf("  -blend_alpha <hex> ..... blend colors against background color\n"
         "                           expressed as RGB values written in\n"
         "                           hexadecimal, e.g. 0xc0e0d0 for red=0xc0\n"
         "                           green=0xe0 and blue=0xd0\n");
  printf("  -noalpha ............... discard any transparency information\n");
  printf("  -lossless .............. encode image losslessly, default=off\n");
  printf("  -near_lossless <int> ... use near-lossless image\n"
         "                           preprocessing (0..100=off), "
         "default=100\n");
  printf("  -hint <string> ......... specify image characteristics hint,\n");
  printf("                           one of: photo, picture or graph\n");

  printf("\n");
  printf("  -metadata <string> ..... comma separated list of metadata to\n");
  printf("                           ");
  printf("copy from the input to the output if present.\n");
  printf("                           "
         "Valid values: all, none (default), exif, icc, xmp\n");

  printf("\n");
  printf("  -short ................. condense printed message\n");
  printf("  -quiet ................. don't print anything\n");
  printf("  -version ............... print version number and exit\n");
#ifndef WEBP_DLL
  printf("  -noasm ................. disable all assembly optimizations\n");
#endif
  printf("  -v ..................... verbose, e.g. print encoding/decoding "
         "times\n");
  printf("  -progress .............. report encoding progress\n");
  printf("\n");
  printf("Experimental Options:\n");
  printf("  -jpeg_like ............. roughly match expected JPEG size\n");
  printf("  -af .................... auto-adjust filter strength\n");
  printf("  -pre <int> ............. pre-processing filter\n");
  printf("\n");
}

uint32_t ExUtilGetUInt(const char* const v, int base, int* const error) {
  char* end = NULL;
  const uint32_t n = (v != NULL) ? (uint32_t)strtoul(v, &end, base) : 0u;
  if (end == v && error != NULL && !*error) {
    *error = 1;
    fprintf(stderr, "Error! '%s' is not an integer.\n",
            (v != NULL) ? v : "(null)");
  }
  return n;
}

int ExUtilGetInt(const char* const v, int base, int* const error) {
  return (int)ExUtilGetUInt(v, base, error);
}

#define WEBP_MAX_DIMENSION 16383

float ExUtilGetFloat(const char* const v, int* const error) {
  char* end = NULL;
  const float f = (v != NULL) ? (float)strtod(v, &end) : 0.f;
  if (end == v && error != NULL && !*error) {
    *error = 1;
    fprintf(stderr, "Error! '%s' is not a floating point number.\n",
            (v != NULL) ? v : "(null)");
  }
  return f;
}

// version numbers
#define ENC_MAJ_VERSION 1
#define ENC_MIN_VERSION 0
#define ENC_REV_VERSION 0

int WebPGetEncoderVersion(void) {
  return (ENC_MAJ_VERSION << 16) | (ENC_MIN_VERSION << 8) | ENC_REV_VERSION;
}

static int verbose = 0;

static  void StopwatchReset(Stopwatch* watch) {
  gettimeofday(watch, NULL);
}

static  double StopwatchReadAndReset(Stopwatch* watch) {
  struct timeval old_value;
  double delta_sec, delta_usec;
  memcpy(&old_value, watch, sizeof(old_value));
  gettimeofday(watch, NULL);
  delta_sec = (double)watch->tv_sec - old_value.tv_sec;
  delta_usec = (double)watch->tv_usec - old_value.tv_usec;
  return delta_sec + delta_usec / 1000000.0;
}

typedef struct MetadataPayload {
  uint8_t* bytes;
  size_t size;
} MetadataPayload;

typedef struct Metadata {
  MetadataPayload exif;
  MetadataPayload iccp;
  MetadataPayload xmp;
} Metadata;

FILE* ImgIoUtilSetBinaryMode(FILE* file) {
  return file;
}

int ImgIoUtilReadFromStdin(const uint8_t** data, size_t* data_size) {
  static const size_t kBlockSize = 16384;  // default initial size
  size_t max_size = 0;
  size_t size = 0;
  uint8_t* input = NULL;

  if (data == NULL || data_size == NULL) return 0;
  *data = NULL;
  *data_size = 0;

  if (!ImgIoUtilSetBinaryMode(stdin)) return 0;

  while (!feof(stdin)) {
    // We double the buffer size each time and read as much as possible.
    const size_t extra_size = (max_size == 0) ? kBlockSize : max_size;
    // we allocate one extra byte for the \0 terminator
    void* const new_data = realloc(input, max_size + extra_size + 1);
    if (new_data == NULL) goto Error;
    input = (uint8_t*)new_data;
    max_size += extra_size;
    size += fread(input + size, 1, extra_size, stdin);
    if (size < max_size) break;
  }
  if (ferror(stdin)) goto Error;
  if (input != NULL) input[size] = '\0';  // convenient 0-terminator
  *data = input;
  *data_size = size;
  return 1;

 Error:
  free(input);
  fprintf(stderr, "Could not read from stdin\n");
  return 0;
}

int ImgIoUtilReadFile(const char* const file_name,
                      const uint8_t** data, size_t* data_size) {
  int ok;
  uint8_t* file_data;
  size_t file_size;
  FILE* in;
  const int from_stdin = (file_name == NULL) || !strcmp(file_name, "-");

  if (from_stdin) return ImgIoUtilReadFromStdin(data, data_size);

  if (data == NULL || data_size == NULL) return 0;
  *data = NULL;
  *data_size = 0;

  in = fopen(file_name, "rb");
  if (in == NULL) {
    fprintf(stderr, "cannot open input file '%s'\n", file_name);
    return 0;
  }
  fseek(in, 0, SEEK_END);
  file_size = ftell(in);
  fseek(in, 0, SEEK_SET);
  // we allocate one extra byte for the \0 terminator
  file_data = (uint8_t*)malloc(file_size + 1);
  if (file_data == NULL) {
    fclose(in);
    fprintf(stderr, "memory allocation failure when reading file %s\n",
            file_name);
    return 0;
  }
  ok = (fread(file_data, file_size, 1, in) == 1);
  fclose(in);

  if (!ok) {
    fprintf(stderr, "Could not read %d bytes of data from file %s\n",
            (int)file_size, file_name);
    free(file_data);
    return 0;
  }
  file_data[file_size] = '\0';  // convenient 0-terminator
  *data = file_data;
  *data_size = file_size;
  return 1;
}

typedef int (*WebPImageReader)(const uint8_t* const data, size_t data_size,
                               struct WebPPicture* const pic,
                               int keep_alpha, struct Metadata* const metadata);

typedef enum {
  WEBP_PNG_FORMAT = 0,
  WEBP_JPEG_FORMAT,
  WEBP_TIFF_FORMAT,
  WEBP_WEBP_FORMAT,
  WEBP_PNM_FORMAT,
  WEBP_UNSUPPORTED_FORMAT
} WebPInputFileFormat;

static uint32_t GetBE32(const uint8_t buf[]) {
  return ((uint32_t)buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

WebPInputFileFormat WebPGuessImageType(const uint8_t* const data,
                                       size_t data_size) {
  WebPInputFileFormat format = WEBP_UNSUPPORTED_FORMAT;
  if (data != NULL && data_size >= 12) {
    const uint32_t magic1 = GetBE32(data + 0);
    const uint32_t magic2 = GetBE32(data + 8);
    if (magic1 == 0x89504E47U) {
      format = WEBP_PNG_FORMAT;
    } else if (magic1 >= 0xFFD8FF00U && magic1 <= 0xFFD8FFFFU) {
      format = WEBP_JPEG_FORMAT;
    } else if (magic1 == 0x49492A00 || magic1 == 0x4D4D002A) {
      format = WEBP_TIFF_FORMAT;
    } else if (magic1 == 0x52494646 && magic2 == 0x57454250) {
      format = WEBP_WEBP_FORMAT;
    } else if (((magic1 >> 24) & 0xff) == 'P') {
      const int type = (magic1 >> 16) & 0xff;
      // we only support 'P5 -> P7' for now.
      if (type >= '5' && type <= '7') format = WEBP_PNM_FORMAT;
    }
  }
  return format;
}

int ReadPNG(const uint8_t* const data, size_t data_size,
            struct WebPPicture* const pic,
            int keep_alpha, struct Metadata* const metadata) {
  (void)data;
  (void)data_size;
  (void)pic;
  (void)keep_alpha;
  (void)metadata;
  fprintf(stderr, "PNG support not compiled. Please install the libpng "
          "development package before building.\n");
  return 0;
}

int ReadTIFF(const uint8_t* const data, size_t data_size,
             struct WebPPicture* const pic, int keep_alpha,
             struct Metadata* const metadata) {
  (void)data;
  (void)data_size;
  (void)pic;
  (void)keep_alpha;
  (void)metadata;
  fprintf(stderr, "TIFF support not compiled. Please install the libtiff "
          "development package before building.\n");
  return 0;
}

int ReadWebP(const uint8_t* const data, size_t data_size,
             WebPPicture* const pic,
             int keep_alpha, Metadata* const metadata) {
	  (void)data;
	  (void)data_size;
	  (void)pic;
	  (void)keep_alpha;
	  (void)metadata;
	  fprintf(stderr, "WebP support not compiled. Please install the libwebp "
	          "development package before building.\n");
	  return 0;
}

int ReadPNM(const uint8_t* const data, size_t data_size,
            WebPPicture* const pic, int keep_alpha,
            struct Metadata* const metadata) {
	  (void)data;
	  (void)data_size;
	  (void)pic;
	  (void)keep_alpha;
	  (void)metadata;
	  fprintf(stderr, "PNM support not compiled. Please install the pnm "
	          "development package before building.\n");
	  return 0;
}

typedef struct {
  struct jpeg_source_mgr pub;
  const uint8_t* data;
  size_t data_size;
} JPEGReadContext;

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void my_error_exit(j_common_ptr dinfo) {
  struct my_error_mgr* myerr = (struct my_error_mgr*)dinfo->err;
  dinfo->err->output_message(dinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

void MetadataPayloadDelete(MetadataPayload* const payload) {
  if (payload == NULL) return;
  free(payload->bytes);
  payload->bytes = NULL;
  payload->size = 0;
}

void MetadataFree(Metadata* const metadata) {
  if (metadata == NULL) return;
  MetadataPayloadDelete(&metadata->exif);
  MetadataPayloadDelete(&metadata->iccp);
  MetadataPayloadDelete(&metadata->xmp);
}

static void ContextInit(j_decompress_ptr cinfo) {
  JPEGReadContext* const ctx = (JPEGReadContext*)cinfo->src;
  ctx->pub.next_input_byte = ctx->data;
  ctx->pub.bytes_in_buffer = ctx->data_size;
}

static boolean ContextFill(j_decompress_ptr cinfo) {
  // we shouldn't get here.
  ERREXIT(cinfo, JERR_FILE_READ);
  return FALSE;
}

static void ContextSkip(j_decompress_ptr cinfo, long jump_size) {
  JPEGReadContext* const ctx = (JPEGReadContext*)cinfo->src;
  size_t jump = (size_t)jump_size;
  if (jump > ctx->pub.bytes_in_buffer) {  // Don't overflow the buffer.
    jump = ctx->pub.bytes_in_buffer;
  }
  ctx->pub.bytes_in_buffer -= jump;
  ctx->pub.next_input_byte += jump;
}

static void ContextTerm(j_decompress_ptr cinfo) {
  (void)cinfo;
}

static void ContextSetup(volatile struct jpeg_decompress_struct* const cinfo,
                         JPEGReadContext* const ctx) {
  cinfo->src = (struct jpeg_source_mgr*)ctx;
  ctx->pub.init_source = ContextInit;
  ctx->pub.fill_input_buffer = ContextFill;
  ctx->pub.skip_input_data = ContextSkip;
  ctx->pub.resync_to_restart = jpeg_resync_to_restart;
  ctx->pub.term_source = ContextTerm;
  ctx->pub.bytes_in_buffer = 0;
  ctx->pub.next_input_byte = NULL;
}

int ImgIoUtilCheckSizeArgumentsOverflow(uint64_t nmemb, size_t size) {
  const uint64_t total_size = nmemb * size;
  int ok = (total_size == (size_t)total_size);
#if defined(WEBP_MAX_IMAGE_SIZE)
  ok = ok && (total_size <= (uint64_t)WEBP_MAX_IMAGE_SIZE);
#endif
  return ok;
}

#ifndef JPEG_APP1
# define JPEG_APP1 (JPEG_APP0 + 1)
#endif
#ifndef JPEG_APP2
# define JPEG_APP2 (JPEG_APP0 + 2)
#endif

static void SaveMetadataMarkers(j_decompress_ptr dinfo) {
  const unsigned int max_marker_length = 0xffff;
  jpeg_save_markers(dinfo, JPEG_APP1, max_marker_length);  // Exif/XMP
  jpeg_save_markers(dinfo, JPEG_APP2, max_marker_length);  // ICC profile
}

#define METADATA_OFFSET(x) offsetof(Metadata, x)

typedef struct {
  const uint8_t* data;
  size_t data_length;
  int seq;  // this segment's sequence number [1, 255] for use in reassembly.
} ICCPSegment;

static int CompareICCPSegments(const void* a, const void* b) {
  const ICCPSegment* s1 = (const ICCPSegment*)a;
  const ICCPSegment* s2 = (const ICCPSegment*)b;
  return s1->seq - s2->seq;
}

static int StoreICCP(j_decompress_ptr dinfo, MetadataPayload* const iccp) {
  // ICC.1:2010-12 (4.3.0.0) Annex B.4 Embedding ICC Profiles in JPEG files
  static const char kICCPSignature[] = "ICC_PROFILE";
  static const size_t kICCPSignatureLength = 12;  // signature includes '\0'
  static const size_t kICCPSkipLength = 14;  // signature + seq & count
  int expected_count = 0;
  int actual_count = 0;
  int seq_max = 0;
  size_t total_size = 0;
  ICCPSegment iccp_segments[255];
  jpeg_saved_marker_ptr marker;

  memset(iccp_segments, 0, sizeof(iccp_segments));
  for (marker = dinfo->marker_list; marker != NULL; marker = marker->next) {
    if (marker->marker == JPEG_APP2 &&
        marker->data_length > kICCPSkipLength &&
        !memcmp(marker->data, kICCPSignature, kICCPSignatureLength)) {
      // ICC_PROFILE\0<seq><count>; 'seq' starts at 1.
      const int seq = marker->data[kICCPSignatureLength];
      const int count = marker->data[kICCPSignatureLength + 1];
      const size_t segment_size = marker->data_length - kICCPSkipLength;
      ICCPSegment* segment;

      if (segment_size == 0 || count == 0 || seq == 0) {
        fprintf(stderr, "[ICCP] size (%d) / count (%d) / sequence number (%d)"
                        " cannot be 0!\n",
                (int)segment_size, seq, count);
        return 0;
      }

      if (expected_count == 0) {
        expected_count = count;
      } else if (expected_count != count) {
        fprintf(stderr, "[ICCP] Inconsistent segment count (%d / %d)!\n",
                expected_count, count);
        return 0;
      }

      segment = iccp_segments + seq - 1;
      if (segment->data_length != 0) {
        fprintf(stderr, "[ICCP] Duplicate segment number (%d)!\n" , seq);
        return 0;
      }

      segment->data = marker->data + kICCPSkipLength;
      segment->data_length = segment_size;
      segment->seq = seq;
      total_size += segment_size;
      if (seq > seq_max) seq_max = seq;
      ++actual_count;
    }
  }

  if (actual_count == 0) return 1;
  if (seq_max != actual_count) {
    fprintf(stderr, "[ICCP] Discontinuous segments, expected: %d actual: %d!\n",
            actual_count, seq_max);
    return 0;
  }
  if (expected_count != actual_count) {
    fprintf(stderr, "[ICCP] Segment count: %d does not match expected: %d!\n",
            actual_count, expected_count);
    return 0;
  }

  // The segments may appear out of order in the file, sort them based on
  // sequence number before assembling the payload.
  qsort(iccp_segments, actual_count, sizeof(*iccp_segments),
        CompareICCPSegments);

  iccp->bytes = (uint8_t*)malloc(total_size);
  if (iccp->bytes == NULL) return 0;
  iccp->size = total_size;

  {
    int i;
    size_t offset = 0;
    for (i = 0; i < seq_max; ++i) {
      memcpy(iccp->bytes + offset,
             iccp_segments[i].data, iccp_segments[i].data_length);
      offset += iccp_segments[i].data_length;
    }
  }
  return 1;
}

int MetadataCopy(const char* metadata, size_t metadata_len,
                 MetadataPayload* const payload) {
  if (metadata == NULL || metadata_len == 0 || payload == NULL) return 0;
  payload->bytes = (uint8_t*)malloc(metadata_len);
  if (payload->bytes == NULL) return 0;
  payload->size = metadata_len;
  memcpy(payload->bytes, metadata, metadata_len);
  return 1;
}

static int ExtractMetadataFromJPEG(j_decompress_ptr dinfo,
                                   Metadata* const metadata) {
  static const struct {
    int marker;
    const char* signature;
    size_t signature_length;
    size_t storage_offset;
  } kJPEGMetadataMap[] = {
    // Exif 2.2 Section 4.7.2 Interoperability Structure of APP1 ...
    { JPEG_APP1, "Exif\0",                        6, METADATA_OFFSET(exif) },
    // XMP Specification Part 3 Section 3 Embedding XMP Metadata ... #JPEG
    // TODO(jzern) Add support for 'ExtendedXMP'
    { JPEG_APP1, "http://ns.adobe.com/xap/1.0/", 29, METADATA_OFFSET(xmp) },
    { 0, NULL, 0, 0 },
  };
  jpeg_saved_marker_ptr marker;
  // Treat ICC profiles separately as they may be segmented and out of order.
  if (!StoreICCP(dinfo, &metadata->iccp)) return 0;

  for (marker = dinfo->marker_list; marker != NULL; marker = marker->next) {
    int i;
    for (i = 0; kJPEGMetadataMap[i].marker != 0; ++i) {
      if (marker->marker == kJPEGMetadataMap[i].marker &&
          marker->data_length > kJPEGMetadataMap[i].signature_length &&
          !memcmp(marker->data, kJPEGMetadataMap[i].signature,
                  kJPEGMetadataMap[i].signature_length)) {
        MetadataPayload* const payload =
            (MetadataPayload*)((uint8_t*)metadata +
                               kJPEGMetadataMap[i].storage_offset);

        if (payload->bytes == NULL) {
          const char* marker_data = (const char*)marker->data +
                                    kJPEGMetadataMap[i].signature_length;
          const size_t marker_data_length =
              marker->data_length - kJPEGMetadataMap[i].signature_length;
          if (!MetadataCopy(marker_data, marker_data_length, payload)) return 0;
        } else {
          fprintf(stderr, "Ignoring additional '%s' marker\n",
                  kJPEGMetadataMap[i].signature);
        }
      }
    }
  }
  return 1;
}

static int HasAlpha8b_C(const uint8_t* src, int length) {
  while (length-- > 0) if (*src++ != 0xff) return 1;
  return 0;
}

static int HasAlpha32b_C(const uint8_t* src, int length) {
  int x;
  for (x = 0; length-- > 0; x += 4) if (src[x] != 0xff) return 1;
  return 0;
}

static int CheckNonOpaque(const uint8_t* alpha, int width, int height,
                          int x_step, int y_step) {
  if (alpha == NULL) return 0;
  if (x_step == 1) {
    for (; height-- > 0; alpha += y_step) {
      if (HasAlpha8b_C(alpha, width)) return 1;
    }
  } else {
    for (; height-- > 0; alpha += y_step) {
      if (HasAlpha32b_C(alpha, width)) return 1;
    }
  }
  return 0;
}

static const int kMinDimensionIterativeConversion = 4;

#define Increment(v) do {} while (0)
#define AddMem(p, s) do {} while (0)
#define SubMem(p)    do {} while (0)

void WebPSafeFree(void* const ptr) {
  if (ptr != NULL) {
    Increment(&num_free_calls);
    SubMem(ptr);
  }
  free(ptr);
}

static void WebPPictureResetBufferYUVA(WebPPicture* const picture) {
  picture->memory_ = NULL;
  picture->y = picture->u = picture->v = picture->a = NULL;
  picture->y_stride = picture->uv_stride = 0;
  picture->a_stride = 0;
}

#define WEBP_MAX_ALLOCABLE_MEMORY (1ULL << 34)

static int CheckSizeArgumentsOverflow(uint64_t nmemb, size_t size) {
  const uint64_t total_size = nmemb * size;
  if (nmemb == 0) return 1;
  if ((uint64_t)size > WEBP_MAX_ALLOCABLE_MEMORY / nmemb) return 0;
  if (total_size != (size_t)total_size) return 0;
#if defined(PRINT_MEM_INFO) && defined(MALLOC_FAIL_AT)
  if (countdown_to_fail > 0 && --countdown_to_fail == 0) {
    return 0;    // fake fail!
  }
#endif
#if defined(MALLOC_LIMIT)
  if (mem_limit > 0) {
    const uint64_t new_total_mem = (uint64_t)total_mem + total_size;
    if (new_total_mem != (size_t)new_total_mem ||
        new_total_mem > mem_limit) {
      return 0;   // fake fail!
    }
  }
#endif

  return 1;
}

void* WebPSafeMalloc(uint64_t nmemb, size_t size) {
  void* ptr;
  Increment(&num_malloc_calls);
  if (!CheckSizeArgumentsOverflow(nmemb, size)) return NULL;
  assert(nmemb * size > 0);
  ptr = malloc((size_t)(nmemb * size));
  AddMem(ptr, (size_t)(nmemb * size));
  return ptr;
}

int WebPPictureAllocYUVA(WebPPicture* const picture, int width, int height) {
  const WebPEncCSP uv_csp =
      (WebPEncCSP)((int)picture->colorspace & WEBP_CSP_UV_MASK);
  const int has_alpha = (int)picture->colorspace & WEBP_CSP_ALPHA_BIT;
  const int y_stride = width;
  const int uv_width = (int)(((int64_t)width + 1) >> 1);
  const int uv_height = (int)(((int64_t)height + 1) >> 1);
  const int uv_stride = uv_width;
  int a_width, a_stride;
  uint64_t y_size, uv_size, a_size, total_size;
  uint8_t* mem;

  assert(picture != NULL);

  WebPSafeFree(picture->memory_);
  WebPPictureResetBufferYUVA(picture);

  if (uv_csp != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }

  // alpha
  a_width = has_alpha ? width : 0;
  a_stride = a_width;
  y_size = (uint64_t)y_stride * height;
  uv_size = (uint64_t)uv_stride * uv_height;
  a_size =  (uint64_t)a_stride * height;

  total_size = y_size + a_size + 2 * uv_size;

  // Security and validation checks
  if (width <= 0 || height <= 0 ||           // luma/alpha param error
      uv_width <= 0 || uv_height <= 0) {     // u/v param error
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_BAD_DIMENSION);
  }
  // allocate a new buffer.
  mem = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*mem));
  if (mem == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }

  // From now on, we're in the clear, we can no longer fail...
  picture->memory_ = (void*)mem;
  picture->y_stride  = y_stride;
  picture->uv_stride = uv_stride;
  picture->a_stride  = a_stride;

  // TODO(skal): we could align the y/u/v planes and adjust stride.
  picture->y = mem;
  mem += y_size;

  picture->u = mem;
  mem += uv_size;
  picture->v = mem;
  mem += uv_size;

  if (a_size > 0) {
    picture->a = mem;
    mem += a_size;
  }
  (void)mem;  // makes the static analyzer happy
  return 1;
}

static const int kAlphaFix = 19;

#define kGamma 0.80      // for now we use a different gamma value than kGammaF
#define kGammaFix 12     // fixed-point precision for linear values
#define kGammaScale ((1 << kGammaFix) - 1)
#define kGammaTabFix 7   // fixed-point fractional bits precision
#define kGammaTabScale (1 << kGammaTabFix)
#define kGammaTabRounder (kGammaTabScale >> 1)
#define kGammaTabSize (1 << (kGammaFix - kGammaTabFix))

static int kLinearToGammaTab[kGammaTabSize + 1];
static uint16_t kGammaToLinearTab[256];
static volatile int kGammaTablesOk = 0;

#define SFIX 2                // fixed-point precision of RGB and Y/W
#define MAX_Y_T ((256 << SFIX) - 1)
#define kGammaF (1./0.45)
static uint32_t kLinearToGammaTabS[kGammaTabSize + 2];
#define GAMMA_TO_LINEAR_BITS 14
static uint32_t kGammaToLinearTabS[MAX_Y_T + 1];   // size scales with Y_FIX
static volatile int kGammaTablesSOk = 0;

static void InitGammaTablesS(void) {
  assert(2 * GAMMA_TO_LINEAR_BITS < 32);  // we use uint32_t intermediate values
  if (!kGammaTablesSOk) {
    int v;
    const double norm = 1. / MAX_Y_T;
    const double scale = 1. / kGammaTabSize;
    const double a = 0.09929682680944;
    const double thresh = 0.018053968510807;
    const double final_scale = 1 << GAMMA_TO_LINEAR_BITS;
    for (v = 0; v <= MAX_Y_T; ++v) {
      const double g = norm * v;
      double value;
      if (g <= thresh * 4.5) {
        value = g / 4.5;
      } else {
        const double a_rec = 1. / (1. + a);
        value = pow(a_rec * (g + a), kGammaF);
      }
      kGammaToLinearTabS[v] = (uint32_t)(value * final_scale + .5);
    }
    for (v = 0; v <= kGammaTabSize; ++v) {
      const double g = scale * v;
      double value;
      if (g <= thresh) {
        value = 4.5 * g;
      } else {
        value = (1. + a) * pow(g, 1. / kGammaF) - a;
      }
      // we already incorporate the 1/2 rounding constant here
      kLinearToGammaTabS[v] =
          (uint32_t)(MAX_Y_T * value) + (1 << GAMMA_TO_LINEAR_BITS >> 1);
    }
    // to prevent small rounding errors to cause read-overflow:
    kLinearToGammaTabS[kGammaTabSize + 1] = kLinearToGammaTabS[kGammaTabSize];
    kGammaTablesSOk = 1;
  }
}

static void InitGammaTables(void) {
  if (!kGammaTablesOk) {
    int v;
    const double scale = (double)(1 << kGammaTabFix) / kGammaScale;
    const double norm = 1. / 255.;
    for (v = 0; v <= 255; ++v) {
      kGammaToLinearTab[v] =
          (uint16_t)(pow(norm * v, kGamma) * kGammaScale + .5);
    }
    for (v = 0; v <= kGammaTabSize; ++v) {
      kLinearToGammaTab[v] = (int)(255. * pow(scale * v, 1. / kGamma) + .5);
    }
    kGammaTablesOk = 1;
  }
}

#define SAFE_ALLOC(W, H, T) ((T*)WebPSafeMalloc((W) * (H), sizeof(T)))

typedef int16_t fixed_t;      // signed type with extra SFIX precision for UV
typedef uint16_t fixed_y_t;   // unsigned type with extra SFIX precision for W

#define SHALF (1 << SFIX >> 1)

static fixed_y_t UpLift(uint8_t a) {  // 8bit -> SFIX
  return ((fixed_y_t)a << SFIX) | SHALF;
}

static void ImportOneRow(const uint8_t* const r_ptr,
                         const uint8_t* const g_ptr,
                         const uint8_t* const b_ptr,
                         int step,
                         int pic_width,
                         fixed_y_t* const dst) {
  int i;
  const int w = (pic_width + 1) & ~1;
  for (i = 0; i < pic_width; ++i) {
    const int off = i * step;
    dst[i + 0 * w] = UpLift(r_ptr[off]);
    dst[i + 1 * w] = UpLift(g_ptr[off]);
    dst[i + 2 * w] = UpLift(b_ptr[off]);
  }
  if (pic_width & 1) {  // replicate rightmost pixel
    dst[pic_width + 0 * w] = dst[pic_width + 0 * w - 1];
    dst[pic_width + 1 * w] = dst[pic_width + 1 * w - 1];
    dst[pic_width + 2 * w] = dst[pic_width + 2 * w - 1];
  }
}

enum {
  YUV_FIX = 16,                    // fixed-point precision for RGB->YUV
  YUV_HALF = 1 << (YUV_FIX - 1),

  YUV_FIX2 = 6,                   // fixed-point precision for YUV->RGB
  YUV_MASK2 = (256 << YUV_FIX2) - 1
};

static int RGBToGray(int r, int g, int b) {
  const int luma = 13933 * r + 46871 * g + 4732 * b + YUV_HALF;
  return (luma >> YUV_FIX);
}

static void StoreGray(const fixed_y_t* rgb, fixed_y_t* y, int w) {
  int i;
  for (i = 0; i < w; ++i) {
    y[i] = RGBToGray(rgb[0 * w + i], rgb[1 * w + i], rgb[2 * w + i]);
  }
}

static uint32_t GammaToLinearS(int v) {
  return kGammaToLinearTabS[v];
}

static uint32_t LinearToGammaS(uint32_t value) {
  // 'value' is in GAMMA_TO_LINEAR_BITS fractional precision
  const uint32_t v = value * kGammaTabSize;
  const uint32_t tab_pos = v >> GAMMA_TO_LINEAR_BITS;
  // fractional part, in GAMMA_TO_LINEAR_BITS fixed-point precision
  const uint32_t x = v - (tab_pos << GAMMA_TO_LINEAR_BITS);  // fractional part
  // v0 / v1 are in GAMMA_TO_LINEAR_BITS fixed-point precision (range [0..1])
  const uint32_t v0 = kLinearToGammaTabS[tab_pos + 0];
  const uint32_t v1 = kLinearToGammaTabS[tab_pos + 1];
  // Final interpolation. Note that rounding is already included.
  const uint32_t v2 = (v1 - v0) * x;    // note: v1 >= v0.
  const uint32_t result = v0 + (v2 >> GAMMA_TO_LINEAR_BITS);
  return result;
}

static void UpdateW(const fixed_y_t* src, fixed_y_t* dst, int w) {
  int i;
  for (i = 0; i < w; ++i) {
    const uint32_t R = GammaToLinearS(src[0 * w + i]);
    const uint32_t G = GammaToLinearS(src[1 * w + i]);
    const uint32_t B = GammaToLinearS(src[2 * w + i]);
    const uint32_t Y = RGBToGray(R, G, B);
    dst[i] = (fixed_y_t)LinearToGammaS(Y);
  }
}

static uint32_t ScaleDown(int a, int b, int c, int d) {
  const uint32_t A = GammaToLinearS(a);
  const uint32_t B = GammaToLinearS(b);
  const uint32_t C = GammaToLinearS(c);
  const uint32_t D = GammaToLinearS(d);
  return LinearToGammaS((A + B + C + D + 2) >> 2);
}

static void UpdateChroma(const fixed_y_t* src1, const fixed_y_t* src2,
                         fixed_t* dst, int uv_w) {
  int i;
  for (i = 0; i < uv_w; ++i) {
    const int r = ScaleDown(src1[0 * uv_w + 0], src1[0 * uv_w + 1],
                            src2[0 * uv_w + 0], src2[0 * uv_w + 1]);
    const int g = ScaleDown(src1[2 * uv_w + 0], src1[2 * uv_w + 1],
                            src2[2 * uv_w + 0], src2[2 * uv_w + 1]);
    const int b = ScaleDown(src1[4 * uv_w + 0], src1[4 * uv_w + 1],
                            src2[4 * uv_w + 0], src2[4 * uv_w + 1]);
    const int W = RGBToGray(r, g, b);
    dst[0 * uv_w] = (fixed_t)(r - W);
    dst[1 * uv_w] = (fixed_t)(g - W);
    dst[2 * uv_w] = (fixed_t)(b - W);
    dst  += 1;
    src1 += 2;
    src2 += 2;
  }
}

static const int kNumIterations = 4;

static fixed_y_t clip_y(int y) {
  return (!(y & ~MAX_Y_T)) ? (fixed_y_t)y : (y < 0) ? 0 : MAX_Y_T;
}

static fixed_y_t Filter2(int A, int B, int W0) {
  const int v0 = (A * 3 + B + 2) >> 2;
  return clip_y(v0 + W0);
}

static void SharpYUVFilterRow_C(const int16_t* A, const int16_t* B, int len,
                                const uint16_t* best_y, uint16_t* out) {
  int i;
  for (i = 0; i < len; ++i, ++A, ++B) {
    const int v0 = (A[0] * 9 + A[1] * 3 + B[0] * 3 + B[1] + 8) >> 4;
    const int v1 = (A[1] * 9 + A[0] * 3 + B[1] * 3 + B[0] + 8) >> 4;
    out[2 * i + 0] = clip_y(best_y[2 * i + 0] + v0);
    out[2 * i + 1] = clip_y(best_y[2 * i + 1] + v1);
  }
}

static void InterpolateTwoRows(const fixed_y_t* const best_y,
                               const fixed_t* prev_uv,
                               const fixed_t* cur_uv,
                               const fixed_t* next_uv,
                               int w,
                               fixed_y_t* out1,
                               fixed_y_t* out2) {
  const int uv_w = w >> 1;
  const int len = (w - 1) >> 1;   // length to filter
  int k = 3;
  while (k-- > 0) {   // process each R/G/B segments in turn
    // special boundary case for i==0
    out1[0] = Filter2(cur_uv[0], prev_uv[0], best_y[0]);
    out2[0] = Filter2(cur_uv[0], next_uv[0], best_y[w]);

    SharpYUVFilterRow_C(cur_uv, prev_uv, len, best_y + 0 + 1, out1 + 1);
    SharpYUVFilterRow_C(cur_uv, next_uv, len, best_y + w + 1, out2 + 1);

    // special boundary case for i == w - 1 when w is even
    if (!(w & 1)) {
      out1[w - 1] = Filter2(cur_uv[uv_w - 1], prev_uv[uv_w - 1],
                            best_y[w - 1 + 0]);
      out2[w - 1] = Filter2(cur_uv[uv_w - 1], next_uv[uv_w - 1],
                            best_y[w - 1 + w]);
    }
    out1 += w;
    out2 += w;
    prev_uv += uv_w;
    cur_uv  += uv_w;
    next_uv += uv_w;
  }
}

static uint64_t SharpYUVUpdateY_C(const uint16_t* ref, const uint16_t* src,
                                  uint16_t* dst, int len) {
  uint64_t diff = 0;
  int i;
  for (i = 0; i < len; ++i) {
    const int diff_y = ref[i] - src[i];
    const int new_y = (int)dst[i] + diff_y;
    dst[i] = clip_y(new_y);
    diff += (uint64_t)abs(diff_y);
  }
  return diff;
}

static void SharpYUVUpdateRGB_C(const int16_t* ref, const int16_t* src,
                                int16_t* dst, int len) {
  int i;
  for (i = 0; i < len; ++i) {
    const int diff_uv = ref[i] - src[i];
    dst[i] += diff_uv;
  }
}

#define SROUNDER (1 << (YUV_FIX + SFIX - 1))

static uint8_t clip_8b(fixed_t v) {
  return (!(v & ~0xff)) ? (uint8_t)v : (v < 0) ? 0u : 255u;
}

static uint8_t ConvertRGBToY(int r, int g, int b) {
  const int luma = 16839 * r + 33059 * g + 6420 * b + SROUNDER;
  return clip_8b(16 + (luma >> (YUV_FIX + SFIX)));
}

static uint8_t ConvertRGBToU(int r, int g, int b) {
  const int u =  -9719 * r - 19081 * g + 28800 * b + SROUNDER;
  return clip_8b(128 + (u >> (YUV_FIX + SFIX)));
}

static uint8_t ConvertRGBToV(int r, int g, int b) {
  const int v = +28800 * r - 24116 * g -  4684 * b + SROUNDER;
  return clip_8b(128 + (v >> (YUV_FIX + SFIX)));
}

static int ConvertWRGBToYUV(const fixed_y_t* best_y, const fixed_t* best_uv,
                            WebPPicture* const picture) {
  int i, j;
  uint8_t* dst_y = picture->y;
  uint8_t* dst_u = picture->u;
  uint8_t* dst_v = picture->v;
  const fixed_t* const best_uv_base = best_uv;
  const int w = (picture->width + 1) & ~1;
  const int h = (picture->height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  for (best_uv = best_uv_base, j = 0; j < picture->height; ++j) {
    for (i = 0; i < picture->width; ++i) {
      const int off = (i >> 1);
      const int W = best_y[i];
      const int r = best_uv[off + 0 * uv_w] + W;
      const int g = best_uv[off + 1 * uv_w] + W;
      const int b = best_uv[off + 2 * uv_w] + W;
      dst_y[i] = ConvertRGBToY(r, g, b);
    }
    best_y += w;
    best_uv += (j & 1) * 3 * uv_w;
    dst_y += picture->y_stride;
  }
  for (best_uv = best_uv_base, j = 0; j < uv_h; ++j) {
    for (i = 0; i < uv_w; ++i) {
      const int off = i;
      const int r = best_uv[off + 0 * uv_w];
      const int g = best_uv[off + 1 * uv_w];
      const int b = best_uv[off + 2 * uv_w];
      dst_u[i] = ConvertRGBToU(r, g, b);
      dst_v[i] = ConvertRGBToV(r, g, b);
    }
    best_uv += 3 * uv_w;
    dst_u += picture->uv_stride;
    dst_v += picture->uv_stride;
  }
  return 1;
}

static int PreprocessARGB(const uint8_t* r_ptr,
                          const uint8_t* g_ptr,
                          const uint8_t* b_ptr,
                          int step, int rgb_stride,
                          WebPPicture* const picture) {
  // we expand the right/bottom border if needed
  const int w = (picture->width + 1) & ~1;
  const int h = (picture->height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  uint64_t prev_diff_y_sum = ~0;
  int j, iter;

  // TODO(skal): allocate one big memory chunk. But for now, it's easier
  // for valgrind debugging to have several chunks.
  fixed_y_t* const tmp_buffer = SAFE_ALLOC(w * 3, 2, fixed_y_t);   // scratch
  fixed_y_t* const best_y_base = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const target_y_base = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const best_rgb_y = SAFE_ALLOC(w, 2, fixed_y_t);
  fixed_t* const best_uv_base = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const target_uv_base = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const best_rgb_uv = SAFE_ALLOC(uv_w * 3, 1, fixed_t);
  fixed_y_t* best_y = best_y_base;
  fixed_y_t* target_y = target_y_base;
  fixed_t* best_uv = best_uv_base;
  fixed_t* target_uv = target_uv_base;
  const uint64_t diff_y_threshold = (uint64_t)(3.0 * w * h);
  int ok;

  if (best_y_base == NULL || best_uv_base == NULL ||
      target_y_base == NULL || target_uv_base == NULL ||
      best_rgb_y == NULL || best_rgb_uv == NULL ||
      tmp_buffer == NULL) {
    ok = WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    goto End;
  }
  assert(picture->width >= kMinDimensionIterativeConversion);
  assert(picture->height >= kMinDimensionIterativeConversion);

  // Import RGB samples to W/RGB representation.
  for (j = 0; j < picture->height; j += 2) {
    const int is_last_row = (j == picture->height - 1);
    fixed_y_t* const src1 = tmp_buffer + 0 * w;
    fixed_y_t* const src2 = tmp_buffer + 3 * w;

    // prepare two rows of input
    ImportOneRow(r_ptr, g_ptr, b_ptr, step, picture->width, src1);
    if (!is_last_row) {
      ImportOneRow(r_ptr + rgb_stride, g_ptr + rgb_stride, b_ptr + rgb_stride,
                   step, picture->width, src2);
    } else {
      memcpy(src2, src1, 3 * w * sizeof(*src2));
    }
    StoreGray(src1, best_y + 0, w);
    StoreGray(src2, best_y + w, w);

    UpdateW(src1, target_y, w);
    UpdateW(src2, target_y + w, w);
    UpdateChroma(src1, src2, target_uv, uv_w);
    memcpy(best_uv, target_uv, 3 * uv_w * sizeof(*best_uv));
    best_y += 2 * w;
    best_uv += 3 * uv_w;
    target_y += 2 * w;
    target_uv += 3 * uv_w;
    r_ptr += 2 * rgb_stride;
    g_ptr += 2 * rgb_stride;
    b_ptr += 2 * rgb_stride;
  }

  // Iterate and resolve clipping conflicts.
  for (iter = 0; iter < kNumIterations; ++iter) {
    const fixed_t* cur_uv = best_uv_base;
    const fixed_t* prev_uv = best_uv_base;
    uint64_t diff_y_sum = 0;

    best_y = best_y_base;
    best_uv = best_uv_base;
    target_y = target_y_base;
    target_uv = target_uv_base;
    for (j = 0; j < h; j += 2) {
      fixed_y_t* const src1 = tmp_buffer + 0 * w;
      fixed_y_t* const src2 = tmp_buffer + 3 * w;
      {
        const fixed_t* const next_uv = cur_uv + ((j < h - 2) ? 3 * uv_w : 0);
        InterpolateTwoRows(best_y, prev_uv, cur_uv, next_uv, w, src1, src2);
        prev_uv = cur_uv;
        cur_uv = next_uv;
      }

      UpdateW(src1, best_rgb_y + 0 * w, w);
      UpdateW(src2, best_rgb_y + 1 * w, w);
      UpdateChroma(src1, src2, best_rgb_uv, uv_w);

      // update two rows of Y and one row of RGB
      diff_y_sum += SharpYUVUpdateY_C(target_y, best_rgb_y, best_y, 2 * w);
      SharpYUVUpdateRGB_C(target_uv, best_rgb_uv, best_uv, 3 * uv_w);

      best_y += 2 * w;
      best_uv += 3 * uv_w;
      target_y += 2 * w;
      target_uv += 3 * uv_w;
    }
    // test exit condition
    if (iter > 0) {
      if (diff_y_sum < diff_y_threshold) break;
      if (diff_y_sum > prev_diff_y_sum) break;
    }
    prev_diff_y_sum = diff_y_sum;
  }
  // final reconstruction
  ok = ConvertWRGBToYUV(best_y_base, best_uv_base, picture);

 End:
  WebPSafeFree(best_y_base);
  WebPSafeFree(best_uv_base);
  WebPSafeFree(target_y_base);
  WebPSafeFree(target_uv_base);
  WebPSafeFree(best_rgb_y);
  WebPSafeFree(best_rgb_uv);
  WebPSafeFree(tmp_buffer);
  return ok;
}

#undef SAFE_ALLOC

static int ExtractAlpha_C(const uint8_t* argb, int argb_stride,
                          int width, int height,
                          uint8_t* alpha, int alpha_stride) {
  uint8_t alpha_mask = 0xff;
  int i, j;

  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      const uint8_t alpha_value = argb[4 * i];
      alpha[i] = alpha_value;
      alpha_mask &= alpha_value;
    }
    argb += argb_stride;
    alpha += alpha_stride;
  }
  return (alpha_mask == 0xff);
}

#define VP8_RANDOM_TABLE_SIZE 55

typedef struct {
  int index1_, index2_;
  uint32_t tab_[VP8_RANDOM_TABLE_SIZE];
  int amp_;
} VP8Random;

#define VP8_RANDOM_DITHER_FIX 8   // fixed-point precision for dithering

// 31b-range values
static const uint32_t kRandomTable[VP8_RANDOM_TABLE_SIZE] = {
  0x0de15230, 0x03b31886, 0x775faccb, 0x1c88626a, 0x68385c55, 0x14b3b828,
  0x4a85fef8, 0x49ddb84b, 0x64fcf397, 0x5c550289, 0x4a290000, 0x0d7ec1da,
  0x5940b7ab, 0x5492577d, 0x4e19ca72, 0x38d38c69, 0x0c01ee65, 0x32a1755f,
  0x5437f652, 0x5abb2c32, 0x0faa57b1, 0x73f533e7, 0x685feeda, 0x7563cce2,
  0x6e990e83, 0x4730a7ed, 0x4fc0d9c6, 0x496b153c, 0x4f1403fa, 0x541afb0c,
  0x73990b32, 0x26d7cb1c, 0x6fcc3706, 0x2cbb77d8, 0x75762f2a, 0x6425ccdd,
  0x24b35461, 0x0a7d8715, 0x220414a8, 0x141ebf67, 0x56b41583, 0x73e502e3,
  0x44cab16f, 0x28264d42, 0x73baaefb, 0x0a50ebed, 0x1d6ab6fb, 0x0d3ad40b,
  0x35db3b68, 0x2b081e83, 0x77ce6b95, 0x5181e5f0, 0x78853bbc, 0x009f9494,
  0x27e5ed3c
};

void VP8InitRandom(VP8Random* const rg, float dithering) {
  memcpy(rg->tab_, kRandomTable, sizeof(rg->tab_));
  rg->index1_ = 0;
  rg->index2_ = 31;
  rg->amp_ = (dithering < 0.0) ? 0
           : (dithering > 1.0) ? (1 << VP8_RANDOM_DITHER_FIX)
           : (uint32_t)((1 << VP8_RANDOM_DITHER_FIX) * dithering);
}

static int VP8RGBToY(int r, int g, int b, int rounding) {
  const int luma = 16839 * r + 33059 * g + 6420 * b;
  return (luma + rounding + (16 << YUV_FIX)) >> YUV_FIX;  // no need to clip
}

static void ConvertRGB24ToY_C(const uint8_t* rgb, uint8_t* y, int width) {
  int i;
  for (i = 0; i < width; ++i, rgb += 3) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_C(const uint8_t* bgr, uint8_t* y, int width) {
  int i;
  for (i = 0; i < width; ++i, bgr += 3) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

static int VP8RandomBits2(VP8Random* const rg, int num_bits,
                                      int amp) {
  int diff;
  assert(num_bits + VP8_RANDOM_DITHER_FIX <= 31);
  diff = rg->tab_[rg->index1_] - rg->tab_[rg->index2_];
  if (diff < 0) diff += (1u << 31);
  rg->tab_[rg->index1_] = diff;
  if (++rg->index1_ == VP8_RANDOM_TABLE_SIZE) rg->index1_ = 0;
  if (++rg->index2_ == VP8_RANDOM_TABLE_SIZE) rg->index2_ = 0;
  // sign-extend, 0-center
  diff = (int)((uint32_t)diff << 1) >> (32 - num_bits);
  diff = (diff * amp) >> VP8_RANDOM_DITHER_FIX;  // restrict range
  diff += 1 << (num_bits - 1);                   // shift back to 0.5-center
  return diff;
}

static int VP8RandomBits(VP8Random* const rg, int num_bits) {
  return VP8RandomBits2(rg, num_bits, rg->amp_);
}

static int RGBToY(int r, int g, int b, VP8Random* const rg) {
  return (rg == NULL) ? VP8RGBToY(r, g, b, YUV_HALF)
                      : VP8RGBToY(r, g, b, VP8RandomBits(rg, YUV_FIX));
}

static void ConvertRowToY(const uint8_t* const r_ptr,
                                      const uint8_t* const g_ptr,
                                      const uint8_t* const b_ptr,
                                      int step,
                                      uint8_t* const dst_y,
                                      int width,
                                      VP8Random* const rg) {
  int i, j;
  for (i = 0, j = 0; i < width; i += 1, j += step) {
    dst_y[i] = RGBToY(r_ptr[j], g_ptr[j], b_ptr[j], rg);
  }
}

static int Interpolate(int v) {
  const int tab_pos = v >> (kGammaTabFix + 2);    // integer part
  const int x = v & ((kGammaTabScale << 2) - 1);  // fractional part
  const int v0 = kLinearToGammaTab[tab_pos];
  const int v1 = kLinearToGammaTab[tab_pos + 1];
  const int y = v1 * x + v0 * ((kGammaTabScale << 2) - x);   // interpolate
  assert(tab_pos + 1 < kGammaTabSize + 1);
  return y;
}

static int LinearToGamma(uint32_t base_value, int shift) {
  const int y = Interpolate(base_value << shift);   // final uplifted value
  return (y + kGammaTabRounder) >> kGammaTabFix;    // descale
}

static uint32_t GammaToLinear(uint8_t v) {
  return kGammaToLinearTab[v];
}

#define SUM4(ptr, step) LinearToGamma(                     \
    GammaToLinear((ptr)[0]) +                              \
    GammaToLinear((ptr)[(step)]) +                         \
    GammaToLinear((ptr)[rgb_stride]) +                     \
    GammaToLinear((ptr)[rgb_stride + (step)]), 0)          \

#define SUM2(ptr) \
    LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[rgb_stride]), 1)

static void AccumulateRGB(const uint8_t* const r_ptr,
                                      const uint8_t* const g_ptr,
                                      const uint8_t* const b_ptr,
                                      int step, int rgb_stride,
                                      uint16_t* dst, int width) {
  int i, j;
  for (i = 0, j = 0; i < (width >> 1); i += 1, j += 2 * step, dst += 4) {
    dst[0] = SUM4(r_ptr + j, step);
    dst[1] = SUM4(g_ptr + j, step);
    dst[2] = SUM4(b_ptr + j, step);
  }
  if (width & 1) {
    dst[0] = SUM2(r_ptr + j);
    dst[1] = SUM2(g_ptr + j);
    dst[2] = SUM2(b_ptr + j);
  }
}

#define SUM2ALPHA(ptr) ((ptr)[0] + (ptr)[rgb_stride])
#define SUM4ALPHA(ptr) (SUM2ALPHA(ptr) + SUM2ALPHA((ptr) + 4))

#define USE_INVERSE_ALPHA_TABLE

static const uint32_t kInvAlpha[4 * 0xff + 1] = {
  0,  /* alpha = 0 */
  524288, 262144, 174762, 131072, 104857, 87381, 74898, 65536,
  58254, 52428, 47662, 43690, 40329, 37449, 34952, 32768,
  30840, 29127, 27594, 26214, 24966, 23831, 22795, 21845,
  20971, 20164, 19418, 18724, 18078, 17476, 16912, 16384,
  15887, 15420, 14979, 14563, 14169, 13797, 13443, 13107,
  12787, 12483, 12192, 11915, 11650, 11397, 11155, 10922,
  10699, 10485, 10280, 10082, 9892, 9709, 9532, 9362,
  9198, 9039, 8886, 8738, 8594, 8456, 8322, 8192,
  8065, 7943, 7825, 7710, 7598, 7489, 7384, 7281,
  7182, 7084, 6990, 6898, 6808, 6721, 6636, 6553,
  6472, 6393, 6316, 6241, 6168, 6096, 6026, 5957,
  5890, 5825, 5761, 5698, 5637, 5577, 5518, 5461,
  5405, 5349, 5295, 5242, 5190, 5140, 5090, 5041,
  4993, 4946, 4899, 4854, 4809, 4766, 4723, 4681,
  4639, 4599, 4559, 4519, 4481, 4443, 4405, 4369,
  4332, 4297, 4262, 4228, 4194, 4161, 4128, 4096,
  4064, 4032, 4002, 3971, 3942, 3912, 3883, 3855,
  3826, 3799, 3771, 3744, 3718, 3692, 3666, 3640,
  3615, 3591, 3566, 3542, 3518, 3495, 3472, 3449,
  3426, 3404, 3382, 3360, 3339, 3318, 3297, 3276,
  3256, 3236, 3216, 3196, 3177, 3158, 3139, 3120,
  3102, 3084, 3066, 3048, 3030, 3013, 2995, 2978,
  2962, 2945, 2928, 2912, 2896, 2880, 2864, 2849,
  2833, 2818, 2803, 2788, 2774, 2759, 2744, 2730,
  2716, 2702, 2688, 2674, 2661, 2647, 2634, 2621,
  2608, 2595, 2582, 2570, 2557, 2545, 2532, 2520,
  2508, 2496, 2484, 2473, 2461, 2449, 2438, 2427,
  2416, 2404, 2394, 2383, 2372, 2361, 2351, 2340,
  2330, 2319, 2309, 2299, 2289, 2279, 2269, 2259,
  2250, 2240, 2231, 2221, 2212, 2202, 2193, 2184,
  2175, 2166, 2157, 2148, 2139, 2131, 2122, 2114,
  2105, 2097, 2088, 2080, 2072, 2064, 2056, 2048,
  2040, 2032, 2024, 2016, 2008, 2001, 1993, 1985,
  1978, 1971, 1963, 1956, 1949, 1941, 1934, 1927,
  1920, 1913, 1906, 1899, 1892, 1885, 1879, 1872,
  1865, 1859, 1852, 1846, 1839, 1833, 1826, 1820,
  1814, 1807, 1801, 1795, 1789, 1783, 1777, 1771,
  1765, 1759, 1753, 1747, 1741, 1736, 1730, 1724,
  1718, 1713, 1707, 1702, 1696, 1691, 1685, 1680,
  1675, 1669, 1664, 1659, 1653, 1648, 1643, 1638,
  1633, 1628, 1623, 1618, 1613, 1608, 1603, 1598,
  1593, 1588, 1583, 1579, 1574, 1569, 1565, 1560,
  1555, 1551, 1546, 1542, 1537, 1533, 1528, 1524,
  1519, 1515, 1510, 1506, 1502, 1497, 1493, 1489,
  1485, 1481, 1476, 1472, 1468, 1464, 1460, 1456,
  1452, 1448, 1444, 1440, 1436, 1432, 1428, 1424,
  1420, 1416, 1413, 1409, 1405, 1401, 1398, 1394,
  1390, 1387, 1383, 1379, 1376, 1372, 1368, 1365,
  1361, 1358, 1354, 1351, 1347, 1344, 1340, 1337,
  1334, 1330, 1327, 1323, 1320, 1317, 1314, 1310,
  1307, 1304, 1300, 1297, 1294, 1291, 1288, 1285,
  1281, 1278, 1275, 1272, 1269, 1266, 1263, 1260,
  1257, 1254, 1251, 1248, 1245, 1242, 1239, 1236,
  1233, 1230, 1227, 1224, 1222, 1219, 1216, 1213,
  1210, 1208, 1205, 1202, 1199, 1197, 1194, 1191,
  1188, 1186, 1183, 1180, 1178, 1175, 1172, 1170,
  1167, 1165, 1162, 1159, 1157, 1154, 1152, 1149,
  1147, 1144, 1142, 1139, 1137, 1134, 1132, 1129,
  1127, 1125, 1122, 1120, 1117, 1115, 1113, 1110,
  1108, 1106, 1103, 1101, 1099, 1096, 1094, 1092,
  1089, 1087, 1085, 1083, 1081, 1078, 1076, 1074,
  1072, 1069, 1067, 1065, 1063, 1061, 1059, 1057,
  1054, 1052, 1050, 1048, 1046, 1044, 1042, 1040,
  1038, 1036, 1034, 1032, 1030, 1028, 1026, 1024,
  1022, 1020, 1018, 1016, 1014, 1012, 1010, 1008,
  1006, 1004, 1002, 1000, 998, 996, 994, 992,
  991, 989, 987, 985, 983, 981, 979, 978,
  976, 974, 972, 970, 969, 967, 965, 963,
  961, 960, 958, 956, 954, 953, 951, 949,
  948, 946, 944, 942, 941, 939, 937, 936,
  934, 932, 931, 929, 927, 926, 924, 923,
  921, 919, 918, 916, 914, 913, 911, 910,
  908, 907, 905, 903, 902, 900, 899, 897,
  896, 894, 893, 891, 890, 888, 887, 885,
  884, 882, 881, 879, 878, 876, 875, 873,
  872, 870, 869, 868, 866, 865, 863, 862,
  860, 859, 858, 856, 855, 853, 852, 851,
  849, 848, 846, 845, 844, 842, 841, 840,
  838, 837, 836, 834, 833, 832, 830, 829,
  828, 826, 825, 824, 823, 821, 820, 819,
  817, 816, 815, 814, 812, 811, 810, 809,
  807, 806, 805, 804, 802, 801, 800, 799,
  798, 796, 795, 794, 793, 791, 790, 789,
  788, 787, 786, 784, 783, 782, 781, 780,
  779, 777, 776, 775, 774, 773, 772, 771,
  769, 768, 767, 766, 765, 764, 763, 762,
  760, 759, 758, 757, 756, 755, 754, 753,
  752, 751, 750, 748, 747, 746, 745, 744,
  743, 742, 741, 740, 739, 738, 737, 736,
  735, 734, 733, 732, 731, 730, 729, 728,
  727, 726, 725, 724, 723, 722, 721, 720,
  719, 718, 717, 716, 715, 714, 713, 712,
  711, 710, 709, 708, 707, 706, 705, 704,
  703, 702, 701, 700, 699, 699, 698, 697,
  696, 695, 694, 693, 692, 691, 690, 689,
  688, 688, 687, 686, 685, 684, 683, 682,
  681, 680, 680, 679, 678, 677, 676, 675,
  674, 673, 673, 672, 671, 670, 669, 668,
  667, 667, 666, 665, 664, 663, 662, 661,
  661, 660, 659, 658, 657, 657, 656, 655,
  654, 653, 652, 652, 651, 650, 649, 648,
  648, 647, 646, 645, 644, 644, 643, 642,
  641, 640, 640, 639, 638, 637, 637, 636,
  635, 634, 633, 633, 632, 631, 630, 630,
  629, 628, 627, 627, 626, 625, 624, 624,
  623, 622, 621, 621, 620, 619, 618, 618,
  617, 616, 616, 615, 614, 613, 613, 612,
  611, 611, 610, 609, 608, 608, 607, 606,
  606, 605, 604, 604, 603, 602, 601, 601,
  600, 599, 599, 598, 597, 597, 596, 595,
  595, 594, 593, 593, 592, 591, 591, 590,
  589, 589, 588, 587, 587, 586, 585, 585,
  584, 583, 583, 582, 581, 581, 580, 579,
  579, 578, 578, 577, 576, 576, 575, 574,
  574, 573, 572, 572, 571, 571, 570, 569,
  569, 568, 568, 567, 566, 566, 565, 564,
  564, 563, 563, 562, 561, 561, 560, 560,
  559, 558, 558, 557, 557, 556, 555, 555,
  554, 554, 553, 553, 552, 551, 551, 550,
  550, 549, 548, 548, 547, 547, 546, 546,
  545, 544, 544, 543, 543, 542, 542, 541,
  541, 540, 539, 539, 538, 538, 537, 537,
  536, 536, 535, 534, 534, 533, 533, 532,
  532, 531, 531, 530, 530, 529, 529, 528,
  527, 527, 526, 526, 525, 525, 524, 524,
  523, 523, 522, 522, 521, 521, 520, 520,
  519, 519, 518, 518, 517, 517, 516, 516,
  515, 515, 514, 514
};

#define DIVIDE_BY_ALPHA(sum, a)  (((sum) * kInvAlpha[(a)]) >> (kAlphaFix - 2))

static int LinearToGammaWeighted(const uint8_t* src,
                                             const uint8_t* a_ptr,
                                             uint32_t total_a, int step,
                                             int rgb_stride) {
  const uint32_t sum =
      a_ptr[0] * GammaToLinear(src[0]) +
      a_ptr[step] * GammaToLinear(src[step]) +
      a_ptr[rgb_stride] * GammaToLinear(src[rgb_stride]) +
      a_ptr[rgb_stride + step] * GammaToLinear(src[rgb_stride + step]);
  assert(total_a > 0 && total_a <= 4 * 0xff);
#if defined(USE_INVERSE_ALPHA_TABLE)
  assert((uint64_t)sum * kInvAlpha[total_a] < ((uint64_t)1 << 32));
#endif
  return LinearToGamma(DIVIDE_BY_ALPHA(sum, total_a), 0);
}

static void AccumulateRGBA(const uint8_t* const r_ptr,
                                       const uint8_t* const g_ptr,
                                       const uint8_t* const b_ptr,
                                       const uint8_t* const a_ptr,
                                       int rgb_stride,
                                       uint16_t* dst, int width) {
  int i, j;
  // we loop over 2x2 blocks and produce one R/G/B/A value for each.
  for (i = 0, j = 0; i < (width >> 1); i += 1, j += 2 * 4, dst += 4) {
    const uint32_t a = SUM4ALPHA(a_ptr + j);
    int r, g, b;
    if (a == 4 * 0xff || a == 0) {
      r = SUM4(r_ptr + j, 4);
      g = SUM4(g_ptr + j, 4);
      b = SUM4(b_ptr + j, 4);
    } else {
      r = LinearToGammaWeighted(r_ptr + j, a_ptr + j, a, 4, rgb_stride);
      g = LinearToGammaWeighted(g_ptr + j, a_ptr + j, a, 4, rgb_stride);
      b = LinearToGammaWeighted(b_ptr + j, a_ptr + j, a, 4, rgb_stride);
    }
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
  }
  if (width & 1) {
    const uint32_t a = 2u * SUM2ALPHA(a_ptr + j);
    int r, g, b;
    if (a == 4 * 0xff || a == 0) {
      r = SUM2(r_ptr + j);
      g = SUM2(g_ptr + j);
      b = SUM2(b_ptr + j);
    } else {
      r = LinearToGammaWeighted(r_ptr + j, a_ptr + j, a, 0, rgb_stride);
      g = LinearToGammaWeighted(g_ptr + j, a_ptr + j, a, 0, rgb_stride);
      b = LinearToGammaWeighted(b_ptr + j, a_ptr + j, a, 0, rgb_stride);
    }
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
  }
}

static int VP8ClipUV(int uv, int rounding) {
  uv = (uv + rounding + (128 << (YUV_FIX + 2))) >> (YUV_FIX + 2);
  return ((uv & ~0xff) == 0) ? uv : (uv < 0) ? 0 : 255;
}

static int VP8RGBToU(int r, int g, int b, int rounding) {
  const int u = -9719 * r - 19081 * g + 28800 * b;
  return VP8ClipUV(u, rounding);
}

static int VP8RGBToV(int r, int g, int b, int rounding) {
  const int v = +28800 * r - 24116 * g - 4684 * b;
  return VP8ClipUV(v, rounding);
}

void WebPConvertRGBA32ToUV_C(const uint16_t* rgb,
                             uint8_t* u, uint8_t* v, int width) {
  int i;
  for (i = 0; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    u[i] = VP8RGBToU(r, g, b, YUV_HALF << 2);
    v[i] = VP8RGBToV(r, g, b, YUV_HALF << 2);
  }
}

static int RGBToU(int r, int g, int b, VP8Random* const rg) {
  return (rg == NULL) ? VP8RGBToU(r, g, b, YUV_HALF << 2)
                      : VP8RGBToU(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
}

static int RGBToV(int r, int g, int b, VP8Random* const rg) {
  return (rg == NULL) ? VP8RGBToV(r, g, b, YUV_HALF << 2)
                      : VP8RGBToV(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
}

static void ConvertRowsToUV(const uint16_t* rgb,
                                        uint8_t* const dst_u,
                                        uint8_t* const dst_v,
                                        int width,
                                        VP8Random* const rg) {
  int i;
  for (i = 0; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    dst_u[i] = RGBToU(r, g, b, rg);
    dst_v[i] = RGBToV(r, g, b, rg);
  }
}

static int ImportYUVAFromRGBA(const uint8_t* r_ptr,
                              const uint8_t* g_ptr,
                              const uint8_t* b_ptr,
                              const uint8_t* a_ptr,
                              int step,         // bytes per pixel
                              int rgb_stride,   // bytes per scanline
                              float dithering,
                              int use_iterative_conversion,
                              WebPPicture* const picture) {
  int y;
  const int width = picture->width;
  const int height = picture->height;
  const int has_alpha = CheckNonOpaque(a_ptr, width, height, step, rgb_stride);
  const int is_rgb = (r_ptr < b_ptr);  // otherwise it's bgr

  picture->colorspace = has_alpha ? WEBP_YUV420A : WEBP_YUV420;
  picture->use_argb = 0;

  // disable smart conversion if source is too small (overkill).
  if (width < kMinDimensionIterativeConversion ||
      height < kMinDimensionIterativeConversion) {
    use_iterative_conversion = 0;
  }

  if (!WebPPictureAllocYUVA(picture, width, height)) {
    return 0;
  }
  if (has_alpha) {
    assert(step == 4);
    assert(kAlphaFix + kGammaFix <= 31);
  }

  if (use_iterative_conversion) {
    InitGammaTablesS();
    if (!PreprocessARGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, picture)) {
      return 0;
    }
    if (has_alpha) {
    	ExtractAlpha_C(a_ptr, rgb_stride, width, height,
                       picture->a, picture->a_stride);
    }
  } else {
    const int uv_width = (width + 1) >> 1;
    int use_dsp = (step == 3);  // use special function in this case
    // temporary storage for accumulated R/G/B values during conversion to U/V
    uint16_t* const tmp_rgb =
        (uint16_t*)WebPSafeMalloc(4 * uv_width, sizeof(*tmp_rgb));
    uint8_t* dst_y = picture->y;
    uint8_t* dst_u = picture->u;
    uint8_t* dst_v = picture->v;
    uint8_t* dst_a = picture->a;

    VP8Random base_rg;
    VP8Random* rg = NULL;
    if (dithering > 0.) {
      VP8InitRandom(&base_rg, dithering);
      rg = &base_rg;
      use_dsp = 0;   // can't use dsp in this case
    }

    InitGammaTables();

    if (tmp_rgb == NULL) return 0;  // malloc error

    // Downsample Y/U/V planes, two rows at a time
    for (y = 0; y < (height >> 1); ++y) {
      int rows_have_alpha = has_alpha;
      if (use_dsp) {
        if (is_rgb) {
        	ConvertRGB24ToY_C(r_ptr, dst_y, width);
        	ConvertRGB24ToY_C(r_ptr + rgb_stride,
                              dst_y + picture->y_stride, width);
        } else {
        	ConvertBGR24ToY_C(b_ptr, dst_y, width);
        	ConvertBGR24ToY_C(b_ptr + rgb_stride,
                              dst_y + picture->y_stride, width);
        }
      } else {
        ConvertRowToY(r_ptr, g_ptr, b_ptr, step, dst_y, width, rg);
        ConvertRowToY(r_ptr + rgb_stride,
                      g_ptr + rgb_stride,
                      b_ptr + rgb_stride, step,
                      dst_y + picture->y_stride, width, rg);
      }
      dst_y += 2 * picture->y_stride;
      if (has_alpha) {
        rows_have_alpha &= !ExtractAlpha_C(a_ptr, rgb_stride, width, 2,
                                             dst_a, picture->a_stride);
        dst_a += 2 * picture->a_stride;
      }
      // Collect averaged R/G/B(/A)
      if (!rows_have_alpha) {
        AccumulateRGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, tmp_rgb, width);
      } else {
        AccumulateRGBA(r_ptr, g_ptr, b_ptr, a_ptr, rgb_stride, tmp_rgb, width);
      }
      // Convert to U/V
      if (rg == NULL) {
    	  WebPConvertRGBA32ToUV_C(tmp_rgb, dst_u, dst_v, uv_width);
      } else {
        ConvertRowsToUV(tmp_rgb, dst_u, dst_v, uv_width, rg);
      }
      dst_u += picture->uv_stride;
      dst_v += picture->uv_stride;
      r_ptr += 2 * rgb_stride;
      b_ptr += 2 * rgb_stride;
      g_ptr += 2 * rgb_stride;
      if (has_alpha) a_ptr += 2 * rgb_stride;
    }
    if (height & 1) {    // extra last row
      int row_has_alpha = has_alpha;
      if (use_dsp) {
        if (r_ptr < b_ptr) {
        	ConvertRGB24ToY_C(r_ptr, dst_y, width);
        } else {
        	ConvertBGR24ToY_C(b_ptr, dst_y, width);
        }
      } else {
        ConvertRowToY(r_ptr, g_ptr, b_ptr, step, dst_y, width, rg);
      }
      if (row_has_alpha) {
        row_has_alpha &= !ExtractAlpha_C(a_ptr, 0, width, 1, dst_a, 0);
      }
      // Collect averaged R/G/B(/A)
      if (!row_has_alpha) {
        // Collect averaged R/G/B
        AccumulateRGB(r_ptr, g_ptr, b_ptr, step, /* rgb_stride = */ 0,
                      tmp_rgb, width);
      } else {
        AccumulateRGBA(r_ptr, g_ptr, b_ptr, a_ptr, /* rgb_stride = */ 0,
                       tmp_rgb, width);
      }
      if (rg == NULL) {
    	  WebPConvertRGBA32ToUV_C(tmp_rgb, dst_u, dst_v, uv_width);
      } else {
        ConvertRowsToUV(tmp_rgb, dst_u, dst_v, uv_width, rg);
      }
    }
    WebPSafeFree(tmp_rgb);
  }
  return 1;
}

static void WebPPictureResetBufferARGB(WebPPicture* const picture) {
  picture->memory_argb_ = NULL;
  picture->argb = NULL;
  picture->argb_stride = 0;
}

void WebPPictureResetBuffers(WebPPicture* const picture) {
  WebPPictureResetBufferARGB(picture);
  WebPPictureResetBufferYUVA(picture);
}

void WebPPictureFree(WebPPicture* picture) {
  if (picture != NULL) {
    WebPSafeFree(picture->memory_);
    WebPSafeFree(picture->memory_argb_);
    WebPPictureResetBuffers(picture);
  }
}

#define WEBP_ALIGN_CST 31
#define WEBP_ALIGN(PTR) (((uintptr_t)(PTR) + WEBP_ALIGN_CST) & ~WEBP_ALIGN_CST)

int WebPPictureAllocARGB(WebPPicture* const picture, int width, int height) {
  void* memory;
  const uint64_t argb_size = (uint64_t)width * height;

  assert(picture != NULL);

  WebPSafeFree(picture->memory_argb_);
  WebPPictureResetBufferARGB(picture);

  if (width <= 0 || height <= 0) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_BAD_DIMENSION);
  }
  // allocate a new buffer.
  memory = WebPSafeMalloc(argb_size + WEBP_ALIGN_CST, sizeof(*picture->argb));
  if (memory == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  picture->memory_argb_ = memory;
  picture->argb = (uint32_t*)WEBP_ALIGN(memory);
  picture->argb_stride = width;
  return 1;
}

int WebPPictureAlloc(WebPPicture* picture) {
  if (picture != NULL) {
    const int width = picture->width;
    const int height = picture->height;

    WebPPictureFree(picture);   // erase previous buffer

    if (!picture->use_argb) {
      return WebPPictureAllocYUVA(picture, width, height);
    } else {
      return WebPPictureAllocARGB(picture, width, height);
    }
  }
  return 1;
}

#ifdef WORDS_BIGENDIAN
#define ALPHA_OFFSET 0   // uint32_t 0xff000000 is 0xff,00,00,00 in memory
#else
#define ALPHA_OFFSET 3   // uint32_t 0xff000000 is 0x00,00,00,ff in memory
#endif

static uint32_t MakeARGB32(int a, int r, int g, int b) {
  return (((uint32_t)a << 24) | (r << 16) | (g << 8) | b);
}

#ifdef WORDS_BIGENDIAN
static void PackARGB_C(const uint8_t* a, const uint8_t* r, const uint8_t* g,
                       const uint8_t* b, int len, uint32_t* out) {
  int i;
  for (i = 0; i < len; ++i) {
    out[i] = MakeARGB32(a[4 * i], r[4 * i], g[4 * i], b[4 * i]);
  }
}
#endif

void VP8LConvertBGRAToRGBA_C(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >> 24) & 0xff;
  }
}

static void PackRGB_C(const uint8_t* r, const uint8_t* g, const uint8_t* b,
                      int len, int step, uint32_t* out) {
  int i, offset = 0;
  for (i = 0; i < len; ++i) {
    out[i] = MakeARGB32(0xff, r[offset], g[offset], b[offset]);
    offset += step;
  }
}

static int Import(WebPPicture* const picture,
                  const uint8_t* rgb, int rgb_stride,
                  int step, int swap_rb, int import_alpha) {
  int y;
  // swap_rb -> b,g,r,a , !swap_rb -> r,g,b,a
  const uint8_t* r_ptr = rgb + (swap_rb ? 2 : 0);
  const uint8_t* g_ptr = rgb + 1;
  const uint8_t* b_ptr = rgb + (swap_rb ? 0 : 2);
  const int width = picture->width;
  const int height = picture->height;

  if (!picture->use_argb) {
    const uint8_t* a_ptr = import_alpha ? rgb + 3 : NULL;
    return ImportYUVAFromRGBA(r_ptr, g_ptr, b_ptr, a_ptr, step, rgb_stride,
                              0.f /* no dithering */, 0, picture);
  }
  if (!WebPPictureAlloc(picture)) return 0;

  if (import_alpha) {
    // dst[] byte order is {a,r,g,b} for big-endian, {b,g,r,a} for little endian
    uint32_t* dst = picture->argb;
    const int do_copy = (ALPHA_OFFSET == 3) && swap_rb;
    assert(step == 4);
    if (do_copy) {
      for (y = 0; y < height; ++y) {
        memcpy(dst, rgb, width * 4);
        rgb += rgb_stride;
        dst += picture->argb_stride;
      }
    } else {
      for (y = 0; y < height; ++y) {
#ifdef WORDS_BIGENDIAN
        // BGRA or RGBA input order.
        const uint8_t* a_ptr = rgb + 3;
        WebPPackARGB(a_ptr, r_ptr, g_ptr, b_ptr, width, dst);
        r_ptr += rgb_stride;
        g_ptr += rgb_stride;
        b_ptr += rgb_stride;
#else
        // RGBA input order. Need to swap R and B.
        VP8LConvertBGRAToRGBA_C((const uint32_t*)rgb, width, (uint8_t*)dst);
#endif
        rgb += rgb_stride;
        dst += picture->argb_stride;
      }
    }
  } else {
    uint32_t* dst = picture->argb;
    assert(step >= 3);
    for (y = 0; y < height; ++y) {
      PackRGB_C(r_ptr, g_ptr, b_ptr, width, step, dst);
      r_ptr += rgb_stride;
      g_ptr += rgb_stride;
      b_ptr += rgb_stride;
      dst += picture->argb_stride;
    }
  }
  return 1;
}

int WebPPictureImportRGB(WebPPicture* picture,
                         const uint8_t* rgb, int rgb_stride) {
  return (picture != NULL && rgb != NULL)
             ? Import(picture, rgb, rgb_stride, 3, 0, 0)
             : 0;
}

int ReadJPEG(const uint8_t* const data, size_t data_size,
             WebPPicture* const pic, int keep_alpha,
             Metadata* const metadata) {
  volatile int ok = 0;
  int width, height;
  int64_t stride;
  volatile struct jpeg_decompress_struct dinfo;
  struct my_error_mgr jerr;
  uint8_t* volatile rgb = NULL;
  JSAMPROW buffer[1];
  JPEGReadContext ctx;

  if (data == NULL || data_size == 0 || pic == NULL) return 0;

  (void)keep_alpha;
  memset(&ctx, 0, sizeof(ctx));
  ctx.data = data;
  ctx.data_size = data_size;

  memset((j_decompress_ptr)&dinfo, 0, sizeof(dinfo));   // for setjmp sanity
  dinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;

  if (setjmp(jerr.setjmp_buffer)) {
 Error:
    MetadataFree(metadata);
    jpeg_destroy_decompress((j_decompress_ptr)&dinfo);
    goto End;
  }

  jpeg_create_decompress((j_decompress_ptr)&dinfo);
  ContextSetup(&dinfo, &ctx);
  if (metadata != NULL) SaveMetadataMarkers((j_decompress_ptr)&dinfo);
  jpeg_read_header((j_decompress_ptr)&dinfo, TRUE);

  dinfo.out_color_space = JCS_RGB;
  dinfo.do_fancy_upsampling = TRUE;

  jpeg_start_decompress((j_decompress_ptr)&dinfo);

  if (dinfo.output_components != 3) {
    goto Error;
  }

  width = dinfo.output_width;
  height = dinfo.output_height;
  stride = (int64_t)dinfo.output_width * dinfo.output_components * sizeof(*rgb);

  if (stride != (int)stride ||
      !ImgIoUtilCheckSizeArgumentsOverflow(stride, height)) {
    goto Error;
  }

  rgb = (uint8_t*)malloc((size_t)stride * height);
  if (rgb == NULL) {
    goto Error;
  }
  buffer[0] = (JSAMPLE*)rgb;

  while (dinfo.output_scanline < dinfo.output_height) {
    if (jpeg_read_scanlines((j_decompress_ptr)&dinfo, buffer, 1) != 1) {
      goto Error;
    }
    buffer[0] += stride;
  }

  if (metadata != NULL) {
    ok = ExtractMetadataFromJPEG((j_decompress_ptr)&dinfo, metadata);
    if (!ok) {
      fprintf(stderr, "Error extracting JPEG metadata!\n");
      goto Error;
    }
  }

  jpeg_finish_decompress((j_decompress_ptr)&dinfo);
  jpeg_destroy_decompress((j_decompress_ptr)&dinfo);

  // WebP conversion.
  pic->width = width;
  pic->height = height;
  ok = WebPPictureImportRGB(pic, rgb, (int)stride);
  if (!ok) goto Error;

 End:
  free(rgb);
  return ok;
}

static int FailReader(const uint8_t* const data, size_t data_size,
                      struct WebPPicture* const pic,
                      int keep_alpha, struct Metadata* const metadata) {
  (void)data;
  (void)data_size;
  (void)pic;
  (void)keep_alpha;
  (void)metadata;
  return 0;
}

WebPImageReader WebPGetImageReader(WebPInputFileFormat format) {
  switch (format) {
    case WEBP_PNG_FORMAT: return ReadPNG;
    case WEBP_JPEG_FORMAT: return ReadJPEG;
    case WEBP_TIFF_FORMAT: return ReadTIFF;
    case WEBP_WEBP_FORMAT: return ReadWebP;
    case WEBP_PNM_FORMAT: return ReadPNM;
    default: return FailReader;
  }
}

WebPImageReader WebPGuessImageReader(const uint8_t* const data,
                                     size_t data_size) {
  return WebPGetImageReader(WebPGuessImageType(data, data_size));
}

void ImgIoUtilCopyPlane(const uint8_t* src, int src_stride,
                        uint8_t* dst, int dst_stride, int width, int height) {
  while (height-- > 0) {
    memcpy(dst, src, width * sizeof(*dst));
    src += src_stride;
    dst += dst_stride;
  }
}

typedef void (*WebPUpsampleLinePairFunc)(
    const uint8_t* top_y, const uint8_t* bottom_y,
    const uint8_t* top_u, const uint8_t* top_v,
    const uint8_t* cur_u, const uint8_t* cur_v,
    uint8_t* top_dst, uint8_t* bottom_dst, int len);

#define FANCY_UPSAMPLING   // undefined to remove fancy upsampling support

#define LOAD_UV(u, v) ((u) | ((v) << 16))

#define UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                                  \
static void FUNC_NAME(const uint8_t* top_y, const uint8_t* bottom_y,           \
                      const uint8_t* top_u, const uint8_t* top_v,              \
                      const uint8_t* cur_u, const uint8_t* cur_v,              \
                      uint8_t* top_dst, uint8_t* bottom_dst, int len) {        \
  int x;                                                                       \
  const int last_pixel_pair = (len - 1) >> 1;                                  \
  uint32_t tl_uv = LOAD_UV(top_u[0], top_v[0]);   /* top-left sample */        \
  uint32_t l_uv  = LOAD_UV(cur_u[0], cur_v[0]);   /* left-sample */            \
  assert(top_y != NULL);                                                       \
  {                                                                            \
    const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;                \
    FUNC(top_y[0], uv0 & 0xff, (uv0 >> 16), top_dst);                          \
  }                                                                            \
  if (bottom_y != NULL) {                                                      \
    const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;                \
    FUNC(bottom_y[0], uv0 & 0xff, (uv0 >> 16), bottom_dst);                    \
  }                                                                            \
  for (x = 1; x <= last_pixel_pair; ++x) {                                     \
    const uint32_t t_uv = LOAD_UV(top_u[x], top_v[x]);  /* top sample */       \
    const uint32_t uv   = LOAD_UV(cur_u[x], cur_v[x]);  /* sample */           \
    /* precompute invariant values associated with first and second diagonals*/\
    const uint32_t avg = tl_uv + t_uv + l_uv + uv + 0x00080008u;               \
    const uint32_t diag_12 = (avg + 2 * (t_uv + l_uv)) >> 3;                   \
    const uint32_t diag_03 = (avg + 2 * (tl_uv + uv)) >> 3;                    \
    {                                                                          \
      const uint32_t uv0 = (diag_12 + tl_uv) >> 1;                             \
      const uint32_t uv1 = (diag_03 + t_uv) >> 1;                              \
      FUNC(top_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                          \
           top_dst + (2 * x - 1) * (XSTEP));                                   \
      FUNC(top_y[2 * x - 0], uv1 & 0xff, (uv1 >> 16),                          \
           top_dst + (2 * x - 0) * (XSTEP));                                   \
    }                                                                          \
    if (bottom_y != NULL) {                                                    \
      const uint32_t uv0 = (diag_03 + l_uv) >> 1;                              \
      const uint32_t uv1 = (diag_12 + uv) >> 1;                                \
      FUNC(bottom_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                       \
           bottom_dst + (2 * x - 1) * (XSTEP));                                \
      FUNC(bottom_y[2 * x + 0], uv1 & 0xff, (uv1 >> 16),                       \
           bottom_dst + (2 * x + 0) * (XSTEP));                                \
    }                                                                          \
    tl_uv = t_uv;                                                              \
    l_uv = uv;                                                                 \
  }                                                                            \
  if (!(len & 1)) {                                                            \
    {                                                                          \
      const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;              \
      FUNC(top_y[len - 1], uv0 & 0xff, (uv0 >> 16),                            \
           top_dst + (len - 1) * (XSTEP));                                     \
    }                                                                          \
    if (bottom_y != NULL) {                                                    \
      const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;              \
      FUNC(bottom_y[len - 1], uv0 & 0xff, (uv0 >> 16),                         \
           bottom_dst + (len - 1) * (XSTEP));                                  \
    }                                                                          \
  }                                                                            \
}

static int MultHi(int v, int coeff) {   // _mm_mulhi_epu16 emulation
  return (v * coeff) >> 8;
}

static int VP8Clip8(int v) {
  return ((v & ~YUV_MASK2) == 0) ? (v >> YUV_FIX2) : (v < 0) ? 0 : 255;
}

static int VP8YUVToR(int y, int v) {
  return VP8Clip8(MultHi(y, 19077) + MultHi(v, 26149) - 14234);
}

static int VP8YUVToG(int y, int u, int v) {
  return VP8Clip8(MultHi(y, 19077) - MultHi(u, 6419) - MultHi(v, 13320) + 8708);
}

static int VP8YUVToB(int y, int u) {
  return VP8Clip8(MultHi(y, 19077) + MultHi(u, 33050) - 17685);
}


static void VP8YuvToRgb(int y, int u, int v,
                                    uint8_t* const rgb) {
  rgb[0] = VP8YUVToR(y, v);
  rgb[1] = VP8YUVToG(y, u, v);
  rgb[2] = VP8YUVToB(y, u);
}

static void VP8YuvToRgba(uint8_t y, uint8_t u, uint8_t v,
                                     uint8_t* const rgba) {
  VP8YuvToRgb(y, u, v, rgba);
  rgba[3] = 0xff;
}

UPSAMPLE_FUNC(UpsampleRgbaLinePair_C, VP8YuvToRgba, 4)

static void EmptyUpsampleFunc(const uint8_t* top_y, const uint8_t* bottom_y,
                              const uint8_t* top_u, const uint8_t* top_v,
                              const uint8_t* cur_u, const uint8_t* cur_v,
                              uint8_t* top_dst, uint8_t* bottom_dst, int len) {
  (void)top_y;
  (void)bottom_y;
  (void)top_u;
  (void)top_v;
  (void)cur_u;
  (void)cur_v;
  (void)top_dst;
  (void)bottom_dst;
  (void)len;
  assert(0);   // COLORSPACE SUPPORT NOT COMPILED
}

#define UpsampleArgbLinePair_C EmptyUpsampleFunc

WebPUpsampleLinePairFunc WebPGetLinePairConverter(int alpha_is_last) {
#ifdef FANCY_UPSAMPLING
  return alpha_is_last ? UpsampleRgbaLinePair_C : UpsampleArgbLinePair_C;
#else
  return (alpha_is_last ? DualLineSamplerBGRA : DualLineSamplerARGB);
#endif
}

int WebPPictureYUVAToARGB(WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (picture->y == NULL || picture->u == NULL || picture->v == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_ALPHA_BIT) && picture->a == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_UV_MASK) != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }
  // Allocate a new argb buffer (discarding the previous one).
  if (!WebPPictureAllocARGB(picture, picture->width, picture->height)) return 0;
  picture->use_argb = 1;

  // Convert
  {
    int y;
    const int width = picture->width;
    const int height = picture->height;
    const int argb_stride = 4 * picture->argb_stride;
    uint8_t* dst = (uint8_t*)picture->argb;
    const uint8_t *cur_u = picture->u, *cur_v = picture->v, *cur_y = picture->y;
    WebPUpsampleLinePairFunc upsample =
        WebPGetLinePairConverter(ALPHA_OFFSET > 0);

    // First row, with replicated top samples.
    upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    cur_y += picture->y_stride;
    dst += argb_stride;
    // Center rows.
    for (y = 1; y + 1 < height; y += 2) {
      const uint8_t* const top_u = cur_u;
      const uint8_t* const top_v = cur_v;
      cur_u += picture->uv_stride;
      cur_v += picture->uv_stride;
      upsample(cur_y, cur_y + picture->y_stride, top_u, top_v, cur_u, cur_v,
               dst, dst + argb_stride, width);
      cur_y += 2 * picture->y_stride;
      dst += 2 * argb_stride;
    }
    // Last row (if needed), with replicated bottom samples.
    if (height > 1 && !(height & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    }
    // Insert alpha values if needed, in replacement for the default 0xff ones.
    if (picture->colorspace & WEBP_CSP_ALPHA_BIT) {
      for (y = 0; y < height; ++y) {
        uint32_t* const argb_dst = picture->argb + y * picture->argb_stride;
        const uint8_t* const src = picture->a + y * picture->a_stride;
        int x;
        for (x = 0; x < width; ++x) {
          argb_dst[x] = (argb_dst[x] & 0x00ffffffu) | ((uint32_t)src[x] << 24);
        }
      }
    }
  }
  return 1;
}

static int ReadYUV(const uint8_t* const data, size_t data_size,
                   WebPPicture* const pic) {
  const int use_argb = pic->use_argb;
  const int uv_width = (pic->width + 1) / 2;
  const int uv_height = (pic->height + 1) / 2;
  const int y_plane_size = pic->width * pic->height;
  const int uv_plane_size = uv_width * uv_height;
  const size_t expected_data_size = y_plane_size + 2 * uv_plane_size;

  if (data_size != expected_data_size) {
    fprintf(stderr,
            "input data doesn't have the expected size (%d instead of %d)\n",
            (int)data_size, (int)expected_data_size);
    return 0;
  }

  pic->use_argb = 0;
  if (!WebPPictureAlloc(pic)) return 0;
  ImgIoUtilCopyPlane(data, pic->width, pic->y, pic->y_stride,
                     pic->width, pic->height);
  ImgIoUtilCopyPlane(data + y_plane_size, uv_width,
                     pic->u, pic->uv_stride, uv_width, uv_height);
  ImgIoUtilCopyPlane(data + y_plane_size + uv_plane_size, uv_width,
                     pic->v, pic->uv_stride, uv_width, uv_height);
  return use_argb ? WebPPictureYUVAToARGB(pic) : 1;
}

static int ReadPicture(const char* const filename, WebPPicture* const pic,
                       int keep_alpha, Metadata* const metadata) {
  const uint8_t* data = NULL;
  size_t data_size = 0;
  int ok = 0;

  ok = ImgIoUtilReadFile(filename, &data, &data_size);
  if (!ok) goto End;

  if (pic->width == 0 || pic->height == 0) {
    WebPImageReader reader = WebPGuessImageReader(data, data_size);
    ok = reader(data, data_size, pic, keep_alpha, metadata);
  } else {
    // If image size is specified, infer it as YUV format.
    ok = ReadYUV(data, data_size, pic);
  }
 End:
  if (!ok) {
    fprintf(stderr, "Error! Could not process file %s\n", filename);
  }
  free((void*)data);
  return ok;
}

static int MyWriter(const uint8_t* data, size_t data_size,
                    const WebPPicture* const pic) {
  FILE* const out = (FILE*)pic->custom_ptr;
  return data_size ? (fwrite(data, data_size, 1, out) == 1) : 1;
}

static const char* const kErrorMessages[VP8_ENC_ERROR_LAST] = {
  "OK",
  "OUT_OF_MEMORY: Out of memory allocating objects",
  "BITSTREAM_OUT_OF_MEMORY: Out of memory re-allocating byte buffer",
  "NULL_PARAMETER: NULL parameter passed to function",
  "INVALID_CONFIGURATION: configuration is invalid",
  "BAD_DIMENSION: Bad picture dimension. Maximum width and height "
  "allowed is 16383 pixels.",
  "PARTITION0_OVERFLOW: Partition #0 is too big to fit 512k.\n"
  "To reduce the size of this partition, try using less segments "
  "with the -segments option, and eventually reduce the number of "
  "header bits using -partition_limit. More details are available "
  "in the manual (`man cwebp`)",
  "PARTITION_OVERFLOW: Partition is too big to fit 16M",
  "BAD_WRITE: Picture writer returned an I/O error",
  "FILE_TOO_BIG: File would be too big to fit in 4G",
  "USER_ABORT: encoding abort requested by user"
};

static void PrintByteCount(const int bytes[4], int total_size,
                           int* const totals) {
  int s;
  int total = 0;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "| %7d ", bytes[s]);
    total += bytes[s];
    if (totals) totals[s] += bytes[s];
  }
  fprintf(stderr, "| %7d  (%.1f%%)\n", total, 100.f * total / total_size);
}

static void PrintPercents(const int counts[4]) {
  int s;
  const int total = counts[0] + counts[1] + counts[2] + counts[3];
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "|      %2d%%", (int)(100. * counts[s] / total + .5));
  }
  fprintf(stderr, "| %7d\n", total);
}

static void PrintValues(const int values[4]) {
  int s;
  for (s = 0; s < 4; ++s) {
    fprintf(stderr, "| %7d ", values[s]);
  }
  fprintf(stderr, "|\n");
}


static void PrintFullLosslessInfo(const WebPAuxStats* const stats,
                                  const char* const description) {
  fprintf(stderr, "Lossless-%s compressed size: %d bytes\n",
          description, stats->lossless_size);
  fprintf(stderr, "  * Header size: %d bytes, image data size: %d\n",
          stats->lossless_hdr_size, stats->lossless_data_size);
  if (stats->lossless_features) {
    fprintf(stderr, "  * Lossless features used:");
    if (stats->lossless_features & 1) fprintf(stderr, " PREDICTION");
    if (stats->lossless_features & 2) fprintf(stderr, " CROSS-COLOR-TRANSFORM");
    if (stats->lossless_features & 4) fprintf(stderr, " SUBTRACT-GREEN");
    if (stats->lossless_features & 8) fprintf(stderr, " PALETTE");
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "  * Precision Bits: histogram=%d transform=%d cache=%d\n",
          stats->histogram_bits, stats->transform_bits, stats->cache_bits);
  if (stats->palette_size > 0) {
    fprintf(stderr, "  * Palette size:   %d\n", stats->palette_size);
  }
}


static void PrintExtraInfoLossy(const WebPPicture* const pic, int short_output,
                                int full_details,
                                const char* const file_name) {
  const WebPAuxStats* const stats = pic->stats;
  if (short_output) {
    fprintf(stderr, "%7d %2.2f\n", stats->coded_size, stats->PSNR[3]);
  } else {
    const int num_i4 = stats->block_count[0];
    const int num_i16 = stats->block_count[1];
    const int num_skip = stats->block_count[2];
    const int total = num_i4 + num_i16;
    fprintf(stderr, "File:      %s\n", file_name);
    fprintf(stderr, "Dimension: %d x %d%s\n",
            pic->width, pic->height,
            stats->alpha_data_size ? " (with alpha)" : "");
    fprintf(stderr, "Output:    "
            "%d bytes Y-U-V-All-PSNR %2.2f %2.2f %2.2f   %2.2f dB\n"
            "           (%.2f bpp)\n",
            stats->coded_size,
            stats->PSNR[0], stats->PSNR[1], stats->PSNR[2], stats->PSNR[3],
            8.f * stats->coded_size / pic->width / pic->height);
    if (total > 0) {
      int totals[4] = { 0, 0, 0, 0 };
      fprintf(stderr, "block count:  intra4:     %6d  (%.2f%%)\n"
                      "              intra16:    %6d  (%.2f%%)\n"
                      "              skipped:    %6d  (%.2f%%)\n",
              num_i4, 100.f * num_i4 / total,
              num_i16, 100.f * num_i16 / total,
              num_skip, 100.f * num_skip / total);
      fprintf(stderr, "bytes used:  header:         %6d  (%.1f%%)\n"
                      "             mode-partition: %6d  (%.1f%%)\n",
              stats->header_bytes[0],
              100.f * stats->header_bytes[0] / stats->coded_size,
              stats->header_bytes[1],
              100.f * stats->header_bytes[1] / stats->coded_size);
      if (stats->alpha_data_size > 0) {
        fprintf(stderr, "             transparency:   %6d (%.1f dB)\n",
                stats->alpha_data_size, stats->PSNR[4]);
      }
      fprintf(stderr, " Residuals bytes  "
                      "|segment 1|segment 2|segment 3"
                      "|segment 4|  total\n");
      if (full_details) {
        fprintf(stderr, "  intra4-coeffs:  ");
        PrintByteCount(stats->residual_bytes[0], stats->coded_size, totals);
        fprintf(stderr, " intra16-coeffs:  ");
        PrintByteCount(stats->residual_bytes[1], stats->coded_size, totals);
        fprintf(stderr, "  chroma coeffs:  ");
        PrintByteCount(stats->residual_bytes[2], stats->coded_size, totals);
      }
      fprintf(stderr, "    macroblocks:  ");
      PrintPercents(stats->segment_size);
      fprintf(stderr, "      quantizer:  ");
      PrintValues(stats->segment_quant);
      fprintf(stderr, "   filter level:  ");
      PrintValues(stats->segment_level);
      if (full_details) {
        fprintf(stderr, "------------------+---------");
        fprintf(stderr, "+---------+---------+---------+-----------------\n");
        fprintf(stderr, " segments total:  ");
        PrintByteCount(totals, stats->coded_size, NULL);
      }
    }
    if (stats->lossless_size > 0) {
      PrintFullLosslessInfo(stats, "alpha");
    }
  }
}

void WebPMemoryWriterClear(WebPMemoryWriter* writer) {
  if (writer != NULL) {
    WebPSafeFree(writer->mem);
    writer->mem = NULL;
    writer->size = 0;
    writer->max_size = 0;
  }
}

typedef struct {
  int simple_;             // filtering type: 0=complex, 1=simple
  int level_;              // base filter level [0..63]
  int sharpness_;          // [0..7]
  int i4x4_lf_delta_;      // delta filter level for i4x4 relative to i16x16
} VP8EncFilterHeader;

typedef struct {
  int num_segments_;      // Actual number of segments. 1 segment only = unused.
  int update_map_;        // whether to update the segment map or not.
                          // must be 0 if there's only 1 segment.
  int size_;              // bit-cost for transmitting the segment map
} VP8EncSegmentHeader;

typedef struct VP8BitWriter VP8BitWriter;
struct VP8BitWriter {
  int32_t  range_;      // range-1
  int32_t  value_;
  int      run_;        // number of outstanding bits
  int      nb_bits_;    // number of pending bits
  uint8_t* buf_;        // internal buffer. Re-allocated regularly. Not owned.
  size_t   pos_;
  size_t   max_pos_;
  int      error_;      // true in case of error
};

struct VP8Tokens {
  VP8Tokens* next_;        // pointer to next page
};

typedef struct {
#if !defined(DISABLE_TOKEN_BUFFER)
  VP8Tokens* pages_;        // first page
  VP8Tokens** last_page_;   // last page
  uint16_t* tokens_;        // set to (*last_page_)->tokens_
  int left_;                // how many free tokens left before the page is full
  int page_size_;           // number of tokens per page
#endif
  int error_;         // true in case of malloc error
} VP8TBuffer;

enum { MB_FEATURE_TREE_PROBS = 3,
       NUM_MB_SEGMENTS = 4,
       NUM_REF_LF_DELTAS = 4,
       NUM_MODE_LF_DELTAS = 4,    // I4x4, ZERO, *, SPLIT
       MAX_NUM_PARTITIONS = 8,
       // Probabilities
       NUM_TYPES = 4,   // 0: i16-AC,  1: i16-DC,  2:chroma-AC,  3:i4-AC
       NUM_BANDS = 8,
       NUM_CTX = 3,
       NUM_PROBAS = 11
     };

// State of the worker thread object
typedef enum {
  NOT_OK = 0,   // object is unusable
  OK,           // ready to work
  WORK          // busy finishing the current task
} WebPWorkerStatus;

// Function to be called by the worker thread. Takes two opaque pointers as
// arguments (data1 and data2), and should return false in case of error.
typedef int (*WebPWorkerHook)(void*, void*);

typedef struct {
  void* impl_;            // platform-dependent implementation worker details
  WebPWorkerStatus status_;
  WebPWorkerHook hook;    // hook to call
  void* data1;            // first argument passed to 'hook'
  void* data2;            // second argument passed to 'hook'
  int had_error;          // return value of the last call to 'hook'
} WebPWorker;

typedef struct VP8Matrix {
  uint16_t q_[16];        // quantizer steps
  uint16_t iq_[16];       // reciprocals, fixed point.
  uint32_t bias_[16];     // rounding bias
  uint32_t zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t sharpen_[16];  // frequency boosters for slight sharpening
} VP8Matrix;

typedef int64_t score_t;     // type used for scores, rate, distortion

typedef struct {
  VP8Matrix y1_, y2_, uv_;  // quantization matrices
  int alpha_;      // quant-susceptibility, range [-127,127]. Zero is neutral.
                   // Lower values indicate a lower risk of blurriness.
  int beta_;       // filter-susceptibility, range [0,255].
  int quant_;      // final segment quantizer.
  int fstrength_;  // final in-loop filtering strength
  int max_edge_;   // max edge delta (for filtering strength)
  int min_disto_;  // minimum distortion required to trigger filtering record
  // reactivities
  int lambda_i16_, lambda_i4_, lambda_uv_;
  int lambda_mode_, lambda_trellis_, tlambda_;
  int lambda_trellis_i16_, lambda_trellis_i4_, lambda_trellis_uv_;

  // lambda values for distortion-based evaluation
  score_t i4_penalty_;   // penalty for using Intra4
} VP8SegmentInfo;

typedef uint32_t proba_t;   // 16b + 16b
typedef uint8_t ProbaArray[NUM_CTX][NUM_PROBAS];
typedef proba_t StatsArray[NUM_CTX][NUM_PROBAS];
typedef uint16_t CostArray[NUM_CTX][MAX_VARIABLE_LEVEL + 1];
typedef const uint16_t* (*CostArrayPtr)[NUM_CTX];   // for easy casting
typedef const uint16_t* CostArrayMap[16][NUM_CTX];

typedef struct {
  uint8_t segments_[3];     // probabilities for segment tree
  uint8_t skip_proba_;      // final probability of being skipped.
  ProbaArray coeffs_[NUM_TYPES][NUM_BANDS];      // 1056 bytes
  StatsArray stats_[NUM_TYPES][NUM_BANDS];       // 4224 bytes
  CostArray level_cost_[NUM_TYPES][NUM_BANDS];   // 13056 bytes
  CostArrayMap remapped_costs_[NUM_TYPES];       // 1536 bytes
  int dirty_;               // if true, need to call VP8CalculateLevelCosts()
  int use_skip_proba_;      // Note: we always use skip_proba for now.
  int nb_skip_;             // number of skipped blocks
} VP8EncProba;

typedef enum {   // Rate-distortion optimization levels
  RD_OPT_NONE        = 0,  // no rd-opt
  RD_OPT_BASIC       = 1,  // basic scoring (no trellis)
  RD_OPT_TRELLIS     = 2,  // perform trellis-quant on the final decision only
  RD_OPT_TRELLIS_ALL = 3   // trellis-quant for every scoring (much slower)
} VP8RDLevel;

typedef struct {
  // block type
  unsigned int type_:2;     // 0=i4x4, 1=i16x16
  unsigned int uv_mode_:2;
  unsigned int skip_:1;
  unsigned int segment_:2;
  uint8_t alpha_;      // quantization-susceptibility
} VP8MBInfo;

enum { MAX_LF_LEVELS = 64,       // Maximum loop filter level
       MAX_VARIABLE_LEVEL = 67,  // last (inclusive) level with variable cost
       MAX_LEVEL = 2047          // max level (note: max codable is 2047 + 67)
     };

typedef double LFStats[NUM_MB_SEGMENTS][MAX_LF_LEVELS];  // filter stats

typedef int8_t DError[2 /* u/v */][2 /* top or left */];

struct VP8Encoder {
  const WebPConfig* config_;    // user configuration and parameters
  WebPPicture* pic_;            // input / output picture

  // headers
  VP8EncFilterHeader   filter_hdr_;     // filtering information
  VP8EncSegmentHeader  segment_hdr_;    // segment information

  int profile_;                      // VP8's profile, deduced from Config.

  // dimension, in macroblock units.
  int mb_w_, mb_h_;
  int preds_w_;   // stride of the *preds_ prediction plane (=4*mb_w + 1)

  // number of partitions (1, 2, 4 or 8 = MAX_NUM_PARTITIONS)
  int num_parts_;

  // per-partition boolean decoders.
  VP8BitWriter bw_;                         // part0
  VP8BitWriter parts_[MAX_NUM_PARTITIONS];  // token partitions
  VP8TBuffer tokens_;                       // token buffer

  int percent_;                             // for progress

  // transparency blob
  int has_alpha_;
  uint8_t* alpha_data_;       // non-NULL if transparency is present
  uint32_t alpha_data_size_;
  WebPWorker alpha_worker_;

  // quantization info (one set of DC/AC dequant factor per segment)
  VP8SegmentInfo dqm_[NUM_MB_SEGMENTS];
  int base_quant_;                 // nominal quantizer value. Only used
                                   // for relative coding of segments' quant.
  int alpha_;                      // global susceptibility (<=> complexity)
  int uv_alpha_;                   // U/V quantization susceptibility
  // global offset of quantizers, shared by all segments
  int dq_y1_dc_;
  int dq_y2_dc_, dq_y2_ac_;
  int dq_uv_dc_, dq_uv_ac_;

  // probabilities and statistics
  VP8EncProba proba_;
  uint64_t    sse_[4];      // sum of Y/U/V/A squared errors for all macroblocks
  uint64_t    sse_count_;   // pixel count for the sse_[] stats
  int         coded_size_;
  int         residual_bytes_[3][4];
  int         block_count_[3];

  // quality/speed settings
  int method_;               // 0=fastest, 6=best/slowest.
  VP8RDLevel rd_opt_level_;  // Deduced from method_.
  int max_i4_header_bits_;   // partition #0 safeness factor
  int mb_header_limit_;      // rough limit for header bits per MB
  int thread_level_;         // derived from config->thread_level
  int do_search_;            // derived from config->target_XXX
  int use_tokens_;           // if true, use token buffer

  // Memory
  VP8MBInfo* mb_info_;   // contextual macroblock infos (mb_w_ + 1)
  uint8_t*   preds_;     // predictions modes: (4*mb_w+1) * (4*mb_h+1)
  uint32_t*  nz_;        // non-zero bit context: mb_w+1
  uint8_t*   y_top_;     // top luma samples.
  uint8_t*   uv_top_;    // top u/v samples.
                         // U and V are packed into 16 bytes (8 U + 8 V)
  LFStats*   lf_stats_;  // autofilter stats (if NULL, autofilter is off)
  DError*    top_derr_;  // diffusion error (NULL if disabled)
};

typedef struct VP8Encoder VP8Encoder;

#define SIZE 8
#define SIZE2 (SIZE / 2)

static int IsTransparentARGBArea(const uint32_t* ptr, int stride, int size) {
  int y, x;
  for (y = 0; y < size; ++y) {
    for (x = 0; x < size; ++x) {
      if (ptr[x] & 0xff000000u) {
        return 0;
      }
    }
    ptr += stride;
  }
  return 1;
}

static void Flatten(uint8_t* ptr, int v, int stride, int size) {
  int y;
  for (y = 0; y < size; ++y) {
    memset(ptr, v, size);
    ptr += stride;
  }
}

static void FlattenARGB(uint32_t* ptr, uint32_t v, int stride, int size) {
  int x, y;
  for (y = 0; y < size; ++y) {
    for (x = 0; x < size; ++x) ptr[x] = v;
    ptr += stride;
  }
}

// Smoothen the luma components of transparent pixels. Return true if the whole
// block is transparent.
static int SmoothenBlock(const uint8_t* a_ptr, int a_stride, uint8_t* y_ptr,
                         int y_stride, int width, int height) {
  int sum = 0, count = 0;
  int x, y;
  const uint8_t* alpha_ptr = a_ptr;
  uint8_t* luma_ptr = y_ptr;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      if (alpha_ptr[x] != 0) {
        ++count;
        sum += luma_ptr[x];
      }
    }
    alpha_ptr += a_stride;
    luma_ptr += y_stride;
  }
  if (count > 0 && count < width * height) {
    const uint8_t avg_u8 = (uint8_t)(sum / count);
    alpha_ptr = a_ptr;
    luma_ptr = y_ptr;
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        if (alpha_ptr[x] == 0) luma_ptr[x] = avg_u8;
      }
      alpha_ptr += a_stride;
      luma_ptr += y_stride;
    }
  }
  return (count == 0);
}

void WebPCleanupTransparentArea(WebPPicture* pic) {
  int x, y, w, h;
  if (pic == NULL) return;
  w = pic->width / SIZE;
  h = pic->height / SIZE;

  // note: we ignore the left-overs on right/bottom, except for SmoothenBlock().
  if (pic->use_argb) {
    uint32_t argb_value = 0;
    for (y = 0; y < h; ++y) {
      int need_reset = 1;
      for (x = 0; x < w; ++x) {
        const int off = (y * pic->argb_stride + x) * SIZE;
        if (IsTransparentARGBArea(pic->argb + off, pic->argb_stride, SIZE)) {
          if (need_reset) {
            argb_value = pic->argb[off];
            need_reset = 0;
          }
          FlattenARGB(pic->argb + off, argb_value, pic->argb_stride, SIZE);
        } else {
          need_reset = 1;
        }
      }
    }
  } else {
    const int width = pic->width;
    const int height = pic->height;
    const int y_stride = pic->y_stride;
    const int uv_stride = pic->uv_stride;
    const int a_stride = pic->a_stride;
    uint8_t* y_ptr = pic->y;
    uint8_t* u_ptr = pic->u;
    uint8_t* v_ptr = pic->v;
    const uint8_t* a_ptr = pic->a;
    int values[3] = { 0 };
    if (a_ptr == NULL || y_ptr == NULL || u_ptr == NULL || v_ptr == NULL) {
      return;
    }
    for (y = 0; y + SIZE <= height; y += SIZE) {
      int need_reset = 1;
      for (x = 0; x + SIZE <= width; x += SIZE) {
        if (SmoothenBlock(a_ptr + x, a_stride, y_ptr + x, y_stride,
                          SIZE, SIZE)) {
          if (need_reset) {
            values[0] = y_ptr[x];
            values[1] = u_ptr[x >> 1];
            values[2] = v_ptr[x >> 1];
            need_reset = 0;
          }
          Flatten(y_ptr + x,        values[0], y_stride,  SIZE);
          Flatten(u_ptr + (x >> 1), values[1], uv_stride, SIZE2);
          Flatten(v_ptr + (x >> 1), values[2], uv_stride, SIZE2);
        } else {
          need_reset = 1;
        }
      }
      if (x < width) {
        SmoothenBlock(a_ptr + x, a_stride, y_ptr + x, y_stride,
                      width - x, SIZE);
      }
      a_ptr += SIZE * a_stride;
      y_ptr += SIZE * y_stride;
      u_ptr += SIZE2 * uv_stride;
      v_ptr += SIZE2 * uv_stride;
    }
    if (y < height) {
      const int sub_height = height - y;
      for (x = 0; x + SIZE <= width; x += SIZE) {
        SmoothenBlock(a_ptr + x, a_stride, y_ptr + x, y_stride,
                      SIZE, sub_height);
      }
      if (x < width) {
        SmoothenBlock(a_ptr + x, a_stride, y_ptr + x, y_stride,
                      width - x, sub_height);
      }
    }
  }
}

#undef SIZE
#undef SIZE2

// quality below which error-diffusion is enabled
#define ERROR_DIFFUSION_QUALITY 98

static void MapConfigToTools(VP8Encoder* const enc) {
  const WebPConfig* const config = enc->config_;
  const int method = config->method;
  const int limit = 100 - config->partition_limit;
  enc->method_ = method;
  enc->rd_opt_level_ = (method >= 6) ? RD_OPT_TRELLIS_ALL
                     : (method >= 5) ? RD_OPT_TRELLIS
                     : (method >= 3) ? RD_OPT_BASIC
                     : RD_OPT_NONE;
  enc->max_i4_header_bits_ =
      256 * 16 * 16 *                 // upper bound: up to 16bit per 4x4 block
      (limit * limit) / (100 * 100);  // ... modulated with a quadratic curve.

  // partition0 = 512k max.
  enc->mb_header_limit_ =
      (score_t)256 * 510 * 8 * 1024 / (enc->mb_w_ * enc->mb_h_);

  enc->thread_level_ = config->thread_level;

  enc->do_search_ = (config->target_size > 0 || config->target_PSNR > 0);
  if (!config->low_memory) {
#if !defined(DISABLE_TOKEN_BUFFER)
    enc->use_tokens_ = (enc->rd_opt_level_ >= RD_OPT_BASIC);  // need rd stats
#endif
    if (enc->use_tokens_) {
      enc->num_parts_ = 1;   // doesn't work with multi-partition
    }
  }
}

//------------------------------------------------------------------------------
// run-time tables (~4k)

static uint8_t clip1[255 + 510 + 1];    // clips [-255,510] to [0,255]

// We declare this variable 'volatile' to prevent instruction reordering
// and make sure it's set to true _last_ (so as to be thread-safe)
static volatile int tables_ok = 0;

static void InitTables(void) {
  if (!tables_ok) {
    int i;
    for (i = -255; i <= 255 + 255; ++i) {
      clip1[255 + i] = clip_8b(i);
    }
    tables_ok = 1;
  }
}

// Paragraph 13.5
const uint8_t
  VP8CoeffsProba0[NUM_TYPES][NUM_BANDS][NUM_CTX][NUM_PROBAS] = {
  { { { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128 },
      { 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128 },
      { 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128 }
    },
    { { 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128 },
      { 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128 },
      { 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128 },
    },
    { { 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128 },
      { 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128 },
      { 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128 },
    },
    { { 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128 },
      { 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128 },
      { 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128 }
    },
    { { 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128 },
      { 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128 },
      { 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128 }
    },
    { { 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128 },
      { 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128 },
      { 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128 }
    },
    { { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
    }
  },
  { { { 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62 },
      { 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1 },
      { 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128 }
    },
    { { 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128 },
      { 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128 },
      { 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128 }
    },
    { { 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128 },
      { 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128 },
      { 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128 }
    },
    { { 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128 },
      { 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128 },
      { 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128 }
    },
    { { 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128 },
      { 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128 },
      { 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128 }
    },
    { { 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128 },
      { 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128 },
      { 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128 }
    },
    { { 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128 },
      { 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128 },
      { 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128 }
    },
    { { 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128 },
      { 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128 }
    }
  },
  { { { 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128 },
      { 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128 },
      { 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128 }
    },
    { { 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128 },
      { 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128 },
      { 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128 }
    },
    { { 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128 },
      { 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128 },
      { 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128 }
    },
    { { 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128 },
      { 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128 }
    },
    { { 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128 },
      { 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
    }
  },
  { { { 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255 },
      { 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128 },
      { 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128 }
    },
    { { 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128 },
      { 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128 },
      { 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128 }
    },
    { { 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128 },
      { 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128 },
      { 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128 }
    },
    { { 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128 },
      { 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128 },
      { 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128 }
    },
    { { 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128 },
      { 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128 },
      { 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128 }
    },
    { { 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128 },
      { 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128 },
      { 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128 }
    },
    { { 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128 },
      { 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128 },
      { 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128 }
    },
    { { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    }
  }
};

void VP8DefaultProbas(VP8Encoder* const enc) {
  VP8EncProba* const probas = &enc->proba_;
  probas->use_skip_proba_ = 0;
  memset(probas->segments_, 255u, sizeof(probas->segments_));
  memcpy(probas->coeffs_, VP8CoeffsProba0, sizeof(VP8CoeffsProba0));
  // Note: we could hard-code the level_costs_ corresponding to VP8CoeffsProba0,
  // but that's ~11k of static data. Better call VP8CalculateLevelCosts() later.
  probas->dirty_ = 1;
}

static void ResetSegmentHeader(VP8Encoder* const enc) {
  VP8EncSegmentHeader* const hdr = &enc->segment_hdr_;
  hdr->num_segments_ = enc->config_->segments;
  hdr->update_map_  = (hdr->num_segments_ > 1);
  hdr->size_ = 0;
}

static void ResetFilterHeader(VP8Encoder* const enc) {
  VP8EncFilterHeader* const hdr = &enc->filter_hdr_;
  hdr->simple_ = 1;
  hdr->level_ = 0;
  hdr->sharpness_ = 0;
  hdr->i4x4_lf_delta_ = 0;
}

// intra prediction modes
enum { B_DC_PRED = 0,   // 4x4 modes
       B_TM_PRED = 1,
       B_VE_PRED = 2,
       B_HE_PRED = 3,
       B_RD_PRED = 4,
       B_VR_PRED = 5,
       B_LD_PRED = 6,
       B_VL_PRED = 7,
       B_HD_PRED = 8,
       B_HU_PRED = 9,
       NUM_BMODES = B_HU_PRED + 1 - B_DC_PRED,  // = 10

       // Luma16 or UV modes
       DC_PRED = B_DC_PRED, V_PRED = B_VE_PRED,
       H_PRED = B_HE_PRED, TM_PRED = B_TM_PRED,
       B_PRED = NUM_BMODES,   // refined I4x4 mode
       NUM_PRED_MODES = 4,

       // special modes
       B_DC_PRED_NOTOP = 4,
       B_DC_PRED_NOLEFT = 5,
       B_DC_PRED_NOTOPLEFT = 6,
       NUM_B_DC_MODES = 7 };

static void ResetBoundaryPredictions(VP8Encoder* const enc) {
  // init boundary values once for all
  // Note: actually, initializing the preds_[] is only needed for intra4.
  int i;
  uint8_t* const top = enc->preds_ - enc->preds_w_;
  uint8_t* const left = enc->preds_ - 1;
  for (i = -1; i < 4 * enc->mb_w_; ++i) {
    top[i] = B_DC_PRED;
  }
  for (i = 0; i < 4 * enc->mb_h_; ++i) {
    left[i * enc->preds_w_] = B_DC_PRED;
  }
  enc->nz_[-1] = 0;   // constant
}

int WebPPictureHasTransparency(const WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (!picture->use_argb) {
    return CheckNonOpaque(picture->a, picture->width, picture->height,
                          1, picture->a_stride);
  } else {
    const int alpha_offset = ALPHA_OFFSET;
    return CheckNonOpaque((const uint8_t*)picture->argb + alpha_offset,
                          picture->width, picture->height,
                          4, picture->argb_stride * sizeof(*picture->argb));
  }
  return 0;
}

typedef struct {
  // Must be called first, before any other method.
  void (*Init)(WebPWorker* const worker);
  // Must be called to initialize the object and spawn the thread. Re-entrant.
  // Will potentially launch the thread. Returns false in case of error.
  int (*Reset)(WebPWorker* const worker);
  // Makes sure the previous work is finished. Returns true if worker->had_error
  // was not set and no error condition was triggered by the working thread.
  int (*Sync)(WebPWorker* const worker);
  // Triggers the thread to call hook() with data1 and data2 arguments. These
  // hook/data1/data2 values can be changed at any time before calling this
  // function, but not be changed afterward until the next call to Sync().
  void (*Launch)(WebPWorker* const worker);
  // This function is similar to Launch() except that it calls the
  // hook directly instead of using a thread. Convenient to bypass the thread
  // mechanism while still using the WebPWorker structs. Sync() must
  // still be called afterward (for error reporting).
  void (*Execute)(WebPWorker* const worker);
  // Kill the thread and terminate the object. To use the object again, one
  // must call Reset() again.
  void (*End)(WebPWorker* const worker);
} WebPWorkerInterface;

static void Init(WebPWorker* const worker) {
  memset(worker, 0, sizeof(*worker));
  worker->status_ = NOT_OK;
}

static int Sync(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  ChangeState(worker, OK);
#endif
  assert(worker->status_ <= OK);
  return !worker->had_error;
}

static int Reset(WebPWorker* const worker) {
  int ok = 1;
  worker->had_error = 0;
  if (worker->status_ < OK) {
#ifdef WEBP_USE_THREAD
    WebPWorkerImpl* const impl =
        (WebPWorkerImpl*)WebPSafeCalloc(1, sizeof(WebPWorkerImpl));
    worker->impl_ = (void*)impl;
    if (worker->impl_ == NULL) {
      return 0;
    }
    if (pthread_mutex_init(&impl->mutex_, NULL)) {
      goto Error;
    }
    if (pthread_cond_init(&impl->condition_, NULL)) {
      pthread_mutex_destroy(&impl->mutex_);
      goto Error;
    }
    pthread_mutex_lock(&impl->mutex_);
    ok = !pthread_create(&impl->thread_, NULL, ThreadLoop, worker);
    if (ok) worker->status_ = OK;
    pthread_mutex_unlock(&impl->mutex_);
    if (!ok) {
      pthread_mutex_destroy(&impl->mutex_);
      pthread_cond_destroy(&impl->condition_);
 Error:
      WebPSafeFree(impl);
      worker->impl_ = NULL;
      return 0;
    }
#else
    worker->status_ = OK;
#endif
  } else if (worker->status_ > OK) {
    ok = Sync(worker);
  }
  assert(!ok || (worker->status_ == OK));
  return ok;
}

static void Execute(WebPWorker* const worker) {
  if (worker->hook != NULL) {
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
  }
}

static void Launch(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  ChangeState(worker, WORK);
#else
  Execute(worker);
#endif
}

static void End(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  if (worker->impl_ != NULL) {
    WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl_;
    ChangeState(worker, NOT_OK);
    pthread_join(impl->thread_, NULL);
    pthread_mutex_destroy(&impl->mutex_);
    pthread_cond_destroy(&impl->condition_);
    WebPSafeFree(impl);
    worker->impl_ = NULL;
  }
#else
  worker->status_ = NOT_OK;
  assert(worker->impl_ == NULL);
#endif
  assert(worker->status_ == NOT_OK);
}

static WebPWorkerInterface g_worker_interface = {
  Init, Reset, Sync, Launch, Execute, End
};

const WebPWorkerInterface* WebPGetWorkerInterface(void) {
  return &g_worker_interface;
}

typedef enum {     // Filter types.
  WEBP_FILTER_NONE = 0,
  WEBP_FILTER_HORIZONTAL,
  WEBP_FILTER_VERTICAL,
  WEBP_FILTER_GRADIENT,
  WEBP_FILTER_LAST = WEBP_FILTER_GRADIENT + 1,  // end marker
  WEBP_FILTER_BEST,    // meta-types
  WEBP_FILTER_FAST
} WEBP_FILTER_TYPE;

#define ALPHA_NO_COMPRESSION        0
#define ALPHA_LOSSLESS_COMPRESSION  1

void WebPCopyPlane(const uint8_t* src, int src_stride,
                   uint8_t* dst, int dst_stride, int width, int height) {
  assert(src != NULL && dst != NULL);
  assert(src_stride >= width && dst_stride >= width);
  while (height-- > 0) {
    memcpy(dst, src, width);
    src += src_stride;
    dst += dst_stride;
  }
}

#define NUM_SYMBOLS     256

#define MAX_ITER  6             // Maximum number of convergence steps.
#define ERROR_THRESHOLD 1e-4    // MSE stopping criterion.

int QuantizeLevels(uint8_t* const data, int width, int height,
                   int num_levels, uint64_t* const sse) {
  int freq[NUM_SYMBOLS] = { 0 };
  int q_level[NUM_SYMBOLS] = { 0 };
  double inv_q_level[NUM_SYMBOLS] = { 0 };
  int min_s = 255, max_s = 0;
  const size_t data_size = height * width;
  int i, num_levels_in, iter;
  double last_err = 1.e38, err = 0.;
  const double err_threshold = ERROR_THRESHOLD * data_size;

  if (data == NULL) {
    return 0;
  }

  if (width <= 0 || height <= 0) {
    return 0;
  }

  if (num_levels < 2 || num_levels > 256) {
    return 0;
  }

  {
    size_t n;
    num_levels_in = 0;
    for (n = 0; n < data_size; ++n) {
      num_levels_in += (freq[data[n]] == 0);
      if (min_s > data[n]) min_s = data[n];
      if (max_s < data[n]) max_s = data[n];
      ++freq[data[n]];
    }
  }

  if (num_levels_in <= num_levels) goto End;  // nothing to do!

  // Start with uniformly spread centroids.
  for (i = 0; i < num_levels; ++i) {
    inv_q_level[i] = min_s + (double)(max_s - min_s) * i / (num_levels - 1);
  }

  // Fixed values. Won't be changed.
  q_level[min_s] = 0;
  q_level[max_s] = num_levels - 1;
  assert(inv_q_level[0] == min_s);
  assert(inv_q_level[num_levels - 1] == max_s);

  // k-Means iterations.
  for (iter = 0; iter < MAX_ITER; ++iter) {
    double q_sum[NUM_SYMBOLS] = { 0 };
    double q_count[NUM_SYMBOLS] = { 0 };
    int s, slot = 0;

    // Assign classes to representatives.
    for (s = min_s; s <= max_s; ++s) {
      // Keep track of the nearest neighbour 'slot'
      while (slot < num_levels - 1 &&
             2 * s > inv_q_level[slot] + inv_q_level[slot + 1]) {
        ++slot;
      }
      if (freq[s] > 0) {
        q_sum[slot] += s * freq[s];
        q_count[slot] += freq[s];
      }
      q_level[s] = slot;
    }

    // Assign new representatives to classes.
    if (num_levels > 2) {
      for (slot = 1; slot < num_levels - 1; ++slot) {
        const double count = q_count[slot];
        if (count > 0.) {
          inv_q_level[slot] = q_sum[slot] / count;
        }
      }
    }

    // Compute convergence error.
    err = 0.;
    for (s = min_s; s <= max_s; ++s) {
      const double error = s - inv_q_level[q_level[s]];
      err += freq[s] * error * error;
    }

    // Check for convergence: we stop as soon as the error is no
    // longer improving.
    if (last_err - err < err_threshold) break;
    last_err = err;
  }

  // Remap the alpha plane to quantized values.
  {
    // double->int rounding operation can be costly, so we do it
    // once for all before remapping. We also perform the data[] -> slot
    // mapping, while at it (avoid one indirection in the final loop).
    uint8_t map[NUM_SYMBOLS];
    int s;
    size_t n;
    for (s = min_s; s <= max_s; ++s) {
      const int slot = q_level[s];
      map[s] = (uint8_t)(inv_q_level[slot] + .5);
    }
    // Final pass.
    for (n = 0; n < data_size; ++n) {
      data[n] = map[data[n]];
    }
  }
 End:
  // Store sum of squared error if needed.
  if (sse != NULL) *sse = (uint64_t)err;

  return 1;
}

// Small struct to hold the result of a filter mode compression attempt.
typedef struct {
  size_t score;
  VP8BitWriter bw;
  WebPAuxStats stats;
} FilterTrial;

static int GetNumColors(const uint8_t* data, int width, int height,
                        int stride) {
  int j;
  int colors = 0;
  uint8_t color[256] = { 0 };

  for (j = 0; j < height; ++j) {
    int i;
    const uint8_t* const p = data + j * stride;
    for (i = 0; i < width; ++i) {
      color[p[i]] = 1;
    }
  }
  for (j = 0; j < 256; ++j) {
    if (color[j] > 0) ++colors;
  }
  return colors;
}

#define SMAX 16
#define SDIFF(a, b) (abs((a) - (b)) >> 4)   // Scoring diff, in [0..SMAX)

static int GradientPredictor(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;  // clip to 8bit
}

WEBP_FILTER_TYPE WebPEstimateBestFilter(const uint8_t* data,
                                        int width, int height, int stride) {
  int i, j;
  int bins[WEBP_FILTER_LAST][SMAX];
  memset(bins, 0, sizeof(bins));

  // We only sample every other pixels. That's enough.
  for (j = 2; j < height - 1; j += 2) {
    const uint8_t* const p = data + j * stride;
    int mean = p[0];
    for (i = 2; i < width - 1; i += 2) {
      const int diff0 = SDIFF(p[i], mean);
      const int diff1 = SDIFF(p[i], p[i - 1]);
      const int diff2 = SDIFF(p[i], p[i - width]);
      const int grad_pred =
          GradientPredictor(p[i - 1], p[i - width], p[i - width - 1]);
      const int diff3 = SDIFF(p[i], grad_pred);
      bins[WEBP_FILTER_NONE][diff0] = 1;
      bins[WEBP_FILTER_HORIZONTAL][diff1] = 1;
      bins[WEBP_FILTER_VERTICAL][diff2] = 1;
      bins[WEBP_FILTER_GRADIENT][diff3] = 1;
      mean = (3 * mean + p[i] + 2) >> 2;
    }
  }
  {
    int filter;
    WEBP_FILTER_TYPE best_filter = WEBP_FILTER_NONE;
    int best_score = 0x7fffffff;
    for (filter = WEBP_FILTER_NONE; filter < WEBP_FILTER_LAST; ++filter) {
      int score = 0;
      for (i = 0; i < SMAX; ++i) {
        if (bins[filter][i] > 0) {
          score += i;
        }
      }
      if (score < best_score) {
        best_score = score;
        best_filter = (WEBP_FILTER_TYPE)filter;
      }
    }
    return best_filter;
  }
}

#undef SMAX
#undef SDIFF

#define FILTER_TRY_NONE (1 << WEBP_FILTER_NONE)
#define FILTER_TRY_ALL ((1 << WEBP_FILTER_LAST) - 1)

// Given the input 'filter' option, return an OR'd bit-set of filters to try.
static uint32_t GetFilterMap(const uint8_t* alpha, int width, int height,
                             int filter, int effort_level) {
  uint32_t bit_map = 0U;
  if (filter == WEBP_FILTER_FAST) {
    // Quick estimate of the best candidate.
    int try_filter_none = (effort_level > 3);
    const int kMinColorsForFilterNone = 16;
    const int kMaxColorsForFilterNone = 192;
    const int num_colors = GetNumColors(alpha, width, height, width);
    // For low number of colors, NONE yields better compression.
    filter = (num_colors <= kMinColorsForFilterNone)
        ? WEBP_FILTER_NONE
        : WebPEstimateBestFilter(alpha, width, height, width);
    bit_map |= 1 << filter;
    // For large number of colors, try FILTER_NONE in addition to the best
    // filter as well.
    if (try_filter_none || num_colors > kMaxColorsForFilterNone) {
      bit_map |= FILTER_TRY_NONE;
    }
  } else if (filter == WEBP_FILTER_NONE) {
    bit_map = FILTER_TRY_NONE;
  } else {  // WEBP_FILTER_BEST -> try all
    bit_map = FILTER_TRY_ALL;
  }
  return bit_map;
}

static int BitWriterResize(VP8BitWriter* const bw, size_t extra_size) {
  uint8_t* new_buf;
  size_t new_size;
  const uint64_t needed_size_64b = (uint64_t)bw->pos_ + extra_size;
  const size_t needed_size = (size_t)needed_size_64b;
  if (needed_size_64b != needed_size) {
    bw->error_ = 1;
    return 0;
  }
  if (needed_size <= bw->max_pos_) return 1;
  // If the following line wraps over 32bit, the test just after will catch it.
  new_size = 2 * bw->max_pos_;
  if (new_size < needed_size) new_size = needed_size;
  if (new_size < 1024) new_size = 1024;
  new_buf = (uint8_t*)WebPSafeMalloc(1ULL, new_size);
  if (new_buf == NULL) {
    bw->error_ = 1;
    return 0;
  }
  if (bw->pos_ > 0) {
    assert(bw->buf_ != NULL);
    memcpy(new_buf, bw->buf_, bw->pos_);
  }
  WebPSafeFree(bw->buf_);
  bw->buf_ = new_buf;
  bw->max_pos_ = new_size;
  return 1;
}

int VP8BitWriterInit(VP8BitWriter* const bw, size_t expected_size) {
  bw->range_   = 255 - 1;
  bw->value_   = 0;
  bw->run_     = 0;
  bw->nb_bits_ = -8;
  bw->pos_     = 0;
  bw->max_pos_ = 0;
  bw->error_   = 0;
  bw->buf_     = NULL;
  return (expected_size > 0) ? BitWriterResize(bw, expected_size) : 1;
}

static void InitFilterTrial(FilterTrial* const score) {
  score->score = (size_t)~0U;
  VP8BitWriterInit(&score->bw, 0);
}

typedef void (*WebPFilterFunc)(const uint8_t* in, int width, int height,
                               int stride, uint8_t* out);

typedef uint64_t vp8l_atype_t;   // accumulator type

typedef struct {
  vp8l_atype_t bits_;   // bit accumulator
  int          used_;   // number of bits used in accumulator
  uint8_t*     buf_;    // start of buffer
  uint8_t*     cur_;    // current write position
  uint8_t*     end_;    // end of buffer

  // After all bits are written (VP8LBitWriterFinish()), the caller must observe
  // the state of error_. A value of 1 indicates that a memory allocation
  // failure has happened during bit writing. A value of 0 indicates successful
  // writing of bits.
  int error_;
} VP8LBitWriter;

#define ALPHA_HEADER_LEN            1

# define SANITY_CHECK(in, out)                                                 \
  assert((in) != NULL);                                                        \
  assert((out) != NULL);                                                       \
  assert(width > 0);                                                           \
  assert(height > 0);                                                          \
  assert(stride >= width);                                                     \
  assert(row >= 0 && num_rows > 0 && row + num_rows <= height);                \
  (void)height;  // Silence unused warning.

static void PredictLine_C(const uint8_t* src, const uint8_t* pred,
                                      uint8_t* dst, int length, int inverse) {
  int i;
  if (inverse) {
    for (i = 0; i < length; ++i) dst[i] = src[i] + pred[i];
  } else {
    for (i = 0; i < length; ++i) dst[i] = src[i] - pred[i];
  }
}

static void DoHorizontalFilter_C(const uint8_t* in,
                                             int width, int height, int stride,
                                             int row, int num_rows,
                                             int inverse, uint8_t* out) {
  const uint8_t* preds;
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;
  preds = inverse ? out : in;

  if (row == 0) {
    // Leftmost pixel is the same as input for topmost scanline.
    out[0] = in[0];
    PredictLine_C(in + 1, preds, out + 1, width - 1, inverse);
    row = 1;
    preds += stride;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    // Leftmost pixel is predicted from above.
    PredictLine_C(in, preds - stride, out, 1, inverse);
    PredictLine_C(in + 1, preds, out + 1, width - 1, inverse);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

static void HorizontalFilter_C(const uint8_t* data, int width, int height,
                               int stride, uint8_t* filtered_data) {
  DoHorizontalFilter_C(data, width, height, stride, 0, height, 0,
                       filtered_data);
}

static void DoVerticalFilter_C(const uint8_t* in,
                                           int width, int height, int stride,
                                           int row, int num_rows,
                                           int inverse, uint8_t* out) {
  const uint8_t* preds;
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;
  preds = inverse ? out : in;

  if (row == 0) {
    // Very first top-left pixel is copied.
    out[0] = in[0];
    // Rest of top scan-line is left-predicted.
    PredictLine_C(in + 1, preds, out + 1, width - 1, inverse);
    row = 1;
    in += stride;
    out += stride;
  } else {
    // We are starting from in-between. Make sure 'preds' points to prev row.
    preds -= stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    PredictLine_C(in, preds, out, width, inverse);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

static void VerticalFilter_C(const uint8_t* data, int width, int height,
                             int stride, uint8_t* filtered_data) {
  DoVerticalFilter_C(data, width, height, stride, 0, height, 0, filtered_data);
}

static int GradientPredictor_C(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;  // clip to 8bit
}

static void DoGradientFilter_C(const uint8_t* in,
                                           int width, int height, int stride,
                                           int row, int num_rows,
                                           int inverse, uint8_t* out) {
  const uint8_t* preds;
  const size_t start_offset = row * stride;
  const int last_row = row + num_rows;
  SANITY_CHECK(in, out);
  in += start_offset;
  out += start_offset;
  preds = inverse ? out : in;

  // left prediction for top scan-line
  if (row == 0) {
    out[0] = in[0];
    PredictLine_C(in + 1, preds, out + 1, width - 1, inverse);
    row = 1;
    preds += stride;
    in += stride;
    out += stride;
  }

  // Filter line-by-line.
  while (row < last_row) {
    int w;
    // leftmost pixel: predict from above.
    PredictLine_C(in, preds - stride, out, 1, inverse);
    for (w = 1; w < width; ++w) {
      const int pred = GradientPredictor_C(preds[w - 1],
                                           preds[w - stride],
                                           preds[w - stride - 1]);
      out[w] = in[w] + (inverse ? pred : -pred);
    }
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

static void GradientFilter_C(const uint8_t* data, int width, int height,
                             int stride, uint8_t* filtered_data) {
  DoGradientFilter_C(data, width, height, stride, 0, height, 0, filtered_data);
}

// Returns 1 on success.
static int VP8LBitWriterResize(VP8LBitWriter* const bw, size_t extra_size) {
  uint8_t* allocated_buf;
  size_t allocated_size;
  const size_t max_bytes = bw->end_ - bw->buf_;
  const size_t current_size = bw->cur_ - bw->buf_;
  const uint64_t size_required_64b = (uint64_t)current_size + extra_size;
  const size_t size_required = (size_t)size_required_64b;
  if (size_required != size_required_64b) {
    bw->error_ = 1;
    return 0;
  }
  if (max_bytes > 0 && size_required <= max_bytes) return 1;
  allocated_size = (3 * max_bytes) >> 1;
  if (allocated_size < size_required) allocated_size = size_required;
  // make allocated size multiple of 1k
  allocated_size = (((allocated_size >> 10) + 1) << 10);
  allocated_buf = (uint8_t*)WebPSafeMalloc(1ULL, allocated_size);
  if (allocated_buf == NULL) {
    bw->error_ = 1;
    return 0;
  }
  if (current_size > 0) {
    memcpy(allocated_buf, bw->buf_, current_size);
  }
  WebPSafeFree(bw->buf_);
  bw->buf_ = allocated_buf;
  bw->cur_ = bw->buf_ + current_size;
  bw->end_ = bw->buf_ + allocated_size;
  return 1;
}

int VP8LBitWriterInit(VP8LBitWriter* const bw, size_t expected_size) {
  memset(bw, 0, sizeof(*bw));
  return VP8LBitWriterResize(bw, expected_size);
}

static void DispatchAlphaToGreen_C(const uint8_t* alpha, int alpha_stride,
                                   int width, int height,
                                   uint32_t* dst, int dst_stride) {
  int i, j;
  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      dst[i] = alpha[i] << 8;  // leave A/R/B channels zero'd.
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }
}

typedef enum {
  kEncoderNone = 0,
  kEncoderARGB,
  kEncoderNearLossless,
  kEncoderPalette
} VP8LEncoderARGBContent;

#define MAX_PALETTE_SIZE             256

typedef struct VP8LHashChain VP8LHashChain;
struct VP8LHashChain {
  // The 20 most significant bits contain the offset at which the best match
  // is found. These 20 bits are the limit defined by GetWindowSizeForHashChain
  // (through WINDOW_SIZE = 1<<20).
  // The lower 12 bits contain the length of the match. The 12 bit limit is
  // defined in MaxFindCopyLength with MAX_LENGTH=4096.
  uint32_t* offset_length_;
  // This is the maximum size of the hash_chain that can be constructed.
  // Typically this is the pixel count (width x height) for a given image.
  int size_;
};

typedef struct {
  const WebPConfig* config_;      // user configuration and parameters
  const WebPPicture* pic_;        // input picture.

  uint32_t* argb_;                       // Transformed argb image data.
  VP8LEncoderARGBContent argb_content_;  // Content type of the argb buffer.
  uint32_t* argb_scratch_;               // Scratch memory for argb rows
                                         // (used for prediction).
  uint32_t* transform_data_;             // Scratch memory for transform data.
  uint32_t* transform_mem_;              // Currently allocated memory.
  size_t    transform_mem_size_;         // Currently allocated memory size.

  int       current_width_;       // Corresponds to packed image width.

  // Encoding parameters derived from quality parameter.
  int histo_bits_;
  int transform_bits_;    // <= MAX_TRANSFORM_BITS.
  int cache_bits_;        // If equal to 0, don't use color cache.

  // Encoding parameters derived from image characteristics.
  int use_cross_color_;
  int use_subtract_green_;
  int use_predict_;
  int use_palette_;
  int palette_size_;
  uint32_t palette_[MAX_PALETTE_SIZE];

  // Some 'scratch' (potentially large) objects.
  struct VP8LBackwardRefs refs_[3];  // Backward Refs array for temporaries.
  VP8LHashChain hash_chain_;         // HashChain data for constructing
                                     // backward references.
} VP8LEncoder;

void* WebPSafeCalloc(uint64_t nmemb, size_t size) {
  void* ptr;
  Increment(&num_calloc_calls);
  if (!CheckSizeArgumentsOverflow(nmemb, size)) return NULL;
  assert(nmemb * size > 0);
  ptr = calloc((size_t)nmemb, size);
  AddMem(ptr, (size_t)(nmemb * size));
  return ptr;
}

static VP8LEncoder* VP8LEncoderNew(const WebPConfig* const config,
                                   const WebPPicture* const picture) {
  VP8LEncoder* const enc = (VP8LEncoder*)WebPSafeCalloc(1ULL, sizeof(*enc));
  if (enc == NULL) {
    WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  enc->config_ = config;
  enc->pic_ = picture;
  enc->argb_content_ = kEncoderNone;

  return enc;
}

typedef enum {
  kDirect = 0,
  kSpatial = 1,
  kSubGreen = 2,
  kSpatialSubGreen = 3,
  kPalette = 4,
  kNumEntropyIx = 5
} EntropyIx;

#define CRUNCH_CONFIGS_MAX kNumEntropyIx
#define CRUNCH_CONFIGS_LZ77_MAX 2

typedef struct {
  int entropy_idx_;
  int lz77s_types_to_try_[CRUNCH_CONFIGS_LZ77_MAX];
  int lz77s_types_to_try_size_;
} CrunchConfig;

typedef struct {
  const WebPConfig* config_;
  const WebPPicture* picture_;
  VP8LBitWriter* bw_;
  VP8LEncoder* enc_;
  int use_cache_;
  CrunchConfig crunch_configs_[CRUNCH_CONFIGS_MAX];
  int num_crunch_configs_;
  int red_and_blue_always_zero_;
  WebPEncodingError err_;
  WebPAuxStats* stats_;
} StreamEncodeContext;

static const uint64_t kHashMul = 0x1e35a7bdull;

static int VP8LHashPix(uint32_t argb, int shift) {
  return (int)(((argb * kHashMul) & 0xffffffffu) >> shift);
}

#define COLOR_HASH_SIZE         (MAX_PALETTE_SIZE * 4)
#define COLOR_HASH_RIGHT_SHIFT  22  // 32 - log2(COLOR_HASH_SIZE).

int WebPGetColorPalette(const WebPPicture* const pic, uint32_t* const palette) {
  int i;
  int x, y;
  int num_colors = 0;
  uint8_t in_use[COLOR_HASH_SIZE] = { 0 };
  uint32_t colors[COLOR_HASH_SIZE];
  const uint32_t* argb = pic->argb;
  const int width = pic->width;
  const int height = pic->height;
  uint32_t last_pix = ~argb[0];   // so we're sure that last_pix != argb[0]
  assert(pic != NULL);
  assert(pic->use_argb);

  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      int key;
      if (argb[x] == last_pix) {
        continue;
      }
      last_pix = argb[x];
      key = VP8LHashPix(last_pix, COLOR_HASH_RIGHT_SHIFT);
      while (1) {
        if (!in_use[key]) {
          colors[key] = last_pix;
          in_use[key] = 1;
          ++num_colors;
          if (num_colors > MAX_PALETTE_SIZE) {
            return MAX_PALETTE_SIZE + 1;  // Exact count not needed.
          }
          break;
        } else if (colors[key] == last_pix) {
          break;  // The color is already there.
        } else {
          // Some other color sits here, so do linear conflict resolution.
          ++key;
          key &= (COLOR_HASH_SIZE - 1);  // Key mask.
        }
      }
    }
    argb += pic->argb_stride;
  }

  if (palette != NULL) {  // Fill the colors into palette.
    num_colors = 0;
    for (i = 0; i < COLOR_HASH_SIZE; ++i) {
      if (in_use[i]) {
        palette[num_colors] = colors[i];
        ++num_colors;
      }
    }
  }
  return num_colors;
}

#undef COLOR_HASH_SIZE
#undef COLOR_HASH_RIGHT_SHIFT

static uint32_t WebPMemToUint32(const uint8_t* const ptr) {
  uint32_t A;
  memcpy(&A, ptr, sizeof(A));
  return A;
}

static int PaletteCompareColorsForQsort(const void* p1, const void* p2) {
  const uint32_t a = WebPMemToUint32((uint8_t*)p1);
  const uint32_t b = WebPMemToUint32((uint8_t*)p2);
  assert(a != b);
  return (a < b) ? -1 : 1;
}

static uint32_t VP8LSubPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green =
      0x00ff00ffu + (a & 0xff00ff00u) - (b & 0xff00ff00u);
  const uint32_t red_and_blue =
      0xff00ff00u + (a & 0x00ff00ffu) - (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

static int PaletteHasNonMonotonousDeltas(uint32_t palette[], int num_colors) {
  uint32_t predict = 0x000000;
  int i;
  uint8_t sign_found = 0x00;
  for (i = 0; i < num_colors; ++i) {
    const uint32_t diff = VP8LSubPixels(palette[i], predict);
    const uint8_t rd = (diff >> 16) & 0xff;
    const uint8_t gd = (diff >>  8) & 0xff;
    const uint8_t bd = (diff >>  0) & 0xff;
    if (rd != 0x00) {
      sign_found |= (rd < 0x80) ? 1 : 2;
    }
    if (gd != 0x00) {
      sign_found |= (gd < 0x80) ? 8 : 16;
    }
    if (bd != 0x00) {
      sign_found |= (bd < 0x80) ? 64 : 128;
    }
    predict = palette[i];
  }
  return (sign_found & (sign_found << 1)) != 0;  // two consequent signs.
}

static uint32_t PaletteComponentDistance(uint32_t v) {
  return (v <= 128) ? v : (256 - v);
}

static uint32_t PaletteColorDistance(uint32_t col1, uint32_t col2) {
  const uint32_t diff = VP8LSubPixels(col1, col2);
  const int kMoreWeightForRGBThanForAlpha = 9;
  uint32_t score;
  score =  PaletteComponentDistance((diff >>  0) & 0xff);
  score += PaletteComponentDistance((diff >>  8) & 0xff);
  score += PaletteComponentDistance((diff >> 16) & 0xff);
  score *= kMoreWeightForRGBThanForAlpha;
  score += PaletteComponentDistance((diff >> 24) & 0xff);
  return score;
}

static void SwapColor(uint32_t* const col1, uint32_t* const col2) {
  const uint32_t tmp = *col1;
  *col1 = *col2;
  *col2 = tmp;
}

static void GreedyMinimizeDeltas(uint32_t palette[], int num_colors) {
  // Find greedily always the closest color of the predicted color to minimize
  // deltas in the palette. This reduces storage needs since the
  // palette is stored with delta encoding.
  uint32_t predict = 0x00000000;
  int i, k;
  for (i = 0; i < num_colors; ++i) {
    int best_ix = i;
    uint32_t best_score = ~0U;
    for (k = i; k < num_colors; ++k) {
      const uint32_t cur_score = PaletteColorDistance(palette[k], predict);
      if (best_score > cur_score) {
        best_score = cur_score;
        best_ix = k;
      }
    }
    SwapColor(&palette[best_ix], &palette[i]);
    predict = palette[i];
  }
}

static int AnalyzeAndCreatePalette(const WebPPicture* const pic,
                                   int low_effort,
                                   uint32_t palette[MAX_PALETTE_SIZE],
                                   int* const palette_size) {
  const int num_colors = WebPGetColorPalette(pic, palette);
  if (num_colors > MAX_PALETTE_SIZE) {
    *palette_size = 0;
    return 0;
  }
  *palette_size = num_colors;
  qsort(palette, num_colors, sizeof(*palette), PaletteCompareColorsForQsort);
  if (!low_effort && PaletteHasNonMonotonousDeltas(palette, num_colors)) {
    GreedyMinimizeDeltas(palette, num_colors);
  }
  return 1;
}

static uint32_t VP8LSubSampleSize(uint32_t size,
                                              uint32_t sampling_bits) {
  return (size + (1 << sampling_bits) - 1) >> sampling_bits;
}

#define MAX_HUFF_IMAGE_SIZE       2600
#define MIN_HUFFMAN_BITS             2  // min number of Huffman bits
#define MAX_HUFFMAN_BITS             9  // max number of Huffman bits

static int GetHistoBits(int method, int use_palette, int width, int height) {
  // Make tile size a function of encoding method (Range: 0 to 6).
  int histo_bits = (use_palette ? 9 : 7) - method;
  while (1) {
    const int huff_image_size = VP8LSubSampleSize(width, histo_bits) *
                                VP8LSubSampleSize(height, histo_bits);
    if (huff_image_size <= MAX_HUFF_IMAGE_SIZE) break;
    ++histo_bits;
  }
  return (histo_bits < MIN_HUFFMAN_BITS) ? MIN_HUFFMAN_BITS :
         (histo_bits > MAX_HUFFMAN_BITS) ? MAX_HUFFMAN_BITS : histo_bits;
}

#define MAX_TRANSFORM_BITS 6

static int GetTransformBits(int method, int histo_bits) {
  const int max_transform_bits = (method < 4) ? 6 : (method > 4) ? 4 : 5;
  const int res =
      (histo_bits > max_transform_bits) ? max_transform_bits : histo_bits;
  assert(res <= MAX_TRANSFORM_BITS);
  return res;
}

typedef enum {
  kHistoAlpha = 0,
  kHistoAlphaPred,
  kHistoGreen,
  kHistoGreenPred,
  kHistoRed,
  kHistoRedPred,
  kHistoBlue,
  kHistoBluePred,
  kHistoRedSubGreen,
  kHistoRedPredSubGreen,
  kHistoBlueSubGreen,
  kHistoBluePredSubGreen,
  kHistoPalette,
  kHistoTotal  // Must be last.
} HistoIx;

static void AddSingle(uint32_t p,
                      uint32_t* const a, uint32_t* const r,
                      uint32_t* const g, uint32_t* const b) {
  ++a[(p >> 24) & 0xff];
  ++r[(p >> 16) & 0xff];
  ++g[(p >>  8) & 0xff];
  ++b[(p >>  0) & 0xff];
}

static void AddSingleSubGreen(int p, uint32_t* const r, uint32_t* const b) {
  const int green = p >> 8;  // The upper bits are masked away later.
  ++r[((p >> 16) - green) & 0xff];
  ++b[((p >>  0) - green) & 0xff];
}

static uint32_t HashPix(uint32_t pix) {
  // Note that masking with 0xffffffffu is for preventing an
  // 'unsigned int overflow' warning. Doesn't impact the compiled code.
  return ((((uint64_t)pix + (pix >> 19)) * 0x39c5fba7ull) & 0xffffffffu) >> 24;
}

typedef struct {            // small struct to hold bit entropy results
  double entropy;           // entropy
  uint32_t sum;             // sum of the population
  int nonzeros;             // number of non-zero elements in the population
  uint32_t max_val;         // maximum value in the population
  uint32_t nonzero_code;    // index of the last non-zero in the population
} VP8LBitEntropy;

#define VP8L_NON_TRIVIAL_SYM (0xffffffff)

void VP8LBitEntropyInit(VP8LBitEntropy* const entropy) {
  entropy->entropy = 0.;
  entropy->sum = 0;
  entropy->nonzeros = 0;
  entropy->max_val = 0;
  entropy->nonzero_code = VP8L_NON_TRIVIAL_SYM;
}

#define LOG_LOOKUP_IDX_MAX 256

const float kSLog2Table[LOG_LOOKUP_IDX_MAX] = {
  0.00000000f,    0.00000000f,  2.00000000f,   4.75488750f,
  8.00000000f,   11.60964047f,  15.50977500f,  19.65148445f,
  24.00000000f,  28.52932501f,  33.21928095f,  38.05374781f,
  43.01955001f,  48.10571634f,  53.30296891f,  58.60335893f,
  64.00000000f,  69.48686830f,  75.05865003f,  80.71062276f,
  86.43856190f,  92.23866588f,  98.10749561f,  104.04192499f,
  110.03910002f, 116.09640474f, 122.21143267f, 128.38196256f,
  134.60593782f, 140.88144886f, 147.20671787f, 153.58008562f,
  160.00000000f, 166.46500594f, 172.97373660f, 179.52490559f,
  186.11730005f, 192.74977453f, 199.42124551f, 206.13068654f,
  212.87712380f, 219.65963219f, 226.47733176f, 233.32938445f,
  240.21499122f, 247.13338933f, 254.08384998f, 261.06567603f,
  268.07820003f, 275.12078236f, 282.19280949f, 289.29369244f,
  296.42286534f, 303.57978409f, 310.76392512f, 317.97478424f,
  325.21187564f, 332.47473081f, 339.76289772f, 347.07593991f,
  354.41343574f, 361.77497759f, 369.16017124f, 376.56863518f,
  384.00000000f, 391.45390785f, 398.93001188f, 406.42797576f,
  413.94747321f, 421.48818752f, 429.04981119f, 436.63204548f,
  444.23460010f, 451.85719280f, 459.49954906f, 467.16140179f,
  474.84249102f, 482.54256363f, 490.26137307f, 497.99867911f,
  505.75424759f, 513.52785023f, 521.31926438f, 529.12827280f,
  536.95466351f, 544.79822957f, 552.65876890f, 560.53608414f,
  568.42998244f, 576.34027536f, 584.26677867f, 592.20931226f,
  600.16769996f, 608.14176943f, 616.13135206f, 624.13628279f,
  632.15640007f, 640.19154569f, 648.24156472f, 656.30630539f,
  664.38561898f, 672.47935976f, 680.58738488f, 688.70955430f,
  696.84573069f, 704.99577935f, 713.15956818f, 721.33696754f,
  729.52785023f, 737.73209140f, 745.94956849f, 754.18016116f,
  762.42375127f, 770.68022275f, 778.94946161f, 787.23135586f,
  795.52579543f, 803.83267219f, 812.15187982f, 820.48331383f,
  828.82687147f, 837.18245171f, 845.54995518f, 853.92928416f,
  862.32034249f, 870.72303558f, 879.13727036f, 887.56295522f,
  896.00000000f, 904.44831595f, 912.90781569f, 921.37841320f,
  929.86002376f, 938.35256392f, 946.85595152f, 955.37010560f,
  963.89494641f, 972.43039537f, 980.97637504f, 989.53280911f,
  998.09962237f, 1006.67674069f, 1015.26409097f, 1023.86160116f,
  1032.46920021f, 1041.08681805f, 1049.71438560f, 1058.35183469f,
  1066.99909811f, 1075.65610955f, 1084.32280357f, 1092.99911564f,
  1101.68498204f, 1110.38033993f, 1119.08512727f, 1127.79928282f,
  1136.52274614f, 1145.25545758f, 1153.99735821f, 1162.74838989f,
  1171.50849518f, 1180.27761738f, 1189.05570047f, 1197.84268914f,
  1206.63852876f, 1215.44316535f, 1224.25654560f, 1233.07861684f,
  1241.90932703f, 1250.74862473f, 1259.59645914f, 1268.45278005f,
  1277.31753781f, 1286.19068338f, 1295.07216828f, 1303.96194457f,
  1312.85996488f, 1321.76618236f, 1330.68055071f, 1339.60302413f,
  1348.53355734f, 1357.47210556f, 1366.41862452f, 1375.37307041f,
  1384.33539991f, 1393.30557020f, 1402.28353887f, 1411.26926400f,
  1420.26270412f, 1429.26381818f, 1438.27256558f, 1447.28890615f,
  1456.31280014f, 1465.34420819f, 1474.38309138f, 1483.42941118f,
  1492.48312945f, 1501.54420843f, 1510.61261078f, 1519.68829949f,
  1528.77123795f, 1537.86138993f, 1546.95871952f, 1556.06319119f,
  1565.17476976f, 1574.29342040f, 1583.41910860f, 1592.55180020f,
  1601.69146137f, 1610.83805860f, 1619.99155871f, 1629.15192882f,
  1638.31913637f, 1647.49314911f, 1656.67393509f, 1665.86146266f,
  1675.05570047f, 1684.25661744f, 1693.46418280f, 1702.67836605f,
  1711.89913698f, 1721.12646563f, 1730.36032233f, 1739.60067768f,
  1748.84750254f, 1758.10076802f, 1767.36044551f, 1776.62650662f,
  1785.89892323f, 1795.17766747f, 1804.46271172f, 1813.75402857f,
  1823.05159087f, 1832.35537170f, 1841.66534438f, 1850.98148244f,
  1860.30375965f, 1869.63214999f, 1878.96662767f, 1888.30716711f,
  1897.65374295f, 1907.00633003f, 1916.36490342f, 1925.72943838f,
  1935.09991037f, 1944.47629506f, 1953.85856831f, 1963.24670620f,
  1972.64068498f, 1982.04048108f, 1991.44607117f, 2000.85743204f,
  2010.27454072f, 2019.69737440f, 2029.12591044f, 2038.56012640f
};

#define APPROX_LOG_WITH_CORRECTION_MAX  65536

// lookup table for small values of log2(int)
const float kLog2Table[LOG_LOOKUP_IDX_MAX] = {
  0.0000000000000000f, 0.0000000000000000f,
  1.0000000000000000f, 1.5849625007211560f,
  2.0000000000000000f, 2.3219280948873621f,
  2.5849625007211560f, 2.8073549220576041f,
  3.0000000000000000f, 3.1699250014423121f,
  3.3219280948873621f, 3.4594316186372973f,
  3.5849625007211560f, 3.7004397181410921f,
  3.8073549220576041f, 3.9068905956085187f,
  4.0000000000000000f, 4.0874628412503390f,
  4.1699250014423121f, 4.2479275134435852f,
  4.3219280948873626f, 4.3923174227787606f,
  4.4594316186372973f, 4.5235619560570130f,
  4.5849625007211560f, 4.6438561897747243f,
  4.7004397181410917f, 4.7548875021634682f,
  4.8073549220576037f, 4.8579809951275718f,
  4.9068905956085187f, 4.9541963103868749f,
  5.0000000000000000f, 5.0443941193584533f,
  5.0874628412503390f, 5.1292830169449663f,
  5.1699250014423121f, 5.2094533656289501f,
  5.2479275134435852f, 5.2854022188622487f,
  5.3219280948873626f, 5.3575520046180837f,
  5.3923174227787606f, 5.4262647547020979f,
  5.4594316186372973f, 5.4918530963296747f,
  5.5235619560570130f, 5.5545888516776376f,
  5.5849625007211560f, 5.6147098441152083f,
  5.6438561897747243f, 5.6724253419714951f,
  5.7004397181410917f, 5.7279204545631987f,
  5.7548875021634682f, 5.7813597135246599f,
  5.8073549220576037f, 5.8328900141647412f,
  5.8579809951275718f, 5.8826430493618415f,
  5.9068905956085187f, 5.9307373375628866f,
  5.9541963103868749f, 5.9772799234999167f,
  6.0000000000000000f, 6.0223678130284543f,
  6.0443941193584533f, 6.0660891904577720f,
  6.0874628412503390f, 6.1085244567781691f,
  6.1292830169449663f, 6.1497471195046822f,
  6.1699250014423121f, 6.1898245588800175f,
  6.2094533656289501f, 6.2288186904958804f,
  6.2479275134435852f, 6.2667865406949010f,
  6.2854022188622487f, 6.3037807481771030f,
  6.3219280948873626f, 6.3398500028846243f,
  6.3575520046180837f, 6.3750394313469245f,
  6.3923174227787606f, 6.4093909361377017f,
  6.4262647547020979f, 6.4429434958487279f,
  6.4594316186372973f, 6.4757334309663976f,
  6.4918530963296747f, 6.5077946401986963f,
  6.5235619560570130f, 6.5391588111080309f,
  6.5545888516776376f, 6.5698556083309478f,
  6.5849625007211560f, 6.5999128421871278f,
  6.6147098441152083f, 6.6293566200796094f,
  6.6438561897747243f, 6.6582114827517946f,
  6.6724253419714951f, 6.6865005271832185f,
  6.7004397181410917f, 6.7142455176661224f,
  6.7279204545631987f, 6.7414669864011464f,
  6.7548875021634682f, 6.7681843247769259f,
  6.7813597135246599f, 6.7944158663501061f,
  6.8073549220576037f, 6.8201789624151878f,
  6.8328900141647412f, 6.8454900509443747f,
  6.8579809951275718f, 6.8703647195834047f,
  6.8826430493618415f, 6.8948177633079437f,
  6.9068905956085187f, 6.9188632372745946f,
  6.9307373375628866f, 6.9425145053392398f,
  6.9541963103868749f, 6.9657842846620869f,
  6.9772799234999167f, 6.9886846867721654f,
  7.0000000000000000f, 7.0112272554232539f,
  7.0223678130284543f, 7.0334230015374501f,
  7.0443941193584533f, 7.0552824355011898f,
  7.0660891904577720f, 7.0768155970508308f,
  7.0874628412503390f, 7.0980320829605263f,
  7.1085244567781691f, 7.1189410727235076f,
  7.1292830169449663f, 7.1395513523987936f,
  7.1497471195046822f, 7.1598713367783890f,
  7.1699250014423121f, 7.1799090900149344f,
  7.1898245588800175f, 7.1996723448363644f,
  7.2094533656289501f, 7.2191685204621611f,
  7.2288186904958804f, 7.2384047393250785f,
  7.2479275134435852f, 7.2573878426926521f,
  7.2667865406949010f, 7.2761244052742375f,
  7.2854022188622487f, 7.2946207488916270f,
  7.3037807481771030f, 7.3128829552843557f,
  7.3219280948873626f, 7.3309168781146167f,
  7.3398500028846243f, 7.3487281542310771f,
  7.3575520046180837f, 7.3663222142458160f,
  7.3750394313469245f, 7.3837042924740519f,
  7.3923174227787606f, 7.4008794362821843f,
  7.4093909361377017f, 7.4178525148858982f,
  7.4262647547020979f, 7.4346282276367245f,
  7.4429434958487279f, 7.4512111118323289f,
  7.4594316186372973f, 7.4676055500829976f,
  7.4757334309663976f, 7.4838157772642563f,
  7.4918530963296747f, 7.4998458870832056f,
  7.5077946401986963f, 7.5156998382840427f,
  7.5235619560570130f, 7.5313814605163118f,
  7.5391588111080309f, 7.5468944598876364f,
  7.5545888516776376f, 7.5622424242210728f,
  7.5698556083309478f, 7.5774288280357486f,
  7.5849625007211560f, 7.5924570372680806f,
  7.5999128421871278f, 7.6073303137496104f,
  7.6147098441152083f, 7.6220518194563764f,
  7.6293566200796094f, 7.6366246205436487f,
  7.6438561897747243f, 7.6510516911789281f,
  7.6582114827517946f, 7.6653359171851764f,
  7.6724253419714951f, 7.6794800995054464f,
  7.6865005271832185f, 7.6934869574993252f,
  7.7004397181410917f, 7.7073591320808825f,
  7.7142455176661224f, 7.7210991887071855f,
  7.7279204545631987f, 7.7347096202258383f,
  7.7414669864011464f, 7.7481928495894605f,
  7.7548875021634682f, 7.7615512324444795f,
  7.7681843247769259f, 7.7747870596011736f,
  7.7813597135246599f, 7.7879025593914317f,
  7.7944158663501061f, 7.8008998999203047f,
  7.8073549220576037f, 7.8137811912170374f,
  7.8201789624151878f, 7.8265484872909150f,
  7.8328900141647412f, 7.8392037880969436f,
  7.8454900509443747f, 7.8517490414160571f,
  7.8579809951275718f, 7.8641861446542797f,
  7.8703647195834047f, 7.8765169465649993f,
  7.8826430493618415f, 7.8887432488982591f,
  7.8948177633079437f, 7.9008668079807486f,
  7.9068905956085187f, 7.9128893362299619f,
  7.9188632372745946f, 7.9248125036057812f,
  7.9307373375628866f, 7.9366379390025709f,
  7.9425145053392398f, 7.9483672315846778f,
  7.9541963103868749f, 7.9600019320680805f,
  7.9657842846620869f, 7.9715435539507719f,
  7.9772799234999167f, 7.9829935746943103f,
  7.9886846867721654f, 7.9943534368588577f
};

#define LOG_2_RECIPROCAL 1.44269504088896338700465094007086

static float FastSLog2Slow_C(uint32_t v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    int log_cnt = 0;
    uint32_t y = 1;
    int correction = 0;
    const float v_f = (float)v;
    const uint32_t orig_v = v;
    do {
      ++log_cnt;
      v = v >> 1;
      y = y << 1;
    } while (v >= LOG_LOOKUP_IDX_MAX);
    // vf = (2^log_cnt) * Xf; where y = 2^log_cnt and Xf < 256
    // Xf = floor(Xf) * (1 + (v % y) / v)
    // log2(Xf) = log2(floor(Xf)) + log2(1 + (v % y) / v)
    // The correction factor: log(1 + d) ~ d; for very small d values, so
    // log2(1 + (v % y) / v) ~ LOG_2_RECIPROCAL * (v % y)/v
    // LOG_2_RECIPROCAL ~ 23/16
    correction = (23 * (orig_v & (y - 1))) >> 4;
    return v_f * (kLog2Table[v] + log_cnt) + correction;
  } else {
    return (float)(LOG_2_RECIPROCAL * v * log((double)v));
  }
}

static float VP8LFastSLog2(uint32_t v) {
  return (v < LOG_LOOKUP_IDX_MAX) ? kSLog2Table[v] : FastSLog2Slow_C(v);
}

void VP8LBitsEntropyUnrefined(const uint32_t* const array, int n,
                              VP8LBitEntropy* const entropy) {
  int i;

  VP8LBitEntropyInit(entropy);

  for (i = 0; i < n; ++i) {
    if (array[i] != 0) {
      entropy->sum += array[i];
      entropy->nonzero_code = i;
      ++entropy->nonzeros;
      entropy->entropy -= VP8LFastSLog2(array[i]);
      if (entropy->max_val < array[i]) {
        entropy->max_val = array[i];
      }
    }
  }
  entropy->entropy += VP8LFastSLog2(entropy->sum);
}

static double BitsEntropyRefine(const VP8LBitEntropy* entropy) {
  double mix;
  if (entropy->nonzeros < 5) {
    if (entropy->nonzeros <= 1) {
      return 0;
    }
    // Two symbols, they will be 0 and 1 in a Huffman code.
    // Let's mix in a bit of entropy to favor good clustering when
    // distributions of these are combined.
    if (entropy->nonzeros == 2) {
      return 0.99 * entropy->sum + 0.01 * entropy->entropy;
    }
    // No matter what the entropy says, we cannot be better than min_limit
    // with Huffman coding. I am mixing a bit of entropy into the
    // min_limit since it produces much better (~0.5 %) compression results
    // perhaps because of better entropy clustering.
    if (entropy->nonzeros == 3) {
      mix = 0.95;
    } else {
      mix = 0.7;  // nonzeros == 4.
    }
  } else {
    mix = 0.627;
  }

  {
    double min_limit = 2 * entropy->sum - entropy->max_val;
    min_limit = mix * min_limit + (1.0 - mix) * entropy->entropy;
    return (entropy->entropy < min_limit) ? min_limit : entropy->entropy;
  }
}

double VP8LBitsEntropy(const uint32_t* const array, int n) {
  VP8LBitEntropy entropy;
  VP8LBitsEntropyUnrefined(array, n, &entropy);

  return BitsEntropyRefine(&entropy);
}

#define APPROX_LOG_MAX                   4096

static float FastLog2Slow_C(uint32_t v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    int log_cnt = 0;
    uint32_t y = 1;
    const uint32_t orig_v = v;
    double log_2;
    do {
      ++log_cnt;
      v = v >> 1;
      y = y << 1;
    } while (v >= LOG_LOOKUP_IDX_MAX);
    log_2 = kLog2Table[v] + log_cnt;
    if (orig_v >= APPROX_LOG_MAX) {
      // Since the division is still expensive, add this correction factor only
      // for large values of 'v'.
      const int correction = (23 * (orig_v & (y - 1))) >> 4;
      log_2 += (double)correction / orig_v;
    }
    return (float)log_2;
  } else {
    return (float)(LOG_2_RECIPROCAL * log((double)v));
  }
}

static float VP8LFastLog2(uint32_t v) {
  return (v < LOG_LOOKUP_IDX_MAX) ? kLog2Table[v] : FastLog2Slow_C(v);
}

static int AnalyzeEntropy(const uint32_t* argb,
                          int width, int height, int argb_stride,
                          int use_palette,
                          int palette_size, int transform_bits,
                          EntropyIx* const min_entropy_ix,
                          int* const red_and_blue_always_zero) {
  // Allocate histogram set with cache_bits = 0.
  uint32_t* histo;

  if (use_palette && palette_size <= 16) {
    // In the case of small palettes, we pack 2, 4 or 8 pixels together. In
    // practice, small palettes are better than any other transform.
    *min_entropy_ix = kPalette;
    *red_and_blue_always_zero = 1;
    return 1;
  }
  histo = (uint32_t*)WebPSafeCalloc(kHistoTotal, sizeof(*histo) * 256);
  if (histo != NULL) {
    int i, x, y;
    const uint32_t* prev_row = NULL;
    const uint32_t* curr_row = argb;
    uint32_t pix_prev = argb[0];  // Skip the first pixel.
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        const uint32_t pix = curr_row[x];
        const uint32_t pix_diff = VP8LSubPixels(pix, pix_prev);
        pix_prev = pix;
        if ((pix_diff == 0) || (prev_row != NULL && pix == prev_row[x])) {
          continue;
        }
        AddSingle(pix,
                  &histo[kHistoAlpha * 256],
                  &histo[kHistoRed * 256],
                  &histo[kHistoGreen * 256],
                  &histo[kHistoBlue * 256]);
        AddSingle(pix_diff,
                  &histo[kHistoAlphaPred * 256],
                  &histo[kHistoRedPred * 256],
                  &histo[kHistoGreenPred * 256],
                  &histo[kHistoBluePred * 256]);
        AddSingleSubGreen(pix,
                          &histo[kHistoRedSubGreen * 256],
                          &histo[kHistoBlueSubGreen * 256]);
        AddSingleSubGreen(pix_diff,
                          &histo[kHistoRedPredSubGreen * 256],
                          &histo[kHistoBluePredSubGreen * 256]);
        {
          // Approximate the palette by the entropy of the multiplicative hash.
          const uint32_t hash = HashPix(pix);
          ++histo[kHistoPalette * 256 + hash];
        }
      }
      prev_row = curr_row;
      curr_row += argb_stride;
    }
    {
      double entropy_comp[kHistoTotal];
      double entropy[kNumEntropyIx];
      int k;
      int last_mode_to_analyze = use_palette ? kPalette : kSpatialSubGreen;
      int j;
      // Let's add one zero to the predicted histograms. The zeros are removed
      // too efficiently by the pix_diff == 0 comparison, at least one of the
      // zeros is likely to exist.
      ++histo[kHistoRedPredSubGreen * 256];
      ++histo[kHistoBluePredSubGreen * 256];
      ++histo[kHistoRedPred * 256];
      ++histo[kHistoGreenPred * 256];
      ++histo[kHistoBluePred * 256];
      ++histo[kHistoAlphaPred * 256];

      for (j = 0; j < kHistoTotal; ++j) {
        entropy_comp[j] = VP8LBitsEntropy(&histo[j * 256], 256);
      }
      entropy[kDirect] = entropy_comp[kHistoAlpha] +
          entropy_comp[kHistoRed] +
          entropy_comp[kHistoGreen] +
          entropy_comp[kHistoBlue];
      entropy[kSpatial] = entropy_comp[kHistoAlphaPred] +
          entropy_comp[kHistoRedPred] +
          entropy_comp[kHistoGreenPred] +
          entropy_comp[kHistoBluePred];
      entropy[kSubGreen] = entropy_comp[kHistoAlpha] +
          entropy_comp[kHistoRedSubGreen] +
          entropy_comp[kHistoGreen] +
          entropy_comp[kHistoBlueSubGreen];
      entropy[kSpatialSubGreen] = entropy_comp[kHistoAlphaPred] +
          entropy_comp[kHistoRedPredSubGreen] +
          entropy_comp[kHistoGreenPred] +
          entropy_comp[kHistoBluePredSubGreen];
      entropy[kPalette] = entropy_comp[kHistoPalette];

      // When including transforms, there is an overhead in bits from
      // storing them. This overhead is small but matters for small images.
      // For spatial, there are 14 transformations.
      entropy[kSpatial] += VP8LSubSampleSize(width, transform_bits) *
                           VP8LSubSampleSize(height, transform_bits) *
                           VP8LFastLog2(14);
      // For color transforms: 24 as only 3 channels are considered in a
      // ColorTransformElement.
      entropy[kSpatialSubGreen] += VP8LSubSampleSize(width, transform_bits) *
                                   VP8LSubSampleSize(height, transform_bits) *
                                   VP8LFastLog2(24);
      // For palettes, add the cost of storing the palette.
      // We empirically estimate the cost of a compressed entry as 8 bits.
      // The palette is differential-coded when compressed hence a much
      // lower cost than sizeof(uint32_t)*8.
      entropy[kPalette] += palette_size * 8;

      *min_entropy_ix = kDirect;
      for (k = kDirect + 1; k <= last_mode_to_analyze; ++k) {
        if (entropy[*min_entropy_ix] > entropy[k]) {
          *min_entropy_ix = (EntropyIx)k;
        }
      }
      assert((int)*min_entropy_ix <= last_mode_to_analyze);
      *red_and_blue_always_zero = 1;
      // Let's check if the histogram of the chosen entropy mode has
      // non-zero red and blue values. If all are zero, we can later skip
      // the cross color optimization.
      {
        static const uint8_t kHistoPairs[5][2] = {
          { kHistoRed, kHistoBlue },
          { kHistoRedPred, kHistoBluePred },
          { kHistoRedSubGreen, kHistoBlueSubGreen },
          { kHistoRedPredSubGreen, kHistoBluePredSubGreen },
          { kHistoRed, kHistoBlue }
        };
        const uint32_t* const red_histo =
            &histo[256 * kHistoPairs[*min_entropy_ix][0]];
        const uint32_t* const blue_histo =
            &histo[256 * kHistoPairs[*min_entropy_ix][1]];
        for (i = 1; i < 256; ++i) {
          if ((red_histo[i] | blue_histo[i]) != 0) {
            *red_and_blue_always_zero = 0;
            break;
          }
        }
      }
    }
    WebPSafeFree(histo);
    return 1;
  } else {
    return 0;
  }
}

enum VP8LLZ77Type {
  kLZ77Standard = 1,
  kLZ77RLE = 2,
  kLZ77Box = 4
};

static int EncoderAnalyze(VP8LEncoder* const enc,
                          CrunchConfig crunch_configs[CRUNCH_CONFIGS_MAX],
                          int* const crunch_configs_size,
                          int* const red_and_blue_always_zero) {
  const WebPPicture* const pic = enc->pic_;
  const int width = pic->width;
  const int height = pic->height;
  const WebPConfig* const config = enc->config_;
  const int method = config->method;
  const int low_effort = (config->method == 0);
  int i;
  int use_palette;
  int n_lz77s;
  assert(pic != NULL && pic->argb != NULL);

  use_palette =
      AnalyzeAndCreatePalette(pic, low_effort,
                              enc->palette_, &enc->palette_size_);

  // Empirical bit sizes.
  enc->histo_bits_ = GetHistoBits(method, use_palette,
                                  pic->width, pic->height);
  enc->transform_bits_ = GetTransformBits(method, enc->histo_bits_);

  if (low_effort) {
    // AnalyzeEntropy is somewhat slow.
    crunch_configs[0].entropy_idx_ = use_palette ? kPalette : kSpatialSubGreen;
    n_lz77s = 1;
    *crunch_configs_size = 1;
  } else {
    EntropyIx min_entropy_ix;
    // Try out multiple LZ77 on images with few colors.
    n_lz77s = (enc->palette_size_ > 0 && enc->palette_size_ <= 16) ? 2 : 1;
    if (!AnalyzeEntropy(pic->argb, width, height, pic->argb_stride, use_palette,
                        enc->palette_size_, enc->transform_bits_,
                        &min_entropy_ix, red_and_blue_always_zero)) {
      return 0;
    }
    if (method == 6 && config->quality == 100) {
      // Go brute force on all transforms.
      *crunch_configs_size = 0;
      for (i = 0; i < kNumEntropyIx; ++i) {
        if (i != kPalette || use_palette) {
          assert(*crunch_configs_size < CRUNCH_CONFIGS_MAX);
          crunch_configs[(*crunch_configs_size)++].entropy_idx_ = i;
        }
      }
    } else {
      // Only choose the guessed best transform.
      *crunch_configs_size = 1;
      crunch_configs[0].entropy_idx_ = min_entropy_ix;
    }
  }
  // Fill in the different LZ77s.
  assert(n_lz77s <= CRUNCH_CONFIGS_LZ77_MAX);
  for (i = 0; i < *crunch_configs_size; ++i) {
    int j;
    for (j = 0; j < n_lz77s; ++j) {
      crunch_configs[i].lz77s_types_to_try_[j] =
          (j == 0) ? kLZ77Standard | kLZ77RLE : kLZ77Box;
    }
    crunch_configs[i].lz77s_types_to_try_size_ = n_lz77s;
  }
  return 1;
}

// maximum number of reference blocks the image will be segmented into
#define MAX_REFS_BLOCK_PER_IMAGE 16

int VP8LHashChainInit(VP8LHashChain* const p, int size) {
  assert(p->size_ == 0);
  assert(p->offset_length_ == NULL);
  assert(size > 0);
  p->offset_length_ =
      (uint32_t*)WebPSafeMalloc(size, sizeof(*p->offset_length_));
  if (p->offset_length_ == NULL) return 0;
  p->size_ = size;

  return 1;
}

typedef struct PixOrCopyBlock PixOrCopyBlock;   // forward declaration
typedef struct VP8LBackwardRefs VP8LBackwardRefs;

typedef struct {
  // mode as uint8_t to make the memory layout to be exactly 8 bytes.
  uint8_t mode;
  uint16_t len;
  uint32_t argb_or_distance;
} PixOrCopy;

struct PixOrCopyBlock {
  PixOrCopyBlock* next_;   // next block (or NULL)
  PixOrCopy* start_;       // data start
  int size_;               // currently used size
};

// Container for blocks chain
struct VP8LBackwardRefs {
  int block_size_;               // common block-size
  int error_;                    // set to true if some memory error occurred
  PixOrCopyBlock* refs_;         // list of currently used blocks
  PixOrCopyBlock** tail_;        // for list recycling
  PixOrCopyBlock* free_blocks_;  // free-list
  PixOrCopyBlock* last_block_;   // used for adding new refs (internal)
};

#define MIN_BLOCK_SIZE 256  // minimum block size for backward references

void VP8LBackwardRefsInit(VP8LBackwardRefs* const refs, int block_size) {
  assert(refs != NULL);
  memset(refs, 0, sizeof(*refs));
  refs->tail_ = &refs->refs_;
  refs->block_size_ =
      (block_size < MIN_BLOCK_SIZE) ? MIN_BLOCK_SIZE : block_size;
}

static int EncoderInit(VP8LEncoder* const enc) {
  const WebPPicture* const pic = enc->pic_;
  const int width = pic->width;
  const int height = pic->height;
  const int pix_cnt = width * height;
  // we round the block size up, so we're guaranteed to have
  // at most MAX_REFS_BLOCK_PER_IMAGE blocks used:
  const int refs_block_size = (pix_cnt - 1) / MAX_REFS_BLOCK_PER_IMAGE + 1;
  int i;
  if (!VP8LHashChainInit(&enc->hash_chain_, pix_cnt)) return 0;

  for (i = 0; i < 3; ++i) VP8LBackwardRefsInit(&enc->refs_[i], refs_block_size);

  return 1;
}

int VP8LBitWriterClone(const VP8LBitWriter* const src,
                       VP8LBitWriter* const dst) {
  const size_t current_size = src->cur_ - src->buf_;
  assert(src->cur_ >= src->buf_ && src->cur_ <= src->end_);
  if (!VP8LBitWriterResize(dst, current_size)) return 0;
  memcpy(dst->buf_, src->buf_, current_size);
  dst->bits_ = src->bits_;
  dst->used_ = src->used_;
  dst->error_ = src->error_;
  return 1;
}

#ifndef WEBP_NEAR_LOSSLESS
#define WEBP_NEAR_LOSSLESS 1
#endif

static size_t VP8LBitWriterNumBytes(const VP8LBitWriter* const bw) {
  return (bw->cur_ - bw->buf_) + ((bw->used_ + 7) >> 3);
}

void VP8LClearBackwardRefs(VP8LBackwardRefs* const refs) {
  assert(refs != NULL);
  if (refs->tail_ != NULL) {
    *refs->tail_ = refs->free_blocks_;  // recycle all blocks at once
  }
  refs->free_blocks_ = refs->refs_;
  refs->tail_ = &refs->refs_;
  refs->last_block_ = NULL;
  refs->refs_ = NULL;
}

void VP8LBackwardRefsClear(VP8LBackwardRefs* const refs) {
  assert(refs != NULL);
  VP8LClearBackwardRefs(refs);
  while (refs->free_blocks_ != NULL) {
    PixOrCopyBlock* const next = refs->free_blocks_->next_;
    WebPSafeFree(refs->free_blocks_);
    refs->free_blocks_ = next;
  }
}

static void ClearTransformBuffer(VP8LEncoder* const enc) {
  WebPSafeFree(enc->transform_mem_);
  enc->transform_mem_ = NULL;
  enc->transform_mem_size_ = 0;
}

// Allocates the memory for argb (W x H) buffer, 2 rows of context for
// prediction and transform data.
// Flags influencing the memory allocated:
//  enc->transform_bits_
//  enc->use_predict_, enc->use_cross_color_
static WebPEncodingError AllocateTransformBuffer(VP8LEncoder* const enc,
                                                 int width, int height) {
  WebPEncodingError err = VP8_ENC_OK;
  const uint64_t image_size = width * height;
  // VP8LResidualImage needs room for 2 scanlines of uint32 pixels with an extra
  // pixel in each, plus 2 regular scanlines of bytes.
  // TODO(skal): Clean up by using arithmetic in bytes instead of words.
  const uint64_t argb_scratch_size =
      enc->use_predict_
          ? (width + 1) * 2 +
            (width * 2 + sizeof(uint32_t) - 1) / sizeof(uint32_t)
          : 0;
  const uint64_t transform_data_size =
      (enc->use_predict_ || enc->use_cross_color_)
          ? VP8LSubSampleSize(width, enc->transform_bits_) *
                VP8LSubSampleSize(height, enc->transform_bits_)
          : 0;
  const uint64_t max_alignment_in_words =
      (WEBP_ALIGN_CST + sizeof(uint32_t) - 1) / sizeof(uint32_t);
  const uint64_t mem_size =
      image_size + max_alignment_in_words +
      argb_scratch_size + max_alignment_in_words +
      transform_data_size;
  uint32_t* mem = enc->transform_mem_;
  if (mem == NULL || mem_size > enc->transform_mem_size_) {
    ClearTransformBuffer(enc);
    mem = (uint32_t*)WebPSafeMalloc(mem_size, sizeof(*mem));
    if (mem == NULL) {
      err = VP8_ENC_ERROR_OUT_OF_MEMORY;
      goto Error;
    }
    enc->transform_mem_ = mem;
    enc->transform_mem_size_ = (size_t)mem_size;
    enc->argb_content_ = kEncoderNone;
  }
  enc->argb_ = mem;
  mem = (uint32_t*)WEBP_ALIGN(mem + image_size);
  enc->argb_scratch_ = mem;
  mem = (uint32_t*)WEBP_ALIGN(mem + argb_scratch_size);
  enc->transform_data_ = mem;

  enc->current_width_ = width;
 Error:
  return err;
}

#if (WEBP_NEAR_LOSSLESS == 1)

#define MIN_DIM_FOR_NEAR_LOSSLESS 64
#define MAX_LIMIT_BITS             5

// Quantizes the value up or down to a multiple of 1<<bits (or to 255),
// choosing the closer one, resolving ties using bankers' rounding.
static uint32_t FindClosestDiscretized(uint32_t a, int bits) {
  const uint32_t mask = (1u << bits) - 1;
  const uint32_t biased = a + (mask >> 1) + ((a >> bits) & 1);
  assert(bits > 0);
  if (biased > 0xff) return 0xff;
  return biased & ~mask;
}

// Applies FindClosestDiscretized to all channels of pixel.
static uint32_t ClosestDiscretizedArgb(uint32_t a, int bits) {
  return
      (FindClosestDiscretized(a >> 24, bits) << 24) |
      (FindClosestDiscretized((a >> 16) & 0xff, bits) << 16) |
      (FindClosestDiscretized((a >> 8) & 0xff, bits) << 8) |
      (FindClosestDiscretized(a & 0xff, bits));
}

// Checks if distance between corresponding channel values of pixels a and b
// is within the given limit.
static int IsNear(uint32_t a, uint32_t b, int limit) {
  int k;
  for (k = 0; k < 4; ++k) {
    const int delta =
        (int)((a >> (k * 8)) & 0xff) - (int)((b >> (k * 8)) & 0xff);
    if (delta >= limit || delta <= -limit) {
      return 0;
    }
  }
  return 1;
}

static int IsSmooth(const uint32_t* const prev_row,
                    const uint32_t* const curr_row,
                    const uint32_t* const next_row,
                    int ix, int limit) {
  // Check that all pixels in 4-connected neighborhood are smooth.
  return (IsNear(curr_row[ix], curr_row[ix - 1], limit) &&
          IsNear(curr_row[ix], curr_row[ix + 1], limit) &&
          IsNear(curr_row[ix], prev_row[ix], limit) &&
          IsNear(curr_row[ix], next_row[ix], limit));
}

// Adjusts pixel values of image with given maximum error.
static void NearLossless(int xsize, int ysize, const uint32_t* argb_src,
                         int stride, int limit_bits, uint32_t* copy_buffer,
                         uint32_t* argb_dst) {
  int x, y;
  const int limit = 1 << limit_bits;
  uint32_t* prev_row = copy_buffer;
  uint32_t* curr_row = prev_row + xsize;
  uint32_t* next_row = curr_row + xsize;
  memcpy(curr_row, argb_src, xsize * sizeof(argb_src[0]));
  memcpy(next_row, argb_src + stride, xsize * sizeof(argb_src[0]));

  for (y = 0; y < ysize; ++y, argb_src += stride, argb_dst += xsize) {
    if (y == 0 || y == ysize - 1) {
      memcpy(argb_dst, argb_src, xsize * sizeof(argb_src[0]));
    } else {
      memcpy(next_row, argb_src + stride, xsize * sizeof(argb_src[0]));
      argb_dst[0] = argb_src[0];
      argb_dst[xsize - 1] = argb_src[xsize - 1];
      for (x = 1; x < xsize - 1; ++x) {
        if (IsSmooth(prev_row, curr_row, next_row, x, limit)) {
          argb_dst[x] = curr_row[x];
        } else {
          argb_dst[x] = ClosestDiscretizedArgb(curr_row[x], limit_bits);
        }
      }
    }
    {
      // Three-way swap.
      uint32_t* const temp = prev_row;
      prev_row = curr_row;
      curr_row = next_row;
      next_row = temp;
    }
  }
}

// Converts near lossless quality into max number of bits shaved off.
static int VP8LNearLosslessBits(int near_lossless_quality) {
  //    100 -> 0
  // 80..99 -> 1
  // 60..79 -> 2
  // 40..59 -> 3
  // 20..39 -> 4
  //  0..19 -> 5
  return 5 - near_lossless_quality / 20;
}

int VP8ApplyNearLossless(const WebPPicture* const picture, int quality,
                         uint32_t* const argb_dst) {
  int i;
  const int xsize = picture->width;
  const int ysize = picture->height;
  const int stride = picture->argb_stride;
  uint32_t* const copy_buffer =
      (uint32_t*)WebPSafeMalloc(xsize * 3, sizeof(*copy_buffer));
  const int limit_bits = VP8LNearLosslessBits(quality);
  assert(argb_dst != NULL);
  assert(limit_bits > 0);
  assert(limit_bits <= MAX_LIMIT_BITS);
  if (copy_buffer == NULL) {
    return 0;
  }
  // For small icon images, don't attempt to apply near-lossless compression.
  if ((xsize < MIN_DIM_FOR_NEAR_LOSSLESS &&
       ysize < MIN_DIM_FOR_NEAR_LOSSLESS) ||
      ysize < 3) {
    for (i = 0; i < ysize; ++i) {
      memcpy(argb_dst + i * xsize, picture->argb + i * picture->argb_stride,
             xsize * sizeof(*argb_dst));
    }
    WebPSafeFree(copy_buffer);
    return 1;
  }

  NearLossless(xsize, ysize, picture->argb, stride, limit_bits, copy_buffer,
               argb_dst);
  for (i = limit_bits - 1; i != 0; --i) {
    NearLossless(xsize, ysize, argb_dst, xsize, i, copy_buffer, argb_dst);
  }
  WebPSafeFree(copy_buffer);
  return 1;
}
#else  // (WEBP_NEAR_LOSSLESS == 1)

// Define a stub to suppress compiler warnings.
extern void VP8LNearLosslessStub(void);
void VP8LNearLosslessStub(void) {}

#endif  // (WEBP_NEAR_LOSSLESS == 1)

#if defined(WORDS_BIGENDIAN)
#define HToLE32 BSwap32
#define HToLE16 BSwap16
#else
#define HToLE32(x) (x)
#define HToLE16(x) (x)
#endif

typedef uint32_t vp8l_wtype_t;   // writing type
#define VP8L_WRITER_BYTES    4   // sizeof(vp8l_wtype_t)
#define VP8L_WRITER_BITS     32  // 8 * sizeof(vp8l_wtype_t)
#define WSWAP HToLE32

// This is the minimum amount of size the memory buffer is guaranteed to grow
// when extra space is needed.
#define MIN_EXTRA_SIZE  (32768ULL)

void VP8LPutBitsFlushBits(VP8LBitWriter* const bw) {
  // If needed, make some room by flushing some bits out.
  if (bw->cur_ + VP8L_WRITER_BYTES > bw->end_) {
    const uint64_t extra_size = (bw->end_ - bw->buf_) + MIN_EXTRA_SIZE;
    if (extra_size != (size_t)extra_size ||
        !VP8LBitWriterResize(bw, (size_t)extra_size)) {
      bw->cur_ = bw->buf_;
      bw->error_ = 1;
      return;
    }
  }
  *(vp8l_wtype_t*)bw->cur_ = (vp8l_wtype_t)WSWAP((vp8l_wtype_t)bw->bits_);
  bw->cur_ += VP8L_WRITER_BYTES;
  bw->bits_ >>= VP8L_WRITER_BITS;
  bw->used_ -= VP8L_WRITER_BITS;
}

void VP8LPutBitsInternal(VP8LBitWriter* const bw, uint32_t bits, int n_bits) {
  assert(n_bits <= 32);
  // That's the max we can handle:
  assert(sizeof(vp8l_wtype_t) == 2);
  if (n_bits > 0) {
    vp8l_atype_t lbits = bw->bits_;
    int used = bw->used_;
    // Special case of overflow handling for 32bit accumulator (2-steps flush).
#if VP8L_WRITER_BITS == 16
    if (used + n_bits >= VP8L_WRITER_MAX_BITS) {
      // Fill up all the VP8L_WRITER_MAX_BITS so it can be flushed out below.
      const int shift = VP8L_WRITER_MAX_BITS - used;
      lbits |= (vp8l_atype_t)bits << used;
      used = VP8L_WRITER_MAX_BITS;
      n_bits -= shift;
      bits >>= shift;
      assert(n_bits <= VP8L_WRITER_MAX_BITS);
    }
#endif
    // If needed, make some room by flushing some bits out.
    while (used >= VP8L_WRITER_BITS) {
      if (bw->cur_ + VP8L_WRITER_BYTES > bw->end_) {
        const uint64_t extra_size = (bw->end_ - bw->buf_) + MIN_EXTRA_SIZE;
        if (extra_size != (size_t)extra_size ||
            !VP8LBitWriterResize(bw, (size_t)extra_size)) {
          bw->cur_ = bw->buf_;
          bw->error_ = 1;
          return;
        }
      }
      *(vp8l_wtype_t*)bw->cur_ = (vp8l_wtype_t)WSWAP((vp8l_wtype_t)lbits);
      bw->cur_ += VP8L_WRITER_BYTES;
      lbits >>= VP8L_WRITER_BITS;
      used -= VP8L_WRITER_BITS;
    }
    bw->bits_ = lbits | ((vp8l_atype_t)bits << used);
    bw->used_ = used + n_bits;
  }
}

// This function writes bits into bytes in increasing addresses (little endian),
// and within a byte least-significant-bit first.
// This function can write up to 32 bits in one go, but VP8LBitReader can only
// read 24 bits max (VP8L_MAX_NUM_BIT_READ).
// VP8LBitWriter's error_ flag is set in case of  memory allocation error.
static void VP8LPutBits(VP8LBitWriter* const bw,
                                    uint32_t bits, int n_bits) {
  if (sizeof(vp8l_wtype_t) == 4) {
    if (n_bits > 0) {
      if (bw->used_ >= 32) {
        VP8LPutBitsFlushBits(bw);
      }
      bw->bits_ |= (vp8l_atype_t)bits << bw->used_;
      bw->used_ += n_bits;
    }
  } else {
    VP8LPutBitsInternal(bw, bits, n_bits);
  }
}

#define TRANSFORM_PRESENT            1  // The bit to be written when next data
                                        // to be read is a transform.
// in a bitstream.
typedef enum {
PREDICTOR_TRANSFORM      = 0,
CROSS_COLOR_TRANSFORM    = 1,
SUBTRACT_GREEN           = 2,
COLOR_INDEXING_TRANSFORM = 3
} VP8LImageTransformType;

// Struct for holding the tree header in coded form.
typedef struct {
  uint8_t code;         // value (0..15) or escape code (16,17,18)
  uint8_t extra_bits;   // extra bits for escape codes
} HuffmanTreeToken;

// Struct to represent the tree codes (depth and bits array).
typedef struct {
  int       num_symbols;   // Number of symbols.
  uint8_t*  code_lengths;  // Code lengths of the symbols.
  uint16_t* codes;         // Symbol Codes.
} HuffmanTreeCode;

#define NUM_LITERAL_CODES            256
#define NUM_DISTANCE_CODES           40

// A simple container for histograms of data.
typedef struct {
  // literal_ contains green literal, palette-code and
  // copy-length-prefix histogram
  uint32_t* literal_;         // Pointer to the allocated buffer for literal.
  uint32_t red_[NUM_LITERAL_CODES];
  uint32_t blue_[NUM_LITERAL_CODES];
  uint32_t alpha_[NUM_LITERAL_CODES];
  // Backward reference prefix-code histogram.
  uint32_t distance_[NUM_DISTANCE_CODES];
  int palette_code_bits_;
  uint32_t trivial_symbol_;  // True, if histograms for Red, Blue & Alpha
                             // literal symbols are single valued.
  double bit_cost_;          // cached value of bit cost.
  double literal_cost_;      // Cached values of dominant entropy costs:
  double red_cost_;          // literal, red & blue.
  double blue_cost_;
} VP8LHistogram;

// Collection of histograms with fixed capacity, allocated as one
// big memory chunk. Can be destroyed by calling WebPSafeFree().
typedef struct {
  int size;         // number of slots currently in use
  int max_size;     // maximum capacity
  VP8LHistogram** histograms;
} VP8LHistogramSet;

// Struct to represent the Huffman tree.
typedef struct {
  uint32_t total_count_;   // Symbol frequency.
  int value_;              // Symbol value.
  int pool_index_left_;    // Index for the left sub-tree.
  int pool_index_right_;   // Index for the right sub-tree.
} HuffmanTree;

#define CODE_LENGTH_CODES            19

// Returns the maximum number of hash chain lookups to do for a
// given compression quality. Return value in range [8, 86].
static int GetMaxItersForQuality(int quality) {
  return 8 + (quality * quality) / 128;
}

#define MAX_LENGTH_BITS 12
#define WINDOW_SIZE_BITS 20
// We want the max value to be attainable and stored in MAX_LENGTH_BITS bits.
#define MAX_LENGTH ((1 << MAX_LENGTH_BITS) - 1)

// 1M window (4M bytes) minus 120 special codes for short distances.
#define WINDOW_SIZE ((1 << WINDOW_SIZE_BITS) - 120)

static int GetWindowSizeForHashChain(int quality, int xsize) {
  const int max_window_size = (quality > 75) ? WINDOW_SIZE
                            : (quality > 50) ? (xsize << 8)
                            : (quality > 25) ? (xsize << 6)
                            : (xsize << 4);
  assert(xsize > 0);
  return (max_window_size > WINDOW_SIZE) ? WINDOW_SIZE : max_window_size;
}

#define HASH_BITS 18
#define HASH_SIZE (1 << HASH_BITS)

#define HASH_MULTIPLIER_HI (0xc6a4a793ULL)
#define HASH_MULTIPLIER_LO (0x5bd1e996ULL)

static uint32_t GetPixPairHash64(const uint32_t* const argb) {
  uint32_t key;
  key  = (argb[1] * HASH_MULTIPLIER_HI) & 0xffffffffu;
  key += (argb[0] * HASH_MULTIPLIER_LO) & 0xffffffffu;
  key = key >> (32 - HASH_BITS);
  return key;
}

static int MaxFindCopyLength(int len) {
  return (len < MAX_LENGTH) ? len : MAX_LENGTH;
}


static int VectorMismatch_C(const uint32_t* const array1,
                            const uint32_t* const array2, int length) {
  int match_len = 0;

  while (match_len < length && array1[match_len] == array2[match_len]) {
    ++match_len;
  }
  return match_len;
}


// Returns the exact index where array1 and array2 are different. For an index
// inferior or equal to best_len_match, the return value just has to be strictly
// inferior to best_len_match. The current behavior is to return 0 if this index
// is best_len_match, and the index itself otherwise.
// If no two elements are the same, it returns max_limit.
static int FindMatchLength(const uint32_t* const array1,
                                       const uint32_t* const array2,
                                       int best_len_match, int max_limit) {
  // Before 'expensive' linear match, check if the two arrays match at the
  // current best length index.
  if (array1[best_len_match] != array2[best_len_match]) return 0;

  return VectorMismatch_C(array1, array2, max_limit);
}

int VP8LHashChainFill(VP8LHashChain* const p, int quality,
                      const uint32_t* const argb, int xsize, int ysize,
                      int low_effort) {
  const int size = xsize * ysize;
  const int iter_max = GetMaxItersForQuality(quality);
  const uint32_t window_size = GetWindowSizeForHashChain(quality, xsize);
  int pos;
  int argb_comp;
  uint32_t base_position;
  int32_t* hash_to_first_index;
  // Temporarily use the p->offset_length_ as a hash chain.
  int32_t* chain = (int32_t*)p->offset_length_;
  assert(size > 0);
  assert(p->size_ != 0);
  assert(p->offset_length_ != NULL);

  if (size <= 2) {
    p->offset_length_[0] = p->offset_length_[size - 1] = 0;
    return 1;
  }

  hash_to_first_index =
      (int32_t*)WebPSafeMalloc(HASH_SIZE, sizeof(*hash_to_first_index));
  if (hash_to_first_index == NULL) return 0;

  // Set the int32_t array to -1.
  memset(hash_to_first_index, 0xff, HASH_SIZE * sizeof(*hash_to_first_index));
  // Fill the chain linking pixels with the same hash.
  argb_comp = (argb[0] == argb[1]);
  for (pos = 0; pos < size - 2;) {
    uint32_t hash_code;
    const int argb_comp_next = (argb[pos + 1] == argb[pos + 2]);
    if (argb_comp && argb_comp_next) {
      // Consecutive pixels with the same color will share the same hash.
      // We therefore use a different hash: the color and its repetition
      // length.
      uint32_t tmp[2];
      uint32_t len = 1;
      tmp[0] = argb[pos];
      // Figure out how far the pixels are the same.
      // The last pixel has a different 64 bit hash, as its next pixel does
      // not have the same color, so we just need to get to the last pixel equal
      // to its follower.
      while (pos + (int)len + 2 < size && argb[pos + len + 2] == argb[pos]) {
        ++len;
      }
      if (len > MAX_LENGTH) {
        // Skip the pixels that match for distance=1 and length>MAX_LENGTH
        // because they are linked to their predecessor and we automatically
        // check that in the main for loop below. Skipping means setting no
        // predecessor in the chain, hence -1.
        memset(chain + pos, 0xff, (len - MAX_LENGTH) * sizeof(*chain));
        pos += len - MAX_LENGTH;
        len = MAX_LENGTH;
      }
      // Process the rest of the hash chain.
      while (len) {
        tmp[1] = len--;
        hash_code = GetPixPairHash64(tmp);
        chain[pos] = hash_to_first_index[hash_code];
        hash_to_first_index[hash_code] = pos++;
      }
      argb_comp = 0;
    } else {
      // Just move one pixel forward.
      hash_code = GetPixPairHash64(argb + pos);
      chain[pos] = hash_to_first_index[hash_code];
      hash_to_first_index[hash_code] = pos++;
      argb_comp = argb_comp_next;
    }
  }
  // Process the penultimate pixel.
  chain[pos] = hash_to_first_index[GetPixPairHash64(argb + pos)];

  WebPSafeFree(hash_to_first_index);

  // Find the best match interval at each pixel, defined by an offset to the
  // pixel and a length. The right-most pixel cannot match anything to the right
  // (hence a best length of 0) and the left-most pixel nothing to the left
  // (hence an offset of 0).
  assert(size > 2);
  p->offset_length_[0] = p->offset_length_[size - 1] = 0;
  for (base_position = size - 2; base_position > 0;) {
    const int max_len = MaxFindCopyLength(size - 1 - base_position);
    const uint32_t* const argb_start = argb + base_position;
    int iter = iter_max;
    int best_length = 0;
    uint32_t best_distance = 0;
    uint32_t best_argb;
    const int min_pos =
        (base_position > window_size) ? base_position - window_size : 0;
    const int length_max = (max_len < 256) ? max_len : 256;
    uint32_t max_base_position;

    pos = chain[base_position];
    if (!low_effort) {
      int curr_length;
      // Heuristic: use the comparison with the above line as an initialization.
      if (base_position >= (uint32_t)xsize) {
        curr_length = FindMatchLength(argb_start - xsize, argb_start,
                                      best_length, max_len);
        if (curr_length > best_length) {
          best_length = curr_length;
          best_distance = xsize;
        }
        --iter;
      }
      // Heuristic: compare to the previous pixel.
      curr_length =
          FindMatchLength(argb_start - 1, argb_start, best_length, max_len);
      if (curr_length > best_length) {
        best_length = curr_length;
        best_distance = 1;
      }
      --iter;
      // Skip the for loop if we already have the maximum.
      if (best_length == MAX_LENGTH) pos = min_pos - 1;
    }
    best_argb = argb_start[best_length];

    for (; pos >= min_pos && --iter; pos = chain[pos]) {
      int curr_length;
      assert(base_position > (uint32_t)pos);

      if (argb[pos + best_length] != best_argb) continue;

      curr_length = VectorMismatch_C(argb + pos, argb_start, max_len);
      if (best_length < curr_length) {
        best_length = curr_length;
        best_distance = base_position - pos;
        best_argb = argb_start[best_length];
        // Stop if we have reached a good enough length.
        if (best_length >= length_max) break;
      }
    }
    // We have the best match but in case the two intervals continue matching
    // to the left, we have the best matches for the left-extended pixels.
    max_base_position = base_position;
    while (1) {
      assert(best_length <= MAX_LENGTH);
      assert(best_distance <= WINDOW_SIZE);
      p->offset_length_[base_position] =
          (best_distance << MAX_LENGTH_BITS) | (uint32_t)best_length;
      --base_position;
      // Stop if we don't have a match or if we are out of bounds.
      if (best_distance == 0 || base_position == 0) break;
      // Stop if we cannot extend the matching intervals to the left.
      if (base_position < best_distance ||
          argb[base_position - best_distance] != argb[base_position]) {
        break;
      }
      // Stop if we are matching at its limit because there could be a closer
      // matching interval with the same maximum length. Then again, if the
      // matching interval is as close as possible (best_distance == 1), we will
      // never find anything better so let's continue.
      if (best_length == MAX_LENGTH && best_distance != 1 &&
          base_position + MAX_LENGTH < max_base_position) {
        break;
      }
      if (best_length < MAX_LENGTH) {
        ++best_length;
        max_base_position = base_position;
      }
    }
  }
  return 1;
}

// Main color cache struct.
typedef struct {
  uint32_t *colors_;  // color entries
  int hash_shift_;    // Hash shift: 32 - hash_bits_.
  int hash_bits_;
} VP8LColorCache;

int VP8LColorCacheInit(VP8LColorCache* const cc, int hash_bits) {
  const int hash_size = 1 << hash_bits;
  assert(cc != NULL);
  assert(hash_bits > 0);
  cc->colors_ = (uint32_t*)WebPSafeCalloc((uint64_t)hash_size,
                                          sizeof(*cc->colors_));
  if (cc->colors_ == NULL) return 0;
  cc->hash_shift_ = 32 - hash_bits;
  cc->hash_bits_ = hash_bits;
  return 1;
}

static int VP8LHashChainFindOffset(const VP8LHashChain* const p,
                                               const int base_position) {
  return p->offset_length_[base_position] >> MAX_LENGTH_BITS;
}

static int VP8LHashChainFindLength(const VP8LHashChain* const p,
                                               const int base_position) {
  return p->offset_length_[base_position] & ((1U << MAX_LENGTH_BITS) - 1);
}

static void VP8LHashChainFindCopy(const VP8LHashChain* const p,
                                              int base_position,
                                              int* const offset_ptr,
                                              int* const length_ptr) {
  *offset_ptr = VP8LHashChainFindOffset(p, base_position);
  *length_ptr = VP8LHashChainFindLength(p, base_position);
}

// Minimum number of pixels for which it is cheaper to encode a
// distance + length instead of each pixel as a literal.
#define MIN_LENGTH 4

static int VP8LColorCacheGetIndex(const VP8LColorCache* const cc,
                                              uint32_t argb) {
  return VP8LHashPix(argb, cc->hash_shift_);
}

// The maximum allowed limit is 11.
#define MAX_COLOR_CACHE_BITS 10

enum Mode {
  kLiteral,
  kCacheIdx,
  kCopy,
  kNone
};

static PixOrCopy PixOrCopyCreateCacheIdx(int idx) {
  PixOrCopy retval;
  assert(idx >= 0);
  assert(idx < (1 << MAX_COLOR_CACHE_BITS));
  retval.mode = kCacheIdx;
  retval.argb_or_distance = idx;
  retval.len = 1;
  return retval;
}

static uint32_t VP8LColorCacheLookup(
    const VP8LColorCache* const cc, uint32_t key) {
  assert((key >> cc->hash_bits_) == 0u);
  return cc->colors_[key];
}

static PixOrCopy PixOrCopyCreateLiteral(uint32_t argb) {
  PixOrCopy retval;
  retval.mode = kLiteral;
  retval.argb_or_distance = argb;
  retval.len = 1;
  return retval;
}

static void VP8LColorCacheSet(const VP8LColorCache* const cc,
                                          uint32_t key, uint32_t argb) {
  assert((key >> cc->hash_bits_) == 0u);
  cc->colors_[key] = argb;
}

// Create a new block, either from the free list or allocated
static PixOrCopyBlock* BackwardRefsNewBlock(VP8LBackwardRefs* const refs) {
  PixOrCopyBlock* b = refs->free_blocks_;
  if (b == NULL) {   // allocate new memory chunk
    const size_t total_size =
        sizeof(*b) + refs->block_size_ * sizeof(*b->start_);
    b = (PixOrCopyBlock*)WebPSafeMalloc(1ULL, total_size);
    if (b == NULL) {
      refs->error_ |= 1;
      return NULL;
    }
    b->start_ = (PixOrCopy*)((uint8_t*)b + sizeof(*b));  // not always aligned
  } else {  // recycle from free-list
    refs->free_blocks_ = b->next_;
  }
  *refs->tail_ = b;
  refs->tail_ = &b->next_;
  refs->last_block_ = b;
  b->next_ = NULL;
  b->size_ = 0;
  return b;
}

void VP8LBackwardRefsCursorAdd(VP8LBackwardRefs* const refs,
                               const PixOrCopy v) {
  PixOrCopyBlock* b = refs->last_block_;
  if (b == NULL || b->size_ == refs->block_size_) {
    b = BackwardRefsNewBlock(refs);
    if (b == NULL) return;   // refs->error_ is set
  }
  b->start_[b->size_++] = v;
}

static void AddSingleLiteral(uint32_t pixel, int use_color_cache,
                                         VP8LColorCache* const hashers,
                                         VP8LBackwardRefs* const refs) {
  PixOrCopy v;
  if (use_color_cache) {
    const uint32_t key = VP8LColorCacheGetIndex(hashers, pixel);
    if (VP8LColorCacheLookup(hashers, key) == pixel) {
      v = PixOrCopyCreateCacheIdx(key);
    } else {
      v = PixOrCopyCreateLiteral(pixel);
      VP8LColorCacheSet(hashers, key, pixel);
    }
  } else {
    v = PixOrCopyCreateLiteral(pixel);
  }
  VP8LBackwardRefsCursorAdd(refs, v);
}

static PixOrCopy PixOrCopyCreateCopy(uint32_t distance,
                                                 uint16_t len) {
  PixOrCopy retval;
  retval.mode = kCopy;
  retval.argb_or_distance = distance;
  retval.len = len;
  return retval;
}

static void VP8LColorCacheInsert(const VP8LColorCache* const cc,
                                             uint32_t argb) {
  const int key = VP8LHashPix(argb, cc->hash_shift_);
  cc->colors_[key] = argb;
}

void VP8LColorCacheClear(VP8LColorCache* const cc) {
  if (cc != NULL) {
    WebPSafeFree(cc->colors_);
    cc->colors_ = NULL;
  }
}

static int BackwardReferencesLz77(int xsize, int ysize,
                                  const uint32_t* const argb, int cache_bits,
                                  const VP8LHashChain* const hash_chain,
                                  VP8LBackwardRefs* const refs) {
  int i;
  int i_last_check = -1;
  int ok = 0;
  int cc_init = 0;
  const int use_color_cache = (cache_bits > 0);
  const int pix_count = xsize * ysize;
  VP8LColorCache hashers;

  if (use_color_cache) {
    cc_init = VP8LColorCacheInit(&hashers, cache_bits);
    if (!cc_init) goto Error;
  }
  VP8LClearBackwardRefs(refs);
  for (i = 0; i < pix_count;) {
    // Alternative#1: Code the pixels starting at 'i' using backward reference.
    int offset = 0;
    int len = 0;
    int j;
    VP8LHashChainFindCopy(hash_chain, i, &offset, &len);
    if (len >= MIN_LENGTH) {
      const int len_ini = len;
      int max_reach = 0;
      const int j_max =
          (i + len_ini >= pix_count) ? pix_count - 1 : i + len_ini;
      // Only start from what we have not checked already.
      i_last_check = (i > i_last_check) ? i : i_last_check;
      // We know the best match for the current pixel but we try to find the
      // best matches for the current pixel AND the next one combined.
      // The naive method would use the intervals:
      // [i,i+len) + [i+len, length of best match at i+len)
      // while we check if we can use:
      // [i,j) (where j<=i+len) + [j, length of best match at j)
      for (j = i_last_check + 1; j <= j_max; ++j) {
        const int len_j = VP8LHashChainFindLength(hash_chain, j);
        const int reach =
            j + (len_j >= MIN_LENGTH ? len_j : 1);  // 1 for single literal.
        if (reach > max_reach) {
          len = j - i;
          max_reach = reach;
          if (max_reach >= pix_count) break;
        }
      }
    } else {
      len = 1;
    }
    // Go with literal or backward reference.
    assert(len > 0);
    if (len == 1) {
      AddSingleLiteral(argb[i], use_color_cache, &hashers, refs);
    } else {
      VP8LBackwardRefsCursorAdd(refs, PixOrCopyCreateCopy(offset, len));
      if (use_color_cache) {
        for (j = i; j < i + len; ++j) VP8LColorCacheInsert(&hashers, argb[j]);
      }
    }
    i += len;
  }

  ok = !refs->error_;
 Error:
  if (cc_init) VP8LColorCacheClear(&hashers);
  return ok;
}

// Cursor for iterating on references content
typedef struct {
  // public:
  PixOrCopy* cur_pos;           // current position
  // private:
  PixOrCopyBlock* cur_block_;   // current block in the refs list
  const PixOrCopy* last_pos_;   // sentinel for switching to next block
} VP8LRefsCursor;

VP8LRefsCursor VP8LRefsCursorInit(const VP8LBackwardRefs* const refs) {
  VP8LRefsCursor c;
  c.cur_block_ = refs->refs_;
  if (refs->refs_ != NULL) {
    c.cur_pos = c.cur_block_->start_;
    c.last_pos_ = c.cur_pos + c.cur_block_->size_;
  } else {
    c.cur_pos = NULL;
    c.last_pos_ = NULL;
  }
  return c;
}

// Returns true if cursor is pointing at a valid position.
static int VP8LRefsCursorOk(const VP8LRefsCursor* const c) {
  return (c->cur_pos != NULL);
}

static int PixOrCopyIsCopy(const PixOrCopy* const p) {
  return (p->mode == kCopy);
}

static const uint8_t plane_to_code_lut[128] = {
 96,   73,  55,  39,  23,  13,   5,  1,  255, 255, 255, 255, 255, 255, 255, 255,
 101,  78,  58,  42,  26,  16,   8,  2,    0,   3,  9,   17,  27,  43,  59,  79,
 102,  86,  62,  46,  32,  20,  10,  6,    4,   7,  11,  21,  33,  47,  63,  87,
 105,  90,  70,  52,  37,  28,  18,  14,  12,  15,  19,  29,  38,  53,  71,  91,
 110,  99,  82,  66,  48,  35,  30,  24,  22,  25,  31,  36,  49,  67,  83, 100,
 115, 108,  94,  76,  64,  50,  44,  40,  34,  41,  45,  51,  65,  77,  95, 109,
 118, 113, 103,  92,  80,  68,  60,  56,  54,  57,  61,  69,  81,  93, 104, 114,
 119, 116, 111, 106,  97,  88,  84,  74,  72,  75,  85,  89,  98, 107, 112, 117
};

int VP8LDistanceToPlaneCode(int xsize, int dist) {
  const int yoffset = dist / xsize;
  const int xoffset = dist - yoffset * xsize;
  if (xoffset <= 8 && yoffset < 8) {
    return plane_to_code_lut[yoffset * 16 + 8 - xoffset] + 1;
  } else if (xoffset > xsize - 8 && yoffset < 7) {
    return plane_to_code_lut[(yoffset + 1) * 16 + 8 + (xsize - xoffset)] + 1;
  }
  return dist + 120;
}

void VP8LRefsCursorNextBlock(VP8LRefsCursor* const c) {
  PixOrCopyBlock* const b = c->cur_block_->next_;
  c->cur_pos = (b == NULL) ? NULL : b->start_;
  c->last_pos_ = (b == NULL) ? NULL : b->start_ + b->size_;
  c->cur_block_ = b;
}

// Move to next position, or NULL. Should not be called if !VP8LRefsCursorOk().
static void VP8LRefsCursorNext(VP8LRefsCursor* const c) {
  assert(c != NULL);
  assert(VP8LRefsCursorOk(c));
  if (++c->cur_pos == c->last_pos_) VP8LRefsCursorNextBlock(c);
}

static void BackwardReferences2DLocality(int xsize,
                                         const VP8LBackwardRefs* const refs) {
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  while (VP8LRefsCursorOk(&c)) {
    if (PixOrCopyIsCopy(c.cur_pos)) {
      const int dist = c.cur_pos->argb_or_distance;
      const int transformed_dist = VP8LDistanceToPlaneCode(xsize, dist);
      c.cur_pos->argb_or_distance = transformed_dist;
    }
    VP8LRefsCursorNext(&c);
  }
}

static VP8LBackwardRefs* GetBackwardReferencesLowEffort(
    int width, int height, const uint32_t* const argb,
    int* const cache_bits, const VP8LHashChain* const hash_chain,
    VP8LBackwardRefs* const refs_lz77) {
  *cache_bits = 0;
  if (!BackwardReferencesLz77(width, height, argb, 0, hash_chain, refs_lz77)) {
    return NULL;
  }
  BackwardReferences2DLocality(width, refs_lz77);
  return refs_lz77;
}

#define NUM_LENGTH_CODES             24

static int VP8LHistogramNumCodes(int palette_code_bits) {
  return NUM_LITERAL_CODES + NUM_LENGTH_CODES +
      ((palette_code_bits > 0) ? (1 << palette_code_bits) : 0);
}

int VP8LGetHistogramSize(int cache_bits) {
  const int literal_size = VP8LHistogramNumCodes(cache_bits);
  const size_t total_size = sizeof(VP8LHistogram) + sizeof(int) * literal_size;
  assert(total_size <= (size_t)0x7fffffff);
  return (int)total_size;
}

static void HistogramClear(VP8LHistogram* const p) {
  uint32_t* const literal = p->literal_;
  const int cache_bits = p->palette_code_bits_;
  const int histo_size = VP8LGetHistogramSize(cache_bits);
  memset(p, 0, histo_size);
  p->palette_code_bits_ = cache_bits;
  p->literal_ = literal;
}

void VP8LHistogramInit(VP8LHistogram* const p, int palette_code_bits) {
  p->palette_code_bits_ = palette_code_bits;
  HistogramClear(p);
}

VP8LHistogram* VP8LAllocateHistogram(int cache_bits) {
  VP8LHistogram* histo = NULL;
  const int total_size = VP8LGetHistogramSize(cache_bits);
  uint8_t* const memory = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*memory));
  if (memory == NULL) return NULL;
  histo = (VP8LHistogram*)memory;
  // literal_ won't necessary be aligned.
  histo->literal_ = (uint32_t*)(memory + sizeof(VP8LHistogram));
  VP8LHistogramInit(histo, cache_bits);
  return histo;
}

static int BackwardReferencesRle(int xsize, int ysize,
                                 const uint32_t* const argb,
                                 int cache_bits, VP8LBackwardRefs* const refs) {
  const int pix_count = xsize * ysize;
  int i, k;
  const int use_color_cache = (cache_bits > 0);
  VP8LColorCache hashers;

  if (use_color_cache && !VP8LColorCacheInit(&hashers, cache_bits)) {
    return 0;
  }
  VP8LClearBackwardRefs(refs);
  // Add first pixel as literal.
  AddSingleLiteral(argb[0], use_color_cache, &hashers, refs);
  i = 1;
  while (i < pix_count) {
    const int max_len = MaxFindCopyLength(pix_count - i);
    const int rle_len = FindMatchLength(argb + i, argb + i - 1, 0, max_len);
    const int prev_row_len = (i < xsize) ? 0 :
        FindMatchLength(argb + i, argb + i - xsize, 0, max_len);
    if (rle_len >= prev_row_len && rle_len >= MIN_LENGTH) {
      VP8LBackwardRefsCursorAdd(refs, PixOrCopyCreateCopy(1, rle_len));
      // We don't need to update the color cache here since it is always the
      // same pixel being copied, and that does not change the color cache
      // state.
      i += rle_len;
    } else if (prev_row_len >= MIN_LENGTH) {
      VP8LBackwardRefsCursorAdd(refs, PixOrCopyCreateCopy(xsize, prev_row_len));
      if (use_color_cache) {
        for (k = 0; k < prev_row_len; ++k) {
          VP8LColorCacheInsert(&hashers, argb[i + k]);
        }
      }
      i += prev_row_len;
    } else {
      AddSingleLiteral(argb[i], use_color_cache, &hashers, refs);
      i++;
    }
  }
  if (use_color_cache) VP8LColorCacheClear(&hashers);
  return !refs->error_;
}

// Compute an LZ77 by forcing matches to happen within a given distance cost.
// We therefore limit the algorithm to the lowest 32 values in the PlaneCode
// definition.
#define WINDOW_OFFSETS_SIZE_MAX 32
static int BackwardReferencesLz77Box(int xsize, int ysize,
                                     const uint32_t* const argb, int cache_bits,
                                     const VP8LHashChain* const hash_chain_best,
                                     VP8LHashChain* hash_chain,
                                     VP8LBackwardRefs* const refs) {
  int i;
  const int pix_count = xsize * ysize;
  uint16_t* counts;
  int window_offsets[WINDOW_OFFSETS_SIZE_MAX] = {0};
  int window_offsets_new[WINDOW_OFFSETS_SIZE_MAX] = {0};
  int window_offsets_size = 0;
  int window_offsets_new_size = 0;
  uint16_t* const counts_ini =
      (uint16_t*)WebPSafeMalloc(xsize * ysize, sizeof(*counts_ini));
  int best_offset_prev = -1, best_length_prev = -1;
  if (counts_ini == NULL) return 0;

  // counts[i] counts how many times a pixel is repeated starting at position i.
  i = pix_count - 2;
  counts = counts_ini + i;
  counts[1] = 1;
  for (; i >= 0; --i, --counts) {
    if (argb[i] == argb[i + 1]) {
      // Max out the counts to MAX_LENGTH.
      counts[0] = counts[1] + (counts[1] != MAX_LENGTH);
    } else {
      counts[0] = 1;
    }
  }

  // Figure out the window offsets around a pixel. They are stored in a
  // spiraling order around the pixel as defined by VP8LDistanceToPlaneCode.
  {
    int x, y;
    for (y = 0; y <= 6; ++y) {
      for (x = -6; x <= 6; ++x) {
        const int offset = y * xsize + x;
        int plane_code;
        // Ignore offsets that bring us after the pixel.
        if (offset <= 0) continue;
        plane_code = VP8LDistanceToPlaneCode(xsize, offset) - 1;
        if (plane_code >= WINDOW_OFFSETS_SIZE_MAX) continue;
        window_offsets[plane_code] = offset;
      }
    }
    // For narrow images, not all plane codes are reached, so remove those.
    for (i = 0; i < WINDOW_OFFSETS_SIZE_MAX; ++i) {
      if (window_offsets[i] == 0) continue;
      window_offsets[window_offsets_size++] = window_offsets[i];
    }
    // Given a pixel P, find the offsets that reach pixels unreachable from P-1
    // with any of the offsets in window_offsets[].
    for (i = 0; i < window_offsets_size; ++i) {
      int j;
      int is_reachable = 0;
      for (j = 0; j < window_offsets_size && !is_reachable; ++j) {
        is_reachable |= (window_offsets[i] == window_offsets[j] + 1);
      }
      if (!is_reachable) {
        window_offsets_new[window_offsets_new_size] = window_offsets[i];
        ++window_offsets_new_size;
      }
    }
  }

  hash_chain->offset_length_[0] = 0;
  for (i = 1; i < pix_count; ++i) {
    int ind;
    int best_length = VP8LHashChainFindLength(hash_chain_best, i);
    int best_offset;
    int do_compute = 1;

    if (best_length >= MAX_LENGTH) {
      // Do not recompute the best match if we already have a maximal one in the
      // window.
      best_offset = VP8LHashChainFindOffset(hash_chain_best, i);
      for (ind = 0; ind < window_offsets_size; ++ind) {
        if (best_offset == window_offsets[ind]) {
          do_compute = 0;
          break;
        }
      }
    }
    if (do_compute) {
      // Figure out if we should use the offset/length from the previous pixel
      // as an initial guess and therefore only inspect the offsets in
      // window_offsets_new[].
      const int use_prev =
          (best_length_prev > 1) && (best_length_prev < MAX_LENGTH);
      const int num_ind =
          use_prev ? window_offsets_new_size : window_offsets_size;
      best_length = use_prev ? best_length_prev - 1 : 0;
      best_offset = use_prev ? best_offset_prev : 0;
      // Find the longest match in a window around the pixel.
      for (ind = 0; ind < num_ind; ++ind) {
        int curr_length = 0;
        int j = i;
        int j_offset =
            use_prev ? i - window_offsets_new[ind] : i - window_offsets[ind];
        if (j_offset < 0 || argb[j_offset] != argb[i]) continue;
        // The longest match is the sum of how many times each pixel is
        // repeated.
        do {
          const int counts_j_offset = counts_ini[j_offset];
          const int counts_j = counts_ini[j];
          if (counts_j_offset != counts_j) {
            curr_length +=
                (counts_j_offset < counts_j) ? counts_j_offset : counts_j;
            break;
          }
          // The same color is repeated counts_pos times at j_offset and j.
          curr_length += counts_j_offset;
          j_offset += counts_j_offset;
          j += counts_j_offset;
        } while (curr_length <= MAX_LENGTH && j < pix_count &&
                 argb[j_offset] == argb[j]);
        if (best_length < curr_length) {
          best_offset =
              use_prev ? window_offsets_new[ind] : window_offsets[ind];
          if (curr_length >= MAX_LENGTH) {
            best_length = MAX_LENGTH;
            break;
          } else {
            best_length = curr_length;
          }
        }
      }
    }

    assert(i + best_length <= pix_count);
    assert(best_length <= MAX_LENGTH);
    if (best_length <= MIN_LENGTH) {
      hash_chain->offset_length_[i] = 0;
      best_offset_prev = 0;
      best_length_prev = 0;
    } else {
      hash_chain->offset_length_[i] =
          (best_offset << MAX_LENGTH_BITS) | (uint32_t)best_length;
      best_offset_prev = best_offset;
      best_length_prev = best_length;
    }
  }
  hash_chain->offset_length_[0] = 0;
  WebPSafeFree(counts_ini);

  return BackwardReferencesLz77(xsize, ysize, argb, cache_bits, hash_chain,
                                refs);
}

#define MAX_ENTROPY    (1e30f)

static int PixOrCopyIsLiteral(const PixOrCopy* const p) {
  return (p->mode == kLiteral);
}

static uint32_t PixOrCopyLength(const PixOrCopy* const p) {
  return p->len;
}

typedef struct {        // small struct to hold counters
  int counts[2];        // index: 0=zero steak, 1=non-zero streak
  int streaks[2][2];    // [zero/non-zero][streak<3 / streak>=3]
} VP8LStreaks;

static void GetEntropyUnrefinedHelper(
    uint32_t val, int i, uint32_t* const val_prev, int* const i_prev,
    VP8LBitEntropy* const bit_entropy, VP8LStreaks* const stats) {
  const int streak = i - *i_prev;

  // Gather info for the bit entropy.
  if (*val_prev != 0) {
    bit_entropy->sum += (*val_prev) * streak;
    bit_entropy->nonzeros += streak;
    bit_entropy->nonzero_code = *i_prev;
    bit_entropy->entropy -= VP8LFastSLog2(*val_prev) * streak;
    if (bit_entropy->max_val < *val_prev) {
      bit_entropy->max_val = *val_prev;
    }
  }

  // Gather info for the Huffman cost.
  stats->counts[*val_prev != 0] += (streak > 3);
  stats->streaks[*val_prev != 0][(streak > 3)] += streak;

  *val_prev = val;
  *i_prev = i;
}

static void GetEntropyUnrefined_C(const uint32_t X[], int length,
                                  VP8LBitEntropy* const bit_entropy,
                                  VP8LStreaks* const stats) {
  int i;
  int i_prev = 0;
  uint32_t x_prev = X[0];

  memset(stats, 0, sizeof(*stats));
  VP8LBitEntropyInit(bit_entropy);

  for (i = 1; i < length; ++i) {
    const uint32_t x = X[i];
    if (x != x_prev) {
      GetEntropyUnrefinedHelper(x, i, &x_prev, &i_prev, bit_entropy, stats);
    }
  }
  GetEntropyUnrefinedHelper(0, i, &x_prev, &i_prev, bit_entropy, stats);

  bit_entropy->entropy += VP8LFastSLog2(bit_entropy->sum);
}

static double InitialHuffmanCost(void) {
  // Small bias because Huffman code length is typically not stored in
  // full length.
  static const int kHuffmanCodeOfHuffmanCodeSize = CODE_LENGTH_CODES * 3;
  static const double kSmallBias = 9.1;
  return kHuffmanCodeOfHuffmanCodeSize - kSmallBias;
}

// Finalize the Huffman cost based on streak numbers and length type (<3 or >=3)
static double FinalHuffmanCost(const VP8LStreaks* const stats) {
  // The constants in this function are experimental and got rounded from
  // their original values in 1/8 when switched to 1/1024.
  double retval = InitialHuffmanCost();
  // Second coefficient: Many zeros in the histogram are covered efficiently
  // by a run-length encode. Originally 2/8.
  retval += stats->counts[0] * 1.5625 + 0.234375 * stats->streaks[0][1];
  // Second coefficient: Constant values are encoded less efficiently, but still
  // RLE'ed. Originally 6/8.
  retval += stats->counts[1] * 2.578125 + 0.703125 * stats->streaks[1][1];
  // 0s are usually encoded more efficiently than non-0s.
  // Originally 15/8.
  retval += 1.796875 * stats->streaks[0][0];
  // Originally 26/8.
  retval += 3.28125 * stats->streaks[1][0];
  return retval;
}

// Get the symbol entropy for the distribution 'population'.
// Set 'trivial_sym', if there's only one symbol present in the distribution.
static double PopulationCost(const uint32_t* const population, int length,
                             uint32_t* const trivial_sym) {
  VP8LBitEntropy bit_entropy;
  VP8LStreaks stats;
  GetEntropyUnrefined_C(population, length, &bit_entropy, &stats);
  if (trivial_sym != NULL) {
    *trivial_sym = (bit_entropy.nonzeros == 1) ? bit_entropy.nonzero_code
                                               : VP8L_NON_TRIVIAL_SYM;
  }

  return BitsEntropyRefine(&bit_entropy) + FinalHuffmanCost(&stats);
}

static double ExtraCost_C(const uint32_t* population, int length) {
  int i;
  double cost = 0.;
  for (i = 2; i < length - 2; ++i) cost += (i >> 1) * population[i + 2];
  return cost;
}

// Estimates the Entropy + Huffman + other block overhead size cost.
double VP8LHistogramEstimateBits(const VP8LHistogram* const p) {
  return
      PopulationCost(
          p->literal_, VP8LHistogramNumCodes(p->palette_code_bits_), NULL)
      + PopulationCost(p->red_, NUM_LITERAL_CODES, NULL)
      + PopulationCost(p->blue_, NUM_LITERAL_CODES, NULL)
      + PopulationCost(p->alpha_, NUM_LITERAL_CODES, NULL)
      + PopulationCost(p->distance_, NUM_DISTANCE_CODES, NULL)
      + ExtraCost_C(p->literal_ + NUM_LITERAL_CODES, NUM_LENGTH_CODES)
      + ExtraCost_C(p->distance_, NUM_DISTANCE_CODES);
}

void VP8LFreeHistogram(VP8LHistogram* const histo) {
  WebPSafeFree(histo);
}

// Evaluate optimal cache bits for the local color cache.
// The input *best_cache_bits sets the maximum cache bits to use (passing 0
// implies disabling the local color cache). The local color cache is also
// disabled for the lower (<= 25) quality.
// Returns 0 in case of memory error.
static int CalculateBestCacheSize(const uint32_t* argb, int quality,
                                  const VP8LBackwardRefs* const refs,
                                  int* const best_cache_bits) {
  int i;
  const int cache_bits_max = (quality <= 25) ? 0 : *best_cache_bits;
  double entropy_min = MAX_ENTROPY;
  int cc_init[MAX_COLOR_CACHE_BITS + 1] = { 0 };
  VP8LColorCache hashers[MAX_COLOR_CACHE_BITS + 1];
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  VP8LHistogram* histos[MAX_COLOR_CACHE_BITS + 1] = { NULL };
  int ok = 0;

  assert(cache_bits_max >= 0 && cache_bits_max <= MAX_COLOR_CACHE_BITS);

  if (cache_bits_max == 0) {
    *best_cache_bits = 0;
    // Local color cache is disabled.
    return 1;
  }

  // Allocate data.
  for (i = 0; i <= cache_bits_max; ++i) {
    histos[i] = VP8LAllocateHistogram(i);
    if (histos[i] == NULL) goto Error;
    if (i == 0) continue;
    cc_init[i] = VP8LColorCacheInit(&hashers[i], i);
    if (!cc_init[i]) goto Error;
  }

  // Find the cache_bits giving the lowest entropy. The search is done in a
  // brute-force way as the function (entropy w.r.t cache_bits) can be
  // anything in practice.
  while (VP8LRefsCursorOk(&c)) {
    const PixOrCopy* const v = c.cur_pos;
    if (PixOrCopyIsLiteral(v)) {
      const uint32_t pix = *argb++;
      const uint32_t a = (pix >> 24) & 0xff;
      const uint32_t r = (pix >> 16) & 0xff;
      const uint32_t g = (pix >>  8) & 0xff;
      const uint32_t b = (pix >>  0) & 0xff;
      // The keys of the caches can be derived from the longest one.
      int key = VP8LHashPix(pix, 32 - cache_bits_max);
      // Do not use the color cache for cache_bits = 0.
      ++histos[0]->blue_[b];
      ++histos[0]->literal_[g];
      ++histos[0]->red_[r];
      ++histos[0]->alpha_[a];
      // Deal with cache_bits > 0.
      for (i = cache_bits_max; i >= 1; --i, key >>= 1) {
        if (VP8LColorCacheLookup(&hashers[i], key) == pix) {
          ++histos[i]->literal_[NUM_LITERAL_CODES + NUM_LENGTH_CODES + key];
        } else {
          VP8LColorCacheSet(&hashers[i], key, pix);
          ++histos[i]->blue_[b];
          ++histos[i]->literal_[g];
          ++histos[i]->red_[r];
          ++histos[i]->alpha_[a];
        }
      }
    } else {
      // We should compute the contribution of the (distance,length)
      // histograms but those are the same independently from the cache size.
      // As those constant contributions are in the end added to the other
      // histogram contributions, we can safely ignore them.
      int len = PixOrCopyLength(v);
      uint32_t argb_prev = *argb ^ 0xffffffffu;
      // Update the color caches.
      do {
        if (*argb != argb_prev) {
          // Efficiency: insert only if the color changes.
          int key = VP8LHashPix(*argb, 32 - cache_bits_max);
          for (i = cache_bits_max; i >= 1; --i, key >>= 1) {
            hashers[i].colors_[key] = *argb;
          }
          argb_prev = *argb;
        }
        argb++;
      } while (--len != 0);
    }
    VP8LRefsCursorNext(&c);
  }

  for (i = 0; i <= cache_bits_max; ++i) {
    const double entropy = VP8LHistogramEstimateBits(histos[i]);
    if (i == 0 || entropy < entropy_min) {
      entropy_min = entropy;
      *best_cache_bits = i;
    }
  }
  ok = 1;
Error:
  for (i = 0; i <= cache_bits_max; ++i) {
    if (cc_init[i]) VP8LColorCacheClear(&hashers[i]);
    VP8LFreeHistogram(histos[i]);
  }
  return ok;
}

// Return the key if cc contains argb, and -1 otherwise.
static int VP8LColorCacheContains(const VP8LColorCache* const cc,
                                              uint32_t argb) {
  const int key = VP8LHashPix(argb, cc->hash_shift_);
  return (cc->colors_[key] == argb) ? key : -1;
}

// Update (in-place) backward references for specified cache_bits.
static int BackwardRefsWithLocalCache(const uint32_t* const argb,
                                      int cache_bits,
                                      VP8LBackwardRefs* const refs) {
  int pixel_index = 0;
  VP8LColorCache hashers;
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  if (!VP8LColorCacheInit(&hashers, cache_bits)) return 0;

  while (VP8LRefsCursorOk(&c)) {
    PixOrCopy* const v = c.cur_pos;
    if (PixOrCopyIsLiteral(v)) {
      const uint32_t argb_literal = v->argb_or_distance;
      const int ix = VP8LColorCacheContains(&hashers, argb_literal);
      if (ix >= 0) {
        // hashers contains argb_literal
        *v = PixOrCopyCreateCacheIdx(ix);
      } else {
        VP8LColorCacheInsert(&hashers, argb_literal);
      }
      ++pixel_index;
    } else {
      // refs was created without local cache, so it can not have cache indexes.
      int k;
      assert(PixOrCopyIsCopy(v));
      for (k = 0; k < v->len; ++k) {
        VP8LColorCacheInsert(&hashers, argb[pixel_index++]);
      }
    }
    VP8LRefsCursorNext(&c);
  }
  VP8LColorCacheClear(&hashers);
  return 1;
}

static uint32_t PixOrCopyLiteral(const PixOrCopy* const p,
                                             int component) {
  assert(p->mode == kLiteral);
  return (p->argb_or_distance >> (component * 8)) & 0xff;
}

static int PixOrCopyIsCacheIdx(const PixOrCopy* const p) {
  return (p->mode == kCacheIdx);
}

#define PREFIX_LOOKUP_IDX_MAX   512
typedef struct {
  int8_t code_;
  int8_t extra_bits_;
} VP8LPrefixCode;

const VP8LPrefixCode kPrefixEncodeCode[PREFIX_LOOKUP_IDX_MAX] = {
  { 0, 0}, { 0, 0}, { 1, 0}, { 2, 0}, { 3, 0}, { 4, 1}, { 4, 1}, { 5, 1},
  { 5, 1}, { 6, 2}, { 6, 2}, { 6, 2}, { 6, 2}, { 7, 2}, { 7, 2}, { 7, 2},
  { 7, 2}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3},
  { 8, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3},
  { 9, 3}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4},
  {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4},
  {10, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4},
  {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4},
  {11, 4}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
};

const uint8_t WebPLogTable8bit[256] = {   // 31 ^ clz(i)
  0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static int WebPLog2FloorC(uint32_t n) {
  int log_value = 0;
  while (n >= 256) {
    log_value += 8;
    n >>= 8;
  }
  return log_value + WebPLogTable8bit[n];
}

static int BitsLog2Floor(uint32_t n) { return WebPLog2FloorC(n); }

// Splitting of distance and length codes into prefixes and
// extra bits. The prefixes are encoded with an entropy code
// while the extra bits are stored just as normal bits.
static void VP8LPrefixEncodeBitsNoLUT(int distance, int* const code,
                                                  int* const extra_bits) {
  const int highest_bit = BitsLog2Floor(--distance);
  const int second_highest_bit = (distance >> (highest_bit - 1)) & 1;
  *extra_bits = highest_bit - 1;
  *code = 2 * highest_bit + second_highest_bit;
}

static void VP8LPrefixEncodeBits(int distance, int* const code,
                                             int* const extra_bits) {
  if (distance < PREFIX_LOOKUP_IDX_MAX) {
    const VP8LPrefixCode prefix_code = kPrefixEncodeCode[distance];
    *code = prefix_code.code_;
    *extra_bits = prefix_code.extra_bits_;
  } else {
    VP8LPrefixEncodeBitsNoLUT(distance, code, extra_bits);
  }
}

static uint32_t PixOrCopyDistance(const PixOrCopy* const p) {
  assert(p->mode == kCopy);
  return p->argb_or_distance;
}

static uint32_t PixOrCopyCacheIdx(const PixOrCopy* const p) {
  assert(p->mode == kCacheIdx);
  assert(p->argb_or_distance < (1U << MAX_COLOR_CACHE_BITS));
  return p->argb_or_distance;
}

void VP8LHistogramAddSinglePixOrCopy(VP8LHistogram* const histo,
                                     const PixOrCopy* const v,
                                     int (*const distance_modifier)(int, int),
                                     int distance_modifier_arg0) {
  if (PixOrCopyIsLiteral(v)) {
    ++histo->alpha_[PixOrCopyLiteral(v, 3)];
    ++histo->red_[PixOrCopyLiteral(v, 2)];
    ++histo->literal_[PixOrCopyLiteral(v, 1)];
    ++histo->blue_[PixOrCopyLiteral(v, 0)];
  } else if (PixOrCopyIsCacheIdx(v)) {
    const int literal_ix =
        NUM_LITERAL_CODES + NUM_LENGTH_CODES + PixOrCopyCacheIdx(v);
    ++histo->literal_[literal_ix];
  } else {
    int code, extra_bits;
    VP8LPrefixEncodeBits(PixOrCopyLength(v), &code, &extra_bits);
    ++histo->literal_[NUM_LITERAL_CODES + code];
    if (distance_modifier == NULL) {
      VP8LPrefixEncodeBits(PixOrCopyDistance(v), &code, &extra_bits);
    } else {
      VP8LPrefixEncodeBits(
          distance_modifier(distance_modifier_arg0, PixOrCopyDistance(v)),
          &code, &extra_bits);
    }
    ++histo->distance_[code];
  }
}

void VP8LHistogramStoreRefs(const VP8LBackwardRefs* const refs,
                            VP8LHistogram* const histo) {
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  while (VP8LRefsCursorOk(&c)) {
    VP8LHistogramAddSinglePixOrCopy(histo, c.cur_pos, NULL, 0);
    VP8LRefsCursorNext(&c);
  }
}

void VP8LHistogramCreate(VP8LHistogram* const p,
                         const VP8LBackwardRefs* const refs,
                         int palette_code_bits) {
  if (palette_code_bits >= 0) {
    p->palette_code_bits_ = palette_code_bits;
  }
  HistogramClear(p);
  VP8LHistogramStoreRefs(refs, p);
}

#define VALUES_IN_BYTE 256

typedef struct {
  double alpha_[VALUES_IN_BYTE];
  double red_[VALUES_IN_BYTE];
  double blue_[VALUES_IN_BYTE];
  double distance_[NUM_DISTANCE_CODES];
  double* literal_;
} CostModel;

// The GetLengthCost(cost_model, k) are cached in a CostCacheInterval.
typedef struct {
  double cost_;
  int start_;
  int end_;       // Exclusive.
} CostCacheInterval;

// To perform backward reference every pixel at index index_ is considered and
// the cost for the MAX_LENGTH following pixels computed. Those following pixels
// at index index_ + k (k from 0 to MAX_LENGTH) have a cost of:
//     cost_ = distance cost at index + GetLengthCost(cost_model, k)
// and the minimum value is kept. GetLengthCost(cost_model, k) is cached in an
// array of size MAX_LENGTH.
// Instead of performing MAX_LENGTH comparisons per pixel, we keep track of the
// minimal values using intervals of constant cost.
// An interval is defined by the index_ of the pixel that generated it and
// is only useful in a range of indices from start_ to end_ (exclusive), i.e.
// it contains the minimum value for pixels between start_ and end_.
// Intervals are stored in a linked list and ordered by start_. When a new
// interval has a better value, old intervals are split or removed. There are
// therefore no overlapping intervals.
typedef struct CostInterval CostInterval;
struct CostInterval {
  float cost_;
  int start_;
  int end_;
  int index_;
  CostInterval* previous_;
  CostInterval* next_;
};

// This structure is in charge of managing intervals and costs.
// It caches the different CostCacheInterval, caches the different
// GetLengthCost(cost_model, k) in cost_cache_ and the CostInterval's (whose
// count_ is limited by COST_CACHE_INTERVAL_SIZE_MAX).
#define COST_MANAGER_MAX_FREE_LIST 10
typedef struct {
  CostInterval* head_;
  int count_;  // The number of stored intervals.
  CostCacheInterval* cache_intervals_;
  size_t cache_intervals_size_;
  double cost_cache_[MAX_LENGTH];  // Contains the GetLengthCost(cost_model, k).
  float* costs_;
  uint16_t* dist_array_;
  // Most of the time, we only need few intervals -> use a free-list, to avoid
  // fragmentation with small allocs in most common cases.
  CostInterval intervals_[COST_MANAGER_MAX_FREE_LIST];
  CostInterval* free_intervals_;
  // These are regularly malloc'd remains. This list can't grow larger than than
  // size COST_CACHE_INTERVAL_SIZE_MAX - COST_MANAGER_MAX_FREE_LIST, note.
  CostInterval* recycled_intervals_;
} CostManager;

static void ConvertPopulationCountTableToBitEstimates(
    int num_symbols, const uint32_t population_counts[], double output[]) {
  uint32_t sum = 0;
  int nonzeros = 0;
  int i;
  for (i = 0; i < num_symbols; ++i) {
    sum += population_counts[i];
    if (population_counts[i] > 0) {
      ++nonzeros;
    }
  }
  if (nonzeros <= 1) {
    memset(output, 0, num_symbols * sizeof(*output));
  } else {
    const double logsum = VP8LFastLog2(sum);
    for (i = 0; i < num_symbols; ++i) {
      output[i] = logsum - VP8LFastLog2(population_counts[i]);
    }
  }
}

static int CostModelBuild(CostModel* const m, int xsize, int cache_bits,
                          const VP8LBackwardRefs* const refs) {
  int ok = 0;
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  VP8LHistogram* const histo = VP8LAllocateHistogram(cache_bits);
  if (histo == NULL) goto Error;

  // The following code is similar to VP8LHistogramCreate but converts the
  // distance to plane code.
  VP8LHistogramInit(histo, cache_bits);
  while (VP8LRefsCursorOk(&c)) {
    VP8LHistogramAddSinglePixOrCopy(histo, c.cur_pos, VP8LDistanceToPlaneCode,
                                    xsize);
    VP8LRefsCursorNext(&c);
  }

  ConvertPopulationCountTableToBitEstimates(
      VP8LHistogramNumCodes(histo->palette_code_bits_),
      histo->literal_, m->literal_);
  ConvertPopulationCountTableToBitEstimates(
      VALUES_IN_BYTE, histo->red_, m->red_);
  ConvertPopulationCountTableToBitEstimates(
      VALUES_IN_BYTE, histo->blue_, m->blue_);
  ConvertPopulationCountTableToBitEstimates(
      VALUES_IN_BYTE, histo->alpha_, m->alpha_);
  ConvertPopulationCountTableToBitEstimates(
      NUM_DISTANCE_CODES, histo->distance_, m->distance_);
  ok = 1;

 Error:
  VP8LFreeHistogram(histo);
  return ok;
}

static void CostIntervalAddToFreeList(CostManager* const manager,
                                      CostInterval* const interval) {
  interval->next_ = manager->free_intervals_;
  manager->free_intervals_ = interval;
}

static void CostManagerInitFreeList(CostManager* const manager) {
  int i;
  manager->free_intervals_ = NULL;
  for (i = 0; i < COST_MANAGER_MAX_FREE_LIST; ++i) {
    CostIntervalAddToFreeList(manager, &manager->intervals_[i]);
  }
}

static double GetLengthCost(const CostModel* const m,
                                        uint32_t length) {
  int code, extra_bits;
  VP8LPrefixEncodeBits(length, &code, &extra_bits);
  return m->literal_[VALUES_IN_BYTE + code] + extra_bits;
}

static int CostIntervalIsInFreeList(const CostManager* const manager,
                                    const CostInterval* const interval) {
  return (interval >= &manager->intervals_[0] &&
          interval <= &manager->intervals_[COST_MANAGER_MAX_FREE_LIST - 1]);
}

static void DeleteIntervalList(CostManager* const manager,
                               const CostInterval* interval) {
  while (interval != NULL) {
    const CostInterval* const next = interval->next_;
    if (!CostIntervalIsInFreeList(manager, interval)) {
      WebPSafeFree((void*)interval);
    }  // else: do nothing
    interval = next;
  }
}

static void CostManagerClear(CostManager* const manager) {
  if (manager == NULL) return;

  WebPSafeFree(manager->costs_);
  WebPSafeFree(manager->cache_intervals_);

  // Clear the interval lists.
  DeleteIntervalList(manager, manager->head_);
  manager->head_ = NULL;
  DeleteIntervalList(manager, manager->recycled_intervals_);
  manager->recycled_intervals_ = NULL;

  // Reset pointers, count_ and cache_intervals_size_.
  memset(manager, 0, sizeof(*manager));
  CostManagerInitFreeList(manager);
}

static int CostManagerInit(CostManager* const manager,
                           uint16_t* const dist_array, int pix_count,
                           const CostModel* const cost_model) {
  int i;
  const int cost_cache_size = (pix_count > MAX_LENGTH) ? MAX_LENGTH : pix_count;

  manager->costs_ = NULL;
  manager->cache_intervals_ = NULL;
  manager->head_ = NULL;
  manager->recycled_intervals_ = NULL;
  manager->count_ = 0;
  manager->dist_array_ = dist_array;
  CostManagerInitFreeList(manager);

  // Fill in the cost_cache_.
  manager->cache_intervals_size_ = 1;
  manager->cost_cache_[0] = GetLengthCost(cost_model, 0);
  for (i = 1; i < cost_cache_size; ++i) {
    manager->cost_cache_[i] = GetLengthCost(cost_model, i);
    // Get the number of bound intervals.
    if (manager->cost_cache_[i] != manager->cost_cache_[i - 1]) {
      ++manager->cache_intervals_size_;
    }
  }

  // With the current cost model, we usually have below 20 intervals.
  // The worst case scenario with a cost model would be if every length has a
  // different cost, hence MAX_LENGTH but that is impossible with the current
  // implementation that spirals around a pixel.
  assert(manager->cache_intervals_size_ <= MAX_LENGTH);
  manager->cache_intervals_ = (CostCacheInterval*)WebPSafeMalloc(
      manager->cache_intervals_size_, sizeof(*manager->cache_intervals_));
  if (manager->cache_intervals_ == NULL) {
    CostManagerClear(manager);
    return 0;
  }

  // Fill in the cache_intervals_.
  {
    CostCacheInterval* cur = manager->cache_intervals_;

    // Consecutive values in cost_cache_ are compared and if a big enough
    // difference is found, a new interval is created and bounded.
    cur->start_ = 0;
    cur->end_ = 1;
    cur->cost_ = manager->cost_cache_[0];
    for (i = 1; i < cost_cache_size; ++i) {
      const double cost_val = manager->cost_cache_[i];
      if (cost_val != cur->cost_) {
        ++cur;
        // Initialize an interval.
        cur->start_ = i;
        cur->cost_ = cost_val;
      }
      cur->end_ = i + 1;
    }
  }

  manager->costs_ = (float*)WebPSafeMalloc(pix_count, sizeof(*manager->costs_));
  if (manager->costs_ == NULL) {
    CostManagerClear(manager);
    return 0;
  }
  // Set the initial costs_ high for every pixel as we will keep the minimum.
  for (i = 0; i < pix_count; ++i) manager->costs_[i] = 1e38f;

  return 1;
}

static double GetCacheCost(const CostModel* const m, uint32_t idx) {
  const int literal_idx = VALUES_IN_BYTE + NUM_LENGTH_CODES + idx;
  return m->literal_[literal_idx];
}

static double GetLiteralCost(const CostModel* const m, uint32_t v) {
  return m->alpha_[v >> 24] +
         m->red_[(v >> 16) & 0xff] +
         m->literal_[(v >> 8) & 0xff] +
         m->blue_[v & 0xff];
}

static void AddSingleLiteralWithCostModel(
    const uint32_t* const argb, VP8LColorCache* const hashers,
    const CostModel* const cost_model, int idx, int use_color_cache,
    float prev_cost, float* const cost, uint16_t* const dist_array) {
  double cost_val = prev_cost;
  const uint32_t color = argb[idx];
  const int ix = use_color_cache ? VP8LColorCacheContains(hashers, color) : -1;
  if (ix >= 0) {
    // use_color_cache is true and hashers contains color
    const double mul0 = 0.68;
    cost_val += GetCacheCost(cost_model, ix) * mul0;
  } else {
    const double mul1 = 0.82;
    if (use_color_cache) VP8LColorCacheInsert(hashers, color);
    cost_val += GetLiteralCost(cost_model, color) * mul1;
  }
  if (cost[idx] > cost_val) {
    cost[idx] = (float)cost_val;
    dist_array[idx] = 1;  // only one is inserted.
  }
}

static double GetDistanceCost(const CostModel* const m,
                                          uint32_t distance) {
  int code, extra_bits;
  VP8LPrefixEncodeBits(distance, &code, &extra_bits);
  return m->distance_[code] + extra_bits;
}

// Empirical value to avoid high memory consumption but good for performance.
#define COST_CACHE_INTERVAL_SIZE_MAX 500

// Given the cost and the position that define an interval, update the cost at
// pixel 'i' if it is smaller than the previously computed value.
static void UpdateCost(CostManager* const manager, int i,
                                   int position, float cost) {
  const int k = i - position;
  assert(k >= 0 && k < MAX_LENGTH);

  if (manager->costs_[i] > cost) {
    manager->costs_[i] = cost;
    manager->dist_array_[i] = k + 1;
  }
}

// Given the cost and the position that define an interval, update the cost for
// all the pixels between 'start' and 'end' excluded.
static void UpdateCostPerInterval(CostManager* const manager,
                                              int start, int end, int position,
                                              float cost) {
  int i;
  for (i = start; i < end; ++i) UpdateCost(manager, i, position, cost);
}

// Given two intervals, make 'prev' be the previous one of 'next' in 'manager'.
static void ConnectIntervals(CostManager* const manager,
                                         CostInterval* const prev,
                                         CostInterval* const next) {
  if (prev != NULL) {
    prev->next_ = next;
  } else {
    manager->head_ = next;
  }

  if (next != NULL) next->previous_ = prev;
}

// Given a current orphan interval and its previous interval, before
// it was orphaned (which can be NULL), set it at the right place in the list
// of intervals using the start_ ordering and the previous interval as a hint.
static void PositionOrphanInterval(CostManager* const manager,
                                               CostInterval* const current,
                                               CostInterval* previous) {
  assert(current != NULL);

  if (previous == NULL) previous = manager->head_;
  while (previous != NULL && current->start_ < previous->start_) {
    previous = previous->previous_;
  }
  while (previous != NULL && previous->next_ != NULL &&
         previous->next_->start_ < current->start_) {
    previous = previous->next_;
  }

  if (previous != NULL) {
    ConnectIntervals(manager, current, previous->next_);
  } else {
    ConnectIntervals(manager, current, manager->head_);
  }
  ConnectIntervals(manager, previous, current);
}

// Insert an interval in the list contained in the manager by starting at
// interval_in as a hint. The intervals are sorted by start_ value.
static void InsertInterval(CostManager* const manager,
                                       CostInterval* const interval_in,
                                       float cost, int position, int start,
                                       int end) {
  CostInterval* interval_new;

  if (start >= end) return;
  if (manager->count_ >= COST_CACHE_INTERVAL_SIZE_MAX) {
    // Serialize the interval if we cannot store it.
    UpdateCostPerInterval(manager, start, end, position, cost);
    return;
  }
  if (manager->free_intervals_ != NULL) {
    interval_new = manager->free_intervals_;
    manager->free_intervals_ = interval_new->next_;
  } else if (manager->recycled_intervals_ != NULL) {
    interval_new = manager->recycled_intervals_;
    manager->recycled_intervals_ = interval_new->next_;
  } else {  // malloc for good
    interval_new = (CostInterval*)WebPSafeMalloc(1, sizeof(*interval_new));
    if (interval_new == NULL) {
      // Write down the interval if we cannot create it.
      UpdateCostPerInterval(manager, start, end, position, cost);
      return;
    }
  }

  interval_new->cost_ = cost;
  interval_new->index_ = position;
  interval_new->start_ = start;
  interval_new->end_ = end;
  PositionOrphanInterval(manager, interval_new, interval_in);

  ++manager->count_;
}

// Pop an interval in the manager.
static void PopInterval(CostManager* const manager,
                                    CostInterval* const interval) {
  if (interval == NULL) return;

  ConnectIntervals(manager, interval->previous_, interval->next_);
  if (CostIntervalIsInFreeList(manager, interval)) {
    CostIntervalAddToFreeList(manager, interval);
  } else {  // recycle regularly malloc'd intervals too
    interval->next_ = manager->recycled_intervals_;
    manager->recycled_intervals_ = interval;
  }
  --manager->count_;
  assert(manager->count_ >= 0);
}

// Given a new cost interval defined by its start at position, its length value
// and distance_cost, add its contributions to the previous intervals and costs.
// If handling the interval or one of its subintervals becomes to heavy, its
// contribution is added to the costs right away.
static void PushInterval(CostManager* const manager,
                                     double distance_cost, int position,
                                     int len) {
  size_t i;
  CostInterval* interval = manager->head_;
  CostInterval* interval_next;
  const CostCacheInterval* const cost_cache_intervals =
      manager->cache_intervals_;
  // If the interval is small enough, no need to deal with the heavy
  // interval logic, just serialize it right away. This constant is empirical.
  const int kSkipDistance = 10;

  if (len < kSkipDistance) {
    int j;
    for (j = position; j < position + len; ++j) {
      const int k = j - position;
      float cost_tmp;
      assert(k >= 0 && k < MAX_LENGTH);
      cost_tmp = (float)(distance_cost + manager->cost_cache_[k]);

      if (manager->costs_[j] > cost_tmp) {
        manager->costs_[j] = cost_tmp;
        manager->dist_array_[j] = k + 1;
      }
    }
    return;
  }

  for (i = 0; i < manager->cache_intervals_size_ &&
              cost_cache_intervals[i].start_ < len;
       ++i) {
    // Define the intersection of the ith interval with the new one.
    int start = position + cost_cache_intervals[i].start_;
    const int end = position + (cost_cache_intervals[i].end_ > len
                                 ? len
                                 : cost_cache_intervals[i].end_);
    const float cost = (float)(distance_cost + cost_cache_intervals[i].cost_);

    for (; interval != NULL && interval->start_ < end;
         interval = interval_next) {
      interval_next = interval->next_;

      // Make sure we have some overlap
      if (start >= interval->end_) continue;

      if (cost >= interval->cost_) {
        // When intervals are represented, the lower, the better.
        // [**********************************************************[
        // start                                                    end
        //                   [----------------------------------[
        //                   interval->start_       interval->end_
        // If we are worse than what we already have, add whatever we have so
        // far up to interval.
        const int start_new = interval->end_;
        InsertInterval(manager, interval, cost, position, start,
                       interval->start_);
        start = start_new;
        if (start >= end) break;
        continue;
      }

      if (start <= interval->start_) {
        if (interval->end_ <= end) {
          //                   [----------------------------------[
          //                   interval->start_       interval->end_
          // [**************************************************************[
          // start                                                        end
          // We can safely remove the old interval as it is fully included.
          PopInterval(manager, interval);
        } else {
          //              [------------------------------------[
          //              interval->start_        interval->end_
          // [*****************************[
          // start                       end
          interval->start_ = end;
          break;
        }
      } else {
        if (end < interval->end_) {
          // [--------------------------------------------------------------[
          // interval->start_                                  interval->end_
          //                     [*****************************[
          //                     start                       end
          // We have to split the old interval as it fully contains the new one.
          const int end_original = interval->end_;
          interval->end_ = start;
          InsertInterval(manager, interval, interval->cost_, interval->index_,
                         end, end_original);
          interval = interval->next_;
          break;
        } else {
          // [------------------------------------[
          // interval->start_        interval->end_
          //                     [*****************************[
          //                     start                       end
          interval->end_ = start;
        }
      }
    }
    // Insert the remaining interval from start to end.
    InsertInterval(manager, interval, cost, position, start, end);
  }
}

// Update the cost at index i by going over all the stored intervals that
// overlap with i.
// If 'do_clean_intervals' is set to something different than 0, intervals that
// end before 'i' will be popped.
static void UpdateCostAtIndex(CostManager* const manager, int i,
                                          int do_clean_intervals) {
  CostInterval* current = manager->head_;

  while (current != NULL && current->start_ <= i) {
    CostInterval* const next = current->next_;
    if (current->end_ <= i) {
      if (do_clean_intervals) {
        // We have an outdated interval, remove it.
        PopInterval(manager, current);
      }
    } else {
      UpdateCost(manager, i, current->index_, current->cost_);
    }
    current = next;
  }
}

static int BackwardReferencesHashChainDistanceOnly(
    int xsize, int ysize, const uint32_t* const argb, int cache_bits,
    const VP8LHashChain* const hash_chain, const VP8LBackwardRefs* const refs,
    uint16_t* const dist_array) {
  int i;
  int ok = 0;
  int cc_init = 0;
  const int pix_count = xsize * ysize;
  const int use_color_cache = (cache_bits > 0);
  const size_t literal_array_size =
      sizeof(double) * (NUM_LITERAL_CODES + NUM_LENGTH_CODES +
                        ((cache_bits > 0) ? (1 << cache_bits) : 0));
  const size_t cost_model_size = sizeof(CostModel) + literal_array_size;
  CostModel* const cost_model =
      (CostModel*)WebPSafeCalloc(1ULL, cost_model_size);
  VP8LColorCache hashers;
  CostManager* cost_manager =
      (CostManager*)WebPSafeMalloc(1ULL, sizeof(*cost_manager));
  int offset_prev = -1, len_prev = -1;
  double offset_cost = -1;
  int first_offset_is_constant = -1;  // initialized with 'impossible' value
  int reach = 0;

  if (cost_model == NULL || cost_manager == NULL) goto Error;

  cost_model->literal_ = (double*)(cost_model + 1);
  if (use_color_cache) {
    cc_init = VP8LColorCacheInit(&hashers, cache_bits);
    if (!cc_init) goto Error;
  }

  if (!CostModelBuild(cost_model, xsize, cache_bits, refs)) {
    goto Error;
  }

  if (!CostManagerInit(cost_manager, dist_array, pix_count, cost_model)) {
    goto Error;
  }

  // We loop one pixel at a time, but store all currently best points to
  // non-processed locations from this point.
  dist_array[0] = 0;
  // Add first pixel as literal.
  AddSingleLiteralWithCostModel(argb, &hashers, cost_model, 0, use_color_cache,
                                0.f, cost_manager->costs_, dist_array);

  for (i = 1; i < pix_count; ++i) {
    const float prev_cost = cost_manager->costs_[i - 1];
    int offset, len;
    VP8LHashChainFindCopy(hash_chain, i, &offset, &len);

    // Try adding the pixel as a literal.
    AddSingleLiteralWithCostModel(argb, &hashers, cost_model, i,
                                  use_color_cache, prev_cost,
                                  cost_manager->costs_, dist_array);

    // If we are dealing with a non-literal.
    if (len >= 2) {
      if (offset != offset_prev) {
        const int code = VP8LDistanceToPlaneCode(xsize, offset);
        offset_cost = GetDistanceCost(cost_model, code);
        first_offset_is_constant = 1;
        PushInterval(cost_manager, prev_cost + offset_cost, i, len);
      } else {
        assert(offset_cost >= 0);
        assert(len_prev >= 0);
        assert(first_offset_is_constant == 0 || first_offset_is_constant == 1);
        // Instead of considering all contributions from a pixel i by calling:
        //         PushInterval(cost_manager, prev_cost + offset_cost, i, len);
        // we optimize these contributions in case offset_cost stays the same
        // for consecutive pixels. This describes a set of pixels similar to a
        // previous set (e.g. constant color regions).
        if (first_offset_is_constant) {
          reach = i - 1 + len_prev - 1;
          first_offset_is_constant = 0;
        }

        if (i + len - 1 > reach) {
          // We can only be go further with the same offset if the previous
          // length was maxed, hence len_prev == len == MAX_LENGTH.
          // TODO(vrabaud), bump i to the end right away (insert cache and
          // update cost).
          // TODO(vrabaud), check if one of the points in between does not have
          // a lower cost.
          // Already consider the pixel at "reach" to add intervals that are
          // better than whatever we add.
          int offset_j, len_j = 0;
          int j;
          assert(len == MAX_LENGTH || len == pix_count - i);
          // Figure out the last consecutive pixel within [i, reach + 1] with
          // the same offset.
          for (j = i; j <= reach; ++j) {
            VP8LHashChainFindCopy(hash_chain, j + 1, &offset_j, &len_j);
            if (offset_j != offset) {
              VP8LHashChainFindCopy(hash_chain, j, &offset_j, &len_j);
              break;
            }
          }
          // Update the cost at j - 1 and j.
          UpdateCostAtIndex(cost_manager, j - 1, 0);
          UpdateCostAtIndex(cost_manager, j, 0);

          PushInterval(cost_manager, cost_manager->costs_[j - 1] + offset_cost,
                       j, len_j);
          reach = j + len_j - 1;
        }
      }
    }

    UpdateCostAtIndex(cost_manager, i, 1);
    offset_prev = offset;
    len_prev = len;
  }

  ok = !refs->error_;
Error:
  if (cc_init) VP8LColorCacheClear(&hashers);
  CostManagerClear(cost_manager);
  WebPSafeFree(cost_model);
  WebPSafeFree(cost_manager);
  return ok;
}

// We pack the path at the end of *dist_array and return
// a pointer to this part of the array. Example:
// dist_array = [1x2xx3x2] => packed [1x2x1232], chosen_path = [1232]
static void TraceBackwards(uint16_t* const dist_array,
                           int dist_array_size,
                           uint16_t** const chosen_path,
                           int* const chosen_path_size) {
  uint16_t* path = dist_array + dist_array_size;
  uint16_t* cur = dist_array + dist_array_size - 1;
  while (cur >= dist_array) {
    const int k = *cur;
    --path;
    *path = k;
    cur -= k;
  }
  *chosen_path = path;
  *chosen_path_size = (int)(dist_array + dist_array_size - path);
}

static int BackwardReferencesHashChainFollowChosenPath(
    const uint32_t* const argb, int cache_bits,
    const uint16_t* const chosen_path, int chosen_path_size,
    const VP8LHashChain* const hash_chain, VP8LBackwardRefs* const refs) {
  const int use_color_cache = (cache_bits > 0);
  int ix;
  int i = 0;
  int ok = 0;
  int cc_init = 0;
  VP8LColorCache hashers;

  if (use_color_cache) {
    cc_init = VP8LColorCacheInit(&hashers, cache_bits);
    if (!cc_init) goto Error;
  }

  VP8LClearBackwardRefs(refs);
  for (ix = 0; ix < chosen_path_size; ++ix) {
    const int len = chosen_path[ix];
    if (len != 1) {
      int k;
      const int offset = VP8LHashChainFindOffset(hash_chain, i);
      VP8LBackwardRefsCursorAdd(refs, PixOrCopyCreateCopy(offset, len));
      if (use_color_cache) {
        for (k = 0; k < len; ++k) {
          VP8LColorCacheInsert(&hashers, argb[i + k]);
        }
      }
      i += len;
    } else {
      PixOrCopy v;
      const int idx =
          use_color_cache ? VP8LColorCacheContains(&hashers, argb[i]) : -1;
      if (idx >= 0) {
        // use_color_cache is true and hashers contains argb[i]
        // push pixel as a color cache index
        v = PixOrCopyCreateCacheIdx(idx);
      } else {
        if (use_color_cache) VP8LColorCacheInsert(&hashers, argb[i]);
        v = PixOrCopyCreateLiteral(argb[i]);
      }
      VP8LBackwardRefsCursorAdd(refs, v);
      ++i;
    }
  }
  ok = !refs->error_;
 Error:
  if (cc_init) VP8LColorCacheClear(&hashers);
  return ok;
}

int VP8LBackwardReferencesTraceBackwards(int xsize, int ysize,
                                         const uint32_t* const argb,
                                         int cache_bits,
                                         const VP8LHashChain* const hash_chain,
                                         const VP8LBackwardRefs* const refs_src,
                                         VP8LBackwardRefs* const refs_dst) {
  int ok = 0;
  const int dist_array_size = xsize * ysize;
  uint16_t* chosen_path = NULL;
  int chosen_path_size = 0;
  uint16_t* dist_array =
      (uint16_t*)WebPSafeMalloc(dist_array_size, sizeof(*dist_array));

  if (dist_array == NULL) goto Error;

  if (!BackwardReferencesHashChainDistanceOnly(
          xsize, ysize, argb, cache_bits, hash_chain, refs_src, dist_array)) {
    goto Error;
  }
  TraceBackwards(dist_array, dist_array_size, &chosen_path, &chosen_path_size);
  if (!BackwardReferencesHashChainFollowChosenPath(
          argb, cache_bits, chosen_path, chosen_path_size, hash_chain,
          refs_dst)) {
    goto Error;
  }
  ok = 1;
 Error:
  WebPSafeFree(dist_array);
  return ok;
}

void VP8LHashChainClear(VP8LHashChain* const p) {
  assert(p != NULL);
  WebPSafeFree(p->offset_length_);

  p->size_ = 0;
  p->offset_length_ = NULL;
}

static VP8LBackwardRefs* GetBackwardReferences(
    int width, int height, const uint32_t* const argb, int quality,
    int lz77_types_to_try, int* const cache_bits,
    const VP8LHashChain* const hash_chain, VP8LBackwardRefs* best,
    VP8LBackwardRefs* worst) {
  const int cache_bits_initial = *cache_bits;
  double bit_cost_best = -1;
  VP8LHistogram* histo = NULL;
  int lz77_type, lz77_type_best = 0;
  VP8LHashChain hash_chain_box;
  memset(&hash_chain_box, 0, sizeof(hash_chain_box));

  histo = VP8LAllocateHistogram(MAX_COLOR_CACHE_BITS);
  if (histo == NULL) goto Error;

  for (lz77_type = 1; lz77_types_to_try;
       lz77_types_to_try &= ~lz77_type, lz77_type <<= 1) {
    int res = 0;
    double bit_cost;
    int cache_bits_tmp = cache_bits_initial;
    if ((lz77_types_to_try & lz77_type) == 0) continue;
    switch (lz77_type) {
      case kLZ77RLE:
        res = BackwardReferencesRle(width, height, argb, 0, worst);
        break;
      case kLZ77Standard:
        // Compute LZ77 with no cache (0 bits), as the ideal LZ77 with a color
        // cache is not that different in practice.
        res = BackwardReferencesLz77(width, height, argb, 0, hash_chain, worst);
        break;
      case kLZ77Box:
        if (!VP8LHashChainInit(&hash_chain_box, width * height)) goto Error;
        res = BackwardReferencesLz77Box(width, height, argb, 0, hash_chain,
                                        &hash_chain_box, worst);
        break;
      default:
        assert(0);
    }
    if (!res) goto Error;

    // Next, try with a color cache and update the references.
    if (!CalculateBestCacheSize(argb, quality, worst, &cache_bits_tmp)) {
      goto Error;
    }
    if (cache_bits_tmp > 0) {
      if (!BackwardRefsWithLocalCache(argb, cache_bits_tmp, worst)) {
        goto Error;
      }
    }

    // Keep the best backward references.
    VP8LHistogramCreate(histo, worst, cache_bits_tmp);
    bit_cost = VP8LHistogramEstimateBits(histo);
    if (lz77_type_best == 0 || bit_cost < bit_cost_best) {
      VP8LBackwardRefs* const tmp = worst;
      worst = best;
      best = tmp;
      bit_cost_best = bit_cost;
      *cache_bits = cache_bits_tmp;
      lz77_type_best = lz77_type;
    }
  }
  assert(lz77_type_best > 0);

  // Improve on simple LZ77 but only for high quality (TraceBackwards is
  // costly).
  if ((lz77_type_best == kLZ77Standard || lz77_type_best == kLZ77Box) &&
      quality >= 25) {
    const VP8LHashChain* const hash_chain_tmp =
        (lz77_type_best == kLZ77Standard) ? hash_chain : &hash_chain_box;
    if (VP8LBackwardReferencesTraceBackwards(width, height, argb, *cache_bits,
                                             hash_chain_tmp, best, worst)) {
      double bit_cost_trace;
      VP8LHistogramCreate(histo, worst, *cache_bits);
      bit_cost_trace = VP8LHistogramEstimateBits(histo);
      if (bit_cost_trace < bit_cost_best) best = worst;
    }
  }

  BackwardReferences2DLocality(width, best);

Error:
  VP8LHashChainClear(&hash_chain_box);
  VP8LFreeHistogram(histo);
  return best;
}

VP8LBackwardRefs* VP8LGetBackwardReferences(
    int width, int height, const uint32_t* const argb, int quality,
    int low_effort, int lz77_types_to_try, int* const cache_bits,
    const VP8LHashChain* const hash_chain, VP8LBackwardRefs* const refs_tmp1,
    VP8LBackwardRefs* const refs_tmp2) {
  if (low_effort) {
    return GetBackwardReferencesLowEffort(width, height, argb, cache_bits,
                                          hash_chain, refs_tmp1);
  } else {
    return GetBackwardReferences(width, height, argb, quality,
                                 lz77_types_to_try, cache_bits, hash_chain,
                                 refs_tmp1, refs_tmp2);
  }
}

VP8LHistogramSet* VP8LAllocateHistogramSet(int size, int cache_bits) {
  int i;
  VP8LHistogramSet* set;
  const int histo_size = VP8LGetHistogramSize(cache_bits);
  const size_t total_size =
      sizeof(*set) + size * (sizeof(*set->histograms) +
      histo_size + WEBP_ALIGN_CST);
  uint8_t* memory = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*memory));
  if (memory == NULL) return NULL;

  set = (VP8LHistogramSet*)memory;
  memory += sizeof(*set);
  set->histograms = (VP8LHistogram**)memory;
  memory += size * sizeof(*set->histograms);
  set->max_size = size;
  set->size = size;
  for (i = 0; i < size; ++i) {
    memory = (uint8_t*)WEBP_ALIGN(memory);
    set->histograms[i] = (VP8LHistogram*)memory;
    // literal_ won't necessary be aligned.
    set->histograms[i]->literal_ = (uint32_t*)(memory + sizeof(VP8LHistogram));
    VP8LHistogramInit(set->histograms[i], cache_bits);
    memory += histo_size;
  }
  return set;
}

// Heuristics for selecting the stride ranges to collapse.
static int ValuesShouldBeCollapsedToStrideAverage(int a, int b) {
  return abs(a - b) < 4;
}

// Change the population counts in a way that the consequent
// Huffman tree compression, especially its RLE-part, give smaller output.
static void OptimizeHuffmanForRle(int length, uint8_t* const good_for_rle,
                                  uint32_t* const counts) {
  // 1) Let's make the Huffman code more compatible with rle encoding.
  int i;
  for (; length >= 0; --length) {
    if (length == 0) {
      return;  // All zeros.
    }
    if (counts[length - 1] != 0) {
      // Now counts[0..length - 1] does not have trailing zeros.
      break;
    }
  }
  // 2) Let's mark all population counts that already can be encoded
  // with an rle code.
  {
    // Let's not spoil any of the existing good rle codes.
    // Mark any seq of 0's that is longer as 5 as a good_for_rle.
    // Mark any seq of non-0's that is longer as 7 as a good_for_rle.
    uint32_t symbol = counts[0];
    int stride = 0;
    for (i = 0; i < length + 1; ++i) {
      if (i == length || counts[i] != symbol) {
        if ((symbol == 0 && stride >= 5) ||
            (symbol != 0 && stride >= 7)) {
          int k;
          for (k = 0; k < stride; ++k) {
            good_for_rle[i - k - 1] = 1;
          }
        }
        stride = 1;
        if (i != length) {
          symbol = counts[i];
        }
      } else {
        ++stride;
      }
    }
  }
  // 3) Let's replace those population counts that lead to more rle codes.
  {
    uint32_t stride = 0;
    uint32_t limit = counts[0];
    uint32_t sum = 0;
    for (i = 0; i < length + 1; ++i) {
      if (i == length || good_for_rle[i] ||
          (i != 0 && good_for_rle[i - 1]) ||
          !ValuesShouldBeCollapsedToStrideAverage(counts[i], limit)) {
        if (stride >= 4 || (stride >= 3 && sum == 0)) {
          uint32_t k;
          // The stride must end, collapse what we have, if we have enough (4).
          uint32_t count = (sum + stride / 2) / stride;
          if (count < 1) {
            count = 1;
          }
          if (sum == 0) {
            // Don't make an all zeros stride to be upgraded to ones.
            count = 0;
          }
          for (k = 0; k < stride; ++k) {
            // We don't want to change value at counts[i],
            // that is already belonging to the next stride. Thus - 1.
            counts[i - k - 1] = count;
          }
        }
        stride = 0;
        sum = 0;
        if (i < length - 3) {
          // All interesting strides have a count of at least 4,
          // at least when non-zeros.
          limit = (counts[i] + counts[i + 1] +
                   counts[i + 2] + counts[i + 3] + 2) / 4;
        } else if (i < length) {
          limit = counts[i];
        } else {
          limit = 0;
        }
      }
      ++stride;
      if (i != length) {
        sum += counts[i];
        if (stride >= 4) {
          limit = (sum + stride / 2) / stride;
        }
      }
    }
  }
}

// A comparer function for two Huffman trees: sorts first by 'total count'
// (more comes first), and then by 'value' (more comes first).
static int CompareHuffmanTrees(const void* ptr1, const void* ptr2) {
  const HuffmanTree* const t1 = (const HuffmanTree*)ptr1;
  const HuffmanTree* const t2 = (const HuffmanTree*)ptr2;
  if (t1->total_count_ > t2->total_count_) {
    return -1;
  } else if (t1->total_count_ < t2->total_count_) {
    return 1;
  } else {
    assert(t1->value_ != t2->value_);
    return (t1->value_ < t2->value_) ? -1 : 1;
  }
}

static void SetBitDepths(const HuffmanTree* const tree,
                         const HuffmanTree* const pool,
                         uint8_t* const bit_depths, int level) {
  if (tree->pool_index_left_ >= 0) {
    SetBitDepths(&pool[tree->pool_index_left_], pool, bit_depths, level + 1);
    SetBitDepths(&pool[tree->pool_index_right_], pool, bit_depths, level + 1);
  } else {
    bit_depths[tree->value_] = level;
  }
}

// Create an optimal Huffman tree.
//
// (data,length): population counts.
// tree_limit: maximum bit depth (inclusive) of the codes.
// bit_depths[]: how many bits are used for the symbol.
//
// Returns 0 when an error has occurred.
//
// The catch here is that the tree cannot be arbitrarily deep
//
// count_limit is the value that is to be faked as the minimum value
// and this minimum value is raised until the tree matches the
// maximum length requirement.
//
// This algorithm is not of excellent performance for very long data blocks,
// especially when population counts are longer than 2**tree_limit, but
// we are not planning to use this with extremely long blocks.
//
// See http://en.wikipedia.org/wiki/Huffman_coding
static void GenerateOptimalTree(const uint32_t* const histogram,
                                int histogram_size,
                                HuffmanTree* tree, int tree_depth_limit,
                                uint8_t* const bit_depths) {
  uint32_t count_min;
  HuffmanTree* tree_pool;
  int tree_size_orig = 0;
  int i;

  for (i = 0; i < histogram_size; ++i) {
    if (histogram[i] != 0) {
      ++tree_size_orig;
    }
  }

  if (tree_size_orig == 0) {   // pretty optimal already!
    return;
  }

  tree_pool = tree + tree_size_orig;

  // For block sizes with less than 64k symbols we never need to do a
  // second iteration of this loop.
  // If we actually start running inside this loop a lot, we would perhaps
  // be better off with the Katajainen algorithm.
  assert(tree_size_orig <= (1 << (tree_depth_limit - 1)));
  for (count_min = 1; ; count_min *= 2) {
    int tree_size = tree_size_orig;
    // We need to pack the Huffman tree in tree_depth_limit bits.
    // So, we try by faking histogram entries to be at least 'count_min'.
    int idx = 0;
    int j;
    for (j = 0; j < histogram_size; ++j) {
      if (histogram[j] != 0) {
        const uint32_t count =
            (histogram[j] < count_min) ? count_min : histogram[j];
        tree[idx].total_count_ = count;
        tree[idx].value_ = j;
        tree[idx].pool_index_left_ = -1;
        tree[idx].pool_index_right_ = -1;
        ++idx;
      }
    }

    // Build the Huffman tree.
    qsort(tree, tree_size, sizeof(*tree), CompareHuffmanTrees);

    if (tree_size > 1) {  // Normal case.
      int tree_pool_size = 0;
      while (tree_size > 1) {  // Finish when we have only one root.
        uint32_t count;
        tree_pool[tree_pool_size++] = tree[tree_size - 1];
        tree_pool[tree_pool_size++] = tree[tree_size - 2];
        count = tree_pool[tree_pool_size - 1].total_count_ +
                tree_pool[tree_pool_size - 2].total_count_;
        tree_size -= 2;
        {
          // Search for the insertion point.
          int k;
          for (k = 0; k < tree_size; ++k) {
            if (tree[k].total_count_ <= count) {
              break;
            }
          }
          memmove(tree + (k + 1), tree + k, (tree_size - k) * sizeof(*tree));
          tree[k].total_count_ = count;
          tree[k].value_ = -1;

          tree[k].pool_index_left_ = tree_pool_size - 1;
          tree[k].pool_index_right_ = tree_pool_size - 2;
          tree_size = tree_size + 1;
        }
      }
      SetBitDepths(&tree[0], tree_pool, bit_depths, 0);
    } else if (tree_size == 1) {  // Trivial case: only one element.
      bit_depths[tree[0].value_] = 1;
    }

    {
      // Test if this Huffman tree satisfies our 'tree_depth_limit' criteria.
      int max_depth = bit_depths[0];
      for (j = 1; j < histogram_size; ++j) {
        if (max_depth < bit_depths[j]) {
          max_depth = bit_depths[j];
        }
      }
      if (max_depth <= tree_depth_limit) {
        break;
      }
    }
  }
}

#define MAX_ALLOWED_CODE_LENGTH      15

// Pre-reversed 4-bit values.
static const uint8_t kReversedBits[16] = {
  0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
  0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
};

static uint32_t ReverseBits(int num_bits, uint32_t bits) {
  uint32_t retval = 0;
  int i = 0;
  while (i < num_bits) {
    i += 4;
    retval |= kReversedBits[bits & 0xf] << (MAX_ALLOWED_CODE_LENGTH + 1 - i);
    bits >>= 4;
  }
  retval >>= (MAX_ALLOWED_CODE_LENGTH + 1 - num_bits);
  return retval;
}

// Get the actual bit values for a tree of bit depths.
static void ConvertBitDepthsToSymbols(HuffmanTreeCode* const tree) {
  // 0 bit-depth means that the symbol does not exist.
  int i;
  int len;
  uint32_t next_code[MAX_ALLOWED_CODE_LENGTH + 1];
  int depth_count[MAX_ALLOWED_CODE_LENGTH + 1] = { 0 };

  assert(tree != NULL);
  len = tree->num_symbols;
  for (i = 0; i < len; ++i) {
    const int code_length = tree->code_lengths[i];
    assert(code_length <= MAX_ALLOWED_CODE_LENGTH);
    ++depth_count[code_length];
  }
  depth_count[0] = 0;  // ignore unused symbol
  next_code[0] = 0;
  {
    uint32_t code = 0;
    for (i = 1; i <= MAX_ALLOWED_CODE_LENGTH; ++i) {
      code = (code + depth_count[i - 1]) << 1;
      next_code[i] = code;
    }
  }
  for (i = 0; i < len; ++i) {
    const int code_length = tree->code_lengths[i];
    tree->codes[i] = ReverseBits(code_length, next_code[code_length]++);
  }
}

void VP8LCreateHuffmanTree(uint32_t* const histogram, int tree_depth_limit,
                           uint8_t* const buf_rle,
                           HuffmanTree* const huff_tree,
                           HuffmanTreeCode* const huff_code) {
  const int num_symbols = huff_code->num_symbols;
  memset(buf_rle, 0, num_symbols * sizeof(*buf_rle));
  OptimizeHuffmanForRle(num_symbols, buf_rle, histogram);
  GenerateOptimalTree(histogram, num_symbols, huff_tree, tree_depth_limit,
                      huff_code->code_lengths);
  // Create the actual bit codes for the bit lengths.
  ConvertBitDepthsToSymbols(huff_code);
}

// Returns false in case of memory error.
static int GetHuffBitLengthsAndCodes(
    const VP8LHistogramSet* const histogram_image,
    HuffmanTreeCode* const huffman_codes) {
  int i, k;
  int ok = 0;
  uint64_t total_length_size = 0;
  uint8_t* mem_buf = NULL;
  const int histogram_image_size = histogram_image->size;
  int max_num_symbols = 0;
  uint8_t* buf_rle = NULL;
  HuffmanTree* huff_tree = NULL;

  // Iterate over all histograms and get the aggregate number of codes used.
  for (i = 0; i < histogram_image_size; ++i) {
    const VP8LHistogram* const histo = histogram_image->histograms[i];
    HuffmanTreeCode* const codes = &huffman_codes[5 * i];
    for (k = 0; k < 5; ++k) {
      const int num_symbols =
          (k == 0) ? VP8LHistogramNumCodes(histo->palette_code_bits_) :
          (k == 4) ? NUM_DISTANCE_CODES : 256;
      codes[k].num_symbols = num_symbols;
      total_length_size += num_symbols;
    }
  }

  // Allocate and Set Huffman codes.
  {
    uint16_t* codes;
    uint8_t* lengths;
    mem_buf = (uint8_t*)WebPSafeCalloc(total_length_size,
                                       sizeof(*lengths) + sizeof(*codes));
    if (mem_buf == NULL) goto End;

    codes = (uint16_t*)mem_buf;
    lengths = (uint8_t*)&codes[total_length_size];
    for (i = 0; i < 5 * histogram_image_size; ++i) {
      const int bit_length = huffman_codes[i].num_symbols;
      huffman_codes[i].codes = codes;
      huffman_codes[i].code_lengths = lengths;
      codes += bit_length;
      lengths += bit_length;
      if (max_num_symbols < bit_length) {
        max_num_symbols = bit_length;
      }
    }
  }

  buf_rle = (uint8_t*)WebPSafeMalloc(1ULL, max_num_symbols);
  huff_tree = (HuffmanTree*)WebPSafeMalloc(3ULL * max_num_symbols,
                                           sizeof(*huff_tree));
  if (buf_rle == NULL || huff_tree == NULL) goto End;

  // Create Huffman trees.
  for (i = 0; i < histogram_image_size; ++i) {
    HuffmanTreeCode* const codes = &huffman_codes[5 * i];
    VP8LHistogram* const histo = histogram_image->histograms[i];
    VP8LCreateHuffmanTree(histo->literal_, 15, buf_rle, huff_tree, codes + 0);
    VP8LCreateHuffmanTree(histo->red_, 15, buf_rle, huff_tree, codes + 1);
    VP8LCreateHuffmanTree(histo->blue_, 15, buf_rle, huff_tree, codes + 2);
    VP8LCreateHuffmanTree(histo->alpha_, 15, buf_rle, huff_tree, codes + 3);
    VP8LCreateHuffmanTree(histo->distance_, 15, buf_rle, huff_tree, codes + 4);
  }
  ok = 1;
 End:
  WebPSafeFree(huff_tree);
  WebPSafeFree(buf_rle);
  if (!ok) {
    WebPSafeFree(mem_buf);
    memset(huffman_codes, 0, 5 * histogram_image_size * sizeof(*huffman_codes));
  }
  return ok;
}

// -----------------------------------------------------------------------------
// Coding of the Huffman tree values

static HuffmanTreeToken* CodeRepeatedValues(int repetitions,
                                            HuffmanTreeToken* tokens,
                                            int value, int prev_value) {
  assert(value <= MAX_ALLOWED_CODE_LENGTH);
  if (value != prev_value) {
    tokens->code = value;
    tokens->extra_bits = 0;
    ++tokens;
    --repetitions;
  }
  while (repetitions >= 1) {
    if (repetitions < 3) {
      int i;
      for (i = 0; i < repetitions; ++i) {
        tokens->code = value;
        tokens->extra_bits = 0;
        ++tokens;
      }
      break;
    } else if (repetitions < 7) {
      tokens->code = 16;
      tokens->extra_bits = repetitions - 3;
      ++tokens;
      break;
    } else {
      tokens->code = 16;
      tokens->extra_bits = 3;
      ++tokens;
      repetitions -= 6;
    }
  }
  return tokens;
}

static HuffmanTreeToken* CodeRepeatedZeros(int repetitions,
                                           HuffmanTreeToken* tokens) {
  while (repetitions >= 1) {
    if (repetitions < 3) {
      int i;
      for (i = 0; i < repetitions; ++i) {
        tokens->code = 0;   // 0-value
        tokens->extra_bits = 0;
        ++tokens;
      }
      break;
    } else if (repetitions < 11) {
      tokens->code = 17;
      tokens->extra_bits = repetitions - 3;
      ++tokens;
      break;
    } else if (repetitions < 139) {
      tokens->code = 18;
      tokens->extra_bits = repetitions - 11;
      ++tokens;
      break;
    } else {
      tokens->code = 18;
      tokens->extra_bits = 0x7f;  // 138 repeated 0s
      ++tokens;
      repetitions -= 138;
    }
  }
  return tokens;
}

int VP8LCreateCompressedHuffmanTree(const HuffmanTreeCode* const tree,
                                    HuffmanTreeToken* tokens, int max_tokens) {
  HuffmanTreeToken* const starting_token = tokens;
  HuffmanTreeToken* const ending_token = tokens + max_tokens;
  const int depth_size = tree->num_symbols;
  int prev_value = 8;  // 8 is the initial value for rle.
  int i = 0;
  assert(tokens != NULL);
  while (i < depth_size) {
    const int value = tree->code_lengths[i];
    int k = i + 1;
    int runs;
    while (k < depth_size && tree->code_lengths[k] == value) ++k;
    runs = k - i;
    if (value == 0) {
      tokens = CodeRepeatedZeros(runs, tokens);
    } else {
      tokens = CodeRepeatedValues(runs, tokens, value, prev_value);
      prev_value = value;
    }
    i += runs;
    assert(tokens <= ending_token);
  }
  (void)ending_token;    // suppress 'unused variable' warning
  return (int)(tokens - starting_token);
}

static void StoreHuffmanTreeOfHuffmanTreeToBitMask(
    VP8LBitWriter* const bw, const uint8_t* code_length_bitdepth) {
  // RFC 1951 will calm you down if you are worried about this funny sequence.
  // This sequence is tuned from that, but more weighted for lower symbol count,
  // and more spiking histograms.
  static const uint8_t kStorageOrder[CODE_LENGTH_CODES] = {
    17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  };
  int i;
  // Throw away trailing zeros:
  int codes_to_store = CODE_LENGTH_CODES;
  for (; codes_to_store > 4; --codes_to_store) {
    if (code_length_bitdepth[kStorageOrder[codes_to_store - 1]] != 0) {
      break;
    }
  }
  VP8LPutBits(bw, codes_to_store - 4, 4);
  for (i = 0; i < codes_to_store; ++i) {
    VP8LPutBits(bw, code_length_bitdepth[kStorageOrder[i]], 3);
  }
}

static void ClearHuffmanTreeIfOnlyOneSymbol(
    HuffmanTreeCode* const huffman_code) {
  int k;
  int count = 0;
  for (k = 0; k < huffman_code->num_symbols; ++k) {
    if (huffman_code->code_lengths[k] != 0) {
      ++count;
      if (count > 1) return;
    }
  }
  for (k = 0; k < huffman_code->num_symbols; ++k) {
    huffman_code->code_lengths[k] = 0;
    huffman_code->codes[k] = 0;
  }
}

static void StoreHuffmanTreeToBitMask(
    VP8LBitWriter* const bw,
    const HuffmanTreeToken* const tokens, const int num_tokens,
    const HuffmanTreeCode* const huffman_code) {
  int i;
  for (i = 0; i < num_tokens; ++i) {
    const int ix = tokens[i].code;
    const int extra_bits = tokens[i].extra_bits;
    VP8LPutBits(bw, huffman_code->codes[ix], huffman_code->code_lengths[ix]);
    switch (ix) {
      case 16:
        VP8LPutBits(bw, extra_bits, 2);
        break;
      case 17:
        VP8LPutBits(bw, extra_bits, 3);
        break;
      case 18:
        VP8LPutBits(bw, extra_bits, 7);
        break;
    }
  }
}

// 'huff_tree' and 'tokens' are pre-alloacted buffers.
static void StoreFullHuffmanCode(VP8LBitWriter* const bw,
                                 HuffmanTree* const huff_tree,
                                 HuffmanTreeToken* const tokens,
                                 const HuffmanTreeCode* const tree) {
  uint8_t code_length_bitdepth[CODE_LENGTH_CODES] = { 0 };
  uint16_t code_length_bitdepth_symbols[CODE_LENGTH_CODES] = { 0 };
  const int max_tokens = tree->num_symbols;
  int num_tokens;
  HuffmanTreeCode huffman_code;
  huffman_code.num_symbols = CODE_LENGTH_CODES;
  huffman_code.code_lengths = code_length_bitdepth;
  huffman_code.codes = code_length_bitdepth_symbols;

  VP8LPutBits(bw, 0, 1);
  num_tokens = VP8LCreateCompressedHuffmanTree(tree, tokens, max_tokens);
  {
    uint32_t histogram[CODE_LENGTH_CODES] = { 0 };
    uint8_t buf_rle[CODE_LENGTH_CODES] = { 0 };
    int i;
    for (i = 0; i < num_tokens; ++i) {
      ++histogram[tokens[i].code];
    }

    VP8LCreateHuffmanTree(histogram, 7, buf_rle, huff_tree, &huffman_code);
  }

  StoreHuffmanTreeOfHuffmanTreeToBitMask(bw, code_length_bitdepth);
  ClearHuffmanTreeIfOnlyOneSymbol(&huffman_code);
  {
    int trailing_zero_bits = 0;
    int trimmed_length = num_tokens;
    int write_trimmed_length;
    int length;
    int i = num_tokens;
    while (i-- > 0) {
      const int ix = tokens[i].code;
      if (ix == 0 || ix == 17 || ix == 18) {
        --trimmed_length;   // discount trailing zeros
        trailing_zero_bits += code_length_bitdepth[ix];
        if (ix == 17) {
          trailing_zero_bits += 3;
        } else if (ix == 18) {
          trailing_zero_bits += 7;
        }
      } else {
        break;
      }
    }
    write_trimmed_length = (trimmed_length > 1 && trailing_zero_bits > 12);
    length = write_trimmed_length ? trimmed_length : num_tokens;
    VP8LPutBits(bw, write_trimmed_length, 1);
    if (write_trimmed_length) {
      if (trimmed_length == 2) {
        VP8LPutBits(bw, 0, 3 + 2);     // nbitpairs=1, trimmed_length=2
      } else {
        const int nbits = BitsLog2Floor(trimmed_length - 2);
        const int nbitpairs = nbits / 2 + 1;
        assert(trimmed_length > 2);
        assert(nbitpairs - 1 < 8);
        VP8LPutBits(bw, nbitpairs - 1, 3);
        VP8LPutBits(bw, trimmed_length - 2, nbitpairs * 2);
      }
    }
    StoreHuffmanTreeToBitMask(bw, tokens, length, &huffman_code);
  }
}

// 'huff_tree' and 'tokens' are pre-alloacted buffers.
static void StoreHuffmanCode(VP8LBitWriter* const bw,
                             HuffmanTree* const huff_tree,
                             HuffmanTreeToken* const tokens,
                             const HuffmanTreeCode* const huffman_code) {
  int i;
  int count = 0;
  int symbols[2] = { 0, 0 };
  const int kMaxBits = 8;
  const int kMaxSymbol = 1 << kMaxBits;

  // Check whether it's a small tree.
  for (i = 0; i < huffman_code->num_symbols && count < 3; ++i) {
    if (huffman_code->code_lengths[i] != 0) {
      if (count < 2) symbols[count] = i;
      ++count;
    }
  }

  if (count == 0) {   // emit minimal tree for empty cases
    // bits: small tree marker: 1, count-1: 0, large 8-bit code: 0, code: 0
    VP8LPutBits(bw, 0x01, 4);
  } else if (count <= 2 && symbols[0] < kMaxSymbol && symbols[1] < kMaxSymbol) {
    VP8LPutBits(bw, 1, 1);  // Small tree marker to encode 1 or 2 symbols.
    VP8LPutBits(bw, count - 1, 1);
    if (symbols[0] <= 1) {
      VP8LPutBits(bw, 0, 1);  // Code bit for small (1 bit) symbol value.
      VP8LPutBits(bw, symbols[0], 1);
    } else {
      VP8LPutBits(bw, 1, 1);
      VP8LPutBits(bw, symbols[0], 8);
    }
    if (count == 2) {
      VP8LPutBits(bw, symbols[1], 8);
    }
  } else {
    StoreFullHuffmanCode(bw, huff_tree, tokens, huffman_code);
  }
}

void VP8LFreeHistogramSet(VP8LHistogramSet* const histo) {
  WebPSafeFree(histo);
}

static void WriteHuffmanCode(VP8LBitWriter* const bw,
                             const HuffmanTreeCode* const code,
                             int code_index) {
  const int depth = code->code_lengths[code_index];
  const int symbol = code->codes[code_index];
  VP8LPutBits(bw, symbol, depth);
}

const uint8_t kPrefixEncodeExtraBitsValue[PREFIX_LOOKUP_IDX_MAX] = {
   0,  0,  0,  0,  0,  0,  1,  0,  1,  0,  1,  2,  3,  0,  1,  2,  3,
   0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
  127,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126
};

static void VP8LPrefixEncodeNoLUT(int distance, int* const code,
                                              int* const extra_bits,
                                              int* const extra_bits_value) {
  const int highest_bit = BitsLog2Floor(--distance);
  const int second_highest_bit = (distance >> (highest_bit - 1)) & 1;
  *extra_bits = highest_bit - 1;
  *extra_bits_value = distance & ((1 << *extra_bits) - 1);
  *code = 2 * highest_bit + second_highest_bit;
}

static void VP8LPrefixEncode(int distance, int* const code,
                                         int* const extra_bits,
                                         int* const extra_bits_value) {
  if (distance < PREFIX_LOOKUP_IDX_MAX) {
    const VP8LPrefixCode prefix_code = kPrefixEncodeCode[distance];
    *code = prefix_code.code_;
    *extra_bits = prefix_code.extra_bits_;
    *extra_bits_value = kPrefixEncodeExtraBitsValue[distance];
  } else {
    VP8LPrefixEncodeNoLUT(distance, code, extra_bits, extra_bits_value);
  }
}

static void WriteHuffmanCodeWithExtraBits(
    VP8LBitWriter* const bw,
    const HuffmanTreeCode* const code,
    int code_index,
    int bits,
    int n_bits) {
  const int depth = code->code_lengths[code_index];
  const int symbol = code->codes[code_index];
  VP8LPutBits(bw, (bits << depth) | symbol, depth + n_bits);
}

static WebPEncodingError StoreImageToBitMask(
    VP8LBitWriter* const bw, int width, int histo_bits,
    const VP8LBackwardRefs* const refs,
    const uint16_t* histogram_symbols,
    const HuffmanTreeCode* const huffman_codes) {
  const int histo_xsize = histo_bits ? VP8LSubSampleSize(width, histo_bits) : 1;
  const int tile_mask = (histo_bits == 0) ? 0 : -(1 << histo_bits);
  // x and y trace the position in the image.
  int x = 0;
  int y = 0;
  int tile_x = x & tile_mask;
  int tile_y = y & tile_mask;
  int histogram_ix = histogram_symbols[0];
  const HuffmanTreeCode* codes = huffman_codes + 5 * histogram_ix;
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  while (VP8LRefsCursorOk(&c)) {
    const PixOrCopy* const v = c.cur_pos;
    if ((tile_x != (x & tile_mask)) || (tile_y != (y & tile_mask))) {
      tile_x = x & tile_mask;
      tile_y = y & tile_mask;
      histogram_ix = histogram_symbols[(y >> histo_bits) * histo_xsize +
                                       (x >> histo_bits)];
      codes = huffman_codes + 5 * histogram_ix;
    }
    if (PixOrCopyIsLiteral(v)) {
      static const uint8_t order[] = { 1, 2, 0, 3 };
      int k;
      for (k = 0; k < 4; ++k) {
        const int code = PixOrCopyLiteral(v, order[k]);
        WriteHuffmanCode(bw, codes + k, code);
      }
    } else if (PixOrCopyIsCacheIdx(v)) {
      const int code = PixOrCopyCacheIdx(v);
      const int literal_ix = 256 + NUM_LENGTH_CODES + code;
      WriteHuffmanCode(bw, codes, literal_ix);
    } else {
      int bits, n_bits;
      int code;

      const int distance = PixOrCopyDistance(v);
      VP8LPrefixEncode(v->len, &code, &n_bits, &bits);
      WriteHuffmanCodeWithExtraBits(bw, codes, 256 + code, bits, n_bits);

      // Don't write the distance with the extra bits code since
      // the distance can be up to 18 bits of extra bits, and the prefix
      // 15 bits, totaling to 33, and our PutBits only supports up to 32 bits.
      VP8LPrefixEncode(distance, &code, &n_bits, &bits);
      WriteHuffmanCode(bw, codes + 4, code);
      VP8LPutBits(bw, bits, n_bits);
    }
    x += PixOrCopyLength(v);
    while (x >= width) {
      x -= width;
      ++y;
    }
    VP8LRefsCursorNext(&c);
  }
  return bw->error_ ? VP8_ENC_ERROR_OUT_OF_MEMORY : VP8_ENC_OK;
}

// Special case of EncodeImageInternal() for cache-bits=0, histo_bits=31
static WebPEncodingError EncodeImageNoHuffman(VP8LBitWriter* const bw,
                                              const uint32_t* const argb,
                                              VP8LHashChain* const hash_chain,
                                              VP8LBackwardRefs* const refs_tmp1,
                                              VP8LBackwardRefs* const refs_tmp2,
                                              int width, int height,
                                              int quality, int low_effort) {
  int i;
  int max_tokens = 0;
  WebPEncodingError err = VP8_ENC_OK;
  VP8LBackwardRefs* refs;
  HuffmanTreeToken* tokens = NULL;
  HuffmanTreeCode huffman_codes[5] = { { 0, NULL, NULL } };
  const uint16_t histogram_symbols[1] = { 0 };    // only one tree, one symbol
  int cache_bits = 0;
  VP8LHistogramSet* histogram_image = NULL;
  HuffmanTree* const huff_tree = (HuffmanTree*)WebPSafeMalloc(
        3ULL * CODE_LENGTH_CODES, sizeof(*huff_tree));
  if (huff_tree == NULL) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  // Calculate backward references from ARGB image.
  if (!VP8LHashChainFill(hash_chain, quality, argb, width, height,
                         low_effort)) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }
  refs = VP8LGetBackwardReferences(width, height, argb, quality, 0,
                                   kLZ77Standard | kLZ77RLE, &cache_bits,
                                   hash_chain, refs_tmp1, refs_tmp2);
  if (refs == NULL) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }
  histogram_image = VP8LAllocateHistogramSet(1, cache_bits);
  if (histogram_image == NULL) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  // Build histogram image and symbols from backward references.
  VP8LHistogramStoreRefs(refs, histogram_image->histograms[0]);

  // Create Huffman bit lengths and codes for each histogram image.
  assert(histogram_image->size == 1);
  if (!GetHuffBitLengthsAndCodes(histogram_image, huffman_codes)) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  // No color cache, no Huffman image.
  VP8LPutBits(bw, 0, 1);

  // Find maximum number of symbols for the huffman tree-set.
  for (i = 0; i < 5; ++i) {
    HuffmanTreeCode* const codes = &huffman_codes[i];
    if (max_tokens < codes->num_symbols) {
      max_tokens = codes->num_symbols;
    }
  }

  tokens = (HuffmanTreeToken*)WebPSafeMalloc(max_tokens, sizeof(*tokens));
  if (tokens == NULL) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  // Store Huffman codes.
  for (i = 0; i < 5; ++i) {
    HuffmanTreeCode* const codes = &huffman_codes[i];
    StoreHuffmanCode(bw, huff_tree, tokens, codes);
    ClearHuffmanTreeIfOnlyOneSymbol(codes);
  }

  // Store actual literals.
  err = StoreImageToBitMask(bw, width, 0, refs, histogram_symbols,
                            huffman_codes);

 Error:
  WebPSafeFree(tokens);
  WebPSafeFree(huff_tree);
  VP8LFreeHistogramSet(histogram_image);
  WebPSafeFree(huffman_codes[0].codes);
  return err;
}

// Save palette_[] to bitstream.
static WebPEncodingError EncodePalette(VP8LBitWriter* const bw, int low_effort,
                                       VP8LEncoder* const enc) {
  int i;
  uint32_t tmp_palette[MAX_PALETTE_SIZE];
  const int palette_size = enc->palette_size_;
  const uint32_t* const palette = enc->palette_;
  VP8LPutBits(bw, TRANSFORM_PRESENT, 1);
  VP8LPutBits(bw, COLOR_INDEXING_TRANSFORM, 2);
  assert(palette_size >= 1 && palette_size <= MAX_PALETTE_SIZE);
  VP8LPutBits(bw, palette_size - 1, 8);
  for (i = palette_size - 1; i >= 1; --i) {
    tmp_palette[i] = VP8LSubPixels(palette[i], palette[i - 1]);
  }
  tmp_palette[0] = palette[0];
  return EncodeImageNoHuffman(bw, tmp_palette, &enc->hash_chain_,
                              &enc->refs_[0], &enc->refs_[1], palette_size, 1,
                              20 /* quality */, low_effort);
}

static int SearchColorNoIdx(const uint32_t sorted[], uint32_t color,
                                        int hi) {
  int low = 0;
  if (sorted[low] == color) return low;  // loop invariant: sorted[low] != color
  while (1) {
    const int mid = (low + hi) >> 1;
    if (sorted[mid] == color) {
      return mid;
    } else if (sorted[mid] < color) {
      low = mid;
    } else {
      hi = mid;
    }
  }
}

#define APPLY_PALETTE_GREEDY_MAX 4

static uint32_t SearchColorGreedy(const uint32_t palette[],
                                              int palette_size,
                                              uint32_t color) {
  (void)palette_size;
  assert(palette_size < APPLY_PALETTE_GREEDY_MAX);
  assert(3 == APPLY_PALETTE_GREEDY_MAX - 1);
  if (color == palette[0]) return 0;
  if (color == palette[1]) return 1;
  if (color == palette[2]) return 2;
  return 3;
}

static uint32_t ApplyPaletteHash0(uint32_t color) {
  // Focus on the green color.
  return (color >> 8) & 0xff;
}

#define PALETTE_INV_SIZE_BITS 11
#define PALETTE_INV_SIZE (1 << PALETTE_INV_SIZE_BITS)

static uint32_t ApplyPaletteHash1(uint32_t color) {
  // Forget about alpha.
  return ((uint32_t)((color & 0x00ffffffu) * 4222244071ull)) >>
         (32 - PALETTE_INV_SIZE_BITS);
}

static uint32_t ApplyPaletteHash2(uint32_t color) {
  // Forget about alpha.
  return ((uint32_t)((color & 0x00ffffffu) * ((1ull << 31) - 1))) >>
         (32 - PALETTE_INV_SIZE_BITS);
}

// Sort palette in increasing order and prepare an inverse mapping array.
static void PrepareMapToPalette(const uint32_t palette[], int num_colors,
                                uint32_t sorted[], uint32_t idx_map[]) {
  int i;
  memcpy(sorted, palette, num_colors * sizeof(*sorted));
  qsort(sorted, num_colors, sizeof(*sorted), PaletteCompareColorsForQsort);
  for (i = 0; i < num_colors; ++i) {
    idx_map[SearchColorNoIdx(sorted, palette[i], num_colors)] = i;
  }
}

// Bundles multiple (1, 2, 4 or 8) pixels into a single pixel.
void VP8LBundleColorMap_C(const uint8_t* const row, int width, int xbits,
                          uint32_t* dst) {
  int x;
  if (xbits > 0) {
    const int bit_depth = 1 << (3 - xbits);
    const int mask = (1 << xbits) - 1;
    uint32_t code = 0xff000000;
    for (x = 0; x < width; ++x) {
      const int xsub = x & mask;
      if (xsub == 0) {
        code = 0xff000000;
      }
      code |= row[x] << (8 + bit_depth * xsub);
      dst[x >> xbits] = code;
    }
  } else {
    for (x = 0; x < width; ++x) dst[x] = 0xff000000 | (row[x] << 8);
  }
}

// Use 1 pixel cache for ARGB pixels.
#define APPLY_PALETTE_FOR(COLOR_INDEX) do {         \
  uint32_t prev_pix = palette[0];                   \
  uint32_t prev_idx = 0;                            \
  for (y = 0; y < height; ++y) {                    \
    for (x = 0; x < width; ++x) {                   \
      const uint32_t pix = src[x];                  \
      if (pix != prev_pix) {                        \
        prev_idx = COLOR_INDEX;                     \
        prev_pix = pix;                             \
      }                                             \
      tmp_row[x] = prev_idx;                        \
    }                                               \
	VP8LBundleColorMap_C(tmp_row, width, xbits, dst); \
    src += src_stride;                              \
    dst += dst_stride;                              \
  }                                                 \
} while (0)

// Remap argb values in src[] to packed palettes entries in dst[]
// using 'row' as a temporary buffer of size 'width'.
// We assume that all src[] values have a corresponding entry in the palette.
// Note: src[] can be the same as dst[]
static WebPEncodingError ApplyPalette(const uint32_t* src, uint32_t src_stride,
                                      uint32_t* dst, uint32_t dst_stride,
                                      const uint32_t* palette, int palette_size,
                                      int width, int height, int xbits) {
  // TODO(skal): this tmp buffer is not needed if VP8LBundleColorMap() can be
  // made to work in-place.
  uint8_t* const tmp_row = (uint8_t*)WebPSafeMalloc(width, sizeof(*tmp_row));
  int x, y;

  if (tmp_row == NULL) return VP8_ENC_ERROR_OUT_OF_MEMORY;

  if (palette_size < APPLY_PALETTE_GREEDY_MAX) {
    APPLY_PALETTE_FOR(SearchColorGreedy(palette, palette_size, pix));
  } else {
    int i, j;
    uint16_t buffer[PALETTE_INV_SIZE];
    uint32_t (*const hash_functions[])(uint32_t) = {
        ApplyPaletteHash0, ApplyPaletteHash1, ApplyPaletteHash2
    };

    // Try to find a perfect hash function able to go from a color to an index
    // within 1 << PALETTE_INV_SIZE_BITS in order to build a hash map to go
    // from color to index in palette.
    for (i = 0; i < 3; ++i) {
      int use_LUT = 1;
      // Set each element in buffer to max uint16_t.
      memset(buffer, 0xff, sizeof(buffer));
      for (j = 0; j < palette_size; ++j) {
        const uint32_t ind = hash_functions[i](palette[j]);
        if (buffer[ind] != 0xffffu) {
          use_LUT = 0;
          break;
        } else {
          buffer[ind] = j;
        }
      }
      if (use_LUT) break;
    }

    if (i == 0) {
      APPLY_PALETTE_FOR(buffer[ApplyPaletteHash0(pix)]);
    } else if (i == 1) {
      APPLY_PALETTE_FOR(buffer[ApplyPaletteHash1(pix)]);
    } else if (i == 2) {
      APPLY_PALETTE_FOR(buffer[ApplyPaletteHash2(pix)]);
    } else {
      uint32_t idx_map[MAX_PALETTE_SIZE];
      uint32_t palette_sorted[MAX_PALETTE_SIZE];
      PrepareMapToPalette(palette, palette_size, palette_sorted, idx_map);
      APPLY_PALETTE_FOR(
          idx_map[SearchColorNoIdx(palette_sorted, pix, palette_size)]);
    }
  }
  WebPSafeFree(tmp_row);
  return VP8_ENC_OK;
}
#undef APPLY_PALETTE_FOR
#undef PALETTE_INV_SIZE_BITS
#undef PALETTE_INV_SIZE
#undef APPLY_PALETTE_GREEDY_MAX

// Note: Expects "enc->palette_" to be set properly.
static WebPEncodingError MapImageFromPalette(VP8LEncoder* const enc,
                                             int in_place) {
  WebPEncodingError err = VP8_ENC_OK;
  const WebPPicture* const pic = enc->pic_;
  const int width = pic->width;
  const int height = pic->height;
  const uint32_t* const palette = enc->palette_;
  const uint32_t* src = in_place ? enc->argb_ : pic->argb;
  const int src_stride = in_place ? enc->current_width_ : pic->argb_stride;
  const int palette_size = enc->palette_size_;
  int xbits;

  // Replace each input pixel by corresponding palette index.
  // This is done line by line.
  if (palette_size <= 4) {
    xbits = (palette_size <= 2) ? 3 : 2;
  } else {
    xbits = (palette_size <= 16) ? 1 : 0;
  }

  err = AllocateTransformBuffer(enc, VP8LSubSampleSize(width, xbits), height);
  if (err != VP8_ENC_OK) return err;

  err = ApplyPalette(src, src_stride,
                     enc->argb_, enc->current_width_,
                     palette, palette_size, width, height, xbits);
  enc->argb_content_ = kEncoderPalette;
  return err;
}

static WebPEncodingError MakeInputImageCopy(VP8LEncoder* const enc) {
  WebPEncodingError err = VP8_ENC_OK;
  const WebPPicture* const picture = enc->pic_;
  const int width = picture->width;
  const int height = picture->height;
  int y;
  err = AllocateTransformBuffer(enc, width, height);
  if (err != VP8_ENC_OK) return err;
  if (enc->argb_content_ == kEncoderARGB) return VP8_ENC_OK;
  for (y = 0; y < height; ++y) {
    memcpy(enc->argb_ + y * width,
           picture->argb + y * picture->argb_stride,
           width * sizeof(*enc->argb_));
  }
  enc->argb_content_ = kEncoderARGB;
  assert(enc->current_width_ == width);
  return VP8_ENC_OK;
}

void VP8LSubtractGreenFromBlueAndRed_C(uint32_t* argb_data, int num_pixels) {
  int i;
  for (i = 0; i < num_pixels; ++i) {
    const int argb = argb_data[i];
    const int green = (argb >> 8) & 0xff;
    const uint32_t new_r = (((argb >> 16) & 0xff) - green) & 0xff;
    const uint32_t new_b = (((argb >>  0) & 0xff) - green) & 0xff;
    argb_data[i] = (argb & 0xff00ff00u) | (new_r << 16) | new_b;
  }
}

static void ApplySubtractGreen(VP8LEncoder* const enc, int width, int height,
                               VP8LBitWriter* const bw) {
  VP8LPutBits(bw, TRANSFORM_PRESENT, 1);
  VP8LPutBits(bw, SUBTRACT_GREEN, 2);
  VP8LSubtractGreenFromBlueAndRed_C(enc->argb_, width * height);
}

#define ARGB_BLACK                   0xff000000

static const int kPredLowEffort = 11;

// Mostly used to reduce code size + readability
static int GetMin(int a, int b) { return (a > b) ? b : a; }

#define MAX_DIFF_COST (1e30f)

static uint32_t AddGreenToBlueAndRed(uint32_t argb) {
  const uint32_t green = (argb >> 8) & 0xff;
  uint32_t red_blue = argb & 0x00ff00ffu;
  red_blue += (green << 16) | green;
  red_blue &= 0x00ff00ffu;
  return (argb & 0xff00ff00u) | red_blue;
}
static int GetMax(int a, int b) { return (a < b) ? b : a; }

static int MaxDiffBetweenPixels(uint32_t p1, uint32_t p2) {
  const int diff_a = abs((int)(p1 >> 24) - (int)(p2 >> 24));
  const int diff_r = abs((int)((p1 >> 16) & 0xff) - (int)((p2 >> 16) & 0xff));
  const int diff_g = abs((int)((p1 >> 8) & 0xff) - (int)((p2 >> 8) & 0xff));
  const int diff_b = abs((int)(p1 & 0xff) - (int)(p2 & 0xff));
  return GetMax(GetMax(diff_a, diff_r), GetMax(diff_g, diff_b));
}

static int MaxDiffAroundPixel(uint32_t current, uint32_t up, uint32_t down,
                              uint32_t left, uint32_t right) {
  const int diff_up = MaxDiffBetweenPixels(current, up);
  const int diff_down = MaxDiffBetweenPixels(current, down);
  const int diff_left = MaxDiffBetweenPixels(current, left);
  const int diff_right = MaxDiffBetweenPixels(current, right);
  return GetMax(GetMax(diff_up, diff_down), GetMax(diff_left, diff_right));
}

static void MaxDiffsForRow(int width, int stride, const uint32_t* const argb,
                           uint8_t* const max_diffs, int used_subtract_green) {
  uint32_t current, up, down, left, right;
  int x;
  if (width <= 2) return;
  current = argb[0];
  right = argb[1];
  if (used_subtract_green) {
    current = AddGreenToBlueAndRed(current);
    right = AddGreenToBlueAndRed(right);
  }
  // max_diffs[0] and max_diffs[width - 1] are never used.
  for (x = 1; x < width - 1; ++x) {
    up = argb[-stride + x];
    down = argb[stride + x];
    left = current;
    current = right;
    right = argb[x + 1];
    if (used_subtract_green) {
      up = AddGreenToBlueAndRed(up);
      down = AddGreenToBlueAndRed(down);
      right = AddGreenToBlueAndRed(right);
    }
    max_diffs[x] = MaxDiffAroundPixel(current, up, down, left, right);
  }
}

typedef void (*VP8LPredictorAddSubFunc)(const uint32_t* in,
                                        const uint32_t* upper, int num_pixels,
                                        uint32_t* out);

VP8LPredictorAddSubFunc VP8LPredictorsSub[16];

static void PredictorSub0_C(const uint32_t* in, const uint32_t* upper,
                            int num_pixels, uint32_t* out) {
  int i;
  for (i = 0; i < num_pixels; ++i) out[i] = VP8LSubPixels(in[i], ARGB_BLACK);
  (void)upper;
}

static void PredictorSub1_C(const uint32_t* in, const uint32_t* upper,
                            int num_pixels, uint32_t* out) {
  int i;
  for (i = 0; i < num_pixels; ++i) out[i] = VP8LSubPixels(in[i], in[i - 1]);
  (void)upper;
}

// It subtracts the prediction from the input pixel and stores the residual
// in the output pixel.
#define GENERATE_PREDICTOR_SUB(PREDICTOR, PREDICTOR_SUB)             \
static void PREDICTOR_SUB(const uint32_t* in, const uint32_t* upper, \
                          int num_pixels, uint32_t* out) {           \
  int x;                                                             \
  for (x = 0; x < num_pixels; ++x) {                                 \
    const uint32_t pred = (PREDICTOR)(in[x - 1], upper + x);         \
    out[x] = VP8LSubPixels(in[x], pred);                             \
  }                                                                  \
}

static uint32_t Average2(uint32_t a0, uint32_t a1) {
  return (((a0 ^ a1) & 0xfefefefeu) >> 1) + (a0 & a1);
}

static uint32_t Average3(uint32_t a0, uint32_t a1, uint32_t a2) {
  return Average2(Average2(a0, a2), a1);
}

static uint32_t Average4(uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3) {
  return Average2(Average2(a0, a1), Average2(a2, a3));
}

static int Sub3(int a, int b, int c) {
  const int pb = b - c;
  const int pa = a - c;
  return abs(pb) - abs(pa);
}

static uint32_t Select(uint32_t a, uint32_t b, uint32_t c) {
  const int pa_minus_pb =
      Sub3((a >> 24)       , (b >> 24)       , (c >> 24)       ) +
      Sub3((a >> 16) & 0xff, (b >> 16) & 0xff, (c >> 16) & 0xff) +
      Sub3((a >>  8) & 0xff, (b >>  8) & 0xff, (c >>  8) & 0xff) +
      Sub3((a      ) & 0xff, (b      ) & 0xff, (c      ) & 0xff);
  return (pa_minus_pb <= 0) ? a : b;
}

static uint32_t Clip255(uint32_t a) {
  if (a < 256) {
    return a;
  }
  // return 0, when a is a negative integer.
  // return 255, when a is positive.
  return ~a >> 24;
}

static int AddSubtractComponentFull(int a, int b, int c) {
  return Clip255(a + b - c);
}

static uint32_t ClampedAddSubtractFull(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const int a = AddSubtractComponentFull(c0 >> 24, c1 >> 24, c2 >> 24);
  const int r = AddSubtractComponentFull((c0 >> 16) & 0xff,
                                         (c1 >> 16) & 0xff,
                                         (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentFull((c0 >> 8) & 0xff,
                                         (c1 >> 8) & 0xff,
                                         (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentFull(c0 & 0xff, c1 & 0xff, c2 & 0xff);
  return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

static int AddSubtractComponentHalf(int a, int b) {
  return Clip255(a + (a - b) / 2);
}

static uint32_t ClampedAddSubtractHalf(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const uint32_t ave = Average2(c0, c1);
  const int a = AddSubtractComponentHalf(ave >> 24, c2 >> 24);
  const int r = AddSubtractComponentHalf((ave >> 16) & 0xff, (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentHalf((ave >> 8) & 0xff, (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentHalf((ave >> 0) & 0xff, (c2 >> 0) & 0xff);
  return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

static uint32_t Predictor2(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[0];
}
static uint32_t Predictor3(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[1];
}
static uint32_t Predictor4(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[-1];
}
static uint32_t Predictor5(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average3(left, top[0], top[1]);
  return pred;
}
static uint32_t Predictor6(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(left, top[-1]);
  return pred;
}
static uint32_t Predictor7(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(left, top[0]);
  return pred;
}
static uint32_t Predictor8(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(top[-1], top[0]);
  (void)left;
  return pred;
}
static uint32_t Predictor9(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(top[0], top[1]);
  (void)left;
  return pred;
}
static uint32_t Predictor10(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average4(left, top[-1], top[0], top[1]);
  return pred;
}
static uint32_t Predictor11(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Select(top[0], left, top[-1]);
  return pred;
}
static uint32_t Predictor12(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractFull(left, top[0], top[-1]);
  return pred;
}
static uint32_t Predictor13(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractHalf(left, top[0], top[-1]);
  return pred;
}

GENERATE_PREDICTOR_SUB(Predictor2, PredictorSub2_C)
GENERATE_PREDICTOR_SUB(Predictor3, PredictorSub3_C)
GENERATE_PREDICTOR_SUB(Predictor4, PredictorSub4_C)
GENERATE_PREDICTOR_SUB(Predictor5, PredictorSub5_C)
GENERATE_PREDICTOR_SUB(Predictor6, PredictorSub6_C)
GENERATE_PREDICTOR_SUB(Predictor7, PredictorSub7_C)
GENERATE_PREDICTOR_SUB(Predictor8, PredictorSub8_C)
GENERATE_PREDICTOR_SUB(Predictor9, PredictorSub9_C)
GENERATE_PREDICTOR_SUB(Predictor10, PredictorSub10_C)
GENERATE_PREDICTOR_SUB(Predictor11, PredictorSub11_C)
GENERATE_PREDICTOR_SUB(Predictor12, PredictorSub12_C)
GENERATE_PREDICTOR_SUB(Predictor13, PredictorSub13_C)

static void PredictBatch(int mode, int x_start, int y,
                                     int num_pixels, const uint32_t* current,
                                     const uint32_t* upper, uint32_t* out) {

	VP8LPredictorsSub[0] = PredictorSub0_C;
	VP8LPredictorsSub[1] = PredictorSub1_C;
	VP8LPredictorsSub[2] = PredictorSub2_C;
	VP8LPredictorsSub[3] = PredictorSub3_C;
	VP8LPredictorsSub[4] = PredictorSub4_C;
	VP8LPredictorsSub[5] = PredictorSub5_C;
	VP8LPredictorsSub[6] = PredictorSub6_C;
	VP8LPredictorsSub[7] = PredictorSub7_C;
	VP8LPredictorsSub[8] = PredictorSub8_C;
	VP8LPredictorsSub[9] = PredictorSub9_C;
	VP8LPredictorsSub[10] = PredictorSub10_C;
	VP8LPredictorsSub[11] = PredictorSub11_C;
	VP8LPredictorsSub[12] = PredictorSub12_C;
	VP8LPredictorsSub[13] = PredictorSub13_C;
	VP8LPredictorsSub[14] = PredictorSub0_C;  // <- padding security sentinels
	VP8LPredictorsSub[15] = PredictorSub0_C;

  if (x_start == 0) {
    if (y == 0) {
      // ARGB_BLACK.
      VP8LPredictorsSub[0](current, NULL, 1, out);
    } else {
      // Top one.
      VP8LPredictorsSub[2](current, upper, 1, out);
    }
    ++x_start;
    ++out;
    --num_pixels;
  }
  if (y == 0) {
    // Left one.
    VP8LPredictorsSub[1](current + x_start, NULL, num_pixels, out);
  } else {
    VP8LPredictorsSub[mode](current + x_start, upper + x_start, num_pixels,
                            out);
  }
}

typedef uint32_t (*VP8LPredictorFunc)(uint32_t left, const uint32_t* const top);

VP8LPredictorFunc VP8LPredictors[16];

#define COPY_PREDICTOR_ARRAY(IN, OUT) do {                \
  (OUT)[0] = IN##0_C;                                     \
  (OUT)[1] = IN##1_C;                                     \
  (OUT)[2] = IN##2_C;                                     \
  (OUT)[3] = IN##3_C;                                     \
  (OUT)[4] = IN##4_C;                                     \
  (OUT)[5] = IN##5_C;                                     \
  (OUT)[6] = IN##6_C;                                     \
  (OUT)[7] = IN##7_C;                                     \
  (OUT)[8] = IN##8_C;                                     \
  (OUT)[9] = IN##9_C;                                     \
  (OUT)[10] = IN##10_C;                                   \
  (OUT)[11] = IN##11_C;                                   \
  (OUT)[12] = IN##12_C;                                   \
  (OUT)[13] = IN##13_C;                                   \
  (OUT)[14] = IN##0_C; /* <- padding security sentinels*/ \
  (OUT)[15] = IN##0_C;                                    \
} while (0);

static uint32_t Predictor0_C(uint32_t left, const uint32_t* const top) {
  (void)top;
  (void)left;
  return ARGB_BLACK;
}
static uint32_t Predictor1_C(uint32_t left, const uint32_t* const top) {
  (void)top;
  return left;
}
static uint32_t Predictor2_C(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[0];
}
static uint32_t Predictor3_C(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[1];
}
static uint32_t Predictor4_C(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[-1];
}
static uint32_t Predictor5_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average3(left, top[0], top[1]);
  return pred;
}
static uint32_t Predictor6_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(left, top[-1]);
  return pred;
}
static uint32_t Predictor7_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(left, top[0]);
  return pred;
}
static uint32_t Predictor8_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(top[-1], top[0]);
  (void)left;
  return pred;
}
static uint32_t Predictor9_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(top[0], top[1]);
  (void)left;
  return pred;
}
static uint32_t Predictor10_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average4(left, top[-1], top[0], top[1]);
  return pred;
}
static uint32_t Predictor11_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Select(top[0], left, top[-1]);
  return pred;
}
static uint32_t Predictor12_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractFull(left, top[0], top[-1]);
  return pred;
}
static uint32_t Predictor13_C(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractHalf(left, top[0], top[-1]);
  return pred;
}

// Quantize the difference between the actual component value and its prediction
// to a multiple of quantization, working modulo 256, taking care not to cross
// a boundary (inclusive upper limit).
static uint8_t NearLosslessComponent(uint8_t value, uint8_t predict,
                                     uint8_t boundary, int quantization) {
  const int residual = (value - predict) & 0xff;
  const int boundary_residual = (boundary - predict) & 0xff;
  const int lower = residual & ~(quantization - 1);
  const int upper = lower + quantization;
  // Resolve ties towards a value closer to the prediction (i.e. towards lower
  // if value comes after prediction and towards upper otherwise).
  const int bias = ((boundary - value) & 0xff) < boundary_residual;
  if (residual - lower < upper - residual + bias) {
    // lower is closer to residual than upper.
    if (residual > boundary_residual && lower <= boundary_residual) {
      // Halve quantization step to avoid crossing boundary. This midpoint is
      // on the same side of boundary as residual because midpoint >= residual
      // (since lower is closer than upper) and residual is above the boundary.
      return lower + (quantization >> 1);
    }
    return lower;
  } else {
    // upper is closer to residual than lower.
    if (residual <= boundary_residual && upper > boundary_residual) {
      // Halve quantization step to avoid crossing boundary. This midpoint is
      // on the same side of boundary as residual because midpoint <= residual
      // (since upper is closer than lower) and residual is below the boundary.
      return lower + (quantization >> 1);
    }
    return upper & 0xff;
  }
}

// Sum of each component, mod 256.
static uint32_t VP8LAddPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green = (a & 0xff00ff00u) + (b & 0xff00ff00u);
  const uint32_t red_and_blue = (a & 0x00ff00ffu) + (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

#define NEAR_LOSSLESS_DIFF(a, b) (uint8_t)((((int)(a) - (int)(b))) & 0xff)
static uint32_t NearLossless(uint32_t value, uint32_t predict,
                             int max_quantization, int max_diff,
                             int used_subtract_green) {
  int quantization;
  uint8_t new_green = 0;
  uint8_t green_diff = 0;
  uint8_t a, r, g, b;
  if (max_diff <= 2) {
    return VP8LSubPixels(value, predict);
  }
  quantization = max_quantization;
  while (quantization >= max_diff) {
    quantization >>= 1;
  }
  if ((value >> 24) == 0 || (value >> 24) == 0xff) {
    // Preserve transparency of fully transparent or fully opaque pixels.
    a = NEAR_LOSSLESS_DIFF(value >> 24, predict >> 24);
  } else {
    a = NearLosslessComponent(value >> 24, predict >> 24, 0xff, quantization);
  }
  g = NearLosslessComponent((value >> 8) & 0xff, (predict >> 8) & 0xff, 0xff,
                            quantization);
  if (used_subtract_green) {
    // The green offset will be added to red and blue components during decoding
    // to obtain the actual red and blue values.
    new_green = ((predict >> 8) + g) & 0xff;
    // The amount by which green has been adjusted during quantization. It is
    // subtracted from red and blue for compensation, to avoid accumulating two
    // quantization errors in them.
    green_diff = NEAR_LOSSLESS_DIFF(new_green, value >> 8);
  }
  r = NearLosslessComponent(NEAR_LOSSLESS_DIFF(value >> 16, green_diff),
                            (predict >> 16) & 0xff, 0xff - new_green,
                            quantization);
  b = NearLosslessComponent(NEAR_LOSSLESS_DIFF(value, green_diff),
                            predict & 0xff, 0xff - new_green, quantization);
  return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
#undef NEAR_LOSSLESS_DIFF

static const uint32_t kMaskAlpha = 0xff000000;

// Stores the difference between the pixel and its prediction in "out".
// In case of a lossy encoding, updates the source image to avoid propagating
// the deviation further to pixels which depend on the current pixel for their
// predictions.
static void GetResidual(
    int width, int height, uint32_t* const upper_row,
    uint32_t* const current_row, const uint8_t* const max_diffs, int mode,
    int x_start, int x_end, int y, int max_quantization, int exact,
    int used_subtract_green, uint32_t* const out) {

	  COPY_PREDICTOR_ARRAY(Predictor, VP8LPredictors)

  if (exact) {
    PredictBatch(mode, x_start, y, x_end - x_start, current_row, upper_row,
                 out);
  } else {
    const VP8LPredictorFunc pred_func = VP8LPredictors[mode];
    int x;
    for (x = x_start; x < x_end; ++x) {
      uint32_t predict;
      uint32_t residual;
      if (y == 0) {
        predict = (x == 0) ? ARGB_BLACK : current_row[x - 1];  // Left.
      } else if (x == 0) {
        predict = upper_row[x];  // Top.
      } else {
        predict = pred_func(current_row[x - 1], upper_row + x);
      }
#if (WEBP_NEAR_LOSSLESS == 1)
      if (max_quantization == 1 || mode == 0 || y == 0 || y == height - 1 ||
          x == 0 || x == width - 1) {
        residual = VP8LSubPixels(current_row[x], predict);
      } else {
        residual = NearLossless(current_row[x], predict, max_quantization,
                                max_diffs[x], used_subtract_green);
        // Update the source image.
        current_row[x] = VP8LAddPixels(predict, residual);
        // x is never 0 here so we do not need to update upper_row like below.
      }
#else
      (void)max_diffs;
      (void)height;
      (void)max_quantization;
      (void)used_subtract_green;
      residual = VP8LSubPixels(current_row[x], predict);
#endif
      if ((current_row[x] & kMaskAlpha) == 0) {
        // If alpha is 0, cleanup RGB. We can choose the RGB values of the
        // residual for best compression. The prediction of alpha itself can be
        // non-zero and must be kept though. We choose RGB of the residual to be
        // 0.
        residual &= kMaskAlpha;
        // Update the source image.
        current_row[x] = predict & ~kMaskAlpha;
        // The prediction for the rightmost pixel in a row uses the leftmost
        // pixel
        // in that row as its top-right context pixel. Hence if we change the
        // leftmost pixel of current_row, the corresponding change must be
        // applied
        // to upper_row as well where top-right context is being read from.
        if (x == 0 && y != 0) upper_row[width] = current_row[0];
      }
      out[x - x_start] = residual;
    }
  }
}

static void UpdateHisto(int histo_argb[4][256], uint32_t argb) {
  ++histo_argb[0][argb >> 24];
  ++histo_argb[1][(argb >> 16) & 0xff];
  ++histo_argb[2][(argb >> 8) & 0xff];
  ++histo_argb[3][argb & 0xff];
}

static float PredictionCostSpatial(const int counts[256], int weight_0,
                                   double exp_val) {
  const int significant_symbols = 256 >> 4;
  const double exp_decay_factor = 0.6;
  double bits = weight_0 * counts[0];
  int i;
  for (i = 1; i < significant_symbols; ++i) {
    bits += exp_val * (counts[i] + counts[256 - i]);
    exp_val *= exp_decay_factor;
  }
  return (float)(-0.1 * bits);
}

// Compute the combined Shanon's entropy for distribution {X} and {X+Y}
static float CombinedShannonEntropy_C(const int X[256], const int Y[256]) {
  int i;
  double retval = 0.;
  int sumX = 0, sumXY = 0;
  for (i = 0; i < 256; ++i) {
    const int x = X[i];
    if (x != 0) {
      const int xy = x + Y[i];
      sumX += x;
      retval -= VP8LFastSLog2(x);
      sumXY += xy;
      retval -= VP8LFastSLog2(xy);
    } else if (Y[i] != 0) {
      sumXY += Y[i];
      retval -= VP8LFastSLog2(Y[i]);
    }
  }
  retval += VP8LFastSLog2(sumX) + VP8LFastSLog2(sumXY);
  return (float)retval;
}

static float PredictionCostSpatialHistogram(const int accumulated[4][256],
                                            const int tile[4][256]) {
  int i;
  double retval = 0;
  for (i = 0; i < 4; ++i) {
    const double kExpValue = 0.94;
    retval += PredictionCostSpatial(tile[i], 1, kExpValue);
    retval += CombinedShannonEntropy_C(tile[i], accumulated[i]);
  }
  return (float)retval;
}

static const float kSpatialPredictorBias = 15.f;

// Returns best predictor and updates the accumulated histogram.
// If max_quantization > 1, assumes that near lossless processing will be
// applied, quantizing residuals to multiples of quantization levels up to
// max_quantization (the actual quantization level depends on smoothness near
// the given pixel).
static int GetBestPredictorForTile(int width, int height,
                                   int tile_x, int tile_y, int bits,
                                   int accumulated[4][256],
                                   uint32_t* const argb_scratch,
                                   const uint32_t* const argb,
                                   int max_quantization,
                                   int exact, int used_subtract_green,
                                   const uint32_t* const modes) {
  const int kNumPredModes = 14;
  const int start_x = tile_x << bits;
  const int start_y = tile_y << bits;
  const int tile_size = 1 << bits;
  const int max_y = GetMin(tile_size, height - start_y);
  const int max_x = GetMin(tile_size, width - start_x);
  // Whether there exist columns just outside the tile.
  const int have_left = (start_x > 0);
  // Position and size of the strip covering the tile and adjacent columns if
  // they exist.
  const int context_start_x = start_x - have_left;
#if (WEBP_NEAR_LOSSLESS == 1)
  const int context_width = max_x + have_left + (max_x < width - start_x);
#endif
  const int tiles_per_row = VP8LSubSampleSize(width, bits);
  // Prediction modes of the left and above neighbor tiles.
  const int left_mode = (tile_x > 0) ?
      (modes[tile_y * tiles_per_row + tile_x - 1] >> 8) & 0xff : 0xff;
  const int above_mode = (tile_y > 0) ?
      (modes[(tile_y - 1) * tiles_per_row + tile_x] >> 8) & 0xff : 0xff;
  // The width of upper_row and current_row is one pixel larger than image width
  // to allow the top right pixel to point to the leftmost pixel of the next row
  // when at the right edge.
  uint32_t* upper_row = argb_scratch;
  uint32_t* current_row = upper_row + width + 1;
  uint8_t* const max_diffs = (uint8_t*)(current_row + width + 1);
  float best_diff = MAX_DIFF_COST;
  int best_mode = 0;
  int mode;
  int histo_stack_1[4][256];
  int histo_stack_2[4][256];
  // Need pointers to be able to swap arrays.
  int (*histo_argb)[256] = histo_stack_1;
  int (*best_histo)[256] = histo_stack_2;
  int i, j;
  uint32_t residuals[1 << MAX_TRANSFORM_BITS];
  assert(bits <= MAX_TRANSFORM_BITS);
  assert(max_x <= (1 << MAX_TRANSFORM_BITS));

  for (mode = 0; mode < kNumPredModes; ++mode) {
    float cur_diff;
    int relative_y;
    memset(histo_argb, 0, sizeof(histo_stack_1));
    if (start_y > 0) {
      // Read the row above the tile which will become the first upper_row.
      // Include a pixel to the left if it exists; include a pixel to the right
      // in all cases (wrapping to the leftmost pixel of the next row if it does
      // not exist).
      memcpy(current_row + context_start_x,
             argb + (start_y - 1) * width + context_start_x,
             sizeof(*argb) * (max_x + have_left + 1));
    }
    for (relative_y = 0; relative_y < max_y; ++relative_y) {
      const int y = start_y + relative_y;
      int relative_x;
      uint32_t* tmp = upper_row;
      upper_row = current_row;
      current_row = tmp;
      // Read current_row. Include a pixel to the left if it exists; include a
      // pixel to the right in all cases except at the bottom right corner of
      // the image (wrapping to the leftmost pixel of the next row if it does
      // not exist in the current row).
      memcpy(current_row + context_start_x,
             argb + y * width + context_start_x,
             sizeof(*argb) * (max_x + have_left + (y + 1 < height)));
#if (WEBP_NEAR_LOSSLESS == 1)
      if (max_quantization > 1 && y >= 1 && y + 1 < height) {
        MaxDiffsForRow(context_width, width, argb + y * width + context_start_x,
                       max_diffs + context_start_x, used_subtract_green);
      }
#endif

      GetResidual(width, height, upper_row, current_row, max_diffs, mode,
                  start_x, start_x + max_x, y, max_quantization, exact,
                  used_subtract_green, residuals);
      for (relative_x = 0; relative_x < max_x; ++relative_x) {
        UpdateHisto(histo_argb, residuals[relative_x]);
      }
    }
    cur_diff = PredictionCostSpatialHistogram(
        (const int (*)[256])accumulated, (const int (*)[256])histo_argb);
    // Favor keeping the areas locally similar.
    if (mode == left_mode) cur_diff -= kSpatialPredictorBias;
    if (mode == above_mode) cur_diff -= kSpatialPredictorBias;

    if (cur_diff < best_diff) {
      int (*tmp)[256] = histo_argb;
      histo_argb = best_histo;
      best_histo = tmp;
      best_diff = cur_diff;
      best_mode = mode;
    }
  }

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 256; j++) {
      accumulated[i][j] += best_histo[i][j];
    }
  }

  return best_mode;
}

// Converts pixels of the image to residuals with respect to predictions.
// If max_quantization > 1, applies near lossless processing, quantizing
// residuals to multiples of quantization levels up to max_quantization
// (the actual quantization level depends on smoothness near the given pixel).
static void CopyImageWithPrediction(int width, int height,
                                    int bits, uint32_t* const modes,
                                    uint32_t* const argb_scratch,
                                    uint32_t* const argb,
                                    int low_effort, int max_quantization,
                                    int exact, int used_subtract_green) {
  const int tiles_per_row = VP8LSubSampleSize(width, bits);
  // The width of upper_row and current_row is one pixel larger than image width
  // to allow the top right pixel to point to the leftmost pixel of the next row
  // when at the right edge.
  uint32_t* upper_row = argb_scratch;
  uint32_t* current_row = upper_row + width + 1;
  uint8_t* current_max_diffs = (uint8_t*)(current_row + width + 1);
#if (WEBP_NEAR_LOSSLESS == 1)
  uint8_t* lower_max_diffs = current_max_diffs + width;
#endif
  int y;

  for (y = 0; y < height; ++y) {
    int x;
    uint32_t* const tmp32 = upper_row;
    upper_row = current_row;
    current_row = tmp32;
    memcpy(current_row, argb + y * width,
           sizeof(*argb) * (width + (y + 1 < height)));

    if (low_effort) {
      PredictBatch(kPredLowEffort, 0, y, width, current_row, upper_row,
                   argb + y * width);
    } else {
#if (WEBP_NEAR_LOSSLESS == 1)
      if (max_quantization > 1) {
        // Compute max_diffs for the lower row now, because that needs the
        // contents of argb for the current row, which we will overwrite with
        // residuals before proceeding with the next row.
        uint8_t* const tmp8 = current_max_diffs;
        current_max_diffs = lower_max_diffs;
        lower_max_diffs = tmp8;
        if (y + 2 < height) {
          MaxDiffsForRow(width, width, argb + (y + 1) * width, lower_max_diffs,
                         used_subtract_green);
        }
      }
#endif
      for (x = 0; x < width;) {
        const int mode =
            (modes[(y >> bits) * tiles_per_row + (x >> bits)] >> 8) & 0xff;
        int x_end = x + (1 << bits);
        if (x_end > width) x_end = width;
        GetResidual(width, height, upper_row, current_row, current_max_diffs,
                    mode, x, x_end, y, max_quantization, exact,
                    used_subtract_green, argb + y * width + x);
        x = x_end;
      }
    }
  }
}

// Finds the best predictor for each tile, and converts the image to residuals
// with respect to predictions. If near_lossless_quality < 100, applies
// near lossless processing, shaving off more bits of residuals for lower
// qualities.
void VP8LResidualImage(int width, int height, int bits, int low_effort,
                       uint32_t* const argb, uint32_t* const argb_scratch,
                       uint32_t* const image, int near_lossless_quality,
                       int exact, int used_subtract_green) {
  const int tiles_per_row = VP8LSubSampleSize(width, bits);
  const int tiles_per_col = VP8LSubSampleSize(height, bits);
  int tile_y;
  int histo[4][256];
  const int max_quantization = 1 << VP8LNearLosslessBits(near_lossless_quality);
  if (low_effort) {
    int i;
    for (i = 0; i < tiles_per_row * tiles_per_col; ++i) {
      image[i] = ARGB_BLACK | (kPredLowEffort << 8);
    }
  } else {
    memset(histo, 0, sizeof(histo));
    for (tile_y = 0; tile_y < tiles_per_col; ++tile_y) {
      int tile_x;
      for (tile_x = 0; tile_x < tiles_per_row; ++tile_x) {
        const int pred = GetBestPredictorForTile(width, height, tile_x, tile_y,
            bits, histo, argb_scratch, argb, max_quantization, exact,
            used_subtract_green, image);
        image[tile_y * tiles_per_row + tile_x] = ARGB_BLACK | (pred << 8);
      }
    }
  }

  CopyImageWithPrediction(width, height, bits, image, argb_scratch, argb,
                          low_effort, max_quantization, exact,
                          used_subtract_green);
}

static WebPEncodingError ApplyPredictFilter(const VP8LEncoder* const enc,
                                            int width, int height,
                                            int quality, int low_effort,
                                            int used_subtract_green,
                                            VP8LBitWriter* const bw) {
  const int pred_bits = enc->transform_bits_;
  const int transform_width = VP8LSubSampleSize(width, pred_bits);
  const int transform_height = VP8LSubSampleSize(height, pred_bits);
  // we disable near-lossless quantization if palette is used.
  const int near_lossless_strength = enc->use_palette_ ? 100
                                   : enc->config_->near_lossless;

  VP8LResidualImage(width, height, pred_bits, low_effort, enc->argb_,
                    enc->argb_scratch_, enc->transform_data_,
                    near_lossless_strength, enc->config_->exact,
                    used_subtract_green);
  VP8LPutBits(bw, TRANSFORM_PRESENT, 1);
  VP8LPutBits(bw, PREDICTOR_TRANSFORM, 2);
  assert(pred_bits >= 2);
  VP8LPutBits(bw, pred_bits - 2, 3);
  return EncodeImageNoHuffman(
      bw, enc->transform_data_, (VP8LHashChain*)&enc->hash_chain_,
      (VP8LBackwardRefs*)&enc->refs_[0],  // cast const away
      (VP8LBackwardRefs*)&enc->refs_[1], transform_width, transform_height,
      quality, low_effort);
}

typedef struct {
  // Note: the members are uint8_t, so that any negative values are
  // automatically converted to "mod 256" values.
  uint8_t green_to_red_;
  uint8_t green_to_blue_;
  uint8_t red_to_blue_;
} VP8LMultipliers;

static void MultipliersClear(VP8LMultipliers* const m) {
  m->green_to_red_ = 0;
  m->green_to_blue_ = 0;
  m->red_to_blue_ = 0;
}

static void ColorCodeToMultipliers(uint32_t color_code,
                                               VP8LMultipliers* const m) {
  m->green_to_red_  = (color_code >>  0) & 0xff;
  m->green_to_blue_ = (color_code >>  8) & 0xff;
  m->red_to_blue_   = (color_code >> 16) & 0xff;
}

static int ColorTransformDelta(int8_t color_pred, int8_t color) {
  return ((int)color_pred * color) >> 5;
}

static uint8_t TransformColorRed(uint8_t green_to_red,
                                             uint32_t argb) {
  const uint32_t green = argb >> 8;
  int new_red = argb >> 16;
  new_red -= ColorTransformDelta(green_to_red, green);
  return (new_red & 0xff);
}

void VP8LCollectColorRedTransforms_C(const uint32_t* argb, int stride,
                                     int tile_width, int tile_height,
                                     int green_to_red, int histo[]) {
  while (tile_height-- > 0) {
    int x;
    for (x = 0; x < tile_width; ++x) {
      ++histo[TransformColorRed(green_to_red, argb[x])];
    }
    argb += stride;
  }
}

static float PredictionCostCrossColor(const int accumulated[256],
                                      const int counts[256]) {
  // Favor low entropy, locally and globally.
  // Favor small absolute values for PredictionCostSpatial
  static const double kExpValue = 2.4;
  return CombinedShannonEntropy_C(counts, accumulated) +
         PredictionCostSpatial(counts, 3, kExpValue);
}

static float GetPredictionCostCrossColorRed(
    const uint32_t* argb, int stride, int tile_width, int tile_height,
    VP8LMultipliers prev_x, VP8LMultipliers prev_y, int green_to_red,
    const int accumulated_red_histo[256]) {
  int histo[256] = { 0 };
  float cur_diff;

  VP8LCollectColorRedTransforms_C(argb, stride, tile_width, tile_height,
                                green_to_red, histo);

  cur_diff = PredictionCostCrossColor(accumulated_red_histo, histo);
  if ((uint8_t)green_to_red == prev_x.green_to_red_) {
    cur_diff -= 3;  // favor keeping the areas locally similar
  }
  if ((uint8_t)green_to_red == prev_y.green_to_red_) {
    cur_diff -= 3;  // favor keeping the areas locally similar
  }
  if (green_to_red == 0) {
    cur_diff -= 3;
  }
  return cur_diff;
}

static void GetBestGreenToRed(
    const uint32_t* argb, int stride, int tile_width, int tile_height,
    VP8LMultipliers prev_x, VP8LMultipliers prev_y, int quality,
    const int accumulated_red_histo[256], VP8LMultipliers* const best_tx) {
  const int kMaxIters = 4 + ((7 * quality) >> 8);  // in range [4..6]
  int green_to_red_best = 0;
  int iter, offset;
  float best_diff = GetPredictionCostCrossColorRed(
      argb, stride, tile_width, tile_height, prev_x, prev_y,
      green_to_red_best, accumulated_red_histo);
  for (iter = 0; iter < kMaxIters; ++iter) {
    // ColorTransformDelta is a 3.5 bit fixed point, so 32 is equal to
    // one in color computation. Having initial delta here as 1 is sufficient
    // to explore the range of (-2, 2).
    const int delta = 32 >> iter;
    // Try a negative and a positive delta from the best known value.
    for (offset = -delta; offset <= delta; offset += 2 * delta) {
      const int green_to_red_cur = offset + green_to_red_best;
      const float cur_diff = GetPredictionCostCrossColorRed(
          argb, stride, tile_width, tile_height, prev_x, prev_y,
          green_to_red_cur, accumulated_red_histo);
      if (cur_diff < best_diff) {
        best_diff = cur_diff;
        green_to_red_best = green_to_red_cur;
      }
    }
  }
  best_tx->green_to_red_ = green_to_red_best;
}

static uint8_t TransformColorBlue(uint8_t green_to_blue,
                                              uint8_t red_to_blue,
                                              uint32_t argb) {
  const uint32_t green = argb >> 8;
  const uint32_t red = argb >> 16;
  uint8_t new_blue = argb;
  new_blue -= ColorTransformDelta(green_to_blue, green);
  new_blue -= ColorTransformDelta(red_to_blue, red);
  return (new_blue & 0xff);
}

void VP8LCollectColorBlueTransforms_C(const uint32_t* argb, int stride,
                                      int tile_width, int tile_height,
                                      int green_to_blue, int red_to_blue,
                                      int histo[]) {
  while (tile_height-- > 0) {
    int x;
    for (x = 0; x < tile_width; ++x) {
      ++histo[TransformColorBlue(green_to_blue, red_to_blue, argb[x])];
    }
    argb += stride;
  }
}

static float GetPredictionCostCrossColorBlue(
    const uint32_t* argb, int stride, int tile_width, int tile_height,
    VP8LMultipliers prev_x, VP8LMultipliers prev_y,
    int green_to_blue, int red_to_blue, const int accumulated_blue_histo[256]) {
  int histo[256] = { 0 };
  float cur_diff;

  VP8LCollectColorBlueTransforms_C(argb, stride, tile_width, tile_height,
                                 green_to_blue, red_to_blue, histo);

  cur_diff = PredictionCostCrossColor(accumulated_blue_histo, histo);
  if ((uint8_t)green_to_blue == prev_x.green_to_blue_) {
    cur_diff -= 3;  // favor keeping the areas locally similar
  }
  if ((uint8_t)green_to_blue == prev_y.green_to_blue_) {
    cur_diff -= 3;  // favor keeping the areas locally similar
  }
  if ((uint8_t)red_to_blue == prev_x.red_to_blue_) {
    cur_diff -= 3;  // favor keeping the areas locally similar
  }
  if ((uint8_t)red_to_blue == prev_y.red_to_blue_) {
    cur_diff -= 3;  // favor keeping the areas locally similar
  }
  if (green_to_blue == 0) {
    cur_diff -= 3;
  }
  if (red_to_blue == 0) {
    cur_diff -= 3;
  }
  return cur_diff;
}

#define kGreenRedToBlueNumAxis 8
#define kGreenRedToBlueMaxIters 7
static void GetBestGreenRedToBlue(
    const uint32_t* argb, int stride, int tile_width, int tile_height,
    VP8LMultipliers prev_x, VP8LMultipliers prev_y, int quality,
    const int accumulated_blue_histo[256],
    VP8LMultipliers* const best_tx) {
  const int8_t offset[kGreenRedToBlueNumAxis][2] =
      {{0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  const int8_t delta_lut[kGreenRedToBlueMaxIters] = { 16, 16, 8, 4, 2, 2, 2 };
  const int iters =
      (quality < 25) ? 1 : (quality > 50) ? kGreenRedToBlueMaxIters : 4;
  int green_to_blue_best = 0;
  int red_to_blue_best = 0;
  int iter;
  // Initial value at origin:
  float best_diff = GetPredictionCostCrossColorBlue(
      argb, stride, tile_width, tile_height, prev_x, prev_y,
      green_to_blue_best, red_to_blue_best, accumulated_blue_histo);
  for (iter = 0; iter < iters; ++iter) {
    const int delta = delta_lut[iter];
    int axis;
    for (axis = 0; axis < kGreenRedToBlueNumAxis; ++axis) {
      const int green_to_blue_cur =
          offset[axis][0] * delta + green_to_blue_best;
      const int red_to_blue_cur = offset[axis][1] * delta + red_to_blue_best;
      const float cur_diff = GetPredictionCostCrossColorBlue(
          argb, stride, tile_width, tile_height, prev_x, prev_y,
          green_to_blue_cur, red_to_blue_cur, accumulated_blue_histo);
      if (cur_diff < best_diff) {
        best_diff = cur_diff;
        green_to_blue_best = green_to_blue_cur;
        red_to_blue_best = red_to_blue_cur;
      }
      if (quality < 25 && iter == 4) {
        // Only axis aligned diffs for lower quality.
        break;  // next iter.
      }
    }
    if (delta == 2 && green_to_blue_best == 0 && red_to_blue_best == 0) {
      // Further iterations would not help.
      break;  // out of iter-loop.
    }
  }
  best_tx->green_to_blue_ = green_to_blue_best;
  best_tx->red_to_blue_ = red_to_blue_best;
}
#undef kGreenRedToBlueMaxIters
#undef kGreenRedToBlueNumAxis

static VP8LMultipliers GetBestColorTransformForTile(
    int tile_x, int tile_y, int bits,
    VP8LMultipliers prev_x,
    VP8LMultipliers prev_y,
    int quality, int xsize, int ysize,
    const int accumulated_red_histo[256],
    const int accumulated_blue_histo[256],
    const uint32_t* const argb) {
  const int max_tile_size = 1 << bits;
  const int tile_y_offset = tile_y * max_tile_size;
  const int tile_x_offset = tile_x * max_tile_size;
  const int all_x_max = GetMin(tile_x_offset + max_tile_size, xsize);
  const int all_y_max = GetMin(tile_y_offset + max_tile_size, ysize);
  const int tile_width = all_x_max - tile_x_offset;
  const int tile_height = all_y_max - tile_y_offset;
  const uint32_t* const tile_argb = argb + tile_y_offset * xsize
                                  + tile_x_offset;
  VP8LMultipliers best_tx;
  MultipliersClear(&best_tx);

  GetBestGreenToRed(tile_argb, xsize, tile_width, tile_height,
                    prev_x, prev_y, quality, accumulated_red_histo, &best_tx);
  GetBestGreenRedToBlue(tile_argb, xsize, tile_width, tile_height,
                        prev_x, prev_y, quality, accumulated_blue_histo,
                        &best_tx);
  return best_tx;
}

static uint32_t MultipliersToColorCode(
    const VP8LMultipliers* const m) {
  return 0xff000000u |
         ((uint32_t)(m->red_to_blue_) << 16) |
         ((uint32_t)(m->green_to_blue_) << 8) |
         m->green_to_red_;
}

void VP8LTransformColor_C(const VP8LMultipliers* const m, uint32_t* data,
                          int num_pixels) {
  int i;
  for (i = 0; i < num_pixels; ++i) {
    const uint32_t argb = data[i];
    const uint32_t green = argb >> 8;
    const uint32_t red = argb >> 16;
    int new_red = red & 0xff;
    int new_blue = argb & 0xff;
    new_red -= ColorTransformDelta(m->green_to_red_, green);
    new_red &= 0xff;
    new_blue -= ColorTransformDelta(m->green_to_blue_, green);
    new_blue -= ColorTransformDelta(m->red_to_blue_, red);
    new_blue &= 0xff;
    data[i] = (argb & 0xff00ff00u) | (new_red << 16) | (new_blue);
  }
}

static void CopyTileWithColorTransform(int xsize, int ysize,
                                       int tile_x, int tile_y,
                                       int max_tile_size,
                                       VP8LMultipliers color_transform,
                                       uint32_t* argb) {
  const int xscan = GetMin(max_tile_size, xsize - tile_x);
  int yscan = GetMin(max_tile_size, ysize - tile_y);
  argb += tile_y * xsize + tile_x;
  while (yscan-- > 0) {
	VP8LTransformColor_C(&color_transform, argb, xscan);
    argb += xsize;
  }
}

void VP8LColorSpaceTransform(int width, int height, int bits, int quality,
                             uint32_t* const argb, uint32_t* image) {
  const int max_tile_size = 1 << bits;
  const int tile_xsize = VP8LSubSampleSize(width, bits);
  const int tile_ysize = VP8LSubSampleSize(height, bits);
  int accumulated_red_histo[256] = { 0 };
  int accumulated_blue_histo[256] = { 0 };
  int tile_x, tile_y;
  VP8LMultipliers prev_x, prev_y;
  MultipliersClear(&prev_y);
  MultipliersClear(&prev_x);
  for (tile_y = 0; tile_y < tile_ysize; ++tile_y) {
    for (tile_x = 0; tile_x < tile_xsize; ++tile_x) {
      int y;
      const int tile_x_offset = tile_x * max_tile_size;
      const int tile_y_offset = tile_y * max_tile_size;
      const int all_x_max = GetMin(tile_x_offset + max_tile_size, width);
      const int all_y_max = GetMin(tile_y_offset + max_tile_size, height);
      const int offset = tile_y * tile_xsize + tile_x;
      if (tile_y != 0) {
        ColorCodeToMultipliers(image[offset - tile_xsize], &prev_y);
      }
      prev_x = GetBestColorTransformForTile(tile_x, tile_y, bits,
                                            prev_x, prev_y,
                                            quality, width, height,
                                            accumulated_red_histo,
                                            accumulated_blue_histo,
                                            argb);
      image[offset] = MultipliersToColorCode(&prev_x);
      CopyTileWithColorTransform(width, height, tile_x_offset, tile_y_offset,
                                 max_tile_size, prev_x, argb);

      // Gather accumulated histogram data.
      for (y = tile_y_offset; y < all_y_max; ++y) {
        int ix = y * width + tile_x_offset;
        const int ix_end = ix + all_x_max - tile_x_offset;
        for (; ix < ix_end; ++ix) {
          const uint32_t pix = argb[ix];
          if (ix >= 2 &&
              pix == argb[ix - 2] &&
              pix == argb[ix - 1]) {
            continue;  // repeated pixels are handled by backward references
          }
          if (ix >= width + 2 &&
              argb[ix - 2] == argb[ix - width - 2] &&
              argb[ix - 1] == argb[ix - width - 1] &&
              pix == argb[ix - width]) {
            continue;  // repeated pixels are handled by backward references
          }
          ++accumulated_red_histo[(pix >> 16) & 0xff];
          ++accumulated_blue_histo[(pix >> 0) & 0xff];
        }
      }
    }
  }
}

static WebPEncodingError ApplyCrossColorFilter(const VP8LEncoder* const enc,
                                               int width, int height,
                                               int quality, int low_effort,
                                               VP8LBitWriter* const bw) {
  const int ccolor_transform_bits = enc->transform_bits_;
  const int transform_width = VP8LSubSampleSize(width, ccolor_transform_bits);
  const int transform_height = VP8LSubSampleSize(height, ccolor_transform_bits);

  VP8LColorSpaceTransform(width, height, ccolor_transform_bits, quality,
                          enc->argb_, enc->transform_data_);
  VP8LPutBits(bw, TRANSFORM_PRESENT, 1);
  VP8LPutBits(bw, CROSS_COLOR_TRANSFORM, 2);
  assert(ccolor_transform_bits >= 2);
  VP8LPutBits(bw, ccolor_transform_bits - 2, 3);
  return EncodeImageNoHuffman(
      bw, enc->transform_data_, (VP8LHashChain*)&enc->hash_chain_,
      (VP8LBackwardRefs*)&enc->refs_[0],  // cast const away
      (VP8LBackwardRefs*)&enc->refs_[1], transform_width, transform_height,
      quality, low_effort);
}

// Construct the histograms from backward references.
static void HistogramBuild(
    int xsize, int histo_bits, const VP8LBackwardRefs* const backward_refs,
    VP8LHistogramSet* const image_histo) {
  int x = 0, y = 0;
  const int histo_xsize = VP8LSubSampleSize(xsize, histo_bits);
  VP8LHistogram** const histograms = image_histo->histograms;
  VP8LRefsCursor c = VP8LRefsCursorInit(backward_refs);
  assert(histo_bits > 0);
  while (VP8LRefsCursorOk(&c)) {
    const PixOrCopy* const v = c.cur_pos;
    const int ix = (y >> histo_bits) * histo_xsize + (x >> histo_bits);
    VP8LHistogramAddSinglePixOrCopy(histograms[ix], v, NULL, 0);
    x += PixOrCopyLength(v);
    while (x >= xsize) {
      x -= xsize;
      ++y;
    }
    VP8LRefsCursorNext(&c);
  }
}

static void UpdateHistogramCost(VP8LHistogram* const h) {
  uint32_t alpha_sym, red_sym, blue_sym;
  const double alpha_cost =
      PopulationCost(h->alpha_, NUM_LITERAL_CODES, &alpha_sym);
  const double distance_cost =
      PopulationCost(h->distance_, NUM_DISTANCE_CODES, NULL) +
	  ExtraCost_C(h->distance_, NUM_DISTANCE_CODES);
  const int num_codes = VP8LHistogramNumCodes(h->palette_code_bits_);
  h->literal_cost_ = PopulationCost(h->literal_, num_codes, NULL) +
		  ExtraCost_C(h->literal_ + NUM_LITERAL_CODES,
                                   NUM_LENGTH_CODES);
  h->red_cost_ = PopulationCost(h->red_, NUM_LITERAL_CODES, &red_sym);
  h->blue_cost_ = PopulationCost(h->blue_, NUM_LITERAL_CODES, &blue_sym);
  h->bit_cost_ = h->literal_cost_ + h->red_cost_ + h->blue_cost_ +
                 alpha_cost + distance_cost;
  if ((alpha_sym | red_sym | blue_sym) == VP8L_NON_TRIVIAL_SYM) {
    h->trivial_symbol_ = VP8L_NON_TRIVIAL_SYM;
  } else {
    h->trivial_symbol_ =
        ((uint32_t)alpha_sym << 24) | (red_sym << 16) | (blue_sym << 0);
  }
}

static void HistogramCopy(const VP8LHistogram* const src,
                          VP8LHistogram* const dst) {
  uint32_t* const dst_literal = dst->literal_;
  const int dst_cache_bits = dst->palette_code_bits_;
  const int histo_size = VP8LGetHistogramSize(dst_cache_bits);
  assert(src->palette_code_bits_ == dst_cache_bits);
  memcpy(dst, src, histo_size);
  dst->literal_ = dst_literal;
}

// Copies the histograms and computes its bit_cost.
static void HistogramCopyAndAnalyze(
    VP8LHistogramSet* const orig_histo, VP8LHistogramSet* const image_histo) {
  int i;
  const int histo_size = orig_histo->size;
  VP8LHistogram** const orig_histograms = orig_histo->histograms;
  VP8LHistogram** const histograms = image_histo->histograms;
  for (i = 0; i < histo_size; ++i) {
    VP8LHistogram* const histo = orig_histograms[i];
    UpdateHistogramCost(histo);
    // Copy histograms from orig_histo[] to image_histo[].
    HistogramCopy(histo, histograms[i]);
  }
}

static double GetCombineCostFactor(int histo_size, int quality) {
  double combine_cost_factor = 0.16;
  if (quality < 90) {
    if (histo_size > 256) combine_cost_factor /= 2.;
    if (histo_size > 512) combine_cost_factor /= 2.;
    if (histo_size > 1024) combine_cost_factor /= 2.;
    if (quality <= 50) combine_cost_factor /= 2.;
  }
  return combine_cost_factor;
}

// The structure to keep track of cost range for the three dominant entropy
// symbols.
// TODO(skal): Evaluate if float can be used here instead of double for
// representing the entropy costs.
typedef struct {
  double literal_max_;
  double literal_min_;
  double red_max_;
  double red_min_;
  double blue_max_;
  double blue_min_;
} DominantCostRange;

#define MAX_COST 1.e38

static void DominantCostRangeInit(DominantCostRange* const c) {
  c->literal_max_ = 0.;
  c->literal_min_ = MAX_COST;
  c->red_max_ = 0.;
  c->red_min_ = MAX_COST;
  c->blue_max_ = 0.;
  c->blue_min_ = MAX_COST;
}

static void UpdateDominantCostRange(
    const VP8LHistogram* const h, DominantCostRange* const c) {
  if (c->literal_max_ < h->literal_cost_) c->literal_max_ = h->literal_cost_;
  if (c->literal_min_ > h->literal_cost_) c->literal_min_ = h->literal_cost_;
  if (c->red_max_ < h->red_cost_) c->red_max_ = h->red_cost_;
  if (c->red_min_ > h->red_cost_) c->red_min_ = h->red_cost_;
  if (c->blue_max_ < h->blue_cost_) c->blue_max_ = h->blue_cost_;
  if (c->blue_min_ > h->blue_cost_) c->blue_min_ = h->blue_cost_;
}

// Number of partitions for the three dominant (literal, red and blue) symbol
// costs.
#define NUM_PARTITIONS 4
// The size of the bin-hash corresponding to the three dominant costs.
#define BIN_SIZE (NUM_PARTITIONS * NUM_PARTITIONS * NUM_PARTITIONS)

static int GetBinIdForEntropy(double min, double max, double val) {
  const double range = max - min;
  if (range > 0.) {
    const double delta = val - min;
    return (int)((NUM_PARTITIONS - 1e-6) * delta / range);
  } else {
    return 0;
  }
}

static int GetHistoBinIndex(const VP8LHistogram* const h,
                            const DominantCostRange* const c, int low_effort) {
  int bin_id = GetBinIdForEntropy(c->literal_min_, c->literal_max_,
                                  h->literal_cost_);
  assert(bin_id < NUM_PARTITIONS);
  if (!low_effort) {
    bin_id = bin_id * NUM_PARTITIONS
           + GetBinIdForEntropy(c->red_min_, c->red_max_, h->red_cost_);
    bin_id = bin_id * NUM_PARTITIONS
           + GetBinIdForEntropy(c->blue_min_, c->blue_max_, h->blue_cost_);
    assert(bin_id < BIN_SIZE);
  }
  return bin_id;
}

// Partition histograms to different entropy bins for three dominant (literal,
// red and blue) symbol costs and compute the histogram aggregate bit_cost.
static void HistogramAnalyzeEntropyBin(VP8LHistogramSet* const image_histo,
                                       uint16_t* const bin_map,
                                       int low_effort) {
  int i;
  VP8LHistogram** const histograms = image_histo->histograms;
  const int histo_size = image_histo->size;
  DominantCostRange cost_range;
  DominantCostRangeInit(&cost_range);

  // Analyze the dominant (literal, red and blue) entropy costs.
  for (i = 0; i < histo_size; ++i) {
    UpdateDominantCostRange(histograms[i], &cost_range);
  }

  // bin-hash histograms on three of the dominant (literal, red and blue)
  // symbol costs and store the resulting bin_id for each histogram.
  for (i = 0; i < histo_size; ++i) {
    bin_map[i] = GetHistoBinIndex(histograms[i], &cost_range, low_effort);
  }
}

static void HistogramAdd_C(const VP8LHistogram* const a,
                           const VP8LHistogram* const b,
                           VP8LHistogram* const out) {
  int i;
  const int literal_size = VP8LHistogramNumCodes(a->palette_code_bits_);
  assert(a->palette_code_bits_ == b->palette_code_bits_);
  if (b != out) {
    for (i = 0; i < literal_size; ++i) {
      out->literal_[i] = a->literal_[i] + b->literal_[i];
    }
    for (i = 0; i < NUM_DISTANCE_CODES; ++i) {
      out->distance_[i] = a->distance_[i] + b->distance_[i];
    }
    for (i = 0; i < NUM_LITERAL_CODES; ++i) {
      out->red_[i] = a->red_[i] + b->red_[i];
      out->blue_[i] = a->blue_[i] + b->blue_[i];
      out->alpha_[i] = a->alpha_[i] + b->alpha_[i];
    }
  } else {
    for (i = 0; i < literal_size; ++i) {
      out->literal_[i] += a->literal_[i];
    }
    for (i = 0; i < NUM_DISTANCE_CODES; ++i) {
      out->distance_[i] += a->distance_[i];
    }
    for (i = 0; i < NUM_LITERAL_CODES; ++i) {
      out->red_[i] += a->red_[i];
      out->blue_[i] += a->blue_[i];
      out->alpha_[i] += a->alpha_[i];
    }
  }
}

static void GetCombinedEntropyUnrefined_C(const uint32_t X[],
                                          const uint32_t Y[],
                                          int length,
                                          VP8LBitEntropy* const bit_entropy,
                                          VP8LStreaks* const stats) {
  int i = 1;
  int i_prev = 0;
  uint32_t xy_prev = X[0] + Y[0];

  memset(stats, 0, sizeof(*stats));
  VP8LBitEntropyInit(bit_entropy);

  for (i = 1; i < length; ++i) {
    const uint32_t xy = X[i] + Y[i];
    if (xy != xy_prev) {
      GetEntropyUnrefinedHelper(xy, i, &xy_prev, &i_prev, bit_entropy, stats);
    }
  }
  GetEntropyUnrefinedHelper(0, i, &xy_prev, &i_prev, bit_entropy, stats);

  bit_entropy->entropy += VP8LFastSLog2(bit_entropy->sum);
}

// trivial_at_end is 1 if the two histograms only have one element that is
// non-zero: both the zero-th one, or both the last one.
static double GetCombinedEntropy(const uint32_t* const X,
                                             const uint32_t* const Y,
                                             int length, int trivial_at_end) {
  VP8LStreaks stats;
  if (trivial_at_end) {
    // This configuration is due to palettization that transforms an indexed
    // pixel into 0xff000000 | (pixel << 8) in VP8LBundleColorMap.
    // BitsEntropyRefine is 0 for histograms with only one non-zero value.
    // Only FinalHuffmanCost needs to be evaluated.
    memset(&stats, 0, sizeof(stats));
    // Deal with the non-zero value at index 0 or length-1.
    stats.streaks[1][0] += 1;
    // Deal with the following/previous zero streak.
    stats.counts[0] += 1;
    stats.streaks[0][1] += length - 1;
    return FinalHuffmanCost(&stats);
  } else {
    VP8LBitEntropy bit_entropy;
    GetCombinedEntropyUnrefined_C(X, Y, length, &bit_entropy, &stats);

    return BitsEntropyRefine(&bit_entropy) + FinalHuffmanCost(&stats);
  }
}

static double ExtraCostCombined_C(const uint32_t* X, const uint32_t* Y,
                                  int length) {
  int i;
  double cost = 0.;
  for (i = 2; i < length - 2; ++i) {
    const int xy = X[i + 2] + Y[i + 2];
    cost += (i >> 1) * xy;
  }
  return cost;
}

static int GetCombinedHistogramEntropy(const VP8LHistogram* const a,
                                       const VP8LHistogram* const b,
                                       double cost_threshold,
                                       double* cost) {
  const int palette_code_bits = a->palette_code_bits_;
  int trivial_at_end = 0;
  assert(a->palette_code_bits_ == b->palette_code_bits_);
  *cost += GetCombinedEntropy(a->literal_, b->literal_,
                              VP8LHistogramNumCodes(palette_code_bits), 0);
  *cost += ExtraCostCombined_C(a->literal_ + NUM_LITERAL_CODES,
                                 b->literal_ + NUM_LITERAL_CODES,
                                 NUM_LENGTH_CODES);
  if (*cost > cost_threshold) return 0;

  if (a->trivial_symbol_ != VP8L_NON_TRIVIAL_SYM &&
      a->trivial_symbol_ == b->trivial_symbol_) {
    // A, R and B are all 0 or 0xff.
    const uint32_t color_a = (a->trivial_symbol_ >> 24) & 0xff;
    const uint32_t color_r = (a->trivial_symbol_ >> 16) & 0xff;
    const uint32_t color_b = (a->trivial_symbol_ >> 0) & 0xff;
    if ((color_a == 0 || color_a == 0xff) &&
        (color_r == 0 || color_r == 0xff) &&
        (color_b == 0 || color_b == 0xff)) {
      trivial_at_end = 1;
    }
  }

  *cost +=
      GetCombinedEntropy(a->red_, b->red_, NUM_LITERAL_CODES, trivial_at_end);
  if (*cost > cost_threshold) return 0;

  *cost +=
      GetCombinedEntropy(a->blue_, b->blue_, NUM_LITERAL_CODES, trivial_at_end);
  if (*cost > cost_threshold) return 0;

  *cost += GetCombinedEntropy(a->alpha_, b->alpha_, NUM_LITERAL_CODES,
                              trivial_at_end);
  if (*cost > cost_threshold) return 0;

  *cost +=
      GetCombinedEntropy(a->distance_, b->distance_, NUM_DISTANCE_CODES, 0);
  *cost +=
		  ExtraCostCombined_C(a->distance_, b->distance_, NUM_DISTANCE_CODES);
  if (*cost > cost_threshold) return 0;

  return 1;
}

static void HistogramAdd(const VP8LHistogram* const a,
                                     const VP8LHistogram* const b,
                                     VP8LHistogram* const out) {
  HistogramAdd_C(a, b, out);
  out->trivial_symbol_ = (a->trivial_symbol_ == b->trivial_symbol_)
                       ? a->trivial_symbol_
                       : VP8L_NON_TRIVIAL_SYM;
}

// Performs out = a + b, computing the cost C(a+b) - C(a) - C(b) while comparing
// to the threshold value 'cost_threshold'. The score returned is
//  Score = C(a+b) - C(a) - C(b), where C(a) + C(b) is known and fixed.
// Since the previous score passed is 'cost_threshold', we only need to compare
// the partial cost against 'cost_threshold + C(a) + C(b)' to possibly bail-out
// early.
static double HistogramAddEval(const VP8LHistogram* const a,
                               const VP8LHistogram* const b,
                               VP8LHistogram* const out,
                               double cost_threshold) {
  double cost = 0;
  const double sum_cost = a->bit_cost_ + b->bit_cost_;
  cost_threshold += sum_cost;

  if (GetCombinedHistogramEntropy(a, b, cost_threshold, &cost)) {
    HistogramAdd(a, b, out);
    out->bit_cost_ = cost;
    out->palette_code_bits_ = a->palette_code_bits_;
  }

  return cost - sum_cost;
}

// Swap two histogram pointers.
static void HistogramSwap(VP8LHistogram** const A, VP8LHistogram** const B) {
  VP8LHistogram* const tmp = *A;
  *A = *B;
  *B = tmp;
}

// Compact image_histo[] by merging some histograms with same bin_id together if
// it's advantageous.
static void HistogramCombineEntropyBin(VP8LHistogramSet* const image_histo,
                                       VP8LHistogram* cur_combo,
                                       const uint16_t* const bin_map,
                                       int bin_map_size, int num_bins,
                                       double combine_cost_factor,
                                       int low_effort) {
  VP8LHistogram** const histograms = image_histo->histograms;
  int idx;
  // Work in-place: processed histograms are put at the beginning of
  // image_histo[]. At the end, we just have to truncate the array.
  int size = 0;
  struct {
    int16_t first;    // position of the histogram that accumulates all
                      // histograms with the same bin_id
    uint16_t num_combine_failures;   // number of combine failures per bin_id
  } bin_info[BIN_SIZE];

  assert(num_bins <= BIN_SIZE);
  for (idx = 0; idx < num_bins; ++idx) {
    bin_info[idx].first = -1;
    bin_info[idx].num_combine_failures = 0;
  }

  for (idx = 0; idx < bin_map_size; ++idx) {
    const int bin_id = bin_map[idx];
    const int first = bin_info[bin_id].first;
    assert(size <= idx);
    if (first == -1) {
      // just move histogram #idx to its final position
      histograms[size] = histograms[idx];
      bin_info[bin_id].first = size++;
    } else if (low_effort) {
      HistogramAdd(histograms[idx], histograms[first], histograms[first]);
    } else {
      // try to merge #idx into #first (both share the same bin_id)
      const double bit_cost = histograms[idx]->bit_cost_;
      const double bit_cost_thresh = -bit_cost * combine_cost_factor;
      const double curr_cost_diff =
          HistogramAddEval(histograms[first], histograms[idx],
                           cur_combo, bit_cost_thresh);
      if (curr_cost_diff < bit_cost_thresh) {
        // Try to merge two histograms only if the combo is a trivial one or
        // the two candidate histograms are already non-trivial.
        // For some images, 'try_combine' turns out to be false for a lot of
        // histogram pairs. In that case, we fallback to combining
        // histograms as usual to avoid increasing the header size.
        const int try_combine =
            (cur_combo->trivial_symbol_ != VP8L_NON_TRIVIAL_SYM) ||
            ((histograms[idx]->trivial_symbol_ == VP8L_NON_TRIVIAL_SYM) &&
             (histograms[first]->trivial_symbol_ == VP8L_NON_TRIVIAL_SYM));
        const int max_combine_failures = 32;
        if (try_combine ||
            bin_info[bin_id].num_combine_failures >= max_combine_failures) {
          // move the (better) merged histogram to its final slot
          HistogramSwap(&cur_combo, &histograms[first]);
        } else {
          histograms[size++] = histograms[idx];
          ++bin_info[bin_id].num_combine_failures;
        }
      } else {
        histograms[size++] = histograms[idx];
      }
    }
  }
  image_histo->size = size;
  if (low_effort) {
    // for low_effort case, update the final cost when everything is merged
    for (idx = 0; idx < size; ++idx) {
      UpdateHistogramCost(histograms[idx]);
    }
  }
}

// Pair of histograms. Negative idx1 value means that pair is out-of-date.
typedef struct {
  int idx1;
  int idx2;
  double cost_diff;
  double cost_combo;
} HistogramPair;

typedef struct {
  HistogramPair* queue;
  int size;
  int max_size;
} HistoQueue;

static int HistoQueueInit(HistoQueue* const histo_queue, const int max_index) {
  histo_queue->size = 0;
  // max_index^2 for the queue size is safe. If you look at
  // HistogramCombineGreedy, and imagine that UpdateQueueFront always pushes
  // data to the queue, you insert at most:
  // - max_index*(max_index-1)/2 (the first two for loops)
  // - max_index - 1 in the last for loop at the first iteration of the while
  //   loop, max_index - 2 at the second iteration ... therefore
  //   max_index*(max_index-1)/2 overall too
  histo_queue->max_size = max_index * max_index;
  // We allocate max_size + 1 because the last element at index "size" is
  // used as temporary data (and it could be up to max_size).
  histo_queue->queue = (HistogramPair*)WebPSafeMalloc(
      histo_queue->max_size + 1, sizeof(*histo_queue->queue));
  return histo_queue->queue != NULL;
}

static void HistoQueueClear(HistoQueue* const histo_queue) {
  assert(histo_queue != NULL);
  WebPSafeFree(histo_queue->queue);
  histo_queue->size = 0;
  histo_queue->max_size = 0;
}

// Pop a specific pair in the queue by replacing it with the last one
// and shrinking the queue.
static void HistoQueuePopPair(HistoQueue* const histo_queue,
                              HistogramPair* const pair) {
  assert(pair >= histo_queue->queue &&
         pair < (histo_queue->queue + histo_queue->size));
  assert(histo_queue->size > 0);
  *pair = histo_queue->queue[histo_queue->size - 1];
  --histo_queue->size;
}

// Check whether a pair in the queue should be updated as head or not.
static void HistoQueueUpdateHead(HistoQueue* const histo_queue,
                                 HistogramPair* const pair) {
  assert(pair->cost_diff < 0.);
  assert(pair >= histo_queue->queue &&
         pair < (histo_queue->queue + histo_queue->size));
  assert(histo_queue->size > 0);
  if (pair->cost_diff < histo_queue->queue[0].cost_diff) {
    // Replace the best pair.
    const HistogramPair tmp = histo_queue->queue[0];
    histo_queue->queue[0] = *pair;
    *pair = tmp;
  }
}

// Create a pair from indices "idx1" and "idx2" provided its cost
// is inferior to "threshold", a negative entropy.
// It returns the cost of the pair, or 0. if it superior to threshold.
static double HistoQueuePush(HistoQueue* const histo_queue,
                             VP8LHistogram** const histograms, int idx1,
                             int idx2, double threshold) {
  const VP8LHistogram* h1;
  const VP8LHistogram* h2;
  HistogramPair pair;
  double sum_cost;

  assert(threshold <= 0.);
  if (idx1 > idx2) {
    const int tmp = idx2;
    idx2 = idx1;
    idx1 = tmp;
  }
  pair.idx1 = idx1;
  pair.idx2 = idx2;
  h1 = histograms[idx1];
  h2 = histograms[idx2];
  sum_cost = h1->bit_cost_ + h2->bit_cost_;
  pair.cost_combo = 0.;
  GetCombinedHistogramEntropy(h1, h2, sum_cost + threshold, &pair.cost_combo);
  pair.cost_diff = pair.cost_combo - sum_cost;

  // Do not even consider the pair if it does not improve the entropy.
  if (pair.cost_diff >= threshold) return 0.;

  // We cannot add more elements than the capacity.
  assert(histo_queue->size < histo_queue->max_size);
  histo_queue->queue[histo_queue->size++] = pair;
  HistoQueueUpdateHead(histo_queue, &histo_queue->queue[histo_queue->size - 1]);

  return pair.cost_diff;
}

// Implement a Lehmer random number generator with a multiplicative constant of
// 48271 and a modulo constant of 2^31 - 1.
static uint32_t MyRand(uint32_t* const seed) {
  *seed = (uint32_t)(((uint64_t)(*seed) * 48271u) % 2147483647u);
  assert(*seed > 0);
  return *seed;
}

// Perform histogram aggregation using a stochastic approach.
// 'do_greedy' is set to 1 if a greedy approach needs to be performed
// afterwards, 0 otherwise.
static int HistogramCombineStochastic(VP8LHistogramSet* const image_histo,
                                      int min_cluster_size,
                                      int* const do_greedy) {
  int iter;
  uint32_t seed = 1;
  int tries_with_no_success = 0;
  int image_histo_size = image_histo->size;
  const int outer_iters = image_histo_size;
  const int num_tries_no_success = outer_iters / 2;
  VP8LHistogram** const histograms = image_histo->histograms;
  // Priority queue of histogram pairs. Its size of "kCostHeapSizeSqrt"^2
  // impacts the quality of the compression and the speed: the smaller the
  // faster but the worse for the compression.
  HistoQueue histo_queue;
  const int kHistoQueueSizeSqrt = 3;
  int ok = 0;

  if (!HistoQueueInit(&histo_queue, kHistoQueueSizeSqrt)) {
    goto End;
  }
  // Collapse similar histograms in 'image_histo'.
  ++min_cluster_size;
  for (iter = 0; iter < outer_iters && image_histo_size >= min_cluster_size &&
                 ++tries_with_no_success < num_tries_no_success;
       ++iter) {
    double best_cost =
        (histo_queue.size == 0) ? 0. : histo_queue.queue[0].cost_diff;
    int best_idx1 = -1, best_idx2 = 1;
    int j;
    const uint32_t rand_range = (image_histo_size - 1) * image_histo_size;
    // image_histo_size / 2 was chosen empirically. Less means faster but worse
    // compression.
    const int num_tries = image_histo_size / 2;

    for (j = 0; j < num_tries; ++j) {
      double curr_cost;
      // Choose two different histograms at random and try to combine them.
      const uint32_t tmp = MyRand(&seed) % rand_range;
      const uint32_t idx1 = tmp / (image_histo_size - 1);
      uint32_t idx2 = tmp % (image_histo_size - 1);
      if (idx2 >= idx1) ++idx2;

      // Calculate cost reduction on combination.
      curr_cost =
          HistoQueuePush(&histo_queue, histograms, idx1, idx2, best_cost);
      if (curr_cost < 0) {  // found a better pair?
        best_cost = curr_cost;
        // Empty the queue if we reached full capacity.
        if (histo_queue.size == histo_queue.max_size) break;
      }
    }
    if (histo_queue.size == 0) continue;

    // Merge the two best histograms.
    best_idx1 = histo_queue.queue[0].idx1;
    best_idx2 = histo_queue.queue[0].idx2;
    assert(best_idx1 < best_idx2);
    HistogramAddEval(histograms[best_idx1], histograms[best_idx2],
                     histograms[best_idx1], 0);
    // Swap the best_idx2 histogram with the last one (which is now unused).
    --image_histo_size;
    if (best_idx2 != image_histo_size) {
      HistogramSwap(&histograms[image_histo_size], &histograms[best_idx2]);
    }
    histograms[image_histo_size] = NULL;
    // Parse the queue and update each pair that deals with best_idx1,
    // best_idx2 or image_histo_size.
    for (j = 0; j < histo_queue.size;) {
      HistogramPair* const p = histo_queue.queue + j;
      const int is_idx1_best = p->idx1 == best_idx1 || p->idx1 == best_idx2;
      const int is_idx2_best = p->idx2 == best_idx1 || p->idx2 == best_idx2;
      int do_eval = 0;
      // The front pair could have been duplicated by a random pick so
      // check for it all the time nevertheless.
      if (is_idx1_best && is_idx2_best) {
        HistoQueuePopPair(&histo_queue, p);
        continue;
      }
      // Any pair containing one of the two best indices should only refer to
      // best_idx1. Its cost should also be updated.
      if (is_idx1_best) {
        p->idx1 = best_idx1;
        do_eval = 1;
      } else if (is_idx2_best) {
        p->idx2 = best_idx1;
        do_eval = 1;
      }
      if (p->idx2 == image_histo_size) {
        // No need to re-evaluate here as it does not involve a pair
        // containing best_idx1 or best_idx2.
        p->idx2 = best_idx2;
      }
      assert(p->idx2 < image_histo_size);
      // Make sure the index order is respected.
      if (p->idx1 > p->idx2) {
        const int tmp = p->idx2;
        p->idx2 = p->idx1;
        p->idx1 = tmp;
      }
      if (do_eval) {
        // Re-evaluate the cost of an updated pair.
        GetCombinedHistogramEntropy(histograms[p->idx1], histograms[p->idx2], 0,
                                    &p->cost_diff);
        if (p->cost_diff >= 0.) {
          HistoQueuePopPair(&histo_queue, p);
          continue;
        }
      }
      HistoQueueUpdateHead(&histo_queue, p);
      ++j;
    }

    tries_with_no_success = 0;
  }
  image_histo->size = image_histo_size;
  *do_greedy = (image_histo->size <= min_cluster_size);
  ok = 1;

End:
  HistoQueueClear(&histo_queue);
  return ok;
}

#define MAX_HISTO_GREEDY 100

// Combines histograms by continuously choosing the one with the highest cost
// reduction.
static int HistogramCombineGreedy(VP8LHistogramSet* const image_histo) {
  int ok = 0;
  int image_histo_size = image_histo->size;
  int i, j;
  VP8LHistogram** const histograms = image_histo->histograms;
  // Indexes of remaining histograms.
  int* const clusters =
      (int*)WebPSafeMalloc(image_histo_size, sizeof(*clusters));
  // Priority queue of histogram pairs.
  HistoQueue histo_queue;

  if (!HistoQueueInit(&histo_queue, image_histo_size) || clusters == NULL) {
    goto End;
  }

  for (i = 0; i < image_histo_size; ++i) {
    // Initialize clusters indexes.
    clusters[i] = i;
    for (j = i + 1; j < image_histo_size; ++j) {
      // Initialize positions array.
      HistoQueuePush(&histo_queue, histograms, i, j, 0.);
    }
  }

  while (image_histo_size > 1 && histo_queue.size > 0) {
    const int idx1 = histo_queue.queue[0].idx1;
    const int idx2 = histo_queue.queue[0].idx2;
    HistogramAdd(histograms[idx2], histograms[idx1], histograms[idx1]);
    histograms[idx1]->bit_cost_ = histo_queue.queue[0].cost_combo;
    // Remove merged histogram.
    for (i = 0; i + 1 < image_histo_size; ++i) {
      if (clusters[i] >= idx2) {
        clusters[i] = clusters[i + 1];
      }
    }
    --image_histo_size;

    // Remove pairs intersecting the just combined best pair.
    for (i = 0; i < histo_queue.size;) {
      HistogramPair* const p = histo_queue.queue + i;
      if (p->idx1 == idx1 || p->idx2 == idx1 ||
          p->idx1 == idx2 || p->idx2 == idx2) {
        HistoQueuePopPair(&histo_queue, p);
      } else {
        HistoQueueUpdateHead(&histo_queue, p);
        ++i;
      }
    }

    // Push new pairs formed with combined histogram to the queue.
    for (i = 0; i < image_histo_size; ++i) {
      if (clusters[i] != idx1) {
        HistoQueuePush(&histo_queue, histograms, idx1, clusters[i], 0.);
      }
    }
  }
  // Move remaining histograms to the beginning of the array.
  for (i = 0; i < image_histo_size; ++i) {
    if (i != clusters[i]) {  // swap the two histograms
      HistogramSwap(&histograms[i], &histograms[clusters[i]]);
    }
  }

  image_histo->size = image_histo_size;
  ok = 1;

 End:
  WebPSafeFree(clusters);
  HistoQueueClear(&histo_queue);
  return ok;
}

// Same as HistogramAddEval(), except that the resulting histogram
// is not stored. Only the cost C(a+b) - C(a) is evaluated. We omit
// the term C(b) which is constant over all the evaluations.
static double HistogramAddThresh(const VP8LHistogram* const a,
                                 const VP8LHistogram* const b,
                                 double cost_threshold) {
  double cost = -a->bit_cost_;
  GetCombinedHistogramEntropy(a, b, cost_threshold, &cost);
  return cost;
}

// Find the best 'out' histogram for each of the 'in' histograms.
// Note: we assume that out[]->bit_cost_ is already up-to-date.
static void HistogramRemap(const VP8LHistogramSet* const in,
                           const VP8LHistogramSet* const out,
                           uint16_t* const symbols) {
  int i;
  VP8LHistogram** const in_histo = in->histograms;
  VP8LHistogram** const out_histo = out->histograms;
  const int in_size = in->size;
  const int out_size = out->size;
  if (out_size > 1) {
    for (i = 0; i < in_size; ++i) {
      int best_out = 0;
      double best_bits = MAX_COST;
      int k;
      for (k = 0; k < out_size; ++k) {
        const double cur_bits =
            HistogramAddThresh(out_histo[k], in_histo[i], best_bits);
        if (k == 0 || cur_bits < best_bits) {
          best_bits = cur_bits;
          best_out = k;
        }
      }
      symbols[i] = best_out;
    }
  } else {
    assert(out_size == 1);
    for (i = 0; i < in_size; ++i) {
      symbols[i] = 0;
    }
  }

  // Recompute each out based on raw and symbols.
  for (i = 0; i < out_size; ++i) {
    HistogramClear(out_histo[i]);
  }

  for (i = 0; i < in_size; ++i) {
    const int idx = symbols[i];
    HistogramAdd(in_histo[i], out_histo[idx], out_histo[idx]);
  }
}

int VP8LGetHistoImageSymbols(int xsize, int ysize,
                             const VP8LBackwardRefs* const refs,
                             int quality, int low_effort,
                             int histo_bits, int cache_bits,
                             VP8LHistogramSet* const image_histo,
                             VP8LHistogram* const tmp_histo,
                             uint16_t* const histogram_symbols) {
  int ok = 0;
  const int histo_xsize = histo_bits ? VP8LSubSampleSize(xsize, histo_bits) : 1;
  const int histo_ysize = histo_bits ? VP8LSubSampleSize(ysize, histo_bits) : 1;
  const int image_histo_raw_size = histo_xsize * histo_ysize;
  VP8LHistogramSet* const orig_histo =
      VP8LAllocateHistogramSet(image_histo_raw_size, cache_bits);
  // Don't attempt linear bin-partition heuristic for
  // histograms of small sizes (as bin_map will be very sparse) and
  // maximum quality q==100 (to preserve the compression gains at that level).
  const int entropy_combine_num_bins = low_effort ? NUM_PARTITIONS : BIN_SIZE;
  const int entropy_combine =
      (orig_histo->size > entropy_combine_num_bins * 2) && (quality < 100);

  if (orig_histo == NULL) goto Error;

  // Construct the histograms from backward references.
  HistogramBuild(xsize, histo_bits, refs, orig_histo);
  // Copies the histograms and computes its bit_cost.
  HistogramCopyAndAnalyze(orig_histo, image_histo);

  if (entropy_combine) {
    const int bin_map_size = orig_histo->size;
    // Reuse histogram_symbols storage. By definition, it's guaranteed to be ok.
    uint16_t* const bin_map = histogram_symbols;
    const double combine_cost_factor =
        GetCombineCostFactor(image_histo_raw_size, quality);

    HistogramAnalyzeEntropyBin(orig_histo, bin_map, low_effort);
    // Collapse histograms with similar entropy.
    HistogramCombineEntropyBin(image_histo, tmp_histo, bin_map, bin_map_size,
                               entropy_combine_num_bins, combine_cost_factor,
                               low_effort);
  }

  // Don't combine the histograms using stochastic and greedy heuristics for
  // low-effort compression mode.
  if (!low_effort || !entropy_combine) {
    const float x = quality / 100.f;
    // cubic ramp between 1 and MAX_HISTO_GREEDY:
    const int threshold_size = (int)(1 + (x * x * x) * (MAX_HISTO_GREEDY - 1));
    int do_greedy;
    if (!HistogramCombineStochastic(image_histo, threshold_size, &do_greedy)) {
      goto Error;
    }
    if (do_greedy && !HistogramCombineGreedy(image_histo)) {
      goto Error;
    }
  }

  // TODO(vrabaud): Optimize HistogramRemap for low-effort compression mode.
  // Find the optimal map from original histograms to the final ones.
  HistogramRemap(orig_histo, image_histo, histogram_symbols);

  ok = 1;

 Error:
  VP8LFreeHistogramSet(orig_histo);
  return ok;
}

void VP8LBitWriterSwap(VP8LBitWriter* const src, VP8LBitWriter* const dst) {
  const VP8LBitWriter tmp = *src;
  *src = *dst;
  *dst = tmp;
}

void VP8LBitWriterWipeOut(VP8LBitWriter* const bw) {
  if (bw != NULL) {
    WebPSafeFree(bw->buf_);
    memset(bw, 0, sizeof(*bw));
  }
}

void VP8LBitWriterReset(const VP8LBitWriter* const bw_init,
                        VP8LBitWriter* const bw) {
  bw->bits_ = bw_init->bits_;
  bw->used_ = bw_init->used_;
  bw->cur_ = bw->buf_ + (bw_init->cur_ - bw_init->buf_);
  assert(bw->cur_ <= bw->end_);
  bw->error_ = bw_init->error_;
}

static WebPEncodingError EncodeImageInternal(
    VP8LBitWriter* const bw, const uint32_t* const argb,
    VP8LHashChain* const hash_chain, VP8LBackwardRefs refs_array[3], int width,
    int height, int quality, int low_effort, int use_cache,
    const CrunchConfig* const config, int* cache_bits, int histogram_bits,
    size_t init_byte_position, int* const hdr_size, int* const data_size) {
  WebPEncodingError err = VP8_ENC_OK;
  const uint32_t histogram_image_xysize =
      VP8LSubSampleSize(width, histogram_bits) *
      VP8LSubSampleSize(height, histogram_bits);
  VP8LHistogramSet* histogram_image = NULL;
  VP8LHistogram* tmp_histo = NULL;
  int histogram_image_size = 0;
  size_t bit_array_size = 0;
  HuffmanTree* const huff_tree = (HuffmanTree*)WebPSafeMalloc(
      3ULL * CODE_LENGTH_CODES, sizeof(*huff_tree));
  HuffmanTreeToken* tokens = NULL;
  HuffmanTreeCode* huffman_codes = NULL;
  VP8LBackwardRefs* refs_best;
  VP8LBackwardRefs* refs_tmp;
  uint16_t* const histogram_symbols =
      (uint16_t*)WebPSafeMalloc(histogram_image_xysize,
                                sizeof(*histogram_symbols));
  int lz77s_idx;
  VP8LBitWriter bw_init = *bw, bw_best;
  int hdr_size_tmp;
  assert(histogram_bits >= MIN_HUFFMAN_BITS);
  assert(histogram_bits <= MAX_HUFFMAN_BITS);
  assert(hdr_size != NULL);
  assert(data_size != NULL);

  if (histogram_symbols == NULL) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  if (use_cache) {
    // If the value is different from zero, it has been set during the
    // palette analysis.
    if (*cache_bits == 0) *cache_bits = MAX_COLOR_CACHE_BITS;
  } else {
    *cache_bits = 0;
  }
  // 'best_refs' is the reference to the best backward refs and points to one
  // of refs_array[0] or refs_array[1].
  // Calculate backward references from ARGB image.
  if (huff_tree == NULL ||
      !VP8LHashChainFill(hash_chain, quality, argb, width, height,
                         low_effort) ||
      !VP8LBitWriterInit(&bw_best, 0) ||
      (config->lz77s_types_to_try_size_ > 1 &&
       !VP8LBitWriterClone(bw, &bw_best))) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }
  for (lz77s_idx = 0; lz77s_idx < config->lz77s_types_to_try_size_;
       ++lz77s_idx) {
    refs_best = VP8LGetBackwardReferences(
        width, height, argb, quality, low_effort,
        config->lz77s_types_to_try_[lz77s_idx], cache_bits, hash_chain,
        &refs_array[0], &refs_array[1]);
    if (refs_best == NULL) {
      err = VP8_ENC_ERROR_OUT_OF_MEMORY;
      goto Error;
    }
    // Keep the best references aside and use the other element from the first
    // two as a temporary for later usage.
    refs_tmp = &refs_array[refs_best == &refs_array[0] ? 1 : 0];

    histogram_image =
        VP8LAllocateHistogramSet(histogram_image_xysize, *cache_bits);
    tmp_histo = VP8LAllocateHistogram(*cache_bits);
    if (histogram_image == NULL || tmp_histo == NULL) {
      err = VP8_ENC_ERROR_OUT_OF_MEMORY;
      goto Error;
    }

    // Build histogram image and symbols from backward references.
    if (!VP8LGetHistoImageSymbols(width, height, refs_best, quality, low_effort,
                                  histogram_bits, *cache_bits, histogram_image,
                                  tmp_histo, histogram_symbols)) {
      err = VP8_ENC_ERROR_OUT_OF_MEMORY;
      goto Error;
    }
    // Create Huffman bit lengths and codes for each histogram image.
    histogram_image_size = histogram_image->size;
    bit_array_size = 5 * histogram_image_size;
    huffman_codes = (HuffmanTreeCode*)WebPSafeCalloc(bit_array_size,
                                                     sizeof(*huffman_codes));
    // Note: some histogram_image entries may point to tmp_histos[], so the
    // latter need to outlive the following call to GetHuffBitLengthsAndCodes().
    if (huffman_codes == NULL ||
        !GetHuffBitLengthsAndCodes(histogram_image, huffman_codes)) {
      err = VP8_ENC_ERROR_OUT_OF_MEMORY;
      goto Error;
    }
    // Free combined histograms.
    VP8LFreeHistogramSet(histogram_image);
    histogram_image = NULL;

    // Free scratch histograms.
    VP8LFreeHistogram(tmp_histo);
    tmp_histo = NULL;

    // Color Cache parameters.
    if (*cache_bits > 0) {
      VP8LPutBits(bw, 1, 1);
      VP8LPutBits(bw, *cache_bits, 4);
    } else {
      VP8LPutBits(bw, 0, 1);
    }

    // Huffman image + meta huffman.
    {
      const int write_histogram_image = (histogram_image_size > 1);
      VP8LPutBits(bw, write_histogram_image, 1);
      if (write_histogram_image) {
        uint32_t* const histogram_argb =
            (uint32_t*)WebPSafeMalloc(histogram_image_xysize,
                                      sizeof(*histogram_argb));
        int max_index = 0;
        uint32_t i;
        if (histogram_argb == NULL) {
          err = VP8_ENC_ERROR_OUT_OF_MEMORY;
          goto Error;
        }
        for (i = 0; i < histogram_image_xysize; ++i) {
          const int symbol_index = histogram_symbols[i] & 0xffff;
          histogram_argb[i] = (symbol_index << 8);
          if (symbol_index >= max_index) {
            max_index = symbol_index + 1;
          }
        }
        histogram_image_size = max_index;

        VP8LPutBits(bw, histogram_bits - 2, 3);
        err = EncodeImageNoHuffman(
            bw, histogram_argb, hash_chain, refs_tmp, &refs_array[2],
            VP8LSubSampleSize(width, histogram_bits),
            VP8LSubSampleSize(height, histogram_bits), quality, low_effort);
        WebPSafeFree(histogram_argb);
        if (err != VP8_ENC_OK) goto Error;
      }
    }

    // Store Huffman codes.
    {
      int i;
      int max_tokens = 0;
      // Find maximum number of symbols for the huffman tree-set.
      for (i = 0; i < 5 * histogram_image_size; ++i) {
        HuffmanTreeCode* const codes = &huffman_codes[i];
        if (max_tokens < codes->num_symbols) {
          max_tokens = codes->num_symbols;
        }
      }
      tokens = (HuffmanTreeToken*)WebPSafeMalloc(max_tokens, sizeof(*tokens));
      if (tokens == NULL) {
        err = VP8_ENC_ERROR_OUT_OF_MEMORY;
        goto Error;
      }
      for (i = 0; i < 5 * histogram_image_size; ++i) {
        HuffmanTreeCode* const codes = &huffman_codes[i];
        StoreHuffmanCode(bw, huff_tree, tokens, codes);
        ClearHuffmanTreeIfOnlyOneSymbol(codes);
      }
    }
    // Store actual literals.
    hdr_size_tmp = (int)(VP8LBitWriterNumBytes(bw) - init_byte_position);
    err = StoreImageToBitMask(bw, width, histogram_bits, refs_best,
                              histogram_symbols, huffman_codes);
    // Keep track of the smallest image so far.
    if (lz77s_idx == 0 ||
        VP8LBitWriterNumBytes(bw) < VP8LBitWriterNumBytes(&bw_best)) {
      *hdr_size = hdr_size_tmp;
      *data_size =
          (int)(VP8LBitWriterNumBytes(bw) - init_byte_position - *hdr_size);
      VP8LBitWriterSwap(bw, &bw_best);
    }
    // Reset the bit writer for the following iteration if any.
    if (config->lz77s_types_to_try_size_ > 1) VP8LBitWriterReset(&bw_init, bw);
    WebPSafeFree(tokens);
    tokens = NULL;
    if (huffman_codes != NULL) {
      WebPSafeFree(huffman_codes->codes);
      WebPSafeFree(huffman_codes);
      huffman_codes = NULL;
    }
  }
  VP8LBitWriterSwap(bw, &bw_best);

 Error:
  WebPSafeFree(tokens);
  WebPSafeFree(huff_tree);
  VP8LFreeHistogramSet(histogram_image);
  VP8LFreeHistogram(tmp_histo);
  if (huffman_codes != NULL) {
    WebPSafeFree(huffman_codes->codes);
    WebPSafeFree(huffman_codes);
  }
  WebPSafeFree(histogram_symbols);
  VP8LBitWriterWipeOut(&bw_best);
  return err;
}

static int EncodeStreamHook(void* input, void* data2) {
  StreamEncodeContext* const params = (StreamEncodeContext*)input;
  const WebPConfig* const config = params->config_;
  const WebPPicture* const picture = params->picture_;
  VP8LBitWriter* const bw = params->bw_;
  VP8LEncoder* const enc = params->enc_;
  const int use_cache = params->use_cache_;
  const CrunchConfig* const crunch_configs = params->crunch_configs_;
  const int num_crunch_configs = params->num_crunch_configs_;
  const int red_and_blue_always_zero = params->red_and_blue_always_zero_;
#if !defined(WEBP_DISABLE_STATS)
  WebPAuxStats* const stats = params->stats_;
#endif
  WebPEncodingError err = VP8_ENC_OK;
  const int quality = (int)config->quality;
  const int low_effort = (config->method == 0);
#if (WEBP_NEAR_LOSSLESS == 1)
  const int width = picture->width;
#endif
  const int height = picture->height;
  const size_t byte_position = VP8LBitWriterNumBytes(bw);
#if (WEBP_NEAR_LOSSLESS == 1)
  int use_near_lossless = 0;
#endif
  int hdr_size = 0;
  int data_size = 0;
  int use_delta_palette = 0;
  int idx;
  size_t best_size = 0;
  VP8LBitWriter bw_init = *bw, bw_best;
  (void)data2;

  if (!VP8LBitWriterInit(&bw_best, 0) ||
      (num_crunch_configs > 1 && !VP8LBitWriterClone(bw, &bw_best))) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  for (idx = 0; idx < num_crunch_configs; ++idx) {
    const int entropy_idx = crunch_configs[idx].entropy_idx_;
    enc->use_palette_ = (entropy_idx == kPalette);
    enc->use_subtract_green_ =
        (entropy_idx == kSubGreen) || (entropy_idx == kSpatialSubGreen);
    enc->use_predict_ =
        (entropy_idx == kSpatial) || (entropy_idx == kSpatialSubGreen);
    if (low_effort) {
      enc->use_cross_color_ = 0;
    } else {
      enc->use_cross_color_ = red_and_blue_always_zero ? 0 : enc->use_predict_;
    }
    // Reset any parameter in the encoder that is set in the previous iteration.
    enc->cache_bits_ = 0;
    VP8LBackwardRefsClear(&enc->refs_[0]);
    VP8LBackwardRefsClear(&enc->refs_[1]);

#if (WEBP_NEAR_LOSSLESS == 1)
    // Apply near-lossless preprocessing.
    use_near_lossless = (config->near_lossless < 100) && !enc->use_palette_ &&
                        !enc->use_predict_;
    if (use_near_lossless) {
      err = AllocateTransformBuffer(enc, width, height);
      if (err != VP8_ENC_OK) goto Error;
      if ((enc->argb_content_ != kEncoderNearLossless) &&
          !VP8ApplyNearLossless(picture, config->near_lossless, enc->argb_)) {
        err = VP8_ENC_ERROR_OUT_OF_MEMORY;
        goto Error;
      }
      enc->argb_content_ = kEncoderNearLossless;
    } else {
      enc->argb_content_ = kEncoderNone;
    }
#else
    enc->argb_content_ = kEncoderNone;
#endif

    // Encode palette
    if (enc->use_palette_) {
      err = EncodePalette(bw, low_effort, enc);
      if (err != VP8_ENC_OK) goto Error;
      err = MapImageFromPalette(enc, use_delta_palette);
      if (err != VP8_ENC_OK) goto Error;
      // If using a color cache, do not have it bigger than the number of
      // colors.
      if (use_cache && enc->palette_size_ < (1 << MAX_COLOR_CACHE_BITS)) {
        enc->cache_bits_ = BitsLog2Floor(enc->palette_size_) + 1;
      }
    }
    if (!use_delta_palette) {
      // In case image is not packed.
      if (enc->argb_content_ != kEncoderNearLossless &&
          enc->argb_content_ != kEncoderPalette) {
        err = MakeInputImageCopy(enc);
        if (err != VP8_ENC_OK) goto Error;
      }

      // -----------------------------------------------------------------------
      // Apply transforms and write transform data.

      if (enc->use_subtract_green_) {
        ApplySubtractGreen(enc, enc->current_width_, height, bw);
      }

      if (enc->use_predict_) {
        err = ApplyPredictFilter(enc, enc->current_width_, height, quality,
                                 low_effort, enc->use_subtract_green_, bw);
        if (err != VP8_ENC_OK) goto Error;
      }

      if (enc->use_cross_color_) {
        err = ApplyCrossColorFilter(enc, enc->current_width_, height, quality,
                                    low_effort, bw);
        if (err != VP8_ENC_OK) goto Error;
      }
    }

    VP8LPutBits(bw, !TRANSFORM_PRESENT, 1);  // No more transforms.

    // -------------------------------------------------------------------------
    // Encode and write the transformed image.
    err = EncodeImageInternal(bw, enc->argb_, &enc->hash_chain_, enc->refs_,
                              enc->current_width_, height, quality, low_effort,
                              use_cache, &crunch_configs[idx],
                              &enc->cache_bits_, enc->histo_bits_,
                              byte_position, &hdr_size, &data_size);
    if (err != VP8_ENC_OK) goto Error;

    // If we are better than what we already have.
    if (idx == 0 || VP8LBitWriterNumBytes(bw) < best_size) {
      best_size = VP8LBitWriterNumBytes(bw);
      // Store the BitWriter.
      VP8LBitWriterSwap(bw, &bw_best);
#if !defined(WEBP_DISABLE_STATS)
      // Update the stats.
      if (stats != NULL) {
        stats->lossless_features = 0;
        if (enc->use_predict_) stats->lossless_features |= 1;
        if (enc->use_cross_color_) stats->lossless_features |= 2;
        if (enc->use_subtract_green_) stats->lossless_features |= 4;
        if (enc->use_palette_) stats->lossless_features |= 8;
        stats->histogram_bits = enc->histo_bits_;
        stats->transform_bits = enc->transform_bits_;
        stats->cache_bits = enc->cache_bits_;
        stats->palette_size = enc->palette_size_;
        stats->lossless_size = (int)(best_size - byte_position);
        stats->lossless_hdr_size = hdr_size;
        stats->lossless_data_size = data_size;
      }
#endif
    }
    // Reset the bit writer for the following iteration if any.
    if (num_crunch_configs > 1) VP8LBitWriterReset(&bw_init, bw);
  }
  VP8LBitWriterSwap(&bw_best, bw);

Error:
  VP8LBitWriterWipeOut(&bw_best);
  params->err_ = err;
  // The hook should return false in case of error.
  return (err == VP8_ENC_OK);
}

static void VP8LEncoderDelete(VP8LEncoder* enc) {
  if (enc != NULL) {
    int i;
    VP8LHashChainClear(&enc->hash_chain_);
    for (i = 0; i < 3; ++i) VP8LBackwardRefsClear(&enc->refs_[i]);
    ClearTransformBuffer(enc);
    WebPSafeFree(enc);
  }
}

WebPEncodingError VP8LEncodeStream(const WebPConfig* const config,
                                   const WebPPicture* const picture,
                                   VP8LBitWriter* const bw_main,
                                   int use_cache) {
  WebPEncodingError err = VP8_ENC_OK;
  VP8LEncoder* const enc_main = VP8LEncoderNew(config, picture);
  VP8LEncoder* enc_side = NULL;
  CrunchConfig crunch_configs[CRUNCH_CONFIGS_MAX];
  int num_crunch_configs_main, num_crunch_configs_side = 0;
  int idx;
  int red_and_blue_always_zero = 0;
  WebPWorker worker_main, worker_side;
  StreamEncodeContext params_main, params_side;
  // The main thread uses picture->stats, the side thread uses stats_side.
  WebPAuxStats stats_side;
  VP8LBitWriter bw_side;
  const WebPWorkerInterface* const worker_interface = WebPGetWorkerInterface();
  int ok_main;

  // Analyze image (entropy, num_palettes etc)
  if (enc_main == NULL ||
      !EncoderAnalyze(enc_main, crunch_configs, &num_crunch_configs_main,
                      &red_and_blue_always_zero) ||
      !EncoderInit(enc_main) || !VP8LBitWriterInit(&bw_side, 0)) {
    err = VP8_ENC_ERROR_OUT_OF_MEMORY;
    goto Error;
  }

  // Split the configs between the main and side threads (if any).
  if (config->thread_level > 0) {
    num_crunch_configs_side = num_crunch_configs_main / 2;
    for (idx = 0; idx < num_crunch_configs_side; ++idx) {
      params_side.crunch_configs_[idx] =
          crunch_configs[num_crunch_configs_main - num_crunch_configs_side +
                         idx];
    }
    params_side.num_crunch_configs_ = num_crunch_configs_side;
  }
  num_crunch_configs_main -= num_crunch_configs_side;
  for (idx = 0; idx < num_crunch_configs_main; ++idx) {
    params_main.crunch_configs_[idx] = crunch_configs[idx];
  }
  params_main.num_crunch_configs_ = num_crunch_configs_main;

  // Fill in the parameters for the thread workers.
  {
    const int params_size = (num_crunch_configs_side > 0) ? 2 : 1;
    for (idx = 0; idx < params_size; ++idx) {
      // Create the parameters for each worker.
      WebPWorker* const worker = (idx == 0) ? &worker_main : &worker_side;
      StreamEncodeContext* const param =
          (idx == 0) ? &params_main : &params_side;
      param->config_ = config;
      param->picture_ = picture;
      param->use_cache_ = use_cache;
      param->red_and_blue_always_zero_ = red_and_blue_always_zero;
      if (idx == 0) {
        param->stats_ = picture->stats;
        param->bw_ = bw_main;
        param->enc_ = enc_main;
      } else {
        param->stats_ = (picture->stats == NULL) ? NULL : &stats_side;
        // Create a side bit writer.
        if (!VP8LBitWriterClone(bw_main, &bw_side)) {
          err = VP8_ENC_ERROR_OUT_OF_MEMORY;
          goto Error;
        }
        param->bw_ = &bw_side;
        // Create a side encoder.
        enc_side = VP8LEncoderNew(config, picture);
        if (enc_side == NULL || !EncoderInit(enc_side)) {
          err = VP8_ENC_ERROR_OUT_OF_MEMORY;
          goto Error;
        }
        // Copy the values that were computed for the main encoder.
        enc_side->histo_bits_ = enc_main->histo_bits_;
        enc_side->transform_bits_ = enc_main->transform_bits_;
        enc_side->palette_size_ = enc_main->palette_size_;
        memcpy(enc_side->palette_, enc_main->palette_,
               sizeof(enc_main->palette_));
        param->enc_ = enc_side;
      }
      // Create the workers.
      worker_interface->Init(worker);
      worker->data1 = param;
      worker->data2 = NULL;
      worker->hook = EncodeStreamHook;
    }
  }

  // Start the second thread if needed.
  if (num_crunch_configs_side != 0) {
    if (!worker_interface->Reset(&worker_side)) {
      err = VP8_ENC_ERROR_OUT_OF_MEMORY;
      goto Error;
    }
#if !defined(WEBP_DISABLE_STATS)
    // This line is here and not in the param initialization above to remove a
    // Clang static analyzer warning.
    if (picture->stats != NULL) {
      memcpy(&stats_side, picture->stats, sizeof(stats_side));
    }
#endif
    // This line is only useful to remove a Clang static analyzer warning.
    params_side.err_ = VP8_ENC_OK;
    worker_interface->Launch(&worker_side);
  }
  // Execute the main thread.
  worker_interface->Execute(&worker_main);
  ok_main = worker_interface->Sync(&worker_main);
  worker_interface->End(&worker_main);
  if (num_crunch_configs_side != 0) {
    // Wait for the second thread.
    const int ok_side = worker_interface->Sync(&worker_side);
    worker_interface->End(&worker_side);
    if (!ok_main || !ok_side) {
      err = ok_main ? params_side.err_ : params_main.err_;
      goto Error;
    }
    if (VP8LBitWriterNumBytes(&bw_side) < VP8LBitWriterNumBytes(bw_main)) {
      VP8LBitWriterSwap(bw_main, &bw_side);
#if !defined(WEBP_DISABLE_STATS)
      if (picture->stats != NULL) {
        memcpy(picture->stats, &stats_side, sizeof(*picture->stats));
      }
#endif
    }
  } else {
    if (!ok_main) {
      err = params_main.err_;
      goto Error;
    }
  }

Error:
  VP8LBitWriterWipeOut(&bw_side);
  VP8LEncoderDelete(enc_main);
  VP8LEncoderDelete(enc_side);
  return err;
}

#undef CRUNCH_CONFIGS_MAX
#undef CRUNCH_CONFIGS_LZ77_MAX

static int EncodeLossless(const uint8_t* const data, int width, int height,
                          int effort_level,  // in [0..6] range
                          int use_quality_100, VP8LBitWriter* const bw,
                          WebPAuxStats* const stats) {
  int ok = 0;
  WebPConfig config;
  WebPPicture picture;

  WebPPictureInit(&picture);
  picture.width = width;
  picture.height = height;
  picture.use_argb = 1;
  picture.stats = stats;
  if (!WebPPictureAlloc(&picture)) return 0;

  // Transfer the alpha values to the green channel.
  DispatchAlphaToGreen_C(data, width, picture.width, picture.height,
                           picture.argb, picture.argb_stride);

  WebPConfigInit(&config);
  config.lossless = 1;
  // Enable exact, or it would alter RGB values of transparent alpha, which is
  // normally OK but not here since we are not encoding the input image but  an
  // internal encoding-related image containing necessary exact information in
  // RGB channels.
  config.exact = 1;
  config.method = effort_level;  // impact is very small
  // Set a low default quality for encoding alpha. Ensure that Alpha quality at
  // lower methods (3 and below) is less than the threshold for triggering
  // costly 'BackwardReferencesTraceBackwards'.
  // If the alpha quality is set to 100 and the method to 6, allow for a high
  // lossless quality to trigger the cruncher.
  config.quality =
      (use_quality_100 && effort_level == 6) ? 100 : 8.f * effort_level;
  assert(config.quality >= 0 && config.quality <= 100.f);

  // TODO(urvang): Temporary fix to avoid generating images that trigger
  // a decoder bug related to alpha with color cache.
  // See: https://code.google.com/p/webp/issues/detail?id=239
  // Need to re-enable this later.
  ok = (VP8LEncodeStream(&config, &picture, bw, 0 /*use_cache*/) == VP8_ENC_OK);
  WebPPictureFree(&picture);
  ok = ok && !bw->error_;
  if (!ok) {
    VP8LBitWriterWipeOut(bw);
    return 0;
  }
  return 1;
}

uint8_t* VP8LBitWriterFinish(VP8LBitWriter* const bw) {
  // flush leftover bits
  if (VP8LBitWriterResize(bw, (bw->used_ + 7) >> 3)) {
    while (bw->used_ > 0) {
      *bw->cur_++ = (uint8_t)bw->bits_;
      bw->bits_ >>= 8;
      bw->used_ -= 8;
    }
    bw->used_ = 0;
  }
  return bw->buf_;
}

#define ALPHA_PREPROCESSED_LEVELS   1

int VP8BitWriterAppend(VP8BitWriter* const bw,
                       const uint8_t* data, size_t size) {
  assert(data != NULL);
  if (bw->nb_bits_ != -8) return 0;   // Flush() must have been called
  if (!BitWriterResize(bw, size)) return 0;
  memcpy(bw->buf_ + bw->pos_, data, size);
  bw->pos_ += size;
  return 1;
}

// Returns the size of the internal buffer.
static size_t VP8BitWriterSize(const VP8BitWriter* const bw) {
  return bw->pos_;
}

// This function always returns an initialized 'bw' object, even upon error.
static int EncodeAlphaInternal(const uint8_t* const data, int width, int height,
                               int method, int filter, int reduce_levels,
                               int effort_level,  // in [0..6] range
                               uint8_t* const tmp_alpha,
                               FilterTrial* result) {
  int ok = 0;
  const uint8_t* alpha_src;
  WebPFilterFunc filter_func;
  uint8_t header;
  const size_t data_size = width * height;
  const uint8_t* output = NULL;
  size_t output_size = 0;
  VP8LBitWriter tmp_bw;

  assert((uint64_t)data_size == (uint64_t)width * height);  // as per spec
  assert(filter >= 0 && filter < WEBP_FILTER_LAST);
  assert(method >= ALPHA_NO_COMPRESSION);
  assert(method <= ALPHA_LOSSLESS_COMPRESSION);
  assert(sizeof(header) == ALPHA_HEADER_LEN);

  WebPFilterFunc WebPFilters[WEBP_FILTER_LAST];
  WebPFilters[WEBP_FILTER_NONE] = NULL;
  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_C;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_C;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_C;

  filter_func = WebPFilters[filter];
  if (filter_func != NULL) {
    filter_func(data, width, height, width, tmp_alpha);
    alpha_src = tmp_alpha;
  }  else {
    alpha_src = data;
  }

  if (method != ALPHA_NO_COMPRESSION) {
    ok = VP8LBitWriterInit(&tmp_bw, data_size >> 3);
    ok = ok && EncodeLossless(alpha_src, width, height, effort_level,
                              !reduce_levels, &tmp_bw, &result->stats);
    if (ok) {
      output = VP8LBitWriterFinish(&tmp_bw);
      output_size = VP8LBitWriterNumBytes(&tmp_bw);
      if (output_size > data_size) {
        // compressed size is larger than source! Revert to uncompressed mode.
        method = ALPHA_NO_COMPRESSION;
        VP8LBitWriterWipeOut(&tmp_bw);
      }
    } else {
      VP8LBitWriterWipeOut(&tmp_bw);
      return 0;
    }
  }

  if (method == ALPHA_NO_COMPRESSION) {
    output = alpha_src;
    output_size = data_size;
    ok = 1;
  }

  // Emit final result.
  header = method | (filter << 2);
  if (reduce_levels) header |= ALPHA_PREPROCESSED_LEVELS << 4;

  VP8BitWriterInit(&result->bw, ALPHA_HEADER_LEN + output_size);
  ok = ok && VP8BitWriterAppend(&result->bw, &header, ALPHA_HEADER_LEN);
  ok = ok && VP8BitWriterAppend(&result->bw, output, output_size);

  if (method != ALPHA_NO_COMPRESSION) {
    VP8LBitWriterWipeOut(&tmp_bw);
  }
  ok = ok && !result->bw.error_;
  result->score = VP8BitWriterSize(&result->bw);
  return ok;
}

void VP8BitWriterWipeOut(VP8BitWriter* const bw) {
  if (bw != NULL) {
    WebPSafeFree(bw->buf_);
    memset(bw, 0, sizeof(*bw));
  }
}

// Returns a pointer to the internal buffer.
static uint8_t* VP8BitWriterBuf(const VP8BitWriter* const bw) {
  return bw->buf_;
}

static int ApplyFiltersAndEncode(const uint8_t* alpha, int width, int height,
                                 size_t data_size, int method, int filter,
                                 int reduce_levels, int effort_level,
                                 uint8_t** const output,
                                 size_t* const output_size,
                                 WebPAuxStats* const stats) {
  int ok = 1;
  FilterTrial best;
  uint32_t try_map =
      GetFilterMap(alpha, width, height, filter, effort_level);
  InitFilterTrial(&best);

  if (try_map != FILTER_TRY_NONE) {
    uint8_t* filtered_alpha =  (uint8_t*)WebPSafeMalloc(1ULL, data_size);
    if (filtered_alpha == NULL) return 0;

    for (filter = WEBP_FILTER_NONE; ok && try_map; ++filter, try_map >>= 1) {
      if (try_map & 1) {
        FilterTrial trial;
        ok = EncodeAlphaInternal(alpha, width, height, method, filter,
                                 reduce_levels, effort_level, filtered_alpha,
                                 &trial);
        if (ok && trial.score < best.score) {
          VP8BitWriterWipeOut(&best.bw);
          best = trial;
        } else {
          VP8BitWriterWipeOut(&trial.bw);
        }
      }
    }
    WebPSafeFree(filtered_alpha);
  } else {
    ok = EncodeAlphaInternal(alpha, width, height, method, WEBP_FILTER_NONE,
                             reduce_levels, effort_level, NULL, &best);
  }
  if (ok) {
#if !defined(WEBP_DISABLE_STATS)
    if (stats != NULL) {
      stats->lossless_features = best.stats.lossless_features;
      stats->histogram_bits = best.stats.histogram_bits;
      stats->transform_bits = best.stats.transform_bits;
      stats->cache_bits = best.stats.cache_bits;
      stats->palette_size = best.stats.palette_size;
      stats->lossless_size = best.stats.lossless_size;
      stats->lossless_hdr_size = best.stats.lossless_hdr_size;
      stats->lossless_data_size = best.stats.lossless_data_size;
    }
#else
    (void)stats;
#endif
    *output_size = VP8BitWriterSize(&best.bw);
    *output = VP8BitWriterBuf(&best.bw);
  } else {
    VP8BitWriterWipeOut(&best.bw);
  }
  return ok;
}

static int EncodeAlpha(VP8Encoder* const enc,
                       int quality, int method, int filter,
                       int effort_level,
                       uint8_t** const output, size_t* const output_size) {
  const WebPPicture* const pic = enc->pic_;
  const int width = pic->width;
  const int height = pic->height;

  uint8_t* quant_alpha = NULL;
  const size_t data_size = width * height;
  uint64_t sse = 0;
  int ok = 1;
  const int reduce_levels = (quality < 100);

  // quick sanity checks
  assert((uint64_t)data_size == (uint64_t)width * height);  // as per spec
  assert(enc != NULL && pic != NULL && pic->a != NULL);
  assert(output != NULL && output_size != NULL);
  assert(width > 0 && height > 0);
  assert(pic->a_stride >= width);
  assert(filter >= WEBP_FILTER_NONE && filter <= WEBP_FILTER_FAST);

  if (quality < 0 || quality > 100) {
    return 0;
  }

  if (method < ALPHA_NO_COMPRESSION || method > ALPHA_LOSSLESS_COMPRESSION) {
    return 0;
  }

  if (method == ALPHA_NO_COMPRESSION) {
    // Don't filter, as filtering will make no impact on compressed size.
    filter = WEBP_FILTER_NONE;
  }

  quant_alpha = (uint8_t*)WebPSafeMalloc(1ULL, data_size);
  if (quant_alpha == NULL) {
    return 0;
  }

  // Extract alpha data (width x height) from raw_data (stride x height).
  WebPCopyPlane(pic->a, pic->a_stride, quant_alpha, width, width, height);

  if (reduce_levels) {  // No Quantization required for 'quality = 100'.
    // 16 alpha levels gives quite a low MSE w.r.t original alpha plane hence
    // mapped to moderate quality 70. Hence Quality:[0, 70] -> Levels:[2, 16]
    // and Quality:]70, 100] -> Levels:]16, 256].
    const int alpha_levels = (quality <= 70) ? (2 + quality / 5)
                                             : (16 + (quality - 70) * 8);
    ok = QuantizeLevels(quant_alpha, width, height, alpha_levels, &sse);
  }

  if (ok) {
    ok = ApplyFiltersAndEncode(quant_alpha, width, height, data_size, method,
                               filter, reduce_levels, effort_level, output,
                               output_size, pic->stats);
#if !defined(WEBP_DISABLE_STATS)
    if (pic->stats != NULL) {  // need stats?
      pic->stats->coded_size += (int)(*output_size);
      enc->sse_[3] = sse;
    }
#endif
  }

  WebPSafeFree(quant_alpha);
  return ok;
}

static int CompressAlphaJob(void* arg1, void* dummy) {
  VP8Encoder* const enc = (VP8Encoder*)arg1;
  const WebPConfig* config = enc->config_;
  uint8_t* alpha_data = NULL;
  size_t alpha_size = 0;
  const int effort_level = config->method;  // maps to [0..6]
  const WEBP_FILTER_TYPE filter =
      (config->alpha_filtering == 0) ? WEBP_FILTER_NONE :
      (config->alpha_filtering == 1) ? WEBP_FILTER_FAST :
                                       WEBP_FILTER_BEST;
  if (!EncodeAlpha(enc, config->alpha_quality, config->alpha_compression,
                   filter, effort_level, &alpha_data, &alpha_size)) {
    return 0;
  }
  if (alpha_size != (uint32_t)alpha_size) {  // Sanity check.
    WebPSafeFree(alpha_data);
    return 0;
  }
  enc->alpha_data_size_ = (uint32_t)alpha_size;
  enc->alpha_data_ = alpha_data;
  (void)dummy;
  return 1;
}

void VP8EncInitAlpha(VP8Encoder* const enc) {
  enc->has_alpha_ = WebPPictureHasTransparency(enc->pic_);
  enc->alpha_data_ = NULL;
  enc->alpha_data_size_ = 0;
  if (enc->thread_level_ > 0) {
    WebPWorker* const worker = &enc->alpha_worker_;
    WebPGetWorkerInterface()->Init(worker);
    worker->data1 = enc;
    worker->data2 = NULL;
    worker->hook = CompressAlphaJob;
  }
}

#define MIN_PAGE_SIZE 8192          // minimum number of token per page

void VP8TBufferInit(VP8TBuffer* const b, int page_size) {
  b->tokens_ = NULL;
  b->pages_ = NULL;
  b->last_page_ = &b->pages_;
  b->left_ = 0;
  b->page_size_ = (page_size < MIN_PAGE_SIZE) ? MIN_PAGE_SIZE : page_size;
  b->error_ = 0;
}

static VP8Encoder* InitVP8Encoder(const WebPConfig* const config,
                                  WebPPicture* const picture) {
  VP8Encoder* enc;
  const int use_filter =
      (config->filter_strength > 0) || (config->autofilter > 0);
  const int mb_w = (picture->width + 15) >> 4;
  const int mb_h = (picture->height + 15) >> 4;
  const int preds_w = 4 * mb_w + 1;
  const int preds_h = 4 * mb_h + 1;
  const size_t preds_size = preds_w * preds_h * sizeof(*enc->preds_);
  const int top_stride = mb_w * 16;
  const size_t nz_size = (mb_w + 1) * sizeof(*enc->nz_) + WEBP_ALIGN_CST;
  const size_t info_size = mb_w * mb_h * sizeof(*enc->mb_info_);
  const size_t samples_size =
      2 * top_stride * sizeof(*enc->y_top_)  // top-luma/u/v
      + WEBP_ALIGN_CST;                      // align all
  const size_t lf_stats_size =
      config->autofilter ? sizeof(*enc->lf_stats_) + WEBP_ALIGN_CST : 0;
  const size_t top_derr_size =
      (config->quality <= ERROR_DIFFUSION_QUALITY || config->pass > 1) ?
          mb_w * sizeof(*enc->top_derr_) : 0;
  uint8_t* mem;
  const uint64_t size = (uint64_t)sizeof(*enc)   // main struct
                      + WEBP_ALIGN_CST           // cache alignment
                      + info_size                // modes info
                      + preds_size               // prediction modes
                      + samples_size             // top/left samples
                      + top_derr_size            // top diffusion error
                      + nz_size                  // coeff context bits
                      + lf_stats_size;           // autofilter stats

#ifdef PRINT_MEMORY_INFO
  printf("===================================\n");
  printf("Memory used:\n"
         "             encoder: %ld\n"
         "                info: %ld\n"
         "               preds: %ld\n"
         "         top samples: %ld\n"
         "       top diffusion: %ld\n"
         "            non-zero: %ld\n"
         "            lf-stats: %ld\n"
         "               total: %ld\n",
         sizeof(*enc) + WEBP_ALIGN_CST, info_size,
         preds_size, samples_size, top_derr_size, nz_size, lf_stats_size, size);
  printf("Transient object sizes:\n"
         "      VP8EncIterator: %ld\n"
         "        VP8ModeScore: %ld\n"
         "      VP8SegmentInfo: %ld\n"
         "         VP8EncProba: %ld\n"
         "             LFStats: %ld\n",
         sizeof(VP8EncIterator), sizeof(VP8ModeScore),
         sizeof(VP8SegmentInfo), sizeof(VP8EncProba),
         sizeof(LFStats));
  printf("Picture size (yuv): %ld\n",
         mb_w * mb_h * 384 * sizeof(uint8_t));
  printf("===================================\n");
#endif
  mem = (uint8_t*)WebPSafeMalloc(size, sizeof(*mem));
  if (mem == NULL) {
    WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  enc = (VP8Encoder*)mem;
  mem = (uint8_t*)WEBP_ALIGN(mem + sizeof(*enc));
  memset(enc, 0, sizeof(*enc));
  enc->num_parts_ = 1 << config->partitions;
  enc->mb_w_ = mb_w;
  enc->mb_h_ = mb_h;
  enc->preds_w_ = preds_w;
  enc->mb_info_ = (VP8MBInfo*)mem;
  mem += info_size;
  enc->preds_ = mem + 1 + enc->preds_w_;
  mem += preds_size;
  enc->nz_ = 1 + (uint32_t*)WEBP_ALIGN(mem);
  mem += nz_size;
  enc->lf_stats_ = lf_stats_size ? (LFStats*)WEBP_ALIGN(mem) : NULL;
  mem += lf_stats_size;

  // top samples (all 16-aligned)
  mem = (uint8_t*)WEBP_ALIGN(mem);
  enc->y_top_ = mem;
  enc->uv_top_ = enc->y_top_ + top_stride;
  mem += 2 * top_stride;
  enc->top_derr_ = top_derr_size ? (DError*)mem : NULL;
  mem += top_derr_size;
  assert(mem <= (uint8_t*)enc + size);

  enc->config_ = config;
  enc->profile_ = use_filter ? ((config->filter_type == 1) ? 0 : 1) : 2;
  enc->pic_ = picture;
  enc->percent_ = 0;

  MapConfigToTools(enc);
  InitTables();
  VP8DefaultProbas(enc);
  ResetSegmentHeader(enc);
  ResetFilterHeader(enc);
  ResetBoundaryPredictions(enc);
  VP8EncInitAlpha(enc);

  // lower quality means smaller output -> we modulate a little the page
  // size based on quality. This is just a crude 1rst-order prediction.
  {
    const float scale = 1.f + config->quality * 5.f / 100.f;  // in [1,6]
    VP8TBufferInit(&enc->tokens_, (int)(mb_w * mb_h * 4 * scale));
  }
  return enc;
}

#define BPS 32   // this is the common stride for enc/dec
#define YUV_SIZE_ENC (BPS * 16)
#define PRED_SIZE_ENC (32 * BPS + 16 * BPS + 8 * BPS)   // I16+Chroma+I4 preds

// Iterator structure to iterate through macroblocks, pointing to the
// right neighbouring data (samples, predictions, contexts, ...)
typedef struct {
  int x_, y_;                      // current macroblock
  uint8_t*      yuv_in_;           // input samples
  uint8_t*      yuv_out_;          // output samples
  uint8_t*      yuv_out2_;         // secondary buffer swapped with yuv_out_.
  uint8_t*      yuv_p_;            // scratch buffer for prediction
  VP8Encoder*   enc_;              // back-pointer
  VP8MBInfo*    mb_;               // current macroblock
  VP8BitWriter* bw_;               // current bit-writer
  uint8_t*      preds_;            // intra mode predictors (4x4 blocks)
  uint32_t*     nz_;               // non-zero pattern
  uint8_t       i4_boundary_[37];  // 32+5 boundary samples needed by intra4x4
  uint8_t*      i4_top_;           // pointer to the current top boundary sample
  int           i4_;               // current intra4x4 mode being tested
  int           top_nz_[9];        // top-non-zero context.
  int           left_nz_[9];       // left-non-zero. left_nz[8] is independent.
  uint64_t      bit_count_[4][3];  // bit counters for coded levels.
  uint64_t      luma_bits_;        // macroblock bit-cost for luma
  uint64_t      uv_bits_;          // macroblock bit-cost for chroma
  LFStats*      lf_stats_;         // filter stats (borrowed from enc_)
  int           do_trellis_;       // if true, perform extra level optimisation
  int           count_down_;       // number of mb still to be processed
  int           count_down0_;      // starting counter value (for progress)
  int           percent0_;         // saved initial progress percent

  DError        left_derr_;        // left error diffusion (u/v)
  DError       *top_derr_;         // top diffusion error - NULL if disabled

  uint8_t* y_left_;    // left luma samples (addressable from index -1 to 15).
  uint8_t* u_left_;    // left u samples (addressable from index -1 to 7)
  uint8_t* v_left_;    // left v samples (addressable from index -1 to 7)

  uint8_t* y_top_;     // top luma samples at position 'x_'
  uint8_t* uv_top_;    // top u/v samples at position 'x_', packed as 16 bytes

  // memory for storing y/u/v_left_
  uint8_t yuv_left_mem_[17 + 16 + 16 + 8 + WEBP_ALIGN_CST];
  // memory for yuv_*
  uint8_t yuv_mem_[3 * YUV_SIZE_ENC + PRED_SIZE_ENC + WEBP_ALIGN_CST];
} VP8EncIterator;

#define MAX_ALPHA 255                // 8b of precision for susceptibilities.

// struct used to collect job result
typedef struct {
  WebPWorker worker;
  int alphas[MAX_ALPHA + 1];
  int alpha, uv_alpha;
  VP8EncIterator it;
  int delta_progress;
} SegmentJob;

int VP8IteratorIsDone(const VP8EncIterator* const it) {
  return (it->count_down_ <= 0);
}

static void ImportBlock(const uint8_t* src, int src_stride,
                        uint8_t* dst, int w, int h, int size) {
  int i;
  for (i = 0; i < h; ++i) {
    memcpy(dst, src, w);
    if (w < size) {
      memset(dst + w, dst[w - 1], size - w);
    }
    dst += BPS;
    src += src_stride;
  }
  for (i = h; i < size; ++i) {
    memcpy(dst, dst - BPS, size);
    dst += BPS;
  }
}

static int MinSize(int a, int b) { return (a < b) ? a : b; }

#define Y_OFF_ENC    (0)
#define U_OFF_ENC    (16)
#define V_OFF_ENC    (16 + 8)

static void InitLeft(VP8EncIterator* const it) {
  it->y_left_[-1] = it->u_left_[-1] = it->v_left_[-1] =
      (it->y_ > 0) ? 129 : 127;
  memset(it->y_left_, 129, 16);
  memset(it->u_left_, 129, 8);
  memset(it->v_left_, 129, 8);
  it->left_nz_[8] = 0;
  if (it->top_derr_ != NULL) {
    memset(&it->left_derr_, 0, sizeof(it->left_derr_));
  }
}

static void ImportLine(const uint8_t* src, int src_stride,
                       uint8_t* dst, int len, int total_len) {
  int i;
  for (i = 0; i < len; ++i, src += src_stride) dst[i] = *src;
  for (; i < total_len; ++i) dst[i] = dst[len - 1];
}

void VP8IteratorImport(VP8EncIterator* const it, uint8_t* tmp_32) {
  const VP8Encoder* const enc = it->enc_;
  const int x = it->x_, y = it->y_;
  const WebPPicture* const pic = enc->pic_;
  const uint8_t* const ysrc = pic->y + (y * pic->y_stride  + x) * 16;
  const uint8_t* const usrc = pic->u + (y * pic->uv_stride + x) * 8;
  const uint8_t* const vsrc = pic->v + (y * pic->uv_stride + x) * 8;
  const int w = MinSize(pic->width - x * 16, 16);
  const int h = MinSize(pic->height - y * 16, 16);
  const int uv_w = (w + 1) >> 1;
  const int uv_h = (h + 1) >> 1;

  ImportBlock(ysrc, pic->y_stride,  it->yuv_in_ + Y_OFF_ENC, w, h, 16);
  ImportBlock(usrc, pic->uv_stride, it->yuv_in_ + U_OFF_ENC, uv_w, uv_h, 8);
  ImportBlock(vsrc, pic->uv_stride, it->yuv_in_ + V_OFF_ENC, uv_w, uv_h, 8);

  if (tmp_32 == NULL) return;

  // Import source (uncompressed) samples into boundary.
  if (x == 0) {
    InitLeft(it);
  } else {
    if (y == 0) {
      it->y_left_[-1] = it->u_left_[-1] = it->v_left_[-1] = 127;
    } else {
      it->y_left_[-1] = ysrc[- 1 - pic->y_stride];
      it->u_left_[-1] = usrc[- 1 - pic->uv_stride];
      it->v_left_[-1] = vsrc[- 1 - pic->uv_stride];
    }
    ImportLine(ysrc - 1, pic->y_stride,  it->y_left_, h,   16);
    ImportLine(usrc - 1, pic->uv_stride, it->u_left_, uv_h, 8);
    ImportLine(vsrc - 1, pic->uv_stride, it->v_left_, uv_h, 8);
  }

  it->y_top_  = tmp_32 + 0;
  it->uv_top_ = tmp_32 + 16;
  if (y == 0) {
    memset(tmp_32, 127, 32 * sizeof(*tmp_32));
  } else {
    ImportLine(ysrc - pic->y_stride,  1, tmp_32,          w,   16);
    ImportLine(usrc - pic->uv_stride, 1, tmp_32 + 16,     uv_w, 8);
    ImportLine(vsrc - pic->uv_stride, 1, tmp_32 + 16 + 8, uv_w, 8);
  }
}

void VP8SetIntra16Mode(const VP8EncIterator* const it, int mode) {
  uint8_t* preds = it->preds_;
  int y;
  for (y = 0; y < 4; ++y) {
    memset(preds, mode, 4);
    preds += it->enc_->preds_w_;
  }
  it->mb_->type_ = 1;
}

void VP8SetSkip(const VP8EncIterator* const it, int skip) {
  it->mb_->skip_ = skip;
}

void VP8SetSegment(const VP8EncIterator* const it, int segment) {
  it->mb_->segment_ = segment;
}

static void Mean16x4_C(const uint8_t* ref, uint32_t dc[4]) {
  int k, x, y;
  for (k = 0; k < 4; ++k) {
    uint32_t avg = 0;
    for (y = 0; y < 4; ++y) {
      for (x = 0; x < 4; ++x) {
        avg += ref[x + y * BPS];
      }
    }
    dc[k] = avg;
    ref += 4;   // go to next 4x4 block.
  }
}

void VP8SetIntra4Mode(const VP8EncIterator* const it, const uint8_t* modes) {
  uint8_t* preds = it->preds_;
  int y;
  for (y = 4; y > 0; --y) {
    memcpy(preds, modes, 4 * sizeof(*modes));
    preds += it->enc_->preds_w_;
    modes += 4;
  }
  it->mb_->type_ = 0;
}

static int FastMBAnalyze(VP8EncIterator* const it) {
  // Empirical cut-off value, should be around 16 (~=block size). We use the
  // [8-17] range and favor intra4 at high quality, intra16 for low quality.
  const int q = (int)it->enc_->config_->quality;
  const uint32_t kThreshold = 8 + (17 - 8) * q / 100;
  int k;
  uint32_t dc[16], m, m2;
  for (k = 0; k < 16; k += 4) {
	  Mean16x4_C(it->yuv_in_ + Y_OFF_ENC + k * BPS, &dc[k]);
  }
  for (m = 0, m2 = 0, k = 0; k < 16; ++k) {
    m += dc[k];
    m2 += dc[k] * dc[k];
  }
  if (kThreshold * m2 < m * m) {
    VP8SetIntra16Mode(it, 0);   // DC16
  } else {
    const uint8_t modes[16] = { 0 };  // DC4
    VP8SetIntra4Mode(it, modes);
  }
  return 0;
}

#define MAX_INTRA16_MODE 2
#define DEFAULT_ALPHA (-1)

//------------------------------------------------------------------------------
// Intra predictions

static void Fill(uint8_t* dst, int value, int size) {
  int j;
  for (j = 0; j < size; ++j) {
    memset(dst + j * BPS, value, size);
  }
}

static void VerticalPred(uint8_t* dst,
                                     const uint8_t* top, int size) {
  int j;
  if (top != NULL) {
    for (j = 0; j < size; ++j) memcpy(dst + j * BPS, top, size);
  } else {
    Fill(dst, 127, size);
  }
}

static void HorizontalPred(uint8_t* dst,
                                       const uint8_t* left, int size) {
  if (left != NULL) {
    int j;
    for (j = 0; j < size; ++j) {
      memset(dst + j * BPS, left[j], size);
    }
  } else {
    Fill(dst, 129, size);
  }
}

static void TrueMotion(uint8_t* dst, const uint8_t* left,
                                   const uint8_t* top, int size) {
  int y;
  if (left != NULL) {
    if (top != NULL) {
      const uint8_t* const clip = clip1 + 255 - left[-1];
      for (y = 0; y < size; ++y) {
        const uint8_t* const clip_table = clip + left[y];
        int x;
        for (x = 0; x < size; ++x) {
          dst[x] = clip_table[top[x]];
        }
        dst += BPS;
      }
    } else {
      HorizontalPred(dst, left, size);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (top != NULL) {
      VerticalPred(dst, top, size);
    } else {
      Fill(dst, 129, size);
    }
  }
}

static void DCMode(uint8_t* dst, const uint8_t* left,
                               const uint8_t* top,
                               int size, int round, int shift) {
  int DC = 0;
  int j;
  if (top != NULL) {
    for (j = 0; j < size; ++j) DC += top[j];
    if (left != NULL) {   // top and left present
      for (j = 0; j < size; ++j) DC += left[j];
    } else {      // top, but no left
      DC += DC;
    }
    DC = (DC + round) >> shift;
  } else if (left != NULL) {   // left but no top
    for (j = 0; j < size; ++j) DC += left[j];
    DC += DC;
    DC = (DC + round) >> shift;
  } else {   // no top, no left, nothing.
    DC = 0x80;
  }
  Fill(dst, DC, size);
}

// intra 16x16
#define I16DC16 (0 * 16 * BPS)
#define I16TM16 (I16DC16 + 16)
#define I16VE16 (1 * 16 * BPS)
#define I16HE16 (I16VE16 + 16)

static void Intra16Preds_C(uint8_t* dst,
                           const uint8_t* left, const uint8_t* top) {
  DCMode(I16DC16 + dst, left, top, 16, 16, 5);
  VerticalPred(I16VE16 + dst, top, 16);
  HorizontalPred(I16HE16 + dst, left, 16);
  TrueMotion(I16TM16 + dst, left, top, 16);
}

void VP8MakeLuma16Preds(const VP8EncIterator* const it) {
  const uint8_t* const left = it->x_ ? it->y_left_ : NULL;
  const uint8_t* const top = it->y_ ? it->y_top_ : NULL;
  Intra16Preds_C(it->yuv_p_, left, top);
}

typedef struct {
  // We only need to store max_value and last_non_zero, not the distribution.
  int max_value;
  int last_non_zero;
} VP8Histogram;

static void InitHistogram(VP8Histogram* const histo) {
  histo->max_value = 0;
  histo->last_non_zero = 1;
}

#define MAX_COEFF_THRESH   31   // size of histogram used by CollectHistogram.

static void FTransform_C(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  int i;
  int tmp[16];
  for (i = 0; i < 4; ++i, src += BPS, ref += BPS) {
    const int d0 = src[0] - ref[0];   // 9bit dynamic range ([-255,255])
    const int d1 = src[1] - ref[1];
    const int d2 = src[2] - ref[2];
    const int d3 = src[3] - ref[3];
    const int a0 = (d0 + d3);         // 10b                      [-510,510]
    const int a1 = (d1 + d2);
    const int a2 = (d1 - d2);
    const int a3 = (d0 - d3);
    tmp[0 + i * 4] = (a0 + a1) * 8;   // 14b                      [-8160,8160]
    tmp[1 + i * 4] = (a2 * 2217 + a3 * 5352 + 1812) >> 9;      // [-7536,7542]
    tmp[2 + i * 4] = (a0 - a1) * 8;
    tmp[3 + i * 4] = (a3 * 2217 - a2 * 5352 +  937) >> 9;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[12 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[ 8 + i]);
    const int a2 = (tmp[4 + i] - tmp[ 8 + i]);
    const int a3 = (tmp[0 + i] - tmp[12 + i]);
    out[0 + i] = (a0 + a1 + 7) >> 4;            // 12b
    out[4 + i] = ((a2 * 2217 + a3 * 5352 + 12000) >> 16) + (a3 != 0);
    out[8 + i] = (a0 - a1 + 7) >> 4;
    out[12+ i] = ((a3 * 2217 - a2 * 5352 + 51000) >> 16);
  }
}

const int VP8DspScan[16 + 4 + 4] = {
  // Luma
  0 +  0 * BPS,  4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS,
  0 +  4 * BPS,  4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS,
  0 +  8 * BPS,  4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS,
  0 + 12 * BPS,  4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS,

  0 + 0 * BPS,   4 + 0 * BPS, 0 + 4 * BPS,  4 + 4 * BPS,    // U
  8 + 0 * BPS,  12 + 0 * BPS, 8 + 4 * BPS, 12 + 4 * BPS     // V
};

static int clip_max(int v, int max) {
  return (v > max) ? max : v;
}

// general-purpose util function
void VP8SetHistogramData(const int distribution[MAX_COEFF_THRESH + 1],
                         VP8Histogram* const histo) {
  int max_value = 0, last_non_zero = 1;
  int k;
  for (k = 0; k <= MAX_COEFF_THRESH; ++k) {
    const int value = distribution[k];
    if (value > 0) {
      if (value > max_value) max_value = value;
      last_non_zero = k;
    }
  }
  histo->max_value = max_value;
  histo->last_non_zero = last_non_zero;
}

static void CollectHistogram_C(const uint8_t* ref, const uint8_t* pred,
                               int start_block, int end_block,
                               VP8Histogram* const histo) {
  int j;
  int distribution[MAX_COEFF_THRESH + 1] = { 0 };
  for (j = start_block; j < end_block; ++j) {
    int k;
    int16_t out[16];

    FTransform_C(ref + VP8DspScan[j], pred + VP8DspScan[j], out);

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      const int v = abs(out[k]) >> 3;
      const int clipped_value = clip_max(v, MAX_COEFF_THRESH);
      ++distribution[clipped_value];
    }
  }
  VP8SetHistogramData(distribution, histo);
}

const uint16_t VP8I16ModeOffsets[4] = { I16DC16, I16TM16, I16VE16, I16HE16 };

#define ALPHA_SCALE (2 * MAX_ALPHA)  // scaling factor for alpha.

static int GetAlpha(const VP8Histogram* const histo) {
  // 'alpha' will later be clipped to [0..MAX_ALPHA] range, clamping outer
  // values which happen to be mostly noise. This leaves the maximum precision
  // for handling the useful small values which contribute most.
  const int max_value = histo->max_value;
  const int last_non_zero = histo->last_non_zero;
  const int alpha =
      (max_value > 1) ? ALPHA_SCALE * last_non_zero / max_value : 0;
  return alpha;
}

#define IS_BETTER_ALPHA(alpha, best_alpha) ((alpha) > (best_alpha))

static int MBAnalyzeBestIntra16Mode(VP8EncIterator* const it) {
  const int max_mode = MAX_INTRA16_MODE;
  int mode;
  int best_alpha = DEFAULT_ALPHA;
  int best_mode = 0;

  VP8MakeLuma16Preds(it);
  for (mode = 0; mode < max_mode; ++mode) {
    VP8Histogram histo;
    int alpha;

    InitHistogram(&histo);
    CollectHistogram_C(it->yuv_in_ + Y_OFF_ENC,
                        it->yuv_p_ + VP8I16ModeOffsets[mode],
                        0, 16, &histo);
    alpha = GetAlpha(&histo);
    if (IS_BETTER_ALPHA(alpha, best_alpha)) {
      best_alpha = alpha;
      best_mode = mode;
    }
  }
  VP8SetIntra16Mode(it, best_mode);
  return best_alpha;
}

#define MAX_INTRA4_MODE  2

// Convert packed context to byte array
#define BIT(nz, n) (!!((nz) & (1 << (n))))

void VP8IteratorNzToBytes(VP8EncIterator* const it) {
  const int tnz = it->nz_[0], lnz = it->nz_[-1];
  int* const top_nz = it->top_nz_;
  int* const left_nz = it->left_nz_;

  // Top-Y
  top_nz[0] = BIT(tnz, 12);
  top_nz[1] = BIT(tnz, 13);
  top_nz[2] = BIT(tnz, 14);
  top_nz[3] = BIT(tnz, 15);
  // Top-U
  top_nz[4] = BIT(tnz, 18);
  top_nz[5] = BIT(tnz, 19);
  // Top-V
  top_nz[6] = BIT(tnz, 22);
  top_nz[7] = BIT(tnz, 23);
  // DC
  top_nz[8] = BIT(tnz, 24);

  // left-Y
  left_nz[0] = BIT(lnz,  3);
  left_nz[1] = BIT(lnz,  7);
  left_nz[2] = BIT(lnz, 11);
  left_nz[3] = BIT(lnz, 15);
  // left-U
  left_nz[4] = BIT(lnz, 17);
  left_nz[5] = BIT(lnz, 19);
  // left-V
  left_nz[6] = BIT(lnz, 21);
  left_nz[7] = BIT(lnz, 23);
  // left-DC is special, iterated separately
}

// Array to record the position of the top sample to pass to the prediction
// functions in dsp.c.
static const uint8_t VP8TopLeftI4[16] = {
  17, 21, 25, 29,
  13, 17, 21, 25,
  9,  13, 17, 21,
  5,   9, 13, 17
};

void VP8IteratorStartI4(VP8EncIterator* const it) {
  const VP8Encoder* const enc = it->enc_;
  int i;

  it->i4_ = 0;    // first 4x4 sub-block
  it->i4_top_ = it->i4_boundary_ + VP8TopLeftI4[0];

  // Import the boundary samples
  for (i = 0; i < 17; ++i) {    // left
    it->i4_boundary_[i] = it->y_left_[15 - i];
  }
  for (i = 0; i < 16; ++i) {    // top
    it->i4_boundary_[17 + i] = it->y_top_[i];
  }
  // top-right samples have a special case on the far right of the picture
  if (it->x_ < enc->mb_w_ - 1) {
    for (i = 16; i < 16 + 4; ++i) {
      it->i4_boundary_[17 + i] = it->y_top_[i];
    }
  } else {    // else, replicate the last valid pixel four times
    for (i = 16; i < 16 + 4; ++i) {
      it->i4_boundary_[17 + i] = it->i4_boundary_[17 + 15];
    }
  }
  VP8IteratorNzToBytes(it);  // import the non-zero context
}

//------------------------------------------------------------------------------
// luma 4x4 prediction

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

static void VE4(uint8_t* dst, const uint8_t* top) {    // vertical
  const uint8_t vals[4] = {
    AVG3(top[-1], top[0], top[1]),
    AVG3(top[ 0], top[1], top[2]),
    AVG3(top[ 1], top[2], top[3]),
    AVG3(top[ 2], top[3], top[4])
  };
  int i;
  for (i = 0; i < 4; ++i) {
    memcpy(dst + i * BPS, vals, 4);
  }
}

static void WebPUint32ToMem(uint8_t* const ptr, uint32_t val) {
  memcpy(ptr, &val, sizeof(val));
}

static void HE4(uint8_t* dst, const uint8_t* top) {    // horizontal
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  WebPUint32ToMem(dst + 0 * BPS, 0x01010101U * AVG3(X, I, J));
  WebPUint32ToMem(dst + 1 * BPS, 0x01010101U * AVG3(I, J, K));
  WebPUint32ToMem(dst + 2 * BPS, 0x01010101U * AVG3(J, K, L));
  WebPUint32ToMem(dst + 3 * BPS, 0x01010101U * AVG3(K, L, L));
}

static void DC4(uint8_t* dst, const uint8_t* top) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += top[i] + top[-5 + i];
  Fill(dst, dc >> 3, 4);
}

static void RD4(uint8_t* dst, const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  DST(0, 3)                                     = AVG3(J, K, L);
  DST(0, 2) = DST(1, 3)                         = AVG3(I, J, K);
  DST(0, 1) = DST(1, 2) = DST(2, 3)             = AVG3(X, I, J);
  DST(0, 0) = DST(1, 1) = DST(2, 2) = DST(3, 3) = AVG3(A, X, I);
  DST(1, 0) = DST(2, 1) = DST(3, 2)             = AVG3(B, A, X);
  DST(2, 0) = DST(3, 1)                         = AVG3(C, B, A);
  DST(3, 0)                                     = AVG3(D, C, B);
}

static void LD4(uint8_t* dst, const uint8_t* top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  DST(0, 0)                                     = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1)                         = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2)             = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3)             = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3)                         = AVG3(F, G, H);
  DST(3, 3)                                     = AVG3(G, H, H);
}

static void VR4(uint8_t* dst, const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  DST(0, 0) = DST(1, 2) = AVG2(X, A);
  DST(1, 0) = DST(2, 2) = AVG2(A, B);
  DST(2, 0) = DST(3, 2) = AVG2(B, C);
  DST(3, 0)             = AVG2(C, D);

  DST(0, 3) =             AVG3(K, J, I);
  DST(0, 2) =             AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
  DST(3, 1) =             AVG3(B, C, D);
}

static void VL4(uint8_t* dst, const uint8_t* top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  DST(0, 0) =             AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);

  DST(0, 1) =             AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
              DST(3, 2) = AVG3(E, F, G);
              DST(3, 3) = AVG3(F, G, H);
}

static void HU4(uint8_t* dst, const uint8_t* top) {
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  DST(0, 0) =             AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) =             AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) =
  DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

static void HD4(uint8_t* dst, const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];

  DST(0, 0) = DST(2, 1) = AVG2(I, X);
  DST(0, 1) = DST(2, 2) = AVG2(J, I);
  DST(0, 2) = DST(2, 3) = AVG2(K, J);
  DST(0, 3)             = AVG2(L, K);

  DST(3, 0)             = AVG3(A, B, C);
  DST(2, 0)             = AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
  DST(1, 3)             = AVG3(L, K, J);
}

static void TM4(uint8_t* dst, const uint8_t* top) {
  int x, y;
  const uint8_t* const clip = clip1 + 255 - top[-1];
  for (y = 0; y < 4; ++y) {
    const uint8_t* const clip_table = clip + top[-2 - y];
    for (x = 0; x < 4; ++x) {
      dst[x] = clip_table[top[x]];
    }
    dst += BPS;
  }
}

#undef DST
#undef AVG3
#undef AVG2

// intra 4x4
#define I4DC4 (3 * 16 * BPS +  0)
#define I4TM4 (I4DC4 +  4)
#define I4VE4 (I4DC4 +  8)
#define I4HE4 (I4DC4 + 12)
#define I4RD4 (I4DC4 + 16)
#define I4VR4 (I4DC4 + 20)
#define I4LD4 (I4DC4 + 24)
#define I4VL4 (I4DC4 + 28)
#define I4HD4 (3 * 16 * BPS + 4 * BPS)
#define I4HU4 (I4HD4 + 4)
#define I4TMP (I4HD4 + 8)

// Left samples are top[-5 .. -2], top_left is top[-1], top are
// located at top[0..3], and top right is top[4..7]
static void Intra4Preds_C(uint8_t* dst, const uint8_t* top) {
  DC4(I4DC4 + dst, top);
  TM4(I4TM4 + dst, top);
  VE4(I4VE4 + dst, top);
  HE4(I4HE4 + dst, top);
  RD4(I4RD4 + dst, top);
  VR4(I4VR4 + dst, top);
  LD4(I4LD4 + dst, top);
  VL4(I4VL4 + dst, top);
  HD4(I4HD4 + dst, top);
  HU4(I4HU4 + dst, top);
}

void VP8MakeIntra4Preds(const VP8EncIterator* const it) {
	Intra4Preds_C(it->yuv_p_, it->i4_top_);
}

// Must be indexed using {B_DC_PRED -> B_HU_PRED} as index
const uint16_t VP8I4ModeOffsets[NUM_BMODES] = {
  I4DC4, I4TM4, I4VE4, I4HE4, I4RD4, I4VR4, I4LD4, I4VL4, I4HD4, I4HU4
};

static void MergeHistograms(const VP8Histogram* const in,
                            VP8Histogram* const out) {
  if (in->max_value > out->max_value) {
    out->max_value = in->max_value;
  }
  if (in->last_non_zero > out->last_non_zero) {
    out->last_non_zero = in->last_non_zero;
  }
}

const uint16_t VP8Scan[16] = {  // Luma
  0 +  0 * BPS,  4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS,
  0 +  4 * BPS,  4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS,
  0 +  8 * BPS,  4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS,
  0 + 12 * BPS,  4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS,
};

int VP8IteratorRotateI4(VP8EncIterator* const it,
                        const uint8_t* const yuv_out) {
  const uint8_t* const blk = yuv_out + VP8Scan[it->i4_];
  uint8_t* const top = it->i4_top_;
  int i;

  // Update the cache with 7 fresh samples
  for (i = 0; i <= 3; ++i) {
    top[-4 + i] = blk[i + 3 * BPS];   // store future top samples
  }
  if ((it->i4_ & 3) != 3) {  // if not on the right sub-blocks #3, #7, #11, #15
    for (i = 0; i <= 2; ++i) {        // store future left samples
      top[i] = blk[3 + (2 - i) * BPS];
    }
  } else {  // else replicate top-right samples, as says the specs.
    for (i = 0; i <= 3; ++i) {
      top[i] = top[i + 4];
    }
  }
  // move pointers to next sub-block
  ++it->i4_;
  if (it->i4_ == 16) {    // we're done
    return 0;
  }

  it->i4_top_ = it->i4_boundary_ + VP8TopLeftI4[it->i4_];
  return 1;
}

static int MBAnalyzeBestIntra4Mode(VP8EncIterator* const it,
                                   int best_alpha) {
  uint8_t modes[16];
  const int max_mode = MAX_INTRA4_MODE;
  int i4_alpha;
  VP8Histogram total_histo;
  int cur_histo = 0;
  InitHistogram(&total_histo);

  VP8IteratorStartI4(it);
  do {
    int mode;
    int best_mode_alpha = DEFAULT_ALPHA;
    VP8Histogram histos[2];
    const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC + VP8Scan[it->i4_];

    VP8MakeIntra4Preds(it);
    for (mode = 0; mode < max_mode; ++mode) {
      int alpha;

      InitHistogram(&histos[cur_histo]);
      CollectHistogram_C(src, it->yuv_p_ + VP8I4ModeOffsets[mode],
                          0, 1, &histos[cur_histo]);
      alpha = GetAlpha(&histos[cur_histo]);
      if (IS_BETTER_ALPHA(alpha, best_mode_alpha)) {
        best_mode_alpha = alpha;
        modes[it->i4_] = mode;
        cur_histo ^= 1;   // keep track of best histo so far.
      }
    }
    // accumulate best histogram
    MergeHistograms(&histos[cur_histo ^ 1], &total_histo);
    // Note: we reuse the original samples for predictors
  } while (VP8IteratorRotateI4(it, it->yuv_in_ + Y_OFF_ENC));

  i4_alpha = GetAlpha(&total_histo);
  if (IS_BETTER_ALPHA(i4_alpha, best_alpha)) {
    VP8SetIntra4Mode(it, modes);
    best_alpha = i4_alpha;
  }
  return best_alpha;
}

#define MAX_UV_MODE      2

// chroma 8x8, two U/V blocks side by side (hence: 16x8 each)
#define C8DC8 (2 * 16 * BPS)
#define C8TM8 (C8DC8 + 1 * 16)
#define C8VE8 (2 * 16 * BPS + 8 * BPS)
#define C8HE8 (C8VE8 + 1 * 16)

static void IntraChromaPreds_C(uint8_t* dst, const uint8_t* left,
                               const uint8_t* top) {
  // U block
  DCMode(C8DC8 + dst, left, top, 8, 8, 4);
  VerticalPred(C8VE8 + dst, top, 8);
  HorizontalPred(C8HE8 + dst, left, 8);
  TrueMotion(C8TM8 + dst, left, top, 8);
  // V block
  dst += 8;
  if (top != NULL) top += 8;
  if (left != NULL) left += 16;
  DCMode(C8DC8 + dst, left, top, 8, 8, 4);
  VerticalPred(C8VE8 + dst, top, 8);
  HorizontalPred(C8HE8 + dst, left, 8);
  TrueMotion(C8TM8 + dst, left, top, 8);
}

void VP8MakeChroma8Preds(const VP8EncIterator* const it) {
  const uint8_t* const left = it->x_ ? it->u_left_ : NULL;
  const uint8_t* const top = it->y_ ? it->uv_top_ : NULL;
  IntraChromaPreds_C(it->yuv_p_, left, top);
}

const uint16_t VP8UVModeOffsets[4] = { C8DC8, C8TM8, C8VE8, C8HE8 };

void VP8SetIntraUVMode(const VP8EncIterator* const it, int mode) {
  it->mb_->uv_mode_ = mode;
}

static int MBAnalyzeBestUVMode(VP8EncIterator* const it) {
  int best_alpha = DEFAULT_ALPHA;
  int smallest_alpha = 0;
  int best_mode = 0;
  const int max_mode = MAX_UV_MODE;
  int mode;

  VP8MakeChroma8Preds(it);
  for (mode = 0; mode < max_mode; ++mode) {
    VP8Histogram histo;
    int alpha;
    InitHistogram(&histo);
    CollectHistogram_C(it->yuv_in_ + U_OFF_ENC,
                        it->yuv_p_ + VP8UVModeOffsets[mode],
                        16, 16 + 4 + 4, &histo);
    alpha = GetAlpha(&histo);
    if (IS_BETTER_ALPHA(alpha, best_alpha)) {
      best_alpha = alpha;
    }
    // The best prediction mode tends to be the one with the smallest alpha.
    if (mode == 0 || alpha < smallest_alpha) {
      smallest_alpha = alpha;
      best_mode = mode;
    }
  }
  VP8SetIntraUVMode(it, best_mode);
  return best_alpha;
}

static int clip(int v, int m, int M) {
  return (v < m) ? m : (v > M) ? M : v;
}

static int FinalAlphaValue(int alpha) {
  alpha = MAX_ALPHA - alpha;
  return clip(alpha, 0, MAX_ALPHA);
}

static void MBAnalyze(VP8EncIterator* const it,
                      int alphas[MAX_ALPHA + 1],
                      int* const alpha, int* const uv_alpha) {
  const VP8Encoder* const enc = it->enc_;
  int best_alpha, best_uv_alpha;

  VP8SetIntra16Mode(it, 0);  // default: Intra16, DC_PRED
  VP8SetSkip(it, 0);         // not skipped
  VP8SetSegment(it, 0);      // default segment, spec-wise.

  if (enc->method_ <= 1) {
    best_alpha = FastMBAnalyze(it);
  } else {
    best_alpha = MBAnalyzeBestIntra16Mode(it);
    if (enc->method_ >= 5) {
      // We go and make a fast decision for intra4/intra16.
      // It's usually not a good and definitive pick, but helps seeding the
      // stats about level bit-cost.
      // TODO(skal): improve criterion.
      best_alpha = MBAnalyzeBestIntra4Mode(it, best_alpha);
    }
  }
  best_uv_alpha = MBAnalyzeBestUVMode(it);

  // Final susceptibility mix
  best_alpha = (3 * best_alpha + best_uv_alpha + 2) >> 2;
  best_alpha = FinalAlphaValue(best_alpha);
  alphas[best_alpha]++;
  it->mb_->alpha_ = best_alpha;   // for later remapping.

  // Accumulate for later complexity analysis.
  *alpha += best_alpha;   // mixed susceptibility (not just luma)
  *uv_alpha += best_uv_alpha;
}

int WebPReportProgress(const WebPPicture* const pic,
                       int percent, int* const percent_store) {
  if (percent_store != NULL && percent != *percent_store) {
    *percent_store = percent;
    if (pic->progress_hook && !pic->progress_hook(percent, pic)) {
      // user abort requested
      WebPEncodingSetError(pic, VP8_ENC_ERROR_USER_ABORT);
      return 0;
    }
  }
  return 1;  // ok
}

void VP8IteratorSetRow(VP8EncIterator* const it, int y) {
  VP8Encoder* const enc = it->enc_;
  it->x_ = 0;
  it->y_ = y;
  it->bw_ = &enc->parts_[y & (enc->num_parts_ - 1)];
  it->preds_ = enc->preds_ + y * 4 * enc->preds_w_;
  it->nz_ = enc->nz_;
  it->mb_ = enc->mb_info_ + y * enc->mb_w_;
  it->y_top_ = enc->y_top_;
  it->uv_top_ = enc->uv_top_;
  InitLeft(it);
}

int VP8IteratorNext(VP8EncIterator* const it) {
  if (++it->x_ == it->enc_->mb_w_) {
    VP8IteratorSetRow(it, ++it->y_);
  } else {
    it->preds_ += 4;
    it->mb_ += 1;
    it->nz_ += 1;
    it->y_top_ += 16;
    it->uv_top_ += 16;
  }
  return (0 < --it->count_down_);
}

int VP8IteratorProgress(const VP8EncIterator* const it, int delta) {
  VP8Encoder* const enc = it->enc_;
  if (delta && enc->pic_->progress_hook != NULL) {
    const int done = it->count_down0_ - it->count_down_;
    const int percent = (it->count_down0_ <= 0)
                      ? it->percent0_
                      : it->percent0_ + delta * done / it->count_down0_;
    return WebPReportProgress(enc->pic_, percent, &enc->percent_);
  }
  return 1;
}

// main work call
static int DoSegmentsJob(void* arg1, void* arg2) {
  SegmentJob* const job = (SegmentJob*)arg1;
  VP8EncIterator* const it = (VP8EncIterator*)arg2;
  int ok = 1;
  if (!VP8IteratorIsDone(it)) {
    uint8_t tmp[32 + WEBP_ALIGN_CST];
    uint8_t* const scratch = (uint8_t*)WEBP_ALIGN(tmp);
    do {
      // Let's pretend we have perfect lossless reconstruction.
      VP8IteratorImport(it, scratch);
      MBAnalyze(it, job->alphas, &job->alpha, &job->uv_alpha);
      ok = VP8IteratorProgress(it, job->delta_progress);
    } while (ok && VP8IteratorNext(it));
  }
  return ok;
}

void VP8IteratorSetCountDown(VP8EncIterator* const it, int count_down) {
  it->count_down_ = it->count_down0_ = count_down;
}

static void InitTop(VP8EncIterator* const it) {
  const VP8Encoder* const enc = it->enc_;
  const size_t top_size = enc->mb_w_ * 16;
  memset(enc->y_top_, 127, 2 * top_size);
  memset(enc->nz_, 0, enc->mb_w_ * sizeof(*enc->nz_));
  if (enc->top_derr_ != NULL) {
    memset(enc->top_derr_, 0, enc->mb_w_ * sizeof(*enc->top_derr_));
  }
}

void VP8IteratorReset(VP8EncIterator* const it) {
  VP8Encoder* const enc = it->enc_;
  VP8IteratorSetRow(it, 0);
  VP8IteratorSetCountDown(it, enc->mb_w_ * enc->mb_h_);  // default
  InitTop(it);
  memset(it->bit_count_, 0, sizeof(it->bit_count_));
  it->do_trellis_ = 0;
}

void VP8IteratorInit(VP8Encoder* const enc, VP8EncIterator* const it) {
  it->enc_ = enc;
  it->yuv_in_   = (uint8_t*)WEBP_ALIGN(it->yuv_mem_);
  it->yuv_out_  = it->yuv_in_ + YUV_SIZE_ENC;
  it->yuv_out2_ = it->yuv_out_ + YUV_SIZE_ENC;
  it->yuv_p_    = it->yuv_out2_ + YUV_SIZE_ENC;
  it->lf_stats_ = enc->lf_stats_;
  it->percent0_ = enc->percent_;
  it->y_left_ = (uint8_t*)WEBP_ALIGN(it->yuv_left_mem_ + 1);
  it->u_left_ = it->y_left_ + 16 + 16;
  it->v_left_ = it->u_left_ + 16;
  it->top_derr_ = enc->top_derr_;
  VP8IteratorReset(it);
}

// initialize the job struct with some TODOs
static void InitSegmentJob(VP8Encoder* const enc, SegmentJob* const job,
                           int start_row, int end_row) {
  WebPGetWorkerInterface()->Init(&job->worker);
  job->worker.data1 = job;
  job->worker.data2 = &job->it;
  job->worker.hook = DoSegmentsJob;
  VP8IteratorInit(enc, &job->it);
  VP8IteratorSetRow(&job->it, start_row);
  VP8IteratorSetCountDown(&job->it, (end_row - start_row) * enc->mb_w_);
  memset(job->alphas, 0, sizeof(job->alphas));
  job->alpha = 0;
  job->uv_alpha = 0;
  // only one of both jobs can record the progress, since we don't
  // expect the user's hook to be multi-thread safe
  job->delta_progress = (start_row == 0) ? 20 : 0;
}

static void MergeJobs(const SegmentJob* const src, SegmentJob* const dst) {
  int i;
  for (i = 0; i <= MAX_ALPHA; ++i) dst->alphas[i] += src->alphas[i];
  dst->alpha += src->alpha;
  dst->uv_alpha += src->uv_alpha;
}

#define MAX_ITERS_K_MEANS  6

static void SmoothSegmentMap(VP8Encoder* const enc) {
  int n, x, y;
  const int w = enc->mb_w_;
  const int h = enc->mb_h_;
  const int majority_cnt_3_x_3_grid = 5;
  uint8_t* const tmp = (uint8_t*)WebPSafeMalloc(w * h, sizeof(*tmp));
  assert((uint64_t)(w * h) == (uint64_t)w * h);   // no overflow, as per spec

  if (tmp == NULL) return;
  for (y = 1; y < h - 1; ++y) {
    for (x = 1; x < w - 1; ++x) {
      int cnt[NUM_MB_SEGMENTS] = { 0 };
      const VP8MBInfo* const mb = &enc->mb_info_[x + w * y];
      int majority_seg = mb->segment_;
      // Check the 8 neighbouring segment values.
      cnt[mb[-w - 1].segment_]++;  // top-left
      cnt[mb[-w + 0].segment_]++;  // top
      cnt[mb[-w + 1].segment_]++;  // top-right
      cnt[mb[   - 1].segment_]++;  // left
      cnt[mb[   + 1].segment_]++;  // right
      cnt[mb[ w - 1].segment_]++;  // bottom-left
      cnt[mb[ w + 0].segment_]++;  // bottom
      cnt[mb[ w + 1].segment_]++;  // bottom-right
      for (n = 0; n < NUM_MB_SEGMENTS; ++n) {
        if (cnt[n] >= majority_cnt_3_x_3_grid) {
          majority_seg = n;
          break;
        }
      }
      tmp[x + y * w] = majority_seg;
    }
  }
  for (y = 1; y < h - 1; ++y) {
    for (x = 1; x < w - 1; ++x) {
      VP8MBInfo* const mb = &enc->mb_info_[x + w * y];
      mb->segment_ = tmp[x + y * w];
    }
  }
  WebPSafeFree(tmp);
}

static void SetSegmentAlphas(VP8Encoder* const enc,
                             const int centers[NUM_MB_SEGMENTS],
                             int mid) {
  const int nb = enc->segment_hdr_.num_segments_;
  int min = centers[0], max = centers[0];
  int n;

  if (nb > 1) {
    for (n = 0; n < nb; ++n) {
      if (min > centers[n]) min = centers[n];
      if (max < centers[n]) max = centers[n];
    }
  }
  if (max == min) max = min + 1;
  assert(mid <= max && mid >= min);
  for (n = 0; n < nb; ++n) {
    const int alpha = 255 * (centers[n] - mid) / (max - min);
    const int beta = 255 * (centers[n] - min) / (max - min);
    enc->dqm_[n].alpha_ = clip(alpha, -127, 127);
    enc->dqm_[n].beta_ = clip(beta, 0, 255);
  }
}

static void AssignSegments(VP8Encoder* const enc,
                           const int alphas[MAX_ALPHA + 1]) {
  // 'num_segments_' is previously validated and <= NUM_MB_SEGMENTS, but an
  // explicit check is needed to avoid spurious warning about 'n + 1' exceeding
  // array bounds of 'centers' with some compilers (noticed with gcc-4.9).
  const int nb = (enc->segment_hdr_.num_segments_ < NUM_MB_SEGMENTS) ?
                 enc->segment_hdr_.num_segments_ : NUM_MB_SEGMENTS;
  int centers[NUM_MB_SEGMENTS];
  int weighted_average = 0;
  int map[MAX_ALPHA + 1];
  int a, n, k;
  int min_a = 0, max_a = MAX_ALPHA, range_a;
  // 'int' type is ok for histo, and won't overflow
  int accum[NUM_MB_SEGMENTS], dist_accum[NUM_MB_SEGMENTS];

  assert(nb >= 1);
  assert(nb <= NUM_MB_SEGMENTS);

  // bracket the input
  for (n = 0; n <= MAX_ALPHA && alphas[n] == 0; ++n) {}
  min_a = n;
  for (n = MAX_ALPHA; n > min_a && alphas[n] == 0; --n) {}
  max_a = n;
  range_a = max_a - min_a;

  // Spread initial centers evenly
  for (k = 0, n = 1; k < nb; ++k, n += 2) {
    assert(n < 2 * nb);
    centers[k] = min_a + (n * range_a) / (2 * nb);
  }

  for (k = 0; k < MAX_ITERS_K_MEANS; ++k) {     // few iters are enough
    int total_weight;
    int displaced;
    // Reset stats
    for (n = 0; n < nb; ++n) {
      accum[n] = 0;
      dist_accum[n] = 0;
    }
    // Assign nearest center for each 'a'
    n = 0;    // track the nearest center for current 'a'
    for (a = min_a; a <= max_a; ++a) {
      if (alphas[a]) {
        while (n + 1 < nb && abs(a - centers[n + 1]) < abs(a - centers[n])) {
          n++;
        }
        map[a] = n;
        // accumulate contribution into best centroid
        dist_accum[n] += a * alphas[a];
        accum[n] += alphas[a];
      }
    }
    // All point are classified. Move the centroids to the
    // center of their respective cloud.
    displaced = 0;
    weighted_average = 0;
    total_weight = 0;
    for (n = 0; n < nb; ++n) {
      if (accum[n]) {
        const int new_center = (dist_accum[n] + accum[n] / 2) / accum[n];
        displaced += abs(centers[n] - new_center);
        centers[n] = new_center;
        weighted_average += new_center * accum[n];
        total_weight += accum[n];
      }
    }
    weighted_average = (weighted_average + total_weight / 2) / total_weight;
    if (displaced < 5) break;   // no need to keep on looping...
  }

  // Map each original value to the closest centroid
  for (n = 0; n < enc->mb_w_ * enc->mb_h_; ++n) {
    VP8MBInfo* const mb = &enc->mb_info_[n];
    const int alpha = mb->alpha_;
    mb->segment_ = map[alpha];
    mb->alpha_ = centers[map[alpha]];  // for the record.
  }

  if (nb > 1) {
    const int smooth = (enc->config_->preprocessing & 1);
    if (smooth) SmoothSegmentMap(enc);
  }

  SetSegmentAlphas(enc, centers, weighted_average);  // pick some alphas.
}

#undef MAX_ALPHA

static void DefaultMBInfo(VP8MBInfo* const mb) {
  mb->type_ = 1;     // I16x16
  mb->uv_mode_ = 0;
  mb->skip_ = 0;     // not skipped
  mb->segment_ = 0;  // default segment
  mb->alpha_ = 0;
}

static void ResetAllMBInfo(VP8Encoder* const enc) {
  int n;
  for (n = 0; n < enc->mb_w_ * enc->mb_h_; ++n) {
    DefaultMBInfo(&enc->mb_info_[n]);
  }
  // Default susceptibilities.
  enc->dqm_[0].alpha_ = 0;
  enc->dqm_[0].beta_ = 0;
  // Note: we can't compute this alpha_ / uv_alpha_ -> set to default value.
  enc->alpha_ = 0;
  enc->uv_alpha_ = 0;
  WebPReportProgress(enc->pic_, enc->percent_ + 20, &enc->percent_);
}

// main entry point
int VP8EncAnalyze(VP8Encoder* const enc) {
  int ok = 1;
  const int do_segments =
      enc->config_->emulate_jpeg_size ||   // We need the complexity evaluation.
      (enc->segment_hdr_.num_segments_ > 1) ||
      (enc->method_ <= 1);  // for method 0 - 1, we need preds_[] to be filled.
  if (do_segments) {
    const int last_row = enc->mb_h_;
    // We give a little more than a half work to the main thread.
    const int split_row = (9 * last_row + 15) >> 4;
    const int total_mb = last_row * enc->mb_w_;
#ifdef WEBP_USE_THREAD
    const int kMinSplitRow = 2;  // minimal rows needed for mt to be worth it
    const int do_mt = (enc->thread_level_ > 0) && (split_row >= kMinSplitRow);
#else
    const int do_mt = 0;
#endif
    const WebPWorkerInterface* const worker_interface =
        WebPGetWorkerInterface();
    SegmentJob main_job;
    if (do_mt) {
      SegmentJob side_job;
      // Note the use of '&' instead of '&&' because we must call the functions
      // no matter what.
      InitSegmentJob(enc, &main_job, 0, split_row);
      InitSegmentJob(enc, &side_job, split_row, last_row);
      // we don't need to call Reset() on main_job.worker, since we're calling
      // WebPWorkerExecute() on it
      ok &= worker_interface->Reset(&side_job.worker);
      // launch the two jobs in parallel
      if (ok) {
        worker_interface->Launch(&side_job.worker);
        worker_interface->Execute(&main_job.worker);
        ok &= worker_interface->Sync(&side_job.worker);
        ok &= worker_interface->Sync(&main_job.worker);
      }
      worker_interface->End(&side_job.worker);
      if (ok) MergeJobs(&side_job, &main_job);  // merge results together
    } else {
      // Even for single-thread case, we use the generic Worker tools.
      InitSegmentJob(enc, &main_job, 0, last_row);
      worker_interface->Execute(&main_job.worker);
      ok &= worker_interface->Sync(&main_job.worker);
    }
    worker_interface->End(&main_job.worker);
    if (ok) {
      enc->alpha_ = main_job.alpha / total_mb;
      enc->uv_alpha_ = main_job.uv_alpha / total_mb;
      AssignSegments(enc, main_job.alphas);
    }
  } else {   // Use only one default segment.
    ResetAllMBInfo(enc);
  }
  return ok;
}

int VP8EncStartAlpha(VP8Encoder* const enc) {
  if (enc->has_alpha_) {
    if (enc->thread_level_ > 0) {
      WebPWorker* const worker = &enc->alpha_worker_;
      // Makes sure worker is good to go.
      if (!WebPGetWorkerInterface()->Reset(worker)) {
        return 0;
      }
      WebPGetWorkerInterface()->Launch(worker);
      return 1;
    } else {
      return CompressAlphaJob(enc, NULL);   // just do the job right away
    }
  }
  return 1;
}

static const uint8_t kAverageBytesPerMB[8] = { 50, 24, 16, 9, 7, 5, 3, 2 };

void VP8EncFreeBitWriters(VP8Encoder* const enc) {
  int p;
  VP8BitWriterWipeOut(&enc->bw_);
  for (p = 0; p < enc->num_parts_; ++p) {
    VP8BitWriterWipeOut(enc->parts_ + p);
  }
}

static int PreLoopInitialize(VP8Encoder* const enc) {
  int p;
  int ok = 1;
  const int average_bytes_per_MB = kAverageBytesPerMB[enc->base_quant_ >> 4];
  const int bytes_per_parts =
      enc->mb_w_ * enc->mb_h_ * average_bytes_per_MB / enc->num_parts_;
  // Initialize the bit-writers
  for (p = 0; ok && p < enc->num_parts_; ++p) {
    ok = VP8BitWriterInit(enc->parts_ + p, bytes_per_parts);
  }
  if (!ok) {
    VP8EncFreeBitWriters(enc);  // malloc error occurred
    WebPEncodingSetError(enc->pic_, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  return ok;
}

typedef struct {  // struct for organizing convergence in either size or PSNR
  int is_first;
  float dq;
  float q, last_q;
  double value, last_value;   // PSNR or size
  double target;
  int do_size_search;
} PassStats;

static int InitPassStats(const VP8Encoder* const enc, PassStats* const s) {
  const uint64_t target_size = (uint64_t)enc->config_->target_size;
  const int do_size_search = (target_size != 0);
  const float target_PSNR = enc->config_->target_PSNR;

  s->is_first = 1;
  s->dq = 10.f;
  s->q = s->last_q = enc->config_->quality;
  s->target = do_size_search ? (double)target_size
            : (target_PSNR > 0.) ? target_PSNR
            : 40.;   // default, just in case
  s->value = s->last_value = 0.;
  s->do_size_search = do_size_search;
  return do_size_search;
}

static void ResetTokenStats(VP8Encoder* const enc) {
  VP8EncProba* const proba = &enc->proba_;
  memset(proba->stats_, 0, sizeof(proba->stats_));
}

static float Clamp(float v, float min, float max) {
  return (v < min) ? min : (v > max) ? max : v;
}

#define SNS_TO_DQ 0.9     // Scaling constant between the sns value and the QP
                          // power-law modulation. Must be strictly less than 1.

static double QualityToJPEGCompression(double c, double alpha) {
  // We map the complexity 'alpha' and quality setting 'c' to a compression
  // exponent empirically matched to the compression curve of libjpeg6b.
  // On average, the WebP output size will be roughly similar to that of a
  // JPEG file compressed with same quality factor.
  const double amin = 0.30;
  const double amax = 0.85;
  const double exp_min = 0.4;
  const double exp_max = 0.9;
  const double slope = (exp_min - exp_max) / (amax - amin);
  // Linearly interpolate 'expn' from exp_min to exp_max
  // in the [amin, amax] range.
  const double expn = (alpha > amax) ? exp_min
                    : (alpha < amin) ? exp_max
                    : exp_max + slope * (alpha - amin);
  const double v = pow(c, expn);
  return v;
}

// We want to emulate jpeg-like behaviour where the expected "good" quality
// is around q=75. Internally, our "good" middle is around c=50. So we
// map accordingly using linear piece-wise function
static double QualityToCompression(double c) {
  const double linear_c = (c < 0.75) ? c * (2. / 3.) : 2. * c - 1.;
  // The file size roughly scales as pow(quantizer, 3.). Actually, the
  // exponent is somewhere between 2.8 and 3.2, but we're mostly interested
  // in the mid-quant range. So we scale the compressibility inversely to
  // this power-law: quant ~= compression ^ 1/3. This law holds well for
  // low quant. Finer modeling for high-quant would make use of kAcTable[]
  // more explicitly.
  const double v = pow(linear_c, 1 / 3.);
  return v;
}

#define MID_ALPHA 64      // neutral value for susceptibility
#define MIN_ALPHA 30      // lowest usable value for susceptibility
#define MAX_ALPHA 100     // higher meaningful value for susceptibility

// Note: if you change the values below, remember that the max range
// allowed by the syntax for DQ_UV is [-16,16].
#define MAX_DQ_UV (6)
#define MIN_DQ_UV (-4)

static const uint16_t kAcTable[128] = {
  4,     5,   6,   7,   8,   9,  10,  11,
  12,   13,  14,  15,  16,  17,  18,  19,
  20,   21,  22,  23,  24,  25,  26,  27,
  28,   29,  30,  31,  32,  33,  34,  35,
  36,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  47,  48,  49,  50,  51,
  52,   53,  54,  55,  56,  57,  58,  60,
  62,   64,  66,  68,  70,  72,  74,  76,
  78,   80,  82,  84,  86,  88,  90,  92,
  94,   96,  98, 100, 102, 104, 106, 108,
  110, 112, 114, 116, 119, 122, 125, 128,
  131, 134, 137, 140, 143, 146, 149, 152,
  155, 158, 161, 164, 167, 170, 173, 177,
  181, 185, 189, 193, 197, 201, 205, 209,
  213, 217, 221, 225, 229, 234, 239, 245,
  249, 254, 259, 264, 269, 274, 279, 284
};

// This table gives, for a given sharpness, the filtering strength to be
// used (at least) in order to filter a given edge step delta.
// This is constructed by brute force inspection: for all delta, we iterate
// over all possible filtering strength / thresh until needs_filter() returns
// true.
#define MAX_DELTA_SIZE 64
static const uint8_t kLevelsFromDelta[8][MAX_DELTA_SIZE] = {
  { 0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 },
  { 0,  1,  2,  3,  5,  6,  7,  8,  9, 11, 12, 13, 14, 15, 17, 18,
    20, 21, 23, 24, 26, 27, 29, 30, 32, 33, 35, 36, 38, 39, 41, 42,
    44, 45, 47, 48, 50, 51, 53, 54, 56, 57, 59, 60, 62, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 },
  {  0,  1,  2,  3,  5,  6,  7,  8,  9, 11, 12, 13, 14, 16, 17, 19,
    20, 22, 23, 25, 26, 28, 29, 31, 32, 34, 35, 37, 38, 40, 41, 43,
    44, 46, 47, 49, 50, 52, 53, 55, 56, 58, 59, 61, 62, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 },
  {  0,  1,  2,  3,  5,  6,  7,  8,  9, 11, 12, 13, 15, 16, 18, 19,
    21, 22, 24, 25, 27, 28, 30, 31, 33, 34, 36, 37, 39, 40, 42, 43,
    45, 46, 48, 49, 51, 52, 54, 55, 57, 58, 60, 61, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 },
  {  0,  1,  2,  3,  5,  6,  7,  8,  9, 11, 12, 14, 15, 17, 18, 20,
    21, 23, 24, 26, 27, 29, 30, 32, 33, 35, 36, 38, 39, 41, 42, 44,
    45, 47, 48, 50, 51, 53, 54, 56, 57, 59, 60, 62, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 },
  {  0,  1,  2,  4,  5,  7,  8,  9, 11, 12, 13, 15, 16, 17, 19, 20,
    22, 23, 25, 26, 28, 29, 31, 32, 34, 35, 37, 38, 40, 41, 43, 44,
    46, 47, 49, 50, 52, 53, 55, 56, 58, 59, 61, 62, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 },
  {  0,  1,  2,  4,  5,  7,  8,  9, 11, 12, 13, 15, 16, 18, 19, 21,
    22, 24, 25, 27, 28, 30, 31, 33, 34, 36, 37, 39, 40, 42, 43, 45,
    46, 48, 49, 51, 52, 54, 55, 57, 58, 60, 61, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 },
  {  0,  1,  2,  4,  5,  7,  8,  9, 11, 12, 14, 15, 17, 18, 20, 21,
    23, 24, 26, 27, 29, 30, 32, 33, 35, 36, 38, 39, 41, 42, 44, 45,
    47, 48, 50, 51, 53, 54, 56, 57, 59, 60, 62, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63 }
};

int VP8FilterStrengthFromDelta(int sharpness, int delta) {
  const int pos = (delta < MAX_DELTA_SIZE) ? delta : MAX_DELTA_SIZE - 1;
  assert(sharpness >= 0 && sharpness <= 7);
  return kLevelsFromDelta[sharpness][pos];
}

// Very small filter-strength values have close to no visual effect. So we can
// save a little decoding-CPU by turning filtering off for these.
#define FSTRENGTH_CUTOFF 2

static void SetupFilterStrength(VP8Encoder* const enc) {
  int i;
  // level0 is in [0..500]. Using '-f 50' as filter_strength is mid-filtering.
  const int level0 = 5 * enc->config_->filter_strength;
  for (i = 0; i < NUM_MB_SEGMENTS; ++i) {
    VP8SegmentInfo* const m = &enc->dqm_[i];
    // We focus on the quantization of AC coeffs.
    const int qstep = kAcTable[clip(m->quant_, 0, 127)] >> 2;
    const int base_strength =
        VP8FilterStrengthFromDelta(enc->filter_hdr_.sharpness_, qstep);
    // Segments with lower complexity ('beta') will be less filtered.
    const int f = base_strength * level0 / (256 + m->beta_);
    m->fstrength_ = (f < FSTRENGTH_CUTOFF) ? 0 : (f > 63) ? 63 : f;
  }
  // We record the initial strength (mainly for the case of 1-segment only).
  enc->filter_hdr_.level_ = enc->dqm_[0].fstrength_;
  enc->filter_hdr_.simple_ = (enc->config_->filter_type == 0);
  enc->filter_hdr_.sharpness_ = enc->config_->filter_sharpness;
}

static int SegmentsAreEquivalent(const VP8SegmentInfo* const S1,
                                 const VP8SegmentInfo* const S2) {
  return (S1->quant_ == S2->quant_) && (S1->fstrength_ == S2->fstrength_);
}

static void SimplifySegments(VP8Encoder* const enc) {
  int map[NUM_MB_SEGMENTS] = { 0, 1, 2, 3 };
  // 'num_segments_' is previously validated and <= NUM_MB_SEGMENTS, but an
  // explicit check is needed to avoid a spurious warning about 'i' exceeding
  // array bounds of 'dqm_' with some compilers (noticed with gcc-4.9).
  const int num_segments = (enc->segment_hdr_.num_segments_ < NUM_MB_SEGMENTS)
                               ? enc->segment_hdr_.num_segments_
                               : NUM_MB_SEGMENTS;
  int num_final_segments = 1;
  int s1, s2;
  for (s1 = 1; s1 < num_segments; ++s1) {    // find similar segments
    const VP8SegmentInfo* const S1 = &enc->dqm_[s1];
    int found = 0;
    // check if we already have similar segment
    for (s2 = 0; s2 < num_final_segments; ++s2) {
      const VP8SegmentInfo* const S2 = &enc->dqm_[s2];
      if (SegmentsAreEquivalent(S1, S2)) {
        found = 1;
        break;
      }
    }
    map[s1] = s2;
    if (!found) {
      if (num_final_segments != s1) {
        enc->dqm_[num_final_segments] = enc->dqm_[s1];
      }
      ++num_final_segments;
    }
  }
  if (num_final_segments < num_segments) {  // Remap
    int i = enc->mb_w_ * enc->mb_h_;
    while (i-- > 0) enc->mb_info_[i].segment_ = map[enc->mb_info_[i].segment_];
    enc->segment_hdr_.num_segments_ = num_final_segments;
    // Replicate the trailing segment infos (it's mostly cosmetics)
    for (i = num_final_segments; i < num_segments; ++i) {
      enc->dqm_[i] = enc->dqm_[num_final_segments - 1];
    }
  }
}

static const uint8_t kDcTable[128] = {
  4,     5,   6,   7,   8,   9,  10,  10,
  11,   12,  13,  14,  15,  16,  17,  17,
  18,   19,  20,  20,  21,  21,  22,  22,
  23,   23,  24,  25,  25,  26,  27,  28,
  29,   30,  31,  32,  33,  34,  35,  36,
  37,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  46,  47,  48,  49,  50,
  51,   52,  53,  54,  55,  56,  57,  58,
  59,   60,  61,  62,  63,  64,  65,  66,
  67,   68,  69,  70,  71,  72,  73,  74,
  75,   76,  76,  77,  78,  79,  80,  81,
  82,   83,  84,  85,  86,  87,  88,  89,
  91,   93,  95,  96,  98, 100, 101, 102,
  104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 134, 136,
  138, 140, 143, 145, 148, 151, 154, 157
};

static const uint16_t kAcTable2[128] = {
  8,     8,   9,  10,  12,  13,  15,  17,
  18,   20,  21,  23,  24,  26,  27,  29,
  31,   32,  34,  35,  37,  38,  40,  41,
  43,   44,  46,  48,  49,  51,  52,  54,
  55,   57,  58,  60,  62,  63,  65,  66,
  68,   69,  71,  72,  74,  75,  77,  79,
  80,   82,  83,  85,  86,  88,  89,  93,
  96,   99, 102, 105, 108, 111, 114, 117,
  120, 124, 127, 130, 133, 136, 139, 142,
  145, 148, 151, 155, 158, 161, 164, 167,
  170, 173, 176, 179, 184, 189, 193, 198,
  203, 207, 212, 217, 221, 226, 230, 235,
  240, 244, 249, 254, 258, 263, 268, 274,
  280, 286, 292, 299, 305, 311, 317, 323,
  330, 336, 342, 348, 354, 362, 370, 379,
  385, 393, 401, 409, 416, 424, 432, 440
};

static const uint8_t kBiasMatrices[3][2] = {  // [luma-ac,luma-dc,chroma][dc,ac]
  { 96, 110 }, { 96, 108 }, { 110, 115 }
};

#define QFIX 17
#define BIAS(b)  ((b) << (QFIX - 8))

// Sharpening by (slightly) raising the hi-frequency coeffs.
// Hack-ish but helpful for mid-bitrate range. Use with care.
#define SHARPEN_BITS 11  // number of descaling bits for sharpening bias
static const uint8_t kFreqSharpening[16] = {
  0,  30, 60, 90,
  30, 60, 90, 90,
  60, 90, 90, 90,
  90, 90, 90, 90
};

// Returns the average quantizer
static int ExpandMatrix(VP8Matrix* const m, int type) {
  int i, sum;
  for (i = 0; i < 2; ++i) {
    const int is_ac_coeff = (i > 0);
    const int bias = kBiasMatrices[type][is_ac_coeff];
    m->iq_[i] = (1 << QFIX) / m->q_[i];
    m->bias_[i] = BIAS(bias);
    // zthresh_ is the exact value such that QUANTDIV(coeff, iQ, B) is:
    //   * zero if coeff <= zthresh
    //   * non-zero if coeff > zthresh
    m->zthresh_[i] = ((1 << QFIX) - 1 - m->bias_[i]) / m->iq_[i];
  }
  for (i = 2; i < 16; ++i) {
    m->q_[i] = m->q_[1];
    m->iq_[i] = m->iq_[1];
    m->bias_[i] = m->bias_[1];
    m->zthresh_[i] = m->zthresh_[1];
  }
  for (sum = 0, i = 0; i < 16; ++i) {
    if (type == 0) {  // we only use sharpening for AC luma coeffs
      m->sharpen_[i] = (kFreqSharpening[i] * m->q_[i]) >> SHARPEN_BITS;
    } else {
      m->sharpen_[i] = 0;
    }
    sum += m->q_[i];
  }
  return (sum + 8) >> 4;
}

static void CheckLambdaValue(int* const v) { if (*v < 1) *v = 1; }

static void SetupMatrices(VP8Encoder* enc) {
  int i;
  const int tlambda_scale =
    (enc->method_ >= 4) ? enc->config_->sns_strength
                        : 0;
  const int num_segments = enc->segment_hdr_.num_segments_;
  for (i = 0; i < num_segments; ++i) {
    VP8SegmentInfo* const m = &enc->dqm_[i];
    const int q = m->quant_;
    int q_i4, q_i16, q_uv;
    m->y1_.q_[0] = kDcTable[clip(q + enc->dq_y1_dc_, 0, 127)];
    m->y1_.q_[1] = kAcTable[clip(q,                  0, 127)];

    m->y2_.q_[0] = kDcTable[ clip(q + enc->dq_y2_dc_, 0, 127)] * 2;
    m->y2_.q_[1] = kAcTable2[clip(q + enc->dq_y2_ac_, 0, 127)];

    m->uv_.q_[0] = kDcTable[clip(q + enc->dq_uv_dc_, 0, 117)];
    m->uv_.q_[1] = kAcTable[clip(q + enc->dq_uv_ac_, 0, 127)];

    q_i4  = ExpandMatrix(&m->y1_, 0);
    q_i16 = ExpandMatrix(&m->y2_, 1);
    q_uv  = ExpandMatrix(&m->uv_, 2);

    m->lambda_i4_          = (3 * q_i4 * q_i4) >> 7;
    m->lambda_i16_         = (3 * q_i16 * q_i16);
    m->lambda_uv_          = (3 * q_uv * q_uv) >> 6;
    m->lambda_mode_        = (1 * q_i4 * q_i4) >> 7;
    m->lambda_trellis_i4_  = (7 * q_i4 * q_i4) >> 3;
    m->lambda_trellis_i16_ = (q_i16 * q_i16) >> 2;
    m->lambda_trellis_uv_  = (q_uv * q_uv) << 1;
    m->tlambda_            = (tlambda_scale * q_i4) >> 5;

    // none of these constants should be < 1
    CheckLambdaValue(&m->lambda_i4_);
    CheckLambdaValue(&m->lambda_i16_);
    CheckLambdaValue(&m->lambda_uv_);
    CheckLambdaValue(&m->lambda_mode_);
    CheckLambdaValue(&m->lambda_trellis_i4_);
    CheckLambdaValue(&m->lambda_trellis_i16_);
    CheckLambdaValue(&m->lambda_trellis_uv_);
    CheckLambdaValue(&m->tlambda_);

    m->min_disto_ = 20 * m->y1_.q_[0];   // quantization-aware min disto
    m->max_edge_  = 0;

    m->i4_penalty_ = 1000 * q_i4 * q_i4;
  }
}

void VP8SetSegmentParams(VP8Encoder* const enc, float quality) {
  int i;
  int dq_uv_ac, dq_uv_dc;
  const int num_segments = enc->segment_hdr_.num_segments_;
  const double amp = SNS_TO_DQ * enc->config_->sns_strength / 100. / 128.;
  const double Q = quality / 100.;
  const double c_base = enc->config_->emulate_jpeg_size ?
      QualityToJPEGCompression(Q, enc->alpha_ / 255.) :
      QualityToCompression(Q);
  for (i = 0; i < num_segments; ++i) {
    // We modulate the base coefficient to accommodate for the quantization
    // susceptibility and allow denser segments to be quantized more.
    const double expn = 1. - amp * enc->dqm_[i].alpha_;
    const double c = pow(c_base, expn);
    const int q = (int)(127. * (1. - c));
    assert(expn > 0.);
    enc->dqm_[i].quant_ = clip(q, 0, 127);
  }

  // purely indicative in the bitstream (except for the 1-segment case)
  enc->base_quant_ = enc->dqm_[0].quant_;

  // fill-in values for the unused segments (required by the syntax)
  for (i = num_segments; i < NUM_MB_SEGMENTS; ++i) {
    enc->dqm_[i].quant_ = enc->base_quant_;
  }

  // uv_alpha_ is normally spread around ~60. The useful range is
  // typically ~30 (quite bad) to ~100 (ok to decimate UV more).
  // We map it to the safe maximal range of MAX/MIN_DQ_UV for dq_uv.
  dq_uv_ac = (enc->uv_alpha_ - MID_ALPHA) * (MAX_DQ_UV - MIN_DQ_UV)
                                          / (MAX_ALPHA - MIN_ALPHA);
  // we rescale by the user-defined strength of adaptation
  dq_uv_ac = dq_uv_ac * enc->config_->sns_strength / 100;
  // and make it safe.
  dq_uv_ac = clip(dq_uv_ac, MIN_DQ_UV, MAX_DQ_UV);
  // We also boost the dc-uv-quant a little, based on sns-strength, since
  // U/V channels are quite more reactive to high quants (flat DC-blocks
  // tend to appear, and are unpleasant).
  dq_uv_dc = -4 * enc->config_->sns_strength / 100;
  dq_uv_dc = clip(dq_uv_dc, -15, 15);   // 4bit-signed max allowed

  enc->dq_y1_dc_ = 0;       // TODO(skal): dq-lum
  enc->dq_y2_dc_ = 0;
  enc->dq_y2_ac_ = 0;
  enc->dq_uv_dc_ = dq_uv_dc;
  enc->dq_uv_ac_ = dq_uv_ac;

  SetupFilterStrength(enc);   // initialize segments' filtering, eventually

  if (num_segments > 1) SimplifySegments(enc);

  SetupMatrices(enc);         // finalize quantization matrices
}

static int GetProba(int a, int b) {
  const int total = a + b;
  return (total == 0) ? 255     // that's the default probability.
                      : (255 * a + total / 2) / total;  // rounded proba
}

const uint16_t VP8EntropyCost[256] = {
  1792, 1792, 1792, 1536, 1536, 1408, 1366, 1280, 1280, 1216,
  1178, 1152, 1110, 1076, 1061, 1024, 1024,  992,  968,  951,
   939,  911,  896,  878,  871,  854,  838,  820,  811,  794,
   786,  768,  768,  752,  740,  732,  720,  709,  704,  690,
   683,  672,  666,  655,  647,  640,  631,  622,  615,  607,
   598,  592,  586,  576,  572,  564,  559,  555,  547,  541,
   534,  528,  522,  512,  512,  504,  500,  494,  488,  483,
   477,  473,  467,  461,  458,  452,  448,  443,  438,  434,
   427,  424,  419,  415,  410,  406,  403,  399,  394,  390,
   384,  384,  377,  374,  370,  366,  362,  359,  355,  351,
   347,  342,  342,  336,  333,  330,  326,  323,  320,  316,
   312,  308,  305,  302,  299,  296,  293,  288,  287,  283,
   280,  277,  274,  272,  268,  266,  262,  256,  256,  256,
   251,  248,  245,  242,  240,  237,  234,  232,  228,  226,
   223,  221,  218,  216,  214,  211,  208,  205,  203,  201,
   198,  196,  192,  191,  188,  187,  183,  181,  179,  176,
   175,  171,  171,  168,  165,  163,  160,  159,  156,  154,
   152,  150,  148,  146,  144,  142,  139,  138,  135,  133,
   131,  128,  128,  125,  123,  121,  119,  117,  115,  113,
   111,  110,  107,  105,  103,  102,  100,   98,   96,   94,
    92,   91,   89,   86,   86,   83,   82,   80,   77,   76,
    74,   73,   71,   69,   67,   66,   64,   63,   61,   59,
    57,   55,   54,   52,   51,   49,   47,   46,   44,   43,
    41,   40,   38,   36,   35,   33,   32,   30,   29,   27,
    25,   24,   22,   21,   19,   18,   16,   15,   13,   12,
    10,    9,    7,    6,    4,    3
};

// Cost of coding one event with probability 'proba'.
static int VP8BitCost(int bit, uint8_t proba) {
  return !bit ? VP8EntropyCost[proba] : VP8EntropyCost[255 - proba];
}

static void ResetSegments(VP8Encoder* const enc) {
  int n;
  for (n = 0; n < enc->mb_w_ * enc->mb_h_; ++n) {
    enc->mb_info_[n].segment_ = 0;
  }
}

static void SetSegmentProbas(VP8Encoder* const enc) {
  int p[NUM_MB_SEGMENTS] = { 0 };
  int n;

  for (n = 0; n < enc->mb_w_ * enc->mb_h_; ++n) {
    const VP8MBInfo* const mb = &enc->mb_info_[n];
    ++p[mb->segment_];
  }
#if !defined(WEBP_DISABLE_STATS)
  if (enc->pic_->stats != NULL) {
    for (n = 0; n < NUM_MB_SEGMENTS; ++n) {
      enc->pic_->stats->segment_size[n] = p[n];
    }
  }
#endif
  if (enc->segment_hdr_.num_segments_ > 1) {
    uint8_t* const probas = enc->proba_.segments_;
    probas[0] = GetProba(p[0] + p[1], p[2] + p[3]);
    probas[1] = GetProba(p[0], p[1]);
    probas[2] = GetProba(p[2], p[3]);

    enc->segment_hdr_.update_map_ =
        (probas[0] != 255) || (probas[1] != 255) || (probas[2] != 255);
    if (!enc->segment_hdr_.update_map_) ResetSegments(enc);
    enc->segment_hdr_.size_ =
        p[0] * (VP8BitCost(0, probas[0]) + VP8BitCost(0, probas[1])) +
        p[1] * (VP8BitCost(0, probas[0]) + VP8BitCost(1, probas[1])) +
        p[2] * (VP8BitCost(1, probas[0]) + VP8BitCost(0, probas[2])) +
        p[3] * (VP8BitCost(1, probas[0]) + VP8BitCost(1, probas[2]));
  } else {
    enc->segment_hdr_.update_map_ = 0;
    enc->segment_hdr_.size_ = 0;
  }
}

// For each given level, the following table gives the pattern of contexts to
// use for coding it (in [][0]) as well as the bit value to use for each
// context (in [][1]).
const uint16_t VP8LevelCodes[MAX_VARIABLE_LEVEL][2] = {
                  {0x001, 0x000}, {0x007, 0x001}, {0x00f, 0x005},
  {0x00f, 0x00d}, {0x033, 0x003}, {0x033, 0x003}, {0x033, 0x023},
  {0x033, 0x023}, {0x033, 0x023}, {0x033, 0x023}, {0x0d3, 0x013},
  {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x013},
  {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x153}
};

static int VariableLevelCost(int level, const uint8_t probas[NUM_PROBAS]) {
  int pattern = VP8LevelCodes[level - 1][0];
  int bits = VP8LevelCodes[level - 1][1];
  int cost = 0;
  int i;
  for (i = 2; pattern; ++i) {
    if (pattern & 1) {
      cost += VP8BitCost(bits & 1, probas[i]);
    }
    bits >>= 1;
    pattern >>= 1;
  }
  return cost;
}

const uint8_t VP8EncBands[16 + 1] = {
  0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
  0  // sentinel
};

void VP8CalculateLevelCosts(VP8EncProba* const proba) {
  int ctype, band, ctx;

  if (!proba->dirty_) return;  // nothing to do.

  for (ctype = 0; ctype < NUM_TYPES; ++ctype) {
    int n;
    for (band = 0; band < NUM_BANDS; ++band) {
      for (ctx = 0; ctx < NUM_CTX; ++ctx) {
        const uint8_t* const p = proba->coeffs_[ctype][band][ctx];
        uint16_t* const table = proba->level_cost_[ctype][band][ctx];
        const int cost0 = (ctx > 0) ? VP8BitCost(1, p[0]) : 0;
        const int cost_base = VP8BitCost(1, p[1]) + cost0;
        int v;
        table[0] = VP8BitCost(0, p[1]) + cost0;
        for (v = 1; v <= MAX_VARIABLE_LEVEL; ++v) {
          table[v] = cost_base + VariableLevelCost(v, p);
        }
        // Starting at level 67 and up, the variable part of the cost is
        // actually constant.
      }
    }
    for (n = 0; n < 16; ++n) {    // replicate bands. We don't need to sentinel.
      for (ctx = 0; ctx < NUM_CTX; ++ctx) {
        proba->remapped_costs_[ctype][n][ctx] =
            proba->level_cost_[ctype][VP8EncBands[n]][ctx];
      }
    }
  }
  proba->dirty_ = 0;
}

static void ResetStats(VP8Encoder* const enc) {
  VP8EncProba* const proba = &enc->proba_;
  VP8CalculateLevelCosts(proba);
  proba->nb_skip_ = 0;
}

static void ResetSSE(VP8Encoder* const enc) {
  enc->sse_[0] = 0;
  enc->sse_[1] = 0;
  enc->sse_[2] = 0;
  // Note: enc->sse_[3] is managed by alpha.c
  enc->sse_count_ = 0;
}

static void SetLoopParams(VP8Encoder* const enc, float q) {
  // Make sure the quality parameter is inside valid bounds
  q = Clamp(q, 0.f, 100.f);

  VP8SetSegmentParams(enc, q);      // setup segment quantizations and filters
  SetSegmentProbas(enc);            // compute segment probabilities

  ResetStats(enc);
  ResetSSE(enc);
}

// Handy transient struct to accumulate score and info during RD-optimization
// and mode evaluation.
typedef struct {
  score_t D, SD;              // Distortion, spectral distortion
  score_t H, R, score;        // header bits, rate, score.
  int16_t y_dc_levels[16];    // Quantized levels for luma-DC, luma-AC, chroma.
  int16_t y_ac_levels[16][16];
  int16_t uv_levels[4 + 4][16];
  int mode_i16;               // mode number for intra16 prediction
  uint8_t modes_i4[16];       // mode numbers for intra4 predictions
  int mode_uv;                // mode number of chroma prediction
  uint32_t nz;                // non-zero blocks
  int8_t derr[2][3];          // DC diffusion errors for U/V for blocks #1/2/3
} VP8ModeScore;

// Init/Copy the common fields in score.
static void InitScore(VP8ModeScore* const rd) {
  rd->D  = 0;
  rd->SD = 0;
  rd->R  = 0;
  rd->H  = 0;
  rd->nz = 0;
  rd->score = MAX_COST;
}

static void FTransformWHT_C(const int16_t* in, int16_t* out) {
  // input is 12b signed
  int32_t tmp[16];
  int i;
  for (i = 0; i < 4; ++i, in += 64) {
    const int a0 = (in[0 * 16] + in[2 * 16]);  // 13b
    const int a1 = (in[1 * 16] + in[3 * 16]);
    const int a2 = (in[1 * 16] - in[3 * 16]);
    const int a3 = (in[0 * 16] - in[2 * 16]);
    tmp[0 + i * 4] = a0 + a1;   // 14b
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[8 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[12+ i]);
    const int a2 = (tmp[4 + i] - tmp[12+ i]);
    const int a3 = (tmp[0 + i] - tmp[8 + i]);
    const int b0 = a0 + a1;    // 16b
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;
    out[ 0 + i] = b0 >> 1;     // 15b
    out[ 4 + i] = b1 >> 1;
    out[ 8 + i] = b2 >> 1;
    out[12 + i] = b3 >> 1;
  }
}

static void FTransform2_C(const uint8_t* src, const uint8_t* ref,
                          int16_t* out) {
	FTransform_C(src, ref, out);
	FTransform_C(src + 4, ref + 4, out + 16);
}

static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

static int QUANTDIV(uint32_t n, uint32_t iQ, uint32_t B) {
  return (int)((n * iQ + B) >> QFIX);
}

// Simple quantization
static int QuantizeBlock_C(int16_t in[16], int16_t out[16],
                           const VP8Matrix* const mtx) {
  int last = -1;
  int n;
  for (n = 0; n < 16; ++n) {
    const int j = kZigzag[n];
    const int sign = (in[j] < 0);
    const uint32_t coeff = (sign ? -in[j] : in[j]) + mtx->sharpen_[j];
    if (coeff > mtx->zthresh_[j]) {
      const uint32_t Q = mtx->q_[j];
      const uint32_t iQ = mtx->iq_[j];
      const uint32_t B = mtx->bias_[j];
      int level = QUANTDIV(coeff, iQ, B);
      if (level > MAX_LEVEL) level = MAX_LEVEL;
      if (sign) level = -level;
      in[j] = level * (int)Q;
      out[n] = level;
      if (level) last = n;
    } else {
      out[n] = 0;
      in[j] = 0;
    }
  }
  return (last >= 0);
}

static int Quantize2Blocks_C(int16_t in[32], int16_t out[32],
                             const VP8Matrix* const mtx) {
  int nz;
  nz  = QuantizeBlock_C(in + 0 * 16, out + 0 * 16, mtx) << 0;
  nz |= QuantizeBlock_C(in + 1 * 16, out + 1 * 16, mtx) << 1;
  return nz;
}

static void TransformWHT_C(const int16_t* in, int16_t* out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
    const int a0 = in[0 + i] + in[12 + i];
    const int a1 = in[4 + i] + in[ 8 + i];
    const int a2 = in[4 + i] - in[ 8 + i];
    const int a3 = in[0 + i] - in[12 + i];
    tmp[0  + i] = a0 + a1;
    tmp[8  + i] = a0 - a1;
    tmp[4  + i] = a3 + a2;
    tmp[12 + i] = a3 - a2;
  }
  for (i = 0; i < 4; ++i) {
    const int dc = tmp[0 + i * 4] + 3;    // w/ rounder
    const int a0 = dc             + tmp[3 + i * 4];
    const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
    const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
    const int a3 = dc             - tmp[3 + i * 4];
    out[ 0] = (a0 + a1) >> 3;
    out[16] = (a3 + a2) >> 3;
    out[32] = (a0 - a1) >> 3;
    out[48] = (a3 - a2) >> 3;
    out += 64;
  }
}

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(ref[(x) + (y) * BPS] + ((v) >> 3))

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)

static void ITransformOne(const uint8_t* ref, const int16_t* in,
                                      uint8_t* dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
    const int a = in[0] + in[8];
    const int b = in[0] - in[8];
    const int c = MUL(in[4], kC2) - MUL(in[12], kC1);
    const int d = MUL(in[4], kC1) + MUL(in[12], kC2);
    tmp[0] = a + d;
    tmp[1] = b + c;
    tmp[2] = b - c;
    tmp[3] = a - d;
    tmp += 4;
    in++;
  }

  tmp = C;
  for (i = 0; i < 4; ++i) {    // horizontal pass
    const int dc = tmp[0] + 4;
    const int a =  dc +  tmp[8];
    const int b =  dc -  tmp[8];
    const int c = MUL(tmp[4], kC2) - MUL(tmp[12], kC1);
    const int d = MUL(tmp[4], kC1) + MUL(tmp[12], kC2);
    STORE(0, i, a + d);
    STORE(1, i, b + c);
    STORE(2, i, b - c);
    STORE(3, i, a - d);
    tmp++;
  }
}

static void ITransform_C(const uint8_t* ref, const int16_t* in, uint8_t* dst,
                         int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

static int ReconstructIntra16(VP8EncIterator* const it,
                              VP8ModeScore* const rd,
                              uint8_t* const yuv_out,
                              int mode) {
  const VP8Encoder* const enc = it->enc_;
  const uint8_t* const ref = it->yuv_p_ + VP8I16ModeOffsets[mode];
  const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC;
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  int nz = 0;
  int n;
  int16_t tmp[16][16], dc_tmp[16];

  for (n = 0; n < 16; n += 2) {
	  FTransform2_C(src + VP8Scan[n], ref + VP8Scan[n], tmp[n]);
  }

  FTransformWHT_C(tmp[0], dc_tmp);

  nz |= QuantizeBlock_C(dc_tmp, rd->y_dc_levels, &dqm->y2_) << 24;

  for (n = 0; n < 16; n += 2) {
    // Zero-out the first coeff, so that: a) nz is correct below, and
    // b) finding 'last' non-zero coeffs in SetResidualCoeffs() is simplified.
    tmp[n][0] = tmp[n + 1][0] = 0;
    nz |= Quantize2Blocks_C(tmp[n], rd->y_ac_levels[n], &dqm->y1_) << n;
    assert(rd->y_ac_levels[n + 0][0] == 0);
    assert(rd->y_ac_levels[n + 1][0] == 0);
  }


  // Transform back
  TransformWHT_C(dc_tmp, tmp[0]);

  for (n = 0; n < 16; n += 2) {
	  ITransform_C(ref + VP8Scan[n], tmp[n], yuv_out + VP8Scan[n], 1);
  }

  return nz;
}

static int GetSSE(const uint8_t* a, const uint8_t* b,
                              int w, int h) {
  int count = 0;
  int y, x;
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      const int diff = (int)a[x] - b[x];
      count += diff * diff;
    }
    a += BPS;
    b += BPS;
  }
  return count;
}

static int SSE16x16_C(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 16);
}
static int SSE16x8_C(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 8);
}
static int SSE8x8_C(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 8, 8);
}
static int SSE4x4_C(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 4, 4);
}

#define MULT_8B(a, b) (((a) * (b) + 128) >> 8)

static int TTransform(const uint8_t* in, const uint16_t* w) {
  int sum = 0;
  int tmp[16];
  int i;
  // horizontal pass
  for (i = 0; i < 4; ++i, in += BPS) {
    const int a0 = in[0] + in[2];
    const int a1 = in[1] + in[3];
    const int a2 = in[1] - in[3];
    const int a3 = in[0] - in[2];
    tmp[0 + i * 4] = a0 + a1;
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  // vertical pass
  for (i = 0; i < 4; ++i, ++w) {
    const int a0 = tmp[0 + i] + tmp[8 + i];
    const int a1 = tmp[4 + i] + tmp[12+ i];
    const int a2 = tmp[4 + i] - tmp[12+ i];
    const int a3 = tmp[0 + i] - tmp[8 + i];
    const int b0 = a0 + a1;
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;

    sum += w[ 0] * abs(b0);
    sum += w[ 4] * abs(b1);
    sum += w[ 8] * abs(b2);
    sum += w[12] * abs(b3);
  }
  return sum;
}

static int Disto4x4_C(const uint8_t* const a, const uint8_t* const b,
                      const uint16_t* const w) {
  const int sum1 = TTransform(a, w);
  const int sum2 = TTransform(b, w);
  return abs(sum2 - sum1) >> 5;
}

static int Disto16x16_C(const uint8_t* const a, const uint8_t* const b,
                        const uint16_t* const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4_C(a + x + y, b + x + y, w);
    }
  }
  return D;
}

static const uint16_t kWeightY[16] = {
  38, 32, 20, 9, 32, 28, 17, 7, 20, 17, 10, 4, 9, 7, 4, 2
};

#define RD_DISTO_MULT      256  // distortion multiplier (equivalent of lambda)

static void SetRDScore(int lambda, VP8ModeScore* const rd) {
  rd->score = (rd->R + rd->H) * lambda + RD_DISTO_MULT * (rd->D + rd->SD);
}

static void SwapModeScore(VP8ModeScore** a, VP8ModeScore** b) {
  VP8ModeScore* const tmp = *a;
  *a = *b;
  *b = tmp;
}

static void SwapPtr(uint8_t** a, uint8_t** b) {
  uint8_t* const tmp = *a;
  *a = *b;
  *b = tmp;
}

static void SwapOut(VP8EncIterator* const it) {
  SwapPtr(&it->yuv_out_, &it->yuv_out2_);
}

static void StoreMaxDelta(VP8SegmentInfo* const dqm, const int16_t DCs[16]) {
  // We look at the first three AC coefficients to determine what is the average
  // delta between each sub-4x4 block.
  const int v0 = abs(DCs[1]);
  const int v1 = abs(DCs[2]);
  const int v2 = abs(DCs[4]);
  int max_v = (v1 > v0) ? v1 : v0;
  max_v = (v2 > max_v) ? v2 : max_v;
  if (max_v > dqm->max_edge_) dqm->max_edge_ = max_v;
}

// These are the fixed probabilities (in the coding trees) turned into bit-cost
// by calling VP8BitCost().
const uint16_t VP8FixedCostsUV[4] = { 302, 984, 439, 642 };
// note: these values include the fixed VP8BitCost(1, 145) mode selection cost.
const uint16_t VP8FixedCostsI16[4] = { 663, 919, 872, 919 };
const uint16_t VP8FixedCostsI4[NUM_BMODES][NUM_BMODES][NUM_BMODES] = {
  { {   40, 1151, 1723, 1874, 2103, 2019, 1628, 1777, 2226, 2137 },
    {  192,  469, 1296, 1308, 1849, 1794, 1781, 1703, 1713, 1522 },
    {  142,  910,  762, 1684, 1849, 1576, 1460, 1305, 1801, 1657 },
    {  559,  641, 1370,  421, 1182, 1569, 1612, 1725,  863, 1007 },
    {  299, 1059, 1256, 1108,  636, 1068, 1581, 1883,  869, 1142 },
    {  277, 1111,  707, 1362, 1089,  672, 1603, 1541, 1545, 1291 },
    {  214,  781, 1609, 1303, 1632, 2229,  726, 1560, 1713,  918 },
    {  152, 1037, 1046, 1759, 1983, 2174, 1358,  742, 1740, 1390 },
    {  512, 1046, 1420,  753,  752, 1297, 1486, 1613,  460, 1207 },
    {  424,  827, 1362,  719, 1462, 1202, 1199, 1476, 1199,  538 } },
  { {  240,  402, 1134, 1491, 1659, 1505, 1517, 1555, 1979, 2099 },
    {  467,  242,  960, 1232, 1714, 1620, 1834, 1570, 1676, 1391 },
    {  500,  455,  463, 1507, 1699, 1282, 1564,  982, 2114, 2114 },
    {  672,  643, 1372,  331, 1589, 1667, 1453, 1938,  996,  876 },
    {  458,  783, 1037,  911,  738,  968, 1165, 1518,  859, 1033 },
    {  504,  815,  504, 1139, 1219,  719, 1506, 1085, 1268, 1268 },
    {  333,  630, 1445, 1239, 1883, 3672,  799, 1548, 1865,  598 },
    {  399,  644,  746, 1342, 1856, 1350, 1493,  613, 1855, 1015 },
    {  622,  749, 1205,  608, 1066, 1408, 1290, 1406,  546,  971 },
    {  500,  753, 1041,  668, 1230, 1617, 1297, 1425, 1383,  523 } },
  { {  394,  553,  523, 1502, 1536,  981, 1608, 1142, 1666, 2181 },
    {  655,  430,  375, 1411, 1861, 1220, 1677, 1135, 1978, 1553 },
    {  690,  640,  245, 1954, 2070, 1194, 1528,  982, 1972, 2232 },
    {  559,  834,  741,  867, 1131,  980, 1225,  852, 1092,  784 },
    {  690,  875,  516,  959,  673,  894, 1056, 1190, 1528, 1126 },
    {  740,  951,  384, 1277, 1177,  492, 1579, 1155, 1846, 1513 },
    {  323,  775, 1062, 1776, 3062, 1274,  813, 1188, 1372,  655 },
    {  488,  971,  484, 1767, 1515, 1775, 1115,  503, 1539, 1461 },
    {  740, 1006,  998,  709,  851, 1230, 1337,  788,  741,  721 },
    {  522, 1073,  573, 1045, 1346,  887, 1046, 1146, 1203,  697 } },
  { {  105,  864, 1442, 1009, 1934, 1840, 1519, 1920, 1673, 1579 },
    {  534,  305, 1193,  683, 1388, 2164, 1802, 1894, 1264, 1170 },
    {  305,  518,  877, 1108, 1426, 3215, 1425, 1064, 1320, 1242 },
    {  683,  732, 1927,  257, 1493, 2048, 1858, 1552, 1055,  947 },
    {  394,  814, 1024,  660,  959, 1556, 1282, 1289,  893, 1047 },
    {  528,  615,  996,  940, 1201,  635, 1094, 2515,  803, 1358 },
    {  347,  614, 1609, 1187, 3133, 1345, 1007, 1339, 1017,  667 },
    {  218,  740,  878, 1605, 3650, 3650, 1345,  758, 1357, 1617 },
    {  672,  750, 1541,  558, 1257, 1599, 1870, 2135,  402, 1087 },
    {  592,  684, 1161,  430, 1092, 1497, 1475, 1489, 1095,  822 } },
  { {  228, 1056, 1059, 1368,  752,  982, 1512, 1518,  987, 1782 },
    {  494,  514,  818,  942,  965,  892, 1610, 1356, 1048, 1363 },
    {  512,  648,  591, 1042,  761,  991, 1196, 1454, 1309, 1463 },
    {  683,  749, 1043,  676,  841, 1396, 1133, 1138,  654,  939 },
    {  622, 1101, 1126,  994,  361, 1077, 1203, 1318,  877, 1219 },
    {  631, 1068,  857, 1650,  651,  477, 1650, 1419,  828, 1170 },
    {  555,  727, 1068, 1335, 3127, 1339,  820, 1331, 1077,  429 },
    {  504,  879,  624, 1398,  889,  889, 1392,  808,  891, 1406 },
    {  683, 1602, 1289,  977,  578,  983, 1280, 1708,  406, 1122 },
    {  399,  865, 1433, 1070, 1072,  764,  968, 1477, 1223,  678 } },
  { {  333,  760,  935, 1638, 1010,  529, 1646, 1410, 1472, 2219 },
    {  512,  494,  750, 1160, 1215,  610, 1870, 1868, 1628, 1169 },
    {  572,  646,  492, 1934, 1208,  603, 1580, 1099, 1398, 1995 },
    {  786,  789,  942,  581, 1018,  951, 1599, 1207,  731,  768 },
    {  690, 1015,  672, 1078,  582,  504, 1693, 1438, 1108, 2897 },
    {  768, 1267,  571, 2005, 1243,  244, 2881, 1380, 1786, 1453 },
    {  452,  899, 1293,  903, 1311, 3100,  465, 1311, 1319,  813 },
    {  394,  927,  942, 1103, 1358, 1104,  946,  593, 1363, 1109 },
    {  559, 1005, 1007, 1016,  658, 1173, 1021, 1164,  623, 1028 },
    {  564,  796,  632, 1005, 1014,  863, 2316, 1268,  938,  764 } },
  { {  266,  606, 1098, 1228, 1497, 1243,  948, 1030, 1734, 1461 },
    {  366,  585,  901, 1060, 1407, 1247,  876, 1134, 1620, 1054 },
    {  452,  565,  542, 1729, 1479, 1479, 1016,  886, 2938, 1150 },
    {  555, 1088, 1533,  950, 1354,  895,  834, 1019, 1021,  496 },
    {  704,  815, 1193,  971,  973,  640, 1217, 2214,  832,  578 },
    {  672, 1245,  579,  871,  875,  774,  872, 1273, 1027,  949 },
    {  296, 1134, 2050, 1784, 1636, 3425,  442, 1550, 2076,  722 },
    {  342,  982, 1259, 1846, 1848, 1848,  622,  568, 1847, 1052 },
    {  555, 1064, 1304,  828,  746, 1343, 1075, 1329, 1078,  494 },
    {  288, 1167, 1285, 1174, 1639, 1639,  833, 2254, 1304,  509 } },
  { {  342,  719,  767, 1866, 1757, 1270, 1246,  550, 1746, 2151 },
    {  483,  653,  694, 1509, 1459, 1410, 1218,  507, 1914, 1266 },
    {  488,  757,  447, 2979, 1813, 1268, 1654,  539, 1849, 2109 },
    {  522, 1097, 1085,  851, 1365, 1111,  851,  901,  961,  605 },
    {  709,  716,  841,  728,  736,  945,  941,  862, 2845, 1057 },
    {  512, 1323,  500, 1336, 1083,  681, 1342,  717, 1604, 1350 },
    {  452, 1155, 1372, 1900, 1501, 3290,  311,  944, 1919,  922 },
    {  403, 1520,  977, 2132, 1733, 3522, 1076,  276, 3335, 1547 },
    {  559, 1374, 1101,  615,  673, 2462,  974,  795,  984,  984 },
    {  547, 1122, 1062,  812, 1410,  951, 1140,  622, 1268,  651 } },
  { {  165,  982, 1235,  938, 1334, 1366, 1659, 1578,  964, 1612 },
    {  592,  422,  925,  847, 1139, 1112, 1387, 2036,  861, 1041 },
    {  403,  837,  732,  770,  941, 1658, 1250,  809, 1407, 1407 },
    {  896,  874, 1071,  381, 1568, 1722, 1437, 2192,  480, 1035 },
    {  640, 1098, 1012, 1032,  684, 1382, 1581, 2106,  416,  865 },
    {  559, 1005,  819,  914,  710,  770, 1418,  920,  838, 1435 },
    {  415, 1258, 1245,  870, 1278, 3067,  770, 1021, 1287,  522 },
    {  406,  990,  601, 1009, 1265, 1265, 1267,  759, 1017, 1277 },
    {  968, 1182, 1329,  788, 1032, 1292, 1705, 1714,  203, 1403 },
    {  732,  877, 1279,  471,  901, 1161, 1545, 1294,  755,  755 } },
  { {  111,  931, 1378, 1185, 1933, 1648, 1148, 1714, 1873, 1307 },
    {  406,  414, 1030, 1023, 1910, 1404, 1313, 1647, 1509,  793 },
    {  342,  640,  575, 1088, 1241, 1349, 1161, 1350, 1756, 1502 },
    {  559,  766, 1185,  357, 1682, 1428, 1329, 1897, 1219,  802 },
    {  473,  909, 1164,  771,  719, 2508, 1427, 1432,  722,  782 },
    {  342,  892,  785, 1145, 1150,  794, 1296, 1550,  973, 1057 },
    {  208, 1036, 1326, 1343, 1606, 3395,  815, 1455, 1618,  712 },
    {  228,  928,  890, 1046, 3499, 1711,  994,  829, 1720, 1318 },
    {  768,  724, 1058,  636,  991, 1075, 1319, 1324,  616,  825 },
    {  305, 1167, 1358,  899, 1587, 1587,  987, 1988, 1332,  501 } }
};

static void PickBestIntra16(VP8EncIterator* const it, VP8ModeScore* rd) {
  const int kNumBlocks = 16;
  VP8SegmentInfo* const dqm = &it->enc_->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_i16_;
  const int tlambda = dqm->tlambda_;
  const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC;
  VP8ModeScore rd_tmp;
  VP8ModeScore* rd_cur = &rd_tmp;
  VP8ModeScore* rd_best = rd;
  int mode;

  rd->mode_i16 = -1;
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    uint8_t* const tmp_dst = it->yuv_out2_ + Y_OFF_ENC;  // scratch buffer
    rd_cur->mode_i16 = mode;

    // Reconstruct
    rd_cur->nz = ReconstructIntra16(it, rd_cur, tmp_dst, mode);

    // Measure RD-score
    rd_cur->D = SSE16x16_C(src, tmp_dst);
    rd_cur->SD =
        tlambda ? MULT_8B(tlambda, Disto16x16_C(src, tmp_dst, kWeightY)) : 0;
    rd_cur->H = VP8FixedCostsI16[mode];
    /*rd_cur->R = VP8GetCostLuma16(it, rd_cur);

    if (mode > 0 &&
        IsFlat(rd_cur->y_ac_levels[0], kNumBlocks, FLATNESS_LIMIT_I16)) {
      // penalty to avoid flat area to be mispredicted by complex mode
      rd_cur->R += FLATNESS_PENALTY * kNumBlocks;
    }*/

	int64_t test_R = 0;
	int y, x;
	for (y = 1; y < 16; ++y) {
	  for (x = 1; x < 16; ++x) {
	    test_R += rd_cur->y_ac_levels[y][x] * rd_cur->y_ac_levels[y][x];
	  }
	}
	for (y = 0; y < 16; ++y) {
	  test_R += rd_cur->y_dc_levels[y] * rd_cur->y_dc_levels[y];
	}
	rd_cur->R = test_R << 10;
	//fprintf(stdout, "%llu:%llu\n", rd_cur->R, test_R);

    // Since we always examine Intra16 first, we can overwrite *rd directly.
    SetRDScore(lambda, rd_cur);

    if (mode == 0 || rd_cur->score < rd_best->score) {
      SwapModeScore(&rd_cur, &rd_best);
      SwapOut(it);
    }
  }
  if (rd_best != rd) {
    memcpy(rd, rd_best, sizeof(*rd));
  }
  SetRDScore(dqm->lambda_mode_, rd);   // finalize score for mode decision.
  VP8SetIntra16Mode(it, rd->mode_i16);

  // we have a blocky macroblock (only DCs are non-zero) with fairly high
  // distortion, record max delta so we can later adjust the minimal filtering
  // strength needed to smooth these blocks out.
  if ((rd->nz & 0x100ffff) == 0x1000000 && rd->D > dqm->min_disto_) {
    StoreMaxDelta(dqm, rd->y_dc_levels);
  }
}

static int ReconstructIntra4(VP8EncIterator* const it,
                             int16_t levels[16],
                             const uint8_t* const src,
                             uint8_t* const yuv_out,
                             int mode) {
  const VP8Encoder* const enc = it->enc_;
  const uint8_t* const ref = it->yuv_p_ + VP8I4ModeOffsets[mode];
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  int nz = 0;
  int16_t tmp[16];

  FTransform_C(src, ref, tmp);

  nz = QuantizeBlock_C(tmp, levels, &dqm->y1_);

  ITransform_C(ref, tmp, yuv_out, 0);

  return nz;
}

static void CopyScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  = src->D;
  dst->SD = src->SD;
  dst->R  = src->R;
  dst->H  = src->H;
  dst->nz = src->nz;      // note that nz is not accumulated, but just copied.
  dst->score = src->score;
}

static void AddScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  += src->D;
  dst->SD += src->SD;
  dst->R  += src->R;
  dst->H  += src->H;
  dst->nz |= src->nz;     // here, new nz bits are accumulated.
  dst->score += src->score;
}

static void Copy(const uint8_t* src, uint8_t* dst, int w, int h) {
  int y;
  for (y = 0; y < h; ++y) {
    memcpy(dst, src, w);
    src += BPS;
    dst += BPS;
  }
}

static void Copy4x4_C(const uint8_t* src, uint8_t* dst) {
  Copy(src, dst, 4, 4);
}

static void Copy16x8_C(const uint8_t* src, uint8_t* dst) {
  Copy(src, dst, 16, 8);
}

static int PickBestIntra4(VP8EncIterator* const it, VP8ModeScore* const rd) {
  const VP8Encoder* const enc = it->enc_;
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_i4_;
  const int tlambda = dqm->tlambda_;
  const uint8_t* const src0 = it->yuv_in_ + Y_OFF_ENC;
  uint8_t* const best_blocks = it->yuv_out2_ + Y_OFF_ENC;
  int total_header_bits = 0;
  VP8ModeScore rd_best;

  if (enc->max_i4_header_bits_ == 0) {
    return 0;
  }

  InitScore(&rd_best);
  rd_best.H = 211;  // '211' is the value of VP8BitCost(0, 145)
  SetRDScore(dqm->lambda_mode_, &rd_best);
  VP8IteratorStartI4(it);
  do {
    const int kNumBlocks = 1;
    VP8ModeScore rd_i4;
    int mode;
    int best_mode = -1;
    const uint8_t* const src = src0 + VP8Scan[it->i4_];
    //const uint16_t* const mode_costs = GetCostModeI4(it, rd->modes_i4);
    const uint16_t* const mode_costs = VP8FixedCostsI4[0][0];
    uint8_t* best_block = best_blocks + VP8Scan[it->i4_];
    uint8_t* tmp_dst = it->yuv_p_ + I4TMP;    // scratch buffer.

    InitScore(&rd_i4);

    VP8MakeIntra4Preds(it);

    for (mode = 0; mode < NUM_BMODES; ++mode) {
      VP8ModeScore rd_tmp;
      int16_t tmp_levels[16];

      // Reconstruct
      rd_tmp.nz =
          ReconstructIntra4(it, tmp_levels, src, tmp_dst, mode) << it->i4_;

      // Compute RD-score
      rd_tmp.D = SSE4x4_C(src, tmp_dst);
      rd_tmp.SD =
          tlambda ? MULT_8B(tlambda, Disto4x4_C(src, tmp_dst, kWeightY))
                  : 0;
      rd_tmp.H = mode_costs[mode];

	  int64_t test_R = 0;
	  int y;
	  for (y = 0; y < 16; ++y) {
		test_R += tmp_levels[y] * tmp_levels[y];
	  }
	  rd_tmp.R = test_R << 10;

      /*// Add flatness penalty
      if (mode > 0 && IsFlat(tmp_levels, kNumBlocks, FLATNESS_LIMIT_I4)) {
        rd_tmp.R = FLATNESS_PENALTY * kNumBlocks;
      } else {
        rd_tmp.R = 0;
      }

      // early-out check
      SetRDScore(lambda, &rd_tmp);
      if (best_mode >= 0 && rd_tmp.score >= rd_i4.score) continue;

      // finish computing score
      rd_tmp.R += VP8GetCostLuma4(it, tmp_levels);*/
      SetRDScore(lambda, &rd_tmp);

      if (best_mode < 0 || rd_tmp.score < rd_i4.score) {
        CopyScore(&rd_i4, &rd_tmp);
        best_mode = mode;
        SwapPtr(&tmp_dst, &best_block);
        memcpy(rd_best.y_ac_levels[it->i4_], tmp_levels,
               sizeof(rd_best.y_ac_levels[it->i4_]));
      }
    }
    SetRDScore(dqm->lambda_mode_, &rd_i4);
    AddScore(&rd_best, &rd_i4);
    if (rd_best.score >= rd->score) {
      return 0;
    }
    //total_header_bits += (int)rd_i4.H;   // <- equal to mode_costs[best_mode];
    //if (total_header_bits > enc->max_i4_header_bits_) {
    //  return 0;
    //}
    // Copy selected samples if not in the right place already.
    if (best_block != best_blocks + VP8Scan[it->i4_]) {
    	Copy4x4_C(best_block, best_blocks + VP8Scan[it->i4_]);
    }
    rd->modes_i4[it->i4_] = best_mode;
    it->top_nz_[it->i4_ & 3] = it->left_nz_[it->i4_ >> 2] = (rd_i4.nz ? 1 : 0);
  } while (VP8IteratorRotateI4(it, best_blocks));

  // finalize state
  CopyScore(rd, &rd_best);
  VP8SetIntra4Mode(it, rd->modes_i4);
  SwapOut(it);
  memcpy(rd->y_ac_levels, rd_best.y_ac_levels, sizeof(rd->y_ac_levels));
  return 1;   // select intra4x4 over intra16x16
}

static const uint16_t VP8ScanUV[4 + 4] = {
  0 + 0 * BPS,   4 + 0 * BPS, 0 + 4 * BPS,  4 + 4 * BPS,    // U
  8 + 0 * BPS,  12 + 0 * BPS, 8 + 4 * BPS, 12 + 4 * BPS     // V
};

#define C1 7    // fraction of error sent to the 4x4 block below
#define C2 8    // fraction of error sent to the 4x4 block on the right
#define DSHIFT 4
#define DSCALE 1   // storage descaling, needed to make the error fit int8_t

static int QuantizeSingle(int16_t* const v, const VP8Matrix* const mtx) {
  int V = *v;
  const int sign = (V < 0);
  if (sign) V = -V;
  if (V > (int)mtx->zthresh_[0]) {
    const int qV = QUANTDIV(V, mtx->iq_[0], mtx->bias_[0]) * mtx->q_[0];
    const int err = (V - qV);
    *v = sign ? -qV : qV;
    return (sign ? -err : err) >> DSCALE;
  }
  *v = 0;
  return (sign ? -V : V) >> DSCALE;
}

static void CorrectDCValues(const VP8EncIterator* const it,
                            const VP8Matrix* const mtx,
                            int16_t tmp[][16], VP8ModeScore* const rd) {
  //         | top[0] | top[1]
  // --------+--------+---------
  // left[0] | tmp[0]   tmp[1]  <->   err0 err1
  // left[1] | tmp[2]   tmp[3]        err2 err3
  //
  // Final errors {err1,err2,err3} are preserved and later restored
  // as top[]/left[] on the next block.
  int ch;
  for (ch = 0; ch <= 1; ++ch) {
    const int8_t* const top = it->top_derr_[it->x_][ch];
    const int8_t* const left = it->left_derr_[ch];
    int16_t (* const c)[16] = &tmp[ch * 4];
    int err0, err1, err2, err3;
    c[0][0] += (C1 * top[0] + C2 * left[0]) >> (DSHIFT - DSCALE);
    err0 = QuantizeSingle(&c[0][0], mtx);
    c[1][0] += (C1 * top[1] + C2 * err0) >> (DSHIFT - DSCALE);
    err1 = QuantizeSingle(&c[1][0], mtx);
    c[2][0] += (C1 * err0 + C2 * left[1]) >> (DSHIFT - DSCALE);
    err2 = QuantizeSingle(&c[2][0], mtx);
    c[3][0] += (C1 * err1 + C2 * err2) >> (DSHIFT - DSCALE);
    err3 = QuantizeSingle(&c[3][0], mtx);
    // error 'err' is bounded by mtx->q_[0] which is 132 at max. Hence
    // err >> DSCALE will fit in an int8_t type if DSCALE>=1.
    assert(abs(err1) <= 127 && abs(err2) <= 127 && abs(err3) <= 127);
    rd->derr[ch][0] = (int8_t)err1;
    rd->derr[ch][1] = (int8_t)err2;
    rd->derr[ch][2] = (int8_t)err3;
  }
}

static void StoreDiffusionErrors(VP8EncIterator* const it,
                                 const VP8ModeScore* const rd) {
  int ch;
  for (ch = 0; ch <= 1; ++ch) {
    int8_t* const top = it->top_derr_[it->x_][ch];
    int8_t* const left = it->left_derr_[ch];
    left[0] = rd->derr[ch][0];            // restore err1
    left[1] = 3 * rd->derr[ch][2] >> 2;   //     ... 3/4th of err3
    top[0]  = rd->derr[ch][1];            //     ... err2
    top[1]  = rd->derr[ch][2] - left[1];  //     ... 1/4th of err3.
  }
}

#undef C1
#undef C2
#undef DSHIFT
#undef DSCALE

static int ReconstructUV(VP8EncIterator* const it, VP8ModeScore* const rd,
                         uint8_t* const yuv_out, int mode) {
  const VP8Encoder* const enc = it->enc_;
  const uint8_t* const ref = it->yuv_p_ + VP8UVModeOffsets[mode];
  const uint8_t* const src = it->yuv_in_ + U_OFF_ENC;
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  int nz = 0;
  int n;
  int16_t tmp[8][16];

  for (n = 0; n < 8; n += 2) {
	  FTransform2_C(src + VP8ScanUV[n], ref + VP8ScanUV[n], tmp[n]);
  }

  if (it->top_derr_ != NULL) CorrectDCValues(it, &dqm->uv_, tmp, rd);

  for (n = 0; n < 8; n += 2) {
    nz |= Quantize2Blocks_C(tmp[n], rd->uv_levels[n], &dqm->uv_) << n;
  }

  for (n = 0; n < 8; n += 2) {
	  ITransform_C(ref + VP8ScanUV[n], tmp[n], yuv_out + VP8ScanUV[n], 1);
  }

  return (nz << 16);
}

static void PickBestUV(VP8EncIterator* const it, VP8ModeScore* const rd) {
  const int kNumBlocks = 8;
  const VP8SegmentInfo* const dqm = &it->enc_->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_uv_;
  const uint8_t* const src = it->yuv_in_ + U_OFF_ENC;
  uint8_t* tmp_dst = it->yuv_out2_ + U_OFF_ENC;  // scratch buffer
  uint8_t* dst0 = it->yuv_out_ + U_OFF_ENC;
  uint8_t* dst = dst0;
  VP8ModeScore rd_best;
  int mode;

  rd->mode_uv = -1;
  InitScore(&rd_best);
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    VP8ModeScore rd_uv;

    // Reconstruct
    rd_uv.nz = ReconstructUV(it, &rd_uv, tmp_dst, mode);

    // Compute RD-score
    rd_uv.D  = SSE16x8_C(src, tmp_dst);
    rd_uv.SD = 0;    // not calling TDisto here: it tends to flatten areas.
    rd_uv.H  = VP8FixedCostsUV[mode];

	int64_t test_R = 0;
	int x, y;
	for (y = 0; y < 8; ++y) {
	  for (x = 0; x < 16; ++x) {
	    test_R += rd_uv.uv_levels[y][x] * rd_uv.uv_levels[y][x];
	  }
	}
	rd_uv.R = test_R << 10;


    /*rd_uv.R  = VP8GetCostUV(it, &rd_uv);
    if (mode > 0 && IsFlat(rd_uv.uv_levels[0], kNumBlocks, FLATNESS_LIMIT_UV)) {
      rd_uv.R += FLATNESS_PENALTY * kNumBlocks;
    }*/

    SetRDScore(lambda, &rd_uv);

    if (mode == 0 || rd_uv.score < rd_best.score) {
      CopyScore(&rd_best, &rd_uv);
      rd->mode_uv = mode;
      memcpy(rd->uv_levels, rd_uv.uv_levels, sizeof(rd->uv_levels));
      if (it->top_derr_ != NULL) {
        memcpy(rd->derr, rd_uv.derr, sizeof(rd_uv.derr));
      }
      SwapPtr(&dst, &tmp_dst);
    }
  }
  VP8SetIntraUVMode(it, rd->mode_uv);
  AddScore(rd, &rd_best);
  if (dst != dst0) {   // copy 16x8 block if needed
	  Copy16x8_C(dst, dst0);
  }
  if (it->top_derr_ != NULL) {  // store diffusion errors for next block
    StoreDiffusionErrors(it, rd);
  }
}

int VP8Decimate(VP8EncIterator* const it, VP8ModeScore* const rd,
                VP8RDLevel rd_opt) {
  int is_skipped;
  const int method = it->enc_->method_;

  InitScore(rd);

  // We can perform predictions for Luma16x16 and Chroma8x8 already.
  // Luma4x4 predictions needs to be done as-we-go.

  VP8MakeLuma16Preds(it);

  VP8MakeChroma8Preds(it);

  PickBestIntra16(it, rd);
  PickBestIntra4(it, rd);
  PickBestUV(it, rd);

  is_skipped = (rd->nz == 0);
  VP8SetSkip(it, is_skipped);
  return is_skipped;
}

// On-the-fly info about the current set of residuals. Handy to avoid
// passing zillions of params.
typedef struct VP8Residual VP8Residual;
struct VP8Residual {
  int first;
  int last;
  const int16_t* coeffs;

  int coeff_type;
  ProbaArray*   prob;
  StatsArray*   stats;
  CostArrayPtr  costs;
};

void VP8InitResidual(int first, int coeff_type,
                     VP8Encoder* const enc, VP8Residual* const res) {
  res->coeff_type = coeff_type;
  res->prob  = enc->proba_.coeffs_[coeff_type];
  res->stats = enc->proba_.stats_[coeff_type];
  res->costs = enc->proba_.remapped_costs_[coeff_type];
  res->first = first;
}

static void SetResidualCoeffs_C(const int16_t* const coeffs,
                                VP8Residual* const res) {
  int n;
  res->last = -1;
  assert(res->first == 0 || coeffs[0] == 0);
  for (n = 15; n >= 0; --n) {
    if (coeffs[n]) {
      res->last = n;
      break;
    }
  }
  res->coeffs = coeffs;
}

// Record proba context used.
static int VP8RecordStats(int bit, proba_t* const stats) {
  proba_t p = *stats;
  // An overflow is inbound. Note we handle this at 0xfffe0000u instead of
  // 0xffff0000u to make sure p + 1u does not overflow.
  if (p >= 0xfffe0000u) {
    p = ((p + 1u) >> 1) & 0x7fff7fffu;  // -> divide the stats by 2.
  }
  // record bit count (lower 16 bits) and increment total count (upper 16 bits).
  p += 0x00010000u + bit;
  *stats = p;
  return bit;
}

// We keep the table-free variant around for reference, in case.
#define USE_LEVEL_CODE_TABLE

// Simulate block coding, but only record statistics.
// Note: no need to record the fixed probas.
int VP8RecordCoeffs(int ctx, const VP8Residual* const res) {
  int n = res->first;
  // should be stats[VP8EncBands[n]], but it's equivalent for n=0 or 1
  proba_t* s = res->stats[n][ctx];
  if (res->last  < 0) {
    VP8RecordStats(0, s + 0);
    return 0;
  }
  while (n <= res->last) {
    int v;
    VP8RecordStats(1, s + 0);  // order of record doesn't matter
    while ((v = res->coeffs[n++]) == 0) {
      VP8RecordStats(0, s + 1);
      s = res->stats[VP8EncBands[n]][0];
    }
    VP8RecordStats(1, s + 1);
    if (!VP8RecordStats(2u < (unsigned int)(v + 1), s + 2)) {  // v = -1 or 1
      s = res->stats[VP8EncBands[n]][1];
    } else {
      v = abs(v);
#if !defined(USE_LEVEL_CODE_TABLE)
      if (!VP8RecordStats(v > 4, s + 3)) {
        if (VP8RecordStats(v != 2, s + 4))
          VP8RecordStats(v == 4, s + 5);
      } else if (!VP8RecordStats(v > 10, s + 6)) {
        VP8RecordStats(v > 6, s + 7);
      } else if (!VP8RecordStats((v >= 3 + (8 << 2)), s + 8)) {
        VP8RecordStats((v >= 3 + (8 << 1)), s + 9);
      } else {
        VP8RecordStats((v >= 3 + (8 << 3)), s + 10);
      }
#else
      if (v > MAX_VARIABLE_LEVEL) {
        v = MAX_VARIABLE_LEVEL;
      }

      {
        const int bits = VP8LevelCodes[v - 1][1];
        int pattern = VP8LevelCodes[v - 1][0];
        int i;
        for (i = 0; (pattern >>= 1) != 0; ++i) {
          const int mask = 2 << i;
          if (pattern & 1) VP8RecordStats(!!(bits & mask), s + 3 + i);
        }
      }
#endif
      s = res->stats[VP8EncBands[n]][2];
    }
  }
  if (n < 16) VP8RecordStats(0, s + 0);
  return 1;
}

void VP8IteratorBytesToNz(VP8EncIterator* const it) {
  uint32_t nz = 0;
  const int* const top_nz = it->top_nz_;
  const int* const left_nz = it->left_nz_;
  // top
  nz |= (top_nz[0] << 12) | (top_nz[1] << 13);
  nz |= (top_nz[2] << 14) | (top_nz[3] << 15);
  nz |= (top_nz[4] << 18) | (top_nz[5] << 19);
  nz |= (top_nz[6] << 22) | (top_nz[7] << 23);
  nz |= (top_nz[8] << 24);  // we propagate the _top_ bit, esp. for intra4
  // left
  nz |= (left_nz[0] << 3) | (left_nz[1] << 7);
  nz |= (left_nz[2] << 11);
  nz |= (left_nz[4] << 17) | (left_nz[6] << 21);

  *it->nz_ = nz;
}

// Same as CodeResiduals, but doesn't actually write anything.
// Instead, it just records the event distribution.
static void RecordResiduals(VP8EncIterator* const it,
                            const VP8ModeScore* const rd) {
  int x, y, ch;
  VP8Residual res;
  VP8Encoder* const enc = it->enc_;

  VP8IteratorNzToBytes(it);

  if (it->mb_->type_ == 1) {   // i16x16
    VP8InitResidual(0, 1, enc, &res);
    SetResidualCoeffs_C(rd->y_dc_levels, &res);
    it->top_nz_[8] = it->left_nz_[8] =
      VP8RecordCoeffs(it->top_nz_[8] + it->left_nz_[8], &res);
    VP8InitResidual(1, 0, enc, &res);
  } else {
    VP8InitResidual(0, 3, enc, &res);
  }

  // luma-AC
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      const int ctx = it->top_nz_[x] + it->left_nz_[y];
      SetResidualCoeffs_C(rd->y_ac_levels[x + y * 4], &res);
      it->top_nz_[x] = it->left_nz_[y] = VP8RecordCoeffs(ctx, &res);
    }
  }

  // U/V
  VP8InitResidual(0, 2, enc, &res);
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
        const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
        SetResidualCoeffs_C(rd->uv_levels[ch * 2 + x + y * 2], &res);
        it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] =
            VP8RecordCoeffs(ctx, &res);
      }
    }
  }

  VP8IteratorBytesToNz(it);
}

void VP8IteratorSaveBoundary(VP8EncIterator* const it) {
  VP8Encoder* const enc = it->enc_;
  const int x = it->x_, y = it->y_;
  const uint8_t* const ysrc = it->yuv_out_ + Y_OFF_ENC;
  const uint8_t* const uvsrc = it->yuv_out_ + U_OFF_ENC;
  if (x < enc->mb_w_ - 1) {   // left
    int i;
    for (i = 0; i < 16; ++i) {
      it->y_left_[i] = ysrc[15 + i * BPS];
    }
    for (i = 0; i < 8; ++i) {
      it->u_left_[i] = uvsrc[7 + i * BPS];
      it->v_left_[i] = uvsrc[15 + i * BPS];
    }
    // top-left (before 'top'!)
    it->y_left_[-1] = it->y_top_[15];
    it->u_left_[-1] = it->uv_top_[0 + 7];
    it->v_left_[-1] = it->uv_top_[8 + 7];
  }
  if (y < enc->mb_h_ - 1) {  // top
    memcpy(it->y_top_, ysrc + 15 * BPS, 16);
    memcpy(it->uv_top_, uvsrc + 7 * BPS, 8 + 8);
  }
}

#define SKIP_PROBA_THRESHOLD 250  // value below which using skip_proba is OK.

static int CalcSkipProba(uint64_t nb, uint64_t total) {
  return (int)(total ? (total - nb) * 255 / total : 255);
}

// Returns the bit-cost for coding the skip probability.
static int FinalizeSkipProba(VP8Encoder* const enc) {
  VP8EncProba* const proba = &enc->proba_;
  const int nb_mbs = enc->mb_w_ * enc->mb_h_;
  const int nb_events = proba->nb_skip_;
  int size;
  proba->skip_proba_ = CalcSkipProba(nb_events, nb_mbs);
  proba->use_skip_proba_ = (proba->skip_proba_ < SKIP_PROBA_THRESHOLD);
  size = 256;   // 'use_skip_proba' bit
  if (proba->use_skip_proba_) {
    size +=  nb_events * VP8BitCost(1, proba->skip_proba_)
         + (nb_mbs - nb_events) * VP8BitCost(0, proba->skip_proba_);
    size += 8 * 256;   // cost of signaling the skip_proba_ itself.
  }
  return size;
}

const uint8_t
    VP8CoeffsUpdateProba[NUM_TYPES][NUM_BANDS][NUM_CTX][NUM_PROBAS] = {
  { { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255 },
      { 250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  },
  { { { 217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255 },
      { 234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255 }
    },
    { { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  },
  { { { 186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255 }
    },
    { { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  },
  { { { 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255 },
      { 248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  }
};

// Collect statistics and deduce probabilities for next coding pass.
// Return the total bit-cost for coding the probability updates.
static int CalcTokenProba(int nb, int total) {
  assert(nb <= total);
  return nb ? (255 - nb * 255 / total) : 255;
}

// Cost of coding 'nb' 1's and 'total-nb' 0's using 'proba' probability.
static int BranchCost(int nb, int total, int proba) {
  return nb * VP8BitCost(1, proba) + (total - nb) * VP8BitCost(0, proba);
}

static int FinalizeTokenProbas(VP8EncProba* const proba) {
  int has_changed = 0;
  int size = 0;
  int t, b, c, p;
  for (t = 0; t < NUM_TYPES; ++t) {
    for (b = 0; b < NUM_BANDS; ++b) {
      for (c = 0; c < NUM_CTX; ++c) {
        for (p = 0; p < NUM_PROBAS; ++p) {
          const proba_t stats = proba->stats_[t][b][c][p];
          const int nb = (stats >> 0) & 0xffff;
          const int total = (stats >> 16) & 0xffff;
          const int update_proba = VP8CoeffsUpdateProba[t][b][c][p];
          const int old_p = VP8CoeffsProba0[t][b][c][p];
          const int new_p = CalcTokenProba(nb, total);
          const int old_cost = BranchCost(nb, total, old_p)
                             + VP8BitCost(0, update_proba);
          const int new_cost = BranchCost(nb, total, new_p)
                             + VP8BitCost(1, update_proba)
                             + 8 * 256;
          const int use_new_p = (old_cost > new_cost);
          size += VP8BitCost(use_new_p, update_proba);
          if (use_new_p) {  // only use proba that seem meaningful enough.
            proba->coeffs_[t][b][c][p] = new_p;
            has_changed |= (new_p != old_p);
            size += 8 * 256;
          } else {
            proba->coeffs_[t][b][c][p] = old_p;
          }
        }
      }
    }
  }
  proba->dirty_ = has_changed;
  return size;
}

#define CHUNK_HEADER_SIZE  8     // Size of a chunk header.
#define RIFF_HEADER_SIZE   12    // Size of the RIFF header ("RIFFnnnnWEBP").
#define VP8_FRAME_HEADER_SIZE 10  // Size of the frame header within VP8 data.
#define HEADER_SIZE_ESTIMATE (RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE +  \
                              VP8_FRAME_HEADER_SIZE)

static double GetPSNR(uint64_t mse, uint64_t size) {
  return (mse > 0 && size > 0) ? 10. * log10(255. * 255. * size / mse) : 99;
}

static uint64_t OneStatPass(VP8Encoder* const enc, VP8RDLevel rd_opt,
                            int nb_mbs, int percent_delta,
                            PassStats* const s) {
  VP8EncIterator it;
  uint64_t size = 0;
  uint64_t size_p0 = 0;
  uint64_t distortion = 0;
  const uint64_t pixel_count = nb_mbs * 384;

  VP8IteratorInit(enc, &it);
  SetLoopParams(enc, s->q);
  do {
    VP8ModeScore info;
    VP8IteratorImport(&it, NULL);
    if (VP8Decimate(&it, &info, rd_opt)) {
      // Just record the number of skips and act like skip_proba is not used.
      ++enc->proba_.nb_skip_;
    }
    RecordResiduals(&it, &info);
    size += info.R + info.H;
    size_p0 += info.H;
    distortion += info.D;
    if (percent_delta && !VP8IteratorProgress(&it, percent_delta)) {
      return 0;
    }
    VP8IteratorSaveBoundary(&it);
  } while (VP8IteratorNext(&it) && --nb_mbs > 0);

  size_p0 += enc->segment_hdr_.size_;
  if (s->do_size_search) {
    size += FinalizeSkipProba(enc);
    size += FinalizeTokenProbas(&enc->proba_);
    size = ((size + size_p0 + 1024) >> 11) + HEADER_SIZE_ESTIMATE;
    s->value = (double)size;
  } else {
    s->value = GetPSNR(distortion, pixel_count);
  }
  return size_p0;
}

#define DQ_LIMIT 0.4  // convergence is considered reached if dq < DQ_LIMIT
// we allow 2k of extra head-room in PARTITION0 limit.
#define VP8_MAX_PARTITION0_SIZE (1 << 19)   // max size of mode partition
#define PARTITION0_SIZE_LIMIT ((VP8_MAX_PARTITION0_SIZE - 2048ULL) << 11)

static float ComputeNextQ(PassStats* const s) {
  float dq;
  if (s->is_first) {
    dq = (s->value > s->target) ? -s->dq : s->dq;
    s->is_first = 0;
  } else if (s->value != s->last_value) {
    const double slope = (s->target - s->value) / (s->last_value - s->value);
    dq = (float)(slope * (s->last_q - s->q));
  } else {
    dq = 0.;  // we're done?!
  }
  // Limit variable to avoid large swings.
  s->dq = Clamp(dq, -30.f, 30.f);
  s->last_q = s->q;
  s->last_value = s->value;
  s->q = Clamp(s->q + s->dq, 0.f, 100.f);
  return s->q;
}

static int StatLoop(VP8Encoder* const enc) {
  const int method = enc->method_;
  const int do_search = enc->do_search_;
  const int fast_probe = ((method == 0 || method == 3) && !do_search);
  int num_pass_left = enc->config_->pass;
  const int task_percent = 20;
  const int percent_per_pass =
      (task_percent + num_pass_left / 2) / num_pass_left;
  const int final_percent = enc->percent_ + task_percent;
  const VP8RDLevel rd_opt =
      (method >= 3 || do_search) ? RD_OPT_BASIC : RD_OPT_NONE;
  int nb_mbs = enc->mb_w_ * enc->mb_h_;
  PassStats stats;

  InitPassStats(enc, &stats);
  ResetTokenStats(enc);

  // Fast mode: quick analysis pass over few mbs. Better than nothing.
  if (fast_probe) {
    if (method == 3) {  // we need more stats for method 3 to be reliable.
      nb_mbs = (nb_mbs > 200) ? nb_mbs >> 1 : 100;
    } else {
      nb_mbs = (nb_mbs > 200) ? nb_mbs >> 2 : 50;
    }
  }

  while (num_pass_left-- > 0) {
    const int is_last_pass = (fabs(stats.dq) <= DQ_LIMIT) ||
                             (num_pass_left == 0) ||
                             (enc->max_i4_header_bits_ == 0);
    const uint64_t size_p0 =
        OneStatPass(enc, rd_opt, nb_mbs, percent_per_pass, &stats);
    if (size_p0 == 0) return 0;
#if (DEBUG_SEARCH > 0)
    printf("#%d value:%.1lf -> %.1lf   q:%.2f -> %.2f\n",
           num_pass_left, stats.last_value, stats.value, stats.last_q, stats.q);
#endif
    if (enc->max_i4_header_bits_ > 0 && size_p0 > PARTITION0_SIZE_LIMIT) {
      ++num_pass_left;
      enc->max_i4_header_bits_ >>= 1;  // strengthen header bit limitation...
      continue;                        // ...and start over
    }
    if (is_last_pass) {
      break;
    }
    // If no target size: just do several pass without changing 'q'
    if (do_search) {
      ComputeNextQ(&stats);
      if (fabs(stats.dq) <= DQ_LIMIT) break;
    }
  }
  if (!do_search || !stats.do_size_search) {
    // Need to finalize probas now, since it wasn't done during the search.
    FinalizeSkipProba(enc);
    FinalizeTokenProbas(&enc->proba_);
  }
  VP8CalculateLevelCosts(&enc->proba_);  // finalize costs
  return WebPReportProgress(enc->pic_, final_percent, &enc->percent_);
}

void VP8InitFilter(VP8EncIterator* const it) {
#if !defined(WEBP_REDUCE_SIZE)
  if (it->lf_stats_ != NULL) {
    int s, i;
    for (s = 0; s < NUM_MB_SEGMENTS; s++) {
      for (i = 0; i < MAX_LF_LEVELS; i++) {
        (*it->lf_stats_)[s][i] = 0;
      }
    }
  }
#else
  (void)it;
#endif
}

// return approximate write position (in bits)
static uint64_t VP8BitWriterPos(const VP8BitWriter* const bw) {
  const uint64_t nb_bits = 8 + bw->nb_bits_;   // bw->nb_bits_ is <= 0, note
  return (bw->pos_ + bw->run_) * 8 + nb_bits;
}

static const uint8_t kNorm[128] = {  // renorm_sizes[i] = 8 - log2(i)
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

// range = ((range + 1) << kVP8Log2Range[range]) - 1
static const uint8_t kNewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127, 143, 159, 175, 191, 207, 223, 239,
  127, 135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239,
  247, 127, 131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179,
  183, 187, 191, 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239,
  243, 247, 251, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149,
  151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179,
  181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209,
  211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

static void Flush(VP8BitWriter* const bw) {
  const int s = 8 + bw->nb_bits_;
  const int32_t bits = bw->value_ >> s;
  assert(bw->nb_bits_ >= 0);
  bw->value_ -= bits << s;
  bw->nb_bits_ -= 8;
  if ((bits & 0xff) != 0xff) {
    size_t pos = bw->pos_;
    if (!BitWriterResize(bw, bw->run_ + 1)) {
      return;
    }
    if (bits & 0x100) {  // overflow -> propagate carry over pending 0xff's
      if (pos > 0) bw->buf_[pos - 1]++;
    }
    if (bw->run_ > 0) {
      const int value = (bits & 0x100) ? 0x00 : 0xff;
      for (; bw->run_ > 0; --bw->run_) bw->buf_[pos++] = value;
    }
    bw->buf_[pos++] = bits;
    bw->pos_ = pos;
  } else {
    bw->run_++;   // delay writing of bytes 0xff, pending eventual carry.
  }
}

int VP8PutBit(VP8BitWriter* const bw, int bit, int prob) {
  const int split = (bw->range_ * prob) >> 8;
  if (bit) {
    bw->value_ += split + 1;
    bw->range_ -= split + 1;
  } else {
    bw->range_ = split;
  }
  if (bw->range_ < 127) {   // emit 'shift' bits out and renormalize
    const int shift = kNorm[bw->range_];
    bw->range_ = kNewRange[bw->range_];
    bw->value_ <<= shift;
    bw->nb_bits_ += shift;
    if (bw->nb_bits_ > 0) Flush(bw);
  }
  return bit;
}

const uint8_t VP8Cat3[] = { 173, 148, 140 };
const uint8_t VP8Cat4[] = { 176, 155, 140, 135 };
const uint8_t VP8Cat5[] = { 180, 157, 141, 134, 130 };
const uint8_t VP8Cat6[] =
    { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129 };

int VP8PutBitUniform(VP8BitWriter* const bw, int bit) {
  const int split = bw->range_ >> 1;
  if (bit) {
    bw->value_ += split + 1;
    bw->range_ -= split + 1;
  } else {
    bw->range_ = split;
  }
  if (bw->range_ < 127) {
    bw->range_ = kNewRange[bw->range_];
    bw->value_ <<= 1;
    bw->nb_bits_ += 1;
    if (bw->nb_bits_ > 0) Flush(bw);
  }
  return bit;
}

static int PutCoeffs(VP8BitWriter* const bw, int ctx, const VP8Residual* res) {
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  const uint8_t* p = res->prob[n][ctx];
  if (!VP8PutBit(bw, res->last >= 0, p[0])) {
    return 0;
  }

  while (n < 16) {
    const int c = res->coeffs[n++];
    const int sign = c < 0;
    int v = sign ? -c : c;
    if (!VP8PutBit(bw, v != 0, p[1])) {
      p = res->prob[VP8EncBands[n]][0];
      continue;
    }
    if (!VP8PutBit(bw, v > 1, p[2])) {
      p = res->prob[VP8EncBands[n]][1];
    } else {
      if (!VP8PutBit(bw, v > 4, p[3])) {
        if (VP8PutBit(bw, v != 2, p[4])) {
          VP8PutBit(bw, v == 4, p[5]);
        }
      } else if (!VP8PutBit(bw, v > 10, p[6])) {
        if (!VP8PutBit(bw, v > 6, p[7])) {
          VP8PutBit(bw, v == 6, 159);
        } else {
          VP8PutBit(bw, v >= 9, 165);
          VP8PutBit(bw, !(v & 1), 145);
        }
      } else {
        int mask;
        const uint8_t* tab;
        if (v < 3 + (8 << 1)) {          // VP8Cat3  (3b)
          VP8PutBit(bw, 0, p[8]);
          VP8PutBit(bw, 0, p[9]);
          v -= 3 + (8 << 0);
          mask = 1 << 2;
          tab = VP8Cat3;
        } else if (v < 3 + (8 << 2)) {   // VP8Cat4  (4b)
          VP8PutBit(bw, 0, p[8]);
          VP8PutBit(bw, 1, p[9]);
          v -= 3 + (8 << 1);
          mask = 1 << 3;
          tab = VP8Cat4;
        } else if (v < 3 + (8 << 3)) {   // VP8Cat5  (5b)
          VP8PutBit(bw, 1, p[8]);
          VP8PutBit(bw, 0, p[10]);
          v -= 3 + (8 << 2);
          mask = 1 << 4;
          tab = VP8Cat5;
        } else {                         // VP8Cat6 (11b)
          VP8PutBit(bw, 1, p[8]);
          VP8PutBit(bw, 1, p[10]);
          v -= 3 + (8 << 3);
          mask = 1 << 10;
          tab = VP8Cat6;
        }
        while (mask) {
          VP8PutBit(bw, !!(v & mask), *tab++);
          mask >>= 1;
        }
      }
      p = res->prob[VP8EncBands[n]][2];
    }
    VP8PutBitUniform(bw, sign);
    if (n == 16 || !VP8PutBit(bw, n <= res->last, p[0])) {
      return 1;   // EOB
    }
  }
  return 1;
}

static void CodeResiduals(VP8BitWriter* const bw, VP8EncIterator* const it,
                          const VP8ModeScore* const rd) {
  int x, y, ch;
  VP8Residual res;
  uint64_t pos1, pos2, pos3;
  const int i16 = (it->mb_->type_ == 1);
  const int segment = it->mb_->segment_;
  VP8Encoder* const enc = it->enc_;

  VP8IteratorNzToBytes(it);

  pos1 = VP8BitWriterPos(bw);
  if (i16) {
    VP8InitResidual(0, 1, enc, &res);
    SetResidualCoeffs_C(rd->y_dc_levels, &res);
    it->top_nz_[8] = it->left_nz_[8] =
      PutCoeffs(bw, it->top_nz_[8] + it->left_nz_[8], &res);
    VP8InitResidual(1, 0, enc, &res);
  } else {
    VP8InitResidual(0, 3, enc, &res);
  }

  // luma-AC
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      const int ctx = it->top_nz_[x] + it->left_nz_[y];
      SetResidualCoeffs_C(rd->y_ac_levels[x + y * 4], &res);
      it->top_nz_[x] = it->left_nz_[y] = PutCoeffs(bw, ctx, &res);
    }
  }
  pos2 = VP8BitWriterPos(bw);

  // U/V
  VP8InitResidual(0, 2, enc, &res);
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
        const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
        SetResidualCoeffs_C(rd->uv_levels[ch * 2 + x + y * 2], &res);
        it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] =
            PutCoeffs(bw, ctx, &res);
      }
    }
  }
  pos3 = VP8BitWriterPos(bw);
  it->luma_bits_ = pos2 - pos1;
  it->uv_bits_ = pos3 - pos2;
  it->bit_count_[segment][i16] += it->luma_bits_;
  it->bit_count_[segment][2] += it->uv_bits_;
  VP8IteratorBytesToNz(it);
}

static void ResetAfterSkip(VP8EncIterator* const it) {
  if (it->mb_->type_ == 1) {
    *it->nz_ = 0;  // reset all predictors
    it->left_nz_[8] = 0;
  } else {
    *it->nz_ &= (1 << 24);  // preserve the dc_nz bit
  }
}

static void StoreSSE(const VP8EncIterator* const it) {
  VP8Encoder* const enc = it->enc_;
  const uint8_t* const in = it->yuv_in_;
  const uint8_t* const out = it->yuv_out_;
  // Note: not totally accurate at boundary. And doesn't include in-loop filter.
  enc->sse_[0] += SSE16x16_C(in + Y_OFF_ENC, out + Y_OFF_ENC);
  enc->sse_[1] += SSE8x8_C(in + U_OFF_ENC, out + U_OFF_ENC);
  enc->sse_[2] += SSE8x8_C(in + V_OFF_ENC, out + V_OFF_ENC);
  enc->sse_count_ += 16 * 16;
}

#define SEGMENT_VISU 0

static void StoreSideInfo(const VP8EncIterator* const it) {
  VP8Encoder* const enc = it->enc_;
  const VP8MBInfo* const mb = it->mb_;
  WebPPicture* const pic = enc->pic_;

  if (pic->stats != NULL) {
    StoreSSE(it);
    enc->block_count_[0] += (mb->type_ == 0);
    enc->block_count_[1] += (mb->type_ == 1);
    enc->block_count_[2] += (mb->skip_ != 0);
  }

  if (pic->extra_info != NULL) {
    uint8_t* const info = &pic->extra_info[it->x_ + it->y_ * enc->mb_w_];
    switch (pic->extra_info_type) {
      case 1: *info = mb->type_; break;
      case 2: *info = mb->segment_; break;
      case 3: *info = enc->dqm_[mb->segment_].quant_; break;
      case 4: *info = (mb->type_ == 1) ? it->preds_[0] : 0xff; break;
      case 5: *info = mb->uv_mode_; break;
      case 6: {
        const int b = (int)((it->luma_bits_ + it->uv_bits_ + 7) >> 3);
        *info = (b > 255) ? 255 : b; break;
      }
      case 7: *info = mb->alpha_; break;
      default: *info = 0; break;
    }
  }
#if SEGMENT_VISU  // visualize segments and prediction modes
  SetBlock(it->yuv_out_ + Y_OFF_ENC, mb->segment_ * 64, 16);
  SetBlock(it->yuv_out_ + U_OFF_ENC, it->preds_[0] * 64, 8);
  SetBlock(it->yuv_out_ + V_OFF_ENC, mb->uv_mode_ * 64, 8);
#endif
}

#define VP8_SSIM_KERNEL 3   // total size of the kernel: 2 * VP8_SSIM_KERNEL + 1

// struct for accumulating statistical moments
typedef struct {
  uint32_t w;              // sum(w_i) : sum of weights
  uint32_t xm, ym;         // sum(w_i * x_i), sum(w_i * y_i)
  uint32_t xxm, xym, yym;  // sum(w_i * x_i * x_i), etc.
} VP8DistoStats;

static double SSIMCalculation(
    const VP8DistoStats* const stats, uint32_t N  /*num samples*/) {
  const uint32_t w2 =  N * N;
  const uint32_t C1 = 20 * w2;
  const uint32_t C2 = 60 * w2;
  const uint32_t C3 = 8 * 8 * w2;   // 'dark' limit ~= 6
  const uint64_t xmxm = (uint64_t)stats->xm * stats->xm;
  const uint64_t ymym = (uint64_t)stats->ym * stats->ym;
  if (xmxm + ymym >= C3) {
    const int64_t xmym = (int64_t)stats->xm * stats->ym;
    const int64_t sxy = (int64_t)stats->xym * N - xmym;    // can be negative
    const uint64_t sxx = (uint64_t)stats->xxm * N - xmxm;
    const uint64_t syy = (uint64_t)stats->yym * N - ymym;
    // we descale by 8 to prevent overflow during the fnum/fden multiply.
    const uint64_t num_S = (2 * (uint64_t)(sxy < 0 ? 0 : sxy) + C2) >> 8;
    const uint64_t den_S = (sxx + syy + C2) >> 8;
    const uint64_t fnum = (2 * xmym + C1) * num_S;
    const uint64_t fden = (xmxm + ymym + C1) * den_S;
    const double r = (double)fnum / fden;
    assert(r >= 0. && r <= 1.0);
    return r;
  }
  return 1.;   // area is too dark to contribute meaningfully
}

double VP8SSIMFromStatsClipped(const VP8DistoStats* const stats) {
  return SSIMCalculation(stats, stats->w);
}

// hat-shaped filter. Sum of coefficients is equal to 16.
static const uint32_t kWeight[2 * VP8_SSIM_KERNEL + 1] = {
  1, 2, 3, 4, 3, 2, 1
};

static double SSIMGetClipped_C(const uint8_t* src1, int stride1,
                               const uint8_t* src2, int stride2,
                               int xo, int yo, int W, int H) {
  VP8DistoStats stats = { 0, 0, 0, 0, 0, 0 };
  const int ymin = (yo - VP8_SSIM_KERNEL < 0) ? 0 : yo - VP8_SSIM_KERNEL;
  const int ymax = (yo + VP8_SSIM_KERNEL > H - 1) ? H - 1
                                                  : yo + VP8_SSIM_KERNEL;
  const int xmin = (xo - VP8_SSIM_KERNEL < 0) ? 0 : xo - VP8_SSIM_KERNEL;
  const int xmax = (xo + VP8_SSIM_KERNEL > W - 1) ? W - 1
                                                  : xo + VP8_SSIM_KERNEL;
  int x, y;
  src1 += ymin * stride1;
  src2 += ymin * stride2;
  for (y = ymin; y <= ymax; ++y, src1 += stride1, src2 += stride2) {
    for (x = xmin; x <= xmax; ++x) {
      const uint32_t w = kWeight[VP8_SSIM_KERNEL + x - xo]
                       * kWeight[VP8_SSIM_KERNEL + y - yo];
      const uint32_t s1 = src1[x];
      const uint32_t s2 = src2[x];
      stats.w   += w;
      stats.xm  += w * s1;
      stats.ym  += w * s2;
      stats.xxm += w * s1 * s1;
      stats.xym += w * s1 * s2;
      stats.yym += w * s2 * s2;
    }
  }
  return VP8SSIMFromStatsClipped(&stats);
}

static double GetMBSSIM(const uint8_t* yuv1, const uint8_t* yuv2) {
  int x, y;
  double sum = 0.;

  // compute SSIM in a 10 x 10 window
  for (y = VP8_SSIM_KERNEL; y < 16 - VP8_SSIM_KERNEL; y++) {
    for (x = VP8_SSIM_KERNEL; x < 16 - VP8_SSIM_KERNEL; x++) {
      sum += SSIMGetClipped_C(yuv1 + Y_OFF_ENC, BPS, yuv2 + Y_OFF_ENC, BPS,
                               x, y, 16, 16);
    }
  }
  for (x = 1; x < 7; x++) {
    for (y = 1; y < 7; y++) {
      sum += SSIMGetClipped_C(yuv1 + U_OFF_ENC, BPS, yuv2 + U_OFF_ENC, BPS,
                               x, y, 8, 8);
      sum += SSIMGetClipped_C(yuv1 + V_OFF_ENC, BPS, yuv2 + V_OFF_ENC, BPS,
                               x, y, 8, 8);
    }
  }
  return sum;
}

static int GetILevel(int sharpness, int level) {
  if (sharpness > 0) {
    if (sharpness > 4) {
      level >>= 2;
    } else {
      level >>= 1;
    }
    if (level > 9 - sharpness) {
      level = 9 - sharpness;
    }
  }
  if (level < 1) level = 1;
  return level;
}

static const uint8_t abs0[255 + 255 + 1] = {
  0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4,
  0xf3, 0xf2, 0xf1, 0xf0, 0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8,
  0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0, 0xdf, 0xde, 0xdd, 0xdc,
  0xdb, 0xda, 0xd9, 0xd8, 0xd7, 0xd6, 0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xd0,
  0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xc7, 0xc6, 0xc5, 0xc4,
  0xc3, 0xc2, 0xc1, 0xc0, 0xbf, 0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8,
  0xb7, 0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1, 0xb0, 0xaf, 0xae, 0xad, 0xac,
  0xab, 0xaa, 0xa9, 0xa8, 0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0,
  0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94,
  0x93, 0x92, 0x91, 0x90, 0x8f, 0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88,
  0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80, 0x7f, 0x7e, 0x7d, 0x7c,
  0x7b, 0x7a, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
  0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64,
  0x63, 0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d, 0x5c, 0x5b, 0x5a, 0x59, 0x58,
  0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x4e, 0x4d, 0x4c,
  0x4b, 0x4a, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40,
  0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34,
  0x33, 0x32, 0x31, 0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28,
  0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x1f, 0x1e, 0x1d, 0x1c,
  0x1b, 0x1a, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
  0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
  0x03, 0x02, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
  0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
  0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
  0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
  0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
  0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
  0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
  0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
  0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const uint8_t* const VP8kabs0 = &abs0[255];

static int NeedsFilter_C(const uint8_t* p, int step, int t) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) <= t);
}

static const uint8_t sclip1[1020 + 1020 + 1] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
  0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
  0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
  0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
  0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
  0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
  0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
  0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
  0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
  0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f
};

static const uint8_t sclip2[112 + 112 + 1] = {
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
  0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
};

const int8_t* const VP8ksclip1 = (const int8_t*)&sclip1[1020];
const int8_t* const VP8ksclip2 = (const int8_t*)&sclip2[112];
const uint8_t* const VP8kclip1 = &clip1[255];

static void DoFilter2_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0) + VP8ksclip1[p1 - q1];  // in [-893,892]
  const int a1 = VP8ksclip2[(a + 4) >> 3];            // in [-16,15]
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  p[-step] = VP8kclip1[p0 + a2];
  p[    0] = VP8kclip1[q0 - a1];
}

static void SimpleHFilter16_C(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (NeedsFilter_C(p + i * stride, 1, thresh2)) {
      DoFilter2_C(p + i * stride, 1);
    }
  }
}

static void SimpleHFilter16i_C(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16_C(p, stride, thresh);
  }
}

static void SimpleVFilter16_C(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (NeedsFilter_C(p + i, stride, thresh2)) {
      DoFilter2_C(p + i, stride);
    }
  }
}

static void SimpleVFilter16i_C(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16_C(p, stride, thresh);
  }
}

static int Hev(const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (VP8kabs0[p1 - p0] > thresh) || (VP8kabs0[q1 - q0] > thresh);
}

static int NeedsFilter2_C(const uint8_t* p,
                                      int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step];
  const int p0 = p[-step], q0 = p[0];
  const int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) > t) return 0;
  return VP8kabs0[p3 - p2] <= it && VP8kabs0[p2 - p1] <= it &&
         VP8kabs0[p1 - p0] <= it && VP8kabs0[q3 - q2] <= it &&
         VP8kabs0[q2 - q1] <= it && VP8kabs0[q1 - q0] <= it;
}

static void DoFilter4_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0);
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  const int a3 = (a1 + 1) >> 1;
  p[-2*step] = VP8kclip1[p1 + a3];
  p[-  step] = VP8kclip1[p0 + a2];
  p[      0] = VP8kclip1[q0 - a1];
  p[   step] = VP8kclip1[q1 - a3];
}

static void FilterLoop24_C(uint8_t* p,
                                       int hstride, int vstride, int size,
                                       int thresh, int ithresh,
                                       int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (NeedsFilter2_C(p, hstride, thresh2, ithresh)) {
      if (Hev(p, hstride, hev_thresh)) {
        DoFilter2_C(p, hstride);
      } else {
        DoFilter4_C(p, hstride);
      }
    }
    p += vstride;
  }
}
static void HFilter16i_C(uint8_t* p, int stride,
                         int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    FilterLoop24_C(p, 1, stride, 16, thresh, ithresh, hev_thresh);
  }
}

static void HFilter8i_C(uint8_t* u, uint8_t* v, int stride,
                        int thresh, int ithresh, int hev_thresh) {
  FilterLoop24_C(u + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop24_C(v + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
}

static void VFilter16i_C(uint8_t* p, int stride,
                         int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    FilterLoop24_C(p, stride, 1, 16, thresh, ithresh, hev_thresh);
  }
}

static void VFilter8i_C(uint8_t* u, uint8_t* v, int stride,
                        int thresh, int ithresh, int hev_thresh) {
  FilterLoop24_C(u + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop24_C(v + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
}

static void DoFilter(const VP8EncIterator* const it, int level) {
  const VP8Encoder* const enc = it->enc_;
  const int ilevel = GetILevel(enc->config_->filter_sharpness, level);
  const int limit = 2 * level + ilevel;

  uint8_t* const y_dst = it->yuv_out2_ + Y_OFF_ENC;
  uint8_t* const u_dst = it->yuv_out2_ + U_OFF_ENC;
  uint8_t* const v_dst = it->yuv_out2_ + V_OFF_ENC;

  // copy current block to yuv_out2_
  memcpy(y_dst, it->yuv_out_, YUV_SIZE_ENC * sizeof(uint8_t));

  if (enc->filter_hdr_.simple_ == 1) {   // simple
	  SimpleHFilter16i_C(y_dst, BPS, limit);
	  SimpleVFilter16i_C(y_dst, BPS, limit);
  } else {    // complex
    const int hev_thresh = (level >= 40) ? 2 : (level >= 15) ? 1 : 0;
    HFilter16i_C(y_dst, BPS, limit, ilevel, hev_thresh);
    HFilter8i_C(u_dst, v_dst, BPS, limit, ilevel, hev_thresh);
    VFilter16i_C(y_dst, BPS, limit, ilevel, hev_thresh);
    VFilter8i_C(u_dst, v_dst, BPS, limit, ilevel, hev_thresh);
  }
}

void VP8StoreFilterStats(VP8EncIterator* const it) {
#if !defined(WEBP_REDUCE_SIZE)
  int d;
  VP8Encoder* const enc = it->enc_;
  const int s = it->mb_->segment_;
  const int level0 = enc->dqm_[s].fstrength_;

  // explore +/-quant range of values around level0
  const int delta_min = -enc->dqm_[s].quant_;
  const int delta_max = enc->dqm_[s].quant_;
  const int step_size = (delta_max - delta_min >= 4) ? 4 : 1;

  if (it->lf_stats_ == NULL) return;

  // NOTE: Currently we are applying filter only across the sublock edges
  // There are two reasons for that.
  // 1. Applying filter on macro block edges will change the pixels in
  // the left and top macro blocks. That will be hard to restore
  // 2. Macro Blocks on the bottom and right are not yet compressed. So we
  // cannot apply filter on the right and bottom macro block edges.
  if (it->mb_->type_ == 1 && it->mb_->skip_) return;

  // Always try filter level  zero
  (*it->lf_stats_)[s][0] += GetMBSSIM(it->yuv_in_, it->yuv_out_);

  for (d = delta_min; d <= delta_max; d += step_size) {
    const int level = level0 + d;
    if (level <= 0 || level >= MAX_LF_LEVELS) {
      continue;
    }
    DoFilter(it, level);
    (*it->lf_stats_)[s][level] += GetMBSSIM(it->yuv_in_, it->yuv_out2_);
  }
#else  // defined(WEBP_REDUCE_SIZE)
  (void)it;
#endif  // !defined(WEBP_REDUCE_SIZE)
}

static void ExportBlock(const uint8_t* src, uint8_t* dst, int dst_stride,
                        int w, int h) {
  while (h-- > 0) {
    memcpy(dst, src, w);
    dst += dst_stride;
    src += BPS;
  }
}

void VP8IteratorExport(const VP8EncIterator* const it) {
  const VP8Encoder* const enc = it->enc_;
  if (enc->config_->show_compressed) {
    const int x = it->x_, y = it->y_;
    const uint8_t* const ysrc = it->yuv_out_ + Y_OFF_ENC;
    const uint8_t* const usrc = it->yuv_out_ + U_OFF_ENC;
    const uint8_t* const vsrc = it->yuv_out_ + V_OFF_ENC;
    const WebPPicture* const pic = enc->pic_;
    uint8_t* const ydst = pic->y + (y * pic->y_stride + x) * 16;
    uint8_t* const udst = pic->u + (y * pic->uv_stride + x) * 8;
    uint8_t* const vdst = pic->v + (y * pic->uv_stride + x) * 8;
    int w = (pic->width - x * 16);
    int h = (pic->height - y * 16);

    if (w > 16) w = 16;
    if (h > 16) h = 16;

    // Luma plane
    ExportBlock(ysrc, ydst, pic->y_stride, w, h);

    {   // U/V planes
      const int uv_w = (w + 1) >> 1;
      const int uv_h = (h + 1) >> 1;
      ExportBlock(usrc, udst, pic->uv_stride, uv_w, uv_h);
      ExportBlock(vsrc, vdst, pic->uv_stride, uv_w, uv_h);
    }
  }
}

void VP8PutBits(VP8BitWriter* const bw, uint32_t value, int nb_bits) {
  uint32_t mask;
  assert(nb_bits > 0 && nb_bits < 32);
  for (mask = 1u << (nb_bits - 1); mask; mask >>= 1) {
    VP8PutBitUniform(bw, value & mask);
  }
}

uint8_t* VP8BitWriterFinish(VP8BitWriter* const bw) {
  VP8PutBits(bw, 0, 9 - bw->nb_bits_);
  bw->nb_bits_ = 0;   // pad with zeroes
  Flush(bw);
  return bw->buf_;
}

void VP8AdjustFilterStrength(VP8EncIterator* const it) {
  VP8Encoder* const enc = it->enc_;
#if !defined(WEBP_REDUCE_SIZE)
  if (it->lf_stats_ != NULL) {
    int s;
    for (s = 0; s < NUM_MB_SEGMENTS; s++) {
      int i, best_level = 0;
      // Improvement over filter level 0 should be at least 1e-5 (relatively)
      double best_v = 1.00001 * (*it->lf_stats_)[s][0];
      for (i = 1; i < MAX_LF_LEVELS; i++) {
        const double v = (*it->lf_stats_)[s][i];
        if (v > best_v) {
          best_v = v;
          best_level = i;
        }
      }
      enc->dqm_[s].fstrength_ = best_level;
    }
    return;
  }
#endif  // !defined(WEBP_REDUCE_SIZE)
  if (enc->config_->filter_strength > 0) {
    int max_level = 0;
    int s;
    for (s = 0; s < NUM_MB_SEGMENTS; s++) {
      VP8SegmentInfo* const dqm = &enc->dqm_[s];
      // this '>> 3' accounts for some inverse WHT scaling
      const int delta = (dqm->max_edge_ * dqm->y2_.q_[1]) >> 3;
      const int level =
          VP8FilterStrengthFromDelta(enc->filter_hdr_.sharpness_, delta);
      if (level > dqm->fstrength_) {
        dqm->fstrength_ = level;
      }
      if (max_level < dqm->fstrength_) {
        max_level = dqm->fstrength_;
      }
    }
    enc->filter_hdr_.level_ = max_level;
  }
}

static int PostLoopFinalize(VP8EncIterator* const it, int ok) {
  VP8Encoder* const enc = it->enc_;
  if (ok) {      // Finalize the partitions, check for extra errors.
    int p;
    for (p = 0; p < enc->num_parts_; ++p) {
      VP8BitWriterFinish(enc->parts_ + p);
      ok &= !enc->parts_[p].error_;
    }
  }

  if (ok) {      // All good. Finish up.
#if !defined(WEBP_DISABLE_STATS)
    if (enc->pic_->stats != NULL) {  // finalize byte counters...
      int i, s;
      for (i = 0; i <= 2; ++i) {
        for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
          enc->residual_bytes_[i][s] = (int)((it->bit_count_[s][i] + 7) >> 3);
        }
      }
    }
#endif
    VP8AdjustFilterStrength(it);     // ...and store filter stats.
  } else {
    // Something bad happened -> need to do some memory cleanup.
    VP8EncFreeBitWriters(enc);
  }
  return ok;
}

int VP8EncLoop(VP8Encoder* const enc) {
  VP8EncIterator it;
  int ok = PreLoopInitialize(enc);
  if (!ok) return 0;

  StatLoop(enc);  // stats-collection loop

  VP8IteratorInit(enc, &it);
  VP8InitFilter(&it);
  do {
    VP8ModeScore info;
    const int dont_use_skip = !enc->proba_.use_skip_proba_;
    const VP8RDLevel rd_opt = enc->rd_opt_level_;

    VP8IteratorImport(&it, NULL);
    // Warning! order is important: first call VP8Decimate() and
    // *then* decide how to code the skip decision if there's one.
    if (!VP8Decimate(&it, &info, rd_opt) || dont_use_skip) {
      CodeResiduals(it.bw_, &it, &info);
    } else {   // reset predictors after a skip
      ResetAfterSkip(&it);
    }
    StoreSideInfo(&it);
    VP8StoreFilterStats(&it);
    VP8IteratorExport(&it);
    ok = VP8IteratorProgress(&it, 20);
    VP8IteratorSaveBoundary(&it);
  } while (ok && VP8IteratorNext(&it));

  return PostLoopFinalize(&it, ok);
}

void VP8TBufferClear(VP8TBuffer* const b) {
  if (b != NULL) {
    VP8Tokens* p = b->pages_;
    while (p != NULL) {
      VP8Tokens* const next = p->next_;
      WebPSafeFree(p);
      p = next;
    }
    VP8TBufferInit(b, b->page_size_);
  }
}

typedef uint16_t token_t;  // bit #15: bit value
                           // bit #14: flags for constant proba or idx
                           // bits #0..13: slot or constant proba

#define TOKEN_DATA(p) ((const token_t*)&(p)[1])

static int TBufferNewPage(VP8TBuffer* const b) {
  VP8Tokens* page = NULL;
  if (!b->error_) {
    const size_t size = sizeof(*page) + b->page_size_ * sizeof(token_t);
    page = (VP8Tokens*)WebPSafeMalloc(1ULL, size);
  }
  if (page == NULL) {
    b->error_ = 1;
    return 0;
  }
  page->next_ = NULL;

  *b->last_page_ = page;
  b->last_page_ = &page->next_;
  b->left_ = b->page_size_;
  b->tokens_ = (token_t*)TOKEN_DATA(page);
  return 1;
}

#define FIXED_PROBA_BIT (1u << 14)

#define TOKEN_ID(t, b, ctx) \
    (NUM_PROBAS * ((ctx) + NUM_CTX * ((b) + NUM_BANDS * (t))))

static uint32_t AddToken(VP8TBuffer* const b, uint32_t bit,
                                     uint32_t proba_idx,
                                     proba_t* const stats) {
  assert(proba_idx < FIXED_PROBA_BIT);
  assert(bit <= 1);
  if (b->left_ > 0 || TBufferNewPage(b)) {
    const int slot = --b->left_;
    b->tokens_[slot] = (bit << 15) | proba_idx;
  }
  VP8RecordStats(bit, stats);
  return bit;
}

static void AddConstantToken(VP8TBuffer* const b,
                                         uint32_t bit, uint32_t proba) {
  assert(proba < 256);
  assert(bit <= 1);
  if (b->left_ > 0 || TBufferNewPage(b)) {
    const int slot = --b->left_;
    b->tokens_[slot] = (bit << 15) | FIXED_PROBA_BIT | proba;
  }
}

int VP8RecordCoeffTokens(int ctx, const struct VP8Residual* const res,
                         VP8TBuffer* const tokens) {
  const int16_t* const coeffs = res->coeffs;
  const int coeff_type = res->coeff_type;
  const int last = res->last;
  int n = res->first;
  uint32_t base_id = TOKEN_ID(coeff_type, n, ctx);
  // should be stats[VP8EncBands[n]], but it's equivalent for n=0 or 1
  proba_t* s = res->stats[n][ctx];
  if (!AddToken(tokens, last >= 0, base_id + 0, s + 0)) {
    return 0;
  }

  while (n < 16) {
    const int c = coeffs[n++];
    const int sign = c < 0;
    const uint32_t v = sign ? -c : c;
    if (!AddToken(tokens, v != 0, base_id + 1, s + 1)) {
      base_id = TOKEN_ID(coeff_type, VP8EncBands[n], 0);  // ctx=0
      s = res->stats[VP8EncBands[n]][0];
      continue;
    }
    if (!AddToken(tokens, v > 1, base_id + 2, s + 2)) {
      base_id = TOKEN_ID(coeff_type, VP8EncBands[n], 1);  // ctx=1
      s = res->stats[VP8EncBands[n]][1];
    } else {
      if (!AddToken(tokens, v > 4, base_id + 3, s + 3)) {
        if (AddToken(tokens, v != 2, base_id + 4, s + 4)) {
          AddToken(tokens, v == 4, base_id + 5, s + 5);
        }
      } else if (!AddToken(tokens, v > 10, base_id + 6, s + 6)) {
        if (!AddToken(tokens, v > 6, base_id + 7, s + 7)) {
          AddConstantToken(tokens, v == 6, 159);
        } else {
          AddConstantToken(tokens, v >= 9, 165);
          AddConstantToken(tokens, !(v & 1), 145);
        }
      } else {
        int mask;
        const uint8_t* tab;
        uint32_t residue = v - 3;
        if (residue < (8 << 1)) {          // VP8Cat3  (3b)
          AddToken(tokens, 0, base_id + 8, s + 8);
          AddToken(tokens, 0, base_id + 9, s + 9);
          residue -= (8 << 0);
          mask = 1 << 2;
          tab = VP8Cat3;
        } else if (residue < (8 << 2)) {   // VP8Cat4  (4b)
          AddToken(tokens, 0, base_id + 8, s + 8);
          AddToken(tokens, 1, base_id + 9, s + 9);
          residue -= (8 << 1);
          mask = 1 << 3;
          tab = VP8Cat4;
        } else if (residue < (8 << 3)) {   // VP8Cat5  (5b)
          AddToken(tokens, 1, base_id + 8, s + 8);
          AddToken(tokens, 0, base_id + 10, s + 9);
          residue -= (8 << 2);
          mask = 1 << 4;
          tab = VP8Cat5;
        } else {                         // VP8Cat6 (11b)
          AddToken(tokens, 1, base_id + 8, s + 8);
          AddToken(tokens, 1, base_id + 10, s + 9);
          residue -= (8 << 3);
          mask = 1 << 10;
          tab = VP8Cat6;
        }
        while (mask) {
          AddConstantToken(tokens, !!(residue & mask), *tab++);
          mask >>= 1;
        }
      }
      base_id = TOKEN_ID(coeff_type, VP8EncBands[n], 2);  // ctx=2
      s = res->stats[VP8EncBands[n]][2];
    }
    AddConstantToken(tokens, sign, 128);
    if (n == 16 || !AddToken(tokens, n <= last, base_id + 0, s + 0)) {
      return 1;   // EOB
    }
  }
  return 1;
}

#undef TOKEN_ID

static int RecordTokens(VP8EncIterator* const it, const VP8ModeScore* const rd,
                        VP8TBuffer* const tokens) {
  int x, y, ch;
  VP8Residual res;
  VP8Encoder* const enc = it->enc_;

  VP8IteratorNzToBytes(it);
  if (it->mb_->type_ == 1) {   // i16x16
    const int ctx = it->top_nz_[8] + it->left_nz_[8];
    VP8InitResidual(0, 1, enc, &res);
    SetResidualCoeffs_C(rd->y_dc_levels, &res);
    it->top_nz_[8] = it->left_nz_[8] =
        VP8RecordCoeffTokens(ctx, &res, tokens);
    VP8InitResidual(1, 0, enc, &res);
  } else {
    VP8InitResidual(0, 3, enc, &res);
  }

  // luma-AC
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      const int ctx = it->top_nz_[x] + it->left_nz_[y];
      SetResidualCoeffs_C(rd->y_ac_levels[x + y * 4], &res);
      it->top_nz_[x] = it->left_nz_[y] =
          VP8RecordCoeffTokens(ctx, &res, tokens);
    }
  }

  // U/V
  VP8InitResidual(0, 2, enc, &res);
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
        const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
        SetResidualCoeffs_C(rd->uv_levels[ch * 2 + x + y * 2], &res);
        it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] =
            VP8RecordCoeffTokens(ctx, &res, tokens);
      }
    }
  }
  VP8IteratorBytesToNz(it);
  return !tokens->error_;
}

int VP8EmitTokens(VP8TBuffer* const b, VP8BitWriter* const bw,
                  const uint8_t* const probas, int final_pass) {
  const VP8Tokens* p = b->pages_;
  assert(!b->error_);
  while (p != NULL) {
    const VP8Tokens* const next = p->next_;
    const int N = (next == NULL) ? b->left_ : 0;
    int n = b->page_size_;
    const token_t* const tokens = TOKEN_DATA(p);
    while (n-- > N) {
      const token_t token = tokens[n];
      const int bit = (token >> 15) & 1;
      if (token & FIXED_PROBA_BIT) {
        VP8PutBit(bw, bit, token & 0xffu);  // constant proba
      } else {
        VP8PutBit(bw, bit, probas[token & 0x3fffu]);
      }
    }
    if (final_pass) WebPSafeFree((void*)p);
    p = next;
  }
  if (final_pass) b->pages_ = NULL;
  return 1;
}

#define MIN_COUNT 96  // minimum number of macroblocks before updating stats
#define DEBUG_SEARCH 0    // useful to track search convergence

int VP8EncTokenLoop(VP8Encoder* const enc) {
  // Roughly refresh the proba eight times per pass
  int max_count = (enc->mb_w_ * enc->mb_h_) >> 3;
  int num_pass_left = enc->config_->pass;
  const int do_search = enc->do_search_;
  VP8EncIterator it;
  VP8EncProba* const proba = &enc->proba_;
  const VP8RDLevel rd_opt = enc->rd_opt_level_;
  const uint64_t pixel_count = enc->mb_w_ * enc->mb_h_ * 384;
  PassStats stats;
  int ok;

  InitPassStats(enc, &stats);
  ok = PreLoopInitialize(enc);
  if (!ok) return 0;

  if (max_count < MIN_COUNT) max_count = MIN_COUNT;

  assert(enc->num_parts_ == 1);
  assert(enc->use_tokens_);
  assert(proba->use_skip_proba_ == 0);
  assert(rd_opt >= RD_OPT_BASIC);   // otherwise, token-buffer won't be useful
  assert(num_pass_left > 0);



    uint64_t size_p0 = 0;
    uint64_t distortion = 0;
    int cnt = max_count;
    VP8IteratorInit(enc, &it);
    SetLoopParams(enc, stats.q);
    ResetTokenStats(enc);
    VP8InitFilter(&it);
    VP8TBufferClear(&enc->tokens_);
    do {
      VP8ModeScore info;
      VP8IteratorImport(&it, NULL);
      VP8Decimate(&it, &info, rd_opt);
      ok = RecordTokens(&it, &info, &enc->tokens_);
      if (!ok) {
        WebPEncodingSetError(enc->pic_, VP8_ENC_ERROR_OUT_OF_MEMORY);
        break;
      }
      distortion += info.D;
      StoreSideInfo(&it);
      VP8StoreFilterStats(&it);
      VP8IteratorExport(&it);
      VP8IteratorSaveBoundary(&it);
    } while (ok && VP8IteratorNext(&it));

    // compute and store PSNR
      stats.value = GetPSNR(distortion, pixel_count);

#if (DEBUG_SEARCH > 0)
    printf("#%2d metric:%.1lf -> %.1lf   last_q=%.2lf q=%.2lf dq=%.2lf\n",
           num_pass_left, stats.last_value, stats.value,
           stats.last_q, stats.q, stats.dq);
#endif

  if (ok) {
    FinalizeTokenProbas(&enc->proba_);
    ok = VP8EmitTokens(&enc->tokens_, enc->parts_ + 0,
                       (const uint8_t*)proba->coeffs_, 1);
  }

  return PostLoopFinalize(&it, ok);
}

int VP8EncFinishAlpha(VP8Encoder* const enc) {
  if (enc->has_alpha_) {
    if (enc->thread_level_ > 0) {
      WebPWorker* const worker = &enc->alpha_worker_;
      if (!WebPGetWorkerInterface()->Sync(worker)) return 0;  // error
    }
  }
  return WebPReportProgress(enc->pic_, enc->percent_ + 20, &enc->percent_);
}

void VP8PutSignedBits(VP8BitWriter* const bw, int value, int nb_bits) {
  if (!VP8PutBitUniform(bw, value != 0)) return;
  if (value < 0) {
    VP8PutBits(bw, ((-value) << 1) | 1, nb_bits + 1);
  } else {
    VP8PutBits(bw, value << 1, nb_bits + 1);
  }
}

// Segmentation header
static void PutSegmentHeader(VP8BitWriter* const bw,
                             const VP8Encoder* const enc) {
  const VP8EncSegmentHeader* const hdr = &enc->segment_hdr_;
  const VP8EncProba* const proba = &enc->proba_;
  if (VP8PutBitUniform(bw, (hdr->num_segments_ > 1))) {
    // We always 'update' the quant and filter strength values
    const int update_data = 1;
    int s;
    VP8PutBitUniform(bw, hdr->update_map_);
    if (VP8PutBitUniform(bw, update_data)) {
      // we always use absolute values, not relative ones
      VP8PutBitUniform(bw, 1);   // (segment_feature_mode = 1. Paragraph 9.3.)
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        VP8PutSignedBits(bw, enc->dqm_[s].quant_, 7);
      }
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        VP8PutSignedBits(bw, enc->dqm_[s].fstrength_, 6);
      }
    }
    if (hdr->update_map_) {
      for (s = 0; s < 3; ++s) {
        if (VP8PutBitUniform(bw, (proba->segments_[s] != 255u))) {
          VP8PutBits(bw, proba->segments_[s], 8);
        }
      }
    }
  }
}

// Filtering parameters header
static void PutFilterHeader(VP8BitWriter* const bw,
                            const VP8EncFilterHeader* const hdr) {
  const int use_lf_delta = (hdr->i4x4_lf_delta_ != 0);
  VP8PutBitUniform(bw, hdr->simple_);
  VP8PutBits(bw, hdr->level_, 6);
  VP8PutBits(bw, hdr->sharpness_, 3);
  if (VP8PutBitUniform(bw, use_lf_delta)) {
    // '0' is the default value for i4x4_lf_delta_ at frame #0.
    const int need_update = (hdr->i4x4_lf_delta_ != 0);
    if (VP8PutBitUniform(bw, need_update)) {
      // we don't use ref_lf_delta => emit four 0 bits
      VP8PutBits(bw, 0, 4);
      // we use mode_lf_delta for i4x4
      VP8PutSignedBits(bw, hdr->i4x4_lf_delta_, 6);
      VP8PutBits(bw, 0, 3);    // all others unused
    }
  }
}

// Nominal quantization parameters
static void PutQuant(VP8BitWriter* const bw,
                     const VP8Encoder* const enc) {
  VP8PutBits(bw, enc->base_quant_, 7);
  VP8PutSignedBits(bw, enc->dq_y1_dc_, 4);
  VP8PutSignedBits(bw, enc->dq_y2_dc_, 4);
  VP8PutSignedBits(bw, enc->dq_y2_ac_, 4);
  VP8PutSignedBits(bw, enc->dq_uv_dc_, 4);
  VP8PutSignedBits(bw, enc->dq_uv_ac_, 4);
}

void VP8WriteProbas(VP8BitWriter* const bw, const VP8EncProba* const probas) {
  int t, b, c, p;
  for (t = 0; t < NUM_TYPES; ++t) {
    for (b = 0; b < NUM_BANDS; ++b) {
      for (c = 0; c < NUM_CTX; ++c) {
        for (p = 0; p < NUM_PROBAS; ++p) {
          const uint8_t p0 = probas->coeffs_[t][b][c][p];
          const int update = (p0 != VP8CoeffsProba0[t][b][c][p]);
          if (VP8PutBit(bw, update, VP8CoeffsUpdateProba[t][b][c][p])) {
            VP8PutBits(bw, p0, 8);
          }
        }
      }
    }
  }
  if (VP8PutBitUniform(bw, probas->use_skip_proba_)) {
    VP8PutBits(bw, probas->skip_proba_, 8);
  }
}

static void PutSegment(VP8BitWriter* const bw, int s, const uint8_t* p) {
  if (VP8PutBit(bw, s >= 2, p[0])) p += 1;
  VP8PutBit(bw, s & 1, p[1]);
}

static void PutI16Mode(VP8BitWriter* const bw, int mode) {
  if (VP8PutBit(bw, (mode == TM_PRED || mode == H_PRED), 156)) {
    VP8PutBit(bw, mode == TM_PRED, 128);    // TM or HE
  } else {
    VP8PutBit(bw, mode == V_PRED, 163);     // VE or DC
  }
}

static int PutI4Mode(VP8BitWriter* const bw, int mode,
                     const uint8_t* const prob) {
  if (VP8PutBit(bw, mode != B_DC_PRED, prob[0])) {
    if (VP8PutBit(bw, mode != B_TM_PRED, prob[1])) {
      if (VP8PutBit(bw, mode != B_VE_PRED, prob[2])) {
        if (!VP8PutBit(bw, mode >= B_LD_PRED, prob[3])) {
          if (VP8PutBit(bw, mode != B_HE_PRED, prob[4])) {
            VP8PutBit(bw, mode != B_RD_PRED, prob[5]);
          }
        } else {
          if (VP8PutBit(bw, mode != B_LD_PRED, prob[6])) {
            if (VP8PutBit(bw, mode != B_VL_PRED, prob[7])) {
              VP8PutBit(bw, mode != B_HD_PRED, prob[8]);
            }
          }
        }
      }
    }
  }
  return mode;
}

static void PutUVMode(VP8BitWriter* const bw, int uv_mode) {
  if (VP8PutBit(bw, uv_mode != DC_PRED, 142)) {
    if (VP8PutBit(bw, uv_mode != V_PRED, 114)) {
      VP8PutBit(bw, uv_mode != H_PRED, 183);    // else: TM_PRED
    }
  }
}

// Paragraph 11.5.  900bytes.
static const uint8_t kBModesProba[NUM_BMODES][NUM_BMODES][NUM_BMODES - 1] = {
  { { 231, 120, 48, 89, 115, 113, 120, 152, 112 },
    { 152, 179, 64, 126, 170, 118, 46, 70, 95 },
    { 175, 69, 143, 80, 85, 82, 72, 155, 103 },
    { 56, 58, 10, 171, 218, 189, 17, 13, 152 },
    { 114, 26, 17, 163, 44, 195, 21, 10, 173 },
    { 121, 24, 80, 195, 26, 62, 44, 64, 85 },
    { 144, 71, 10, 38, 171, 213, 144, 34, 26 },
    { 170, 46, 55, 19, 136, 160, 33, 206, 71 },
    { 63, 20, 8, 114, 114, 208, 12, 9, 226 },
    { 81, 40, 11, 96, 182, 84, 29, 16, 36 } },
  { { 134, 183, 89, 137, 98, 101, 106, 165, 148 },
    { 72, 187, 100, 130, 157, 111, 32, 75, 80 },
    { 66, 102, 167, 99, 74, 62, 40, 234, 128 },
    { 41, 53, 9, 178, 241, 141, 26, 8, 107 },
    { 74, 43, 26, 146, 73, 166, 49, 23, 157 },
    { 65, 38, 105, 160, 51, 52, 31, 115, 128 },
    { 104, 79, 12, 27, 217, 255, 87, 17, 7 },
    { 87, 68, 71, 44, 114, 51, 15, 186, 23 },
    { 47, 41, 14, 110, 182, 183, 21, 17, 194 },
    { 66, 45, 25, 102, 197, 189, 23, 18, 22 } },
  { { 88, 88, 147, 150, 42, 46, 45, 196, 205 },
    { 43, 97, 183, 117, 85, 38, 35, 179, 61 },
    { 39, 53, 200, 87, 26, 21, 43, 232, 171 },
    { 56, 34, 51, 104, 114, 102, 29, 93, 77 },
    { 39, 28, 85, 171, 58, 165, 90, 98, 64 },
    { 34, 22, 116, 206, 23, 34, 43, 166, 73 },
    { 107, 54, 32, 26, 51, 1, 81, 43, 31 },
    { 68, 25, 106, 22, 64, 171, 36, 225, 114 },
    { 34, 19, 21, 102, 132, 188, 16, 76, 124 },
    { 62, 18, 78, 95, 85, 57, 50, 48, 51 } },
  { { 193, 101, 35, 159, 215, 111, 89, 46, 111 },
    { 60, 148, 31, 172, 219, 228, 21, 18, 111 },
    { 112, 113, 77, 85, 179, 255, 38, 120, 114 },
    { 40, 42, 1, 196, 245, 209, 10, 25, 109 },
    { 88, 43, 29, 140, 166, 213, 37, 43, 154 },
    { 61, 63, 30, 155, 67, 45, 68, 1, 209 },
    { 100, 80, 8, 43, 154, 1, 51, 26, 71 },
    { 142, 78, 78, 16, 255, 128, 34, 197, 171 },
    { 41, 40, 5, 102, 211, 183, 4, 1, 221 },
    { 51, 50, 17, 168, 209, 192, 23, 25, 82 } },
  { { 138, 31, 36, 171, 27, 166, 38, 44, 229 },
    { 67, 87, 58, 169, 82, 115, 26, 59, 179 },
    { 63, 59, 90, 180, 59, 166, 93, 73, 154 },
    { 40, 40, 21, 116, 143, 209, 34, 39, 175 },
    { 47, 15, 16, 183, 34, 223, 49, 45, 183 },
    { 46, 17, 33, 183, 6, 98, 15, 32, 183 },
    { 57, 46, 22, 24, 128, 1, 54, 17, 37 },
    { 65, 32, 73, 115, 28, 128, 23, 128, 205 },
    { 40, 3, 9, 115, 51, 192, 18, 6, 223 },
    { 87, 37, 9, 115, 59, 77, 64, 21, 47 } },
  { { 104, 55, 44, 218, 9, 54, 53, 130, 226 },
    { 64, 90, 70, 205, 40, 41, 23, 26, 57 },
    { 54, 57, 112, 184, 5, 41, 38, 166, 213 },
    { 30, 34, 26, 133, 152, 116, 10, 32, 134 },
    { 39, 19, 53, 221, 26, 114, 32, 73, 255 },
    { 31, 9, 65, 234, 2, 15, 1, 118, 73 },
    { 75, 32, 12, 51, 192, 255, 160, 43, 51 },
    { 88, 31, 35, 67, 102, 85, 55, 186, 85 },
    { 56, 21, 23, 111, 59, 205, 45, 37, 192 },
    { 55, 38, 70, 124, 73, 102, 1, 34, 98 } },
  { { 125, 98, 42, 88, 104, 85, 117, 175, 82 },
    { 95, 84, 53, 89, 128, 100, 113, 101, 45 },
    { 75, 79, 123, 47, 51, 128, 81, 171, 1 },
    { 57, 17, 5, 71, 102, 57, 53, 41, 49 },
    { 38, 33, 13, 121, 57, 73, 26, 1, 85 },
    { 41, 10, 67, 138, 77, 110, 90, 47, 114 },
    { 115, 21, 2, 10, 102, 255, 166, 23, 6 },
    { 101, 29, 16, 10, 85, 128, 101, 196, 26 },
    { 57, 18, 10, 102, 102, 213, 34, 20, 43 },
    { 117, 20, 15, 36, 163, 128, 68, 1, 26 } },
  { { 102, 61, 71, 37, 34, 53, 31, 243, 192 },
    { 69, 60, 71, 38, 73, 119, 28, 222, 37 },
    { 68, 45, 128, 34, 1, 47, 11, 245, 171 },
    { 62, 17, 19, 70, 146, 85, 55, 62, 70 },
    { 37, 43, 37, 154, 100, 163, 85, 160, 1 },
    { 63, 9, 92, 136, 28, 64, 32, 201, 85 },
    { 75, 15, 9, 9, 64, 255, 184, 119, 16 },
    { 86, 6, 28, 5, 64, 255, 25, 248, 1 },
    { 56, 8, 17, 132, 137, 255, 55, 116, 128 },
    { 58, 15, 20, 82, 135, 57, 26, 121, 40 } },
  { { 164, 50, 31, 137, 154, 133, 25, 35, 218 },
    { 51, 103, 44, 131, 131, 123, 31, 6, 158 },
    { 86, 40, 64, 135, 148, 224, 45, 183, 128 },
    { 22, 26, 17, 131, 240, 154, 14, 1, 209 },
    { 45, 16, 21, 91, 64, 222, 7, 1, 197 },
    { 56, 21, 39, 155, 60, 138, 23, 102, 213 },
    { 83, 12, 13, 54, 192, 255, 68, 47, 28 },
    { 85, 26, 85, 85, 128, 128, 32, 146, 171 },
    { 18, 11, 7, 63, 144, 171, 4, 4, 246 },
    { 35, 27, 10, 146, 174, 171, 12, 26, 128 } },
  { { 190, 80, 35, 99, 180, 80, 126, 54, 45 },
    { 85, 126, 47, 87, 176, 51, 41, 20, 32 },
    { 101, 75, 128, 139, 118, 146, 116, 128, 85 },
    { 56, 41, 15, 176, 236, 85, 37, 9, 62 },
    { 71, 30, 17, 119, 118, 255, 17, 18, 138 },
    { 101, 38, 60, 138, 55, 70, 43, 26, 142 },
    { 146, 36, 19, 30, 171, 255, 97, 27, 20 },
    { 138, 45, 61, 62, 219, 1, 81, 188, 64 },
    { 32, 41, 20, 117, 151, 142, 20, 21, 163 },
    { 112, 19, 12, 61, 195, 128, 48, 4, 24 } }
};

void VP8CodeIntraModes(VP8Encoder* const enc) {
  VP8BitWriter* const bw = &enc->bw_;
  VP8EncIterator it;
  VP8IteratorInit(enc, &it);
  do {
    const VP8MBInfo* const mb = it.mb_;
    const uint8_t* preds = it.preds_;
    if (enc->segment_hdr_.update_map_) {
      PutSegment(bw, mb->segment_, enc->proba_.segments_);
    }
    if (enc->proba_.use_skip_proba_) {
      VP8PutBit(bw, mb->skip_, enc->proba_.skip_proba_);
    }
    if (VP8PutBit(bw, (mb->type_ != 0), 145)) {  // i16x16
      PutI16Mode(bw, preds[0]);
    } else {
      const int preds_w = enc->preds_w_;
      const uint8_t* top_pred = preds - preds_w;
      int x, y;
      for (y = 0; y < 4; ++y) {
        int left = preds[-1];
        for (x = 0; x < 4; ++x) {
          const uint8_t* const probas = kBModesProba[top_pred[x]][left];
          left = PutI4Mode(bw, preds[x], probas);
        }
        top_pred = preds;
        preds += preds_w;
      }
    }
    PutUVMode(bw, mb->uv_mode_);
  } while (VP8IteratorNext(&it));
}

static int GeneratePartition0(VP8Encoder* const enc) {
  VP8BitWriter* const bw = &enc->bw_;
  const int mb_size = enc->mb_w_ * enc->mb_h_;
  uint64_t pos1, pos2, pos3;

  pos1 = VP8BitWriterPos(bw);
  if (!VP8BitWriterInit(bw, mb_size * 7 / 8)) {        // ~7 bits per macroblock
    return WebPEncodingSetError(enc->pic_, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  VP8PutBitUniform(bw, 0);   // colorspace
  VP8PutBitUniform(bw, 0);   // clamp type

  PutSegmentHeader(bw, enc);
  PutFilterHeader(bw, &enc->filter_hdr_);
  VP8PutBits(bw, enc->num_parts_ == 8 ? 3 :
                 enc->num_parts_ == 4 ? 2 :
                 enc->num_parts_ == 2 ? 1 : 0, 2);
  PutQuant(bw, enc);
  VP8PutBitUniform(bw, 0);   // no proba update
  VP8WriteProbas(bw, &enc->proba_);
  pos2 = VP8BitWriterPos(bw);
  VP8CodeIntraModes(enc);
  VP8BitWriterFinish(bw);

  pos3 = VP8BitWriterPos(bw);

#if !defined(WEBP_DISABLE_STATS)
  if (enc->pic_->stats) {
    enc->pic_->stats->header_bytes[0] = (int)((pos2 - pos1 + 7) >> 3);
    enc->pic_->stats->header_bytes[1] = (int)((pos3 - pos2 + 7) >> 3);
    enc->pic_->stats->alpha_data_size = (int)enc->alpha_data_size_;
  }
#else
  (void)pos1;
  (void)pos2;
  (void)pos3;
#endif
  if (bw->error_) {
    return WebPEncodingSetError(enc->pic_, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  return 1;
}

#define TAG_SIZE           4     // Size of a chunk tag (e.g. "VP8L").

static int IsVP8XNeeded(const VP8Encoder* const enc) {
  return !!enc->has_alpha_;  // Currently the only case when VP8X is needed.
                             // This could change in the future.
}

#define VP8X_CHUNK_SIZE    10    // Size of a VP8X chunk.

static void PutLE16(uint8_t* const data, int val) {
  assert(val < (1 << 16));
  data[0] = (val >> 0);
  data[1] = (val >> 8);
}

static void PutLE24(uint8_t* const data, int val) {
  assert(val < (1 << 24));
  PutLE16(data, val & 0xffff);
  data[2] = (val >> 16);
}

static void PutLE32(uint8_t* const data, uint32_t val) {
  PutLE16(data, (int)(val & 0xffff));
  PutLE16(data + 2, (int)(val >> 16));
}

static WebPEncodingError PutRIFFHeader(const VP8Encoder* const enc,
                                       size_t riff_size) {
  const WebPPicture* const pic = enc->pic_;
  uint8_t riff[RIFF_HEADER_SIZE] = {
    'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'
  };
  assert(riff_size == (uint32_t)riff_size);
  PutLE32(riff + TAG_SIZE, (uint32_t)riff_size);
  if (!pic->writer(riff, sizeof(riff), pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }
  return VP8_ENC_OK;
}

#define MAX_CANVAS_SIZE     (1 << 24)     // 24-bit max for VP8X width/height.

// VP8X Feature Flags.
typedef enum WebPFeatureFlags {
  ANIMATION_FLAG  = 0x00000002,
  XMP_FLAG        = 0x00000004,
  EXIF_FLAG       = 0x00000008,
  ALPHA_FLAG      = 0x00000010,
  ICCP_FLAG       = 0x00000020,

  ALL_VALID_FLAGS = 0x0000003e
} WebPFeatureFlags;

static WebPEncodingError PutVP8XHeader(const VP8Encoder* const enc) {
  const WebPPicture* const pic = enc->pic_;
  uint8_t vp8x[CHUNK_HEADER_SIZE + VP8X_CHUNK_SIZE] = {
    'V', 'P', '8', 'X'
  };
  uint32_t flags = 0;

  assert(IsVP8XNeeded(enc));
  assert(pic->width >= 1 && pic->height >= 1);
  assert(pic->width <= MAX_CANVAS_SIZE && pic->height <= MAX_CANVAS_SIZE);

  if (enc->has_alpha_) {
    flags |= ALPHA_FLAG;
  }

  PutLE32(vp8x + TAG_SIZE,              VP8X_CHUNK_SIZE);
  PutLE32(vp8x + CHUNK_HEADER_SIZE,     flags);
  PutLE24(vp8x + CHUNK_HEADER_SIZE + 4, pic->width - 1);
  PutLE24(vp8x + CHUNK_HEADER_SIZE + 7, pic->height - 1);
  if (!pic->writer(vp8x, sizeof(vp8x), pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }
  return VP8_ENC_OK;
}

static int PutPaddingByte(const WebPPicture* const pic) {
  const uint8_t pad_byte[1] = { 0 };
  return !!pic->writer(pad_byte, 1, pic);
}

static WebPEncodingError PutAlphaChunk(const VP8Encoder* const enc) {
  const WebPPicture* const pic = enc->pic_;
  uint8_t alpha_chunk_hdr[CHUNK_HEADER_SIZE] = {
    'A', 'L', 'P', 'H'
  };

  assert(enc->has_alpha_);

  // Alpha chunk header.
  PutLE32(alpha_chunk_hdr + TAG_SIZE, enc->alpha_data_size_);
  if (!pic->writer(alpha_chunk_hdr, sizeof(alpha_chunk_hdr), pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }

  // Alpha chunk data.
  if (!pic->writer(enc->alpha_data_, enc->alpha_data_size_, pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }

  // Padding.
  if ((enc->alpha_data_size_ & 1) && !PutPaddingByte(pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }
  return VP8_ENC_OK;
}

static WebPEncodingError PutVP8Header(const WebPPicture* const pic,
                                      size_t vp8_size) {
  uint8_t vp8_chunk_hdr[CHUNK_HEADER_SIZE] = {
    'V', 'P', '8', ' '
  };
  assert(vp8_size == (uint32_t)vp8_size);
  PutLE32(vp8_chunk_hdr + TAG_SIZE, (uint32_t)vp8_size);
  if (!pic->writer(vp8_chunk_hdr, sizeof(vp8_chunk_hdr), pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }
  return VP8_ENC_OK;
}

#define VP8_SIGNATURE 0x9d012a              // Signature in VP8 data.

static WebPEncodingError PutVP8FrameHeader(const WebPPicture* const pic,
                                           int profile, size_t size0) {
  uint8_t vp8_frm_hdr[VP8_FRAME_HEADER_SIZE];
  uint32_t bits;

  if (size0 >= VP8_MAX_PARTITION0_SIZE) {  // partition #0 is too big to fit
    return VP8_ENC_ERROR_PARTITION0_OVERFLOW;
  }

  // Paragraph 9.1.
  bits = 0                         // keyframe (1b)
       | (profile << 1)            // profile (3b)
       | (1 << 4)                  // visible (1b)
       | ((uint32_t)size0 << 5);   // partition length (19b)
  vp8_frm_hdr[0] = (bits >>  0) & 0xff;
  vp8_frm_hdr[1] = (bits >>  8) & 0xff;
  vp8_frm_hdr[2] = (bits >> 16) & 0xff;
  // signature
  vp8_frm_hdr[3] = (VP8_SIGNATURE >> 16) & 0xff;
  vp8_frm_hdr[4] = (VP8_SIGNATURE >>  8) & 0xff;
  vp8_frm_hdr[5] = (VP8_SIGNATURE >>  0) & 0xff;
  // dimensions
  vp8_frm_hdr[6] = pic->width & 0xff;
  vp8_frm_hdr[7] = pic->width >> 8;
  vp8_frm_hdr[8] = pic->height & 0xff;
  vp8_frm_hdr[9] = pic->height >> 8;

  if (!pic->writer(vp8_frm_hdr, sizeof(vp8_frm_hdr), pic)) {
    return VP8_ENC_ERROR_BAD_WRITE;
  }
  return VP8_ENC_OK;
}

// WebP Headers.
static int PutWebPHeaders(const VP8Encoder* const enc, size_t size0,
                          size_t vp8_size, size_t riff_size) {
  WebPPicture* const pic = enc->pic_;
  WebPEncodingError err = VP8_ENC_OK;

  // RIFF header.
  err = PutRIFFHeader(enc, riff_size);
  if (err != VP8_ENC_OK) goto Error;

  // VP8X.
  if (IsVP8XNeeded(enc)) {
    err = PutVP8XHeader(enc);
    if (err != VP8_ENC_OK) goto Error;
  }

  // Alpha.
  if (enc->has_alpha_) {
    err = PutAlphaChunk(enc);
    if (err != VP8_ENC_OK) goto Error;
  }

  // VP8 header.
  err = PutVP8Header(pic, vp8_size);
  if (err != VP8_ENC_OK) goto Error;

  // VP8 frame header.
  err = PutVP8FrameHeader(pic, enc->profile_, size0);
  if (err != VP8_ENC_OK) goto Error;

  // All OK.
  return 1;

  // Error.
 Error:
  return WebPEncodingSetError(pic, err);
}

#define VP8_MAX_PARTITION_SIZE  (1 << 24)   // max size for token partition

// Partition sizes
static int EmitPartitionsSize(const VP8Encoder* const enc,
                              WebPPicture* const pic) {
  uint8_t buf[3 * (MAX_NUM_PARTITIONS - 1)];
  int p;
  for (p = 0; p < enc->num_parts_ - 1; ++p) {
    const size_t part_size = VP8BitWriterSize(enc->parts_ + p);
    if (part_size >= VP8_MAX_PARTITION_SIZE) {
      return WebPEncodingSetError(pic, VP8_ENC_ERROR_PARTITION_OVERFLOW);
    }
    buf[3 * p + 0] = (part_size >>  0) & 0xff;
    buf[3 * p + 1] = (part_size >>  8) & 0xff;
    buf[3 * p + 2] = (part_size >> 16) & 0xff;
  }
  return p ? pic->writer(buf, 3 * p, pic) : 1;
}

int VP8EncWrite(VP8Encoder* const enc) {
  WebPPicture* const pic = enc->pic_;
  VP8BitWriter* const bw = &enc->bw_;
  const int task_percent = 19;
  const int percent_per_part = task_percent / enc->num_parts_;
  const int final_percent = enc->percent_ + task_percent;
  int ok = 0;
  size_t vp8_size, pad, riff_size;
  int p;

  // Partition #0 with header and partition sizes
  ok = GeneratePartition0(enc);
  if (!ok) return 0;

  // Compute VP8 size
  vp8_size = VP8_FRAME_HEADER_SIZE +
             VP8BitWriterSize(bw) +
             3 * (enc->num_parts_ - 1);
  for (p = 0; p < enc->num_parts_; ++p) {
    vp8_size += VP8BitWriterSize(enc->parts_ + p);
  }
  pad = vp8_size & 1;
  vp8_size += pad;

  // Compute RIFF size
  // At the minimum it is: "WEBPVP8 nnnn" + VP8 data size.
  riff_size = TAG_SIZE + CHUNK_HEADER_SIZE + vp8_size;
  if (IsVP8XNeeded(enc)) {  // Add size for: VP8X header + data.
    riff_size += CHUNK_HEADER_SIZE + VP8X_CHUNK_SIZE;
  }
  if (enc->has_alpha_) {  // Add size for: ALPH header + data.
    const uint32_t padded_alpha_size = enc->alpha_data_size_ +
                                       (enc->alpha_data_size_ & 1);
    riff_size += CHUNK_HEADER_SIZE + padded_alpha_size;
  }
  // Sanity check.
  if (riff_size > 0xfffffffeU) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_FILE_TOO_BIG);
  }

  // Emit headers and partition #0
  {
    const uint8_t* const part0 = VP8BitWriterBuf(bw);
    const size_t size0 = VP8BitWriterSize(bw);
    ok = ok && PutWebPHeaders(enc, size0, vp8_size, riff_size)
            && pic->writer(part0, size0, pic)
            && EmitPartitionsSize(enc, pic);
    VP8BitWriterWipeOut(bw);    // will free the internal buffer.
  }

  // Token partitions
  for (p = 0; p < enc->num_parts_; ++p) {
    const uint8_t* const buf = VP8BitWriterBuf(enc->parts_ + p);
    const size_t size = VP8BitWriterSize(enc->parts_ + p);
    if (size) ok = ok && pic->writer(buf, size, pic);
    VP8BitWriterWipeOut(enc->parts_ + p);    // will free the internal buffer.
    ok = ok && WebPReportProgress(pic, enc->percent_ + percent_per_part,
                                  &enc->percent_);
  }

  // Padding byte
  if (ok && pad) {
    ok = PutPaddingByte(pic);
  }

  enc->coded_size_ = (int)(CHUNK_HEADER_SIZE + riff_size);
  ok = ok && WebPReportProgress(pic, final_percent, &enc->percent_);
  return ok;
}

static void FinalizePSNR(const VP8Encoder* const enc) {
  WebPAuxStats* stats = enc->pic_->stats;
  const uint64_t size = enc->sse_count_;
  const uint64_t* const sse = enc->sse_;
  stats->PSNR[0] = (float)GetPSNR(sse[0], size);
  stats->PSNR[1] = (float)GetPSNR(sse[1], size / 4);
  stats->PSNR[2] = (float)GetPSNR(sse[2], size / 4);
  stats->PSNR[3] = (float)GetPSNR(sse[0] + sse[1] + sse[2], size * 3 / 2);
  stats->PSNR[4] = (float)GetPSNR(sse[3], size);
}

static void StoreStats(VP8Encoder* const enc) {
#if !defined(WEBP_DISABLE_STATS)
  WebPAuxStats* const stats = enc->pic_->stats;
  if (stats != NULL) {
    int i, s;
    for (i = 0; i < NUM_MB_SEGMENTS; ++i) {
      stats->segment_level[i] = enc->dqm_[i].fstrength_;
      stats->segment_quant[i] = enc->dqm_[i].quant_;
      for (s = 0; s <= 2; ++s) {
        stats->residual_bytes[s][i] = enc->residual_bytes_[s][i];
      }
    }
    FinalizePSNR(enc);
    stats->coded_size = enc->coded_size_;
    for (i = 0; i < 3; ++i) {
      stats->block_count[i] = enc->block_count_[i];
    }
  }
#else  // defined(WEBP_DISABLE_STATS)
  WebPReportProgress(enc->pic_, 100, &enc->percent_);  // done!
#endif  // !defined(WEBP_DISABLE_STATS)
}

int VP8EncDeleteAlpha(VP8Encoder* const enc) {
  int ok = 1;
  if (enc->thread_level_ > 0) {
    WebPWorker* const worker = &enc->alpha_worker_;
    // finish anything left in flight
    ok = WebPGetWorkerInterface()->Sync(worker);
    // still need to end the worker, even if !ok
    WebPGetWorkerInterface()->End(worker);
  }
  WebPSafeFree(enc->alpha_data_);
  enc->alpha_data_ = NULL;
  enc->alpha_data_size_ = 0;
  enc->has_alpha_ = 0;
  return ok;
}

static int DeleteVP8Encoder(VP8Encoder* enc) {
  int ok = 1;
  if (enc != NULL) {
    ok = VP8EncDeleteAlpha(enc);
    VP8TBufferClear(&enc->tokens_);
    WebPSafeFree(enc);
  }
  return ok;
}

int WebPEncode(const WebPConfig* config, WebPPicture* pic) {
  int ok = 0;
  if (pic == NULL) return 0;

  WebPEncodingSetError(pic, VP8_ENC_OK);  // all ok so far
  if (config == NULL) {  // bad params
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if (!WebPValidateConfig(config)) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }
  if (pic->width <= 0 || pic->height <= 0) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_BAD_DIMENSION);
  }
  if (pic->width > WEBP_MAX_DIMENSION || pic->height > WEBP_MAX_DIMENSION) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_BAD_DIMENSION);
  }

  if (pic->stats != NULL) memset(pic->stats, 0, sizeof(*pic->stats));

    VP8Encoder* enc = NULL;

    if (!config->exact) {
      WebPCleanupTransparentArea(pic);
    }

//	struct timespec time_start={0, 0},time_end={0, 0};
//	clock_gettime(CLOCK_REALTIME, &time_start);

    enc = InitVP8Encoder(config, pic);
    if (enc == NULL) return 0;  // pic->error is already set.
    // Note: each of the tasks below account for 20% in the progress report.
    ok = VP8EncAnalyze(enc);

    // Analysis is done, proceed to actual coding.
    ok = ok && VP8EncStartAlpha(enc);   // possibly done in parallel
    if (!enc->use_tokens_) {
      ok = ok && VP8EncLoop(enc);
    } else {
      ok = ok && VP8EncTokenLoop(enc);
    }
    ok = ok && VP8EncFinishAlpha(enc);

    ok = ok && VP8EncWrite(enc);
    StoreStats(enc);
    if (!ok) {
      VP8EncFreeBitWriters(enc);
    }
    ok &= DeleteVP8Encoder(enc);  // must always be called, even if !ok

//	clock_gettime(CLOCK_REALTIME, &time_end);
//	fprintf(stdout, "%lluns\n", (long long)((double)((time_end.tv_sec-time_start.tv_sec)*1000000000+(time_end.tv_nsec-time_start.tv_nsec))));

  return ok;
}

int main(int argc, const char *argv[]) {
  int return_value = -1;
  const char *in_file = NULL, *out_file = NULL;
  FILE *out = NULL;
  int c;
  int short_output = 0;
  int keep_alpha = 1;
  int show_progress = 0;
  WebPPicture picture;
  WebPConfig config;
  WebPAuxStats stats;
  WebPMemoryWriter memory_writer;
  Stopwatch stop_watch;

  WebPMemoryWriterInit(&memory_writer);
  if (!WebPPictureInit(&picture) ||
      !WebPConfigInit(&config)) {
    fprintf(stderr, "Error! Version mismatch!\n");
    return -1;
  }

  if (argc == 1) {
    HelpShort();
    return 0;
  }

  for (c = 1; c < argc; ++c) {
    int parse_error = 0;
    if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      HelpShort();
      return 0;
    } else if (!strcmp(argv[c], "-H") || !strcmp(argv[c], "-longhelp")) {
      HelpLong();
      return 0;
    } else if (!strcmp(argv[c], "-o") && c < argc - 1) {
      out_file = argv[++c];
    } else if (!strcmp(argv[c], "-s") && c < argc - 2) {
      picture.width = ExUtilGetInt(argv[++c], 0, &parse_error);
      picture.height = ExUtilGetInt(argv[++c], 0, &parse_error);
      if (picture.width > WEBP_MAX_DIMENSION || picture.width < 0 ||
          picture.height > WEBP_MAX_DIMENSION ||  picture.height < 0) {
        fprintf(stderr,
                "Specified dimension (%d x %d) is out of range.\n",
                picture.width, picture.height);
        goto Error;
      }
    } else if (!strcmp(argv[c], "-q") && c < argc - 1) {
      config.quality = ExUtilGetFloat(argv[++c], &parse_error);
    } else if (!strcmp(argv[c], "-version")) {
      const int version = WebPGetEncoderVersion();
      printf("%d.%d.%d\n",
             (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
      return 0;
    } else if (!strcmp(argv[c], "-v")) {
      verbose = 1;
    } else if (!strcmp(argv[c], "--")) {
      if (c < argc - 1) in_file = argv[++c];
      break;
    } else if (argv[c][0] == '-') {
      fprintf(stderr, "Error! Unknown option '%s'\n", argv[c]);
      HelpLong();
      return -1;
    } else {
      in_file = argv[c];
    }

    if (parse_error) {
      HelpLong();
      return -1;
    }
  }

  if (in_file == NULL) {
    fprintf(stderr, "No input file specified!\n");
    HelpShort();
    goto Error;
  }

  // Check for unsupported command line options for lossless mode and log
  // warning for such options.

  if (!WebPValidateConfig(&config)) {
    fprintf(stderr, "Error! Invalid configuration.\n");
    goto Error;
  }

  // Read the input.
  if (verbose) {
    StopwatchReset(&stop_watch);
  }
  if (!ReadPicture(in_file, &picture, keep_alpha, NULL)) {
    fprintf(stderr, "Error! Cannot read input picture file '%s'\n", in_file);
    goto Error;
  }
  picture.progress_hook = NULL;

  if (verbose) {
    const double read_time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "Time to read input: %.3fs\n", read_time);
  }

  // Open the output
  if (out_file != NULL) {
    const int use_stdout = !strcmp(out_file, "-");
    out = use_stdout ? ImgIoUtilSetBinaryMode(stdout) : fopen(out_file, "wb");
    if (out == NULL) {
      fprintf(stderr, "Error! Cannot open output file '%s'\n", out_file);
      goto Error;
    } else {
      fprintf(stderr, "Saving file '%s'\n", out_file);
    }
      picture.writer = MyWriter;
      picture.custom_ptr = (void*)out;
  } else {
    out = NULL;
    fprintf(stderr, "No output file specified (no -o flag). Encoding will\n");
    fprintf(stderr, "be performed, but its results discarded.\n\n");
  }
  picture.stats = &stats;
  picture.user_data = (void*)in_file;

  // Compress.
  if (verbose) {
    StopwatchReset(&stop_watch);
  }
  if (!WebPEncode(&config, &picture)) {
    fprintf(stderr, "Error! Cannot encode picture as WebP\n");
    fprintf(stderr, "Error code: %d (%s)\n",
            picture.error_code, kErrorMessages[picture.error_code]);
    goto Error;
  }
  if (verbose) {
    const double encode_time = StopwatchReadAndReset(&stop_watch);
    fprintf(stderr, "Time to encode picture: %.3fs\n", encode_time);
  }

  // Write info
  PrintExtraInfoLossy(&picture, short_output, config.low_memory, in_file);

  return_value = 0;

 Error:
  WebPMemoryWriterClear(&memory_writer);
  WebPPictureFree(&picture);
  if (out != NULL && out != stdout) {
    fclose(out);
  }

  return return_value;
}
