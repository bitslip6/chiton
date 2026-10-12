#include "illumination_fixture.h"
using namespace fixture;
int main() {
    Run r({{1,2,3,4}}, {{12,12,12,12}});
    assert(!r.adapter.opportunity(0, r.controller));
    assert(r.adapter.opportunity(1, r.controller));
    const auto call = r.port.log[0];
    assert((call.addresses == std::array<uint16_t,4>{{0x4011,0x4012,0x4013,0x4014}}));
    assert((call.values == std::array<uint16_t,4>{{4,3,2,1}}));
    auto eligible = r.controller.state().eligible_from;
    assert(r.adapter.opportunity(4, r.controller));
    assert(r.controller.state().eligible_from == eligible); // Repeated writes retained.
    RegisterAdapter unavailable(r.port, {{3,2,1,0}}, false);
    assert(!unavailable.opportunity(7, r.controller));
    auto calls = r.port.calls;
    r.controller.invalidate(1);
    assert(!r.adapter.opportunity(7, r.controller)); // No assumed startup/default tuple.
    assert(r.port.calls == calls);

    for (auto status : {aditof::Status::BUSY, aditof::Status::GENERIC_ERROR,
                        aditof::Status::UNAVAILABLE, aditof::Status::OK}) {
        Run failure({{2,2,2,2}});
        failure.run(5);
        assert(failure.controller.state().pending);
        auto acknowledged = failure.controller.state().applied;
        auto generation = failure.controller.state().command;
        failure.port.result = status;
        failure.port.certain = status != aditof::Status::OK;
        failure.port.partial = true;
        failure.run(2);
        assert(!failure.controller.state().applied_known && !failure.controller.state().pending);
        assert(failure.controller.state().applied == acknowledged);
        assert(failure.controller.state().command == generation);
        assert(failure.port.sensor.actual != acknowledged);
        auto writes = failure.port.calls;
        failure.run(100);
        assert(failure.port.calls == writes); // No blind retries/fallback.
        // Fictional documented readback recovery; no real register call.
        auto readback = failure.port.sensor.actual;
        assert(failure.controller.recover(readback, 1, 100, failure.frame + 4));
        assert(failure.controller.state().applied == readback);
    }
    Run refresh_failure({{4,4,4,4}});
    refresh_failure.port.result = aditof::Status::BUSY;
    assert(refresh_failure.adapter.opportunity(1, refresh_failure.controller));
    assert(!refresh_failure.controller.state().applied_known);
    assert(!refresh_failure.adapter.opportunity(4, refresh_failure.controller));

    // Successful changed writes advance acknowledged tuple, not physical state.
    Run delayed({{2,2,2,2}});
    delayed.run(7);
    assert(!delayed.controller.state().pending);
    assert(delayed.controller.state().applied != delayed.port.sensor.actual);
    assert(delayed.controller.state().command != delayed.port.sensor.command);
    delayed.run(2);
    assert(delayed.controller.state().applied == delayed.port.sensor.actual);
    for (auto n : delayed.controller.state().clear) assert(n == 0);
    delayed.run(2);
    assert(delayed.controller.state().clear[0] == 1);

    // Capture stop/start, initialization, exposure/mode/dynamic transitions and
    // external register writes all require owner-provided invalidation. Ceilings
    // survive: these events must not be an accidental unsafe-probe reset.
    Run lifecycle({{0,0,0,0}}, {{3,3,3,3}});
    lifecycle.constant_safe();
    auto ceiling = lifecycle.controller.state().ceiling;
    for (int event = 0; event < 6; ++event) {
        lifecycle.controller.invalidate(++lifecycle.lifecycle);
        assert(lifecycle.controller.state().ceiling == ceiling);
        lifecycle.port.external({{1,1,1,1}});
        assert(!lifecycle.adapter.opportunity(1, lifecycle.controller));
        auto generation = lifecycle.controller.state().command + 100;
        lifecycle.port.sensor.command = generation;
        assert(lifecycle.controller.recover(lifecycle.port.sensor.actual,
            lifecycle.lifecycle, generation, lifecycle.frame + 4));
        lifecycle.constant_safe();
        assert(lifecycle.controller.state().ceiling == ceiling);
    }
    // Fixed-size log wraps over a long unchanged run; cadence never suppressed.
    auto old_calls = lifecycle.port.calls, frame = lifecycle.frame;
    lifecycle.run(300);
    assert(lifecycle.port.calls - old_calls == 100);
    assert(lifecycle.frame == frame + 300);
    assert(lifecycle.port.cursor < lifecycle.port.log.size());
}
