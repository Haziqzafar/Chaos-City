// =====================================================================
//  Chaos City - Queue.h
//  Two queue implementations:
//   1) Queue<T>         - linked list (unbounded). Used for vehicles
//                         waiting at a traffic light and for BFS.
//   2) CircularQueue<T> - fixed-size array with wrap-around. Used for
//                         a road lane that can hold only N cars.
//  Concepts: queues, arrays, dynamic memory.
// =====================================================================
#ifndef QUEUE_H
#define QUEUE_H

// ---------------- Linked-list queue ----------------
template <typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& v) : data(v), next(NULL) {}
    };
    Node* frontNode;
    Node* rearNode;
    int count;

    Queue(const Queue&);
    Queue& operator=(const Queue&);

public:
    Queue() : frontNode(NULL), rearNode(NULL), count(0) {}
    ~Queue() { clear(); }

    void enqueue(const T& value) {         // O(1)
        Node* node = new Node(value);
        if (rearNode == NULL) frontNode = rearNode = node;
        else { rearNode->next = node; rearNode = node; }
        count++;
    }

    bool dequeue(T& out) {                 // O(1)
        if (frontNode == NULL) return false;
        Node* temp = frontNode;
        out = temp->data;
        frontNode = frontNode->next;
        if (frontNode == NULL) rearNode = NULL;
        delete temp;
        count--;
        return true;
    }

    bool peek(T& out) const {
        if (frontNode == NULL) return false;
        out = frontNode->data;
        return true;
    }

    void clear() {
        T dummy;
        while (dequeue(dummy)) {}
    }

    bool isEmpty() const { return count == 0; }
    int size() const { return count; }
};

// ---------------- Circular array queue ----------------
template <typename T>
class CircularQueue {
private:
    T* arr;
    int capacity;
    int frontIdx;
    int rearIdx;
    int count;

    CircularQueue(const CircularQueue&);
    CircularQueue& operator=(const CircularQueue&);

public:
    CircularQueue(int cap) : capacity(cap), frontIdx(0), rearIdx(-1), count(0) {
        arr = new T[capacity];
    }
    ~CircularQueue() { delete[] arr; }

    bool enqueue(const T& value) {         // O(1), fails when lane is full
        if (isFull()) return false;
        rearIdx = (rearIdx + 1) % capacity;
        arr[rearIdx] = value;
        count++;
        return true;
    }

    bool dequeue(T& out) {                 // O(1)
        if (isEmpty()) return false;
        out = arr[frontIdx];
        frontIdx = (frontIdx + 1) % capacity;
        count--;
        return true;
    }

    bool peek(T& out) const {
        if (isEmpty()) return false;
        out = arr[frontIdx];
        return true;
    }

    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count == capacity; }
    int size() const { return count; }
    int getCapacity() const { return capacity; }
};

#endif
