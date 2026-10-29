// Hooks: Attack Warp's one engine hook and its helpers. Research and numbers: D:\Modding\skyrim_re\MOTIONWARP_RE.md.
//  MovementTweenerAgentAnimationDriven, second vtable slot 3 (SE 41160 / 1.7.104 42246) returns each frame's root motion
//  (translation at out+0 in the actor's local frame, +y forward) after ProcessMotionData and AMR. Only while a melee attack
//  runs, Attack Warp changes the forward part:
//   stretch  while winding up and swinging (attack states draw/swing, which end one frame after HitFrame), forward motion eases
//            toward a factor that grows with the gap still to close (up to fMaxScale) if the target is straight ahead and the
//            swing started within fMaxStartDist (fMaxStartDistPower for power and charge attacks);
//   turn     in the same window the attacker turns toward the target (SetHeading, as Momentum Movement does in Move);
//   brake    for the whole attack, forward motion tapers over fBrakeZone and never brings the actor's path closer than the stop
//            distance to the target's centre, so the vanilla stop-on-contact (MOTIONWARP_RE.md 6.1) does not cut the lunge.
//            With Precision the stop distance moves with the weapon's collision length.
//  Stretch and turn aim at where a moving target will be fLeadTime ahead; the brake always uses where it is.
//  Sideways and backward root motion stay as authored. Targets: an NPC's combat target; the player's is TDM's locked target,
//  else picked per swing from hostile actors inside fTargetCone of the facing (a combo keeps its target unless another is
//  clearly better).
#include "Hooks.h"
#include "Settings.h"
#include "Spring.h"

#include "API/PrecisionAPI.h"
#include "API/TDMAPI.h"

namespace AttackWarp
{
	namespace
	{
		// ---- small helpers -------------------------------------------------------------------------------------

		float      Len(float a_x, float a_y) { return std::sqrt(a_x * a_x + a_y * a_y); }
		float      Dist2D(const RE::NiPoint3& a, const RE::NiPoint3& b) { return Len(a.x - b.x, a.y - b.y); }
		const auto g_start = std::chrono::steady_clock::now();
		double     Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count(); }

		constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
		constexpr float kNoLimit = 1e9f;
		constexpr float kUnset = -2.0f;  // State::swingDist0 before the swing's first frame
		constexpr float kBodyContact = 45.0f;  // run 1: bodies touch at 41-46 centre to centre

		// "+0x7AAE50" inside the game exe, else "other.dll+0x1A" (another mod already owns the slot)
		std::string Where(std::uintptr_t a_addr)
		{
			const auto& mod = REL::Module::get();
			const auto  text = mod.segment(REL::Segment::textx);
			if (a_addr >= mod.base() && a_addr < mod.base() + text.offset() + text.size()) {
				return std::format("+0x{:X}", a_addr - mod.base());
			}
			HMODULE h = nullptr;
			char    path[MAX_PATH]{};
			if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(a_addr), &h) && h) {
				GetModuleFileNameA(h, path, MAX_PATH);
				const std::string_view p(path);
				return std::format("{}+0x{:X}", p.substr(p.find_last_of("\\/") + 1), a_addr - reinterpret_cast<std::uintptr_t>(h));
			}
			return std::format("0x{:X}", a_addr);
		}

		// Degrees the actor must turn to face a_to (Skyrim heading: 0 = +y, clockwise positive), in (-180, 180].
		float HeadingError(const RE::TESObjectREFR* a_from, const RE::NiPoint3& a_to)
		{
			const auto p = a_from->GetPosition();
			float      d = (std::atan2(a_to.x - p.x, a_to.y - p.y) - a_from->GetAngleZ()) * kRadToDeg;
			while (d > 180.0f) d -= 360.0f;
			while (d <= -180.0f) d += 360.0f;
			return d;
		}

		// Forward distance the actor may still move along its facing before its path comes within a_stop of the target
		// (ray against a circle). Unlimited when the target is behind or the path already clears it sideways.
		float ForwardRoom(float a_dist, float a_headDeg, float a_stop)
		{
			const float h = a_headDeg / kRadToDeg;
			const float along = a_dist * std::cos(h);
			const float side = std::abs(a_dist * std::sin(h));
			if (along <= 0.0f || side >= a_stop) {
				return kNoLimit;
			}
			return (std::max)(along - std::sqrt(a_stop * a_stop - side * side), 0.0f);
		}

		// 0 at x <= 0, 1 at x >= 1, smooth in between (zero slope at both ends: no jolt entering or leaving the brake zone)
		float SmoothStep(float x)
		{
			x = std::clamp(x, 0.0f, 1.0f);
			return x * x * (3.0f - 2.0f * x);
		}

		// heading in [0, 2pi) after adding a turn
		float WrapHeading(float a_rad)
		{
			constexpr float k2Pi = 2.0f * std::numbers::pi_v<float>;
			a_rad = std::fmod(a_rad, k2Pi);
			return a_rad < 0.0f ? a_rad + k2Pi : a_rad;
		}

		// stop distance for a weapon: the base (tuned for a collision of length a_ref) moves out by the extra length;
		// never inside body contact. a_capsule 0 = unknown, the base as it is.
		float StopFor(float a_base, float a_capsule, float a_ref, float a_weight)
		{
			return a_capsule > 0.0f ? (std::max)(a_base + (a_capsule - a_ref) * a_weight, kBodyContact) : a_base;
		}

		// stretch goal: none once the gap left is inside the brake zone, full fMaxScale at fStretchRange beyond it, smooth between.
		// The gap shrinks as the lunge closes in, so the stretch fades out by itself instead of handing a fast lunge to the brake.
		float StretchGoal(float a_dist, float a_stop, const Settings& c)
		{
			if (c.stretchRange <= 0.0f) {
				return c.maxScale;
			}
			return 1.0f + (c.maxScale - 1.0f) * SmoothStep((a_dist - a_stop - c.brakeZone) / c.stretchRange);
		}

		bool Melee(RE::ATTACK_STATE_ENUM a_s) { return a_s >= RE::ATTACK_STATE_ENUM::kDraw && a_s <= RE::ATTACK_STATE_ENUM::kBash; }
		bool WindUp(RE::ATTACK_STATE_ENUM a_s) { return a_s == RE::ATTACK_STATE_ENUM::kDraw || a_s == RE::ATTACK_STATE_ENUM::kSwing; }

		bool PowerOrCharge(RE::Actor* a_actor)
		{
			const auto* proc = a_actor->GetActorRuntimeData().currentProcess;
			if (!proc || !proc->high) {
				return false;
			}
			const auto atk = proc->high->attackData;
			return atk && atk->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack, RE::AttackData::AttackFlag::kChargeAttack);
		}

		// ---- optional integrations: looked up at kDataLoaded; absent = the same as without them -----------------

		PRECISION_API::IVPrecision1* g_precision = nullptr;
		TDM_API::IVTDM1*             g_tdm = nullptr;

		// True Directional Movement owns the player's yaw in directional mode and target lock; turning there would fight it
		bool TDMSteersPlayer() { return g_tdm && (g_tdm->GetDirectionalMovementState() || g_tdm->GetTargetLockState()); }

		RE::ActorHandle TDMLockedTarget(const Settings& c)
		{
			return c.tdmTarget && g_tdm && g_tdm->GetTargetLockState() ? g_tdm->GetCurrentTarget() : RE::ActorHandle{};
		}

		// ---- player target candidates --------------------------------------------------------------------------
		// Hostile actors near the player, rebuilt on the main thread (IsHostileToActor and the process lists are not safe on
		// the movement threads): on every combat state change and at most every 0.25 s while the player attacks.
		std::mutex                   g_candLock;
		std::vector<RE::ActorHandle> g_cands;
		std::atomic<double>          g_candTime{ -1.0 };
		std::atomic<bool>            g_candQueued{ false };

		void RefreshCandidates()  // main thread
		{
			std::vector<RE::ActorHandle> found;
			auto*                        pc = RE::PlayerCharacter::GetSingleton();
			auto*                        lists = RE::ProcessLists::GetSingleton();
			if (pc && lists) {
				for (auto& h : lists->highActorHandles) {
					const auto npc = h.get();
					if (!npc || npc->IsDead() || Dist2D(npc->GetPosition(), pc->GetPosition()) > 1500.0f) {
						continue;
					}
					if (npc->IsHostileToActor(pc) || npc->GetActorRuntimeData().currentCombatTarget.get().get() == pc) {
						found.push_back(h);
					}
				}
			}
			std::scoped_lock lk(g_candLock);
			g_cands = std::move(found);
			g_candTime = Now();
		}

		void RequestCandidates(double a_maxAge)
		{
			if (Now() - g_candTime.load(std::memory_order_relaxed) > a_maxAge && !g_candQueued.exchange(true)) {
				SKSE::GetTaskInterface()->AddTask([] {
					RefreshCandidates();
					g_candQueued = false;
				});
			}
		}

		// Best candidate inside the cone around the player's facing: lowest |angle| / cone + distance / fMaxStartDist.
		RE::ActorHandle PickTarget(const RE::TESObjectREFR* a_player, RE::ActorHandle a_prev, float a_maxDist)
		{
			std::vector<RE::ActorHandle> cands;
			{
				std::scoped_lock lk(g_candLock);
				cands = g_cands;
			}
			const auto& c = Cfg();
			RE::ActorHandle best;
			float       bestScore = kNoLimit;
			for (auto& h : cands) {
				const auto npc = h.get();
				if (!npc || npc->IsDead()) {
					continue;
				}
				const float d = Dist2D(npc->GetPosition(), a_player->GetPosition());
				const float ang = std::abs(HeadingError(a_player, npc->GetPosition()));
				if (ang > c.targetCone || d > a_maxDist) {
					continue;
				}
				// a combo's previous target gets a head start, so two enemies side by side do not swap between swings (run 4)
				if (const float score = ang / c.targetCone + d / a_maxDist - (h == a_prev ? c.targetStickiness : 0.0f); score < bestScore) {
					bestScore = score;
					best = h;
				}
			}
			return best;
		}

		struct CombatSink : RE::BSTEventSink<RE::TESCombatEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent*, RE::BSTEventSource<RE::TESCombatEvent>*) override
			{
				RequestCandidates(0.0);  // someone entered or left combat: rebuild on the main thread
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		CombatSink g_combatSink;

		// ---- per-actor attack state ----------------------------------------------------------------------------
		// shortcut: State fields are written only from the actor's own root-motion call, which the engine runs for one actor
		// at a time; nothing else reads them. Add a lock per State if a menu readout or another thread ever needs them.

		enum class Kind : std::uint8_t
		{
			kPlayer,
			kNPC,
			kCreature
		};

		struct State
		{
			Kind          kind{ Kind::kNPC };
			bool          inAttack{ false };  // a melee attack span: first melee frame until root motion stops
			bool          armed{ false };     // wind-up / swing of the current swing: stretch allowed
			double        tLast{ 0 };
			RE::ActorHandle targetH;        // the swing's target (player: picked per swing; NPCs: combat target)
			RE::FormID    target{ 0 };      // its FormID, for the log
			int           spanSwings{ 0 };  // swings in the current attack span (combo)
			bool          power{ false };   // power or charge attack
			float         swingDist0{ -1.0f };
			float         k{ 1.0f };   // eased stretch factor
			float         kVel{ 0 };   // its spring velocity, 1/s
			float         fb{ 1.0f };  // eased brake factor (1 = no braking)
			float         fbVel{ 0 };
			float         yawVel{ 0 };   // turn spring velocity, deg/s
			float         turned{ 0 };   // degrees turned this swing (log)
			RE::FormID    trackId{ 0 };  // target whose position is tracked for the lead
			RE::NiPoint3  tPrev{};
			float         tvx{ 0 }, tvy{ 0 };            // its smoothed velocity, units/s
			std::atomic<float> capsule{ 0.0f };  // Precision collision length (0 = unknown); written on the main thread
			// debug: per swing
			int           swing{ 0 };
			RE::NiPoint3  p0{};
			float         raw{ 0 }, out{ 0 };
			float         minFactor{ 1.0f };  // smallest brake factor seen (1 = never braked)
		};

		std::shared_mutex                                       g_stateLock;
		std::unordered_map<RE::FormID, std::unique_ptr<State>> g_states;  // never shrinks; a few hundred bytes per actor seen attacking

		State& StateOf(RE::Actor* a_actor)
		{
			const auto id = a_actor->GetFormID();
			{
				std::shared_lock lk(g_stateLock);
				if (auto it = g_states.find(id); it != g_states.end()) {
					return *it->second;
				}
			}
			auto s = std::make_unique<State>();
			if (a_actor->IsPlayerRef()) {
				s->kind = Kind::kPlayer;
			} else {
				const auto* race = a_actor->GetRace();
				s->kind = race && race->HasKeywordString("ActorTypeNPC") ? Kind::kNPC : Kind::kCreature;
			}
			std::unique_lock lk(g_stateLock);
			return *g_states.try_emplace(id, std::move(s)).first->second;
		}

		bool Allowed(RE::Actor* a_actor, const State& s, const Settings& c)
		{
			const bool who = s.kind == Kind::kPlayer ? c.player : s.kind == Kind::kNPC ? c.npcs : c.creatures;
			return who && !a_actor->IsInKillMove() && !a_actor->IsOnMount();
		}

		// Precision's collision length for the swing's weapon, read on the main thread (its header: not every frame).
		// The swing's first frames use the previous swing's value. State objects are never freed, so the pointer stays valid.
		void QueryCapsule(RE::Actor* a_actor, State& s)
		{
			if (!g_precision || !Cfg().precisionReach) {
				return;
			}
			SKSE::GetTaskInterface()->AddTask([h = a_actor->GetHandle(), st = &s] {
				const float len = g_precision->GetAttackCollisionCapsuleLength(h);
				st->capsule.store(std::isfinite(len) && len > 0.0f ? len : 0.0f, std::memory_order_relaxed);
			});
		}

		const char* KindName(Kind a_kind) { return a_kind == Kind::kPlayer ? "player" : a_kind == Kind::kNPC ? "npc" : "creature"; }

		void LogSwingEnd(RE::Actor* a_actor, const State& s, const char* a_why)
		{
			if (!spdlog::should_log(spdlog::level::debug)) {
				return;
			}
			const auto t = s.targetH.get();
			logger::debug("swing {:08X} {} #{}{} {}: target={:08X} dist {:.0f} -> {:.0f} head {:.0f} turned {:.0f} fwd {:.1f} -> {:.1f} moved {:.1f} stretch x{:.2f} "
						  "min brake {:.2f} capsule {:.0f} target speed {:.0f}",
				a_actor->GetFormID(), KindName(s.kind), s.swing, s.power ? " power" : "", a_why, s.target, s.swingDist0,
				t ? Dist2D(t->GetPosition(), a_actor->GetPosition()) : -1.0f, t ? HeadingError(a_actor, t->GetPosition()) : 0.0f, s.turned, s.raw, s.out,
				Dist2D(a_actor->GetPosition(), s.p0), s.k, s.minFactor, s.capsule.load(std::memory_order_relaxed), Len(s.tvx, s.tvy));
		}

		// ---- root motion: MovementTweenerAgentAnimationDriven slot 3 ------------------------------------------------

		struct TweenHook
		{
			// out: NiPoint3 translation (+0x00, local: +y forward), NiMatrix3 rotation (+0x0C); this+0x18 = actor handle; ctx+0x1C = frame time
			static void thunk(void* a_this, void* a_ctx, float* a_out)
			{
				func(a_this, a_ctx, a_out);
				const auto& c = Cfg();
				if (!c.enabled || !a_this || !a_ctx || !a_out) {
					return;
				}
				const auto handle = *reinterpret_cast<const RE::ObjectRefHandle*>(static_cast<const std::byte*>(a_this) + 0x18);
				const auto ref = handle.get();
				auto*      a = ref ? ref->As<RE::Actor>() : nullptr;
				const auto* st = a ? a->AsActorState() : nullptr;
				if (!st) {
					return;
				}
				const auto atk = st->GetAttackState();
				const bool melee = Melee(atk);
				// slot 3 only runs while root motion is on; nothing to do outside attacks for actors not mid-attack
				if (!melee && !a->IsPlayerRef()) {
					std::shared_lock lk(g_stateLock);
					if (!g_states.contains(a->GetFormID())) {
						return;
					}
				}
				State&       s = StateOf(a);
				const double now = Now();
				const float  dt = *reinterpret_cast<const float*>(static_cast<const std::byte*>(a_ctx) + 0x1C);

				// the attack span ends when root motion pauses (no call for 0.25 s); BFCO recovery frames (state 0) stay inside it
				if (s.inAttack && now - s.tLast > 0.25) {
					if (s.armed) {
						LogSwingEnd(a, s, "interrupted");
					}
					s.inAttack = s.armed = false;
					s.k = s.fb = 1.0f;
					s.kVel = s.fbVel = s.yawVel = 0.0f;
					s.spanSwings = 0;
					s.trackId = 0;
				}
				if (melee) {
					s.inAttack = true;
				}
				if (!s.inAttack) {
					return;
				}
				s.tLast = now;
				if (!Allowed(a, s, c)) {
					s.armed = false;
					return;
				}

				// swing boundaries from the attack state: draw/swing = before the hit (ends one frame after HitFrame)
				const bool windUp = WindUp(atk);
				if (windUp && !s.armed) {
					const bool combo = s.spanSwings++ > 0;
					s.armed = true;
					++s.swing;
					s.power = PowerOrCharge(a);
					s.turned = 0.0f;
					s.yawVel = 0.0f;
					s.p0 = a->GetPosition();
					s.raw = s.out = 0.0f;
					s.minFactor = 1.0f;
					s.swingDist0 = kUnset;
					if (s.kind == Kind::kPlayer) {
						RequestCandidates(0.25);
						const auto prev = combo ? s.targetH : RE::ActorHandle{};
						s.targetH = TDMLockedTarget(c);
						if (!s.targetH) {
							s.targetH = PickTarget(a, prev, s.power ? c.maxStartDistPower : c.maxStartDist);
						}
					}
					QueryCapsule(a, s);
				} else if (!windUp && s.armed) {
					s.armed = false;
					LogSwingEnd(a, s, "hit phase");
				}

				if (s.kind != Kind::kPlayer) {
					s.targetH = a->GetActorRuntimeData().currentCombatTarget;
				}
				const auto hold = s.targetH.get();  // held for the frame: the target can unload meanwhile
				RE::Actor* tgt = hold && !hold->IsDead() ? hold.get() : nullptr;
				s.target = tgt ? tgt->GetFormID() : 0;
				const auto pos = a->GetPosition();
				const auto tpos = tgt ? tgt->GetPosition() : RE::NiPoint3{};

				// target velocity from its own position track, smoothed over ~0.1 s, for the lead
				if (tgt && s.trackId == s.target && dt > 0.0f) {
					const float w = 1.0f - std::exp(-dt / 0.1f);
					s.tvx += ((tpos.x - s.tPrev.x) / dt - s.tvx) * w;
					s.tvy += ((tpos.y - s.tPrev.y) / dt - s.tvy) * w;
				} else {
					s.tvx = s.tvy = 0.0f;
				}
				s.trackId = s.target;
				s.tPrev = tpos;
				// aim point: where the target will be fLeadTime ahead, at most fMaxLead away (wind-up and swing only)
				RE::NiPoint3 aim = tpos;
				if (s.armed && c.leadTime > 0.0f) {
					float       lx = s.tvx * c.leadTime, ly = s.tvy * c.leadTime;
					const float l = Len(lx, ly);
					if (l > c.maxLead) {
						lx *= c.maxLead / l;
						ly *= c.maxLead / l;
					}
					aim.x += lx;
					aim.y += ly;
				}

				// turn toward the aim point while winding up and swinging; the follow-through keeps the authored facing.
				// Heading error and SetHeading share GetAngleZ's convention, so the step's sign is right by construction.
				const bool mayTurn = s.kind != Kind::kPlayer || (c.turnPlayer && !TDMSteersPlayer());
				if (c.turn && mayTurn && s.armed && tgt && dt > 0.0f) {
					const float err = HeadingError(a, aim);
					if (std::abs(err) <= c.maxTurnAngle) {
						float step = err;
						if (c.turnEase > 0.0f) {
							step = 0.0f;
							Spring::Damper(step, s.yawVel, err, Spring::HalflifeFromEase(c.turnEase), dt);
							s.yawVel = std::clamp(s.yawVel, -c.turnRate, c.turnRate);
						}
						step = std::clamp(step, -c.turnRate * dt, c.turnRate * dt);
						a->SetHeading(WrapHeading(a->GetAngleZ() + step / kRadToDeg));
						s.turned += step;
					}
				}

				const float dist = tgt ? Dist2D(tpos, pos) : -1.0f;
				const float head = tgt ? HeadingError(a, tpos) : 0.0f;
				const float aimDist = tgt ? Dist2D(aim, pos) : -1.0f;
				const float aimHead = tgt ? HeadingError(a, aim) : 0.0f;
				if (s.swingDist0 == kUnset) {
					s.swingDist0 = dist;  // first frame of the swing; -1 = no target
				}
				const float stop = StopFor(s.kind == Kind::kPlayer ? c.stopDistPlayer : c.stopDistNpc,
					c.precisionReach ? s.capsule.load(std::memory_order_relaxed) : 0.0f, c.reachRef, c.reachWeight);

				// stretch target through a critically damped spring: the lunge speeds up and settles back with no jump in
				// speed or in acceleration onset (0.1.0's first-order ease still kicked in at full rate on the first frame)
				const float maxStart = s.power ? c.maxStartDistPower : c.maxStartDist;
				const bool  stretch = c.stretch && s.armed && tgt && s.swingDist0 >= 0.0f && s.swingDist0 <= maxStart && std::abs(aimHead) <= c.maxStretchAngle;
				const float kT = stretch ? StretchGoal(aimDist, stop, c) : 1.0f;
				if (c.stretchEase <= 0.0f) {
					s.k = kT;
					s.kVel = 0.0f;
				} else {
					Spring::Damper(s.k, s.kVel, kT, Spring::HalflifeFromEase(c.stretchEase), (std::max)(dt, 0.0f));
				}

				const float rawY = a_out[1];
				if (rawY <= 0.0f) {
					return;  // stepping back or standing: as authored
				}
				float y = rawY * s.k;
				if (c.brake && tgt) {
					// the brake factor is sprung too: when the target steps in or its body comes into the path, the room
					// jumps and smoothstep jumps with it; the spring turns that into a quick taper. The clamp to the room
					// stays as the backstop, so lag never pushes into the body.
					const float room = ForwardRoom(dist, head, stop);
					const float fT = room < kNoLimit && c.brakeZone > 0.0f ? SmoothStep(room / c.brakeZone) : 1.0f;
					if (c.brakeEase <= 0.0f) {
						s.fb = fT;
						s.fbVel = 0.0f;
					} else {
						Spring::Damper(s.fb, s.fbVel, fT, Spring::HalflifeFromEase(c.brakeEase), (std::max)(dt, 0.0f));
						s.fb = std::clamp(s.fb, 0.0f, 1.0f);
					}
					s.minFactor = (std::min)(s.minFactor, s.fb);
					y = (std::min)(y * s.fb, room);
				}
				a_out[1] = y;
				s.raw += rawY;
				s.out += y;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void InstallHooks()
	{
		// self-check of the brake geometry and ease curve (cases checked by hand against probe run 2)
		// spring: a 1 -> 1.8 stretch at ease 0.12 s covers 63% in 0.12 s and never overshoots
		const Settings t;  // built-in defaults: brake zone 60, stretch range 150, max scale 1.8
		float          k = 1.0f, kv = 0.0f, kPeak = 1.0f;
		for (int i = 0; i < 12; ++i) {
			Spring::Damper(k, kv, 1.8f, Spring::HalflifeFromEase(0.12f), 0.01f);
			kPeak = (std::max)(kPeak, k);
		}
		const bool mathOk = std::abs(k - (1.0f + 0.8f * 0.632f)) < 0.01f && kPeak <= 1.8f
		                 && std::abs(ForwardRoom(100.0f, 0.0f, 70.0f) - 30.0f) < 0.01f  // target dead ahead
		                 && ForwardRoom(36.5f, 79.0f, 70.0f) == 0.0f                    // beside and too close: no forward at all
		                 && ForwardRoom(100.0f, 120.0f, 70.0f) == kNoLimit               // behind: unlimited
		                 && ForwardRoom(200.0f, 30.0f, 70.0f) == kNoLimit                // path clears it sideways (100 > 70)
		                 && SmoothStep(0.5f) == 0.5f && SmoothStep(-1.0f) == 0.0f && SmoothStep(2.0f) == 1.0f
		                 && StopFor(70.0f, 0.0f, 60.0f, 1.0f) == 70.0f && StopFor(70.0f, 90.0f, 60.0f, 1.0f) == 100.0f  // unknown; longer blade
		                 && StopFor(70.0f, 20.0f, 60.0f, 1.0f) == kBodyContact                                        // never inside the body
		                 && StretchGoal(130.0f, 70.0f, t) == 1.0f && std::abs(StretchGoal(130.0f + t.stretchRange, 70.0f, t) - t.maxScale) < 1e-5f
		                 && std::abs(WrapHeading(-0.5f) - (2.0f * std::numbers::pi_v<float> - 0.5f)) < 1e-5f;
		if (!mathOk) {
			logger::error("self-check of the warp math FAILED: report this with the log");
		}

		// second vtable (this-offset 0x18), slot 3 = SE 41160 / 1.7.104 42246; both call ProcessMotionData (31949 / 32703)
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_MovementTweenerAgentAnimationDriven[1] };
		TweenHook::func = vtbl.write_vfunc(3, TweenHook::thunk);
		const auto where = Where(TweenHook::func.address());
		if (where.starts_with('+')) {
			logger::info("hook MovementTweenerAgentAnimationDriven::ProcessMotion slot 0x3: vanilla {}", where);
		} else {
			logger::warn("hook MovementTweenerAgentAnimationDriven::ProcessMotion slot 0x3: already replaced by {} (chained; disable MotionWarpProbe if it is that)",
				where);
		}
	}

	void RegisterEvents()
	{
		g_precision = static_cast<PRECISION_API::IVPrecision1*>(PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V1));
		logger::info("Precision: {}", g_precision ? "found, stop distances follow the weapon's collision length" : "not loaded, fixed stop distances");
		g_tdm = static_cast<TDM_API::IVTDM1*>(TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V1));
		logger::info("True Directional Movement: {}", g_tdm ? "found, its locked target is the player's target; no player turning while it steers" : "not loaded");

		if (auto* src = RE::ScriptEventSourceHolder::GetSingleton()) {
			src->AddEventSink<RE::TESCombatEvent>(&g_combatSink);
			logger::info("combat event sink registered");
		} else {
			logger::error("combat event sink NOT registered: ScriptEventSourceHolder missing (the player's target list refreshes only while attacking)");
		}
	}
}
