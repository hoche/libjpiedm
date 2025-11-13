/**
 * This file the header file for a C++ version of Keith Wannamaker's Java code
 * for parsing JPI EDM data. It has been modified by Michel Hoche-Mong to handle
 * more data elements from the EDM files.
 *
 * Original copyright below. Modifications copyright Michel Hoche-Mong,
 * under the Creative Commons CC-BY-4.0 license.
 * SPDX-License-Identifier: CC-BY-NC-4.0
 */

/**
 *    Copyright 2015 Keith Wannamaker
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Metadata.hpp"
#include "MetricId.hpp"

namespace jpi_edm {

class Metric
{
  public:
    enum ScaleFactor { NONE, TEN, TEN_IF_GPH };

    enum InitialValue {
        NEGATIVE_TEN = -10,
        NEGATIVE_ONE = -1,
        ZERO = 0x00,
        DEFAULT = 0xF0,
    };

    Metric(int versionMask, int lowByteBit, std::optional<int> highByteBit, MetricId mid, const std::string &shortName,
           const std::string &name, ScaleFactor scale = ScaleFactor::NONE,
           InitialValue initialValue = InitialValue::DEFAULT)
        : m_versionMask(versionMask), m_lowByteBitIdx(lowByteBit), m_highByteBitIdx(highByteBit), m_metricId(mid),
          m_shortName(shortName), m_name(name), m_scaleFactor(scale), m_initialValue(static_cast<float>(initialValue))
    {
    }

    Metric(int versionMask, int lowByteBit, MetricId mid, const std::string &shortName, const std::string &name,
           ScaleFactor scale = ScaleFactor::NONE, InitialValue initialValue = InitialValue::DEFAULT)
        : Metric(versionMask, lowByteBit, std::nullopt, mid, shortName, name, scale, initialValue)
    {
    }

    MetricId getMetricId() const { return m_metricId; }
    int getVersionMask() const { return m_versionMask; }
    int getLowByteBitIdx() const { return m_lowByteBitIdx; }
    const std::optional<int> &getHighByteBitIdx() const { return m_highByteBitIdx; }
    const std::string &getShortName() const { return m_shortName; }
    const std::string &getName() const { return m_name; }
    ScaleFactor getScaleFactor() const { return m_scaleFactor; }
    float getInitialValue() const { return m_initialValue; }

  private:
    int m_versionMask;
    int m_lowByteBitIdx;
    std::optional<int> m_highByteBitIdx;
    MetricId m_metricId;
    std::string m_shortName;
    std::string m_name;
    ScaleFactor m_scaleFactor;
    float m_initialValue;
};

class Metrics
{
  public:
    static std::map<int, Metric> getBitToMetricMap(const EDMVersion &edmversion);

  private:
    static const std::vector<Metric> m_metrics;
};

} // namespace jpi_edm
