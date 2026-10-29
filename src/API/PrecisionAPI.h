#pragma once

// Read-only subset of Precision's public modder API (github.com/ersh1/Precision, src/PrecisionAPI.h, by Ersh, GPL-3.0 repository;
// the header invites modders to copy it). The declarations through IVPrecision1 keep their upstream order so the virtual
// interface stays ABI-compatible. Attack Warp uses only GetAttackCollisionCapsuleLength; the callbacks are declared
// solely because they precede it in the V1 vtable. Same subset as FenixCombatAI-NG, which read 48-76 from it in game.
#include <functional>
#include <vector>
#include <Windows.h>

namespace PRECISION_API
{
	inline constexpr auto PrecisionPluginName = "Precision";

	enum class InterfaceVersion : std::uint8_t
	{
		V1,
		V2,
		V3,
		V4
	};

	enum class APIResult : std::uint8_t
	{
		OK,
		AlreadyRegistered,
		NotRegistered
	};

	struct PreHitModifier
	{
		enum class ModifierType : std::uint8_t
		{
			Damage,
			Stagger
		};

		enum class ModifierOperation : std::uint8_t
		{
			Additive,
			Multiplicative
		};

		ModifierType modifierType;
		ModifierOperation modifierOperation;
		float modifierValue;
	};

	struct PreHitCallbackReturn
	{
		bool bIgnoreHit{ false };
		std::vector<PreHitModifier> modifiers;
	};

	struct PrecisionHitData
	{
		PrecisionHitData(
			RE::Actor* a_attacker,
			RE::TESObjectREFR* a_target,
			RE::hkpRigidBody* a_hitRigidBody,
			RE::hkpRigidBody* a_hittingRigidBody,
			const RE::NiPoint3& a_hitPos,
			const RE::NiPoint3& a_separatingNormal,
			const RE::NiPoint3& a_hitPointVelocity,
			RE::hkpShapeKey a_hitBodyShapeKey,
			RE::hkpShapeKey a_hittingBodyShapeKey) :
			attacker(a_attacker),
			target(a_target),
			hitRigidBody(a_hitRigidBody),
			hittingRigidBody(a_hittingRigidBody),
			hitPos(a_hitPos),
			separatingNormal(a_separatingNormal),
			hitPointVelocity(a_hitPointVelocity),
			hitBodyShapeKey(a_hitBodyShapeKey),
			hittingBodyShapeKey(a_hittingBodyShapeKey)
		{}

		RE::Actor* attacker;
		RE::TESObjectREFR* target;
		RE::hkpRigidBody* hitRigidBody;
		RE::hkpRigidBody* hittingRigidBody;
		RE::NiPoint3 hitPos;
		RE::NiPoint3 separatingNormal;
		RE::NiPoint3 hitPointVelocity;
		RE::hkpShapeKey hitBodyShapeKey;
		RE::hkpShapeKey hittingBodyShapeKey;
	};

	enum class CollisionFilterComparisonResult : std::uint8_t
	{
		Continue,
		Collide,
		Ignore
	};

	enum class RequestedAttackCollisionType : std::uint8_t
	{
		Default,
		Current,
		RightWeapon,
		LeftWeapon
	};

	using PreHitCallback = std::function<PreHitCallbackReturn(const PrecisionHitData&)>;
	using PostHitCallback = std::function<void(const PrecisionHitData&, const RE::HitData&)>;
	using PrePhysicsStepCallback = std::function<void(RE::bhkWorld*)>;
	using CollisionFilterComparisonCallback =
		std::function<CollisionFilterComparisonResult(RE::bhkCollisionFilter*, std::uint32_t, std::uint32_t)>;

	class IVPrecision1
	{
	public:
		virtual APIResult AddPreHitCallback(
			SKSE::PluginHandle a_pluginHandle, PreHitCallback&& a_callback) noexcept = 0;
		virtual APIResult AddPostHitCallback(
			SKSE::PluginHandle a_pluginHandle, PostHitCallback&& a_callback) noexcept = 0;
		virtual APIResult AddPrePhysicsStepCallback(
			SKSE::PluginHandle a_pluginHandle, PrePhysicsStepCallback&& a_callback) noexcept = 0;
		virtual APIResult AddCollisionFilterComparisonCallback(
			SKSE::PluginHandle a_pluginHandle, CollisionFilterComparisonCallback&& a_callback) noexcept = 0;
		virtual APIResult RemovePreHitCallback(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
		virtual APIResult RemovePostHitCallback(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
		virtual APIResult RemovePrePhysicsStepCallback(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
		virtual APIResult RemoveCollisionFilterComparisonCallback(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
		virtual float GetAttackCollisionCapsuleLength(
			RE::ActorHandle a_actorHandle,
			RequestedAttackCollisionType a_collisionType = RequestedAttackCollisionType::Default) const noexcept = 0;
	};

	using RequestPluginAPI_t = void* (*)(InterfaceVersion);

	[[nodiscard]] inline void* RequestPluginAPI(InterfaceVersion a_version = InterfaceVersion::V1)
	{
		const auto module = GetModuleHandleA("Precision.dll");
		if (!module) return nullptr;
		const auto request = reinterpret_cast<RequestPluginAPI_t>(GetProcAddress(module, "RequestPluginAPI"));
		return request ? request(a_version) : nullptr;
	}
}
