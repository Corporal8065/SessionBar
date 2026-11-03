#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
namespace sessionbar {
// CrashWatch (v0.4.19) — the pure naming/notice logic behind
// SessionBar's hard-crash record (family crash-log round, Jackson
// 2026-11-03: every bar leaves a sendable crash record + an "Open
// Log Folder" button). Ported from MeterBar v0.28.3's CrashWatch
// (namespace + record title changed only). The app side (Windows:
// unhandled-exception filter + MiniDumpWriteDump) lives in
// app/Source/CrashHandler.h; everything DECIDABLE lives here so the
// suite can pin it:
//
//  - Crash files are named crash-YYYY-MM-DD_HHMMSS.txt (+ a sibling
//    .dmp written by the handler). The text file is the marker the
//    next-launch notice keys on.
//  - A crash counts as SEEN once a sibling "<name>.seen" file
//    exists (written when Jackson dismisses the notice or opens
//    the folder from it). A crash file with no .seen sibling is
//    UNSEEN and triggers exactly one notice, however many launches
//    it survives.
//  - The record text is composed here from plain fields, so what a
//    crash file contains is suite-pinned, not handler-improvised.

// Broken-down local time, supplied by the caller (the core never
// reads a clock — the same discipline as the rest of the suite).
struct CrashTimeParts { int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0; };

inline std::string crashFileStem(const CrashTimeParts& t) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "crash-%04d-%02d-%02d_%02d%02d%02d",
                  t.year, t.month, t.day, t.hour, t.minute, t.second);
    return buf;
}
inline std::string crashTextName(const CrashTimeParts& t) { return crashFileStem(t) + ".txt"; }
inline std::string crashDumpName(const CrashTimeParts& t) { return crashFileStem(t) + ".dmp"; }

inline bool isCrashTextName(const std::string& name) {
    // crash-YYYY-MM-DD_HHMMSS.txt — lengths: stem 23 ("crash-" 6 +
    // date 10 + "_" 1 + time 6), + ".txt" = 27.
    return name.size() == 27 && name.rfind("crash-", 0) == 0
        && name.compare(name.size() - 4, 4, ".txt") == 0;
}
inline bool isSeenMarkerName(const std::string& name) {
    // crash text name (27) + ".seen" (5) = 32.
    return name.size() == 32 && name.compare(name.size() - 5, 5, ".seen") == 0
        && isCrashTextName(name.substr(0, name.size() - 5));
}
inline std::string seenMarkerFor(const std::string& crashTextName) { return crashTextName + ".seen"; }

// Given a folder's file names, the crash records not yet seen —
// sorted oldest first (name order IS time order for this scheme).
inline std::vector<std::string> unseenCrashes(const std::vector<std::string>& fileNames) {
    std::vector<std::string> out;
    for (auto& n : fileNames) {
        if (!isCrashTextName(n)) continue;
        const std::string seen = seenMarkerFor(n);
        if (std::find(fileNames.begin(), fileNames.end(), seen) == fileNames.end())
            out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// --- FALLBACK CAPTURE ROUTE (v0.4.19 amendment, DESIGN §9) ---
// Field finding (MeterBar v0.28.3): the SetUnhandledExceptionFilter
// handler NEVER FIRES for heap-corruption deaths — ntdll fast-fail
// goes straight to WER, and no crash-*.txt/.dmp appeared despite
// confirmed crashes. The fallback: WER's own LocalDumps output in
// %LOCALAPPDATA%\CrashDumps (files named "SessionBar.exe.<pid>.dmp")
// is HARVESTED on the next launch into Documents\SessionBar and
// surfaced through the same notice + folder flow, dismiss-once per
// file (a "<name>.seen" sibling, same marker shape as crash texts).
inline bool isHarvestedDumpName(const std::string& name) {
    // "SessionBar.exe." (15) + pid digits + ".dmp" (4)
    return name.size() > 19 && name.rfind("SessionBar.exe.", 0) == 0
        && name.compare(name.size() - 4, 4, ".dmp") == 0;
}
inline std::string dumpSeenMarkerFor(const std::string& dumpName) { return dumpName + ".seen"; }
// Which CrashDumps files to copy: harvested-dump names that are not
// already in the Documents folder (by name) — a file is harvested
// at most once, however many launches follow.
inline std::vector<std::string> crashDumpsToHarvest(const std::vector<std::string>& crashDumpsNames,
                                                    const std::vector<std::string>& docsNames) {
    std::vector<std::string> out;
    for (auto& n : crashDumpsNames) {
        if (!isHarvestedDumpName(n)) continue;
        if (std::find(docsNames.begin(), docsNames.end(), n) == docsNames.end())
            out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}
// Harvested dumps in the Documents folder not yet seen — these earn
// the next-launch notice alongside unseen crash texts.
inline std::vector<std::string> unseenHarvestedDumps(const std::vector<std::string>& fileNames) {
    std::vector<std::string> out;
    for (auto& n : fileNames) {
        if (!isHarvestedDumpName(n)) continue;
        if (std::find(fileNames.begin(), fileNames.end(), dumpSeenMarkerFor(n)) == fileNames.end())
            out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// The crash record's text. `stateSnapshot` is the app's last honest
// state (pre-formatted "key: value" lines maintained by the app at
// runtime — the handler itself formats nothing beyond this plain
// composition, which also runs at install time in tests).
inline std::string crashRecordText(const std::string& appVersion,
                                   const std::string& windowsVersion,
                                   const std::string& whenText,
                                   unsigned long exceptionCode,
                                   const std::string& exceptionAddressHex,
                                   const std::string& stateSnapshot) {
    char code[16];
    std::snprintf(code, sizeof code, "0x%08lx", exceptionCode);
    std::string out = "SessionBar crash record\n";
    out += "app version: " + appVersion + "\n";
    out += "windows: " + windowsVersion + "\n";
    out += "when: " + whenText + "\n";
    out += "exception: " + std::string(code) + " at " + exceptionAddressHex + "\n";
    out += "state at crash (last known):\n";
    out += stateSnapshot.empty() ? "(none recorded)\n" : stateSnapshot;
    if (out.empty() || out.back() != '\n') out += "\n";
    out += "Send the .txt and the matching .dmp from this folder to Milo.\n";
    return out;
}
} // namespace sessionbar
