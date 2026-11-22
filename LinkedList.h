// =====================================================================
//  Chaos City - LinkedList.h
//  Singly linked list with head and tail pointers.
//  Used for: graph adjacency lists, active vehicle list, event log.
//  Concepts: pointers, dynamic memory (new/delete), linked lists.
// =====================================================================
#ifndef LINKEDLIST_H
#define LINKEDLIST_H

template <typename T>
struct ListNode {
    T data;
    ListNode<T>* next;
    ListNode(const T& value) : data(value), next(NULL) {}
};

template <typename T>
class LinkedList {
private:
    ListNode<T>* head;
    ListNode<T>* tail;
    int count;

    // Copying is disabled so two lists never delete the same nodes.
    LinkedList(const LinkedList&);
    LinkedList& operator=(const LinkedList&);

public:
    LinkedList() : head(NULL), tail(NULL), count(0) {}
    ~LinkedList() { clear(); }

    // O(1): add at the front
    void pushFront(const T& value) {
        ListNode<T>* node = new ListNode<T>(value);
        node->next = head;
        head = node;
        if (tail == NULL) tail = node;
        count++;
    }

    // O(1): add at the back (tail pointer avoids walking the list)
    void pushBack(const T& value) {
        ListNode<T>* node = new ListNode<T>(value);
        if (tail == NULL) {
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
        count++;
    }

    // O(1): remove from the front. Returns false if empty.
    bool popFront(T& out) {
        if (head == NULL) return false;
        ListNode<T>* temp = head;
        out = temp->data;
        head = head->next;
        if (head == NULL) tail = NULL;
        delete temp;
        count--;
        return true;
    }

    // O(n): remove the first node whose data == value
    bool remove(const T& value) {
        ListNode<T>* prev = NULL;
        ListNode<T>* curr = head;
        while (curr != NULL) {
            if (curr->data == value) {
                if (prev == NULL) head = curr->next;
                else prev->next = curr->next;
                if (curr == tail) tail = prev;
                delete curr;
                count--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    // O(n): linear search
    bool contains(const T& value) const {
        for (ListNode<T>* c = head; c != NULL; c = c->next)
            if (c->data == value) return true;
        return false;
    }

    // Recursive reversal (demonstrates recursion on linked structures)
    void reverse() {
        tail = head;
        head = reverseRec(head);
    }

    void clear() {
        while (head != NULL) {
            ListNode<T>* temp = head;
            head = head->next;
            delete temp;
        }
        tail = NULL;
        count = 0;
    }

    int size() const { return count; }
    bool isEmpty() const { return count == 0; }

    // Read access for traversal: for (ListNode<T>* n = list.getHead(); n; n = n->next)
    ListNode<T>* getHead() const { return head; }

private:
    ListNode<T>* reverseRec(ListNode<T>* node) {
        if (node == NULL || node->next == NULL) return node;
        ListNode<T>* newHead = reverseRec(node->next);
        node->next->next = node;
        node->next = NULL;
        return newHead;
    }
};

#endif
