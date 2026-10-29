// Menu: the in-game settings page (SKSE Menu Framework, section "Attack Warp"). Changes apply immediately and are saved
// to AttackWarp_User.ini once no slider is being dragged. Without the framework the INI files still work.
#include "Menu.h"
#include "Settings.h"

// The vendored SKSE Menu Framework header uses <codecvt> (deprecated in C++17) and has a few enum/struct-tag quirks.
#pragma warning(push)
#pragma warning(disable: 4996 4099 5054)
#include "API/SKSEMenuFramework.h"
#pragma warning(pop)

namespace AttackWarp::Menu
{
	namespace
	{
		namespace UI = ImGuiMCP;

		Settings g_work;             // the copy the widgets edit (render thread)
		bool     g_changed = false;  // published but not saved yet

		void Tip(const char* a_text)
		{
			if (UI::BeginItemTooltip()) {
				UI::PushTextWrapPos(UI::GetFontSize() * 30.0f);
				UI::TextUnformatted(a_text);
				UI::PopTextWrapPos();
				UI::EndTooltip();
			}
		}

		void Check(const char* a_label, bool* a_value, const char* a_tip)
		{
			g_changed |= UI::Checkbox(a_label, a_value);
			Tip(a_tip);
		}

		void Slider(const char* a_label, float* a_value, float a_min, float a_max, const char* a_fmt, const char* a_tip)
		{
			UI::SetNextItemWidth(UI::GetFontSize() * 12.0f);
			g_changed |= UI::SliderFloat(a_label, a_value, a_min, a_max, a_fmt);
			Tip(a_tip);
		}

		void __stdcall DrawSettings()
		{
			UI::TextUnformatted("Melee lunges reach their target and stop short of its body. Distances are centre to centre in game units.");

			UI::SeparatorText("General");
			Check("Enabled", &g_work.enabled, "Master switch. Off = vanilla attack movement for everyone.");
			Check("Player", &g_work.player, "Warp the player's melee attacks.");
			Check("NPCs", &g_work.npcs, "Warp humanoid NPCs' melee attacks toward their combat target.");
			Check("Creatures", &g_work.creatures, "Warp animals and monsters too. Off by default: not measured yet.");
			Check("Debug log", &g_work.debugLog, "One line per swing in AttackWarp.log (target, distances, stretch, brake).");

			UI::SeparatorText("Target");
			Slider("Target cone", &g_work.targetCone, 10.0f, 180.0f, "%.0f deg",
				"Player: at each swing the hostile actor inside this half-angle of your facing is the target (angle and distance decide). "
				"Nobody inside = vanilla swing.");
			Slider("Max start distance", &g_work.maxStartDist, 100.0f, 1500.0f, "%.0f",
				"Swings that start further from the target than this are not stretched or picked. Weapon reach is about 145.");
			Slider("Max start distance (power)", &g_work.maxStartDistPower, 100.0f, 1500.0f, "%.0f",
				"The same for power and charge (sprint) attacks, whose lunges are longer.");
			Slider("Combo stickiness", &g_work.targetStickiness, 0.0f, 2.0f, "%.2f",
				"Player combos: the previous swing's target stays the target unless another one scores this much better. "
				"Stops the lunge swapping between two enemies standing side by side. 0 = re-pick freely.");
			Check("Use TDM lock-on target", &g_work.tdmTarget, "With True Directional Movement's target lock on, the locked target is the player's target.");

			UI::SeparatorText("Stretch (reach the target)");
			Check("Stretch", &g_work.stretch, "Before the hit, a lunge that would fall short covers more ground.");
			Slider("Max stretch", &g_work.maxScale, 1.0f, 3.0f, "x%.2f", "Most a lunge's forward movement can grow. 1.8 brought 2 s charges from up to 468 away into reach.");
			Slider("Only within", &g_work.maxStretchAngle, 0.0f, 90.0f, "%.0f deg", "Stretch only when the target is this close to straight ahead.");
			Slider("Stretch ease", &g_work.stretchEase, 0.0f, 0.5f, "%.2f s",
				"How gradually the stretch comes in and goes out. 0 = instant (feels like snapping); higher = smoother but reaches less.");
			Slider("Stretch range", &g_work.stretchRange, 0.0f, 500.0f, "%.0f",
				"Full stretch only when the gap left is this much beyond the brake zone; closer, it fades out. "
				"Close swings play as animated. 0 = always full stretch (0.2.0).");
			Slider("Lead time", &g_work.leadTime, 0.0f, 1.0f, "%.2f s",
				"Stretch and turn aim at where a moving target will be this far ahead, so a backpedalling target is still reached. 0 = off.");
			Slider("Max lead", &g_work.maxLead, 0.0f, 300.0f, "%.0f", "The lead never moves the aim point further than this.");

			UI::SeparatorText("Turn (face the target)");
			Check("Turn", &g_work.turn, "While winding up and swinging, the attacker turns toward its target. The follow-through keeps its animated facing.");
			Check("Turn the player", &g_work.turnPlayer,
				"Also the player. Skipped while True Directional Movement steers you (directional mode or lock-on), which already does its own facing.");
			Slider("Turn rate", &g_work.turnRate, 0.0f, 1080.0f, "%.0f deg/s", "Fastest turn.");
			Slider("Turn ease", &g_work.turnEase, 0.0f, 0.5f, "%.2f s", "How gradually the turn starts and settles. 0 = rate limit only.");
			Slider("Max turn angle", &g_work.maxTurnAngle, 0.0f, 180.0f, "%.0f deg", "Targets further off your facing than this are not turned to.");

			UI::SeparatorText("Brake (stop short of the body)");
			Check("Brake", &g_work.brake, "The forward path never comes closer than the stop distance, so lunges do not push into or slide past the target.");
			Slider("Stop distance (player)", &g_work.stopDistPlayer, 30.0f, 200.0f, "%.0f",
				"Where your forward movement stops. Bodies touch at about 45; reach is about 145.");
			Slider("Stop distance (NPCs)", &g_work.stopDistNpc, 30.0f, 200.0f, "%.0f", "Where an NPC's forward movement stops. NPCs touch you further out (up to about 108).");
			Slider("Brake zone", &g_work.brakeZone, 0.0f, 200.0f, "%.0f",
				"Forward speed tapers smoothly over this distance before the stop. 0 = hard stop at the line (feels like hitting a wall).");
			Slider("Brake ease", &g_work.brakeEase, 0.0f, 0.3f, "%.2f s",
				"Smooths sudden brake changes when the target steps in or moves into your path. 0 = none. The stop distance always holds.");
			Check("Precision weapon reach", &g_work.precisionReach,
				"With Precision installed, the stop distance moves with the weapon's collision length: longer weapons stop further out.");
			Slider("Reference length", &g_work.reachRef, 0.0f, 300.0f, "%.0f",
				"The Precision collision length the stop distances above are for. The debug log shows each swing's length ('capsule').");
			Slider("Reach weight", &g_work.reachWeight, 0.0f, 2.0f, "x%.2f", "Stop distance change per unit of extra length. 1 = one to one; 0 = ignore Precision.");

			UI::Separator();
			if (UI::Button("Reset to defaults")) {
				g_work = DefaultSettings();
				g_changed = true;
			}
			Tip("Back to the values in AttackWarp.ini. Saved like any other change.");

			if (g_changed) {
				Publish(g_work);
				// save once the drag ends; file writes go to the main thread
				if (!UI::IsAnyItemActive()) {
					g_changed = false;
					SKSE::GetTaskInterface()->AddTask([] { SaveSettings(); });
				}
			}
		}
	}

	void Register()
	{
		g_work = Cfg();
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("SKSE Menu Framework not installed: no in-game menu (edit AttackWarp_User.ini instead)");
			return;
		}
		SKSEMenuFramework::SetSection("Attack Warp");
		SKSEMenuFramework::AddSectionItem("Settings", DrawSettings);
		logger::info("SKSE Menu Framework: Attack Warp / Settings registered");
	}
}
