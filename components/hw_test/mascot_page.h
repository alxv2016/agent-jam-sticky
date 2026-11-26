// The hardware test's mascot page: every move of Claude's mascot on the glass, with its sounds, played once or looped
// (STICKY_DESIGN_TOKENS §13). Its route is `hw.mascot`.
#pragma once

#include <memory>

#include "router/screen.h"

namespace hw_test {

std::unique_ptr<router::Screen> MascotScreen();

}  // namespace hw_test
