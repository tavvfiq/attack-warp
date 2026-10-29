// Attack Warp entry point: log setup, settings, the root-motion hook, the combat event and the optional in-game menu.
// Log: Documents/My Games/Skyrim Special Edition/SKSE/AttackWarp.log (rewritten every launch; bDebugLog adds one line per swing).
#include "Hooks.h"
#include "Menu.h"
#include "Settings.h"

namespace
{
	void InitLog()
	{
		auto dir = logger::log_directory();
		if (!dir) {
			return;
		}
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "AttackWarp.log").string(), true);
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		log->set_pattern("[%H:%M:%S.%e] [%l] %v");
		spdlog::set_default_logger(std::move(log));
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			AttackWarp::InstallHooks();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			// not kPostLoadGame: that message never reached the probes in the tested setup (MOTIONWARP_RE.md run 2)
			AttackWarp::RegisterEvents();
			AttackWarp::Menu::Register();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitLog();
	SKSE::Init(a_skse, { .log = false });
	const auto ver = REL::Module::get().version();
	// the hooked slot was checked on 1.5.97 and 1.7.104; other runtimes load but are unverified
	const bool verified = ver == SKSE::RUNTIME_SSE_1_5_97 || ver == REL::Version(1, 7, 104, 0);
	logger::info("Attack Warp {} loading, game {} ({})", AttackWarp::kVersion, ver.string(), verified ? "verified runtime" : "UNVERIFIED runtime");
	AttackWarp::LoadSettings();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
