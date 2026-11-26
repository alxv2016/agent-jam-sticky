#include "screen_home.h"

#include "app_hooks.h"
#include "router/screen.h"
#include "ui.h"
#include "lvgl.h"

namespace screen_home {
namespace {

// What a feature that claimed home (app_hooks::ClaimHome) gets, as the router's calls reach it.
void Create(lv_obj_t *home, lv_group_t *group, void *user)
{
    ui::StyleScreen(home);  // the glass, a column, no scrolling: the claimed screen places its own regions
    static_cast<const app_hooks::HomeScreen *>(user)->create(home, group);
}
void Enter(void *user)
{
    if (auto enter = static_cast<const app_hooks::HomeScreen *>(user)->enter) {
        enter();
    }
}
void Exit(void *user)
{
    if (auto exit = static_cast<const app_hooks::HomeScreen *>(user)->exit) {
        exit();
    }
}
void Covered(bool covered, void *user)
{
    if (auto cover = static_cast<const app_hooks::HomeScreen *>(user)->covered) {
        cover(covered);
    }
}

}  // namespace

void Build(lv_obj_t *home, lv_group_t *, void *)
{
    ui::StyleScreen(home);  // the glass, a column, no scrolling
    ui::NewStatusBar(home, "Agent Jam Sticky");  // ux-exception: the product's name, a proper noun

    // The shell's own content on home goes here; then each feature's, in registration order.
    app_hooks::BuildHome(home);
}

std::unique_ptr<router::Screen> Make()
{
    if (const app_hooks::HomeScreen *claimed = app_hooks::ClaimedHome()) {
        return router::MakeScreen(Create, Enter, Exit, Covered, const_cast<app_hooks::HomeScreen *>(claimed));
    }
    return router::MakeScreen(Build);
}

}  // namespace screen_home
