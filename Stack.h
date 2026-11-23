// =====================================================================
//  Chaos City - Stack.h
//  Linked-list based stack (LIFO).
//  Used for: Undo of Chaos Mode actions, iterative DFS,
//            rebuilding a Dijkstra path from the parent array.
// =====================================================================
#ifndef STACK_H
#define STACK_H

template <typename T>
class Stack {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& v, Node* n) : data(v), next(n) {}
    };
    Node* topNode;
    int count;

    Stack(const Stack&);
    Stack& operator=(const Stack&);

public:
    Stack() : topNode(NULL), count(0) {}
    ~Stack() { clear(); }

    void push(const T& value) {            // O(1)
        topNode = new Node(value, topNode);
        count++;
    }

    bool pop(T& out) {                     // O(1)
        if (topNode == NULL) return false;
        Node* temp = topNode;
        out = temp->data;
        topNode = topNode->next;
        delete temp;
        count--;
        return true;
    }

    bool peek(T& out) const {              // O(1)
        if (topNode == NULL) return false;
        out = topNode->data;
        return true;
    }

    void clear() {
        T dummy;
        while (pop(dummy)) {}
    }

    bool isEmpty() const { return topNode == NULL; }
    int size() const { return count; }
};

#endif
