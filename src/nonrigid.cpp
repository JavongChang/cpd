// cpd - Coherent Point Drift
// Copyright (C) 2016 Pete Gadomski <pete.gadomski@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "cpd/nonrigid.hpp"
#include "affinity.hpp"
#include "cpd/runner.hpp"

namespace cpd {

void Nonrigid::init(const Matrix&, const Matrix& moving) {
    m_g = affinity(moving, moving, m_beta);
    m_w = Matrix::Zero(moving.rows(), moving.cols());
}

void Nonrigid::modify_probabilities(Probabilities& probabilities) const {
    probabilities.l += m_lambda / 2.0 * (m_w.transpose() * m_g * m_w).trace();
}

Nonrigid::Result Nonrigid::compute(const Matrix& fixed, const Matrix& moving,
                                   const Probabilities& probabilities,
                                   double sigma2) {
    size_t cols = fixed.cols();
    auto dp = probabilities.p1.asDiagonal();
    m_w = (dp * m_g +
           m_lambda * sigma2 * Matrix::Identity(moving.rows(), moving.rows()))
              .colPivHouseholderQr()
              .solve(probabilities.px - dp * moving);
    Nonrigid::Result result;
    result.points = moving + m_g * m_w;
    double np = probabilities.p1.sum();
    result.sigma2 = std::abs(
        ((fixed.array().pow(2) * probabilities.pt1.replicate(1, cols).array())
             .sum() +
         (result.points.array().pow(2) *
          probabilities.p1.replicate(1, cols).array())
             .sum() -
         2 * (probabilities.px.transpose() * result.points).trace()) /
        (np * cols));
    return result;
}

void Nonrigid::denormalize(const Normalization& normalization,
                           Nonrigid::Result& result) const {
    result.points =
        result.points * normalization.scale +
        normalization.fixed_mean.transpose().replicate(result.points.rows(), 1);
}

Nonrigid::Result nonrigid(const Matrix& fixed, const Matrix& moving) {
    return run<Nonrigid>(fixed, moving);
}
}
