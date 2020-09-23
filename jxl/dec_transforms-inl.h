// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(JXL_DEC_TRANSFORMS_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef JXL_DEC_TRANSFORMS_INL_H_
#undef JXL_DEC_TRANSFORMS_INL_H_
#else
#define JXL_DEC_TRANSFORMS_INL_H_
#endif

#include <stddef.h>

#include <hwy/highway.h>

#include "jxl/ac_strategy.h"
#include "jxl/coeff_order_fwd.h"
#include "jxl/dct_scales.h"
#include "jxl/dec_dct-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

template <size_t ROWS, size_t COLS>
struct DoIDCT {
  template <typename From, typename To>
  void operator()(const From& from, const To& to) {
    ComputeScaledIDCT<ROWS, COLS>()(from, to);
  }
};

template <size_t N>
struct DoIDCT<N, N> {
  template <typename From, typename To>
  void operator()(const From& from, const To& to) const {
    ComputeTransposedScaledIDCT<N>()(from, to);
  }
};

// Inverse of ReinterpretingDCT.
template <size_t DCT_ROWS, size_t DCT_COLS, size_t LF_ROWS, size_t LF_COLS,
          size_t ROWS, size_t COLS>
HWY_INLINE void ReinterpretingIDCT(const float* input,
                                   const size_t input_stride, float* output,
                                   const size_t output_stride) {
  // TODO(veluca): avoid copy using special FromBlock adapters.
  HWY_ALIGN float block[ROWS * COLS] = {};
  if (ROWS < COLS) {
    for (size_t y = 0; y < LF_ROWS; y++) {
      for (size_t x = 0; x < LF_COLS; x++) {
        block[y * COLS + x] = input[y * input_stride + x] *
                              DCTTotalResampleScale<DCT_ROWS, ROWS>(y) *
                              DCTTotalResampleScale<DCT_COLS, COLS>(x);
      }
    }
  } else {
    for (size_t y = 0; y < LF_COLS; y++) {
      for (size_t x = 0; x < LF_ROWS; x++) {
        block[y * ROWS + x] = input[y * input_stride + x] *
                              DCTTotalResampleScale<DCT_COLS, COLS>(y) *
                              DCTTotalResampleScale<DCT_ROWS, ROWS>(x);
      }
    }
  }

  constexpr size_t IN_ROWS = CoefficientRows(ROWS, COLS);
  constexpr size_t IN_COLS = CoefficientColumns(ROWS, COLS);
  DoIDCT<ROWS, COLS>()(FromBlock(IN_ROWS, IN_COLS, block),
                       ToBlock(ROWS, COLS, block));

  for (size_t y = 0; y < ROWS; y++) {
    for (size_t x = 0; x < COLS; x++) {
      output[y * output_stride + x] = block[y * COLS + x];
    }
  }
}

template <size_t S>
void IDCT2TopBlock(const float* block, size_t stride_out, float* out) {
  static_assert(kBlockDim % S == 0, "S should be a divisor of kBlockDim");
  static_assert(S % 2 == 0, "S should be even");
  float temp[kDCTBlockSize];
  constexpr size_t num_2x2 = S / 2;
  for (size_t y = 0; y < num_2x2; y++) {
    for (size_t x = 0; x < num_2x2; x++) {
      float c00 = block[y * kBlockDim + x];
      float c01 = block[y * kBlockDim + num_2x2 + x];
      float c10 = block[(y + num_2x2) * kBlockDim + x];
      float c11 = block[(y + num_2x2) * kBlockDim + num_2x2 + x];
      float r00 = c00 + c01 + c10 + c11;
      float r01 = c00 + c01 - c10 - c11;
      float r10 = c00 - c01 + c10 - c11;
      float r11 = c00 - c01 - c10 + c11;
      temp[y * 2 * kBlockDim + x * 2] = r00;
      temp[y * 2 * kBlockDim + x * 2 + 1] = r01;
      temp[(y * 2 + 1) * kBlockDim + x * 2] = r10;
      temp[(y * 2 + 1) * kBlockDim + x * 2 + 1] = r11;
    }
  }
  for (size_t y = 0; y < S; y++) {
    for (size_t x = 0; x < S; x++) {
      out[y * stride_out + x] = temp[y * kBlockDim + x];
    }
  }
}

void AFVIDCT4x4(const float* JXL_RESTRICT coeffs, float* JXL_RESTRICT pixels) {
  HWY_ALIGN static constexpr float k4x4AFVBasis[16][16] = {
      {
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
          0.25,
      },
      {
          0.876902929799142f,
          0.2206518106944235f,
          -0.10140050393753763f,
          -0.1014005039375375f,
          0.2206518106944236f,
          -0.10140050393753777f,
          -0.10140050393753772f,
          -0.10140050393753763f,
          -0.10140050393753758f,
          -0.10140050393753769f,
          -0.1014005039375375f,
          -0.10140050393753768f,
          -0.10140050393753768f,
          -0.10140050393753759f,
          -0.10140050393753763f,
          -0.10140050393753741f,
      },
      {
          0.0,
          0.0,
          0.40670075830260755f,
          0.44444816619734445f,
          0.0,
          0.0,
          0.19574399372042936f,
          0.2929100136981264f,
          -0.40670075830260716f,
          -0.19574399372042872f,
          0.0,
          0.11379074460448091f,
          -0.44444816619734384f,
          -0.29291001369812636f,
          -0.1137907446044814f,
          0.0,
      },
      {
          0.0,
          0.0,
          -0.21255748058288748f,
          0.3085497062849767f,
          0.0,
          0.4706702258572536f,
          -0.1621205195722993f,
          0.0,
          -0.21255748058287047f,
          -0.16212051957228327f,
          -0.47067022585725277f,
          -0.1464291867126764f,
          0.3085497062849487f,
          0.0,
          -0.14642918671266536f,
          0.4251149611657548f,
      },
      {
          0.0,
          -0.7071067811865474f,
          0.0,
          0.0,
          0.7071067811865476f,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0.0,
      },
      {
          -0.4105377591765233f,
          0.6235485373547691f,
          -0.06435071657946274f,
          -0.06435071657946266f,
          0.6235485373547694f,
          -0.06435071657946284f,
          -0.0643507165794628f,
          -0.06435071657946274f,
          -0.06435071657946272f,
          -0.06435071657946279f,
          -0.06435071657946266f,
          -0.06435071657946277f,
          -0.06435071657946277f,
          -0.06435071657946273f,
          -0.06435071657946274f,
          -0.0643507165794626f,
      },
      {
          0.0,
          0.0,
          -0.4517556589999482f,
          0.15854503551840063f,
          0.0,
          -0.04038515160822202f,
          0.0074182263792423875f,
          0.39351034269210167f,
          -0.45175565899994635f,
          0.007418226379244351f,
          0.1107416575309343f,
          0.08298163094882051f,
          0.15854503551839705f,
          0.3935103426921022f,
          0.0829816309488214f,
          -0.45175565899994796f,
      },
      {
          0.0,
          0.0,
          -0.304684750724869f,
          0.5112616136591823f,
          0.0,
          0.0,
          -0.290480129728998f,
          -0.06578701549142804f,
          0.304684750724884f,
          0.2904801297290076f,
          0.0,
          -0.23889773523344604f,
          -0.5112616136592012f,
          0.06578701549142545f,
          0.23889773523345467f,
          0.0,
      },
      {
          0.0,
          0.0,
          0.3017929516615495f,
          0.25792362796341184f,
          0.0,
          0.16272340142866204f,
          0.09520022653475037f,
          0.0,
          0.3017929516615503f,
          0.09520022653475055f,
          -0.16272340142866173f,
          -0.35312385449816297f,
          0.25792362796341295f,
          0.0,
          -0.3531238544981624f,
          -0.6035859033230976f,
      },
      {
          0.0,
          0.0,
          0.40824829046386274f,
          0.0,
          0.0,
          0.0,
          0.0,
          -0.4082482904638628f,
          -0.4082482904638635f,
          0.0,
          0.0,
          -0.40824829046386296f,
          0.0,
          0.4082482904638634f,
          0.408248290463863f,
          0.0,
      },
      {
          0.0,
          0.0,
          0.1747866975480809f,
          0.0812611176717539f,
          0.0,
          0.0,
          -0.3675398009862027f,
          -0.307882213957909f,
          -0.17478669754808135f,
          0.3675398009862011f,
          0.0,
          0.4826689115059883f,
          -0.08126111767175039f,
          0.30788221395790305f,
          -0.48266891150598584f,
          0.0,
      },
      {
          0.0,
          0.0,
          -0.21105601049335784f,
          0.18567180916109802f,
          0.0,
          0.0,
          0.49215859013738733f,
          -0.38525013709251915f,
          0.21105601049335806f,
          -0.49215859013738905f,
          0.0,
          0.17419412659916217f,
          -0.18567180916109904f,
          0.3852501370925211f,
          -0.1741941265991621f,
          0.0,
      },
      {
          0.0,
          0.0,
          -0.14266084808807264f,
          -0.3416446842253372f,
          0.0,
          0.7367497537172237f,
          0.24627107722075148f,
          -0.08574019035519306f,
          -0.14266084808807344f,
          0.24627107722075137f,
          0.14883399227113567f,
          -0.04768680350229251f,
          -0.3416446842253373f,
          -0.08574019035519267f,
          -0.047686803502292804f,
          -0.14266084808807242f,
      },
      {
          0.0,
          0.0,
          -0.13813540350758585f,
          0.3302282550303788f,
          0.0,
          0.08755115000587084f,
          -0.07946706605909573f,
          -0.4613374887461511f,
          -0.13813540350758294f,
          -0.07946706605910261f,
          0.49724647109535086f,
          0.12538059448563663f,
          0.3302282550303805f,
          -0.4613374887461554f,
          0.12538059448564315f,
          -0.13813540350758452f,
      },
      {
          0.0,
          0.0,
          -0.17437602599651067f,
          0.0702790691196284f,
          0.0,
          -0.2921026642334881f,
          0.3623817333531167f,
          0.0,
          -0.1743760259965108f,
          0.36238173335311646f,
          0.29210266423348785f,
          -0.4326608024727445f,
          0.07027906911962818f,
          0.0,
          -0.4326608024727457f,
          0.34875205199302267f,
      },
      {
          0.0,
          0.0,
          0.11354987314994337f,
          -0.07417504595810355f,
          0.0,
          0.19402893032594343f,
          -0.435190496523228f,
          0.21918684838857466f,
          0.11354987314994257f,
          -0.4351904965232251f,
          0.5550443808910661f,
          -0.25468277124066463f,
          -0.07417504595810233f,
          0.2191868483885728f,
          -0.25468277124066413f,
          0.1135498731499429f,
      },
  };

  const HWY_CAPPED(float, 16) d;
  for (size_t i = 0; i < 16; i += Lanes(d)) {
    auto pixel = Zero(d);
    for (size_t j = 0; j < 16; j++) {
      auto cf = Set(d, coeffs[j]);
      auto basis = Load(d, k4x4AFVBasis[j] + i);
      pixel = MulAdd(cf, basis, pixel);
    }
    Store(pixel, d, pixels + i);
  }
}

template <size_t afv_kind>
void AFVTransformToPixels(const float* JXL_RESTRICT coefficients,
                          float* JXL_RESTRICT pixels, size_t pixels_stride) {
  size_t afv_x = afv_kind & 1;
  size_t afv_y = afv_kind / 2;
  float dcs[3] = {};
  float block00 = coefficients[0];
  float block01 = coefficients[1];
  float block10 = coefficients[8];
  dcs[0] = (block00 + block10 + block01) * 4.0f;
  dcs[1] = (block00 + block10 - block01);
  dcs[2] = block00 - block10;
  // IAFV: (even, even) positions.
  HWY_ALIGN float coeff[4 * 4];
  coeff[0] = dcs[0];
  for (size_t iy = 0; iy < 4; iy++) {
    for (size_t ix = 0; ix < 4; ix++) {
      if (ix == 0 && iy == 0) continue;
      coeff[iy * 4 + ix] = coefficients[iy * 2 * 8 + ix * 2];
    }
  }
  HWY_ALIGN float block[4 * 8];
  AFVIDCT4x4(coeff, block);
  for (size_t iy = 0; iy < 4; iy++) {
    for (size_t ix = 0; ix < 4; ix++) {
      pixels[(iy + afv_y * 4) * pixels_stride + afv_x * 4 + ix] =
          block[(afv_y == 1 ? 3 - iy : iy) * 4 + (afv_x == 1 ? 3 - ix : ix)];
    }
  }
  // IDCT4x4 in (odd, even) positions.
  block[0] = dcs[1];
  for (size_t iy = 0; iy < 4; iy++) {
    for (size_t ix = 0; ix < 4; ix++) {
      if (ix == 0 && iy == 0) continue;
      block[iy * 4 + ix] = coefficients[iy * 2 * 8 + ix * 2 + 1];
    }
  }
  ComputeTransposedScaledIDCT<4>()(
      FromBlock(4, 4, block),
      ToLines(pixels + afv_y * 4 * pixels_stride + (afv_x == 1 ? 0 : 4),
              pixels_stride));
  // IDCT4x8.
  block[0] = dcs[2];
  for (size_t iy = 0; iy < 4; iy++) {
    for (size_t ix = 0; ix < 8; ix++) {
      if (ix == 0 && iy == 0) continue;
      block[iy * 8 + ix] = coefficients[(1 + iy * 2) * 8 + ix];
    }
  }
  ComputeScaledIDCT<4, 8>()(
      FromBlock(4, 8, block),
      ToLines(pixels + (afv_y == 1 ? 0 : 4) * pixels_stride, pixels_stride));
}

HWY_MAYBE_UNUSED void TransformToPixels(const AcStrategy::Type strategy,
                                        const float* JXL_RESTRICT coefficients,
                                        float* JXL_RESTRICT pixels,
                                        size_t pixels_stride) {
  using Type = AcStrategy::Type;
  switch (strategy) {
    case Type::IDENTITY: {
      PROFILER_ZONE("IDCT Identity");
      float dcs[4] = {};
      float block00 = coefficients[0];
      float block01 = coefficients[1];
      float block10 = coefficients[8];
      float block11 = coefficients[9];
      dcs[0] = block00 + block01 + block10 + block11;
      dcs[1] = block00 + block01 - block10 - block11;
      dcs[2] = block00 - block01 + block10 - block11;
      dcs[3] = block00 - block01 - block10 + block11;
      for (size_t y = 0; y < 2; y++) {
        for (size_t x = 0; x < 2; x++) {
          float block_dc = dcs[y * 2 + x];
          float residual_sum = 0;
          for (size_t iy = 0; iy < 4; iy++) {
            for (size_t ix = 0; ix < 4; ix++) {
              if (ix == 0 && iy == 0) continue;
              residual_sum += coefficients[(y + iy * 2) * 8 + x + ix * 2];
            }
          }
          pixels[(4 * y + 1) * pixels_stride + 4 * x + 1] =
              block_dc - residual_sum * (1.0f / 16);
          for (size_t iy = 0; iy < 4; iy++) {
            for (size_t ix = 0; ix < 4; ix++) {
              if (ix == 1 && iy == 1) continue;
              pixels[(y * 4 + iy) * pixels_stride + x * 4 + ix] =
                  coefficients[(y + iy * 2) * 8 + x + ix * 2] +
                  pixels[(4 * y + 1) * pixels_stride + 4 * x + 1];
            }
          }
          pixels[y * 4 * pixels_stride + x * 4] =
              coefficients[(y + 2) * 8 + x + 2] +
              pixels[(4 * y + 1) * pixels_stride + 4 * x + 1];
        }
      }
      break;
    }
    case Type::DCT8X4: {
      PROFILER_ZONE("IDCT 8x4");
      float dcs[2] = {};
      float block0 = coefficients[0];
      float block1 = coefficients[8];
      dcs[0] = block0 + block1;
      dcs[1] = block0 - block1;
      for (size_t x = 0; x < 2; x++) {
        HWY_ALIGN float block[4 * 8];
        block[0] = dcs[x];
        for (size_t iy = 0; iy < 4; iy++) {
          for (size_t ix = 0; ix < 8; ix++) {
            if (ix == 0 && iy == 0) continue;
            block[iy * 8 + ix] = coefficients[(x + iy * 2) * 8 + ix];
          }
        }
        ComputeScaledIDCT<8, 4>()(FromBlock(4, 8, block),
                                  ToLines(pixels + x * 4, pixels_stride));
      }
      break;
    }
    case Type::DCT4X8: {
      PROFILER_ZONE("IDCT 4x8");
      float dcs[2] = {};
      float block0 = coefficients[0];
      float block1 = coefficients[8];
      dcs[0] = block0 + block1;
      dcs[1] = block0 - block1;
      for (size_t y = 0; y < 2; y++) {
        HWY_ALIGN float block[4 * 8];
        block[0] = dcs[y];
        for (size_t iy = 0; iy < 4; iy++) {
          for (size_t ix = 0; ix < 8; ix++) {
            if (ix == 0 && iy == 0) continue;
            block[iy * 8 + ix] = coefficients[(y + iy * 2) * 8 + ix];
          }
        }
        ComputeScaledIDCT<4, 8>()(
            FromBlock(4, 8, block),
            ToLines(pixels + y * 4 * pixels_stride, pixels_stride));
      }
      break;
    }
    case Type::DCT4X4: {
      PROFILER_ZONE("IDCT 4");
      float dcs[4] = {};
      float block00 = coefficients[0];
      float block01 = coefficients[1];
      float block10 = coefficients[8];
      float block11 = coefficients[9];
      dcs[0] = block00 + block01 + block10 + block11;
      dcs[1] = block00 + block01 - block10 - block11;
      dcs[2] = block00 - block01 + block10 - block11;
      dcs[3] = block00 - block01 - block10 + block11;
      for (size_t y = 0; y < 2; y++) {
        for (size_t x = 0; x < 2; x++) {
          HWY_ALIGN float block[4 * 4];
          block[0] = dcs[y * 2 + x];
          for (size_t iy = 0; iy < 4; iy++) {
            for (size_t ix = 0; ix < 4; ix++) {
              if (ix == 0 && iy == 0) continue;
              block[iy * 4 + ix] = coefficients[(y + iy * 2) * 8 + x + ix * 2];
            }
          }
          ComputeTransposedScaledIDCT<4>()(
              FromBlock(4, 4, block),
              ToLines(pixels + y * 4 * pixels_stride + x * 4, pixels_stride));
        }
      }
      break;
    }
    case Type::DCT2X2: {
      PROFILER_ZONE("IDCT 2");
      HWY_ALIGN float coeffs[kDCTBlockSize];
      memcpy(coeffs, coefficients, sizeof(float) * kDCTBlockSize);
      IDCT2TopBlock<2>(coeffs, kBlockDim, coeffs);
      IDCT2TopBlock<4>(coeffs, kBlockDim, coeffs);
      IDCT2TopBlock<8>(coeffs, kBlockDim, coeffs);
      for (size_t y = 0; y < kBlockDim; y++) {
        for (size_t x = 0; x < kBlockDim; x++) {
          pixels[y * pixels_stride + x] = coeffs[y * kBlockDim + x];
        }
      }
      break;
    }
    case Type::DCT16X16: {
      PROFILER_ZONE("IDCT 16");
      ComputeTransposedScaledIDCT<2 * kBlockDim>()(
          FromBlock(2 * kBlockDim, 2 * kBlockDim, coefficients),
          ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT16X8: {
      PROFILER_ZONE("IDCT 16x8");
      ComputeScaledIDCT<16, 8>()(FromBlock(8, 16, coefficients),
                                 ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT8X16: {
      PROFILER_ZONE("IDCT 8x16");
      ComputeScaledIDCT<8, 16>()(FromBlock(8, 16, coefficients),
                                 ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT32X8: {
      PROFILER_ZONE("IDCT 32x8");
      ComputeScaledIDCT<32, 8>()(FromBlock(8, 32, coefficients),
                                 ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT8X32: {
      PROFILER_ZONE("IDCT 8x32");
      ComputeScaledIDCT<8, 32>()(FromBlock(8, 32, coefficients),
                                 ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT32X16: {
      PROFILER_ZONE("IDCT 32x16");
      ComputeScaledIDCT<32, 16>()(FromBlock(16, 32, coefficients),
                                  ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT16X32: {
      PROFILER_ZONE("IDCT 16x32");
      ComputeScaledIDCT<16, 32>()(FromBlock(16, 32, coefficients),
                                  ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT32X32: {
      PROFILER_ZONE("IDCT 32");
      ComputeTransposedScaledIDCT<4 * kBlockDim>()(
          FromBlock(4 * kBlockDim, 4 * kBlockDim, coefficients),
          ToLines(pixels, pixels_stride));
      break;
    }
    case Type::DCT: {
      PROFILER_ZONE("IDCT 8");
      ComputeTransposedScaledIDCT<kBlockDim>()(
          FromBlock(kBlockDim, kBlockDim, coefficients),
          ToLines(pixels, pixels_stride));
      break;
    }
    case Type::AFV0: {
      PROFILER_ZONE("IAFV0");
      AFVTransformToPixels<0>(coefficients, pixels, pixels_stride);
      break;
    }
    case Type::AFV1: {
      PROFILER_ZONE("IAFV1");
      AFVTransformToPixels<1>(coefficients, pixels, pixels_stride);
      break;
    }
    case Type::AFV2: {
      PROFILER_ZONE("IAFV2");
      AFVTransformToPixels<2>(coefficients, pixels, pixels_stride);
      break;
    }
    case Type::AFV3: {
      PROFILER_ZONE("IAFV3");
      AFVTransformToPixels<3>(coefficients, pixels, pixels_stride);
      break;
    }
    case Type::kNumValidStrategies:
      JXL_ABORT("Invalid strategy");
  }
}

HWY_MAYBE_UNUSED void DCFromLowestFrequencies(const AcStrategy::Type strategy,
                                              const float* block, float* dc,
                                              size_t dc_stride) {
  using Type = AcStrategy::Type;
  switch (strategy) {
    case Type::DCT16X8: {
      ReinterpretingIDCT</*DCT_ROWS=*/2 * kBlockDim, /*DCT_COLS=*/kBlockDim,
                         /*LF_ROWS=*/2, /*LF_COLS=*/1, /*ROWS=*/2, /*COLS=*/1>(
          block, 2 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT8X16: {
      ReinterpretingIDCT</*DCT_ROWS=*/kBlockDim, /*DCT_COLS=*/2 * kBlockDim,
                         /*LF_ROWS=*/1, /*LF_COLS=*/2, /*ROWS=*/1, /*COLS=*/2>(
          block, 2 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT16X16: {
      ReinterpretingIDCT</*DCT_ROWS=*/2 * kBlockDim, /*DCT_COLS=*/2 * kBlockDim,
                         /*LF_ROWS=*/2, /*LF_COLS=*/2, /*ROWS=*/2, /*COLS=*/2>(
          block, 2 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT32X8: {
      ReinterpretingIDCT</*DCT_ROWS=*/4 * kBlockDim, /*DCT_COLS=*/kBlockDim,
                         /*LF_ROWS=*/4, /*LF_COLS=*/1, /*ROWS=*/4, /*COLS=*/1>(
          block, 4 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT8X32: {
      ReinterpretingIDCT</*DCT_ROWS=*/kBlockDim, /*DCT_COLS=*/4 * kBlockDim,
                         /*LF_ROWS=*/1, /*LF_COLS=*/4, /*ROWS=*/1, /*COLS=*/4>(
          block, 4 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT32X16: {
      ReinterpretingIDCT</*DCT_ROWS=*/4 * kBlockDim, /*DCT_COLS=*/2 * kBlockDim,
                         /*LF_ROWS=*/4, /*LF_COLS=*/2, /*ROWS=*/4, /*COLS=*/2>(
          block, 4 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT16X32: {
      ReinterpretingIDCT</*DCT_ROWS=*/2 * kBlockDim, /*DCT_COLS=*/4 * kBlockDim,
                         /*LF_ROWS=*/2, /*LF_COLS=*/4, /*ROWS=*/2, /*COLS=*/4>(
          block, 4 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT32X32: {
      ReinterpretingIDCT</*DCT_ROWS=*/4 * kBlockDim, /*DCT_COLS=*/4 * kBlockDim,
                         /*LF_ROWS=*/4, /*LF_COLS=*/4, /*ROWS=*/4, /*COLS=*/4>(
          block, 4 * kBlockDim, dc, dc_stride);
      break;
    }
    case Type::DCT:
    case Type::DCT2X2:
    case Type::DCT4X4:
    case Type::DCT4X8:
    case Type::DCT8X4:
    case Type::AFV0:
    case Type::AFV1:
    case Type::AFV2:
    case Type::AFV3:
    case Type::IDENTITY:
      dc[0] = block[0];
      break;
    case Type::kNumValidStrategies:
      JXL_ABORT("Invalid strategy");
  }
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // include guard
