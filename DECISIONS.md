# Decisions
- Hook point: tweener slot 3 post-call (after ProcessMotionData and AMR), same as MotionWarpProbe. Translation is local: +y forward.
- Swing boundaries from attack state (draw/swing armed; hit phase disarms). Run 4: state 2 -> 3 comes 12 ms after HitFrame for player and NPCs,
  so no animation event sinks are needed.
- Easing (user: "sometimes it feels like snapping"): stretch factor follows its target with an exponential ease (fStretchEase);
  brake multiplies forward motion by smoothstep(room / fBrakeZone), with a hard clamp to the room as a backstop.
- Brake geometry: ray along the facing vs a circle of the stop distance around the target (no pushing in, no sliding past).
- Stop distances from run 2: player 70 (vanilla contact 40-52), NPC 100 (NPC contact 43-108).
- Player target: cone pick (60 deg) per swing, run 4 matched the hit victim 33/38. NPCs: combat target.
- Player candidates are rebuilt on the main thread (combat events + every 0.25 s while attacking): IsHostileToActor and the process lists are main-thread data.
- Settings and menu follow Momentum Movement (double-buffered Cfg, Visit table, SKSE Menu Framework per user preference).
- No turning in 0.1.0: not measured as needed; keeps root rotation as authored.
- 0.2.0 Springs (orangeduck spring.h) on the stretch factor and the brake factor; fStretchEase keeps its 63%-time meaning
  (halflife = 0.646 x ease). The brake spring only smooths the factor; min(y, room) stays as the hard backstop.
- 0.3.0 Stretch goal ramps with the gap left (SmoothStep over fStretchRange beyond stop + brake zone): a constant 1.8x then a hard
  brake was the likely snap on close swings. fStretchRange 0 restores 0.2.0.
- 0.3.0 Turn via Actor::SetHeading inside the hook, not the `out` rotation matrix: Momentum Movement's NPC limiter proved SetHeading
  in the actor's own motion update (INERTIA_RE.md mode 4); the matrix's sign convention was never measured. Sign is right by
  construction (HeadingError and SetHeading both use GetAngleZ). Only during draw/swing.
- 0.3.0 TDM: only V1 reads (lock state, directional state, locked target). V2 yaw control is refused off TDM's thread (BadThread),
  so the player is not turned while TDM steers.
- 0.3.0 Precision: GetAttackCollisionCapsuleLength is the weapon collision length (Fenix runtime: 48-76), not centre-to-centre reach
  (engine 103-147), so it shifts the stop distance (base + (len - fReachRef) x weight) instead of replacing it. Main-thread
  query once per swing (its header: not every frame). Both upstream repos are GPL-3.0; Attack Warp is GPL-3.0-or-later.
- 0.3.0 Lead from the target's own smoothed position track (no engine velocity API off the main thread); brake ignores the lead.
