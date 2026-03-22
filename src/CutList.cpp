#include "CutList.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* CUTLIST_FILE = "/cutlist.json";

CutList::CutList() {}

void CutList::begin() {
    load();
}

void CutList::addEntry(const CutEntry& entry) {
    _entries.push_back(entry);
    save();
}

void CutList::removeEntry(int index) {
    if (index < 0 || index >= (int)_entries.size()) return;
    _entries.erase(_entries.begin() + index);
    save();
}

void CutList::markCompleted(int index) {
    if (index < 0 || index >= (int)_entries.size()) return;
    _entries[index].completed = true;
    save();
}

void CutList::clearAll() {
    _entries.clear();
    save();
}

void CutList::reset() {
    for (auto& e : _entries) {
        e.completed = false;
    }
    save();
}

bool CutList::hasNext() const {
    return getNextIndex() >= 0;
}

CutEntry CutList::getNext() const {
    int idx = getNextIndex();
    if (idx >= 0) return _entries[idx];
    return { "", 0, 0, true };
}

int CutList::getNextIndex() const {
    for (int i = 0; i < (int)_entries.size(); i++) {
        if (!_entries[i].completed) return i;
    }
    return -1;
}

int CutList::size() const {
    return (int)_entries.size();
}

std::vector<CutEntry> CutList::getAll() const {
    return _entries;
}

String CutList::toJSON() const {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& entry : _entries) {
        JsonObject obj = arr.add<JsonObject>();
        obj["label"] = entry.label;
        obj["length_mm"] = entry.lengthMM;
        obj["quantity"] = entry.quantity;
        obj["completed"] = entry.completed;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void CutList::fromJSON(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return;

    _entries.clear();
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        CutEntry entry;
        entry.label = obj["label"].as<String>();
        entry.lengthMM = obj["length_mm"].as<float>();
        entry.quantity = obj["quantity"] | 1;
        entry.completed = obj["completed"] | false;
        _entries.push_back(entry);
    }
    save();
}

void CutList::save() {
    File file = LittleFS.open(CUTLIST_FILE, "w");
    if (!file) return;
    file.print(toJSON());
    file.close();
}

void CutList::load() {
    File file = LittleFS.open(CUTLIST_FILE, "r");
    if (!file) return;
    String json = file.readString();
    file.close();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return;

    _entries.clear();
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        CutEntry entry;
        entry.label = obj["label"].as<String>();
        entry.lengthMM = obj["length_mm"].as<float>();
        entry.quantity = obj["quantity"] | 1;
        entry.completed = obj["completed"] | false;
        _entries.push_back(entry);
    }
}
