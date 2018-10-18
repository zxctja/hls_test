#include <stdint.h>
#include <stdlib.h>
#include <ap_int.h>


void Fill(uint8_t* dst, int value, int size) {
#pragma HLS inline
  int i,j;
  for (j = 0; j < size; ++j) {
#pragma HLS unroll
    for(i = 0; i < size; ++i){
#pragma HLS unroll
        dst[j * size + i] = value;
    }
  }
}

void DCMode(uint8_t* dst, uint8_t* left, uint8_t* top,
                               int size, int round, int shift) {
//pragma HLS inline
  int DC = 0;
  int j;
	for (j = 0; j < size; ++j){
#pragma HLS unroll
		DC += top[j] + left[j];
	}
  DC = (DC + round) >> shift;
  Fill(dst, DC, size);
}

void VerticalPred(uint8_t* dst, uint8_t* top, int size) {
//#pragma HLS inline
  int i,j;
    for (j = 0; j < size; ++j) {
#pragma HLS unroll
    	for(i = 0; i < size; ++i){
#pragma HLS unroll
    		dst[j * size + i] = top[i];
    	}
    }
}

void HorizontalPred(uint8_t* dst, uint8_t* left, int size) {
//#pragma HLS inline
    int i,j;
    for (j = 0; j < size; ++j) {
#pragma HLS unroll
    	for(i = 0; i < size; ++i){
#pragma HLS unroll
    		dst[j * size + i] = left[j];
    	}
    }
}

void TrueMotion(uint8_t* dst, uint8_t* left, uint8_t* top, uint8_t top_left, int size, int x, int y) {
//#pragma HLS inline
  int i,j;
  ap_int<10> tmp;
  if (x != 0) {
    if (y != 0) {
      for (j = 0; j < size; ++j) {
#pragma HLS unroll
        for (i = 0; i < size; ++i) {
#pragma HLS unroll
        	tmp = top[i] + left[j] - top_left;
        	dst[j * size + i] = (!(tmp & ~0xff)) ? (uint8_t)tmp : (tmp < 0) ? 0 : 255;
        }
      }
    } else {
      HorizontalPred(dst, left, size);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (y != 0) {
      VerticalPred(dst, top, size);
    } else {
      Fill(dst, 129, size);
    }
  }
}

void Intra16Preds_C(uint8_t YPred[4][16*16], uint8_t left_y[16],
		uint8_t* top_y, uint8_t top_left_y, int x, int y) {
//#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=top_y complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_y complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred complete dim=0
  DCMode(YPred[0], left_y, top_y, 16, 16, 5);
  VerticalPred(YPred[1], top_y, 16);
  HorizontalPred(YPred[2], left_y, 16);
  TrueMotion(YPred[3], left_y, top_y, top_left_y, 16, x, y);
}

void IntraChromaPreds_C(
		uint8_t UVPred[8][8*8],
        uint8_t left_u[8], uint8_t top_u[8], uint8_t top_left_u,
		uint8_t left_v[8], uint8_t top_v[8], uint8_t top_left_v,
		int x, int y) {
//#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=top_u complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_u complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_v complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_v complete dim=1
#pragma HLS ARRAY_PARTITION variable=UVPred complete dim=0
  // U block
  DCMode(UVPred[0], left_u, top_u, 8, 8, 4);
  VerticalPred(UVPred[1], top_u, 8);
  HorizontalPred(UVPred[2], left_u, 8);
  TrueMotion(UVPred[3], left_u, top_u, top_left_u, 8, x, y);
  // V block
  DCMode(UVPred[4], left_v, top_v, 8, 8, 4);
  VerticalPred(UVPred[5], top_v, 8);
  HorizontalPred(UVPred[6], left_v, 8);
  TrueMotion(UVPred[7], left_v, top_v, top_left_v, 8, x, y);
}

// luma 4x4 prediction

#define DST(x, y) dst[(x) + (y) * 4]
#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

void VE4(uint8_t* dst, uint8_t top_left, uint8_t* top, uint8_t* top_right) {    // vertical
  uint8_t vals[4] = {
    AVG3(top_left, top[0], top[1]),
    AVG3(top[0], top[1], top[2]),
    AVG3(top[1], top[2], top[3]),
    AVG3(top[2], top[3], top_right[0])
  };
  int i,j;
    for (j = 0; j < 4; ++j) {
#pragma HLS unroll
    	for(i = 0; i < 4; ++i){
#pragma HLS unroll
    		dst[j * 4 + i] = vals[i];
    	}
    }
}

static void HE4(uint8_t* dst, uint8_t* left, uint8_t top_left) {    // horizontal
  const int X = top_left;
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  ((uint32_t*)dst)[0] = 0x01010101U * AVG3(X, I, J);
  ((uint32_t*)dst)[1] = 0x01010101U * AVG3(I, J, K);
  ((uint32_t*)dst)[2] = 0x01010101U * AVG3(J, K, L);
  ((uint32_t*)dst)[3] = 0x01010101U * AVG3(K, L, L);
}

void DC4(uint8_t* dst, uint8_t* top, uint8_t* left) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i){
#pragma HLS unroll
	  dc += top[i] + left[i];
  }
  dc = dc >> 3;
  Fill(dst, dc, 4);
}

void RD4(uint8_t* dst, uint8_t* left, uint8_t top_left, uint8_t* top) {
  const int X = top_left;
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
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

void LD4(uint8_t* dst, uint8_t* top, uint8_t* top_right) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top_right[0];
  const int F = top_right[1];
  const int G = top_right[2];
  const int H = top_right[3];
  DST(0, 0)                                     = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1)                         = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2)             = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3)             = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3)                         = AVG3(F, G, H);
  DST(3, 3)                                     = AVG3(G, H, H);
}

void VR4(uint8_t* dst, uint8_t* left, uint8_t top_left, uint8_t* top) {
  const int X = top_left;
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
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

void VL4(uint8_t* dst, uint8_t* top, uint8_t* top_right) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top_right[0];
  const int F = top_right[1];
  const int G = top_right[2];
  const int H = top_right[3];
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

void HU4(uint8_t* dst, uint8_t* left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  DST(0, 0) =             AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) =             AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) =
  DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

void HD4(uint8_t* dst, uint8_t* left, uint8_t top_left, uint8_t* top) {
  const int X = top_left;
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
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

void TM4(uint8_t* dst, uint8_t* top, uint8_t* left, uint8_t top_left) {
  int i, j;
  ap_int<10> tmp;
  for (j = 0; j < 4; ++j) {
#pragma HLS unroll
    for (i = 0; i < 4; ++i) {
#pragma HLS unroll
      tmp = top[i] + left[j] - top_left;
      dst[j * 4 + i] = (tmp>0xff) ? 0xff : (tmp<0) ? 0 : (uint8_t)tmp;
    }
  }
}

void Intra4Preds_C(
		uint8_t Pred[10][16], uint8_t left[4], uint8_t top_left, uint8_t top[4], uint8_t top_right[4]) {
//#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=left complete dim=1
#pragma HLS ARRAY_PARTITION variable=top complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_right complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred complete dim=0

  DC4(Pred[0], top, left);
  TM4(Pred[1], top, left, top_left);
  VE4(Pred[2], top_left, top, top_right);
  HE4(Pred[3], left, top_left);
  RD4(Pred[4], left, top_left, top);
  VR4(Pred[5], left, top_left, top);
  LD4(Pred[6], top, top_right);
  VL4(Pred[7], top, top_right);
  HD4(Pred[8], left, top_left, top);
  HU4(Pred[9], top);
}

typedef struct VP8Matrix {
  uint16_t q_[16];        // quantizer steps
  uint16_t iq_[16];       // reciprocals, fixed point.
  uint32_t bias_[16];     // rounding bias
  uint32_t zthresh_[16];  // value below which a coefficient is zeroed
  uint16_t sharpen_[16];  // frequency boosters for slight sharpening
} VP8Matrix;

void FTransform_C(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  int i;
  int tmp[16];
  for (i = 0; i < 4; ++i, src += 4, ref += 4) {
#pragma HLS unroll
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
#pragma HLS unroll
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

void FTransformWHT_C(const int16_t* in, int16_t* out) {
  // input is 12b signed
  int32_t tmp[16];
  int i;
  for (i = 0; i < 4; ++i, in += 4) {
#pragma HLS unroll
    const int a0 = (in[0] + in[2]);  // 13b
    const int a1 = (in[1] + in[3]);
    const int a2 = (in[1] - in[3]);
    const int a3 = (in[0] - in[2]);
    tmp[0 + i * 4] = a0 + a1;   // 14b
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
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

enum { MAX_LF_LEVELS = 64,       // Maximum loop filter level
       MAX_VARIABLE_LEVEL = 67,  // last (inclusive) level with variable cost
       MAX_LEVEL = 2047          // max level (note: max codable is 2047 + 67)
     };

static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

#define QFIX 17

int QUANTDIV(uint32_t n, uint32_t iQ, uint32_t B) {
  return (int)((n * iQ + B) >> QFIX);
}

int QuantizeBlock_C(int16_t* in, int16_t* out,
                           const VP8Matrix* const mtx) {
  int last = -1;
  int n;
  int16_t in_tmp[16];
  for (n = 0; n < 16; ++n) {
#pragma HLS unroll
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
      in_tmp[j] = level * (int)Q;
      out[n] = level;
      if (level) last = n;
    } else {
      out[n] = 0;
      in_tmp[j] = 0;
    }
  }
  for (n = 0; n < 16; ++n) {
#pragma HLS unroll
	in[n] = in_tmp[n];
  }
  return (last >= 0);
}



void TransformWHT_C(const int16_t* in, int16_t* out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
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
#pragma HLS unroll
    const int dc = tmp[0 + i * 4] + 3;    // w/ rounder
    const int a0 = dc             + tmp[3 + i * 4];
    const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
    const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
    const int a3 = dc             - tmp[3 + i * 4];
    out[0] = (a0 + a1) >> 3;
    out[1] = (a3 + a2) >> 3;
    out[2] = (a0 - a1) >> 3;
    out[3] = (a3 - a2) >> 3;
    out += 4;
  }
}

typedef int16_t fixed_t;

uint8_t clip_8b(fixed_t v) {
  return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

#define STORE(x, y, v) \
  dst[(x) + (y) * 4] = clip_8b(ref[(x) + (y) * 4] + ((v) >> 3))

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)

void ITransformOne(const uint8_t* ref, const int16_t* in,
                                      uint8_t* dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
#pragma HLS unroll
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
#pragma HLS unroll
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


int ReconstructIntra16(
		const uint8_t YPred[16*16], const uint8_t Ysrc[16*16], uint8_t Yout[16*16],
		int16_t y_ac_levels[16][16], int16_t y_dc_levels[16], VP8Matrix y1, VP8Matrix y2) {
//#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=y1.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y2.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y2.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y2.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y2.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y2.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=y_dc_levels complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
#pragma HLS ARRAY_PARTITION variable=Ysrc complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred complete dim=1

  int nz = 0;
  int n;
  int16_t tmp[16][16], dc_tmp[16], tmp_dc[16];
  uint8_t tmp_src[16][16], tmp_pred[16][16], tmp_out[16][16];

  const uint16_t VP8Scan[16] = {  // Luma
    0 +  0 * 16,  4 +  0 * 16, 8 +  0 * 16, 12 +  0 * 16,
    0 +  4 * 16,  4 +  4 * 16, 8 +  4 * 16, 12 +  4 * 16,
    0 +  8 * 16,  4 +  8 * 16, 8 +  8 * 16, 12 +  8 * 16,
    0 + 12 * 16,  4 + 12 * 16, 8 + 12 * 16, 12 + 12 * 16,
  };

#pragma HLS ARRAY_PARTITION variable=tmp_src complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_dc complete dim=1
#pragma HLS ARRAY_PARTITION variable=dc_tmp complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_out complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_pred complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp complete dim=0
#pragma HLS ARRAY_PARTITION variable=VP8Scan complete dim=1

  int i,j;
  for(n = 0; n < 16; n++){
#pragma HLS unroll
	  for(j = 0; j < 4; j++){
#pragma HLS unroll
		  for(i = 0; i < 4; i++){
#pragma HLS unroll
			  tmp_src[n][j * 4 + i] = Ysrc[VP8Scan[n] + j * 16 + i];
			  tmp_pred[n][j * 4 + i] = YPred[VP8Scan[n] + j * 16 + i];
		  }
	  }
  }

  for (n = 0; n < 16; n++) {
#pragma HLS unroll
	  FTransform_C(tmp_src[n], tmp_pred[n], tmp[n]);
  }

  for(n = 0; n < 16; n++){
#pragma HLS unroll
	  tmp_dc[n] = tmp[n][0];
	  tmp[n][0] = 0;
  }

  FTransformWHT_C(tmp_dc, dc_tmp);

  nz |= QuantizeBlock_C(dc_tmp, y_dc_levels, &y2) << 24;

  for (n = 0; n < 16; n++) {
#pragma HLS unroll
    // Zero-out the first coeff, so that: a) nz is correct below, and
    // b) finding 'last' non-zero coeffs in SetResidualCoeffs() is simplified.
    nz |= QuantizeBlock_C(tmp[n],y_ac_levels[n], &y1) << n;

  }

  TransformWHT_C(dc_tmp, tmp_dc);

  for(n = 0; n < 16; n++){
#pragma HLS unroll
	  tmp[n][0] = tmp_dc[n];
  }

  for (n = 0; n < 16; n++) {
#pragma HLS unroll
	  ITransformOne(tmp_pred[n], tmp[n], tmp_out[n]);
  }

  for(n = 0; n < 16; n++){
#pragma HLS unroll
	  for(j = 0; j < 4; j++){
#pragma HLS unroll
		  for(i = 0; i < 4; i++){
#pragma HLS unroll
			  Yout[VP8Scan[n] + j * 16 + i] = tmp_out[n][j * 4 + i];
		  }
	  }
  }

  return nz;
}

int ReconstructIntra4(int16_t levels[16], const uint8_t y_p[16],
		const uint8_t y_src[16], uint8_t y_out[16], VP8Matrix y1) {
//#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=y1.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=levels complete dim=1
#pragma HLS ARRAY_PARTITION variable=y_out complete dim=1
#pragma HLS ARRAY_PARTITION variable=y_src complete dim=1
#pragma HLS ARRAY_PARTITION variable=y_p complete dim=1
  int nz = 0;
  int16_t tmp[16];
#pragma HLS ARRAY_PARTITION variable=tmp complete dim=1

  FTransform_C(y_src, y_p, tmp);

  nz = QuantizeBlock_C(tmp, levels, &y1);

  ITransformOne(y_p, tmp, y_out);

  return nz;
}

typedef int8_t DError[2 /* u/v */][2 /* top or left */];

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
  } else {
  *v = 0;
  return (sign ? -V : V) >> DSCALE;
  }
}

static void CorrectDCValues(DError top_derr[1024], DError left_derr, int x,
                            const VP8Matrix* const mtx,
                            int16_t tmp[][16], int8_t derr[2][3]) {
  //         | top[0] | top[1]
  // --------+--------+---------
  // left[0] | tmp[0]   tmp[1]  <->   err0 err1
  // left[1] | tmp[2]   tmp[3]        err2 err3
  //
  // Final errors {err1,err2,err3} are preserved and later restored
  // as top[]/left[] on the next block.
  int ch;
  for (ch = 0; ch <= 1; ++ch) {
#pragma HLS unroll
    const int8_t* const top = top_derr[x][ch];
    const int8_t* const left = left_derr[ch];
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
    derr[ch][0] = (int8_t)err1;
    derr[ch][1] = (int8_t)err2;
    derr[ch][2] = (int8_t)err3;
  }
}

#undef C1
#undef C2

int ReconstructUV(int16_t uv_levels[8][16],const uint8_t uv_p[8*16],
		const uint8_t uv_src[8*16], uint8_t uv_out[8*16], VP8Matrix uv,
		DError top_derr[1024], DError left_derr, int x, int8_t derr[2][3]) {
//#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=uv.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=uv_out complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv_src complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=2
#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=3
#pragma HLS ARRAY_PARTITION variable=left_derr complete dim=0
#pragma HLS ARRAY_PARTITION variable=derr complete dim=0
  int nz = 0;
  int n;
  int16_t tmp[8][16];
  uint8_t tmp_src[8][16], tmp_p[8][16], tmp_out[8][16];
#pragma HLS ARRAY_PARTITION variable=tmp_src complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_out complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_p complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp complete dim=0

  static const uint16_t VP8ScanUV[4 + 4] = {
    0 + 0 * 16,   4 + 0 * 16, 0 + 4 * 16,  4 + 4 * 16,    // U
    8 + 0 * 16,  12 + 0 * 16, 8 + 4 * 16, 12 + 4 * 16     // V
  };

  int i,j;
  for(n = 0; n < 8; n++){
#pragma HLS unroll
	  for(j = 0; j < 4; j++){
#pragma HLS unroll
		  for(i = 0; i < 4; i++){
#pragma HLS unroll
			  tmp_src[n][j * 4 + i] = uv_src[VP8ScanUV[n] + j * 16 + i];
			  tmp_p[n][j * 4 + i] = uv_p[VP8ScanUV[n] + j * 16 + i];
		  }
	  }
  }

  for (n = 0; n < 8; n++) {
#pragma HLS unroll
	  FTransform_C(tmp_src[n], tmp_p[n], tmp[n]);
  }

  CorrectDCValues(top_derr, left_derr, x, &uv, tmp, derr);


  for (n = 0; n < 8; n++) {
#pragma HLS unroll
    nz |= QuantizeBlock_C(tmp[n], uv_levels[n], &uv) << n;
  }

  for (n = 0; n < 8; n++) {
#pragma HLS unroll
	  ITransformOne(tmp_p[n], tmp[n], tmp_out[n]);
  }

  for(n = 0; n < 8; n++){
#pragma HLS unroll
	  for(j = 0; j < 4; j++){
#pragma HLS unroll
		  for(i = 0; i < 4; i++){
#pragma HLS unroll
			  uv_out[VP8ScanUV[n] + j * 16 + i] = tmp_out[n][j * 4 + i];
		  }
	  }
  }

  return (nz << 16);
}

typedef int64_t score_t;

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

static const uint16_t kWeightY[16] = {
  38, 32, 20, 9, 32, 28, 17, 7, 20, 17, 10, 4, 9, 7, 4, 2
};

#define FLATNESS_LIMIT_I16 10      // I16 mode
#define FLATNESS_PENALTY   140     // roughly ~1bit per block

int GetSSE(const uint8_t* a, const uint8_t* b,
                              int w, int h) {
  int count = 0;
  int y, x;
  for (y = 0; y < h; ++y) {
#pragma HLS unroll
    for (x = 0; x < w; ++x) {
#pragma HLS unroll
      const int diff = (int)a[x + y * w] - b[x + y * w];
      count += diff * diff;
    }
  }
  return count;
}

static int SSE16x16_C(const uint8_t* a, const uint8_t* b) {
//#pragma HLS INLINE off
  return GetSSE(a, b, 16, 16);
}

#define MULT_8B(a, b) (((a) * (b) + 128) >> 8)

static int TTransform(const uint8_t* in, const uint16_t* w) {
  int sum = 0;
  int tmp[16];
  int i;
  // horizontal pass
  for (i = 0; i < 4; ++i, in += 4) {
#pragma HLS unroll
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
#pragma HLS unroll
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
//#pragma HLS INLINE off
  int D = 0;

  uint8_t tmp_a[16][16], tmp_b[16][16];
#pragma HLS ARRAY_PARTITION variable=tmp_a complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_b complete dim=0

  const uint16_t VP8Scan[16] = {
    0 +  0 * 16,  4 +  0 * 16, 8 +  0 * 16, 12 +  0 * 16,
    0 +  4 * 16,  4 +  4 * 16, 8 +  4 * 16, 12 +  4 * 16,
    0 +  8 * 16,  4 +  8 * 16, 8 +  8 * 16, 12 +  8 * 16,
    0 + 12 * 16,  4 + 12 * 16, 8 + 12 * 16, 12 + 12 * 16,
  };

  int i,j,n;
  for(n = 0; n < 16; n++){
#pragma HLS unroll
	  for(j = 0; j < 4; j++){
#pragma HLS unroll
		  for(i = 0; i < 4; i++){
#pragma HLS unroll
			  tmp_a[n][j * 4 + i] = a[VP8Scan[n] + j * 16 + i];
			  tmp_b[n][j * 4 + i] = b[VP8Scan[n] + j * 16 + i];
		  }
	  }
  }


    for (i = 0; i < 16; i++) {
#pragma HLS unroll
      D += Disto4x4_C(tmp_a[i], tmp_b[i], w);
    }

  return D;

}

const uint16_t VP8FixedCostsI16[4] = { 663, 919, 872, 919 };

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

#define RD_DISTO_MULT      256  // distortion multiplier (equivalent of lambda)

static void SetRDScore(int lambda, VP8ModeScore* const rd) {
  rd->score = (rd->R + rd->H) * lambda + RD_DISTO_MULT * (rd->D + rd->SD);
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

static void CopyScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  = src->D;
  dst->SD = src->SD;
  dst->R  = src->R;
  dst->H  = src->H;
  dst->nz = src->nz;      // note that nz is not accumulated, but just copied.
  dst->score = src->score;
}

void PickBestIntra16(uint8_t Yin[16*16], uint8_t Yout[16*16], uint8_t YPred[4][16*16],
		VP8ModeScore* rd, VP8SegmentInfo* const dqm) {

//#pragma HLS pipeline
#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->y_dc_levels complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.q_ complete dim=1

  const int kNumBlocks = 16;
  //VP8SegmentInfo* const dqm = &it->enc_->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_i16_;
  const int tlambda = dqm->tlambda_;
  const uint8_t* const src = Yin;
  VP8ModeScore rd_tmp;
  VP8ModeScore* rd_cur = &rd_tmp;
  VP8ModeScore* rd_best = rd;
  int mode;
  uint8_t Yout_tmp[16*16];

  //rd->mode_i16 = -1;
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    uint8_t* const tmp_dst = Yout_tmp;  // scratch buffer
    rd_cur->mode_i16 = mode;

    // Reconstruct
    rd_cur->nz = ReconstructIntra16(YPred[mode], src, tmp_dst, rd_cur->y_ac_levels,
    		rd_cur->y_dc_levels, dqm->y1_, dqm->y2_);

    // Measure RD-score
    rd_cur->D = SSE16x16_C(src, tmp_dst);
    rd_cur->SD = MULT_8B(tlambda, Disto16x16_C(src, tmp_dst, kWeightY));
    rd_cur->H = VP8FixedCostsI16[mode];

	int64_t test_R = 0;
	int y, x;
	for (y = 1; y < 16; ++y) {
#pragma HLS unroll
	  for (x = 1; x < 16; ++x) {
#pragma HLS unroll
	    test_R += rd_cur->y_ac_levels[y][x] * rd_cur->y_ac_levels[y][x];
	  }
	}
	for (y = 0; y < 16; ++y) {
#pragma HLS unroll
	  test_R += rd_cur->y_dc_levels[y] * rd_cur->y_dc_levels[y];
	}
	rd_cur->R = test_R << 10;

    // Since we always examine Intra16 first, we can overwrite *rd directly.
    SetRDScore(lambda, rd_cur);

    if (rd_cur->score < rd_best->score) {
      rd_best->mode_i16	= rd_cur->mode_i16;
      CopyScore(rd_best, rd_cur);
	  for (y = 0; y < 16; ++y) {
#pragma HLS unroll
		for (x = 0; x < 16; ++x) {
#pragma HLS unroll
		  rd_best->y_ac_levels[y][x] = rd_cur->y_ac_levels[y][x];
		}
	  }
	  for (y = 0; y < 16; ++y) {
#pragma HLS unroll
		rd_best->y_dc_levels[y] = rd_cur->y_dc_levels[y];
	  }
	  for (y = 0; y < 256; ++y) {
#pragma HLS unroll
		Yout[y] = tmp_dst[y];
	  }
    }
  }

  SetRDScore(dqm->lambda_mode_, rd);   // finalize score for mode decision.

  // we have a blocky macroblock (only DCs are non-zero) with fairly high
  // distortion, record max delta so we can later adjust the minimal filtering
  // strength needed to smooth these blocks out.
  if ((rd->nz & 0x100ffff) == 0x1000000 && rd->D > dqm->min_disto_) {
    StoreMaxDelta(dqm, rd->y_dc_levels);
  }
}

#define MAX_COST ((score_t)0x7fffffffffffffLL)

static void InitScore(VP8ModeScore* const rd) {
  rd->D  = 0;
  rd->SD = 0;
  rd->R  = 0;
  rd->H  = 0;
  rd->nz = 0;
  rd->score = MAX_COST;
}

void picki4b(int i4, uint8_t y_left[16], uint8_t y_top_left, uint8_t y_top[20],
		uint8_t left[4], uint8_t* top_left, uint8_t top[4], uint8_t top_right[4]) {

	int i;

    switch(i4){
    case 0 :
    	*top_left = y_top_left;
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[4+i];
		}
		break;
    case 1 :
    	*top_left = y_top[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[i];
		}
		for (i = 0; i <4; ++i) {
#pragma HLS unroll
			top[i] = y_top[4+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[8+i];
		}
		break;
    case 2 :
    	*top_left = y_top[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[8+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[12+i];
		}
		break;
    case 3 :
    	*top_left = y_top[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[12+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[16+i];
		}
		break;
    case 4 :
    	*top_left = y_top_left;
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[4+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[4+i];
		}
		break;
    case 5 :
    	*top_left = y_top[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[4+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[4+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[8+i];
		}
		break;
    case 6 :
    	*top_left = y_top[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[4+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[8+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[12+i];
		}
		break;
    case 7 :
    	*top_left = y_top[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[4+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[12+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[16+i];
		}
		break;
    case 8 :
    	*top_left = y_top_left;
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[8+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[4+i];
		}
		break;
    case 9 :
    	*top_left = y_top[3];
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[8+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[4+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[8+i];
		}
		break;
    case 10 :
    	*top_left = y_top[7];
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[8+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[8+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[12+i];
		}
		break;
    case 11 :
    	*top_left = y_top[11];
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[8+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[12+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[16+i];
		}
		break;
    case 12 :
    	*top_left = y_top_left;
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[12+i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[4+i];
		}
		break;
    case 13 :
    	*top_left = y_top[3];
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[12+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[4+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[8+i];
		}
		break;
    case 14 :
    	*top_left = y_top[7];
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[12+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[8+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[12+i];
		}
		break;
    case 15 :
    	*top_left = y_top[11];
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[12+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top[i] = y_top[12+i];
		}
    	for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			top_right[i] = y_top[16+i];
		}
		break;
    }
}

const uint16_t VP8FixedCostsI4[NUM_BMODES] =
   {   40, 1151, 1723, 1874, 2103, 2019, 1628, 1777, 2226, 2137 };

static int SSE4x4_C(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 4, 4);
}

static void AddScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  += src->D;
  dst->SD += src->SD;
  dst->R  += src->R;
  dst->H  += src->H;
  dst->nz |= src->nz;     // here, new nz bits are accumulated.
  dst->score += src->score;
}

int VP8IteratorRotateI4(uint8_t y_left[16], uint8_t* y_top_left, uint8_t y_top[20], int* i4_,
                        uint8_t yuv_out[16][16]) {
  const uint8_t* const blk = yuv_out[*i4_];
  int i;

  switch(*i4_){
  case 0 :
	  *y_top_left = y_left[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[i] = blk[12+i];
		}
		break;
  case 1 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[4+i] = blk[12+i];
		}
		break;
  case 2 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[8+i] = blk[12+i];
		}
		break;
  case 3 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[12+i] = blk[12+i];
		}
		break;
  case 4 :
	  *y_top_left = y_left[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[4+i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[i] = blk[12+i];
		}
		break;
  case 5 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[4+i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[4+i] = blk[12+i];
		}
		break;
  case 6 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[4+i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[8+i] = blk[12+i];
		}
		break;
  case 7 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[12+i] = blk[12+i];
		}
		break;
  case 8 :
	  *y_top_left = y_left[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[8+i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[i] = blk[12+i];
		}
		break;
  case 9 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[8+i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[4+i] = blk[12+i];
		}
		break;
  case 10 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[8+i] = blk[3+4*i];
		}
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[8+i] = blk[12+i];
		}
		break;
  case 11 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_top[12+i] = blk[12+i];
		}
		break;
  case 12 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[12+i] = blk[3+4*i];
		}
		break;
  case 13 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[12+i] = blk[3+4*i];
		}
		break;
  case 14 :
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			y_left[12+i] = blk[3+4*i];
		}
		break;
  case 15 :
	  	return 0;
  }
  // move pointers to next sub-block
  ++(*i4_);
  return 1;
}


int PickBestIntra4(VP8SegmentInfo* const dqm, uint8_t Yin[16*16], uint8_t Yout[16*16],
		VP8ModeScore* const rd, uint8_t y_left[16], uint8_t y_top_left, uint8_t y_top[20], ap_uint<1>* mbtype) {
#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y_left complete dim=1
#pragma HLS ARRAY_PARTITION variable=y_top complete dim=1
  const int lambda = dqm->lambda_i4_;
  const int tlambda = dqm->tlambda_;
  const uint8_t* const src0 = Yin;
  uint8_t best_blocks[16][16];
  VP8ModeScore rd_best;
  uint8_t i_left[16], i_top_left, i_top[20];
  int i, j, n;
  const uint16_t VP8Scan[16] = {  // Luma
    0 +  0 * 16,  4 +  0 * 16, 8 +  0 * 16, 12 +  0 * 16,
    0 +  4 * 16,  4 +  4 * 16, 8 +  4 * 16, 12 +  4 * 16,
    0 +  8 * 16,  4 +  8 * 16, 8 +  8 * 16, 12 +  8 * 16,
    0 + 12 * 16,  4 + 12 * 16, 8 + 12 * 16, 12 + 12 * 16,
  };

#pragma HLS ARRAY_PARTITION variable=best_blocks complete dim=0
#pragma HLS ARRAY_PARTITION variable=VP8Scan complete dim=1
#pragma HLS ARRAY_PARTITION variable=i_left complete dim=1
#pragma HLS ARRAY_PARTITION variable=i_top complete dim=1
#pragma HLS ARRAY_PARTITION variable=rd_best.y_ac_levels complete dim=0

  i_top_left = y_top_left;
  for (i = 0; i < 16; ++i) {
#pragma HLS unroll
	  i_left[i] = y_left[i];
  }
  for (i = 0; i < 20; ++i) {
#pragma HLS unroll
	  i_top[i] = y_top[i];
  }

  InitScore(&rd_best);
  rd_best.H = 211;  // '211' is the value of VP8BitCost(0, 145)
  SetRDScore(dqm->lambda_mode_, &rd_best);

  int i4_ = 0;

  do {
    const int kNumBlocks = 1;
    VP8ModeScore rd_i4;
    int mode;
    int best_mode = -1;
    uint8_t src[16][16];
    const uint16_t* const mode_costs = VP8FixedCostsI4;
    uint8_t* best_block = best_blocks[i4_];
    uint8_t tmp_pred[10][16];    // scratch buffer.
    uint8_t left[4], top_left, top[4], top_right[4];

#pragma HLS ARRAY_PARTITION variable=src complete dim=0
#pragma HLS ARRAY_PARTITION variable=VP8FixedCostsI4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_pred complete dim=0
#pragma HLS ARRAY_PARTITION variable=left complete dim=1
#pragma HLS ARRAY_PARTITION variable=top complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_right complete dim=1

    for(n = 0; n < 16; n++){
#pragma HLS unroll
  	  for(j = 0; j < 4; j++){
#pragma HLS unroll
  		  for(i = 0; i < 4; i++){
#pragma HLS unroll
  			  src[n][j * 4 + i] = src0[VP8Scan[n] + j * 16 + i];
  		  }
  	  }
    }

    InitScore(&rd_i4);

    picki4b(i4_, i_left, i_top_left, i_top, left, &top_left, top, top_right);

    Intra4Preds_C(tmp_pred, left, top_left, top, top_right);

    for (mode = 0; mode < NUM_BMODES; ++mode) {
      VP8ModeScore rd_tmp;
      int16_t tmp_levels[16];
      uint8_t tmp_dst[10][16];
#pragma HLS ARRAY_PARTITION variable=tmp_dst complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_levels complete dim=1
      // Reconstruct
      rd_tmp.nz =
          ReconstructIntra4(tmp_levels, tmp_pred[mode], src[i4_], tmp_dst[mode], dqm->y1_) << i4_;

      // Compute RD-score
      rd_tmp.D = SSE4x4_C(src[i4_], tmp_dst[mode]);
      rd_tmp.SD = MULT_8B(tlambda, Disto4x4_C(src[i4_], tmp_dst[mode], kWeightY));
      rd_tmp.H = mode_costs[mode];

	  int64_t test_R = 0;
	  int y;
	  for (y = 0; y < 16; ++y) {
#pragma HLS unroll
		test_R += tmp_levels[y] * tmp_levels[y];
	  }
	  rd_tmp.R = test_R << 10;

      SetRDScore(lambda, &rd_tmp);

      if (rd_tmp.score < rd_i4.score) {
        CopyScore(&rd_i4, &rd_tmp);
        best_mode = mode;
  	    for (y = 0; y < 16; ++y) {
#pragma HLS unroll
  		  rd_best.y_ac_levels[i4_][y] = tmp_levels[y];
  	    }
  	    for (y = 0; y < 16; ++y) {
#pragma HLS unroll
  		  best_block[y] = tmp_dst[mode][y];
  	    }
      }
    }
    SetRDScore(dqm->lambda_mode_, &rd_i4);
    AddScore(&rd_best, &rd_i4);
    if (rd_best.score >= rd->score) {
	  *mbtype = 1;
      return 0;
    }
    rd->modes_i4[i4_] = best_mode;
  } while (VP8IteratorRotateI4(i_left, &i_top_left, i_top, &i4_, best_blocks));

  // finalize state
  CopyScore(rd, &rd_best);

  for(n = 0; n < 16; n++){
#pragma HLS unroll
	  for(j = 0; j < 4; j++){
#pragma HLS unroll
		  for(i = 0; i < 4; i++){
#pragma HLS unroll
			  Yout[VP8Scan[n] + j * 16 + i] = best_blocks[n][j * 4 + i];
		  }
	  }
  }

    for (j = 0; j < 16; ++j) {
#pragma HLS unroll
        for (i = 0; i < 16; ++i) {
#pragma HLS unroll
        	rd->y_ac_levels[j][i] = rd_best.y_ac_levels[j][i];
        }
    }
    *mbtype = 0;
  return 1;   // select intra4x4 over intra16x16
}

static int SSE16x8_C(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 8);
}

const uint16_t VP8FixedCostsUV[4] = { 302, 984, 439, 642 };

static void StoreDiffusionErrors(DError top_derr[1024], DError left_derr, int x,
                                 const VP8ModeScore* const rd) {
  int ch;
  for (ch = 0; ch <= 1; ++ch) {
#pragma HLS unroll
    int8_t* const top = top_derr[x][ch];
    int8_t* const left = left_derr[ch];
    left[0] = rd->derr[ch][0];            // restore err1
    left[1] = 3 * rd->derr[ch][2] >> 2;   //     ... 3/4th of err3
    top[0]  = rd->derr[ch][1];            //     ... err2
    top[1]  = rd->derr[ch][2] - left[1];  //     ... 1/4th of err3.
  }
}

void PickBestUV(VP8SegmentInfo* const dqm, uint8_t UVin[8*16], uint8_t UVPred[8][8*8],
		uint8_t UVout[8*16], VP8ModeScore* const rd, int x) {
#pragma HLS ARRAY_PARTITION variable=UVout complete dim=1
#pragma HLS ARRAY_PARTITION variable=UVin complete dim=1
#pragma HLS ARRAY_PARTITION variable=UVPred complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->uv_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.q_ complete dim=1
  const int kNumBlocks = 8;
  const int lambda = dqm->lambda_uv_;
  const uint8_t* const src = UVin;
  uint8_t tmp_dst[8*16];  // scratch buffer
  uint8_t* dst = UVout;
  VP8ModeScore rd_best;
  uint8_t tmp_p[4][8*16];
  DError top_derr[1024] = {0}, left_derr = {0};
  int mode;
  int i, j, k;
#pragma HLS ARRAY_PARTITION variable=tmp_dst complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_p complete dim=0
#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=2
#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=3
#pragma HLS ARRAY_PARTITION variable=left_derr complete dim=0

  if(x == 0){
	  for (i = 0; i < 2; ++i) {
#pragma HLS unroll
		for (j = 0; j < 2; j = j + 16) {
#pragma HLS unroll
			left_derr[i][j] = 0;
		}
	  }
  }

  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
	for (j = 0; j < 128; j = j + 16) {
#pragma HLS unroll
		for (k = 0; k < 8; ++k) {
#pragma HLS unroll
			tmp_p[i][j+k] = UVPred[i][(j>>1)+k];
		}
	}
	for (j = 8; j < 128; j = j + 16) {
#pragma HLS unroll
		for (k = 0; k < 8; ++k) {
#pragma HLS unroll
			tmp_p[i][j+k] = UVPred[i+4][((j-8)>>1)+k];
		}
	}
  }
  rd->mode_uv = -1;
  InitScore(&rd_best);
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    VP8ModeScore rd_uv;
#pragma HLS ARRAY_PARTITION variable=rd_uv.uv_levels complete dim=0

    // Reconstruct
    rd_uv.nz = ReconstructUV(rd_uv.uv_levels, tmp_p[mode], src, tmp_dst,
    		dqm->uv_, top_derr, left_derr, x, rd_uv.derr);

    // Compute RD-score
    rd_uv.D  = SSE16x8_C(src, tmp_dst);
    rd_uv.SD = 0;    // not calling TDisto here: it tends to flatten areas.
    rd_uv.H  = VP8FixedCostsUV[mode];

	int64_t test_R = 0;
	int x, y;
	for (y = 0; y < 8; ++y) {
#pragma HLS unroll
	  for (x = 0; x < 16; ++x) {
#pragma HLS unroll
	    test_R += rd_uv.uv_levels[y][x] * rd_uv.uv_levels[y][x];
	  }
	}
	rd_uv.R = test_R << 10;

    SetRDScore(lambda, &rd_uv);

    if (rd_uv.score < rd_best.score) {
      CopyScore(&rd_best, &rd_uv);
      rd->mode_uv = mode;
      for (j = 0; j < 8; ++j) {
#pragma HLS unroll
          for (i = 0; i < 16; ++i) {
#pragma HLS unroll
        	  rd->uv_levels[j][i] = rd_uv.uv_levels[j][i];
          }
      }
      for (j = 0; j < 2; ++j) {
#pragma HLS unroll
          for (i = 0; i < 3; ++i) {
#pragma HLS unroll
        	  rd->derr[j][i] = rd_uv.derr[j][i];
          }
      }
      for (j = 0; j < 128; ++j) {
#pragma HLS unroll
    	  dst[j] = tmp_dst[j];
      }
    }
  }

  AddScore(rd, &rd_best);
  // store diffusion errors for next block
  StoreDiffusionErrors(top_derr, left_derr, x, rd);
}

void VP8Decimate(uint8_t Yin[16*16], uint8_t Yout16[16*16], uint8_t Yout4[16*16],
		VP8SegmentInfo* const dqm, uint8_t UVin[8*16], uint8_t UVout[8*16], int* is_skipped,
		uint8_t left_y[16], uint8_t top_y[20], uint8_t top_left_y, ap_uint<1>* mbtype,
		uint8_t left_u[8], uint8_t top_u[8], uint8_t top_left_u,uint8_t left_v[8],
		uint8_t top_v[8], uint8_t top_left_v, int x, int y, VP8ModeScore* const rd) {

  uint8_t YPred[4][16*16];
  uint8_t UVPred[8][8*8];

#pragma HLS ARRAY_PARTITION variable=UVout complete dim=1
#pragma HLS ARRAY_PARTITION variable=UVin complete dim=1
#pragma HLS ARRAY_PARTITION variable=UVPred complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->uv_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->uv_.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yout16 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yout4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->y_dc_levels complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y1_.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=dqm->y2_.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_y complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_y complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_u complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_u complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_v complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_v complete dim=1

  InitScore(rd);

  // We can perform predictions for Luma16x16 and Chroma8x8 already.
  // Luma4x4 predictions needs to be done as-we-go.

  Intra16Preds_C( YPred, left_y, top_y, top_left_y, x, y);

  IntraChromaPreds_C(UVPred, left_u, top_u, top_left_u, left_v, top_v, top_left_v, x,  y);

  PickBestIntra16(Yin, Yout16, YPred, rd, dqm);
  PickBestIntra4(dqm, Yin, Yout4, rd, left_y, top_left_y, top_y, mbtype);
  PickBestUV(dqm, UVin, UVPred, UVout, rd, x);

  *is_skipped = (rd->nz == 0);
}

typedef double LFStats_My[MAX_LF_LEVELS];

#define VP8_SSIM_KERNEL 3

// hat-shaped filter. Sum of coefficients is equal to 16.
static const uint32_t kWeight[2 * VP8_SSIM_KERNEL + 1] = {
  1, 2, 3, 4, 3, 2, 1
};

typedef struct {
  uint32_t w;              // sum(w_i) : sum of weights
  uint32_t xm, ym;         // sum(w_i * x_i), sum(w_i * y_i)
  uint32_t xxm, xym, yym;  // sum(w_i * x_i * x_i), etc.
} VP8DistoStats;

 double SSIMCalculation(
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
    return r;
  }
  return 1.;   // area is too dark to contribute meaningfully
}

double VP8SSIMFromStatsClipped(const VP8DistoStats* const stats) {
  return SSIMCalculation(stats, stats->w);
}

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

static double GetMBSSIM(uint8_t Yin[16*16], uint8_t Yout4[16*16],
		uint8_t UVin[8*16], uint8_t UVout[8*16]) {
  int x, y;
  double sum = 0.;

  // compute SSIM in a 10 x 10 window
  for (y = VP8_SSIM_KERNEL; y < 16 - VP8_SSIM_KERNEL; y++) {
    for (x = VP8_SSIM_KERNEL; x < 16 - VP8_SSIM_KERNEL; x++) {
      sum += SSIMGetClipped_C(Yin, 16, Yout4, 16, x, y, 16, 16);
    }
  }
  for (x = 1; x < 7; x++) {
    for (y = 1; y < 7; y++) {
      sum += SSIMGetClipped_C(UVin, 16, UVout, 16, x, y, 8, 8);
      sum += SSIMGetClipped_C(UVin + 8, 16, UVout + 8, 16, x, y, 8, 8);
    }
  }
  return sum;
}

int NeedsFilter2_C(const uint8_t* p,
                                      int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step];
  const int p0 = p[-step], q0 = p[0];
  const int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * abs(p0 - q0) + abs(p1 - q1)) > t) return 0;
  return abs(p3 - p2) <= it && abs(p2 - p1) <= it &&
		 abs(p1 - p0) <= it && abs(q3 - q2) <= it &&
		 abs(q2 - q1) <= it && abs(q1 - q0) <= it;
}

void DoFilter2_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int tmp1 = p1 - q1;
  const int a = 3 * (q0 - p0) + ((tmp1 < -128) ? -128 : (tmp1 > 127) ? 127 : tmp1);// in [-893,892]
  const int tmp2 = (a + 4) >> 3;
  const int tmp3 = (a + 3) >> 3;
  const int a1 = (tmp2 < -16) ? -16 : (tmp2 > 15) ? 15 : tmp2;// in [-16,15]
  const int a2 = (tmp3 < -16) ? -16 : (tmp3 > 15) ? 15 : tmp3;
  const int tmp4 = p0 + a2;
  const int tmp5 = q0 - a1;
  p[-step] = (tmp4 < -128) ? -128 : (tmp4 > 127) ? 127 : tmp4;
  p[    0] = (tmp5 < -128) ? -128 : (tmp5 > 127) ? 127 : tmp5;
}

void DoFilter4_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0);
  const int tmp2 = (a + 4) >> 3;
  const int tmp3 = (a + 3) >> 3;
  const int a1 = (tmp2 < -16) ? -16 : (tmp2 > 15) ? 15 : tmp2;// in [-16,15]
  const int a2 = (tmp3 < -16) ? -16 : (tmp3 > 15) ? 15 : tmp3;
  const int a3 = (a1 + 1) >> 1;
  const int tmp4 = p1 + a3;
  const int tmp5 = p0 + a2;
  const int tmp6 = q0 - a1;
  const int tmp7 = q1 - a3;
  p[-2*step] = (tmp4 < -128) ? -128 : (tmp4 > 127) ? 127 : tmp4;
  p[-  step] = (tmp5 < -128) ? -128 : (tmp5 > 127) ? 127 : tmp5;
  p[      0] = (tmp6 < -128) ? -128 : (tmp6 > 127) ? 127 : tmp6;
  p[   step] = (tmp7 < -128) ? -128 : (tmp7 > 127) ? 127 : tmp7;
}

int Hev(const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (abs(p1 - p0) > thresh) || (abs(q1 - q0) > thresh);
}

void FilterLoop24_C(uint8_t* p, int hstride, int vstride, int size,
		int thresh, int ithresh, int hev_thresh) {
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

static void DoFilter(uint8_t Yout4[16*16], uint8_t UVin[8*16], int level) {
  const int ilevel = (level < 1) ? 1 : level;
  const int limit = 2 * level + ilevel;
  uint8_t Y[16*16],UV[8*16];

  uint8_t* const y_dst = Y;
  uint8_t* const u_dst = UV;
  uint8_t* const v_dst = UV + 8;

  // copy current block to yuv_out2_
  int i;
  for(i = 0; i < 256; i++){
	  Y[i] = Yout4[i];
  }
  for(i = 0; i < 128; i++){
	  UV[i] = UVin[i];
  }

  const int hev_thresh = (level >= 40) ? 2 : (level >= 15) ? 1 : 0;
  HFilter16i_C(y_dst, 16, limit, ilevel, hev_thresh);
  HFilter8i_C(u_dst, v_dst, 16, limit, ilevel, hev_thresh);
  VFilter16i_C(y_dst, 16, limit, ilevel, hev_thresh);
  VFilter8i_C(u_dst, v_dst, 16, limit, ilevel, hev_thresh);

}

void VP8StoreFilterStats(VP8SegmentInfo* const dqm, LFStats_My lf_stats,
		uint8_t Yin[16*16], uint8_t Yout16[16*16], uint8_t Yout4[16*16],
		uint8_t UVin[8*16], uint8_t UVout[8*16], ap_uint<1> mbtype, ap_uint<1> skip) {
  int d;
  const int level0 = dqm->fstrength_;

  // explore +/-quant range of values around level0
  const int delta_min = -dqm->quant_;
  const int delta_max = dqm->quant_;
  const int step_size = (delta_max - delta_min >= 4) ? 4 : 1;

  // NOTE: Currently we are applying filter only across the sublock edges
  // There are two reasons for that.
  // 1. Applying filter on macro block edges will change the pixels in
  // the left and top macro blocks. That will be hard to restore
  // 2. Macro Blocks on the bottom and right are not yet compressed. So we
  // cannot apply filter on the right and bottom macro block edges.

  if (mbtype == 1 && skip) return;

  // Always try filter level  zero
  lf_stats[0] += GetMBSSIM(Yin, Yout4, UVin, UVout);

  for (d = delta_min; d <= delta_max; d += step_size) {
    const int level = level0 + d;
    if (level <= 0 || level >= MAX_LF_LEVELS) {
      continue;
    }
    DoFilter(Yout4, UVin, level);
    lf_stats[level] += GetMBSSIM(Yin, Yout4, UVin, UVout);
  }
}

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
		uint8_t top_left_y = 127;
		uint8_t left_u[8];
		uint8_t top_u[8];
		uint8_t top_left_u = 127;
		uint8_t left_v[8];
		uint8_t top_v[8];
		uint8_t top_left_v = 127;
		int mbtype = 1;
		int is_skipped = 0;
		int x = 0;
		int y = 0;
		int mb_w;
		int mb_h;
		int count_down;
		LFStats_My lf_stats;
		} DATA;

void VP8IteratorSaveBoundary(DATA* data_it) {
  const uint8_t* const ysrc = data_it->mbtype ? data_it->Yout16 : data_it->Yout4;
  const uint8_t* const uvsrc = data_it->UVout;
  int i;
  uint8_t top_y_tmp1[16];
  uint8_t top_y_tmp2[16];
  uint8_t mem_top_y[1024][16];
  uint8_t mem_top_u[1024][8];
  uint8_t mem_top_v[1024][8];

  if (data_it->x < data_it->mb_w - 1) {   // left
    for (i = 0; i < 16; ++i) {
    	data_it->left_y[i] = ysrc[15 + i * 16];
    }
    for (i = 0; i < 8; ++i) {
    	data_it->left_u[i] = uvsrc[7 + i * 16];
    	data_it->left_v[i] = uvsrc[15 + i * 16];
    }
    // top-left (before 'top'!)
    data_it->top_left_y = data_it->top_y[15];
    data_it->top_left_u = data_it->top_u[7];
    data_it->top_left_v = data_it->top_v[7];
  }
  else{
	for (i = 0; i < 16; ++i) {
		data_it->left_y[i] = 129;
	}
	for (i = 0; i < 8; ++i) {
		data_it->left_u[i] = 129;
		data_it->left_v[i] = 129;
	}
	// top-left (before 'top'!)
	data_it->top_left_y = 129;
	data_it->top_left_u = 129;
	data_it->top_left_v = 129;
  }

  if (data_it->y < data_it->mb_h - 1) {  // top mem
	for (i = 0; i < 16; ++i) {
		mem_top_y[data_it->x][i] = ysrc[15 * 16 + i];
	}
	for (i = 0; i < 8; ++i) {
	  	mem_top_u[data_it->x][i] = uvsrc[7 * 16 + i];
	  	mem_top_v[data_it->x][i] = uvsrc[7 * 16 + i + 8];
	}
  }

  int tmp = (data_it->x < data_it->mb_w - 1) ? data_it->x + 1 : 0;
  int tmp_p = (data_it->x + 1 < data_it->mb_w - 1) ? data_it->x + 2 : 0;

	for (i = 0; i < 16; ++i) {
		top_y_tmp1[i] = mem_top_y[tmp_p][i];
		top_y_tmp2[i] = top_y_tmp1[i];
	}

  if (data_it->y == 0) {  // top
	for (i = 0; i < 20; ++i) {
		data_it->top_y[i] = 127;
	}
	for (i = 0; i < 8; ++i) {
	  	data_it->top_u[i] = 127;
	  	data_it->top_v[i] = 127;
	}
  }
  else {  // top
	for (i = 0; i < 16; ++i) {
		data_it->top_y[i] = top_y_tmp2[i];
	}
	for (i = 0; i < 8; ++i) {
		data_it->top_u[i] = mem_top_u[tmp][i];
		data_it->top_v[i] = mem_top_v[tmp][i];
	}
	if (data_it->x < data_it->mb_w - 1) {
		for (i = 0; i < 4; ++i) {
			data_it->top_y[16 + i] = top_y_tmp1[i];
		}
	} else {    // else, replicate the last valid pixel four times
		for (i = 16; i < 16 + 4; ++i) {
			data_it->top_y[16 + i] = data_it->top_y[15];
		}
	}
  }
}


int VP8IteratorNext(DATA* data_it) {
  if (++data_it->x == data_it->mb_w) {
	  data_it->x = 0;
  }
  return (0 < --data_it->count_down);
}
