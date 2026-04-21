#include "machine_core.h"

Machine::Machine()
  : state_(SysState::IDLE),
    stateEnteredMs_(0), lastPeriodicMs_(0),
    faultEnteredMs_(0), lastFaultMs_(0),
    faultCount_(0), lastEvent_(MachineEvent::NONE),
    initialized_(false),
    outAuger_(false), outAgitator_(false), outMixer_(false), outWater_(false)
{}

void Machine::reset(uint32_t nowMs) {
    state_          = SysState::IDLE;
    stateEnteredMs_ = nowMs;
    lastPeriodicMs_ = nowMs;
    faultEnteredMs_ = 0;
    lastFaultMs_    = 0;
    faultCount_     = 0;
    lastEvent_      = MachineEvent::NONE;
    initialized_    = true;
    writeRelays_(false, false, false, false);
}

void Machine::writeRelays_(bool a, bool ag, bool m, bool w) {
    outAuger_    = a;
    outAgitator_ = ag;
    outMixer_    = m;
    outWater_    = w;
}

void Machine::fillOutputs_(MachineOutputs& out) const {
    out.auger    = outAuger_;
    out.agitator = outAgitator_;
    out.mixer    = outMixer_;
    out.water    = outWater_;
}

void Machine::transitionTo_(SysState s, uint32_t now) {
    state_          = s;
    stateEnteredMs_ = now;
}

// SAFETY INVARIANT: every fault entry path runs writeRelays_(0,0,0,0).
void Machine::enterFault_(uint32_t now, MachineEvent why) {
    writeRelays_(false, false, false, false);
    transitionTo_(SysState::FAULT, now);
    faultEnteredMs_ = now;
    lastEvent_      = why;

    // Wrap-safe window check
    if (faultCount_ > 0 && elapsed(now, lastFaultMs_) <= cfg.faultWindowMs) {
        faultCount_++;
    } else {
        faultCount_ = 1;
    }
    lastFaultMs_ = now;
}

void Machine::tick(const MachineInputs& in, MachineOutputs& out) {
    if (!initialized_) reset(in.nowMs);

    // Clear per-tick flags
    out.rebootRequested = false;

    // Global heap guard: force fault unless already in fault.
    if (in.freeHeap < cfg.minSafeHeap && state_ != SysState::FAULT) {
        enterFault_(in.nowMs, MachineEvent::FAULT_ENTER_LOW_HEAP);
        if (faultCount_ >= cfg.maxFaults) {
            out.rebootRequested = true;
            lastEvent_ = MachineEvent::FAULT_REBOOT;
        }
        fillOutputs_(out);
        return;
    }

    switch (state_) {
        case SysState::IDLE:
            if (in.levelLow) {
                writeRelays_(true, true, true, true);
                transitionTo_(SysState::MIXING, in.nowMs);
                lastEvent_ = MachineEvent::IDLE_TO_MIXING;
                break;
            }
            if (elapsed(in.nowMs, lastPeriodicMs_) >= cfg.periodicIntMs) {
                writeRelays_(false, false, true, false);
                transitionTo_(SysState::PERIODIC_MIX, in.nowMs);
                lastPeriodicMs_ = in.nowMs;
                lastEvent_ = MachineEvent::IDLE_TO_PERIODIC;
                break;
            }
            break;

        case SysState::MIXING:
            // Hard safety cap
            if (elapsed(in.nowMs, stateEnteredMs_) >= cfg.maxMixMs) {
                enterFault_(in.nowMs, MachineEvent::MIXING_CAP_FAULT);
                if (faultCount_ >= cfg.maxFaults) {
                    out.rebootRequested = true;
                    lastEvent_ = MachineEvent::FAULT_REBOOT;
                }
                break;
            }
            if (!in.levelLow) {
                writeRelays_(false, false, true, false);
                transitionTo_(SysState::POST_MIX, in.nowMs);
                lastEvent_ = MachineEvent::MIXING_TO_POSTMIX;
                break;
            }
            break;

        case SysState::POST_MIX:
            if (elapsed(in.nowMs, stateEnteredMs_) >= cfg.postMixMs) {
                writeRelays_(false, false, false, false);
                lastPeriodicMs_ = in.nowMs;   // [Fix #11] no immediate stir
                transitionTo_(SysState::IDLE, in.nowMs);
                lastEvent_ = MachineEvent::POSTMIX_TO_IDLE;
            }
            break;

        case SysState::PERIODIC_MIX:
            if (elapsed(in.nowMs, stateEnteredMs_) >= cfg.periodicMixMs) {
                writeRelays_(false, false, false, false);
                transitionTo_(SysState::IDLE, in.nowMs);
                lastEvent_ = MachineEvent::PERIODIC_TO_IDLE;
                break;
            }
            if (in.levelLow) {
                writeRelays_(true, true, true, true);
                transitionTo_(SysState::MIXING, in.nowMs);
                lastEvent_ = MachineEvent::PERIODIC_TO_MIXING;
            }
            break;

        case SysState::FAULT:
            if (elapsed(in.nowMs, faultEnteredMs_) >= cfg.faultCooldownMs) {
                writeRelays_(false, false, false, false);
                lastPeriodicMs_ = in.nowMs;  // don't stir immediately after fault
                transitionTo_(SysState::IDLE, in.nowMs);
                lastEvent_ = MachineEvent::FAULT_TO_IDLE;
            }
            break;
    }

    // Defensive sanity guard outside the main switch — catches any
    // pathological path that left MIXING running past cap+margin.
    if (state_ == SysState::MIXING &&
        elapsed(in.nowMs, stateEnteredMs_) > (cfg.maxMixMs + cfg.safetyMarginMs)) {
        enterFault_(in.nowMs, MachineEvent::FAULT_ENTER_SANITY);
        if (faultCount_ >= cfg.maxFaults) {
            out.rebootRequested = true;
            lastEvent_ = MachineEvent::FAULT_REBOOT;
        }
    }

    fillOutputs_(out);
}

const char* Machine::stateStr() const {
    switch (state_) {
        case SysState::IDLE:         return "IDLE";
        case SysState::MIXING:       return "MIXING";
        case SysState::POST_MIX:     return "POST_MIX";
        case SysState::PERIODIC_MIX: return "PERIODIC_MIX";
        case SysState::FAULT:        return "FAULT";
    }
    return "UNKNOWN";
}

const char* Machine::eventStr() const {
    switch (lastEvent_) {
        case MachineEvent::NONE:                  return "NONE";
        case MachineEvent::IDLE_TO_MIXING:        return "IDLE->MIXING";
        case MachineEvent::IDLE_TO_PERIODIC:      return "IDLE->PERIODIC_MIX";
        case MachineEvent::MIXING_TO_POSTMIX:     return "MIXING->POST_MIX";
        case MachineEvent::MIXING_CAP_FAULT:      return "MIXING->FAULT(cap)";
        case MachineEvent::POSTMIX_TO_IDLE:       return "POST_MIX->IDLE";
        case MachineEvent::PERIODIC_TO_IDLE:      return "PERIODIC_MIX->IDLE";
        case MachineEvent::PERIODIC_TO_MIXING:    return "PERIODIC_MIX->MIXING";
        case MachineEvent::FAULT_ENTER_LOW_HEAP:  return "FAULT(low heap)";
        case MachineEvent::FAULT_ENTER_SANITY:    return "FAULT(sanity)";
        case MachineEvent::FAULT_TO_IDLE:         return "FAULT->IDLE";
        case MachineEvent::FAULT_REBOOT:          return "FAULT->REBOOT";
    }
    return "UNKNOWN";
}
