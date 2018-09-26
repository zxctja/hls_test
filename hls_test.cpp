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

void Intra16Preds_C(uint8_t YPred_DC[16*16],uint8_t YPred_VE[16*16],uint8_t YPred_HE[16*16],uint8_t YPred_TM[16*16],
                           uint8_t left_y[16], uint8_t top_y[16], uint8_t top_left_y, int x, int y) {
#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=top_y complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_y complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_DC complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_VE complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_HE complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_TM complete dim=1
  DCMode(YPred_DC, left_y, top_y, 16, 16, 5);
  VerticalPred(YPred_VE, top_y, 16);
  HorizontalPred(YPred_HE, left_y, 16);
  TrueMotion(YPred_TM, left_y, top_y, top_left_y, 16, x, y);
}

void IntraChromaPreds_C(
		uint8_t UPred_DC[8*8],uint8_t UPred_VE[8*8],uint8_t UPred_HE[8*8],uint8_t UPred_TM[8*8],
		uint8_t VPred_DC[8*8],uint8_t VPred_VE[8*8],uint8_t VPred_HE[8*8],uint8_t VPred_TM[8*8],
        uint8_t left_u[8], uint8_t top_u[8], uint8_t top_left_u,
		uint8_t left_v[8], uint8_t top_v[8], uint8_t top_left_v,
		int x, int y) {
#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=top_u complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_u complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_v complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_v complete dim=1
#pragma HLS ARRAY_PARTITION variable=UPred_DC complete dim=1
#pragma HLS ARRAY_PARTITION variable=UPred_VE complete dim=1
#pragma HLS ARRAY_PARTITION variable=UPred_HE complete dim=1
#pragma HLS ARRAY_PARTITION variable=UPred_TM complete dim=1
#pragma HLS ARRAY_PARTITION variable=VPred_DC complete dim=1
#pragma HLS ARRAY_PARTITION variable=VPred_VE complete dim=1
#pragma HLS ARRAY_PARTITION variable=VPred_HE complete dim=1
#pragma HLS ARRAY_PARTITION variable=VPred_TM complete dim=1
  // U block
  DCMode(UPred_DC, left_u, top_u, 8, 8, 4);
  VerticalPred(UPred_VE, top_u, 8);
  HorizontalPred(UPred_HE, left_u, 8);
  TrueMotion(UPred_TM, left_u, top_u, top_left_u, 8, x, y);
  // V block
  DCMode(VPred_DC, left_v, top_v, 8, 8, 4);
  VerticalPred(VPred_VE, top_v, 8);
  HorizontalPred(VPred_HE, left_v, 8);
  TrueMotion(VPred_TM, left_v, top_v, top_left_v, 8, x, y);
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
		uint8_t Pred_DC4[16], uint8_t Pred_TM4[16], uint8_t Pred_VE4[16], uint8_t Pred_HE4[16], uint8_t Pred_RD4[16],
		uint8_t Pred_VR4[16], uint8_t Pred_LD4[16], uint8_t Pred_VL4[16], uint8_t Pred_HD4[16], uint8_t Pred_HU4[16],
		uint8_t left[4], uint8_t top_left, uint8_t top[4], uint8_t top_right[4]) {
#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=left complete dim=1
#pragma HLS ARRAY_PARTITION variable=top complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_right complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_DC4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_TM4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_VE4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_HE4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_RD4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_VR4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_LD4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_VL4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_HD4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=Pred_HU4 complete dim=1
  DC4(Pred_DC4, top, left);
  TM4(Pred_TM4, top, left, top_left);
  VE4(Pred_VE4, top_left, top, top_right);
  HE4(Pred_HE4, left, top_left);
  RD4(Pred_RD4, left, top_left, top);
  VR4(Pred_VR4, left, top_left, top);
  LD4(Pred_LD4, top, top_right);
  VL4(Pred_VL4, top, top_right);
  HD4(Pred_HD4, left, top_left, top);
  HU4(Pred_HU4, top);
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

uint8_t clip_8b(int v) {
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
#pragma HLS PIPELINE
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
#pragma HLS ARRAY_PARTITION variable=tmp_src complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_dc complete dim=1
#pragma HLS ARRAY_PARTITION variable=dc_tmp complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_out complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_pred complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp complete dim=0

  const uint16_t VP8Scan[16] = {  // Luma
    0 +  0 * 16,  4 +  0 * 16, 8 +  0 * 16, 12 +  0 * 16,
    0 +  4 * 16,  4 +  4 * 16, 8 +  4 * 16, 12 +  4 * 16,
    0 +  8 * 16,  4 +  8 * 16, 8 +  8 * 16, 12 +  8 * 16,
    0 + 12 * 16,  4 + 12 * 16, 8 + 12 * 16, 12 + 12 * 16,
  };

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
		const uint8_t const y_src[16], uint8_t y_out[16], VP8Matrix y1) {
#pragma HLS PIPELINE
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

int ReconstructUV(int16_t uv_levels[8][16],const uint8_t uv_p[8*16],
		const uint8_t uv_src[8*16], uint8_t uv_out[8*16], VP8Matrix uv) {
#pragma HLS PIPELINE
#pragma HLS ARRAY_PARTITION variable=uv.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv.q_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=uv_out complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv_src complete dim=1
#pragma HLS ARRAY_PARTITION variable=uv_p complete dim=1
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

  //CorrectDCValues(it, &dqm->uv_, tmp, rd);


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

#define BIT(nz, n) (!!((nz) & (1 << (n))))

void VP8IteratorNzToBytes(const int tnz, const int lnz, int top_nz[9], int left_nz[9]) {
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

typedef uint32_t proba_t;   // 16b + 16b
typedef uint8_t ProbaArray[NUM_CTX][NUM_PROBAS];
typedef proba_t StatsArray[NUM_CTX][NUM_PROBAS];
typedef uint16_t CostArrayMap[16][NUM_CTX][MAX_VARIABLE_LEVEL + 1];
typedef const uint16_t (*CostArrayPtr)[NUM_CTX][MAX_VARIABLE_LEVEL + 1];   // for easy casting

static int SetResidualCoeffs_C(const int16_t const coeffs[16]) {
#pragma HLS INLINE off
  int n, last;
  last = -1;
  for (n = 15; n >= 0; --n) {
#pragma HLS unroll
    if (coeffs[n]) {
      last = n;
      break;
    }
  }
  return last;
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


static int VP8BitCost(int bit, uint8_t proba) {
#pragma HLS INLINE off
  return !bit ? VP8EntropyCost[proba] : VP8EntropyCost[255 - proba];
}

const uint16_t VP8LevelFixedCosts[MAX_LEVEL + 1] = {
     0,  256,  256,  256,  256,  432,  618,  630,
   731,  640,  640,  828,  901,  948, 1021, 1101,
  1174, 1221, 1294, 1042, 1085, 1115, 1158, 1202,
  1245, 1275, 1318, 1337, 1380, 1410, 1453, 1497,
  1540, 1570, 1613, 1280, 1295, 1317, 1332, 1358,
  1373, 1395, 1410, 1454, 1469, 1491, 1506, 1532,
  1547, 1569, 1584, 1601, 1616, 1638, 1653, 1679,
  1694, 1716, 1731, 1775, 1790, 1812, 1827, 1853,
  1868, 1890, 1905, 1727, 1733, 1742, 1748, 1759,
  1765, 1774, 1780, 1800, 1806, 1815, 1821, 1832,
  1838, 1847, 1853, 1878, 1884, 1893, 1899, 1910,
  1916, 1925, 1931, 1951, 1957, 1966, 1972, 1983,
  1989, 1998, 2004, 2027, 2033, 2042, 2048, 2059,
  2065, 2074, 2080, 2100, 2106, 2115, 2121, 2132,
  2138, 2147, 2153, 2178, 2184, 2193, 2199, 2210,
  2216, 2225, 2231, 2251, 2257, 2266, 2272, 2283,
  2289, 2298, 2304, 2168, 2174, 2183, 2189, 2200,
  2206, 2215, 2221, 2241, 2247, 2256, 2262, 2273,
  2279, 2288, 2294, 2319, 2325, 2334, 2340, 2351,
  2357, 2366, 2372, 2392, 2398, 2407, 2413, 2424,
  2430, 2439, 2445, 2468, 2474, 2483, 2489, 2500,
  2506, 2515, 2521, 2541, 2547, 2556, 2562, 2573,
  2579, 2588, 2594, 2619, 2625, 2634, 2640, 2651,
  2657, 2666, 2672, 2692, 2698, 2707, 2713, 2724,
  2730, 2739, 2745, 2540, 2546, 2555, 2561, 2572,
  2578, 2587, 2593, 2613, 2619, 2628, 2634, 2645,
  2651, 2660, 2666, 2691, 2697, 2706, 2712, 2723,
  2729, 2738, 2744, 2764, 2770, 2779, 2785, 2796,
  2802, 2811, 2817, 2840, 2846, 2855, 2861, 2872,
  2878, 2887, 2893, 2913, 2919, 2928, 2934, 2945,
  2951, 2960, 2966, 2991, 2997, 3006, 3012, 3023,
  3029, 3038, 3044, 3064, 3070, 3079, 3085, 3096,
  3102, 3111, 3117, 2981, 2987, 2996, 3002, 3013,
  3019, 3028, 3034, 3054, 3060, 3069, 3075, 3086,
  3092, 3101, 3107, 3132, 3138, 3147, 3153, 3164,
  3170, 3179, 3185, 3205, 3211, 3220, 3226, 3237,
  3243, 3252, 3258, 3281, 3287, 3296, 3302, 3313,
  3319, 3328, 3334, 3354, 3360, 3369, 3375, 3386,
  3392, 3401, 3407, 3432, 3438, 3447, 3453, 3464,
  3470, 3479, 3485, 3505, 3511, 3520, 3526, 3537,
  3543, 3552, 3558, 2816, 2822, 2831, 2837, 2848,
  2854, 2863, 2869, 2889, 2895, 2904, 2910, 2921,
  2927, 2936, 2942, 2967, 2973, 2982, 2988, 2999,
  3005, 3014, 3020, 3040, 3046, 3055, 3061, 3072,
  3078, 3087, 3093, 3116, 3122, 3131, 3137, 3148,
  3154, 3163, 3169, 3189, 3195, 3204, 3210, 3221,
  3227, 3236, 3242, 3267, 3273, 3282, 3288, 3299,
  3305, 3314, 3320, 3340, 3346, 3355, 3361, 3372,
  3378, 3387, 3393, 3257, 3263, 3272, 3278, 3289,
  3295, 3304, 3310, 3330, 3336, 3345, 3351, 3362,
  3368, 3377, 3383, 3408, 3414, 3423, 3429, 3440,
  3446, 3455, 3461, 3481, 3487, 3496, 3502, 3513,
  3519, 3528, 3534, 3557, 3563, 3572, 3578, 3589,
  3595, 3604, 3610, 3630, 3636, 3645, 3651, 3662,
  3668, 3677, 3683, 3708, 3714, 3723, 3729, 3740,
  3746, 3755, 3761, 3781, 3787, 3796, 3802, 3813,
  3819, 3828, 3834, 3629, 3635, 3644, 3650, 3661,
  3667, 3676, 3682, 3702, 3708, 3717, 3723, 3734,
  3740, 3749, 3755, 3780, 3786, 3795, 3801, 3812,
  3818, 3827, 3833, 3853, 3859, 3868, 3874, 3885,
  3891, 3900, 3906, 3929, 3935, 3944, 3950, 3961,
  3967, 3976, 3982, 4002, 4008, 4017, 4023, 4034,
  4040, 4049, 4055, 4080, 4086, 4095, 4101, 4112,
  4118, 4127, 4133, 4153, 4159, 4168, 4174, 4185,
  4191, 4200, 4206, 4070, 4076, 4085, 4091, 4102,
  4108, 4117, 4123, 4143, 4149, 4158, 4164, 4175,
  4181, 4190, 4196, 4221, 4227, 4236, 4242, 4253,
  4259, 4268, 4274, 4294, 4300, 4309, 4315, 4326,
  4332, 4341, 4347, 4370, 4376, 4385, 4391, 4402,
  4408, 4417, 4423, 4443, 4449, 4458, 4464, 4475,
  4481, 4490, 4496, 4521, 4527, 4536, 4542, 4553,
  4559, 4568, 4574, 4594, 4600, 4609, 4615, 4626,
  4632, 4641, 4647, 3515, 3521, 3530, 3536, 3547,
  3553, 3562, 3568, 3588, 3594, 3603, 3609, 3620,
  3626, 3635, 3641, 3666, 3672, 3681, 3687, 3698,
  3704, 3713, 3719, 3739, 3745, 3754, 3760, 3771,
  3777, 3786, 3792, 3815, 3821, 3830, 3836, 3847,
  3853, 3862, 3868, 3888, 3894, 3903, 3909, 3920,
  3926, 3935, 3941, 3966, 3972, 3981, 3987, 3998,
  4004, 4013, 4019, 4039, 4045, 4054, 4060, 4071,
  4077, 4086, 4092, 3956, 3962, 3971, 3977, 3988,
  3994, 4003, 4009, 4029, 4035, 4044, 4050, 4061,
  4067, 4076, 4082, 4107, 4113, 4122, 4128, 4139,
  4145, 4154, 4160, 4180, 4186, 4195, 4201, 4212,
  4218, 4227, 4233, 4256, 4262, 4271, 4277, 4288,
  4294, 4303, 4309, 4329, 4335, 4344, 4350, 4361,
  4367, 4376, 4382, 4407, 4413, 4422, 4428, 4439,
  4445, 4454, 4460, 4480, 4486, 4495, 4501, 4512,
  4518, 4527, 4533, 4328, 4334, 4343, 4349, 4360,
  4366, 4375, 4381, 4401, 4407, 4416, 4422, 4433,
  4439, 4448, 4454, 4479, 4485, 4494, 4500, 4511,
  4517, 4526, 4532, 4552, 4558, 4567, 4573, 4584,
  4590, 4599, 4605, 4628, 4634, 4643, 4649, 4660,
  4666, 4675, 4681, 4701, 4707, 4716, 4722, 4733,
  4739, 4748, 4754, 4779, 4785, 4794, 4800, 4811,
  4817, 4826, 4832, 4852, 4858, 4867, 4873, 4884,
  4890, 4899, 4905, 4769, 4775, 4784, 4790, 4801,
  4807, 4816, 4822, 4842, 4848, 4857, 4863, 4874,
  4880, 4889, 4895, 4920, 4926, 4935, 4941, 4952,
  4958, 4967, 4973, 4993, 4999, 5008, 5014, 5025,
  5031, 5040, 5046, 5069, 5075, 5084, 5090, 5101,
  5107, 5116, 5122, 5142, 5148, 5157, 5163, 5174,
  5180, 5189, 5195, 5220, 5226, 5235, 5241, 5252,
  5258, 5267, 5273, 5293, 5299, 5308, 5314, 5325,
  5331, 5340, 5346, 4604, 4610, 4619, 4625, 4636,
  4642, 4651, 4657, 4677, 4683, 4692, 4698, 4709,
  4715, 4724, 4730, 4755, 4761, 4770, 4776, 4787,
  4793, 4802, 4808, 4828, 4834, 4843, 4849, 4860,
  4866, 4875, 4881, 4904, 4910, 4919, 4925, 4936,
  4942, 4951, 4957, 4977, 4983, 4992, 4998, 5009,
  5015, 5024, 5030, 5055, 5061, 5070, 5076, 5087,
  5093, 5102, 5108, 5128, 5134, 5143, 5149, 5160,
  5166, 5175, 5181, 5045, 5051, 5060, 5066, 5077,
  5083, 5092, 5098, 5118, 5124, 5133, 5139, 5150,
  5156, 5165, 5171, 5196, 5202, 5211, 5217, 5228,
  5234, 5243, 5249, 5269, 5275, 5284, 5290, 5301,
  5307, 5316, 5322, 5345, 5351, 5360, 5366, 5377,
  5383, 5392, 5398, 5418, 5424, 5433, 5439, 5450,
  5456, 5465, 5471, 5496, 5502, 5511, 5517, 5528,
  5534, 5543, 5549, 5569, 5575, 5584, 5590, 5601,
  5607, 5616, 5622, 5417, 5423, 5432, 5438, 5449,
  5455, 5464, 5470, 5490, 5496, 5505, 5511, 5522,
  5528, 5537, 5543, 5568, 5574, 5583, 5589, 5600,
  5606, 5615, 5621, 5641, 5647, 5656, 5662, 5673,
  5679, 5688, 5694, 5717, 5723, 5732, 5738, 5749,
  5755, 5764, 5770, 5790, 5796, 5805, 5811, 5822,
  5828, 5837, 5843, 5868, 5874, 5883, 5889, 5900,
  5906, 5915, 5921, 5941, 5947, 5956, 5962, 5973,
  5979, 5988, 5994, 5858, 5864, 5873, 5879, 5890,
  5896, 5905, 5911, 5931, 5937, 5946, 5952, 5963,
  5969, 5978, 5984, 6009, 6015, 6024, 6030, 6041,
  6047, 6056, 6062, 6082, 6088, 6097, 6103, 6114,
  6120, 6129, 6135, 6158, 6164, 6173, 6179, 6190,
  6196, 6205, 6211, 6231, 6237, 6246, 6252, 6263,
  6269, 6278, 6284, 6309, 6315, 6324, 6330, 6341,
  6347, 6356, 6362, 6382, 6388, 6397, 6403, 6414,
  6420, 6429, 6435, 3515, 3521, 3530, 3536, 3547,
  3553, 3562, 3568, 3588, 3594, 3603, 3609, 3620,
  3626, 3635, 3641, 3666, 3672, 3681, 3687, 3698,
  3704, 3713, 3719, 3739, 3745, 3754, 3760, 3771,
  3777, 3786, 3792, 3815, 3821, 3830, 3836, 3847,
  3853, 3862, 3868, 3888, 3894, 3903, 3909, 3920,
  3926, 3935, 3941, 3966, 3972, 3981, 3987, 3998,
  4004, 4013, 4019, 4039, 4045, 4054, 4060, 4071,
  4077, 4086, 4092, 3956, 3962, 3971, 3977, 3988,
  3994, 4003, 4009, 4029, 4035, 4044, 4050, 4061,
  4067, 4076, 4082, 4107, 4113, 4122, 4128, 4139,
  4145, 4154, 4160, 4180, 4186, 4195, 4201, 4212,
  4218, 4227, 4233, 4256, 4262, 4271, 4277, 4288,
  4294, 4303, 4309, 4329, 4335, 4344, 4350, 4361,
  4367, 4376, 4382, 4407, 4413, 4422, 4428, 4439,
  4445, 4454, 4460, 4480, 4486, 4495, 4501, 4512,
  4518, 4527, 4533, 4328, 4334, 4343, 4349, 4360,
  4366, 4375, 4381, 4401, 4407, 4416, 4422, 4433,
  4439, 4448, 4454, 4479, 4485, 4494, 4500, 4511,
  4517, 4526, 4532, 4552, 4558, 4567, 4573, 4584,
  4590, 4599, 4605, 4628, 4634, 4643, 4649, 4660,
  4666, 4675, 4681, 4701, 4707, 4716, 4722, 4733,
  4739, 4748, 4754, 4779, 4785, 4794, 4800, 4811,
  4817, 4826, 4832, 4852, 4858, 4867, 4873, 4884,
  4890, 4899, 4905, 4769, 4775, 4784, 4790, 4801,
  4807, 4816, 4822, 4842, 4848, 4857, 4863, 4874,
  4880, 4889, 4895, 4920, 4926, 4935, 4941, 4952,
  4958, 4967, 4973, 4993, 4999, 5008, 5014, 5025,
  5031, 5040, 5046, 5069, 5075, 5084, 5090, 5101,
  5107, 5116, 5122, 5142, 5148, 5157, 5163, 5174,
  5180, 5189, 5195, 5220, 5226, 5235, 5241, 5252,
  5258, 5267, 5273, 5293, 5299, 5308, 5314, 5325,
  5331, 5340, 5346, 4604, 4610, 4619, 4625, 4636,
  4642, 4651, 4657, 4677, 4683, 4692, 4698, 4709,
  4715, 4724, 4730, 4755, 4761, 4770, 4776, 4787,
  4793, 4802, 4808, 4828, 4834, 4843, 4849, 4860,
  4866, 4875, 4881, 4904, 4910, 4919, 4925, 4936,
  4942, 4951, 4957, 4977, 4983, 4992, 4998, 5009,
  5015, 5024, 5030, 5055, 5061, 5070, 5076, 5087,
  5093, 5102, 5108, 5128, 5134, 5143, 5149, 5160,
  5166, 5175, 5181, 5045, 5051, 5060, 5066, 5077,
  5083, 5092, 5098, 5118, 5124, 5133, 5139, 5150,
  5156, 5165, 5171, 5196, 5202, 5211, 5217, 5228,
  5234, 5243, 5249, 5269, 5275, 5284, 5290, 5301,
  5307, 5316, 5322, 5345, 5351, 5360, 5366, 5377,
  5383, 5392, 5398, 5418, 5424, 5433, 5439, 5450,
  5456, 5465, 5471, 5496, 5502, 5511, 5517, 5528,
  5534, 5543, 5549, 5569, 5575, 5584, 5590, 5601,
  5607, 5616, 5622, 5417, 5423, 5432, 5438, 5449,
  5455, 5464, 5470, 5490, 5496, 5505, 5511, 5522,
  5528, 5537, 5543, 5568, 5574, 5583, 5589, 5600,
  5606, 5615, 5621, 5641, 5647, 5656, 5662, 5673,
  5679, 5688, 5694, 5717, 5723, 5732, 5738, 5749,
  5755, 5764, 5770, 5790, 5796, 5805, 5811, 5822,
  5828, 5837, 5843, 5868, 5874, 5883, 5889, 5900,
  5906, 5915, 5921, 5941, 5947, 5956, 5962, 5973,
  5979, 5988, 5994, 5858, 5864, 5873, 5879, 5890,
  5896, 5905, 5911, 5931, 5937, 5946, 5952, 5963,
  5969, 5978, 5984, 6009, 6015, 6024, 6030, 6041,
  6047, 6056, 6062, 6082, 6088, 6097, 6103, 6114,
  6120, 6129, 6135, 6158, 6164, 6173, 6179, 6190,
  6196, 6205, 6211, 6231, 6237, 6246, 6252, 6263,
  6269, 6278, 6284, 6309, 6315, 6324, 6330, 6341,
  6347, 6356, 6362, 6382, 6388, 6397, 6403, 6414,
  6420, 6429, 6435, 5303, 5309, 5318, 5324, 5335,
  5341, 5350, 5356, 5376, 5382, 5391, 5397, 5408,
  5414, 5423, 5429, 5454, 5460, 5469, 5475, 5486,
  5492, 5501, 5507, 5527, 5533, 5542, 5548, 5559,
  5565, 5574, 5580, 5603, 5609, 5618, 5624, 5635,
  5641, 5650, 5656, 5676, 5682, 5691, 5697, 5708,
  5714, 5723, 5729, 5754, 5760, 5769, 5775, 5786,
  5792, 5801, 5807, 5827, 5833, 5842, 5848, 5859,
  5865, 5874, 5880, 5744, 5750, 5759, 5765, 5776,
  5782, 5791, 5797, 5817, 5823, 5832, 5838, 5849,
  5855, 5864, 5870, 5895, 5901, 5910, 5916, 5927,
  5933, 5942, 5948, 5968, 5974, 5983, 5989, 6000,
  6006, 6015, 6021, 6044, 6050, 6059, 6065, 6076,
  6082, 6091, 6097, 6117, 6123, 6132, 6138, 6149,
  6155, 6164, 6170, 6195, 6201, 6210, 6216, 6227,
  6233, 6242, 6248, 6268, 6274, 6283, 6289, 6300,
  6306, 6315, 6321, 6116, 6122, 6131, 6137, 6148,
  6154, 6163, 6169, 6189, 6195, 6204, 6210, 6221,
  6227, 6236, 6242, 6267, 6273, 6282, 6288, 6299,
  6305, 6314, 6320, 6340, 6346, 6355, 6361, 6372,
  6378, 6387, 6393, 6416, 6422, 6431, 6437, 6448,
  6454, 6463, 6469, 6489, 6495, 6504, 6510, 6521,
  6527, 6536, 6542, 6567, 6573, 6582, 6588, 6599,
  6605, 6614, 6620, 6640, 6646, 6655, 6661, 6672,
  6678, 6687, 6693, 6557, 6563, 6572, 6578, 6589,
  6595, 6604, 6610, 6630, 6636, 6645, 6651, 6662,
  6668, 6677, 6683, 6708, 6714, 6723, 6729, 6740,
  6746, 6755, 6761, 6781, 6787, 6796, 6802, 6813,
  6819, 6828, 6834, 6857, 6863, 6872, 6878, 6889,
  6895, 6904, 6910, 6930, 6936, 6945, 6951, 6962,
  6968, 6977, 6983, 7008, 7014, 7023, 7029, 7040,
  7046, 7055, 7061, 7081, 7087, 7096, 7102, 7113,
  7119, 7128, 7134, 6392, 6398, 6407, 6413, 6424,
  6430, 6439, 6445, 6465, 6471, 6480, 6486, 6497,
  6503, 6512, 6518, 6543, 6549, 6558, 6564, 6575,
  6581, 6590, 6596, 6616, 6622, 6631, 6637, 6648,
  6654, 6663, 6669, 6692, 6698, 6707, 6713, 6724,
  6730, 6739, 6745, 6765, 6771, 6780, 6786, 6797,
  6803, 6812, 6818, 6843, 6849, 6858, 6864, 6875,
  6881, 6890, 6896, 6916, 6922, 6931, 6937, 6948,
  6954, 6963, 6969, 6833, 6839, 6848, 6854, 6865,
  6871, 6880, 6886, 6906, 6912, 6921, 6927, 6938,
  6944, 6953, 6959, 6984, 6990, 6999, 7005, 7016,
  7022, 7031, 7037, 7057, 7063, 7072, 7078, 7089,
  7095, 7104, 7110, 7133, 7139, 7148, 7154, 7165,
  7171, 7180, 7186, 7206, 7212, 7221, 7227, 7238,
  7244, 7253, 7259, 7284, 7290, 7299, 7305, 7316,
  7322, 7331, 7337, 7357, 7363, 7372, 7378, 7389,
  7395, 7404, 7410, 7205, 7211, 7220, 7226, 7237,
  7243, 7252, 7258, 7278, 7284, 7293, 7299, 7310,
  7316, 7325, 7331, 7356, 7362, 7371, 7377, 7388,
  7394, 7403, 7409, 7429, 7435, 7444, 7450, 7461,
  7467, 7476, 7482, 7505, 7511, 7520, 7526, 7537,
  7543, 7552, 7558, 7578, 7584, 7593, 7599, 7610,
  7616, 7625, 7631, 7656, 7662, 7671, 7677, 7688,
  7694, 7703, 7709, 7729, 7735, 7744, 7750, 7761
};

static int VP8LevelCost(const uint16_t* table, int level) {
#pragma HLS INLINE off
  return VP8LevelFixedCosts[level] + table[(level > MAX_VARIABLE_LEVEL) ? MAX_VARIABLE_LEVEL : level];
}

const uint8_t VP8EncBands[16 + 1] = {
  0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
  0  // sentinel
};

int GetResidualCost_C(int ctx0, int first, int last,
		const int16_t const coeffs[16], ProbaArray prob[NUM_BANDS], CostArrayMap costs) {
#pragma HLS ARRAY_PARTITION variable=coeffs complete dim=1
#pragma HLS ARRAY_PARTITION variable=costs complete dim=1
  //should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  const int p0 = prob[first][ctx0][0];

  if (last < 0) {
    return VP8BitCost(0, p0);
  }

  int n, cost = 0, cost_t[18];
#pragma HLS ARRAY_PARTITION variable=cost_t complete dim=1
#pragma HLS ARRAY_PARTITION variable=VP8EncBands complete dim=1
  for(n = 0; n < 18; n++){
#pragma HLS unroll
	  cost_t[n] = 0;
  }

  // bit_cost(1, p0) is already incorporated in t[] tables, but only if ctx != 0
  // (as required by the syntax). For ctx0 == 0, we need to add it here or it'll
  // be missing during the loop.
  cost_t[16] = (ctx0 == 0) ? VP8BitCost(1, p0) : 0;

  int v[16], ctx[16];

  v[0] = abs(coeffs[0]);
  ctx[0] = ctx0;
  cost_t[0] = VP8LevelCost(costs[0][ctx[0]], v[0]);

  for(n = 1; n < 16; n++){
#pragma HLS unroll
	v[n] = abs(coeffs[n]);
	ctx[n] = (v[n-1] >= 2) ? 2 : v[n-1];
  }

  for(n = 1; n < 16; n++){
#pragma HLS unroll
	cost_t[n] = VP8LevelCost(costs[n][ctx[n]], v[n]);
  }

  if (last < 15) {
	const int v1 = abs(coeffs[last]);
    const int ctx1 = (v1 == 1) ? 1 : 2;
	const int b = VP8EncBands[last + 1];
	const int last_p0 = prob[b][ctx1][0];
    cost_t[17] = VP8BitCost(0, last_p0);
  }

  for(n = 0; n < 18; n++){
#pragma HLS unroll
	  cost += cost_t[n];
  }

  return cost;
}

int VP8GetCostLuma16(const int tnz, const int lnz, int top_nz[9], int left_nz[9],
		ProbaArray coeffs_dc[NUM_BANDS],CostArrayMap remapped_costs_dc,
		ProbaArray coeffs_ac[NUM_BANDS],CostArrayMap remapped_costs_ac,
		const VP8ModeScore* const rd) {
//#pragma HLS pipeline
#pragma HLS ARRAY_PARTITION variable=remapped_costs_dc complete dim=1
#pragma HLS ARRAY_PARTITION variable=remapped_costs_ac complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_nz complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_nz complete dim=1
#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd->y_dc_levels complete dim=1
  int last;
  int ctx;
  int x, y;
  int R = 0;

  VP8IteratorNzToBytes(tnz, lnz, top_nz, left_nz);   // re-import the non-zero context

  // DC
  last = SetResidualCoeffs_C(rd->y_dc_levels);
  ctx = top_nz[8] + left_nz[8];
  R += GetResidualCost_C(ctx, 0, last, rd->y_dc_levels, coeffs_dc, remapped_costs_dc);

  // AC
  int ctx_t[16],last_t[16],i;
  int16_t y_ac_levels_t[16];

  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      for(i = 0; i < 16; ++i) {
#pragma HLS unroll
    	  y_ac_levels_t[i] = rd->y_ac_levels[x + y * 4][i];
      }
      ctx_t[x + y * 4] = top_nz[x] + left_nz[y];
      last_t[x + y * 4] = SetResidualCoeffs_C(y_ac_levels_t);
      top_nz[x] = left_nz[y] = (last_t[x + y * 4] >= 0);
    }
  }
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      R += GetResidualCost_C(ctx_t[x + y * 4], 1, last_t[x + y * 4], rd->y_ac_levels[x + y * 4], coeffs_ac, remapped_costs_ac);
    }
  }
  return R;
}

static score_t IsFlat(const int16_t (*levels)[16], int num_blocks, score_t thresh) {
  score_t score = 0;
  int i,j;
  for (j = 0; j < num_blocks; ++j) { // TODO(skal): refine positional scoring?
#pragma HLS unroll
    for (i = 1; i < 16; ++i) {     // omit DC, we're only interested in AC
#pragma HLS unroll
	  score += (levels[j][i] != 0);
	}
  }
  if (score > thresh)
	return 0;
  else
	return 1;
}

void rd_cal(
		VP8ModeScore * rd_cur, uint8_t Yin[16*16], uint8_t Yout[16*16],
		const int lambda, const int tlambda, ap_uint<2> mode, const int tnz,
		const int lnz, int top_nz[9], int left_nz[9],
		ProbaArray coeffs_dc[NUM_BANDS], CostArrayMap remapped_costs_dc,
		ProbaArray coeffs_ac[NUM_BANDS], CostArrayMap remapped_costs_ac) {
#pragma HLS pipeline
#pragma HLS ARRAY_PARTITION variable=remapped_costs_dc complete dim=1
#pragma HLS ARRAY_PARTITION variable=remapped_costs_ac complete dim=1
#pragma HLS ARRAY_PARTITION variable=left_nz complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_nz complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
#pragma HLS ARRAY_PARTITION variable=rd_cur->y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd_cur->y_dc_levels complete dim=1

		const int kNumBlocks = 16;
	    rd_cur->D = SSE16x16_C(Yin, Yout);
	    rd_cur->SD = MULT_8B(tlambda, Disto16x16_C(Yin, Yout, kWeightY));
	    rd_cur->H = VP8FixedCostsI16[mode];
//	    rd_cur->R = VP8GetCostLuma16(tnz, lnz, top_nz, left_nz, coeffs_dc, remapped_costs_dc, coeffs_ac, remapped_costs_ac, rd_cur);
//	    rd_cur->R += FLATNESS_PENALTY * kNumBlocks * (mode != 0) * IsFlat(rd_cur->y_ac_levels, kNumBlocks, FLATNESS_LIMIT_I16);
}
