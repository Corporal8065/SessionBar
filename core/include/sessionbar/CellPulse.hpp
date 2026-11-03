#pragma once
#include <algorithm>
#include <cmath>
namespace sessionbar {
// CELL PULSE (v0.4.16, items 1+2) — the audio-driven motion model
// for SessionBar's city/time CELLS, ported from MeterBar's cell
// treatment (its ModuleViews beat pulse + the Visualizer/Glow
// envelope constants), replacing the literal positional "bumping"
// Jackson called "crazy to the eyes" (2026-11-02 19:38).
//
// MeterBar's mechanic, in its own code:
//  - band energies are NEVER rendered raw: they pass through a
//    fast-attack / slow-release smoother (GlowField: attack rate
//    10/s, release 1.6/s; the particle engine's activity gate uses
//    the same shape), so a linked packet arriving at link-tick
//    rate cannot step the picture per tick — that raw sampling
//    was the linked-only SessionBar cell flicker;
//  - an onset (overall level jumping by more than 0.07) slams a
//    pump envelope to full and it relaxes exponentially
//    (MeterBar's beat glow: exp decay, ~0.10-0.12 s time constant);
//  - the RENDER spends the envelope on glow and scale only — a
//    whisper of accent fill (alpha 0.055 x glow), a brighter
//    border (0.5 x glow) and a slightly larger state dot — never
//    on position. Cells do not move, bounce or jump, ever.
//
// This class is the pure envelope half; the app layer (SessionCell
// paint) spends glow() exactly the MeterBar way. Feeding it
// silence decays every envelope to zero, at which point the cells
// render byte-for-byte the pre-v0.4.16 static picture.
class CellPulse {
public:
    // Advance the envelopes. bands = [low, mid, high] energies
    // 0..1 (the BarLink audio packet's values, or silence), level
    // = overall loudness 0..1 (onset detection).
    void update(float dt, const float bands[3], float level) {
        if (dt <= 0) return;
        // Band smoothing: fast attack, slow release — the
        // GlowField constants, so a hit surges and melts instead
        // of flickering per frame.
        for (int i = 0; i < 3; ++i) {
            const float target = bands[i] < 0.f ? 0.f : bands[i] > 1.f ? 1.f : bands[i];
            const float rate = target > smooth_[i] ? 10.f : 1.6f;
            float a = rate * dt; if (a > 1.f) a = 1.f;
            smooth_[i] += (target - smooth_[i]) * a;
        }
        // Onset pump: a level jump slams the envelope; it then
        // decays with a ~0.12 s time constant (MeterBar beatEnv
        // uses exp(-dt / 0.10)).
        const float lv = level < 0.f ? 0.f : level > 1.f ? 1.f : level;
        if (lv > prevLevel_ + 0.07f) pump_ = 1.f;
        prevLevel_ = lv;
        pump_ *= std::exp(-dt / 0.12f);
        if (pump_ < 0.f) pump_ = 0.f;
    }

    // The cell glow 0..1: smoothed band energy carrying the onset
    // pump (GlowField's pumpGain shape: the pump AMPLIFIES the
    // smoothed bands, it is not a separate light source — silence
    // with a stale pump still reads as no glow once the bands die).
    float glow() const {
        const float m = std::max({ smooth_[0], smooth_[1], smooth_[2] });
        const float g = m * (0.65f + 0.35f * pump_) + 0.20f * pump_ * m;
        return g < 0.f ? 0.f : g > 1.f ? 1.f : g;
    }
    // Scale of the one thing that scales: the state dot's radius.
    // The cell's own rect never changes.
    float dotScale() const { return 1.f + 0.35f * glow(); }
    float band(int i) const { return smooth_[i < 0 ? 0 : i > 2 ? 2 : i]; }
    float pump() const { return pump_; }

private:
    float smooth_[3] = { 0.f, 0.f, 0.f };
    float pump_ = 0.f;
    float prevLevel_ = 0.f;
};
} // namespace sessionbar
