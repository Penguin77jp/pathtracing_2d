#include "pt2d/RISDirection.h"
#include "pt2d/Integrator.h"

#include <numbers>
#include <cmath>
#include <optional>

namespace pt2d {
	RISDirection::RISDirection(const int num_bins, const int exploration_percent, const int smooth_sigma_deg, uint64_t seed)
		: m_num_bins(num_bins), m_smooth_sigma_deg(smooth_sigma_deg), m_sampler(Sampler(seed)) {

		m_bin_width = 2.0 * std::numbers::pi / static_cast<float>(m_num_bins);
		if (exploration_percent < 0 || exploration_percent > 100) {
			throw std::invalid_argument("exploration_percent should be in [0, 100].");
		}
		m_exploration_prob = 0.01f * exploration_percent;
		m_score.resize(static_cast<size_t>(m_num_bins), 0.0f);
	}

	std::vector<float> RISDirection::circular_smoothed_score() const {
		std::vector<float> result(static_cast<size_t>(m_num_bins), 0.0f);

		const float sigma_rad =
			static_cast<float>(m_smooth_sigma_deg)
			* std::numbers::pi_v<float>
			/ 180.0f;
		const float sigma_bins = sigma_rad / m_bin_width;
		if (sigma_bins <= 0.0f) {
			return m_score;
		}

		const int radius =
			std::max(1, static_cast<int>(std::ceil(3.0f * sigma_bins)));

		for (int i = 0; i < m_num_bins; ++i) {
			float weighted_sum = 0.0f;
			float weight_sum = 0.0f;

			for (int offset = -radius; offset <= radius; ++offset) {
				int j = (i + offset) % m_num_bins;
				if (j < 0) {
					j += m_num_bins;
				}

				const float x = static_cast<float>(offset);
				const float w = std::exp(
					-0.5f * (x * x) / (sigma_bins * sigma_bins)
				);

				weighted_sum += m_score[static_cast<size_t>(j)] * w;
				weight_sum += w;
			}

			if (weight_sum > 0.0f) {
				result[static_cast<size_t>(i)] = weighted_sum / weight_sum;
			}
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

	int random_bin_selection(const std::vector<float>& pdf, const float sum_pdf, Sampler& sampler) {
		if (sum_pdf <= 0.0f) {
			return -1;
		}
		
		const size_t N = pdf.size();
		const float random_value = sampler.next1D() * sum_pdf;
		float cumulative_score = 0.0f;
		for (size_t i = 0; i < N; ++i) {
			cumulative_score += pdf[i];
			if (random_value <= cumulative_score) {
				return static_cast<int>(i);
			}
		}
		return static_cast<int>(N - 1);
	}

	RISDirection::AngularSample RISDirection::sample() {
		std::vector<float> smoothed_score = circular_smoothed_score();
		float score_sum = sum_vector(smoothed_score);

		// bin probability を作る
		std::vector<float> bin_prob(static_cast<size_t>(m_num_bins), 0.0f);
		if (score_sum <= 0.0f) {
			const float theta = m_sampler.next1D() * 2.0f * std::numbers::pi_v<float>;
			return {
				theta,
				1.0f / (2.0f * std::numbers::pi_v<float>)
			};
		}
		else {
			// m_min_prob は「全体で何割を一様探索に残すか」
			const float uniform_bin_prob = 1.0f / static_cast<float>(m_num_bins);

			for (int i = 0; i < m_num_bins; ++i) {
				const float learned =
					std::max(0.0f, smoothed_score[static_cast<size_t>(i)]) / score_sum;

				bin_prob[static_cast<size_t>(i)] =
					(1.0f - m_exploration_prob) * learned
					+ m_exploration_prob * uniform_bin_prob;
			}
		}

		// bin random selection
		const float sum_bin_prob = sum_vector(bin_prob);
		int selected_index = random_bin_selection(bin_prob, sum_bin_prob, m_sampler);
		if (selected_index < 0) {
			throw std::runtime_error("Failed to select a bin in RISDirection::sample");
		}

		// uniform random theta in the selected bin
		const float theta = (static_cast<float>(selected_index) + m_sampler.next1D()) * m_bin_width;

		// PDF calculation
		const float pdf = bin_prob[selected_index] / m_bin_width / sum_bin_prob;

		return { theta, pdf };
	}

	void RISDirection::update(const float theta, float weighted_contribution) {
		if (!std::isfinite(weighted_contribution) || weighted_contribution <= 0.0f) {
			return;
		}

		const size_t index = bin_index(theta);
		m_score[index] += weighted_contribution;
	}

	void RISDirection::update(std::vector<RISDirection::AngularSample>& angular_samples, std::vector<Color>& weighted_contributions){
		const size_t N = angular_samples.size();
		std::vector<float> weighted_contributions_alpha(N), cdf(N);
		for (size_t i = 0; i < N; ++i) {
			weighted_contributions_alpha[i] = std::max(weighted_contributions[i].r, std::max(weighted_contributions[i].g, weighted_contributions[i].b));
		}
		const float weighted_contributions_alpha_sum = sum_vector(weighted_contributions_alpha);
		int selected_index = random_bin_selection(weighted_contributions_alpha, weighted_contributions_alpha_sum, m_sampler);
		if (selected_index < 0)
			return;

		if (selected_index >= N) {
			selected_index = N - 1;
		}

		const float representative_weight = weighted_contributions_alpha_sum / static_cast<float>(N);
		update(angular_samples[selected_index].theta, weighted_contributions_alpha[selected_index]);
	}

	size_t RISDirection::bin_index(const float theta) const
	{
		if (theta < 0.0f || theta >= 2.0f * std::numbers::pi) {
			throw std::out_of_range("theta out of range in bin_index in RISDirection::bin_index");
		}
		return static_cast<size_t>(theta / m_bin_width) % m_score.size();
	}

} // namespace pt2d
