# Validation

## 0.1.0 (2026-10-27)
- Build: `xmake f -p windows -a x64 -c -m releasedbg -y && xmake -y` from Git Bash: build ok, no warnings in Attack Warp sources
  (one C4701 inside the minhook dependency).
- Static: hook slot = MotionWarpProbe's (runtime-proven runs 1-4 on 1.7.104). Math self-check at load (ForwardRoom, SmoothStep).
- Runtime: NOT yet run.
- 12e3a9a0791dbf9ac3b5627f24dfc9bcdc40163ddd1968a8c4c2a966665d750e *releases/0.1.0/AttackWarp-0.1.0-source.zip
- e9f0b0c8504aa3955b2c6129c59890032c087401c249f2e4c66fb9dfbaacb4e4 *releases/0.1.0/AttackWarp-0.1.0.zip
- a2bc182432770b6d04e87a7a0f2811bc0116875ba569b6a9de1f836b0869ad24 *build/windows/x64/releasedbg/AttackWarp.dll

## 0.2.0 (2026-10-27)
- Build: full rebuild ok, no warnings in Attack Warp sources. Hook unchanged from 0.1.0 (math only).
- Static: INI keys vs code 16/16; spring replica in Python (63% at the ease time, no overshoot, frame-rate independent);
  load-time self-check extended with the spring. Package zip test ok, bytes match the build.
- Runtime: NOT yet run.
- 6861f75b4acbf33e547585098e3fd963c858f6a2e2496b9c9d39e02c5c5b0e3e *releases/0.2.0/AttackWarp-0.2.0.zip
- b1fb4e3f2612522b78f3c0d03b688c25b49bc637b307017676a6c7fab2e8468e *build/windows/x64/releasedbg/AttackWarp.dll

## 0.3.0 (2026-10-29)
- Build: full rebuild ok, no warnings in Attack Warp sources. Hook slot unchanged.
- Static: INI keys vs code 30/30. API vtables: TDM V1 prefix matches upstream TrueDirectionalMovementAPI.h (fetched 2026-10-29);
  Precision V1 subset = FenixCombatAI-NG's (runtime-read 48-76 there). Load-time self-check extended (StopFor, StretchGoal, WrapHeading).
  Python replica: turn spring settles under 1 deg in 0.18-0.29 s for 20-60 deg errors, peak 300 deg/s, no overshoot;
  stretch goal 1.0 at gap <= 130, 1.36 at 200, 1.8 from 280 (player defaults).
- Package zip test ok, DLL bytes match the build.
- Runtime: NOT yet run.
- 6fe0af07cce0608e41a88732f916be7d92e0c35db2c6adfdf7ae05981ec10d43 *releases/0.3.0/AttackWarp-0.3.0.zip
- 52b1ebb5c6022c8f1892f05fb082bb3220184fe61b3eb8b1a1ccfb53d3b5f677 *build/windows/x64/releasedbg/AttackWarp.dll
- f963ae90042f7cb645f974e5de1c74e78181eb6fa386671a98ae71aaced678e8 *releases/0.2.0/AttackWarp-0.2.0-source.zip (made 2026-10-29 before the 0.3.0 edits)
