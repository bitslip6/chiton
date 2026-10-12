#include "illumination_fixture.h"
#include <limits>
#include <stdexcept>
using namespace fixture;

static void convergence() {
    Run high({{12,12,12,12}});
    high.constant_safe(10000);
    assert(high.controller.state().failed_probes <= 4);
    assert(high.controller.state().reversals <= 12);
    Run low({{2,2,2,2}});
    low.constant_safe();
    assert((low.controller.state().applied == Tuple{{6,6,6,6}}));
    assert(low.controller.state().failed_probes == 4);
    auto failures = low.controller.state().failed_probes;
    low.run(10000);
    assert(low.controller.state().failed_probes == failures);
    Run different({{0,0,0,0}}, {{2,5,8,12}});
    different.constant_safe();
    assert((different.controller.state().applied == Tuple{{2,5,8,12}}));

    int p = 12, previous_direction = 0, reversals = 0;
    for (int i = 0; i < 1000; ++i) {
        int next = legacy(p, p > 6 ? 400 : 2000);
        int d = (next > p) - (next < p);
        if (d && previous_direction && d != previous_direction) ++reversals;
        if (d) previous_direction = d;
        p = next;
    }
    assert(reversals > 900); // Fixture demonstration, not a hardware diagnosis.
}
static void scenes() {
    Run r({{0,0,0,0}}, {{8,8,8,8}});
    r.constant_safe();
    auto failures = r.controller.state().failed_probes;
    r.port.sensor.boundary.fill(3);
    auto transitions = r.controller.state().transitions;
    r.run(8);
    assert(r.controller.state().transitions > transitions);
    r.constant_safe();
    auto t = r.controller.state().applied;
    r.port.sensor.boundary.fill(9);
    r.run(2000);
    assert(r.controller.state().applied == t);
    assert(r.controller.state().failed_probes >= failures);
    r.controller.reset_learning(); // Approved independent synthetic scene event.
    r.constant_safe();
    assert((r.controller.state().applied == Tuple{{9,9,9,9}}));

    Run unresolved({{12,12,12,12}}, {{-1,-1,-1,-1}});
    unresolved.run(200);
    assert((unresolved.controller.state().applied == Tuple{{0,0,0,0}}));
    assert(unresolved.controller.state().reason == Reason::SaturatedAtMinimum);
    for (bool known : unresolved.controller.state().safe_known) assert(!known);
    auto changes = unresolved.controller.state().transitions;
    unresolved.run(1000);
    assert(unresolved.controller.state().transitions == changes);
}
static void eligibility() {
    Run r({{2,2,2,2}});
    r.run(5); // First probe pending, cannot accumulate speculative increases.
    assert(r.controller.state().pending);
    auto desired = r.controller.state().desired;
    r.run(2); // Write at frame 7; effect at 9; eligible at 11.
    assert(r.controller.state().command == desired.generation);
    r.run(3);
    for (auto n : r.controller.state().clear) assert(n == 0);
    r.tick();
    assert(r.controller.state().clear[0] == 1);
    auto o = r.port.sensor.observe(r.frame, r.lifecycle);
    auto clear = r.controller.state().clear;
    r.controller.observe(o); // Duplicate
    assert(r.controller.state().clear == clear);
    --o.frame; r.controller.observe(o); // Out of order
    assert(r.controller.state().clear == clear);
    o.frame = r.frame + 1; o.lifecycle = 0; r.controller.observe(o);
    assert(r.controller.state().clear == clear);
    o.lifecycle = 1; o.time = 0; r.controller.observe(o);
    assert(r.controller.state().clear == clear); // Regressing time.
    o.time = o.frame; o.command = 1; r.controller.observe(o);
    for (auto n : r.controller.state().clear) assert(n == 0);
    assert(!r.controller.acknowledge(1, true, 0)); // Old ack

    for (int mode : {0,1,3,4,5,6}) {
        (void)mode; // Pure observations have no display-mode/history dependency.
        Run missing({{2,2,2,2}});
        missing.port.sensor.missing = true;
        missing.run(100);
        assert(missing.controller.state().transitions == 0);
        missing.port.sensor.missing = false;
        missing.port.sensor.supported = false; // IR without qualified detector.
        missing.run(100);
        assert(missing.controller.state().reason == Reason::Unsupported);
        assert(missing.controller.state().transitions == 0);
    }
}
static void unknown_samples() {
    Controller c(parameters());
    assert(!c.state().applied_known && !c.state().pending);
    assert(c.recover({{2,2,2,2}}, 1, 1, 0));
    Observation o; o.lifecycle = o.command = 1; o.supported = o.eligible = true;
    o.sectors.fill({true,0,100});
    for (unsigned i = 1; i <= 30; ++i) {
        o.frame = o.time = i;
        o.sectors[0] = i % 2 ? Samples{true,0,0} : Samples{true,101,100};
        c.observe(o);
        assert(!c.state().pending && c.state().reason == Reason::Unknown);
        assert(!c.state().safe_known[0]);
    }
    // Qualified fresh saturation permits a reduction, even with a missing sector.
    o.frame = o.time = 31; o.sectors[1] = {true,25,100}; c.observe(o);
    assert(c.state().pending && c.state().desired.tuple[1] == 0);
    assert(c.state().desired.tuple[0] == 2);
}
static void noise_and_coupling() {
    Controller c(parameters());
    assert(c.recover({{6,6,6,6}}, 1, 1, 1));
    Observation o;
    o.lifecycle = 1; o.command = 1; o.supported = o.eligible = true;
    // Replay around low threshold, including the band: never five clear frames.
    for (unsigned i = 1; i < 1000; ++i) {
        o.frame = o.time = i;
        unsigned count = i % 2 ? 2 : 3;
        o.sectors.fill({true, count, 100}); c.observe(o);
        assert(!c.state().pending);
    }
    // Alternating high/low bursts: reductions never authorize immediate ascent.
    for (unsigned i = 1000; i < 1100; ++i) {
        o.frame = o.time = i;
        o.sectors.fill({true, i % 2 ? 9u : 10u, 100});
        c.observe(o);
        if (c.state().pending) {
            auto cmd = c.state().desired;
            assert(cmd.tuple[0] <= c.state().applied[0]);
            c.acknowledge(cmd.generation, true, i + 1); o.command = cmd.generation;
        }
    }
    assert(c.state().reversals == 0);
    Run coupling({{2,2,2,2}}, {{12,12,12,12}});
    coupling.port.sensor.crosstalk = true;
    coupling.run(200);
    assert(coupling.controller.state().upward_frozen);
    assert(coupling.controller.state().failed_probes == 1);
    auto t = coupling.controller.state().applied;
    coupling.run(1000);
    assert(coupling.controller.state().applied == t);
    assert(coupling.controller.state().reason == Reason::CouplingUnknown);
}
static void bounds() {
    Controller c(parameters());
    assert(!c.recover({{-1,0,0,0}}, 1, 1, 1));
    assert(!c.recover({{13,0,0,0}}, 1, 1, 1));
    assert(c.recover({{0,0,0,0}}, 1, 1, 1));
    auto p = parameters(); p.decrease_step = 65535;
    Controller big(p);
    assert(big.recover({{1,1,1,1}}, 1, 1, 0));
    Observation o; o.frame = o.time = 1; o.lifecycle = o.command = 1;
    o.supported = o.eligible = true; o.sectors.fill({true, 25,100});
    big.observe(o);
    assert((big.state().desired.tuple == Tuple{{0,0,0,0}}));
    p.independent_sectors = false;
    Controller coupled(p);
    assert(coupled.recover({{0,0,0,0}}, 1, 1, 0));
    o.sectors.fill({true,0,100});
    for (int i = 1; i <= 10; ++i) { o.frame = o.time = i; coupled.observe(o); }
    assert(!coupled.state().pending && coupled.state().reason == Reason::CouplingUnknown);
    assert(!coupled.recover({{0,0,0,0}}, 0, 100, 0));
    p.maximum = 65536;
    bool rejected = false;
    try { Controller invalid(p); } catch (const std::invalid_argument &) { rejected = true; }
    assert(rejected);
    Controller exhausted(parameters());
    assert(exhausted.recover({{0,0,0,0}}, 1, std::numeric_limits<uint64_t>::max(), 0));
    o.command = std::numeric_limits<uint64_t>::max(); o.sectors.fill({true,0,100});
    for (int i = 1; i <= 5; ++i) { o.frame = o.time = i; exhausted.observe(o); }
    assert(!exhausted.state().applied_known && !exhausted.state().pending);
}
static_assert(sizeof(Controller) < 1024, "Controller storage must stay bounded");
int main() { convergence(); scenes(); eligibility(); unknown_samples(); noise_and_coupling(); bounds(); }
