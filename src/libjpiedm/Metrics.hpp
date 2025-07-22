/**
 * This file the header file for a C++ version of Keith Wannamaker's Java code
 * for parsing JPI EDM data. It has been modified by Michel Hoche-Mong to handle
 * more data elements from the EDM files.
 *
 * Original copyright below. Modifications copyright Michel Hoche-Mong, under
 * the same license as the original (Apache License, Version 2.0).
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
#include <string>
#include <optional>
#include <vector>
#include <memory>

#include "Metadata.hpp"

namespace jpi_edm {

class Metric {
public:
    enum class ScaleFactor {
        NONE,
        TEN,
        TEN_IF_GPH
    };

    Metric(int versionMask, int lowByteBit, std::optional<int> highByteBit, 
           const std::string& name, ScaleFactor scale = ScaleFactor::NONE)
        : versionMask_(versionMask)
        , lowByteBit_(lowByteBit)
        , highByteBit_(highByteBit)
        , name_(name)
        , scaleFactor_(scale) {}

    Metric(int versionMask, int lowByteBit, const std::string& name, 
           ScaleFactor scale = ScaleFactor::NONE)
        : Metric(versionMask, lowByteBit, std::nullopt, name, scale) {}

    int getVersionMask() const { return versionMask_; }
    int getLowByteBit() const { return lowByteBit_; }
    const std::optional<int>& getHighByteBit() const { return highByteBit_; }
    const std::string& getName() const { return name_; }
    ScaleFactor getScaleFactor() const { return scaleFactor_; }

private:
    int versionMask_;
    int lowByteBit_;
    std::optional<int> highByteBit_;
    std::string name_;
    ScaleFactor scaleFactor_;
};

class Metrics {
public:
    static std::map<int, Metric> getBitToMetricMap(const Metadata& metadata);

private:
    static const std::string UNSUPPORTED_METRIC;
    static const std::vector<Metric> m_metrics;
};

}
