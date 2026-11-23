// =====================================================================
//  Chaos City - Sorting.h
//  Six classic sorting algorithms on arrays, all generic (templates).
//  Each counts comparisons and element moves so the dashboard / DSA panel
//  can compare them on real simulation data.
//
//    Algorithm   Best        Average     Worst       Stable  Extra memory
//    Bubble      O(n)        O(n^2)      O(n^2)      yes     O(1)
//    Selection   O(n^2)      O(n^2)      O(n^2)      no      O(1)
//    Insertion   O(n)        O(n^2)      O(n^2)      yes     O(1)
//    Merge       O(n log n)  O(n log n)  O(n log n)  yes     O(n)
//    Quick       O(n log n)  O(n log n)  O(n^2)*     no      O(log n)
//    Heap        O(n log n)  O(n log n)  O(n log n)  no      O(1)
//    * median-of-three pivot makes the worst case very unlikely
//
//  'Less' is a functor: less(a, b) == true when a must come before b.
//  Example: struct ByTime { bool operator()(const TripRecord& a,
//                         const TripRecord& b) const { return a.travelTime < b.travelTime; } };
// =====================================================================
#ifndef SORTING_H
#define SORTING_H

struct SortStats {
    long long comparisons;
    long long moves;          // element assignments / swaps
    SortStats() : comparisons(0), moves(0) {}
    void reset() { comparisons = moves = 0; }
};

struct DefaultLess {
    template <typename T>
    bool operator()(const T& a, const T& b) const { return a < b; }
};

enum SortAlgorithm { BUBBLE_SORT = 0, SELECTION_SORT, INSERTION_SORT, MERGE_SORT, QUICK_SORT, HEAP_SORT };
const int SORT_ALGORITHM_COUNT = 6;

inline const char* sortName(SortAlgorithm a) {
    switch (a) {
        case BUBBLE_SORT:    return "Bubble Sort";
        case SELECTION_SORT: return "Selection Sort";
        case INSERTION_SORT: return "Insertion Sort";
        case MERGE_SORT:     return "Merge Sort";
        case QUICK_SORT:     return "Quick Sort";
        default:             return "Heap Sort";
    }
}

class Sorting {
private:
    template <typename T>
    static void swapItems(T* a, int i, int j, SortStats& s) {
        T t = a[i]; a[i] = a[j]; a[j] = t;
        s.moves += 3;
    }

    // ---------- merge sort helpers (recursive) ----------
    template <typename T, typename Less>
    static void mergeRec(T* a, T* tmp, int lo, int hi, Less less, SortStats& s) {
        if (hi - lo < 1) return;
        int mid = lo + (hi - lo) / 2;
        mergeRec(a, tmp, lo, mid, less, s);          // sort left half
        mergeRec(a, tmp, mid + 1, hi, less, s);      // sort right half
        int i = lo, j = mid + 1, k = lo;
        while (i <= mid && j <= hi) {
            s.comparisons++;
            // take from the RIGHT only if strictly smaller -> stable
            if (less(a[j], a[i])) tmp[k++] = a[j++];
            else                  tmp[k++] = a[i++];
            s.moves++;
        }
        while (i <= mid) { tmp[k++] = a[i++]; s.moves++; }
        while (j <= hi)  { tmp[k++] = a[j++]; s.moves++; }
        for (k = lo; k <= hi; k++) { a[k] = tmp[k]; s.moves++; }
    }

    // ---------- quick sort helper (recursive) ----------
    // Hoare partition with median-of-three pivot.
    // Two pointers move towards each other; BOTH stop on elements equal to
    // the pivot and swap them, so duplicates are spread evenly over both
    // halves (data with many equal keys, e.g. queue lengths of 0, stays
    // O(n log n)), and sorted / reversed input splits in the middle.
    template <typename T, typename Less>
    static void quickRec(T* a, int lo, int hi, Less less, SortStats& s) {
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            s.comparisons += 3;
            if (less(a[mid], a[lo])) swapItems(a, mid, lo, s);
            if (less(a[hi], a[lo]))  swapItems(a, hi, lo, s);
            if (less(a[hi], a[mid])) swapItems(a, hi, mid, s);
            T pivot = a[mid];                      // median of three
            int i = lo, j = hi;
            while (i <= j) {
                while (s.comparisons++, less(a[i], pivot)) i++;
                while (s.comparisons++, less(pivot, a[j])) j--;
                if (i <= j) {
                    if (i != j) swapItems(a, i, j, s);
                    i++; j--;
                }
            }
            // now a[lo..j] <= pivot <= a[i..hi]
            // recurse into the smaller side, loop on the bigger -> O(log n) stack
            if (j - lo < hi - i) { quickRec(a, lo, j, less, s); lo = i; }
            else                 { quickRec(a, i, hi, less, s); hi = j; }
        }
    }

    // ---------- heap sort helper ----------
    template <typename T, typename Less>
    static void siftDown(T* a, int i, int n, Less less, SortStats& s) {
        while (true) {
            int l = 2 * i + 1, r = l + 1, big = i;
            if (l < n) { s.comparisons++; if (less(a[big], a[l])) big = l; }
            if (r < n) { s.comparisons++; if (less(a[big], a[r])) big = r; }
            if (big == i) return;
            swapItems(a, i, big, s);
            i = big;
        }
    }

public:
    template <typename T, typename Less>
    static void bubbleSort(T* a, int n, Less less, SortStats& s) {
        for (int pass = 0; pass < n - 1; pass++) {
            bool swapped = false;
            for (int j = 0; j < n - 1 - pass; j++) {
                s.comparisons++;
                if (less(a[j + 1], a[j])) { swapItems(a, j, j + 1, s); swapped = true; }
            }
            if (!swapped) break;                   // already sorted -> O(n)
        }
    }

    template <typename T, typename Less>
    static void selectionSort(T* a, int n, Less less, SortStats& s) {
        for (int i = 0; i < n - 1; i++) {
            int minIdx = i;
            for (int j = i + 1; j < n; j++) {
                s.comparisons++;
                if (less(a[j], a[minIdx])) minIdx = j;
            }
            if (minIdx != i) swapItems(a, i, minIdx, s);
        }
    }

    template <typename T, typename Less>
    static void insertionSort(T* a, int n, Less less, SortStats& s) {
        for (int i = 1; i < n; i++) {
            T key = a[i];
            s.moves++;
            int j = i - 1;
            while (j >= 0) {
                s.comparisons++;
                if (!less(key, a[j])) break;
                a[j + 1] = a[j];
                s.moves++;
                j--;
            }
            a[j + 1] = key;
            s.moves++;
        }
    }

    template <typename T, typename Less>
    static void mergeSort(T* a, int n, Less less, SortStats& s) {
        if (n < 2) return;
        T* tmp = new T[n];
        mergeRec(a, tmp, 0, n - 1, less, s);
        delete[] tmp;
    }

    template <typename T, typename Less>
    static void quickSort(T* a, int n, Less less, SortStats& s) {
        if (n > 1) quickRec(a, 0, n - 1, less, s);
    }

    template <typename T, typename Less>
    static void heapSort(T* a, int n, Less less, SortStats& s) {
        for (int i = n / 2 - 1; i >= 0; i--) siftDown(a, i, n, less, s);   // build max-heap
        for (int end = n - 1; end > 0; end--) {
            swapItems(a, 0, end, s);               // largest to the back
            siftDown(a, 0, end, less, s);
        }
    }

    // Run any algorithm by enum (used by the DSA panel buttons)
    template <typename T, typename Less>
    static void sort(SortAlgorithm algo, T* a, int n, Less less, SortStats& s) {
        switch (algo) {
            case BUBBLE_SORT:    bubbleSort(a, n, less, s); break;
            case SELECTION_SORT: selectionSort(a, n, less, s); break;
            case INSERTION_SORT: insertionSort(a, n, less, s); break;
            case MERGE_SORT:     mergeSort(a, n, less, s); break;
            case QUICK_SORT:     quickSort(a, n, less, s); break;
            default:             heapSort(a, n, less, s); break;
        }
    }

    template <typename T, typename Less>
    static bool isSorted(const T* a, int n, Less less) {
        for (int i = 1; i < n; i++) if (less(a[i], a[i - 1])) return false;
        return true;
    }
};

#endif
