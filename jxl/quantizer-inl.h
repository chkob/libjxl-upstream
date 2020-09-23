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

#if defined(JXL_QUANTIZER_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef JXL_QUANTIZER_INL_H_
#undef JXL_QUANTIZER_INL_H_
#else
#define JXL_QUANTIZER_INL_H_
#endif

#include <stddef.h>

#include <hwy/highway.h>
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Vec;

template <class DF>
HWY_INLINE HWY_MAYBE_UNUSED Vec<DF> AdjustQuantBias(
    DF df, const size_t c, const Vec<DF> quant,
    const float* HWY_RESTRICT biases) {
  const hwy::HWY_NAMESPACE::Simd<int32_t, MaxLanes(df)> di;

  // Compare |quant|, keep sign bit for negating result.
  const auto kSign = BitCast(df, Set(di, INT32_MIN));
  const auto sign = And(quant, kSign);  // TODO(janwas): = abs ^ orig
  const auto abs_quant = AndNot(kSign, quant);

  // If |x| is 1, kZeroBias creates a different bias for each channel.
  // We're implementing the following:
  // if (quant == 0) return 0;
  // if (quant == 1) return biases[c];
  // if (quant == -1) return -biases[c];
  // return quant - biases[3] / quant;

  // Integer comparison is not helpful because Clang incurs bypass penalties
  // from unnecessarily mixing integer and float.
  const auto is_01 = abs_quant < Set(df, 1.125f);
  const auto not_0 = abs_quant > Zero(df);

  // Bitwise logic is faster than quant * biases[3].
  const auto one_bias = IfThenElseZero(not_0, Xor(Set(df, biases[c]), sign));

  // About 2E-5 worse than ReciprocalNR or division.
  const auto bias =
      NegMulAdd(Set(df, biases[3]), ApproximateReciprocal(quant), quant);

  return IfThenElse(is_01, one_bias, bias);
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // include guard
