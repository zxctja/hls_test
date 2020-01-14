#include <stdint.h>
#include <stdio.h>

typedef int64_t score_t;     // type used for scores, rate, distortion

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

typedef struct DATA_O {
	VP8ModeScore info;
	uint8_t mbtype;
	uint8_t is_skipped;
	int max_edge_;
	uint8_t pad[4];
	} DATA_O;


typedef struct VP8Matrix {
  uint16_t q_[16];        // quantizer steps
  uint16_t iq_[16];       // reciprocals, fixed point.
  uint32_t bias_[16];     // rounding bias
  uint32_t zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t sharpen_[16];  // frequency boosters for slight sharpening
} VP8Matrix;


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
  uint8_t pad[24];
} VP8SegmentInfo;

typedef struct {
  uint16_t y1_q_[16];        // quantizer steps
  uint16_t y1_iq_[16];       // reciprocals, fixed point.
  uint32_t y1_bias_[16];     // rounding bias
  uint32_t y1_zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t y1_sharpen_[16];  // frequency boosters for slight sharpening
  uint16_t y2_q_[16];        // quantizer steps
  uint16_t y2_iq_[16];       // reciprocals, fixed point.
  uint32_t y2_bias_[16];     // rounding bias
  uint32_t y2_zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t y2_sharpen_[16];  // frequency boosters for slight sharpening
  uint16_t uv_q_[16];        // quantizer steps
  uint16_t uv_iq_[16];       // reciprocals, fixed point.
  uint32_t uv_bias_[16];     // rounding bias
  uint32_t uv_zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t uv_sharpen_[16];  // frequency boosters for slight sharpening
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
} VP8SegmentInfo_snap;

int main (void){
DATA_O a;
VP8SegmentInfo b;
fprintf(stdout, "%lu\n", sizeof(a));
fprintf(stdout, "%lu\n", sizeof(b));
return 1;

}
