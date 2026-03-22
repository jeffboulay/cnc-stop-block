#pragma once

#include <Arduino.h>
#include <vector>

struct CutEntry {
    String label;
    float  lengthMM;
    int    quantity;
    bool   completed;
};

class CutList {
public:
    CutList();

    void begin();
    void addEntry(const CutEntry& entry);
    void removeEntry(int index);
    void markCompleted(int index);
    void clearAll();
    void reset(); // Mark all as not completed

    bool hasNext() const;
    CutEntry getNext() const;
    int getNextIndex() const;
    int size() const;
    std::vector<CutEntry> getAll() const;

    String toJSON() const;
    void fromJSON(const String& json);

private:
    std::vector<CutEntry> _entries;
    void save();
    void load();
};
