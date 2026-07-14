/*
 * SmartType UI addon tests
 *
 * Three tests required by the architecture audit:
 *   1. animation_math       — easeOutCubic interpolation at t=0, 0.5, 1.0
 *   2. animation_interrupt  — interrupting animation uses currentX, not startX
 *   3. addon_smoke          — smarttypeui.conf has correct Category/Library/deps,
 *                             smarttypeui.so exists and has non-zero size
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// ─── helpers ─────────────────────────────────────────────────────────────────

static void fail(const char* msg) {
    std::cerr << "FAIL: " << msg << "\n";
    std::exit(1);
}
static void ok(const char* msg) {
    std::cout << "  PASS: " << msg << "\n";
}
static bool near_eq(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) < eps;
}

// ─── 1. Animation math ───────────────────────────────────────────────────────

// Mirrors InputWindow::easeOutCubic (inputwindow.h:134)
static double easeOutCubic(double t) {
    return 1.0 - std::pow(1.0 - t, 3);
}

struct AnimState {
    bool   animating  = false;
    double startX = 0, startY = 0, startW = 0, startH = 0;
    double currentX = -9999, currentY = -9999, currentW = -9999, currentH = -9999;
    double targetX = -9999, targetY = -9999, targetW = -9999, targetH = -9999;

    // Mirrors InputWindow::startAnimation (inputwindow.cpp:1181)
    void startAnimation(double tx, double ty, double tw, double th) {
        if (!animating) {
            startX = (currentX != -9999.0) ? currentX : tx;
            startY = (currentY != -9999.0) ? currentY : ty;
            startW = (currentW != -9999.0) ? currentW : tw;
            startH = (currentH != -9999.0) ? currentH : th;
        } else {
            // Interrupt: new start = current visual position
            startX = currentX;
            startY = currentY;
            startW = currentW;
            startH = currentH;
        }
        targetX = tx; targetY = ty; targetW = tw; targetH = th;
        animating = true;
    }

    // Mirrors paint() tick (inputwindow.cpp:658-679)
    void tick(double t) {
        if (!animating) return;
        double progress = easeOutCubic(t);
        currentX = startX + (targetX - startX) * progress;
        currentY = startY + (targetY - startY) * progress;
        currentW = startW + (targetW - startW) * progress;
        currentH = startH + (targetH - startH) * progress;
        if (t >= 1.0) animating = false;
    }
};

static void test_animation_math() {
    std::cout << "\n[1/3] animation_math\n";

    // easeOutCubic boundary values
    if (!near_eq(easeOutCubic(0.0), 0.0))
        fail("easeOutCubic(0) must be 0.0");
    ok("easeOutCubic(0) == 0.0");

    if (!near_eq(easeOutCubic(1.0), 1.0))
        fail("easeOutCubic(1) must be 1.0");
    ok("easeOutCubic(1) == 1.0");

    // At t=0.5: 1 - (0.5)^3 = 0.875
    if (!near_eq(easeOutCubic(0.5), 0.875))
        fail("easeOutCubic(0.5) must be 0.875");
    ok("easeOutCubic(0.5) == 0.875");

    // Monotone: each step must be >= previous
    double prev = 0.0;
    for (int i = 1; i <= 100; ++i) {
        double cur = easeOutCubic(i / 100.0);
        if (cur < prev - 1e-9)
            fail("easeOutCubic is not monotone increasing");
        prev = cur;
    }
    ok("easeOutCubic is monotone increasing over [0,1]");

    // t=0 → current == start
    AnimState s;
    s.currentX = 10.0; s.currentY = 20.0;
    s.currentW = 60.0; s.currentH = 30.0;
    s.startAnimation(100.0, 200.0, 80.0, 40.0);
    s.tick(0.0);
    if (!near_eq(s.currentX, s.startX) || !near_eq(s.currentY, s.startY))
        fail("at t=0 current must equal start");
    ok("t=0: current == start");

    // t=1 → current == target
    s.tick(1.0);
    if (!near_eq(s.currentX, 100.0) || !near_eq(s.currentY, 200.0) ||
        !near_eq(s.currentW, 80.0)  || !near_eq(s.currentH, 40.0))
        fail("at t=1 current must equal target");
    ok("t=1: current == target");

    // t=0.5 → strictly between start and target
    AnimState s2;
    s2.currentX = 0.0; s2.currentY = 0.0;
    s2.currentW = 40.0; s2.currentH = 20.0;
    s2.startAnimation(100.0, 100.0, 80.0, 40.0);
    s2.tick(0.5);
    double expectedX = 0.0 + (100.0 - 0.0) * easeOutCubic(0.5); // 87.5
    if (!near_eq(s2.currentX, expectedX, 1e-4))
        fail("at t=0.5 currentX must be easeOutCubic-interpolated");
    if (s2.currentX <= 0.0 || s2.currentX >= 100.0)
        fail("at t=0.5 currentX must be strictly between start and target");
    ok("t=0.5: currentX is correctly easeOutCubic-interpolated and between start and target");

    std::cout << "  >>> animation_math PASSED\n";
}

// ─── 2. Animation interrupt ──────────────────────────────────────────────────

static void test_animation_interrupt() {
    std::cout << "\n[2/3] animation_interrupt\n";

    AnimState s;
    s.currentX = 0.0; s.currentY = 0.0;
    s.currentW = 80.0; s.currentH = 40.0;

    // Arrow → candidate 1: target X=100
    s.startAnimation(100.0, 0.0, 80.0, 40.0);
    // Advance halfway
    s.tick(0.5);
    double midX = s.currentX;
    if (midX <= 0.0 || midX >= 100.0)
        fail("midX must be strictly between 0 and 100");
    ok("midpoint X is strictly between 0 and 100");
    if (!s.animating)
        fail("animation must still be running at t=0.5");
    ok("animation is still running at t=0.5");

    // Arrow → candidate 2 while still animating: interrupt
    double snapshotX = s.currentX;
    s.startAnimation(200.0, 0.0, 80.0, 40.0);

    // Key check: startX must be the visual position at interrupt, not 0
    if (!near_eq(s.startX, snapshotX, 1e-4))
        fail("after interrupt: startX must == currentX at interrupt time (no jump to original 0)");
    ok("interrupt: startX == visual position at interrupt (no position jump)");

    if (!near_eq(s.targetX, 200.0))
        fail("after interrupt: targetX must be the new target");
    ok("interrupt: targetX updated to new target");

    if (!s.animating)
        fail("animation must restart after interrupt");
    ok("animation is running after interrupt");

    // Advance new animation to t=1
    s.tick(1.0);
    if (!near_eq(s.currentX, 200.0, 1e-4))
        fail("after interrupt + t=1: currentX must reach new targetX=200");
    ok("interrupt + t=1: currentX reaches new targetX=200");

    std::cout << "  >>> animation_interrupt PASSED\n";
}

// ─── 3. Addon smoke test ─────────────────────────────────────────────────────

static void test_addon_smoke(const std::filesystem::path& conf_path,
                             const std::filesystem::path& so_path,
                             bool expect_x11) {
    std::cout << "\n[3/3] addon_smoke\n";

    if (!std::filesystem::exists(conf_path))
        fail(("current-build smarttypeui.conf not found at: " + conf_path.string()).c_str());
    ok("current-build smarttypeui.conf exists");

    std::ifstream conf(conf_path);
    if (!conf.is_open()) fail("cannot open smarttypeui.conf");

    bool found_category_ui   = false;
    bool found_library       = false;
    bool found_dep_core      = false;
    bool found_dep_wayland   = false;
    bool found_dep_waylandim = false;
    std::string line;
    while (std::getline(conf, line)) {
        if (line == "Category=UI") found_category_ui = true;
        if (line == "Library=smarttypeui") found_library = true;
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string value = line.substr(separator + 1);
        if (value.starts_with("core:")) found_dep_core = true;
        if (value == "wayland") found_dep_wayland = true;
        if (value == "waylandim") found_dep_waylandim = true;
    }

    if (!found_category_ui)
        fail("Category=UI not found — Fcitx won't treat this as a UI addon");
    ok("Category=UI present");
    if (!found_library)
        fail("Library=smarttypeui not found");
    ok("Library=smarttypeui present");
    if (!found_dep_core)
        fail("core dependency not declared");
    ok("core dependency declared");
    if (!found_dep_wayland)
        fail("wayland optional dependency not declared");
    ok("wayland optional dependency declared");
    if (!found_dep_waylandim)
        fail("waylandim optional dependency not declared");
    ok("waylandim optional dependency declared");

    if (!std::filesystem::exists(so_path))
        fail(("current-build smarttypeui.so not found at: " + so_path.string()).c_str());
    ok("current-build smarttypeui.so exists");

    auto so_size = std::filesystem::file_size(so_path);
    if (so_size < 100'000)
        std::cerr << "  WARN: smarttypeui.so is only " << so_size
                  << " bytes — may be a stub\n";
    else
        ok(("smarttypeui.so is " + std::to_string(so_size) + " bytes").c_str());

    void* module = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        fail(("cannot load current-build smarttypeui.so: " + std::string(dlerror())).c_str());
    }
    dlerror();
    void* factory = dlsym(module, "fcitx_addon_factory_instance_smarttypeui");
    const char* symbol_error = dlerror();
    if (symbol_error || !factory) {
        dlclose(module);
        fail("fcitx_addon_factory_instance_smarttypeui symbol missing — Fcitx can't load the addon");
    }
    ok("fcitx_addon_factory_instance_smarttypeui exported symbol present");

    if (expect_x11) {
        dlerror();
        void* x11_update = dlsym(
            module,
            "_ZN5fcitx9classicui14XCBInputWindow6updateEPNS_12InputContextE");
        const char* x11_symbol_error = dlerror();
        if (x11_symbol_error || !x11_update) {
            dlclose(module);
            fail("X11 renderer was requested, but XCBInputWindow is missing from smarttypeui.so");
        }
        ok("native X11 XCBInputWindow renderer is compiled into smarttypeui.so");
    }
    dlclose(module);

    std::cout << "  >>> addon_smoke PASSED\n";
}

// ─── main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    const std::string selected = argc > 1 ? argv[1] : "all";
    std::cout << "=== SmartType UI addon tests ===\n";

    if (selected == "math") {
        test_animation_math();
    } else if (selected == "interrupt") {
        test_animation_interrupt();
    } else if (selected == "smoke") {
        if (argc != 5) {
            fail("smoke requires <smarttypeui.conf> <smarttypeui.so> <expect-x11>");
        }
        test_addon_smoke(argv[2], argv[3], std::string(argv[4]) == "ON");
    } else if (selected == "all") {
        if (argc != 5) {
            fail("all requires <smarttypeui.conf> <smarttypeui.so> <expect-x11>");
        }
        test_animation_math();
        test_animation_interrupt();
        test_addon_smoke(argv[2], argv[3], std::string(argv[4]) == "ON");
    } else {
        fail(("unknown test selector: " + selected).c_str());
    }

    std::cout << "\n=== Selected UI addon tests PASSED ===\n";
    return 0;
}
