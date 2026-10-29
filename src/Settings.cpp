// Settings loader and saver. Two files in Data/SKSE/Plugins:
//  AttackWarp.ini       shipped defaults (replaced by every update)
//  AttackWarp_User.ini  written by the in-game menu (SKSE Menu Framework); survives updates
// Value order per key: user file > default file > built-in default. The menu publishes changes live.
#include "Settings.h"

namespace AttackWarp
{
	namespace
	{
		// shortcut: a reader holding Cfg() across two publishes in a row could see a torn struct; publishes happen at
		// most once per rendered frame while a menu widget changes, readers hold the reference for microseconds.
		// Upgrade to an atomic shared_ptr if settings ever change from more than one thread.
		std::array<Settings, 2> g_buf;
		std::atomic<int>        g_cur{ 0 };

		// GetPrivateProfile* needs absolute paths (a relative one resolves to the Windows directory).
		std::string PluginPath(std::string_view a_file) { return (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / a_file).string(); }

		struct Ini
		{
			std::string def = PluginPath("AttackWarp.ini");
			std::string user = PluginPath("AttackWarp_User.ini");
			bool        withUser{ true };

			std::string Str(const char* a_sec, const char* a_key, const std::string& a_fallback) const
			{
				char buf[64]{};
				GetPrivateProfileStringA(a_sec, a_key, a_fallback.c_str(), buf, sizeof(buf), def.c_str());
				if (!withUser) {
					return buf;
				}
				const std::string fromDef = buf;
				GetPrivateProfileStringA(a_sec, a_key, fromDef.c_str(), buf, sizeof(buf), user.c_str());
				return buf;
			}

			void Get(const char* a_sec, const char* a_key, bool& a_v) const
			{
				const auto s = Str(a_sec, a_key, a_v ? "1" : "0");
				a_v = !(s == "0" || s == "false" || s == "False");
			}

			void Get(const char* a_sec, const char* a_key, float& a_v, float a_min, float a_max) const
			{
				try {
					a_v = std::stof(Str(a_sec, a_key, std::to_string(a_v)));
				} catch (...) {
					logger::warn("settings: [{}] {} is not a number, using {}", a_sec, a_key, a_v);
				}
				a_v = std::clamp(a_v, a_min, a_max);
			}
		};

		// One table drives reading and writing so the two can never disagree.
		template <class Fn>
		void Visit(Settings& c, Fn&& f)
		{
			f("General", "bEnabled", c.enabled);
			f("General", "bPlayer", c.player);
			f("General", "bNPCs", c.npcs);
			f("General", "bCreatures", c.creatures);
			f("General", "bDebugLog", c.debugLog);

			f("Target", "fTargetCone", c.targetCone, 10.0f, 180.0f);
			f("Target", "fMaxStartDist", c.maxStartDist, 100.0f, 1500.0f);
			f("Target", "fMaxStartDistPower", c.maxStartDistPower, 100.0f, 1500.0f);
			f("Target", "fTargetStickiness", c.targetStickiness, 0.0f, 2.0f);
			f("Target", "bTDMTarget", c.tdmTarget);

			f("Stretch", "bStretch", c.stretch);
			f("Stretch", "fMaxScale", c.maxScale, 1.0f, 3.0f);
			f("Stretch", "fMaxStretchAngle", c.maxStretchAngle, 0.0f, 90.0f);
			f("Stretch", "fStretchEase", c.stretchEase, 0.0f, 0.5f);
			f("Stretch", "fStretchRange", c.stretchRange, 0.0f, 500.0f);
			f("Stretch", "fLeadTime", c.leadTime, 0.0f, 1.0f);
			f("Stretch", "fMaxLead", c.maxLead, 0.0f, 300.0f);

			f("Turn", "bTurn", c.turn);
			f("Turn", "bTurnPlayer", c.turnPlayer);
			f("Turn", "fTurnRate", c.turnRate, 0.0f, 1080.0f);
			f("Turn", "fTurnEase", c.turnEase, 0.0f, 0.5f);
			f("Turn", "fMaxTurnAngle", c.maxTurnAngle, 0.0f, 180.0f);

			f("Brake", "bBrake", c.brake);
			f("Brake", "fStopDistPlayer", c.stopDistPlayer, 30.0f, 200.0f);
			f("Brake", "fStopDistNpc", c.stopDistNpc, 30.0f, 200.0f);
			f("Brake", "fBrakeZone", c.brakeZone, 0.0f, 200.0f);
			f("Brake", "fBrakeEase", c.brakeEase, 0.0f, 0.3f);
			f("Brake", "bPrecisionReach", c.precisionReach);
			f("Brake", "fReachRef", c.reachRef, 0.0f, 300.0f);
			f("Brake", "fReachWeight", c.reachWeight, 0.0f, 2.0f);
		}

		struct Reader
		{
			const Ini& ini;
			void       operator()(const char* s, const char* k, bool& v) const { ini.Get(s, k, v); }
			void       operator()(const char* s, const char* k, float& v, float lo, float hi) const { ini.Get(s, k, v, lo, hi); }
		};

		Settings Read(bool a_withUser)
		{
			Ini ini;
			ini.withUser = a_withUser;
			Settings c;
			Visit(c, Reader{ ini });
			return c;
		}

		void Log(const Settings& c, const char* a_why)
		{
			logger::info("settings ({}): enabled={} player={} npcs={} creatures={} debug={} | target cone {:.0f} deg, max start {:.0f} (power {:.0f}), sticky {:.2f}, "
						 "TDM target={} | stretch={} x{:.2f} within {:.0f} deg, ease {:.2f}s, range {:.0f}, lead {:.2f}s max {:.0f} | turn={} player={} {:.0f} deg/s "
						 "ease {:.2f}s within {:.0f} deg | brake={} stop player {:.0f} npc {:.0f}, zone {:.0f} ease {:.2f}s, Precision reach={} ref {:.0f} x{:.2f}",
				a_why, c.enabled, c.player, c.npcs, c.creatures, c.debugLog, c.targetCone, c.maxStartDist, c.maxStartDistPower, c.targetStickiness, c.tdmTarget,
				c.stretch, c.maxScale, c.maxStretchAngle, c.stretchEase, c.stretchRange, c.leadTime, c.maxLead, c.turn, c.turnPlayer, c.turnRate, c.turnEase,
				c.maxTurnAngle, c.brake, c.stopDistPlayer, c.stopDistNpc, c.brakeZone, c.brakeEase, c.precisionReach, c.reachRef, c.reachWeight);
		}
	}

	const Settings& Cfg() { return g_buf[g_cur.load(std::memory_order_acquire)]; }

	void Publish(const Settings& a_settings)
	{
		const int next = 1 - g_cur.load(std::memory_order_relaxed);
		g_buf[next] = a_settings;
		g_cur.store(next, std::memory_order_release);
		spdlog::set_level(a_settings.debugLog ? spdlog::level::debug : spdlog::level::info);
	}

	Settings DefaultSettings() { return Read(false); }

	void LoadSettings()
	{
		const Ini ini;
		logger::info("settings: defaults {} ({}), user {} ({})", ini.def, std::filesystem::exists(ini.def) ? "found" : "missing, built-in values", ini.user,
			std::filesystem::exists(ini.user) ? "found" : "none yet");
		Publish(Read(true));
		Log(Cfg(), "loaded");
	}

	void SaveSettings()
	{
		Settings  c = Cfg();
		const Ini ini;
		bool      ok = true;
		Visit(c, [&](const char* s, const char* k, auto& v, auto...) {
			std::string text;
			if constexpr (std::is_same_v<std::remove_cvref_t<decltype(v)>, bool>) {
				text = v ? "1" : "0";
			} else {
				text = std::format("{:g}", v);
			}
			ok &= WritePrivateProfileStringA(s, k, text.c_str(), ini.user.c_str()) != 0;
		});
		if (ok) {
			Log(c, "saved to AttackWarp_User.ini");
		} else {
			logger::error("settings: could not write {} (error {})", ini.user, GetLastError());
		}
	}
}
