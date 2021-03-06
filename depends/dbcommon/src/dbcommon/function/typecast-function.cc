/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "dbcommon/function/typecast-function.h"

#include <iomanip>
#include <string>
#include <typeinfo>

#include "dbcommon/common/vector-transformer.h"
#include "dbcommon/common/vector.h"
#include "dbcommon/common/vector/decimal-vector.h"
#include "dbcommon/common/vector/fixed-length-vector.h"
#include "dbcommon/common/vector/variable-length-vector.h"
#include "dbcommon/function/arithmetic-function.h"
#include "dbcommon/function/decimal-function.h"
#include "dbcommon/function/function.h"
#include "dbcommon/type/decimal.h"
#include "dbcommon/type/type-util.h"
#include "dbcommon/utils/int-util.h"
#include "dbcommon/utils/macro.h"
#include "dbcommon/utils/string-util.h"

namespace dbcommon {
template <typename TP>
Datum integer_to_text(Datum *params, uint64_t size) {
  assert(size == 2 && "invalid input");
  Object *para = params[1];

  auto intToText = [](TP val, char *&bufferPtr) -> uint64_t {
    uint64_t unsigned_val;
    uint64_t isNegative = val < 0;
    if (isNegative) {
      *bufferPtr++ = '-';
      if (val == INT16_MIN || val == INT32_MIN || val == INT64_MIN) {
        unsigned_val = -(val + 1);
        unsigned_val++;
      } else {
        unsigned_val = -val;
      }
    } else {
      unsigned_val = val;
    }

    auto numOfDigit = getNumOfDigit<uint64_t>(unsigned_val);
    auto bufferBackPtr = bufferPtr + numOfDigit;  // hint at using register
    do {
      auto old = unsigned_val;
      unsigned_val /= 10;
      *--bufferBackPtr = old - unsigned_val * 10 + '0';
    } while (unsigned_val > 0);

    bufferPtr += numOfDigit;
    return numOfDigit + isNegative;
  };

  if (dynamic_cast<Vector *>(para)) {
    Vector *retVector = params[0];
    Vector *srcVector = params[1];

    FixedSizeTypeVectorRawData<TP> src(srcVector);
    retVector->resize(src.plainSize, src.sel, src.nulls);
    VariableSizeTypeVectorRawData ret(retVector);

    auto retBuffer = retVector->getValueBuffer();
    retBuffer->resize(src.plainSize * 21);
    char *bufferPtr = retBuffer->data();

    auto inttypeTotext = [&](uint64_t plainIdx) {
      TP val = src.values[plainIdx];
      ret.lengths[plainIdx] = intToText(val, bufferPtr);
    };
    dbcommon::transformVector(ret.plainSize, ret.sel, ret.nulls, inttypeTotext);
    retBuffer->resize(bufferPtr - retBuffer->data());
    retVector->computeValPtrs();

  } else {
    Scalar *retScalar = params[0];
    Scalar *srcScalar = params[1];
    if (srcScalar->isnull) {
      retScalar->isnull = true;
    } else {
      TP val = srcScalar->value;
      char *bufferPtr = retScalar->allocateValue(22);
      retScalar->isnull = false;
      retScalar->length = intToText(val, bufferPtr);
    }
  }
  return params[0];
}

template <typename TP>
Datum floattype_to_text(Datum *params, uint64_t size) {
  auto floatToText = [](ByteBuffer &buf, TP val) -> text {
    uint64_t len = 0;
    char temp[30];
    float f;
    if (std::is_same<TP, float>::value) {
      len = std::snprintf(temp, sizeof(temp), "%g", val);
    } else {
      len = std::snprintf(temp, sizeof(temp), "%.15g", val);
    }
    buf.resize(buf.size() + len);
    char *ret = const_cast<char *>(buf.tail() - len);
    strncpy(ret, temp, len);
    return text(nullptr, len);
  };

  return one_param_bind<text, TP>(params, size, floatToText);
}

Datum decimal_to_text(Datum *params, uint64_t size) {
  auto decimalToText = [](ByteBuffer &buf, DecimalVar val) -> text {
    uint64_t len = 0, scale = 0;
    __uint128_t unsigned_val;
    __int128_t srcVal = val.highbits;
    srcVal = (srcVal << 64) + val.lowbits;

    uint64_t low_min = 0;
    int64_t high_min = INT64_MIN;
    __int128_t MIN = INT64_MIN;
    MIN = (MIN << 64) + low_min;

    bool isNegative = srcVal < 0;
    if (isNegative) {
      if (srcVal == MIN) {
        unsigned_val = -(srcVal + 1);
        unsigned_val++;
      } else {
        unsigned_val = -srcVal;
      }
    } else {
      unsigned_val = srcVal;
    }
    auto numOfDigit = getNumOfDigit<__uint128_t>(unsigned_val);
    if (srcVal == 0) {
      numOfDigit = 1 + val.scale;
    }
    if (val.scale == 0) {
      len += (numOfDigit + isNegative);
    } else {
      len += (numOfDigit + 1 + isNegative);
    }
    buf.resize(buf.size() + len);
    char *ret = const_cast<char *>(buf.tail());
    do {
      auto old = unsigned_val;
      unsigned_val = unsigned_val / 10;
      *--ret = old - unsigned_val * 10 + '0';
      scale++;
    } while (unsigned_val > 0 && scale != val.scale);

    if (val.scale != 0) {
      *--ret = '.';
      do {
        auto old = unsigned_val;
        unsigned_val = unsigned_val / 10;
        *--ret = old - unsigned_val * 10 + '0';
      } while (unsigned_val > 0);
    }
    if (isNegative) {
      *--ret = '-';
    }
    return text(nullptr, len);
  };

  return one_param_bind<text, DecimalVar>(params, size, decimalToText);
}

Datum text_to_char(Datum *params, uint64_t size) {
  auto textToChar = [](ByteBuffer &buf, text src) -> int8_t {
    int8_t ret;
    int32_t tmpLen = utf8_mblen(src.val);
    if (tmpLen == 1) {
      ret = src.val[0];
    } else {
      ret = '\0';
    }
    return ret;
  };
  return one_param_bind<int8_t, text>(params, size, textToChar);
}

Datum int4_to_char(Datum *params, uint64_t size) {
  auto intToChar = [](ByteBuffer &buf, int32_t src) -> int8_t {
    int8_t ret;
    if (src > SCHAR_MAX || src < SCHAR_MIN) {
      LOG_ERROR(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE, "\"char\" out of range");
    } else {
      ret = (int8_t)(src);
    }
    return ret;
  };
  return one_param_bind<int8_t, int32_t>(params, size, intToChar);
}

Datum int2_to_text(Datum *params, uint64_t size) {
  return integer_to_text<int16_t>(params, size);
}
Datum int4_to_text(Datum *params, uint64_t size) {
  return integer_to_text<int32_t>(params, size);
}
Datum int8_to_text(Datum *params, uint64_t size) {
  return integer_to_text<int64_t>(params, size);
}
Datum float4_to_text(Datum *params, uint64_t size) {
  return floattype_to_text<float>(params, size);
}
Datum float8_to_text(Datum *params, uint64_t size) {
  return floattype_to_text<double>(params, size);
}

}  // namespace dbcommon
