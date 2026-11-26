// Host tests of the select sheet's arithmetic (components/widgets/src/internal/sheet_rules.h), on the computer with no board: the
// height for three and four rows, and which a field gets — four when the sheet leaves the field clear, else three
// (STICKY_DESIGN_TOKENS §12).
// Build and run: bash tests/host/run.sh

#include <cstdio>

#include "design_tokens.h"
#include "internal/sheet_rules.h"

using namespace widgets::detail::sheet;
namespace dt = design_tokens;

namespace {

int s_failures = 0;
int s_checks = 0;

void Check(bool ok, const char *what)
{
    ++s_checks;
    if (!ok) {
        ++s_failures;
        std::printf("FAIL  %s\n", what);
    }
}

constexpr int32_t kGlass = 800;

void TestHeights()
{
    Check(Band(4) == 304 && Band(3) == 224 && Band(2) == 144, "four rows and three gaps are 304 px, three rows 224, two 144");
    Check(Height(4) == 484 && Height(3) == 404 && Height(2) == 324, "the sheet is 484 px for four rows, 404 for three, 324 for two");
    Check(Height(4) == 80 * 4 + 164 && Height(3) == 80 * 3 + 164, "a sheet is 80 px for each row and 164 for the rest");
    Check(kGlass - Height(4) == 316 && kGlass - Height(3) == 396, "the sheet's top is at y 316 for four rows and 396 for three");
    Check(Clearance() == 22, "the ring, white and the offset above the sheet are 22 px");
}

void TestRowsPerPage()
{
    // The draft's page: Remind me ends at y 182, Repeat at 262, the disabled Alert at 342.
    Check(RowsPerPage(182, kGlass) == 4 && RowsPerPage(262, kGlass) == 4, "the first two fields leave room for four rows");
    Check(RowsPerPage(294, kGlass) == 4, "a field ending at y 294 is the last to take four");
    Check(RowsPerPage(295, kGlass) == 3, "a field ending at y 295 takes three");
    Check(RowsPerPage(342, kGlass) == 3 && RowsPerPage(374, kGlass) == 3, "a field between 295 and 374 takes three");
    Check(RowsPerPage(375, kGlass) == 3 && RowsPerPage(700, kGlass) == 3, "a field below 374 is covered even by three: never fewer");
    Check(Clearance() + 294 <= kGlass - Height(4) && Clearance() + 295 > kGlass - Height(4), "294 is the edge of the clearance's arithmetic");
    Check(Clearance() + 374 <= kGlass - Height(3) && Clearance() + 375 > kGlass - Height(3), "374 is the edge of three rows' clearance");
}

static_assert(Height(4) == 484 && RowsPerPage(294, 800) == 4 && RowsPerPage(295, 800) == 3, "the rules are constexpr");

}  // namespace

int main()
{
    TestHeights();
    TestRowsPerPage();
    std::printf("sheet_rules: %d of %d checks pass\n", s_checks - s_failures, s_checks);
    return s_failures == 0 ? 0 : 1;
}
