# Attack Warp 0.3.0

SKSE plugin for Skyrim SE 1.5.97 and AE (tested on 1.7.104). Melee lunges reach their target and stop short of its body,
eased so they neither snap forward nor hit a wall.

## What it does
Each frame of a melee attack, the engine hands out the animation's root motion. Attack Warp changes only its forward part:
- **Stretch:** while winding up and swinging, a lunge that would fall short grows, by how much gap is left: none inside the
  brake zone, up to `fMaxScale` (1.8x) when the gap is `fStretchRange` (150) beyond it. Only if the target is within
  `fMaxStretchAngle` (30 deg) of straight ahead and the swing started within `fMaxStartDist` (450; power and charge attacks
  `fMaxStartDistPower` 700). Spring-eased over `fStretchEase` (0.12 s), so speed never jumps.
- **Turn:** in the same window the attacker turns toward its target, up to `fTurnRate` (300 deg/s), spring-eased. Not the
  player while True Directional Movement steers (directional mode or lock-on): TDM handles that facing.
- **Lead:** stretch and turn aim at where a moving target will be `fLeadTime` (0.25 s) ahead, so a backpedalling target is reached.
- **Brake:** for the whole attack the forward path never comes closer than the stop distance (player 70, NPCs 100) to the
  target's centre. Speed tapers over `fBrakeZone` (60 units) before that line;
  sudden changes (target steps in) are smoothed over `fBrakeEase` (0.05 s) without ever crossing the line. This replaces the vanilla stop-on-contact,
  which freezes a lunge the moment it touches the target.
- **Targets:** NPCs use their combat target. The player: TDM's locked target if locked, else one per swing, the hostile actor
  inside `fTargetCone` (60 deg) of your facing, by angle and distance; in a combo the previous target stays unless another is
  clearly better (`fTargetStickiness`). Nobody inside = vanilla swing.
- **Precision:** if installed, the stop distance moves with the weapon's attack collision length, so greatswords stop further out
  than daggers (`fReachRef`, `fReachWeight`).
Sideways and backward motion, kill moves and mounted attacks stay vanilla.

Measured in MotionWarpProbe (research notes `D:\Modding\skyrim_re\MOTIONWARP_RE.md`, runs 1-4): player lunges stopped getting
stuck on bodies (10 of 26 swings stuck in vanilla, 2 of 22 warped), 2 s charges ended 93-126 away instead of 50-468,
NPC hit rate in a group fight 67% vanilla vs 89% warped.

## Requirements
- SKSE64 and Address Library for SKSE Plugins.
- Optional: SKSE Menu Framework for the in-game settings page (without it, edit `AttackWarp_User.ini`).
- Optional: Precision (weapon-length stop distance), True Directional Movement (lock-on target). Detected at load; the log says which.

## Install
Install `AttackWarp-0.3.0.zip` with your mod manager. It adds `SKSE/Plugins/AttackWarp.dll` and `AttackWarp.ini`.
**Disable MotionWarpProbe**: it hooks the same function.

## Settings
In game: SKSE Menu Framework, section "Attack Warp". Changes apply at once and are saved to `Data/SKSE/Plugins/AttackWarp_User.ini`.
Feels like snapping: raise Stretch ease and Brake zone. Turning feels robotic: raise Turn ease or lower Turn rate; `bTurn=0` turns it off. Lunges too short: raise Max stretch. Stops too far away: lower the stop distance.

## Compatibility
- Animation Motion Revolution (AMR): works with it; Attack Warp sees the motion after AMR.
- Animation Motion Fix (AMF): works with or without it. AMF switches off the vanilla stop-on-contact; Attack Warp's brake
  then is the only thing keeping lunges out of the target's body.
- BFCO / MCO / SCAR movesets: tested with BFCO (probe).
- True Directional Movement: lock-on target used; player turning left to TDM while it steers.
- Precision: read-only use of its API (collision length); no callbacks.
- Another mod that replaces the same function is chained; the log names it.

## Log
`Documents\My Games\Skyrim Special Edition\SKSE\AttackWarp.log` (rewritten each launch). Startup, settings and the hook
always; `bDebugLog=1` (or the menu) adds one line per swing: target, distance at start and at the hit, degrees turned, stretch, brake, Precision length ("capsule") and target speed.

## Build
Git Bash: `xmake f -p windows -a x64 -c -m releasedbg -y && xmake -y` (CommonLibSSE-NG at D:/Modding/commonlibsse-xmake).

## License
GPL-3.0-or-later (links CommonLibSSE-NG).
