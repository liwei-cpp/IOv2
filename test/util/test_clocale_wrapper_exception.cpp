#include <clocale>

namespace
{
// Mocking system calls for testing exception paths
static bool mock_newlocale_fail = false;
static bool mock_duplocale_fail = false;

inline locale_t mock_newlocale(int category_mask, const char* locale, locale_t base)
{
    if (mock_newlocale_fail) return (locale_t)0;
    return newlocale(category_mask, locale, base);
}

inline locale_t mock_duplocale(locale_t loc)
{
    if (mock_duplocale_fail) return (locale_t)0;
    return duplocale(loc);
}
}

// Macro Interception:
// 1. Redirect system calls to our mocks
// 2. Rename the class to avoid ODR conflicts with the original version in the same binary
#define newlocale mock_newlocale
#define duplocale mock_duplocale
#define clocale_wrapper clocale_wrapper_mock
#define clocale_user clocale_user_mock

#include <common/clocale_wrapper.h>
#include <common/verify.h>
#include <common/dump_info.h>

#undef newlocale
#undef duplocale
// We keep the class renames active for the rest of this file so we can use the names naturally

using namespace IOv2;

void test_clocale_wrapper_exception_paths()
{
    dump_info("Test clocale_wrapper exception paths (Coverage for lines 49, 63, 92)...");

    // Line 49: newlocale failure
    mock_newlocale_fail = true;
    try {
        clocale_wrapper_mock loc("C");
        VERIFY(false);
    } catch (const cvt_error& e) {
        // Expected
    }
    mock_newlocale_fail = false;

    // Line 63: duplocale failure in copy constructor
    {
        clocale_wrapper_mock loc1("C");
        mock_duplocale_fail = true;
        try {
            clocale_wrapper_mock loc2(loc1);
            VERIFY(false);
        } catch (const cvt_error& e) {
            // Expected
        }
        mock_duplocale_fail = false;
    }

    // Line 92: duplocale failure in copy assignment
    {
        clocale_wrapper_mock loc1("C");
        clocale_wrapper_mock loc2("C");
        mock_duplocale_fail = true;
        try {
            loc2 = loc1;
            VERIFY(false);
        } catch (const cvt_error& e) {
            // Expected
        }
        mock_duplocale_fail = false;
    }

    dump_info("Done\n");
}
