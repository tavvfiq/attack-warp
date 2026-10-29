#pragma once

// Read-only prefix of True Directional Movement's modder API (github.com/ersh1/TrueDirectionalMovement,
// src/TrueDirectionalMovementAPI.h, by Ersh, GPL-3.0 repository; the header invites modders to copy it). Only the first four IVTDM1
// methods, in upstream vtable order, are declared: Attack Warp only reads whether TDM steers the player and its locked target.
// The yaw-control calls (V2) are not used: TDM refuses them off its own thread (APIResult::BadThread).

namespace TDM_API
{
	enum class InterfaceVersion : std::uint8_t
	{
		V1
	};

	class IVTDM1
	{
	public:
		[[nodiscard]] virtual unsigned long   GetTDMThreadId() const noexcept = 0;
		[[nodiscard]] virtual bool            GetDirectionalMovementState() const noexcept = 0;
		[[nodiscard]] virtual bool            GetTargetLockState() const noexcept = 0;
		[[nodiscard]] virtual RE::ActorHandle GetCurrentTarget() const noexcept = 0;
	};

	[[nodiscard]] inline void* RequestPluginAPI(InterfaceVersion a_version = InterfaceVersion::V1)
	{
		const auto module = GetModuleHandleA("TrueDirectionalMovement.dll");
		if (!module) return nullptr;
		const auto request = reinterpret_cast<void* (*)(InterfaceVersion)>(GetProcAddress(module, "RequestPluginAPI"));
		return request ? request(a_version) : nullptr;
	}
}
