#pragma once
#include "Sessions.hpp" // SessionState
namespace sessionbar {
// SESSION-STATE COLOUR SCHEMES (v0.3.0) — user-selectable colours for
// the session states themselves (the dot + state word in each cell),
// modelled on MeterBar's colour-map family: each map scheme's state
// colours are real stops of that map's LUT (see meters-app
// ColorMap.hpp). "Family" is the shipping v0.1-v0.2.1 language:
// Open = family green, Break/Pre = family amber, Closed = family
// red, Post = family teal. In every map scheme Pre shares Break's
// colour (both are "session-adjacent" states, as in Family) and the
// five states stay mutually distinguishable.
struct Rgb {
    int r = 0, g = 0, b = 0;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb& o) const { return !(*this == o); }
};
inline constexpr int kSessionColorSchemeCount = 5;
inline const char* sessionColorSchemeName(int id) {
    switch (id) {
        case 1: return "Plasma";
        case 2: return "Inferno";
        case 3: return "Viridis";
        case 4: return "Magma";
        default: return "Family";
    }
}
inline Rgb sessionStateColour(int scheme, SessionState s) {
    // LUT stops used below (meters-app ColorMap.hpp):
    //   Plasma:  .25 (126,3,168)  .5 (204,71,120)  .75 (248,149,64)  1.0 (240,249,33)
    //   Inferno: .25 (87,16,110)  .5 (188,55,84)   .75 (249,142,9)   1.0 (252,255,164)
    //   Viridis: .0 (68,1,84)     .5 (33,145,140)  .75 (94,201,98)   1.0 (253,231,37)
    //   Magma:   .25 (81,18,124)  .5 (183,54,121)  .75 (252,136,97)  1.0 (252,253,191)
    switch (scheme) {
        case 1: // Plasma
            switch (s) {
                case SessionState::Open:   return {240, 249, 33};
                case SessionState::Break:  return {248, 149, 64};
                case SessionState::Pre:    return {248, 149, 64};
                case SessionState::Post:   return {126, 3, 168};
                default:                   return {204, 71, 120}; // Closed
            }
        case 2: // Inferno
            switch (s) {
                case SessionState::Open:   return {252, 255, 164};
                case SessionState::Break:  return {249, 142, 9};
                case SessionState::Pre:    return {249, 142, 9};
                case SessionState::Post:   return {87, 16, 110};
                default:                   return {188, 55, 84}; // Closed
            }
        case 3: // Viridis
            switch (s) {
                case SessionState::Open:   return {94, 201, 98};
                case SessionState::Break:  return {253, 231, 37};
                case SessionState::Pre:    return {253, 231, 37};
                case SessionState::Post:   return {33, 145, 140};
                default:                   return {68, 1, 84}; // Closed
            }
        case 4: // Magma
            switch (s) {
                case SessionState::Open:   return {252, 253, 191};
                case SessionState::Break:  return {252, 136, 97};
                case SessionState::Pre:    return {252, 136, 97};
                case SessionState::Post:   return {81, 18, 124};
                default:                   return {183, 54, 121}; // Closed
            }
        default: // Family (the v0.1-v0.2.1 colours, verbatim)
            switch (s) {
                case SessionState::Open:   return {47, 214, 138};
                case SessionState::Break:  return {250, 217, 39};
                case SessionState::Pre:    return {250, 217, 39};
                case SessionState::Post:   return {64, 170, 192};
                default:                   return {254, 90, 60}; // Closed
            }
    }
}
} // namespace sessionbar
