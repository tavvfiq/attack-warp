# STATE - Attack Warp
Goal: eased motion warping for melee lunges (player + NPCs). Research: D:\Modding\skyrim_re\MOTIONWARP_RE.md; probe: D:\Modding\MotionWarpProbe (0.2.2 known good).
Done: 0.1.0, 0.2.0 built and packaged. 0.3.0 (2026-10-29): stretch ramp by gap, turn toward target, target lead, combo stickiness,
TDM lock target, Precision-scaled stop distance, power/charge start distance; built (no warnings), packaged (releases/0.3.0).
None of 0.1.0-0.3.0 has run in game.
Next: user installs 0.3.0 (MotionWarpProbe disabled), Debug log on, fights 1v1 and a group, with and without TDM lock-on.
Check in AttackWarp.log: "self-check ... FAILED" absent; "Precision: found"; swing lines' capsule values (calibrate fReachRef
to the one-handed sword's length); turned degrees; NPCs not spinning. Feel: snap gone? turning natural or robotic?
If the turn misbehaves: bTurn=0 isolates it. If anything crashes on load: roll back to 0.2.0 zip.
Last known good: none yet (untested). Rollback = releases/0.2.0 zip (source zip there too) or 0.1.0.
Log: Documents\My Games\Skyrim Special Edition\SKSE\AttackWarp.log
Build: Git Bash, `xmake f -p windows -a x64 -c -m releasedbg -y && xmake -y`; DLL at build/windows/x64/releasedbg.
