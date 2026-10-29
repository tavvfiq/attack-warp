// Settings: every tunable of Attack Warp. Defaults live in Data/SKSE/Plugins/AttackWarp.ini, user changes in
// AttackWarp_User.ini next to it (written by the in-game menu; editable by hand without SKSE Menu Framework).
// Distances are centre to centre in game units. Probe evidence for each default: MOTIONWARP_RE.md runs 1-4.
#pragma once

namespace AttackWarp
{
	inline constexpr auto kVersion = "0.3.0"sv;

	struct Settings
	{
		// [General]
		bool enabled{ true };
		bool player{ true };      // the player's melee attacks
		bool npcs{ true };        // humanoid NPCs (race keyword ActorTypeNPC)
		bool creatures{ false };  // everything else (animals, monsters); not measured in the probe
		bool debugLog{ false };   // one line per swing in AttackWarp.log

		// [Target] the player picks the hostile actor inside this cone around the facing; NPCs use their combat target
		float targetCone{ 60.0f };     // half-angle, degrees (run 4: pick matched the hit victim in 33 of 38 hits)
		float maxStartDist{ 450.0f };  // swings that start further away are left alone
		float maxStartDistPower{ 700.0f };  // power and charge (sprint) attacks: run 4 lost 15 of 243 picks to charges started beyond 450
		float targetStickiness{ 0.3f };     // player combo: the previous swing's target wins unless another scores this much better
		bool  tdmTarget{ true };            // player: True Directional Movement's locked target, when locked, is the target

		// [Stretch] before the hit, forward root motion grows up to maxScale so short lunges still reach
		bool  stretch{ true };
		float maxScale{ 1.8f };         // run 4: charges ended 93-126 away instead of 50-468
		float maxStretchAngle{ 30.0f }; // only when the target is this close to straight ahead
		float stretchEase{ 0.12f };     // seconds to cover ~63% of a change in the stretch factor, via a spring (0 = instant, the probe)
		float stretchRange{ 150.0f };   // full stretch only this far beyond the brake zone; closer it fades to none (0 = always full, 0.2.0)
		float leadTime{ 0.25f };        // aim the stretch and turn at where a moving target will be this many seconds ahead (0 = off)
		float maxLead{ 80.0f };         // the lead never moves the aim point further than this

		// [Turn] while winding up and swinging, the attacker turns toward the target (root motion barely turns: run 1, 0.7 deg/frame)
		bool  turn{ true };
		bool  turnPlayer{ true };       // also the player, unless True Directional Movement steers the player
		float turnRate{ 300.0f };       // deg/s at most
		float turnEase{ 0.08f };        // seconds to cover ~63% of the heading error, via a spring (0 = rate limit only)
		float maxTurnAngle{ 90.0f };    // targets further off the facing are not turned to

		// [Brake] the forward path never comes closer than the stop distance; speed tapers over brakeZone before it
		bool  brake{ true };
		float stopDistPlayer{ 70.0f };  // run 2: vanilla stop-on-contact holds the player at 40-52
		float stopDistNpc{ 100.0f };    // run 2: NPCs touch the player at 43-108
		float brakeZone{ 60.0f };       // 0 = hard stop at the line (the probe)
		float brakeEase{ 0.05f };       // seconds to cover ~63% of a jump in the brake factor (0 = no smoothing, 0.1.0)
		bool  precisionReach{ true };   // shift the stop distance by the weapon's Precision collision length
		float reachRef{ 60.0f };        // the length the stop distances above are for (Fenix runtime log: 48-76)
		float reachWeight{ 1.0f };      // units of stop distance per unit of length beyond reachRef
	};

	// Current settings. Reads are lock-free; Publish writes the other buffer and flips.
	const Settings& Cfg();
	void            Publish(const Settings& a_settings);  // apply now (menu changes)
	Settings        DefaultSettings();                    // shipped defaults, ignoring the user file
	void            LoadSettings();                       // defaults + user file, then publish
	void            SaveSettings();                       // current settings to AttackWarp_User.ini (main thread)
}
