// =====================================================================
//  Chaos City - Searching.h
//  Searching on arrays.
//    linearSearch          O(n)      works on any array
//    binarySearch          O(log n)  array must be sorted (iterative)
//    binarySearchRecursive O(log n)  same, recursive version
//    lowerBound            O(log n)  first position with a[i] >= key
//                                     ("how many trips took < 10 min?")
//  'comparisons' is increased so the dashboard can show the difference.
// =====================================================================
#ifndef SEARCHING_H
#define SEARCHING_H

class Searching {
private:
    template <typename T, typename Less>
    static int binaryRec(const T* a, int lo, int hi, const T& key, Less less, int& comparisons) {
        if (lo > hi) return -1;                    // base case: not found
        int mid = lo + (hi - lo) / 2;
        comparisons++;
        if (less(a[mid], key)) return binaryRec(a, mid + 1, hi, key, less, comparisons);
        comparisons++;
        if (less(key, a[mid])) return binaryRec(a, lo, mid - 1, key, less, comparisons);
        return mid;                                // neither smaller nor bigger: equal
    }

public:
    // Equality is "neither less than the other", so the same functor works
    // for all searches.
    template <typename T, typename Less>
    static int linearSearch(const T* a, int n, const T& key, Less less, int& comparisons) {
        for (int i = 0; i < n; i++) {
            comparisons++;
            if (!less(a[i], key) && !less(key, a[i])) return i;
        }
        return -1;
    }

    template <typename T, typename Less>
    static int binarySearch(const T* a, int n, const T& key, Less less, int& comparisons) {
        int lo = 0, hi = n - 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;          // avoids (lo+hi) overflow
            comparisons++;
            if (less(a[mid], key)) { lo = mid + 1; continue; }
            comparisons++;
            if (less(key, a[mid])) { hi = mid - 1; continue; }
            return mid;
        }
        return -1;
    }

    template <typename T, typename Less>
    static int binarySearchRecursive(const T* a, int n, const T& key, Less less, int& comparisons) {
        return binaryRec(a, 0, n - 1, key, less, comparisons);
    }

    // Index of first element NOT less than key (n if all are less)
    template <typename T, typename Less>
    static int lowerBound(const T* a, int n, const T& key, Less less, int& comparisons) {
        int lo = 0, hi = n;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            comparisons++;
            if (less(a[mid], key)) lo = mid + 1;
            else hi = mid;
        }
        return lo;
    }
};

#endif
