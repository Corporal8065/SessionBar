#pragma once
#include <utility>
#include <vector>
namespace sessionbar {
// CELL LAYOUT MATH (v0.3.0, items 9/10) — shared shape with
// TickerBar's Layout.hpp helpers: dragged explicit widths, an
// equal-share solver, divider drags that preserve the pair sum, and
// the drop-slot math behind drag-to-reorder.

// ------------------------------------------------------------------
// CELL WIDTH SOLVER + REORDER MATH (v0.3.0, items 9/10). The bar's
// cells share the strip: cells with a user-dragged explicit width
// keep it (clamped to minW), the rest split what remains equally
// (the last auto cell absorbs rounding so the row fills exactly).
inline std::vector<int> solveCellWidths(int total, int gap, const std::vector<int>& explicitW, int minW) {
    const int n = (int)explicitW.size();
    std::vector<int> out((size_t)n, 0);
    if (n == 0) return out;
    int avail = total - gap * (n - 1);
    if (avail < n) avail = n;
    int fixedSum = 0, autos = 0;
    for (int i = 0; i < n; ++i) {
        if (explicitW[(size_t)i] > 0) { out[(size_t)i] = explicitW[(size_t)i] < minW ? minW : explicitW[(size_t)i]; fixedSum += out[(size_t)i]; }
        else ++autos;
    }
    if (autos > 0) {
        int rest = avail - fixedSum;
        int each = rest / autos;
        if (each < minW) each = minW;
        int lastAuto = -1;
        for (int i = 0; i < n; ++i) if (explicitW[(size_t)i] <= 0) { out[(size_t)i] = each; lastAuto = i; }
        const int remainder = (avail - fixedSum) - each * autos;
        if (lastAuto >= 0 && remainder > 0) out[(size_t)lastAuto] += remainder;
    }
    return out;
}

// Dragging the divider between two adjacent cells: the pair's total
// is preserved; neither side goes below minW (or 1 in degenerate sums).
inline std::pair<int, int> adjustPairWidths(int w1, int w2, int delta, int minW) {
    const int sum = w1 + w2;
    int lo = minW, hi = sum - minW;
    if (hi < lo) { lo = sum / 2; hi = sum - lo; if (lo < 1) lo = 1; if (hi < lo) hi = lo; }
    int n1 = w1 + delta;
    if (n1 < lo) n1 = lo;
    if (n1 > hi) n1 = hi;
    return { n1, sum - n1 };
}

// Where a dragged cell would land: the count of sibling centres the
// pointer has passed = the insertion slot (0..n) in the sibling list
// WITHOUT the dragged cell.
inline int dropSlot(const std::vector<int>& centres, int pointer) {
    int slot = 0;
    for (int c : centres) if (pointer > c) ++slot;
    return slot;
}

// The index sequence after moving the item at `from` to position
// `to` (both in the ORIGINAL indexing; `to` is the slot in the list
// with the item removed, matching dropSlot).
inline std::vector<int> indexOrderAfterMove(int n, int from, int to) {
    std::vector<int> idx;
    for (int i = 0; i < n; ++i) if (i != from) idx.push_back(i);
    if (to < 0) to = 0;
    if (to > (int)idx.size()) to = (int)idx.size();
    idx.insert(idx.begin() + to, from);
    return idx;
}
} // namespace sessionbar
