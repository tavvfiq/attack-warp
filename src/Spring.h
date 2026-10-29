// Spring: critically damped spring, after Daniel Holden's spring.h (orangeduck/Motion-Matching, MIT;
// theorangeduck.com "Spring-It-On"). It moves a value toward a goal with a continuous velocity, so a change starts and
// settles smoothly instead of jumping, is exact for any frame time, and never overshoots when starting from rest.
#pragma once

#include <cmath>

namespace Spring
{
	// Halflife for which the spring covers 63% of a step in a_ease seconds, like the first-order ease of 0.1.0,
	// so ease-time settings keep their meaning: (1 + y t) e^(-y t) = e^-1 at y t = 2.146, halflife = 2 ln2 / y.
	inline float HalflifeFromEase(float a_ease) { return 0.646f * a_ease; }

	// x and v are the spring's value and velocity; a_halflife (s) sets the stiffness, a_dt the step.
	inline void Damper(float& x, float& v, float a_goal, float a_halflife, float a_dt)
	{
		const float y = 1.386294f / (a_halflife + 1e-5f);  // half of the damping 4 ln2 / halflife
		const float j0 = x - a_goal;
		const float j1 = v + j0 * y;
		const float eydt = std::exp(-y * a_dt);
		x = eydt * (j0 + j1 * a_dt) + a_goal;
		v = eydt * (v - j1 * y * a_dt);
	}
}
