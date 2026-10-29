-- Build script for AttackWarp.dll: SKSE plugin that bends melee attack root motion so lunges reach their target and stop
-- short of its body, with eased stretch and brake. Design and evidence: D:\Modding\skyrim_re\MOTIONWARP_RE.md (probe runs 1-4).
-- One DLL for SE 1.5.97 and AE (vtable hook resolved through Address Library vtable IDs).
-- From Git Bash the platform must be forced:  xmake f -p windows -a x64 -c -m releasedbg -y && xmake -y
set_xmakever("3.0.0")

set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)

-- CommonLibSSE-NG checkout (override with COMMONLIBSSE_NG_DIR)
local commonlibsse_ng_dir = os.getenv("COMMONLIBSSE_NG_DIR") or "D:/Modding/commonlibsse-xmake/CommonLibSSE-NG"
includes(path.join(commonlibsse_ng_dir, "xmake.lua"))

set_project("AttackWarp")
set_version("0.3.0")
set_license("GPL-3.0-or-later")

set_languages("c++23")
set_warnings("allextra")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

target("AttackWarp")
    add_deps("commonlibsse-ng")
    add_rules("commonlibsse-ng.plugin", {
        name = "AttackWarp",
        author = "taufiq",
        description = "Melee lunges reach their target and stop short of its body, eased (SE 1.5.97 / AE)"
    })
    set_pcxxheader("src/PCH.h")
    add_includedirs("src")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
target_end()
