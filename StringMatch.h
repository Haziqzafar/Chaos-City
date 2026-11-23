// =====================================================================
//  Chaos City - StringMatch.h
//  Pattern matching, used to search the Chaos event log and place names.
//
//    Naive       O(n*m) worst case - try every start position
//    KMP         O(n+m)            - LPS table: never moves backwards in text
//    Rabin-Karp  O(n+m) average    - rolling hash; characters compared only
//                                    when hashes match (spurious hits counted)
//
//  All three return the number of matches (overlapping matches count),
//  optionally write match positions, and count character comparisons.
// =====================================================================
#ifndef STRINGMATCH_H
#define STRINGMATCH_H

#include <string>

enum MatchAlgorithm { NAIVE_MATCH = 0, KMP_MATCH = 1, RABIN_KARP_MATCH = 2 };
const int MATCH_ALGORITHM_COUNT = 3;

inline const char* matchName(MatchAlgorithm a) {
    switch (a) {
        case NAIVE_MATCH: return "Naive";
        case KMP_MATCH:   return "KMP";
        default:          return "Rabin-Karp";
    }
}

struct MatchStats {
    long long comparisons;     // character comparisons
    int matches;
    int spuriousHits;          // Rabin-Karp: hash equal but text different
    MatchStats() : comparisons(0), matches(0), spuriousHits(0) {}
    void reset() { comparisons = 0; matches = 0; spuriousHits = 0; }
    void add(const MatchStats& o) {
        comparisons += o.comparisons; matches += o.matches; spuriousHits += o.spuriousHits;
    }
};

class StringMatch {
private:
    static void record(int pos, int* positions, int maxPositions, MatchStats& s) {
        if (positions != NULL && s.matches < maxPositions) positions[s.matches] = pos;
        s.matches++;
    }

public:
    static std::string toLower(const std::string& s) {
        std::string r = s;
        for (size_t i = 0; i < r.length(); i++)
            if (r[i] >= 'A' && r[i] <= 'Z') r[i] = (char)(r[i] - 'A' + 'a');
        return r;
    }

    // ---------------- Naive ----------------
    static int naive(const std::string& text, const std::string& pat,
                     int* positions, int maxPositions, MatchStats& s) {
        int n = (int)text.length(), m = (int)pat.length();
        if (m == 0 || m > n) return 0;
        int before = s.matches;
        for (int i = 0; i <= n - m; i++) {
            int j = 0;
            while (j < m) {
                s.comparisons++;
                if (text[i + j] != pat[j]) break;
                j++;
            }
            if (j == m) record(i, positions, maxPositions, s);
        }
        return s.matches - before;
    }

    // ---------------- KMP ----------------
    // lps[i] = length of the longest proper prefix of pat[0..i] that is
    // also a suffix of it. On a mismatch we jump to lps instead of
    // restarting, so the text index never goes back.
    static void buildLPS(const std::string& pat, int* lps) {
        int m = (int)pat.length();
        if (m == 0) return;
        lps[0] = 0;
        int len = 0, i = 1;
        while (i < m) {
            if (pat[i] == pat[len]) { lps[i++] = ++len; }
            else if (len > 0)       { len = lps[len - 1]; }
            else                    { lps[i++] = 0; }
        }
    }

    static int kmp(const std::string& text, const std::string& pat,
                   int* positions, int maxPositions, MatchStats& s) {
        int n = (int)text.length(), m = (int)pat.length();
        if (m == 0 || m > n) return 0;
        int before = s.matches;
        int* lps = new int[m];
        buildLPS(pat, lps);
        int i = 0, j = 0;
        while (i < n) {
            s.comparisons++;
            if (text[i] == pat[j]) {
                i++; j++;
                if (j == m) {
                    record(i - m, positions, maxPositions, s);
                    j = lps[j - 1];                  // continue: overlapping matches
                }
            } else if (j > 0) {
                j = lps[j - 1];
            } else {
                i++;
            }
        }
        delete[] lps;
        return s.matches - before;
    }

    // ---------------- Rabin-Karp ----------------
    // hash(s) = s[0]*B^(m-1) + ... + s[m-1]  (mod M)
    // Sliding the window: remove old char * B^(m-1), multiply by B, add new char.
    static int rabinKarp(const std::string& text, const std::string& pat,
                         int* positions, int maxPositions, MatchStats& s,
                         long long mod = 1000003LL) {
        int n = (int)text.length(), m = (int)pat.length();
        if (m == 0 || m > n) return 0;
        const long long B = 256;
        int before = s.matches;
        long long highPow = 1;                       // B^(m-1) mod M
        for (int i = 0; i < m - 1; i++) highPow = (highPow * B) % mod;
        long long hp = 0, ht = 0;
        for (int i = 0; i < m; i++) {
            hp = (hp * B + (unsigned char)pat[i]) % mod;
            ht = (ht * B + (unsigned char)text[i]) % mod;
        }
        for (int i = 0; i <= n - m; i++) {
            if (hp == ht) {                          // possible match -> verify
                int j = 0;
                while (j < m) {
                    s.comparisons++;
                    if (text[i + j] != pat[j]) break;
                    j++;
                }
                if (j == m) record(i, positions, maxPositions, s);
                else s.spuriousHits++;
            }
            if (i < n - m) {                         // roll the window
                ht = (ht - (unsigned char)text[i] * highPow % mod + mod) % mod;
                ht = (ht * B + (unsigned char)text[i + m]) % mod;
            }
        }
        return s.matches - before;
    }

    // ---------------- dispatcher ----------------
    static int search(MatchAlgorithm algo, const std::string& text, const std::string& pattern,
                      int* positions, int maxPositions, MatchStats& s, bool ignoreCase = true) {
        std::string t = ignoreCase ? toLower(text) : text;
        std::string p = ignoreCase ? toLower(pattern) : pattern;
        switch (algo) {
            case NAIVE_MATCH: return naive(t, p, positions, maxPositions, s);
            case KMP_MATCH:   return kmp(t, p, positions, maxPositions, s);
            default:          return rabinKarp(t, p, positions, maxPositions, s);
        }
    }

    static bool contains(MatchAlgorithm algo, const std::string& text, const std::string& pattern,
                         MatchStats& s, bool ignoreCase = true) {
        return search(algo, text, pattern, NULL, 0, s, ignoreCase) > 0;
    }
};

#endif
