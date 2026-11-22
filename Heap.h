// =====================================================================
//  Chaos City - Heap.h
//  Array-based binary heap that grows dynamically.
//  The same class works as a MIN-heap or MAX-heap (constructor flag).
//   MIN-heap -> priority queue for Dijkstra (smallest distance first)
//   MAX-heap -> adaptive signals (most congested intersection first)
//  Concepts: heaps, priority queues, arrays, dynamic memory.
//  T must support operator< .
// =====================================================================
#ifndef HEAP_H
#define HEAP_H

template <typename T>
class Heap {
private:
    T* arr;
    int capacity;
    int count;
    bool isMinHeap;

    Heap(const Heap&);
    Heap& operator=(const Heap&);

    // true if a should be above b in the heap
    bool higherPriority(const T& a, const T& b) const {
        return isMinHeap ? (a < b) : (b < a);
    }

    void swap(int i, int j) {
        T temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }

    // Double the array when full (dynamic memory)
    void grow() {
        int newCap = capacity * 2;
        T* bigger = new T[newCap];
        for (int i = 0; i < count; i++) bigger[i] = arr[i];
        delete[] arr;
        arr = bigger;
        capacity = newCap;
    }

    void heapifyUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (higherPriority(arr[i], arr[parent])) {
                swap(i, parent);
                i = parent;
            } else break;
        }
    }

    void heapifyDown(int i) {
        while (true) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int best = i;
            if (left < count && higherPriority(arr[left], arr[best])) best = left;
            if (right < count && higherPriority(arr[right], arr[best])) best = right;
            if (best == i) break;
            swap(i, best);
            i = best;
        }
    }

public:
    Heap(bool minHeap = true, int initialCapacity = 16)
        : capacity(initialCapacity), count(0), isMinHeap(minHeap) {
        arr = new T[capacity];
    }
    ~Heap() { delete[] arr; }

    void push(const T& value) {            // O(log n)
        if (count == capacity) grow();
        arr[count] = value;
        heapifyUp(count);
        count++;
    }

    bool pop(T& out) {                     // O(log n)
        if (count == 0) return false;
        out = arr[0];
        arr[0] = arr[count - 1];
        count--;
        if (count > 0) heapifyDown(0);
        return true;
    }

    bool top(T& out) const {               // O(1)
        if (count == 0) return false;
        out = arr[0];
        return true;
    }

    void clear() { count = 0; }
    bool isEmpty() const { return count == 0; }
    int size() const { return count; }
};

#endif
