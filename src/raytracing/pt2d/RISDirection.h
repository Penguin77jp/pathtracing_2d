#pragma once


#include "pt2d/Sampler.h"
#include "pt2d/Color.h"

#include <iostream>
#include <vector>

namespace pt2d {

	class RISDirection {
	public:
		struct AngularSample {
			float theta;
			float pdf;
		};
	public:
		RISDirection(const int num_bins, const float exploration_percent, const int smooth_sigma_deg, uint64_t seed);
		AngularSample sample();
		void update(const float theta, float weighted_contributions);
		void update(std::vector<AngularSample>& angular_samples, std::vector<Color>& weighed_contributions);

		int num_bins() const { return m_num_bins; }
		float bin_width() const { return m_bin_width; }
		const std::vector<float>& scores() const { return m_score; }
		void set_scores(const std::vector<float>& scores);
		std::vector<float> probabilities() const;
		float score_sum() const;
	private:
		size_t bin_index(const float theta) const;
		std::vector<float> circular_smoothed_score() const;
		void build_smoothing_kernel();
		void invalidate_distribution_cache();
		void rebuild_distribution_cache() const;
	private:
		int m_num_bins;
		float m_bin_width;
		std::vector<float> m_score;
		float m_exploration_prob;
		int m_smooth_sigma_deg;
		Sampler m_sampler;

		std::vector<int> m_kernel_offsets;
		std::vector<float> m_kernel_weights;
		float m_kernel_weight_sum = 1.0f;

		mutable bool m_distribution_dirty = true;
		mutable std::vector<float> m_smoothed_score_cache;
		mutable std::vector<float> m_bin_prob_cache;
		mutable std::vector<float> m_cdf_cache;
	};
	
} // namespace
