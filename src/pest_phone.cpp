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
    } else if (caller == -207) {  // 1008:ced0, partial [inf]
        if (ill()) return listText(*module, -209);
        if (const adm::Node* advice = adm::childModule(*module, -229)) return randomText(*advice);
    } else if (caller == -209) {  // 1008:d6a4, partial [inf]
        if (const adm::Node* diagnoses = adm::childModule(*module, -220)) {
            const int32_t reason = fleas_ ? -221 : rabies_ ? -222 : junkSick() ? -223 :
                                   starving() ? -224 : onHeat() ? -225 : needsToilet() ? -228 : 0;
            if (reason) return listText(*diagnoses, reason);
        }
        if (const adm::Node* advice = adm::childModule(*module, -229)) return randomText(*advice);
    }
    return listText(*module, -219);
}
