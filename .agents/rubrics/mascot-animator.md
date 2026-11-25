# Rubric — mascot-animator

How the [mascot-animator](../agents/mascot-animator.md) scores a change to Claude's mascot or its sounds in review mode.
The verdict, the evidence rules, the items R1–R11 and what each looks for travel with the skill, in
[`mascot-animation/references/rubric.md`](../skills/mascot-animation/references/rubric.md) (MASCOT_REVIEW_RUBRIC), so the agent
and the skill work in any repo. This file holds what this project adds: the items learned from what the developer saw on
the glass or heard from the speaker ([verify-change](../skills/verify-change/SKILL.md#recording-what-was-learned) →
Recording what was learned, route `rubric`). A review scores the skill's items and every item here. A new learned item
goes here, not in the skill or the agent.

## Learned

| Item | Since | From the developer | Looks for |
| --- | --- | --- | --- |
| L1 | 2026-11-10 | Carried from the agent deck, where the mascot was built: the mascot judge's review of the skip trip — a page rebuilt mid-trip dropped a moment the trip's table had due, and the wind-up after a trip told a sway its stumble had left armed | A move told from a timed table keeps its told-up-to point by the move's start, not by the mascot, so a page rebuild neither drops nor repeats a moment (for example `faceplant_moments()`, `trip_moments()`); the front view's moment state reset where the move hands back to it; its test has a run with no rebuild and rebuilds at several offsets, each held with exact counts, never only a floor |
