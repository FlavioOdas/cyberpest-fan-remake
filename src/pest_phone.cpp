#include "pest.h"

#include <cstdlib>
#include <vector>

#include "world.h"

namespace {

std::string listText(const adm::Node& module, int32_t id) {
    for (const adm::Node& list : module.children) {
        if (list.tag != adm::Tag::List || list.id != id) continue;
        for (const adm::Node& value : list.children) {
            if (value.tag == adm::Tag::String) return value.text;
        }
    }
    return {};
}

std::string randomText(const adm::Node& module, size_t first = 0) {
    std::vector<std::string> choices;
    for (const adm::Node& list : module.children) {
        if (list.tag != adm::Tag::List) continue;
        for (const adm::Node& value : list.children) {
            if (value.tag == adm::Tag::String) {
                choices.push_back(value.text);
                break;
            }
        }
    }
    if (first >= choices.size()) return {};
    return choices[first + size_t(std::rand()) % (choices.size() - first)];
}

// Text of the index-th list of a module, counting every list (1018:8f30 with an index path).
std::string nthText(const adm::Node& module, int index) {
    for (const adm::Node& list : module.children) {
        if (list.tag != adm::Tag::List || index-- != 0) continue;
        for (const adm::Node& value : list.children) {
            if (value.tag == adm::Tag::String) return value.text;
        }
        return {};
    }
    return {};
}

}  // namespace

std::string Pest::callerText(int32_t caller) {  // 1008:ca82
    const adm::Node* module = world_.db().findModule(caller);
    if (!module || caller < -210 || caller > -206) return {};
    const int index = caller == -206 ? 0 : caller == -208 ? 1 : caller == -209 ? 2 :
                      caller == -207 ? 3 : 4;
    const bool firstCall = saved29a_[index] == 0;
    if (firstCall) {
        saved29a_[index] = 1;
        if (caller != -210) return listText(*module, -219);
    }
    if (caller == -206) {  // 1008:cd8e
        if (const adm::Node* tips = adm::childModule(*module, -273)) return randomText(*tips);
    } else if (caller == -208) {  // 1008:d3e8
        if (const adm::Node* tips = adm::childModule(*module, roomMessy() ? -272 : -273))
            return randomText(*tips);
    } else if (caller == -210) {  // 1008:dd66
        int hour = 1;
        for (const adm::Node& row : module->children) {
            if (row.tag != adm::Tag::Map) continue;
            if (hour++ != birthHour_) continue;
            if (firstCall) return listText(row, -219);
            return randomText(row, 1);
        }
    } else if (caller == -207) {  // 1008:ced0 TheraPest
        if (ill()) return listText(*module, -209);  // refer to the Vet
        if (const adm::Node* moods = adm::childModule(*module, -220)) {
            if (suicidal()) return listText(*moods, -267);
            if (depressed()) return listText(*moods, -268);
            if (overExcited()) return listText(*moods, -270);
            if (happy()) return listText(*moods, -269);
        }
        if (treated_) {  // a snack or shock was just given
            treated_ = 0;
            if (const adm::Node* drugs = adm::childModule(*module, -229)) return randomText(*drugs);
        }
        for (int tries = 0; tries < 64; ++tries) {  // until some non-empty advice is found
            std::string text;
            const int choice = std::rand() % 3;
            if (choice == 0) {  // personality and mood: [-271, index]
                auto band = [](int v) { return v < 2000 ? 0 : v <= 8000 ? 1 : 2; };
                const int moodBand = mood_ < -20 ? 0 : mood_ <= 20 ? 1 : 2;
                const int index = (band(personality_.y) * 3 + band(personality_.x)) * 3 + moodBand;
                if (const adm::Node* table = adm::childModule(*module, -271)) text = nthText(*table, index);
            } else {  // favourite activity [-238, symbol] or reaction [-252, symbol] (1010:28ec)
                const PreferenceList& prefs = choice == 1 ? activities_ : reactions_;
                const adm::Node* table = adm::childModule(*module, choice == 1 ? -238 : -252);
                if (table && !prefs.entries.empty()) text = listText(*table, prefs.entries.front().first);
            }
            if (!text.empty()) return text;
        }
    } else if (caller == -209) {  // 1008:d6a4 Vet
        if (const adm::Node* symptoms = adm::childModule(*module, -220)) {
            if (state() == -226) return listText(*symptoms, -226);
            if (state() == -227) return listText(*symptoms, -227);
            if (rabid()) return listText(*symptoms, -222);
            if (fleas_) return listText(*symptoms, -221);
            if (starving()) return listText(*symptoms, -224);
            if (needsToilet()) return listText(*symptoms, -228);
            if (junkSick()) return listText(*symptoms, -223);
            if (onHeat()) return listText(*symptoms, -225);
        }
        if (moodExtreme()) return listText(*module, -207);  // refer to the TheraPest
        if (treated_) {
            treated_ = 0;
            if (const adm::Node* misuse = adm::childModule(*module, -229)) return randomText(*misuse);
        }
        if (const adm::Node* diet = adm::childModule(*module, -230)) {
            const int sum = good_ + junk_;
            if (bowels_ >= 80) return listText(*diet, -231);
            if (junk_ >= 80) return listText(*diet, -234);
            if (good_ >= 80) return listText(*diet, -235);
            if (sum >= 80) return listText(*diet, -232);
            if (sum < 11) return listText(*diet, -233);
            if (good_ < 11) return listText(*diet, -237);
            if (junk_ < 11 && good_ < 81) return listText(*diet, -236);
        }
        if (std::rand() % 2) {
            const adm::Node* habits = adm::childModule(*module, -238);
            if (habits && !activities_.entries.empty()) {
                const std::string text = listText(*habits, activities_.entries.front().first);
                if (!text.empty()) return text;
            }
        }
        const adm::Node* reactions = adm::childModule(*module, -252);
        if (reactions && !reactions_.entries.empty()) {
            const std::string text = listText(*reactions, reactions_.entries.front().first);
            if (!text.empty()) return text;
        }
    }
    return listText(*module, -219);
}
