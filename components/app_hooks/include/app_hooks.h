// Where the app plugs together — the shell's extension points, so the shell never names a feature (APP_ARCHITECTURE
// §1), and the core's own boot steps go in the way a feature's do (APP_ARCHITECTURE §3).
//
// A feature is a component listed in main/app_features.def. main calls app_shell::Install() — the core's own
// registrations — then each feature's <name>::Install(), then app_shell::Run(). An Install() only registers here — no
// hardware, no tasks, nothing drawn — and the shell runs what was registered as it boots.
//
// Single owner, no lock: registration happens on the main task before app_shell::Run(), which calls Seal() before the
// first read. A registration after Seal() is logged and ignored. The tables are fixed-size; a registration past one is
// logged and dropped, naming the table — raise its size here.

#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

namespace app_hooks {

// The points in app_shell::Run() where registered steps run, in boot order and, within one, in registration order —
// the core's first. All on the main task.
//   kBoardUp  the board is up: its rails, buses and devices; nothing is drawn yet
//   kReady    the shell is up and home is on screen; start a feature's services and tasks here
//   kLate     the last thing in Run()
enum class Phase : uint8_t { kBoardUp, kReady, kLate };

inline constexpr size_t kMaxBootSteps = 16;

// --- The registering side: called from an Install() -----------------------------------------------------------

void OnBoot(Phase phase, void (*step)());

// What a feature adds to the home screen, built under the LVGL lock after the shell's own content.
inline constexpr size_t kMaxHomeBuilders = 8;
void OnHomeBuilt(void (*build)(lv_obj_t *home));

// A feature that owns the whole home screen — its own regions, with no status bar of the shell's — claims it with its screen's
// four calls, plain functions so this header names no router type (APP_ARCHITECTURE §2, §7). The shell makes the router's root
// from them in place of its own home: `create` builds under the LVGL lock into the root the router made, its focus group bound,
// as router::Screen::OnCreate does; `enter`, `exit` and `covered` are the screen's lifecycle (router/screen.h) and each may be
// nullptr. One feature may claim home; a second claim is logged and ignored, and the first one's screen is home.
struct HomeScreen {
    void (*create)(lv_obj_t *root, lv_group_t *group);
    void (*enter)();
    void (*exit)();
    void (*covered)(bool covered);
};
void ClaimHome(const HomeScreen &screen);

// --- The shell's side: called from app_shell::Run() -----------------------------------------------------------

void Seal();
void RunBoot(Phase phase);
void BuildHome(lv_obj_t *home);
// The claimed home, or nullptr when no feature claimed it: the shell then draws its own.
const HomeScreen *ClaimedHome();

}  // namespace app_hooks
