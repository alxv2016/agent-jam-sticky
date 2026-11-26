#include "widgets/swipe_page.h"

#include "internal/base.h"

namespace widgets {

namespace {

struct State {
    SwipePageTurn turn;
    void *user;
};

State *Of(lv_obj_t *page)
{
    return static_cast<State *>(lv_obj_get_user_data(page));
}

void OnSwipe(lv_obj_t *page, int32_t dir)
{
    const State *s = Of(page);
    if (s->turn != nullptr) {
        s->turn(dir, s->user);
    }
}

void OnDelete(lv_event_t *e)
{
    delete Of(lv_event_get_target_obj(e));
}

// A page that claims the screen's swipe never scrolls by itself: a scroll no finger holds — LVGL's scroll-on-focus bringing a
// control on a hidden page into view — goes back to the top at once. A finger's drag is the swipe's, which puts it back itself.
void OnStrayScroll(lv_event_t *e)
{
    lv_obj_t *page = lv_event_get_target_obj(e);
    lv_indev_t *indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_scroll_obj(indev) == page) {
        return;
    }
    if (lv_obj_get_scroll_y(page) != 0) {
        lv_obj_scroll_to_y(page, 0, LV_ANIM_OFF);
    }
}

// Room to scroll: the swipe claims a drag only on an object that has somewhere to go, and puts it back at once, so nothing moves.
void AddRoom(lv_obj_t *page, int32_t width, int32_t height)
{
    lv_obj_t *room = detail::Bare(page);
    lv_obj_set_ignore_layout(room, true);
    lv_obj_set_size(room, width, height);
    lv_obj_set_pos(room, 0, 0);
}

// What both kinds share: room to go along the axis, scrolling only along it with the other axis chained to the screen behind, no
// key's scroll and no scrollbar, the owner's turn kept, and the swipe claimed.
void Claim(lv_obj_t *page, SwipePageTurn turn, void *user, SwipeAxis axis)
{
    const bool horizontal = axis == SwipeAxis::kHorizontal;
    lv_obj_update_layout(page);
    const int32_t width = lv_obj_get_width(page);
    const int32_t height = lv_obj_get_height(page);
    AddRoom(page, horizontal ? 2 * width : 1, horizontal ? 1 : 2 * height);
    lv_obj_set_scrollable(page, true);
    if (horizontal) {
        lv_obj_set_scroll_chain_ver(page, true);
    } else {
        lv_obj_set_scroll_chain_hor(page, true);
    }
    lv_obj_set_scroll_dir(page, horizontal ? LV_DIR_HOR : LV_DIR_VER);
    lv_obj_set_scroll_with_arrow(page, false);  // no key ever scrolls the page: its turns are the swipe's and the keys'
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_user_data(page, new State{turn, user});
    lv_obj_add_event_cb(page, OnDelete, LV_EVENT_DELETE, nullptr);
    detail::ClaimSwipe(page, OnSwipe, horizontal ? detail::SwipeAxis::kHorizontal : detail::SwipeAxis::kVertical);
}
}  // namespace

lv_obj_t *MakeSwipePage(lv_obj_t *parent, int32_t width, int32_t height, SwipePageTurn turn, void *user, SwipeAxis axis)
{
    lv_obj_t *page = detail::Bare(parent);
    lv_obj_set_ignore_layout(page, true);
    lv_obj_set_size(page, width, height);
    lv_obj_set_pos(page, 0, 0);
    detail::LetRingOut(page);
    Claim(page, turn, user, axis);
    return page;
}

void ClaimPageSwipe(lv_obj_t *page, SwipePageTurn turn, void *user)
{
    Claim(page, turn, user, SwipeAxis::kVertical);
    lv_obj_add_event_cb(page, OnStrayScroll, LV_EVENT_SCROLL, nullptr);
}

}  // namespace widgets
