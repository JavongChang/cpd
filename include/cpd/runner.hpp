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

/// \file
/// Configure cpd registration runs.

#pragma once

#include <cpd/comparer/base.hpp>
#include <cpd/comparer/direct.hpp>
#include <cpd/logging.hpp>
#include <cpd/normalize.hpp>
#include <cpd/utils.hpp>
#include <cpd/vendor/spdlog/spdlog.h>

namespace cpd {

/// The default number of iterations allowed.
const size_t DEFAULT_MAX_ITERATIONS = 150;
/// Whether points should be normalized by default.
const bool DEFAULT_NORMALIZE = true;
/// The default outlier weight.
const double DEFAULT_OUTLIERS = 0.1;
/// The default tolerance.
const double DEFAULT_TOLERANCE = 1e-5;
/// The default sigma2.
const double DEFAULT_SIGMA2 = 0.0;
/// Whether correspondence vector should be computed by default.
const bool DEFAULT_CORRESPONDENCE = false;

/// Template class for running cpd registrations.
///
/// The template argument is the type of transform (e.g. Rigid).
template <typename Transform>
class Runner {
public:
    /// Creates a default Runner.
    Runner()
      : Runner(Transform()) {}

    /// Creates a runner with a pre-constructed probability computer and
    /// transform.
    Runner(Transform transform)
      : m_comparer(Comparer::create())
      , m_correspondence(DEFAULT_CORRESPONDENCE)
      , m_logger(spdlog::get(LOGGER_NAME))
      , m_max_iterations(DEFAULT_MAX_ITERATIONS)
      , m_normalize(DEFAULT_NORMALIZE)
      , m_outliers(DEFAULT_OUTLIERS)
      , m_sigma2(DEFAULT_SIGMA2)
      , m_tolerance(DEFAULT_TOLERANCE)
      , m_transform(transform) {}

    /// Sets the comparer.
    Runner& comparer(std::unique_ptr<Comparer> comparer) {
        m_comparer = std::move(comparer);
        return *this;
    };

    /// Returrns whether the correspondence vector should be calculated.
    bool correspondence() const { return m_correspondence; }

    /// Sets whether the correspondence vector should be calculated.
    Runner& correspondence(bool correspondence) {
        m_correspondence = correspondence;
        return *this;
    }

    /// Returns the maximum number of iterations allowed.
    size_t max_iterations() const { return m_max_iterations; }

    /// Sets the maximum number of iterations allowed.
    Runner& max_iterations(size_t max_iterations) {
        m_max_iterations = max_iterations;
        return *this;
    }

    /// Returns whether the points should be normalized before running.
    bool normalize() const { return m_normalize; }

    /// Sets whether the points should be normalized before running.
    Runner& normalize(bool normalize) {
        m_normalize = normalize;
        return *this;
    }

    /// Returns the sigma2 value.
    double sigma2() const { return m_sigma2; }

    /// Sets the initial sigma2.
    ///
    /// If this value is not set, it will be computed to a reasonable default.
    Runner& sigma2(double sigma2) {
        m_sigma2 = sigma2;
        return *this;
    }

    /// Returns the tolerance value.
    double tolerance() const { return m_tolerance; }

    /// Sets the tolerance.
    Runner& tolerance(double tolerance) {
        m_tolerance = tolerance;
        return *this;
    }

    /// Returns the outliers value.
    double outliers() const { return m_outliers; }

    /// Sets the outlier weight.
    Runner& outliers(double outliers) {
        m_outliers = outliers;
        return *this;
    }

    /// Runs the cpd registration.
    typename Transform::Result run(const Matrix& fixed, const Matrix& moving) {
        if (m_logger) {
            m_logger->info("Number of points in fixed matrix: {}",
                           fixed.rows());
            m_logger->info("Number of points in moving matrix: {}",
                           moving.rows());
        }
        const Matrix* fixed_ptr(&fixed);
        const Matrix* moving_ptr(&moving);
        Normalization normalization;
        const auto tic = std::chrono::high_resolution_clock::now();
        if (m_normalize) {
            normalization = cpd::normalize(fixed, moving);
            fixed_ptr = &normalization.fixed;
            moving_ptr = &normalization.moving;
        }
        m_transform.init(*fixed_ptr, *moving_ptr);
        size_t iter = 0;
        double ntol = m_tolerance + 10.0;
        double l = 0.0;
        typename Transform::Result result;
        result.points = *moving_ptr;
        if (m_sigma2 == 0.0) {
            result.sigma2 = default_sigma2(*fixed_ptr, *moving_ptr);
            if (m_logger) {
                m_logger->info("Initializing sigma2 to {}", result.sigma2);
            }
        } else {
            result.sigma2 = m_sigma2;
            if (m_logger) {
                m_logger->info("sigma2 previously set to {}", result.sigma2);
            }
        }
        while (iter < m_max_iterations && ntol > m_tolerance &&
               result.sigma2 > 10 * std::numeric_limits<double>::epsilon()) {
            auto probabilities = m_comparer->compute(*fixed_ptr, result.points,
                                                     result.sigma2, m_outliers);
            m_transform.modify_probabilities(probabilities);
            ntol = std::abs((probabilities.l - l) / probabilities.l);
            if (m_logger) {
                m_logger->info("iter={}, dL={:.8f}, sigma2={:.8f}", iter, ntol,
                               result.sigma2);
            }
            l = probabilities.l;
            result = m_transform.compute(*fixed_ptr, *moving_ptr, probabilities,
                                         result.sigma2);
            ++iter;
        }
        if (m_normalize) {
            m_transform.denormalize(normalization, result);
        }
        if (m_correspondence) {
            auto probabilities = DirectComparer().compute(fixed, result.points,
                                                          m_sigma2, m_outliers);
            result.correspondence = probabilities.correspondence;
        }
        const auto toc = std::chrono::high_resolution_clock::now();
        result.runtime =
            std::chrono::duration_cast<std::chrono::microseconds>(toc - tic);
        result.iterations = iter;
        return result;
    }

private:
    std::unique_ptr<Comparer> m_comparer;
    bool m_correspondence;
    std::shared_ptr<spdlog::logger> m_logger;
    size_t m_max_iterations;
    bool m_normalize;
    double m_outliers;
    double m_sigma2;
    double m_tolerance;
    Transform m_transform;
};

/// Runs a registration with a default comparer.
template <typename Transform>
typename Transform::Result run(const Matrix& fixed, const Matrix& source) {
    Runner<Transform> runner;
    return runner.run(fixed, source);
}
}
