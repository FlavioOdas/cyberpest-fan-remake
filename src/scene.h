// The room: objects placed by the room module, each running its data-defined state machine.
// M4 step 1: objects enter their initial state and show its image; events/timing come next.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "adm.h"
#include "flc.h"
#include "surface.h"

struct SceneObject {
    int32_t moduleId = 0;
    const adm::Node* module = nullptr;
    const adm::Node* state = nullptr;
    int x = 0, y = 0, z = 0;  // x, y = bottom-centre anchor of the image
    const Flc* image = nullptr;
    int frame = 0;
};

class Scene {
public:
    bool loadRoom(const adm::Database& db, int32_t roomId, std::string* error);
    void draw(Surface& target) const;
    const std::vector<SceneObject>& objects() const { return objects_; }

private:
    void enterState(SceneObject& obj, int32_t stateId);
    const Flc* image(const adm::FileRef& file);

    const adm::Database* db_ = nullptr;
    std::vector<SceneObject> objects_;
    std::unordered_map<uint32_t, std::unique_ptr<Flc>> images_;  // decoded FLCs by file offset
};
