#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "hw_webp.h"

static void Fill_16(uint8_t* dst, int value) {
  int i,j;
  for (j = 0; j < 16; ++j) {
#pragma HLS unroll
    for(i = 0; i < 16; ++i){
#pragma HLS unroll
        dst[j * 16 + i] = value;
    }
  }
}

static void DCMode_16(uint8_t* dst, uint8_t* left, uint8_t* top, int x, int y) {
  int DC = 0;
  int j;

  if (x != 0) {
    if (y != 0) {
  	  for (j = 0; j < 16; ++j){
#pragma HLS unroll
	    DC += top[j] + left[j];
	  }
    } else {
      for (j = 0; j < 16; ++j){
#pragma HLS unroll
		DC += left[j] << 1;
	  }
    }
  } else {
    if (y != 0) {
      for (j = 0; j < 16; ++j){
#pragma HLS unroll
		DC += top[j] << 1;
	  }
    } else {
    	DC = 0x80 << 5;
    }
  }

  DC = (DC + 16) >> 5;
  Fill_16(dst, DC);
}

static void VerticalPred_16(uint8_t* dst, uint8_t* top) {
  int i,j;
    for (j = 0; j < 16; ++j) {
#pragma HLS unroll
    	for(i = 0; i < 16; ++i){
#pragma HLS unroll
    		dst[j * 16 + i] = top[i];
    	}
    }
}

static void HorizontalPred_16(uint8_t* dst, uint8_t* left) {
    int i,j;
    for (j = 0; j < 16; ++j) {
#pragma HLS unroll
    	for(i = 0; i < 16; ++i){
#pragma HLS unroll
    		dst[j * 16 + i] = left[j];
    	}
    }
}

static void TrueMotion_16(uint8_t* dst, uint8_t* left, uint8_t* top, uint8_t top_left, int x, int y) {
  int i,j;
  int tmp;
  if (x != 0) {
    if (y != 0) {
      for (j = 0; j < 16; ++j) {
#pragma HLS unroll
        for (i = 0; i < 16; ++i) {
#pragma HLS unroll
        	tmp = top[i] + left[j] - top_left;
        	dst[j * 16 + i] = (tmp>0xff) ? 0xff : (tmp<0) ? 0 : (uint8_t)tmp;
        }
      }
    } else {
      HorizontalPred_16(dst, left);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (y != 0) {
      VerticalPred_16(dst, top);
    } else {
      Fill_16(dst, 129);
    }
  }
}

static void Fill_8(uint8_t* dst, int value_u, int value_v) {
  int i,j;
  for (j = 0; j < 8; ++j) {
#pragma HLS unroll
    for(i = 0; i < 8; ++i){
#pragma HLS unroll
        dst[j * 16 + i] = value_u;
        dst[j * 16 + i + 8] = value_v;
    }
  }
}

static void DCMode_8(uint8_t* dst, uint8_t* left_u, uint8_t* top_u,
		uint8_t* left_v, uint8_t* top_v, int x, int y) {
  int DC_u = 0;
  int DC_v = 0;
  int j;

  if (x != 0) {
    if (y != 0) {
  	  for (j = 0; j < 8; ++j){
#pragma HLS unroll
	    DC_u += top_u[j] + left_u[j];
	    DC_v += top_v[j] + left_v[j];
	  }
    } else {
      for (j = 0; j < 8; ++j){
#pragma HLS unroll
		DC_u += left_u[j] << 1;
		DC_v += left_v[j] << 1;
	  }
    }
  } else {
    if (y != 0) {
      for (j = 0; j < 8; ++j){
#pragma HLS unroll
		DC_u += top_u[j] << 1;
		DC_v += top_v[j] << 1;
	  }
    } else {
    	DC_u = 0x80 << 4;
    	DC_v = 0x80 << 4;
    }
  }

  DC_u = (DC_u + 8) >> 4;
  DC_v = (DC_v + 8) >> 4;
  Fill_8(dst, DC_u, DC_v);
}

static void VerticalPred_8(uint8_t* dst, uint8_t* top_u, uint8_t* top_v) {
  int i,j;
    for (j = 0; j < 8; ++j) {
#pragma HLS unroll
    	for(i = 0; i < 8; ++i){
#pragma HLS unroll
    		dst[j * 16 + i] = top_u[i];
    		dst[j * 16 + i + 8] = top_v[i];
    	}
    }
}

static void HorizontalPred_8(uint8_t* dst, uint8_t* left_u, uint8_t* left_v) {
    int i,j;
    for (j = 0; j < 8; ++j) {
#pragma HLS unroll
    	for(i = 0; i < 8; ++i){
#pragma HLS unroll
    		dst[j * 16 + i] = left_u[j];
    		dst[j * 16 + i + 8] = left_v[j];
    	}
    }
}

static void TrueMotion_8(uint8_t* dst, uint8_t* left_u, uint8_t* top_u, uint8_t top_left_u,
		uint8_t* left_v, uint8_t* top_v, uint8_t top_left_v, int x, int y) {
  int i,j;
  int tmp_u;
  int tmp_v;
  if (x != 0) {
    if (y != 0) {
      for (j = 0; j < 8; ++j) {
#pragma HLS unroll
        for (i = 0; i < 8; ++i) {
#pragma HLS unroll
        	tmp_u = top_u[i] + left_u[j] - top_left_u;
        	tmp_v = top_v[i] + left_v[j] - top_left_v;
        	dst[j * 16 + i] = (tmp_u>0xff) ? 0xff : (tmp_u<0) ? 0 : (uint8_t)tmp_u;
        	dst[j * 16 + i + 8] = (tmp_v>0xff) ? 0xff : (tmp_v<0) ? 0 : (uint8_t)tmp_v;
        }
      }
    } else {
      HorizontalPred_8(dst, left_u, left_v);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (y != 0) {
      VerticalPred_8(dst, top_u, top_v);
    } else {
      Fill_8(dst, 129, 129);
    }
  }
}

static void IntraChromaPreds_C(uint8_t UVPred[4][8*16], uint8_t left_u[8],
		uint8_t top_u[8], uint8_t top_left_u, uint8_t left_v[8], uint8_t top_v[8],
		uint8_t top_left_v, int x, int y) {
//#pragma HLS ARRAY_PARTITION variable=top_u complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_u complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_v complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_v complete dim=1
//#pragma HLS ARRAY_PARTITION variable=UVPred complete dim=0
  // U V block
  DCMode_8(UVPred[0], left_u, top_u, left_v, top_v, x, y);
  VerticalPred_8(UVPred[2], top_u, top_v);
  HorizontalPred_8(UVPred[3], left_u, left_v);
  TrueMotion_8(UVPred[1], left_u, top_u, top_left_u, left_v, top_v, top_left_v, x, y);
}

// luma 4x4 prediction

#define DST(x, y) dst[(x) + (y) * 4]
#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

static void Fill_4(uint8_t* dst, int value) {
  int i,j;
  for (j = 0; j < 4; ++j) {
#pragma HLS unroll
    for(i = 0; i < 4; ++i){
#pragma HLS unroll
        dst[j * 4 + i] = value;
    }
  }
}

static void VE4(uint8_t* dst, uint8_t top_left, uint8_t* top, uint8_t* top_right) {    // vertical
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

static void DC4(uint8_t* dst, uint8_t* top, uint8_t* left) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i){
#pragma HLS unroll
	  dc += top[i] + left[i];
  }
  dc = dc >> 3;
  Fill_4(dst, dc);
}

static void RD4(uint8_t* dst, uint8_t* left, uint8_t top_left, uint8_t* top) {
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

static void LD4(uint8_t* dst, uint8_t* top, uint8_t* top_right) {
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

static void VR4(uint8_t* dst, uint8_t* left, uint8_t top_left, uint8_t* top) {
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

static void VL4(uint8_t* dst, uint8_t* top, uint8_t* top_right) {
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

static void HU4(uint8_t* dst, uint8_t* left) {
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

static void HD4(uint8_t* dst, uint8_t* left, uint8_t top_left, uint8_t* top) {
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

static void TM4(uint8_t* dst, uint8_t* top, uint8_t* left, uint8_t top_left) {
  int i, j;
  int tmp;
  for (j = 0; j < 4; ++j) {
#pragma HLS unroll
    for (i = 0; i < 4; ++i) {
#pragma HLS unroll
      tmp = top[i] + left[j] - top_left;
      dst[j * 4 + i] = (tmp>0xff) ? 0xff : (tmp<0) ? 0 : (uint8_t)tmp;
    }
  }
}

static void Intra4Preds_C(
		uint8_t Pred[10][16], uint8_t left[4], uint8_t top_left, uint8_t top[4], uint8_t top_right[4]) {
// #pragma HLS ARRAY_PARTITION variable=left complete dim=1
// #pragma HLS ARRAY_PARTITION variable=top complete dim=1
// #pragma HLS ARRAY_PARTITION variable=top_right complete dim=1
// #pragma HLS ARRAY_PARTITION variable=Pred complete dim=0

  DC4(Pred[0], top, left);
  TM4(Pred[1], top, left, top_left);
  VE4(Pred[2], top_left, top, top_right);
  HE4(Pred[3], left, top_left);
  RD4(Pred[4], left, top_left, top);
  VR4(Pred[5], left, top_left, top);
  LD4(Pred[6], top, top_right);
  VL4(Pred[7], top, top_right);
  HD4(Pred[8], left, top_left, top);
  HU4(Pred[9], left);
}

static void FTransform_C(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  int i;
  int tmp[16];
  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
    const int d0 = src[4 * i + 0] - ref[4 * i + 0];   // 9bit dynamic range ([-255,255])
    const int d1 = src[4 * i + 1] - ref[4 * i + 1];
    const int d2 = src[4 * i + 2] - ref[4 * i + 2];
    const int d3 = src[4 * i + 3] - ref[4 * i + 3];
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

static void FTransformWHT_C(const int16_t* in, int16_t* out) {
  // input is 12b signed
  int32_t tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
    const int a0 = (in[4 * i + 0] + in[4 * i + 2]);  // 13b
    const int a1 = (in[4 * i + 1] + in[4 * i + 3]);
    const int a2 = (in[4 * i + 1] - in[4 * i + 3]);
    const int a3 = (in[4 * i + 0] - in[4 * i + 2]);
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

static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

#define QFIX 17

static int QUANTDIV(uint32_t n, uint32_t iQ, uint32_t B) {
  return (int)((n * iQ + B) >> QFIX);
}

static int QuantizeBlock_C(int16_t* in, int16_t* out,
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

static void TransformWHT_C(const int16_t* in, int16_t* out) {
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
    out[4 * i + 0] = (a0 + a1) >> 3;
    out[4 * i + 1] = (a3 + a2) >> 3;
    out[4 * i + 2] = (a0 - a1) >> 3;
    out[4 * i + 3] = (a3 - a2) >> 3;
  }
}

typedef int16_t fixed_t;

uint8_t clip_8b(fixed_t v) {
  return (v>0xff) ? 0xff : (v<0) ? 0 : (uint8_t)v;
}

#define STORE(x, y, v) \
  dst[(x) + (y) * 4] = clip_8b(ref[(x) + (y) * 4] + ((v) >> 3))

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)

static void ITransformOne(const uint8_t* ref, const int16_t* in,
                                      uint8_t* dst) {
  int tmp[4 * 4];
  int i;
  for (i = 0; i < 4; ++i) {    // vertical pass
#pragma HLS unroll
    const int a = in[i + 0] + in[i + 8];
    const int b = in[i + 0] - in[i + 8];
    const int c = MUL(in[i + 4], kC2) - MUL(in[i + 12], kC1);
    const int d = MUL(in[i + 4], kC1) + MUL(in[i + 12], kC2);
    tmp[4 * i + 0] = a + d;
    tmp[4 * i + 1] = b + c;
    tmp[4 * i + 2] = b - c;
    tmp[4 * i + 3] = a - d;
  }

  for (i = 0; i < 4; ++i) {    // horizontal pass
#pragma HLS unroll
    const int dc = tmp[i + 0] + 4;
    const int a =  dc +  tmp[i + 8];
    const int b =  dc -  tmp[i + 8];
    const int c = MUL(tmp[i + 4], kC2) - MUL(tmp[i + 12], kC1);
    const int d = MUL(tmp[i + 4], kC1) + MUL(tmp[i + 12], kC2);
    STORE(0, i, a + d);
    STORE(1, i, b + c);
    STORE(2, i, b - c);
    STORE(3, i, a - d);
  }
}

static int ReconstructIntra16(
		uint8_t YPred[16*16], uint8_t Ysrc[16*16], uint8_t Yout[16*16],
		int16_t y_ac_levels[16][16], int16_t y_dc_levels[16], VP8Matrix y1, VP8Matrix y2) {
//#pragma HLS pipeline
//#pragma HLS ARRAY_PARTITION variable=y1.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y2.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y2.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y2.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y2.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y2.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y_ac_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=y_dc_levels complete dim=1
//#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
//#pragma HLS ARRAY_PARTITION variable=Ysrc complete dim=1
//#pragma HLS ARRAY_PARTITION variable=YPred complete dim=1

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

static void VP8MatrixLoad(VP8Matrix* dst, VP8Matrix* src){
//#pragma HLS INTERFACE ap_none port=dst register
#pragma HLS inline off
	int i;
	for(i=0;i<16;i++){
#pragma HLS unroll
		dst->q_[i]		 = src->q_[i]	   ;
		dst->iq_[i]		 = src->iq_[i]	   ;
		dst->bias_[i]	 = src->bias_[i]   ;
		dst->zthresh_[i] = src->zthresh_[i];
		dst->sharpen_[i] = src->sharpen_[i];
	}
}

static int ReconstructIntra4(int16_t levels[16], uint8_t y_p[16],
		uint8_t y_src[16], uint8_t y_out[16], VP8Matrix y1) {
#pragma HLS inline
//#pragma HLS ARRAY_PARTITION variable=y1.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y1.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=levels complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y_out complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y_src complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y_p complete dim=1
  int nz = 0;
  int16_t tmp[16];
  VP8Matrix y1_i;
#pragma HLS ARRAY_PARTITION variable=tmp complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1_i.sharpen_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1_i.zthresh_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1_i.bias_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1_i.iq_ complete dim=1
#pragma HLS ARRAY_PARTITION variable=y1_i.q_ complete dim=1

  VP8MatrixLoad(&y1_i, &y1);

  FTransform_C(y_src, y_p, tmp);

  nz = QuantizeBlock_C(tmp, levels, &y1_i);

  ITransformOne(y_p, tmp, y_out);

  return nz;
}

#define C1 7    // fraction of error sent to the 4x4 block below
#define C2 8    // fraction of error sent to the 4x4 block on the right
#define DSHIFT 4
#define DSCALE 1   // storage descaling, needed to make the error fit int8_t

static int QuantizeSingle(int16_t* const v, const VP8Matrix* const mtx) {
  int V = *v;
  const int sign = (V < 0);
  if (sign) V = -V;
  if (V > (int)mtx->zthresh_[0]) {
    const uint32_t n = V;
    const uint32_t iQ = mtx->iq_[0];
    const uint32_t B = mtx->bias_[0];
    const int qV = QUANTDIV(n, iQ, B) * mtx->q_[0];
    const int err = (V - qV);
    *v = sign ? -qV : qV;
    return (sign ? -err : err) >> DSCALE;
  } else {
  *v = 0;
  return (sign ? -V : V) >> DSCALE;
  }
}

static void CorrectDCValues(DError top_derr[1024], DError left_derr, int x, int y,
                            const VP8Matrix* const mtx,
                            int16_t tmp[][16], int8_t derr[2][3]) {
  //         | top[0] | top[1]
  // --------+--------+---------
  // left[0] | tmp[0]   tmp[1]  <->   err0 err1
  // left[1] | tmp[2]   tmp[3]        err2 err3
  //
  // Final errors {err1,err2,err3} are preserved and later restored
  // as top[]/left[] on the next block.

  int8_t top_tmp[2][2];
  int i, j;
  for (j = 0; j < 2; ++j) {
#pragma HLS unroll
	  for (i = 0; i < 2; ++i) {
#pragma HLS unroll
		  top_tmp[j][i] = y ? top_derr[x][j][i] : 0;
	  }
  }

  int ch;
  for (ch = 0; ch <= 1; ++ch) {
#pragma HLS unroll
    const int8_t* const top = top_tmp[ch];
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

static int ReconstructUV(int16_t uv_levels[8][16], uint8_t uv_p[8*16],
		uint8_t uv_src[8*16], uint8_t uv_out[8*16], VP8Matrix uv,
		DError top_derr[1024], DError left_derr, int x, int y, int8_t derr[2][3]) {
//#pragma HLS ARRAY_PARTITION variable=uv.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=uv_out complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv_src complete dim=1
//#pragma HLS ARRAY_PARTITION variable=uv_p complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=2
//#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=3
//#pragma HLS ARRAY_PARTITION variable=left_derr complete dim=0
//#pragma HLS ARRAY_PARTITION variable=derr complete dim=0
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

  CorrectDCValues(top_derr, left_derr, x, y, &uv, tmp, derr);

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

static const uint16_t kWeightY[16] = {
  38, 32, 20, 9, 32, 28, 17, 7, 20, 17, 10, 4, 9, 7, 4, 2
};

#define FLATNESS_LIMIT_I16 10      // I16 mode
#define FLATNESS_PENALTY   140     // roughly ~1bit per block

static int GetSSE16x16(const uint8_t* a, const uint8_t* b) {
#pragma HLS inline off
  int count = 0;
  int y, x;
  for (y = 0; y < 16; ++y) {
//#pragma HLS unroll
    for (x = 0; x < 16; ++x) {
#pragma HLS unroll
      const int diff = (int)a[x + y * 16] - b[x + y * 16];
      count += diff * diff;
    }
  }
  return count;
}

#define MULT_8B(a, b) (((a) * (b) + 128) >> 8)

static int TTransform(const uint8_t* in, const uint16_t* w) {
#pragma HLS inline
  int sum = 0;
  int tmp[16];
  int i;
  // horizontal pass
  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
    const int a0 = in[4 * i + 0] + in[4 * i + 2];
    const int a1 = in[4 * i + 1] + in[4 * i + 3];
    const int a2 = in[4 * i + 1] - in[4 * i + 3];
    const int a3 = in[4 * i + 0] - in[4 * i + 2];
    tmp[0 + i * 4] = a0 + a1;
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  // vertical pass
  for (i = 0; i < 4; ++i) {
#pragma HLS unroll
    const int a0 = tmp[0 + i] + tmp[8 + i];
    const int a1 = tmp[4 + i] + tmp[12+ i];
    const int a2 = tmp[4 + i] - tmp[12+ i];
    const int a3 = tmp[0 + i] - tmp[8 + i];
    const int b0 = a0 + a1;
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;

    sum += w[i +  0] * abs(b0);
    sum += w[i +  4] * abs(b1);
    sum += w[i +  8] * abs(b2);
    sum += w[i + 12] * abs(b3);
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
#pragma HLS inline off
  int D = 0;
  int i,j,n;
  uint8_t tmp_a[16][16], tmp_b[16][16];
  const uint16_t VP8Scan[16] = {
    0 +  0 * 16,  4 +  0 * 16, 8 +  0 * 16, 12 +  0 * 16,
    0 +  4 * 16,  4 +  4 * 16, 8 +  4 * 16, 12 +  4 * 16,
    0 +  8 * 16,  4 +  8 * 16, 8 +  8 * 16, 12 +  8 * 16,
    0 + 12 * 16,  4 + 12 * 16, 8 + 12 * 16, 12 + 12 * 16,
  };

#pragma HLS ARRAY_PARTITION variable=tmp_a complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_b complete dim=0
#pragma HLS ARRAY_PARTITION variable=VP8Scan complete dim=1

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
//#pragma HLS unroll
      D += Disto4x4_C(tmp_a[i], tmp_b[i], w);
    }

  return D;

}

const uint16_t VP8FixedCostsI16[4] = { 663, 919, 872, 919 };

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
#pragma HLS inline off
  rd->score = (rd->R + rd->H) * lambda + RD_DISTO_MULT * rd->D + rd->SD;
}

static void SetRDScore_i4(int lambda, VP8ModeScore* const rd) {
  rd->score = (rd->R + rd->H) * lambda + RD_DISTO_MULT * rd->D + rd->SD;
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

static int VP8GetCostLuma16(int16_t y_ac_levels[16][16], int16_t y_dc_levels[16]){
#pragma HLS inline off
	int64_t test_R = 0;
	int y, x;
	for (y = 0; y < 16; ++y) {
//#pragma HLS unroll
	  for (x = 0; x < 16; ++x) {
#pragma HLS unroll
	    test_R += y_ac_levels[y][x] * y_ac_levels[y][x];
	  }
	  test_R += y_dc_levels[y] * y_dc_levels[y];
	}
	return test_R << 10;
}

static void Copy_16x16_int16(int16_t dst[16][16], int16_t src[16][16]){
	int y, x;
	for (y = 0; y < 16; ++y) {
#pragma HLS unroll
		for (x = 0; x < 16; ++x) {
#pragma HLS unroll
			dst[y][x] = src[y][x];
		}
	}
}

static void Copy_16_int16(int16_t dst[16], int16_t src[16]){
	int y;
	for (y = 0; y < 16; ++y) {
#pragma HLS unroll
		dst[y] = src[y];
	}
}

static void Copy_256_uint8(uint8_t dst[16*16], uint8_t src[16*16]){
	int y;
	for (y = 0; y < 256; ++y) {
#pragma HLS unroll
		dst[y] = src[y];
	}
}

static void Copy_16_uint8(uint8_t dst[16], uint8_t src[16]){
	int y;
	for (y = 0; y < 16; ++y) {
#pragma HLS unroll
		dst[y] = src[y];
	}
}

static void PickBestIntra16(uint8_t Yin[16*16], uint8_t Yout[16*16],
		VP8ModeScore* rd, VP8SegmentInfo* const dqm, uint8_t left_y[16],
		uint8_t top_y[20], uint8_t top_left_y, int x, int y) {
//#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
//#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=rd->y_dc_levels complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_y complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_y complete dim=1

  const int lambda = dqm->lambda_i16_;
  const int tlambda = dqm->tlambda_;
  VP8ModeScore rd_tmp;
  int mode;
  uint8_t Yout_tmp[16*16];
  uint8_t YPred_0[16*16];
  uint8_t YPred_1[16*16];
  uint8_t YPred_2[16*16];
  uint8_t YPred_3[16*16];
  uint8_t YPred[16*16];

#pragma HLS ARRAY_PARTITION variable=rd_tmp.y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd_tmp.y_dc_levels complete dim=1
#pragma HLS ARRAY_PARTITION variable=Yout_tmp complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_2 complete dim=1
#pragma HLS ARRAY_PARTITION variable=YPred_3 complete dim=1
#pragma HLS ARRAY_PARTITION variable=VP8FixedCostsI16 complete dim=1

  DCMode_16(YPred_0, left_y, top_y, x, y);
  VerticalPred_16(YPred_2, top_y);
  HorizontalPred_16(YPred_3, left_y);
  TrueMotion_16(YPred_1, left_y, top_y, top_left_y, x, y);

  for (mode = NUM_PRED_MODES - 1; mode >= 0; --mode) {
	switch(mode){
	case 0:
		Copy_256_uint8(YPred, YPred_0);
		break;
	case 1:
		Copy_256_uint8(YPred, YPred_1);
		break;
	case 2:
		Copy_256_uint8(YPred, YPred_2);
		break;
	case 3:
		Copy_256_uint8(YPred, YPred_3);
		break;
	}
	// Reconstruct
    rd_tmp.nz = ReconstructIntra16(YPred, Yin, Yout_tmp, rd_tmp.y_ac_levels,
    		rd_tmp.y_dc_levels, dqm->y1_, dqm->y2_);

    // Measure RD-score
	rd_tmp.D = GetSSE16x16(Yin, Yout_tmp);
	rd_tmp.SD = tlambda * Disto16x16_C(Yin, Yout_tmp, kWeightY);
	rd_tmp.H = VP8FixedCostsI16[mode];
	rd_tmp.R = VP8GetCostLuma16(rd_tmp.y_ac_levels, rd_tmp.y_dc_levels);

    // Since we always examine Intra16 first, we can overwrite *rd directly.
    SetRDScore(lambda, &rd_tmp);

    if (mode == NUM_PRED_MODES - 1 || rd_tmp.score <= rd->score) {
      rd->mode_i16 = mode;
      CopyScore(rd, &rd_tmp);
      Copy_16x16_int16(rd->y_ac_levels, rd_tmp.y_ac_levels);
      Copy_16_int16(rd->y_dc_levels, rd_tmp.y_dc_levels);
      Copy_256_uint8(Yout, Yout_tmp);
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

const uint16_t VP8FixedCostsI4[NUM_BMODES] =
   {   40, 1151, 1723, 1874, 2103, 2019, 1628, 1777, 2226, 2137 };

static int GetSSE4x4(const uint8_t* a, const uint8_t* b) {
  int count = 0;
  int y, x;
  for (y = 0; y < 4; ++y) {
#pragma HLS unroll
    for (x = 0; x < 4; ++x) {
#pragma HLS unroll
      const int diff = (int)a[x + y * 4] - b[x + y * 4];
      count += diff * diff;
    }
  }
  return count;
}

static void AddScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  += src->D;
  dst->SD += src->SD;
  dst->R  += src->R;
  dst->H  += src->H;
  dst->nz |= src->nz;     // here, new nz bits are accumulated.
  dst->score += src->score;
}

static int VP8IteratorRotateI4(uint8_t y_left[16], uint8_t y_top_left,
		uint8_t y_top[20], int i4_, uint8_t top_mem[16], uint8_t yuv_out[16][16],
		uint8_t left[4], uint8_t* top_left, uint8_t top[4], uint8_t top_right[4]) {

  const uint8_t* const blk = yuv_out[i4_];
  int i;

  switch(i4_){
  case 0 :
		*top_left = y_top[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = y_top[4+i];
			top_right[i] = y_top[8+i];
			top_mem[i] = blk[12+i];
		}
		break;
  case 1 :
		*top_left = y_top[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = y_top[8+i];
			top_right[i] = y_top[12+i];
			top_mem[4+i] = blk[12+i];
		}
		break;
  case 2 :
  		*top_left = y_top[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = y_top[12+i];
			top_right[i] = y_top[16+i];
			top_mem[8+i] = blk[12+i];
		}
		break;
  case 3 :
  		*top_left = y_left[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[4+i];
			top[i] = top_mem[i];
			top_right[i] = top_mem[4+i];
			top_mem[12+i] = blk[12+i];
		}
		break;
  case 4 :
		*top_left = top_mem[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[4+i];
			top_right[i] = top_mem[8+i];
			top_mem[i] = blk[12+i];
		}
		break;
  case 5 :
  		*top_left = top_mem[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[8+i];
			top_right[i] = top_mem[12+i];
			top_mem[4+i] = blk[12+i];
		}
		break;
  case 6 :
  		*top_left = top_mem[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[12+i];
			top_right[i] = y_top[16+i];
			top_mem[8+i] = blk[12+i];
		}
		break;
  case 7 :
  		*top_left = y_left[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[8+i];
			top[i] = top_mem[i];
			top_right[i] = top_mem[4+i];
			top_mem[12+i] = blk[12+i];
		}
		break;
  case 8 :
		*top_left = top_mem[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[4+i];
			top_right[i] = top_mem[8+i];
			top_mem[i] = blk[12+i];
		}
		break;
  case 9 :
  		*top_left = top_mem[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[8+i];
			top_right[i] = top_mem[12+i];
			top_mem[4+i] = blk[12+i];
		}
		break;
  case 10 :
  		*top_left = top_mem[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[12+i];
			top_right[i] = y_top[16+i];
			top_mem[8+i] = blk[12+i];
		}
		break;
  case 11 :
  		*top_left = y_left[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = y_left[12+i];
			top[i] = top_mem[i];
			top_right[i] = top_mem[4+i];
			top_mem[12+i] = blk[12+i];
		}
		break;
  case 12 :
		*top_left = top_mem[3];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[4+i];
			top_right[i] = top_mem[8+i];
		}
		break;
  case 13 :
		*top_left = top_mem[7];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[8+i];
			top_right[i] = top_mem[12+i];
		}
		break;
  case 14 :
		*top_left = top_mem[11];
		for (i = 0; i < 4; ++i) {
#pragma HLS unroll
			left[i] = blk[3+4*i];
			top[i] = top_mem[12+i];
			top_right[i] = y_top[16+i];
		}
		break;
  case 15 :
	  	break;
  }
  
  return 1;
}

static int VP8GetCostLuma4(int16_t tmp_levels[16]){
	int64_t test_R = 0;
	int x, y;
	for (y = 0; y < 4; ++y) {
#pragma HLS unroll
	  for (x = 0; x < 4; ++x) {
#pragma HLS unroll
		test_R += tmp_levels[y * 4 + x] * tmp_levels[y * 4 + x];
	  }
	}

	return test_R << 10;
}

static int PickBestMode(VP8ModeScore rd_tmp[10]){
#pragma HLS inline off
    int best_mode_0;
    int best_mode_1;
    int best_mode_2;
    int best_mode_3;
    int best_mode_4;
    int best_mode_5;
    int best_mode_6;
    int best_mode_7;
    int best_mode_8;

	if(rd_tmp[1].score < rd_tmp[0].score){
		best_mode_0 = 1;
	}
	else{
		best_mode_0 = 0;
	}

	if(rd_tmp[3].score < rd_tmp[2].score){
		best_mode_1 = 3;
	}
	else{
		best_mode_1 = 2;
	}

	if(rd_tmp[5].score < rd_tmp[4].score){
		best_mode_2 = 5;
	}
	else{
		best_mode_2 = 4;
	}

	if(rd_tmp[7].score < rd_tmp[6].score){
		best_mode_3 = 7;
	}
	else{
		best_mode_3 = 6;
	}

	if(rd_tmp[9].score < rd_tmp[8].score){
		best_mode_4 = 9;
	}
	else{
		best_mode_4 = 8;
	}

	if(rd_tmp[best_mode_1].score < rd_tmp[best_mode_0].score){
		best_mode_5 = best_mode_1;
	}
	else{
		best_mode_5 = best_mode_0;
	}

	if(rd_tmp[best_mode_3].score < rd_tmp[best_mode_2].score){
		best_mode_6 = best_mode_3;
	}
	else{
		best_mode_6 = best_mode_2;
	}

	if(rd_tmp[best_mode_6].score < rd_tmp[best_mode_5].score){
		best_mode_7 = best_mode_6;
	}
	else{
		best_mode_7 = best_mode_5;
	}

	if(rd_tmp[best_mode_4].score < rd_tmp[best_mode_7].score){
		best_mode_8 = best_mode_4;
	}
	else{
		best_mode_8 = best_mode_7;
	}

	return best_mode_8;
}

static void PickBestIntra4(VP8SegmentInfo* const dqm, uint8_t Yin[16*16], uint8_t Yout[16*16],
		VP8ModeScore* const rd, uint8_t y_left[16], uint8_t y_top_left, uint8_t y_top[20]) {
//#pragma HLS pipeline
//#pragma HLS ARRAY_PARTITION variable=Yout complete dim=1
//#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y_left complete dim=1
//#pragma HLS ARRAY_PARTITION variable=y_top complete dim=1

  const int lambda = dqm->lambda_i4_;
  const int tlambda = dqm->tlambda_;
  uint8_t best_blocks[16][16];
  uint8_t left[4], top_left, top[4], top_right[4];
  int i, j, n, i4_;
  uint8_t top_mem[16];
  uint8_t src[16][16];
  const uint16_t VP8Scan[16] = {  // Luma
    0 +  0 * 16,  4 +  0 * 16, 8 +  0 * 16, 12 +  0 * 16,
    0 +  4 * 16,  4 +  4 * 16, 8 +  4 * 16, 12 +  4 * 16,
    0 +  8 * 16,  4 +  8 * 16, 8 +  8 * 16, 12 +  8 * 16,
    0 + 12 * 16,  4 + 12 * 16, 8 + 12 * 16, 12 + 12 * 16,
  };

#pragma HLS ARRAY_PARTITION variable=left complete dim=1
#pragma HLS ARRAY_PARTITION variable=top complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_right complete dim=1
#pragma HLS ARRAY_PARTITION variable=top_mem complete dim=1
#pragma HLS ARRAY_PARTITION variable=VP8Scan complete dim=1
#pragma HLS ARRAY_PARTITION variable=src complete dim=0
#pragma HLS ARRAY_PARTITION variable=best_blocks complete dim=0

  top_left = y_top_left;
  for (i = 0; i < 4; i++) {
#pragma HLS unroll
	left[i] = y_left[i];
	top[i] = y_top[i];
	top_right[i] = y_top[4+i];
  }
	
  for(n = 0; n < 16; n++){
#pragma HLS unroll
    for(j = 0; j < 4; j++){
#pragma HLS unroll
	  for(i = 0; i < 4; i++){
#pragma HLS unroll
		  src[n][j * 4 + i] = Yin[VP8Scan[n] + j * 16 + i];
	  }
    }
  }

  rd->H = 211;  // '211' is the value of VP8BitCost(0, 145)
  rd->score = rd->H * dqm->lambda_mode_;

  VP8ModeScore rd_i4;
  int mode;
  int best_mode;
  VP8ModeScore rd_tmp[NUM_BMODES];
  uint8_t tmp_pred[NUM_BMODES][16];    // scratch buffer.
  int16_t tmp_levels[NUM_BMODES][16];
  uint8_t tmp_dst[NUM_BMODES][16];

#pragma HLS ARRAY_PARTITION variable=rd_tmp complete dim=1
#pragma HLS ARRAY_PARTITION variable=kWeightY complete dim=1
#pragma HLS ARRAY_PARTITION variable=VP8FixedCostsI4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_pred complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_dst complete dim=0
#pragma HLS ARRAY_PARTITION variable=tmp_levels complete dim=0

  for (i4_ = 0; i4_ < 16; i4_++){

    Intra4Preds_C(tmp_pred, left, top_left, top, top_right);

    for (mode = 0; mode < NUM_BMODES; mode++){
#pragma HLS unroll
      // Reconstruct
      rd_tmp[mode].nz =
          ReconstructIntra4(tmp_levels[mode], tmp_pred[mode], src[i4_], tmp_dst[mode], dqm->y1_) << i4_;

      // Compute RD-score
      rd_tmp[mode].D = GetSSE4x4(src[i4_], tmp_dst[mode]);
      rd_tmp[mode].SD = tlambda * Disto4x4_C(src[i4_], tmp_dst[mode], kWeightY);
      rd_tmp[mode].H = VP8FixedCostsI4[mode];
	  rd_tmp[mode].R = VP8GetCostLuma4(tmp_levels[mode]);

      SetRDScore_i4(lambda, &rd_tmp[mode]);
    }

    best_mode = PickBestMode(rd_tmp);

    CopyScore(&rd_i4, &rd_tmp[best_mode]);
	Copy_16_int16(rd->y_ac_levels[i4_], tmp_levels[best_mode]);
	Copy_16_uint8(best_blocks[i4_], tmp_dst[best_mode]);

    SetRDScore(dqm->lambda_mode_, &rd_i4);
    AddScore(rd, &rd_i4);
    rd->modes_i4[i4_] = best_mode;
    VP8IteratorRotateI4(y_left, y_top_left, y_top, i4_, top_mem,
    		best_blocks, left, &top_left, top, top_right);
  }

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

}

static int GetSSE16x8(const uint8_t* a, const uint8_t* b) {
  int count = 0;
  int y, x;
  for (y = 0; y < 8; ++y) {
//#pragma HLS unroll
    for (x = 0; x < 16; ++x) {
#pragma HLS unroll
      const int diff = (int)a[x + y * 16] - b[x + y * 16];
      count += diff * diff;
    }
  }
  return count;
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

static int64_t VP8GetCostUV(int16_t uv_levels[4 + 4][16]){
#pragma HLS inline off
	int64_t test_R = 0;
	int x, y;
	for (y = 0; y < 8; ++y) {
//#pragma HLS unroll
	  for (x = 0; x < 16; ++x) {
#pragma HLS unroll
	    test_R += uv_levels[y][x] * uv_levels[y][x];
	  }
	}
	return test_R << 10;
}

static void CopyUVLevel(int16_t dst[4 + 4][16], int16_t src[4 + 4][16]) {
  int i, j;
  for (j = 0; j < 8; ++j) {
#pragma HLS unroll
	  for (i = 0; i < 16; ++i) {
#pragma HLS unroll
		  dst[j][i] = src[j][i];
	  }
  }
}

static void CopyUVderr(int8_t dst[2][3], int8_t src[2][3]) {
  int i, j;
	for (j = 0; j < 2; ++j) {
#pragma HLS unroll
		for (i = 0; i < 3; ++i) {
#pragma HLS unroll
			dst[j][i] = src[j][i];
		}
	}
}

static void CopyUVout(uint8_t dst[8*16], uint8_t src[8*16]) {
  int j;
  for (j = 0; j < 128; ++j) {
#pragma HLS unroll
	  dst[j] = src[j];
  }
}

static void PickBestUV(VP8SegmentInfo* const dqm, uint8_t UVin[8*16], uint8_t UVout[8*16],
		VP8ModeScore* const rd, DError top_derr[1024], DError left_derr, uint8_t left_u[8],
		uint8_t top_u[8], uint8_t top_left_u, uint8_t left_v[8], uint8_t top_v[8],
		uint8_t top_left_v, int x, int y) {
//#pragma HLS pipeline
//#pragma HLS ARRAY_PARTITION variable=UVout complete dim=1
//#pragma HLS ARRAY_PARTITION variable=UVin complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rd->uv_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=rd->derr complete dim=0
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=2
//#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=3
//#pragma HLS ARRAY_PARTITION variable=left_derr complete dim=0
//#pragma HLS ARRAY_PARTITION variable=top_u complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_v complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_u complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_v complete dim=1

  const int lambda = dqm->lambda_uv_;
  uint8_t tmp_dst[8*16];  // scratch buffer
  int mode;
  int i, j, k;
  uint8_t UVPred[4][8*16];

#pragma HLS ARRAY_PARTITION variable=tmp_dst complete dim=1
#pragma HLS ARRAY_PARTITION variable=UVPred complete dim=0

  IntraChromaPreds_C(UVPred, left_u, top_u, top_left_u, left_v, top_v, top_left_v, x,  y);

  if(x == 0){
	for(j=0;j<2;j++){
#pragma HLS unroll
	  for(i=0;i<2;i++){
#pragma HLS unroll
		left_derr[j][i] = 0;
	  }
	}
  }

  for (mode = NUM_PRED_MODES - 1; mode >= 0 ; mode--) {
    VP8ModeScore rd_uv;
#pragma HLS ARRAY_PARTITION variable=rd_uv.uv_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd_uv.derr complete dim=0
    // Reconstruct
    rd_uv.nz = ReconstructUV(rd_uv.uv_levels, UVPred[mode], UVin, tmp_dst,
    		dqm->uv_, top_derr, left_derr, x, y, rd_uv.derr);

    // Compute RD-score
    rd_uv.D  = GetSSE16x8(UVin, tmp_dst);
    rd_uv.SD = 0;    // not calling TDisto here: it tends to flatten areas.
    rd_uv.H  = VP8FixedCostsUV[mode];
	rd_uv.R  = VP8GetCostUV(rd_uv.uv_levels);

    SetRDScore(lambda, &rd_uv);

    if (mode == NUM_PRED_MODES - 1 || rd_uv.score <= rd->score) {
      CopyScore(rd, &rd_uv);
      rd->mode_uv = mode;
	  CopyUVLevel(rd->uv_levels, rd_uv.uv_levels);
	  CopyUVderr(rd->derr, rd_uv.derr);
	  CopyUVout(UVout, tmp_dst);
    }
  }

  // store diffusion errors for next block
  StoreDiffusionErrors(top_derr, left_derr, x, rd);
}

void VP8Decimate_snap(uint8_t Yin[16*16], uint8_t Yout16[16*16], uint8_t Yout4[16*16],
		VP8SegmentInfo* const dqm, uint8_t UVin[8*16], uint8_t UVout[8*16], uint8_t* is_skipped,
		uint8_t left_y[16], uint8_t top_y[20], uint8_t top_left_y, uint8_t* mbtype, uint8_t left_u[8], 
		uint8_t top_u[8], uint8_t top_left_u,uint8_t left_v[8], uint8_t top_v[8], uint8_t top_left_v, 
		int x, int y, VP8ModeScore* const rd, DError top_derr[1024], DError left_derr) {
//#pragma HLS ARRAY_PARTITION variable=Yin complete dim=1
//#pragma HLS ARRAY_PARTITION variable=Yout16 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=Yout4 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=UVout complete dim=1
//#pragma HLS ARRAY_PARTITION variable=UVin complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rd->uv_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=rd->y_ac_levels complete dim=0
//#pragma HLS ARRAY_PARTITION variable=rd->y_dc_levels complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rd->modes_i4 complete dim=1
//#pragma HLS ARRAY_PARTITION variable=rd->derr complete dim=0
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->uv_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y1_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.sharpen_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.zthresh_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.bias_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.iq_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=dqm->y2_.q_ complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_y complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_y complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_u complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_u complete dim=1
//#pragma HLS ARRAY_PARTITION variable=left_v complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_v complete dim=1
//#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=2
//#pragma HLS ARRAY_PARTITION variable=top_derr complete dim=3
//#pragma HLS ARRAY_PARTITION variable=left_derr complete dim=0

  VP8ModeScore rd_i16;
  VP8ModeScore rd_i4;
  VP8ModeScore rd_uv;
  
  rd_i4.nz = 0;
  
#pragma HLS ARRAY_PARTITION variable=rd_i16.y_dc_levels complete dim=1
#pragma HLS ARRAY_PARTITION variable=rd_i16.y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd_i4.y_ac_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd_i4.modes_i4 complete dim=1
#pragma HLS ARRAY_PARTITION variable=rd_uv.uv_levels complete dim=0
#pragma HLS ARRAY_PARTITION variable=rd_uv.derr complete dim=0

  // We can perform predictions for Luma16x16 and Chroma8x8 already.
  // Luma4x4 predictions needs to be done as-we-go.

  PickBestIntra4(dqm, Yin, Yout4, &rd_i4, left_y, top_left_y, top_y);

  PickBestIntra16(Yin, Yout16, &rd_i16, dqm, left_y, top_y, top_left_y, x, y);

  PickBestUV(dqm, UVin, UVout, &rd_uv, top_derr, left_derr, left_u, top_u,
		  top_left_u, left_v, top_v, top_left_v, x,  y);

  if (rd_i4.score >= rd_i16.score) {
	*mbtype = 1;
    rd->nz = (rd_i16.nz & 0x0100ffff) | (rd_uv.nz & 0x00ff0000);
	Copy_16x16_int16(rd->y_ac_levels, rd_i16.y_ac_levels);
  }
  else{
    *mbtype = 0;
    rd->nz = (rd_i4.nz & 0x0000ffff) | (rd_uv.nz & 0x00ff0000);
	Copy_16x16_int16(rd->y_ac_levels, rd_i4.y_ac_levels);
  }

  CopyUVLevel(rd->uv_levels, rd_uv.uv_levels);
  //CopyUVderr(rd->derr, rd_uv.derr);//can be disable ?? 
  Copy_16_uint8(rd->modes_i4, rd_i4.modes_i4);
  Copy_16_int16(rd->y_dc_levels, rd_i16.y_dc_levels);

  rd->mode_i16 = rd_i16.mode_i16;
  rd->mode_uv = rd_uv.mode_uv;

  *is_skipped = (rd->nz == 0);
}

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
    return r;
  }
  return 1.;   // area is too dark to contribute meaningfully
}

static double VP8SSIMFromStatsClipped(const VP8DistoStats* const stats) {
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

static int NeedsFilter2_C(const uint8_t* p,
                                      int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step];
  const int p0 = p[-step], q0 = p[0];
  const int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * abs(p0 - q0) + abs(p1 - q1)) > t) return 0;
  return abs(p3 - p2) <= it && abs(p2 - p1) <= it &&
		 abs(p1 - p0) <= it && abs(q3 - q2) <= it &&
		 abs(q2 - q1) <= it && abs(q1 - q0) <= it;
}

static void DoFilter2_C(uint8_t* p, int step) {
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

static void DoFilter4_C(uint8_t* p, int step) {
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

static int Hev(const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (abs(p1 - p0) > thresh) || (abs(q1 - q0) > thresh);
}

static void FilterLoop24_C(uint8_t* p, int hstride, int vstride, int size,
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

void VP8StoreFilterStats_snap(VP8SegmentInfo* const dqm, LFStats_My lf_stats,
		uint8_t Yin[16*16], uint8_t Yout16[16*16], uint8_t Yout4[16*16],
		uint8_t UVin[8*16], uint8_t UVout[8*16], uint8_t mbtype, uint8_t skip) {
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

void VP8IteratorSaveBoundary_snap(DATA* data_it) {
  const uint8_t* const ysrc = data_it->mbtype ? data_it->Yout16 : data_it->Yout4;
  const uint8_t* const uvsrc = data_it->UVout;
  int i;
  uint8_t* top_y_tmp1 = data_it->top_y_tmp1;
  uint8_t* top_y_tmp2 = data_it->top_y_tmp2;
  uint8_t(*mem_top_y)[16] = data_it->mem_top_y;
  uint8_t(*mem_top_u)[8] = data_it->mem_top_u;
  uint8_t(*mem_top_v)[8] = data_it->mem_top_v;

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
  int tmp_p = (data_it->x + 1 < data_it->mb_w - 1) ?  data_it->x + 2 : (data_it->x < data_it->mb_w - 1) ? 0 : 1;

	for (i = 0; i < 16; ++i) {
		top_y_tmp2[i] = top_y_tmp1[i];
		top_y_tmp1[i] = mem_top_y[tmp_p][i];
	}

  if ((data_it->y == 0)&&(data_it->x < data_it->mb_w - 1)) {  // top
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
	if (data_it->x == data_it->mb_w - 2) {
		for (i = 0; i < 4; ++i) {
			data_it->top_y[16 + i] = data_it->top_y[15];
		}
	} else {
		for (i = 0; i < 4; ++i) {
			data_it->top_y[16 + i] = top_y_tmp1[i];
		}		
	}
  }
}


int VP8IteratorNext_snap(DATA* data_it) {
  int i, j;
  if (++data_it->x == data_it->mb_w) {
	  data_it->x = 0;
	  ++data_it->y;
	  for(j=0;j<2;j++){
	  	for(i=0;i<2;i++){
			data_it->left_derr[j][i] = 0;
	  	}
	  }
  }
  return (0 < --data_it->count_down);
}
