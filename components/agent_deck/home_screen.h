// The deck's home: the startup screen, then the carousel — the overview and a page per agent — which owns the whole glass, with no status bar
// (APP_ARCHITECTURE §7; STICKY_AGENT_DECK §4). agent_deck.cpp claims home with these four calls (app_hooks::ClaimHome).

#pragma once

#include "lvgl.h"

namespace agent_deck::home {

void Create(lv_obj_t *root, lv_group_t *group);
void Enter();
void Exit();
void Covered(bool covered);

// What the screens read of the carousel, for the host tests: the page on view (0 the overview), the pages there are, and the objects
// a test drives. All nullptr or 0 until the startup screen has handed over.
struct Probe {
    bool carousel = false;
    int page = 0;
    int pages = 0;
    lv_obj_t *page_object = nullptr;
    lv_obj_t *stop = nullptr;      // the page's one stop in the focus group: a plain page's, or the control's box
    lv_obj_t *chip = nullptr;      // the state chip; the overview's count, or an agent's state
    lv_obj_t *review = nullptr;    // Review, only on a Needs you page
    lv_obj_t *wifi = nullptr;
    lv_obj_t *sound = nullptr;
    lv_obj_t *settings = nullptr;
    lv_obj_t *power = nullptr;
    lv_obj_t *link = nullptr;
    lv_obj_t *dots = nullptr;
    lv_obj_t *clock = nullptr;
    lv_obj_t *mascot = nullptr;
};
Probe Look();
// Ends the startup screen now, as its hold does: the carousel is built in its place. A test's and the bench's way past the 2 s.
void HandOver();

}  // namespace agent_deck::home
