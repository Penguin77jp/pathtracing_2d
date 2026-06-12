#include "pt2d/RISDirection.h"
#include "pt2d/Integrator.h"

#include <numbers>
#include <cmath>
#include <optional>
#include <algorithm>
#include <stdexcept>

namespace pt2d {
	RISDirection::RISDirection(const int num_bins, const float exploration_percent, const int smooth_sigma_deg, uint64_t seed)
		: m_num_bins(std::max(1, num_bins)), m_smooth_sigma_deg(std::max(0, smooth_sigma_deg)), m_sampler(Sampler(seed)) {

		m_bin_width = 2.0f * std::numbers::pi_v<float> / static_cast<float>(m_num_bins);
		m_exploration_prob = 0.01f * std::clamp(exploration_percent, 0.0f, 100.0f);
		m_score.resize(static_cast<size_t>(m_num_bins), 0.0f);
		m_smoothed_score_cache.resize(static_cast<size_t>(m_num_bins), 0.0f);
		m_bin_prob_cache.resize(static_cast<size_t>(m_num_bins), 0.0f);
		m_cdf_cache.resize(static_cast<size_t>(m_num_bins), 0.0f);

		build_smoothing_kernel();
		invalidate_distribution_cache();
	}

	void RISDirection::build_smoothing_kernel() {
		m_kernel_offsets.clear();
		m_kernel_weights.clear();
		m_kernel_weight_sum = 0.0f;

		const float sigma_rad =
			static_cast<float>(m_smooth_sigma_deg)
			* std::numbers::pi_v<float>
			/ 180.0f;
		const float sigma_bins = sigma_rad / m_bin_width;
		if (sigma_bins <= 0.0f || !std::isfinite(sigma_bins)) {
			m_kernel_offsets.push_back(0);
			m_kernel_weights.push_back(1.0f);
			m_kernel_weight_sum = 1.0f;
			return;
		}

		const int radius =
			std::max(1, static_cast<int>(std::ceil(3.0f * sigma_bins)));

		for (int offset = -radius; offset <= radius; ++offset) {
			const float x = static_cast<float>(offset);
			const float w = std::exp(
				-0.5f * (x * x) / (sigma_bins * sigma_bins)
			);
			m_kernel_offsets.push_back(offset);
			m_kernel_weights.push_back(w);
			m_kernel_weight_sum += w;
		}

		if (m_kernel_weight_sum <= 0.0f || !std::isfinite(m_kernel_weight_sum)) {
			m_kernel_offsets.assign(1, 0);
			m_kernel_weights.assign(1, 1.0f);
			m_kernel_weight_sum = 1.0f;
		}
	}

	void RISDirection::invalidate_distribution_cache() {
		m_distribution_dirty = true;
	}

	std::vector<float> RISDirection::circular_smoothed_score() const {
		std::vector<float> result(static_cast<size_t>(m_num_bins), 0.0f);

		if (m_kernel_offsets.size() == 1 && m_kernel_offsets[0] == 0) {
			return m_score;
		}

		for (int i = 0; i < m_num_bins; ++i) {
			float weighted_sum = 0.0f;

			for (size_t k = 0; k < m_kernel_offsets.size(); ++k) {
				int j = (i + m_kernel_offsets[k]) % m_num_bins;
				if (j < 0) {
					j += m_num_bins;
				}

				weighted_sum += m_score[static_cast<size_t>(j)] * m_kernel_weights[k];
			}

			result[static_cast<size_t>(i)] = weighted_sum / m_kernel_weight_sum;
		}

		return result;
	}

	float sum_vector(const std::vector<float>& v) {
		float sum = 0.0f;
		for (float x : v) {
			sum += x;
		}
		return sum;
	}

	int random_weight_selection(const std::vector<float>& weights, const float sum_weights, Sampler& sampler) {
		if (sum_weights <= 0.0f || !std::isfinite(sum_weights)) {
			return -1;
		}
		
		const size_t N = weights.size();
		const float random_value = sampler.next1D() * sum_weights;
		float cumulative_score = 0.0f;
		for (size_t i = 0; i < N; ++i) {
			cumulative_score += weights[i];
			if (random_value <= cumulative_score) {
				return static_cast<int>(i);
			}
		}
		return static_cast<int>(N - 1);
	}

	int random_cdf_selection(const std::vector<float>& cdf, Sampler& sampler) {
		if (cdf.empty()) {
			return -1;
		}

		const float random_value = sampler.next1D();
		auto it = std::lower_bound(cdf.begin(), cdf.end(), random_value);
		if (it == cdf.end()) {
			return static_cast<int>(cdf.size() - 1);
		}
		return static_cast<int>(std::distance(cdf.begin(), it));
	}

	void RISDirection::rebuild_distribution_cache() const {
		if (!m_distribution_dirty) {
			return;
		}

		m_smoothed_score_cache = circular_smoothed_score();
		const float smoothed_score_sum = sum_vector(m_smoothed_score_cache);

		std::fill(m_bin_prob_cache.begin(), m_bin_prob_cache.end(), 0.0f);
		std::fill(m_cdf_cache.begin(), m_cdf_cache.end(), 0.0f);

		const float uniform_bin_prob = 1.0f / static_cast<float>(m_num_bins);
		if (smoothed_score_sum <= 0.0f || !std::isfinite(smoothed_score_sum)) {
			std::fill(m_bin_prob_cache.begin(), m_bin_prob_cache.end(), uniform_bin_prob);
		}
		else {
			for (int i = 0; i < m_num_bins; ++i) {
				const float learned =
					std::max(0.0f, m_smoothed_score_cache[static_cast<size_t>(i)]) / smoothed_score_sum;
				m_bin_prob_cache[static_cast<size_t>(i)] =
					(1.0f - m_exploration_prob) * learned
					+ m_exploration_prob * uniform_bin_prob;
			}

			const float prob_sum = sum_vector(m_bin_prob_cache);
			if (prob_sum > 0.0f && std::isfinite(prob_sum)) {
				for (float& p : m_bin_prob_cache) {
					p /= prob_sum;
				}
			}
			else {
				std::fill(m_bin_prob_cache.begin(), m_bin_prob_cache.end(), uniform_bin_prob);
			}
		}

		float cumulative_prob = 0.0f;
		for (int i = 0; i < m_num_bins; ++i) {
			cumulative_prob += m_bin_prob_cache[static_cast<size_t>(i)];
			m_cdf_cache[static_cast<size_t>(i)] = cumulative_prob;
		}

		// Avoid missing the last bin due to floating-point accumulation error.
		m_cdf_cache.back() = 1.0f;
		m_distribution_dirty = false;
	}

	void RISDirection::set_scores(const std::vector<float>& scores) {
		if (scores.size() != m_score.size()) {
			return;
		}

		for (size_t i = 0; i < m_score.size(); ++i) {
			const float score = scores[i];
			m_score[i] = (std::isfinite(score) && score > 0.0f) ? score : 0.0f;
		}
		invalidate_distribution_cache();
	}

	float RISDirection::score_sum() const {
		return sum_vector(m_score);
	}

	std::vector<float> RISDirection::probabilities() const {
		rebuild_distribution_cache();
		return m_bin_prob_cache;
	}

	RISDirection::AngularSample RISDirection::sample() {
		rebuild_distribution_cache();

		const int selected_index = random_cdf_selection(m_cdf_cache, m_sampler);
		if (selected_index < 0) {
			throw std::runtime_error("Failed to select a bin in RISDirection::sample");
		}

		// uniform random theta in the selected bin
		const float theta = (static_cast<float>(selected_index) + m_sampler.next1D()) * m_bin_width;

		// PDF calculation: discrete bin probability / bin width.
		const float pdf = m_bin_prob_cache[static_cast<size_t>(selected_index)] / m_bin_width;

		return { theta, pdf };
	}

	void RISDirection::update(const float theta, float weighted_contribution) {
		if (!std::isfinite(weighted_contribution) || weighted_contribution <= 0.0f) {
			return;
		}

		const size_t index = bin_index(theta);
		m_score[index] += weighted_contribution;
		invalidate_distribution_cache();
	}

	void RISDirection::update(std::vector<RISDirection::AngularSample>& angular_samples, std::vector<float>& weighted_contributions){
		const size_t N = std::min(angular_samples.size(), weighted_contributions.size());
		if (N == 0) {
			return;
		}

		// All candidates have already been evaluated by the integrator.  Use every
		// finite positive candidate for learning instead of sampling a single one.
		// The 1/N factor keeps the learning rate roughly independent of
		// candidate_count, while preserving the relative bin strengths.
		const float inv_N = 1.0f / static_cast<float>(N);
		bool any_update = false;
		for (size_t i = 0; i < N; ++i) {
			const float w = weighted_contributions[i];
			if (!std::isfinite(w) || w <= 0.0f) {
				continue;
			}

			const float scaled_weight = w * inv_N;
			if (!std::isfinite(scaled_weight) || scaled_weight <= 0.0f) {
				continue;
			}

			const size_t index = bin_index(angular_samples[i].theta);
			m_score[index] += scaled_weight;
			any_update = true;
		}

		if (any_update) {
			invalidate_distribution_cache();
		}
	}

	void RISDirection::update(std::vector<RISDirection::AngularSample>& angular_samples, std::vector<Color>& weighted_contributions){
		const size_t N = angular_samples.size();
		std::vector<float> weighted_contributions_alpha(N);
		for (size_t i = 0; i < N; ++i) {
			weighted_contributions_alpha[i] = std::max(weighted_contributions[i].r, std::max(weighted_contributions[i].g, weighted_contributions[i].b));
		}
		update(angular_samples, weighted_contributions_alpha);
	}

	size_t RISDirection::bin_index(const float theta) const
	{
		if (theta < 0.0f || theta >= 2.0f * std::numbers::pi) {
			throw std::out_of_range("theta out of range in bin_index in RISDirection::bin_index");
		}
		return static_cast<size_t>(theta / m_bin_width) % m_score.size();
	}


void propagate_spatial_ris_direction_scores(
    std::vector<RISDirection>& ris_directions,
    int width,
    int height,
    int radius,
    float strength) {
    width = std::max(0, width);
    height = std::max(0, height);
    radius = std::max(0, radius);
    strength = std::clamp(strength, 0.0f, 1.0f);

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixel_count == 0 || ris_directions.size() != pixel_count || radius <= 0 || strength <= 0.0f) {
        return;
    }

    const size_t bin_count = static_cast<size_t>(ris_directions.front().num_bins());
    if (bin_count == 0) {
        return;
    }

    std::vector<float> blended_scores(pixel_count * bin_count, 0.0f);
    std::vector<float> neighbor_average(bin_count, 0.0f);

    const float sigma = std::max(1.0f, static_cast<float>(radius) * 0.5f);
    const float inv_two_sigma2 = 1.0f / (2.0f * sigma * sigma);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t index = static_cast<size_t>(y * width + x);
            const std::vector<float>& self_scores = ris_directions[index].scores();
            const size_t output_offset = index * bin_count;
            if (self_scores.size() != bin_count) {
                for (size_t bin = 0; bin < std::min(bin_count, self_scores.size()); ++bin) {
                    blended_scores[output_offset + bin] = self_scores[bin];
                }
                continue;
            }

            std::fill(neighbor_average.begin(), neighbor_average.end(), 0.0f);
            float weight_sum = 0.0f;

            for (int dy = -radius; dy <= radius; ++dy) {
                const int ny = y + dy;
                if (ny < 0 || ny >= height) {
                    continue;
                }
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= width || (dx == 0 && dy == 0)) {
                        continue;
                    }

                    const float distance2 = static_cast<float>(dx * dx + dy * dy);
                    const float w = std::exp(-distance2 * inv_two_sigma2);
                    if (w <= 0.0f || !std::isfinite(w)) {
                        continue;
                    }

                    const size_t neighbor_index = static_cast<size_t>(ny * width + nx);
                    const std::vector<float>& neighbor_scores = ris_directions[neighbor_index].scores();
                    if (neighbor_scores.size() != bin_count) {
                        continue;
                    }

                    for (size_t bin = 0; bin < bin_count; ++bin) {
                        const float score = neighbor_scores[bin];
                        if (std::isfinite(score) && score > 0.0f) {
                            neighbor_average[bin] += w * score;
                        }
                    }
                    weight_sum += w;
                }
            }

            if (weight_sum <= 0.0f || !std::isfinite(weight_sum)) {
                for (size_t bin = 0; bin < bin_count; ++bin) {
                    blended_scores[output_offset + bin] = self_scores[bin];
                }
                continue;
            }

            const float inv_weight_sum = 1.0f / weight_sum;
            for (size_t bin = 0; bin < bin_count; ++bin) {
                const float self = (std::isfinite(self_scores[bin]) && self_scores[bin] > 0.0f) ? self_scores[bin] : 0.0f;
                const float neighbor = neighbor_average[bin] * inv_weight_sum;
                blended_scores[output_offset + bin] = (1.0f - strength) * self + strength * neighbor;
            }
        }
    }

    std::vector<float> temp_scores(bin_count, 0.0f);
    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t input_offset = i * bin_count;
        for (size_t bin = 0; bin < bin_count; ++bin) {
            temp_scores[bin] = blended_scores[input_offset + bin];
        }
        ris_directions[i].set_scores(temp_scores);
    }
}
} // namespace pt2d
