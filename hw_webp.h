#include <stdint.h>

typedef int64_t score_t;     // type used for scores, rate, distortion

typedef struct VP8Matrix {
  uint16_t q_[16];        // quantizer steps
  uint16_t iq_[16];       // reciprocals, fixed point.
  uint32_t bias_[16];     // rounding bias
  uint32_t zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t sharpen_[16];  // frequency boosters for slight sharpening
} VP8Matrix;

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

enum { MAX_LF_LEVELS = 64,       // Maximum loop filter level
       MAX_VARIABLE_LEVEL = 67,  // last (inclusive) level with variable cost
       MAX_LEVEL = 2047          // max level (note: max codable is 2047 + 67)
     };

typedef double LFStats_My[MAX_LF_LEVELS];

typedef struct DATA {
		uint8_t Yin[16*16];
		uint8_t UVin[8*16];
		uint8_t Yout16[16*16];
		uint8_t Yout4[16*16];
		uint8_t UVout[8*16];
		VP8SegmentInfo dqm;
		uint8_t left_y[16];
		uint8_t top_y[20];
		uint8_t top_left_y;
		uint8_t left_u[8];
		uint8_t top_u[8];
		uint8_t top_left_u;
		uint8_t left_v[8];
		uint8_t top_v[8];
		uint8_t top_left_v;
		uint8_t mbtype;
		uint8_t is_skipped;
		int x;
		int y;
		int mb_w;
		int mb_h;
		int count_down;
		LFStats_My lf_stats;
		} DATA;

void VP8IteratorSaveBoundary_snap(DATA* data_it);

int VP8IteratorNext_snap(DATA* data_it);

void VP8Decimate_snap(uint8_t Yin[16*16], uint8_t Yout16[16*16], uint8_t Yout4[16*16],
		VP8SegmentInfo* const dqm, uint8_t UVin[8*16], uint8_t UVout[8*16], uint8_t* is_skipped,
		uint8_t left_y[16], uint8_t top_y[20], uint8_t top_left_y, uint8_t* mbtype,
		uint8_t left_u[8], uint8_t top_u[8], uint8_t top_left_u,uint8_t left_v[8],
		uint8_t top_v[8], uint8_t top_left_v, int x, int y, VP8ModeScore* const rd);
		
void VP8StoreFilterStats_snap(VP8SegmentInfo* const dqm, LFStats_My lf_stats,
		uint8_t Yin[16*16], uint8_t Yout16[16*16], uint8_t Yout4[16*16],
		uint8_t UVin[8*16], uint8_t UVout[8*16], uint8_t mbtype, uint8_t skip);
