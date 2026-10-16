#include "world.h"

#include <algorithm>
#include <cstdio>

#include "classes.h"
#include "symbols.h"

namespace {
constexpr int32_t kConfigModule = -42, kDefaultCursor = -59;
}

bool World::load(const adm::Database& db, int32_t roomId, std::string* error) {
    db_ = &db;
    objects_.clear();
    if (const adm::Node* cfg = db.findModule(kConfigModule)) {
        for (const adm::Node& c : cfg->children) {
            const std::vector<int32_t> v = c.tag == adm::Tag::List ? adm::intValues(c) : std::vector<int32_t>{};
            if (v.empty()) continue;
            switch (c.id) {
            case -44: config_.floorMaxY = v[0]; break;
            case -45: config_.floorMinY = v[0]; break;
            case -46: config_.config46 = v[0]; break;
            case -47: config_.gravity = v[0]; break;
            case -49: config_.frameScale = std::max(1, v[0]); break;
            }
        }
    }
    const adm::Node* room = db.findModule(roomId);
    if (!room) {
        *error = "room module not found";
        return false;
    }
    for (const adm::Node& list : room->children) {
        if (list.tag != adm::Tag::List) continue;
        const std::vector<int32_t> v = adm::intValues(list);
        if (list.ref == sym::Place && v.size() >= 4) {
            spawn(v[0], Vec3(v[1], v[2], v[3]), false);
        } else if (list.ref == sym::RoomSetProperty && v.size() >= 3) {
            if (GameObject* o = find(v[0])) o->setProperty(v[1], v[2]);
        }
    }
    return true;
}

std::unique_ptr<GameObject> World::makeObject(int32_t moduleId, const adm::Node* module) {
    // CreateObjectForModule (1000:5604): the module's class symbol picks the C++ class.
    return createObjectForClass(module->ref, *this, moduleId, module);
}

GameObject* World::spawn(int32_t moduleId, const Vec3& pos, bool facing) {
    const adm::Node* module = db_->findModule(moduleId);
    if (!module) return nullptr;
    std::unique_ptr<GameObject> obj = makeObject(moduleId, module);
    if (!obj->create(pos, facing)) return nullptr;
    GameObject* raw = obj.get();
    objects_.push_back(std::move(obj));
    raw->activate();
    return raw;
}

GameObject* World::find(int32_t id) const {
    for (const auto& o : objects_) {
        if (o->id() == id) return o.get();
    }
    return nullptr;
}

int World::count(int32_t id) const {
    int n = 0;
    for (const auto& o : objects_) n += o->id() == id;
    return n;
}

void World::remove(GameObject* obj) {
    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        if (it->get() == obj) {
            if (focus_ == obj) focus_ = nullptr;
            for (auto& o : objects_) {
                if (o->parent() == obj) o->setParent(nullptr);
            }
            removed_.push_back(std::move(*it));  // freed after the tick: callers may still be running
            objects_.erase(it);
            return;
        }
    }
}

void World::send(int32_t target, int32_t message) {
    if (GameObject* o = find(target)) o->sendMessage(message, nullptr);
}

void World::sortByDepth() {  // 1000:07c6: front (highest z) first
    std::stable_sort(objects_.begin(), objects_.end(),
                     [](const auto& a, const auto& b) { return a->position().z > b->position().z; });
}

template <typename F>
void World::broadcast(F&& f) {
    cursor_ = kDefaultCursor;
    focus_ = nullptr;
    // Index loop: handlers may spawn objects (appended) or remove them (deferred).
    std::vector<GameObject*> order;
    for (auto& o : objects_) order.push_back(o.get());
    for (GameObject* o : order) {
        if (std::find_if(objects_.begin(), objects_.end(), [&](auto& p) { return p.get() == o; }) != objects_.end()) f(*o);
    }
}

void World::tick(uint32_t nowMs) {
    now_ = nowMs;
    sortByDepth();
    std::vector<GameObject*> order;
    for (auto& o : objects_) order.push_back(o.get());
    for (GameObject* o : order) {
        if (std::find_if(objects_.begin(), objects_.end(), [&](auto& p) { return p.get() == o; }) != objects_.end()) {
            o->step(nowMs);
        }
    }
    if (mouse_.x >= 0) broadcast([&](GameObject& o) { o.onHover(mouse_); });
    for (auto& o : objects_) o->retireRunners();
    removed_.clear();
}

void World::mouseMove(int x, int y) {
    mouse_ = {x, y};
    broadcast([&](GameObject& o) { o.onMouseMove(mouse_); });
}

void World::buttonDown(int x, int y) {
    mouse_ = {x, y};
    buttonDown_ = true;
    broadcast([&](GameObject& o) { o.onButtonDown(mouse_); });
}

void World::buttonUp(int x, int y) {
    mouse_ = {x, y};
    buttonDown_ = false;
    broadcast([&](GameObject& o) { o.onButtonUp(mouse_); });
}

void World::mouseLeave() {
    mouse_ = {-1, -1};
    cursor_ = kDefaultCursor;
}

void World::draw(Surface& target) const {
    std::vector<const GameObject*> order;
    for (const auto& o : objects_) {
        if (o->shownSprite() && o->shownSprite()->flc) order.push_back(o.get());
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const GameObject* a, const GameObject* b) { return a->position().z < b->position().z; });
    for (const GameObject* o : order) {
        const Sprite& s = *o->shownSprite();
        const Flc& f = *s.image(o->facing());
        const int frame = std::clamp(s.frame, 0, f.frameCount() - 1);
        Vec3 off = s.offset;
        if (o->facing()) off.x = -off.x;
        const Vec3 p = o->position() + off;
        target.blit(f.frames()[size_t(frame)].data(), f.width(), f.height(), p.x - f.width() / 2, p.y - f.height(), 0);
    }
}

const Flc* World::image(const adm::FileRef& file) {
    auto it = images_.find(file.offset);
    if (it != images_.end()) return it->second.get();
    auto flc = std::make_unique<Flc>();
    if (!flc->open(db_->bytes(file)) || !flc->decodeAll() || flc->frames().empty()) flc.reset();
    return images_.emplace(file.offset, std::move(flc)).first->second.get();
}

const Sound* World::sound(const adm::FileRef& file) {
    auto it = sounds_.find(file.offset);
    if (it != sounds_.end()) return it->second.get();
    auto s = std::make_unique<Sound>();
    if (!loadWav(db_->bytes(file), s.get())) s.reset();
    return sounds_.emplace(file.offset, std::move(s)).first->second.get();
}

std::string World::describe() const {
    std::string out;
    char line[200];
    for (const auto& o : objects_) {
        const Sprite* s = o->shownSprite();
        std::snprintf(line, sizeof line, "%5d class=%5d state=%5d pos=(%d,%d,%d) %s frame=%d%s%s%s\n", o->id(),
                      o->module()->ref, o->state(), o->position().x, o->position().y, o->position().z,
                      s ? s->name.c_str() : "-", s ? s->frame : 0, o->paused() ? " paused" : "",
                      o->runner() && o->runner()->done ? " done" : "", o->facing() ? " facing" : "");
        out += line;
    }
    return out;
}
