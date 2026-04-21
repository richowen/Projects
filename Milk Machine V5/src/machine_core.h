// machine_core.h — pure state machine, zero Arduino/ESP deps.
// Safe to compile on native host for unit testing.
#ifndef MACHINE_CORE_H
#define MACHINE_CORE_H

#include <stdint.h>
#include <stddef.h>

enum class SysState : uint8_t { IDLE, MIXING, POST_MIX, PERIODIC_MIX, FAULT };

struct MachineInputs {
    bool          levelLow;   // debounced level switch: true = milk needed
    uint32_t      nowMs;      // monotonic ms (wrap-safe deltas used internally)
    size_t        freeHeap;   // ESP.getFreeHeap() on device; any large value in tests
};

struct MachineOutputs {
    bool auger;
    bool agitator;
    bool mixer;
    bool water;
    bool rebootRequested;     // set true when fault-count exceeds threshold
};

struct MachineConfig {
    uint32_t postMixMs       = 5UL * 1000UL;
    uint32_t periodicMixMs   = 5UL * 1000UL;
    uint32_t periodicIntMs   = 5UL * 60UL * 1000UL;
    uint32_t maxMixMs        = 2UL * 60UL * 1000UL;
    uint32_t faultCooldownMs = 60UL * 1000UL;
    uint32_t faultWindowMs   = 5UL * 60UL * 1000UL;
    uint32_t safetyMarginMs  = 10UL * 1000UL;    // extra margin for sanity guard
    uint8_t  maxFaults       = 3;
    size_t   minSafeHeap     = 8192;
};

// Transition/event log entries for test assertions and remote logging.
enum class MachineEvent : uint8_t {
    NONE,
    IDLE_TO_MIXING,
    IDLE_TO_PERIODIC,
    MIXING_TO_POSTMIX,
    MIXING_CAP_FAULT,
    POSTMIX_TO_IDLE,
    PERIODIC_TO_IDLE,
    PERIODIC_TO_MIXING,
    FAULT_ENTER_LOW_HEAP,
    FAULT_ENTER_SANITY,
    FAULT_TO_IDLE,
    FAULT_REBOOT,
};

class Machine {
public:
    Machine();

    // Reset state. Call once at startup with the current ms timestamp.
    void reset(uint32_t nowMs);

    // Advance the state machine by one step with the supplied inputs.
    // Outputs are fully written every tick (caller decides whether to
    // push them to hardware — the hardware layer caches writes).
    void tick(const MachineInputs& in, MachineOutputs& out);

    // Accessors used by logging/tests.
    SysState       state()       const { return state_; }
    uint8_t        faultCount()  const { return faultCount_; }
    MachineEvent   lastEvent()   const { return lastEvent_; }
    const char*    stateStr()    const;
    const char*    eventStr()    const;

    MachineConfig cfg;

private:
    SysState     state_;
    uint32_t     stateEnteredMs_;
    uint32_t     lastPeriodicMs_;
    uint32_t     faultEnteredMs_;
    uint32_t     lastFaultMs_;
    uint8_t      faultCount_;
    MachineEvent lastEvent_;
    bool         initialized_;

    // Desired relay states tracked across ticks for output caching decisions.
    bool outAuger_, outAgitator_, outMixer_, outWater_;

    // Wrap-safe elapsed.  Both values are 32-bit; subtraction in unsigned
    // arithmetic naturally handles wraparound across the 49.7-day boundary.
    static inline uint32_t elapsed(uint32_t now, uint32_t ref) {
        return (uint32_t)(now - ref);
    }

    void transitionTo_(SysState s, uint32_t now);
    void enterFault_(uint32_t now, MachineEvent why);
    void writeRelays_(bool a, bool ag, bool m, bool w);
    void fillOutputs_(MachineOutputs& out) const;
};

#endif
