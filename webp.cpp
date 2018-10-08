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

#define WEBP_TSAN_IGNORE_FUNCTION

typedef enum {
  kSSE2,
  kSSE3,
  kSlowSSSE3,  // special feature for slow SSSE3 architectures
  kSSE4_1,
  kAVX,
  kAVX2,
  kNEON,
  kMIPS32,
  kMIPSdspR2,
  kMSA
} CPUFeature;

typedef int (*VP8CPUInfo)(CPUFeature feature);

#define WEBP_DSP_INIT(func) do {                                    \
  static volatile VP8CPUInfo func ## _last_cpuinfo_used =           \
      (VP8CPUInfo)&func ## _last_cpuinfo_used;                      \
  if (func ## _last_cpuinfo_used == VP8GetCPUInfo) break;           \
  func();                                                           \
  func ## _last_cpuinfo_used = VP8GetCPUInfo;                       \
} while (0)

#define WEBP_DSP_INIT_FUNC(name)                             \
  static WEBP_TSAN_IGNORE_FUNCTION void name ## _body(void); \
  WEBP_TSAN_IGNORE_FUNCTION void name(void) {                \
    WEBP_DSP_INIT(name ## _body);                            \
  }                                                          \
  static WEBP_TSAN_IGNORE_FUNCTION void name ## _body(void)

WEBP_DSP_INIT_FUNC(WebPInitAlphaProcessing) {
  WebPMultARGBRow = WebPMultARGBRow_C;
  WebPMultRow = WebPMultRow_C;
  WebPApplyAlphaMultiply4444 = ApplyAlphaMultiply_16b_C;

#ifdef WORDS_BIGENDIAN
  WebPPackARGB = PackARGB_C;
#endif
  WebPPackRGB = PackRGB_C;
#if !WEBP_NEON_OMIT_C_CODE
  WebPApplyAlphaMultiply = ApplyAlphaMultiply_C;
  WebPDispatchAlpha = DispatchAlpha_C;
  WebPDispatchAlphaToGreen = DispatchAlphaToGreen_C;
  WebPExtractAlpha = ExtractAlpha_C;
  WebPExtractGreen = ExtractGreen_C;
#endif

  WebPHasAlpha8b = HasAlpha8b_C;
  WebPHasAlpha32b = HasAlpha32b_C;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_USE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitAlphaProcessingSSE2();
#if defined(WEBP_USE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        WebPInitAlphaProcessingSSE41();
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitAlphaProcessingMIPSdspR2();
    }
#endif
  }

#if defined(WEBP_USE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPInitAlphaProcessingNEON();
  }
#endif

  assert(WebPMultARGBRow != NULL);
  assert(WebPMultRow != NULL);
  assert(WebPApplyAlphaMultiply != NULL);
  assert(WebPApplyAlphaMultiply4444 != NULL);
  assert(WebPDispatchAlpha != NULL);
  assert(WebPDispatchAlphaToGreen != NULL);
  assert(WebPExtractAlpha != NULL);
  assert(WebPExtractGreen != NULL);
#ifdef WORDS_BIGENDIAN
  assert(WebPPackARGB != NULL);
#endif
  assert(WebPPackRGB != NULL);
  assert(WebPHasAlpha8b != NULL);
  assert(WebPHasAlpha32b != NULL);
}


static int CheckNonOpaque(const uint8_t* alpha, int width, int height,
                          int x_step, int y_step) {
  if (alpha == NULL) return 0;
  WebPInitAlphaProcessing();
  if (x_step == 1) {
    for (; height-- > 0; alpha += y_step) {
      if (WebPHasAlpha8b(alpha, width)) return 1;
    }
  } else {
    for (; height-- > 0; alpha += y_step) {
      if (WebPHasAlpha32b(alpha, width)) return 1;
    }
  }
  return 0;
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
#if defined(USE_GAMMA_COMPRESSION) && defined(USE_INVERSE_ALPHA_TABLE)
    assert(kAlphaFix + kGammaFix <= 31);
#endif
  }

  if (use_iterative_conversion) {
    InitGammaTablesS();
    if (!PreprocessARGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, picture)) {
      return 0;
    }
    if (has_alpha) {
      WebPExtractAlpha(a_ptr, rgb_stride, width, height,
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
    WebPInitConvertARGBToYUV();
    InitGammaTables();

    if (tmp_rgb == NULL) return 0;  // malloc error

    // Downsample Y/U/V planes, two rows at a time
    for (y = 0; y < (height >> 1); ++y) {
      int rows_have_alpha = has_alpha;
      if (use_dsp) {
        if (is_rgb) {
          WebPConvertRGB24ToY(r_ptr, dst_y, width);
          WebPConvertRGB24ToY(r_ptr + rgb_stride,
                              dst_y + picture->y_stride, width);
        } else {
          WebPConvertBGR24ToY(b_ptr, dst_y, width);
          WebPConvertBGR24ToY(b_ptr + rgb_stride,
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
        rows_have_alpha &= !WebPExtractAlpha(a_ptr, rgb_stride, width, 2,
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
        WebPConvertRGBA32ToUV(tmp_rgb, dst_u, dst_v, uv_width);
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
          WebPConvertRGB24ToY(r_ptr, dst_y, width);
        } else {
          WebPConvertBGR24ToY(b_ptr, dst_y, width);
        }
      } else {
        ConvertRowToY(r_ptr, g_ptr, b_ptr, step, dst_y, width, rg);
      }
      if (row_has_alpha) {
        row_has_alpha &= !WebPExtractAlpha(a_ptr, 0, width, 1, dst_a, 0);
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
        WebPConvertRGBA32ToUV(tmp_rgb, dst_u, dst_v, uv_width);
      } else {
        ConvertRowsToUV(tmp_rgb, dst_u, dst_v, uv_width, rg);
      }
    }
    WebPSafeFree(tmp_rgb);
  }
  return 1;
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

  VP8LDspInit();
  WebPInitAlphaProcessing();

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
        VP8LConvertBGRAToRGBA((const uint32_t*)rgb, width, (uint8_t*)dst);
#endif
        rgb += rgb_stride;
        dst += picture->argb_stride;
      }
    }
  } else {
    uint32_t* dst = picture->argb;
    assert(step >= 3);
    for (y = 0; y < height; ++y) {
      WebPPackRGB(r_ptr, g_ptr, b_ptr, width, step, dst);
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
