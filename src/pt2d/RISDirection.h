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
		RISDirection(const int num_bins, const int exploration_percent, const int smooth_sigma_deg, uint64_t seed);
		AngularSample sample();
		void update(const float theta, float weighted_contributions);
		void update(std::vector<AngularSample>& angular_samples, std::vector<Color>& weighed_contributions);
	private:
		size_t bin_index(const float theta) const;
		std::vector<float> circular_smoothed_score() const;
	private:
		int m_num_bins;
		float m_bin_width;
		std::vector<float> m_score;
		float m_exploration_prob;
		int m_smooth_sigma_deg;
		Sampler m_sampler;
	};
	
} // namespace
