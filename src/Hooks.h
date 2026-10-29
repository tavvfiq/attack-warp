// Hooks: the root-motion hook that warps melee lunges, and the combat event that refreshes the player's target list.
#pragma once

namespace AttackWarp
{
	void InstallHooks();    // kPostLoad
	void RegisterEvents();  // kDataLoaded
}
