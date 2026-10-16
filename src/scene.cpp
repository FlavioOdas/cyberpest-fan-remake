#include "scene.h"

#include <algorithm>

#include "symbols.h"

bool Scene::loadRoom(const adm::Database& db, int32_t roomId, std::string* error) {
    db_ = &db;
    objects_.clear();
    const adm::Node* room = db.findModule(roomId);
    if (!room) {
        *error = "room module not found";
        return false;
    }
    for (const adm::Node& list : room->children) {
        if (list.tag != adm::Tag::List || list.ref != sym::Place) continue;
        const std::vector<int32_t> v = adm::intValues(list);
        if (v.size() < 4) continue;
        SceneObject obj;
        obj.moduleId = v[0];
        obj.module = db.findModule(v[0]);
        obj.x = v[1];
        obj.y = v[2];
        obj.z = v[3];
        if (!obj.module) continue;
        // Activation (1000:5f02): top-level property lists run before the first state.
        for (const adm::Node& c : obj.module->children) {
            if (c.tag == adm::Tag::Map) break;
            if (c.tag == adm::Tag::List && c.ref == sym::SetProperty) {
                const std::vector<int32_t> p = adm::intValues(c);
                if (p.size() >= 2 && p[0] == sym::Z) obj.z = p[1];
            }
        }
        enterState(obj, sym::StateInitial);
        objects_.push_back(std::move(obj));
    }
    // Stable: equal depths keep the room's listing order.
    std::stable_sort(objects_.begin(), objects_.end(),
                     [](const SceneObject& a, const SceneObject& b) { return a.z < b.z; });
    return true;
}

void Scene::enterState(SceneObject& obj, int32_t stateId) {
    obj.state = adm::childModule(*obj.module, stateId);
    obj.image = nullptr;
    obj.frame = 0;
    if (!obj.state) return;
    for (const adm::Node& list : obj.state->children) {
        if (list.tag != adm::Tag::List) continue;
        if (list.ref == sym::Image && !obj.image) {
            if (const adm::FileRef* f = adm::fileValue(list)) obj.image = image(*f);
        } else if (list.ref == sym::SetProperty) {
            const std::vector<int32_t> p = adm::intValues(list);
            if (p.size() >= 2 && p[0] == sym::Z) obj.z = p[1];
        }
    }
}

const Flc* Scene::image(const adm::FileRef& file) {
    auto it = images_.find(file.offset);
    if (it != images_.end()) return it->second.get();
    auto flc = std::make_unique<Flc>();
    if (!flc->open(db_->bytes(file)) || !flc->decodeAll() || flc->frames().empty()) flc.reset();
    return images_.emplace(file.offset, std::move(flc)).first->second.get();
}

void Scene::draw(Surface& target) const {
    for (const SceneObject& obj : objects_) {
        if (!obj.image) continue;
        const Flc& f = *obj.image;
        target.blit(f.frames()[obj.frame].data(), f.width(), f.height(), obj.x - f.width() / 2,
                    obj.y - f.height());
    }
}
