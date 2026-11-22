// =====================================================================
//  Chaos City - HashTable.h
//  Hash table with separate chaining (each bucket is a linked chain).
//  Rehashes (doubles bucket count) when load factor exceeds 0.75.
//  Used for: Vehicle ID -> Vehicle* lookup in O(1) average,
//            intersection name -> index lookup.
//  Concepts: hashing, collision handling, pointers, dynamic memory.
// =====================================================================
#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <string>

// ---- Hash functions (overloaded per key type) ----
inline unsigned int hashKey(int key) {
    // Knuth multiplicative hashing
    return (unsigned int)key * 2654435761u;
}

inline unsigned int hashKey(const std::string& key) {
    // Polynomial rolling hash: h = h*31 + c
    unsigned int h = 0;
    for (size_t i = 0; i < key.length(); i++)
        h = h * 31 + (unsigned char)key[i];
    return h;
}

template <typename K, typename V>
class HashTable {
private:
    struct Entry {
        K key;
        V value;
        Entry* next;
        Entry(const K& k, const V& v, Entry* n) : key(k), value(v), next(n) {}
    };

    Entry** buckets;     // dynamic array of pointers to chains
    int bucketCount;
    int count;
    int collisions;      // tracked for the analytics / DSA panel

    HashTable(const HashTable&);
    HashTable& operator=(const HashTable&);

    int indexFor(const K& key, int nBuckets) const {
        return (int)(hashKey(key) % (unsigned int)nBuckets);
    }

    void rehash() {
        int newCount = bucketCount * 2;
        Entry** newBuckets = new Entry*[newCount];
        for (int i = 0; i < newCount; i++) newBuckets[i] = NULL;

        // Move every existing entry into the new array (no re-allocation of entries)
        for (int i = 0; i < bucketCount; i++) {
            Entry* e = buckets[i];
            while (e != NULL) {
                Entry* nextE = e->next;
                int idx = indexFor(e->key, newCount);
                e->next = newBuckets[idx];
                newBuckets[idx] = e;
                e = nextE;
            }
        }
        delete[] buckets;
        buckets = newBuckets;
        bucketCount = newCount;
    }

public:
    HashTable(int initialBuckets = 16)
        : bucketCount(initialBuckets), count(0), collisions(0) {
        buckets = new Entry*[bucketCount];
        for (int i = 0; i < bucketCount; i++) buckets[i] = NULL;
    }

    ~HashTable() {
        clear();
        delete[] buckets;
    }

    // Insert or update. O(1) average.
    void put(const K& key, const V& value) {
        int idx = indexFor(key, bucketCount);
        for (Entry* e = buckets[idx]; e != NULL; e = e->next) {
            if (e->key == key) { e->value = value; return; }
        }
        if (buckets[idx] != NULL) collisions++;
        buckets[idx] = new Entry(key, value, buckets[idx]);
        count++;
        if ((double)count / bucketCount > 0.75) rehash();
    }

    // Lookup. Returns false if key not present. O(1) average.
    bool get(const K& key, V& out) const {
        int idx = indexFor(key, bucketCount);
        for (Entry* e = buckets[idx]; e != NULL; e = e->next) {
            if (e->key == key) { out = e->value; return true; }
        }
        return false;
    }

    bool contains(const K& key) const {
        V dummy;
        return get(key, dummy);
    }

    bool remove(const K& key) {
        int idx = indexFor(key, bucketCount);
        Entry* prev = NULL;
        Entry* e = buckets[idx];
        while (e != NULL) {
            if (e->key == key) {
                if (prev == NULL) buckets[idx] = e->next;
                else prev->next = e->next;
                delete e;
                count--;
                return true;
            }
            prev = e;
            e = e->next;
        }
        return false;
    }

    void clear() {
        for (int i = 0; i < bucketCount; i++) {
            Entry* e = buckets[i];
            while (e != NULL) {
                Entry* nextE = e->next;
                delete e;
                e = nextE;
            }
            buckets[i] = NULL;
        }
        count = 0;
        collisions = 0;
    }

    // Copies all keys into a caller-provided array (size must be >= size()).
    // Returns number copied. Useful for listing/sorting.
    int getKeys(K* outArray) const {
        int k = 0;
        for (int i = 0; i < bucketCount; i++)
            for (Entry* e = buckets[i]; e != NULL; e = e->next)
                outArray[k++] = e->key;
        return k;
    }

    int size() const { return count; }
    int getBucketCount() const { return bucketCount; }
    int getCollisions() const { return collisions; }
    double loadFactor() const { return (double)count / bucketCount; }
};

#endif
