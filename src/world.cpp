#include "world.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "symbols.h"

namespace {

// Command symbols handled by the runtime (see docs/re/state-commands.md).
enum : int32_t {
    kSet = -2, kSetAt = -3, kRemove = -4, kRemoveAt = -5, kSpawn = -6, kSpawnAt = -7,
    kResume = -8, kResumeAt = -9, kPause = -10, kPauseAt = -11, kOnClick = -12, kOnEvent2 = -13,
    kOnEvent3 = -170, kOnMessage = -121, kIntro = -306, kMain = -111, kOutro = -431,
    kHoldLast = -112, kLoop = -183, kHitArea = -114, kSound = -116, kSend = -119, kSendAt = -176,
    kNext = -132, kRandomNext = -151,
    kTargetSelf = -204, kOwnPosition = -594,
};

Command toCommand(const adm::Node& list) {
    Command c;
    c.symbol = list.ref;
    c.ints = adm::intValues(list);
    c.file = adm::fileValue(list);
    return c;
}

int arg(const Command& c, size_t i, int fallback = 0) { return i < c.ints.size() ? c.ints[i] : fallback; }

}  // namespace

bool World::load(const adm::Database& db, int32_t roomId, int frameScale, std::string* error) {
    db_ = &db;
    frameScale_ = std::max(1, frameScale);
    objects_.clear();
    const adm::Node* room = db.findModule(roomId);
    if (!room) {
        *error = "room module not found";
        return false;
    }
    for (const adm::Node& list : room->children) {
        if (list.tag != adm::Tag::List) continue;
        const std::vector<int32_t> v = adm::intValues(list);
        if (list.ref == sym::Place && v.size() >= 4) {
            spawn(v[0], v[1], v[2], v[3], 0);
        } else if (list.ref == sym::RoomSetProperty && v.size() >= 3) {
            if (Object* o = find(v[0])) o->properties[v[1]] = v[2];
        }
    }
    return true;
}

Object* World::find(int32_t id) {
    for (auto& o : objects_) {
        if (o->id == id) return o.get();
    }
    return nullptr;
}

Object* World::spawn(int32_t moduleId, int x, int y, int z, uint32_t nowMs) {
    const adm::Node* module = db_->findModule(moduleId);
    if (!module) return nullptr;
    auto obj = std::make_unique<Object>();
    obj->id = moduleId;
    obj->module = module;
    obj->x = x;
    obj->y = y;
    obj->z = z;
    obj->lastStep = nowMs;
    // Object_Activate (1000:5f02): top-level property lists, then the first child whose ref is
    // the state class (-109); other sub-modules are class data (e.g. shelf rectangles -275).
    int32_t firstState = 0;
    for (const adm::Node& c : module->children) {
        if (c.tag == adm::Tag::Map && c.ref == sym::ClassState) {
            firstState = c.id;
            break;
        }
        if (c.tag == adm::Tag::List && c.id == kOwnPosition) {
            // Certificate class (-593, create 1010:47c6) positions itself from list -594 [inf].
            const std::vector<int32_t> p = adm::intValues(c);
            if (p.size() >= 3) {
                obj->x = p[0];
                obj->y = p[1];
                obj->z = p[2];
            }
        }
        if (c.tag == adm::Tag::List && c.ref == kSet) {
            const std::vector<int32_t> p = adm::intValues(c);
            if (p.size() < 2) continue;
            if (p[0] == sym::Z) obj->z = p[1];
            else if (p[0] == sym::X) obj->x = p[1];
            else if (p[0] == sym::Y) obj->y = p[1];
            else obj->properties[p[0]] = p[1];
        }
    }
    Object& ref = *obj;
    objects_.push_back(std::move(obj));
    if (firstState) enterState(ref, firstState, nowMs);
    return &ref;
}

void World::remove(int32_t id) { removed_.push_back(id); }

void World::enterState(Object& obj, int32_t state, uint32_t nowMs, int depth) {
    const adm::Node* module = adm::childModule(*obj.module, state);
    if (!module || depth > 16) return;  // depth: states without sprites can chain
    auto runner = std::make_unique<Runner>();
    Runner& r = *runner;
    r.state = state;
    if (obj.runner) obj.retired.push_back(std::move(obj.runner));
    obj.runner = std::move(runner);
    int slot = -1;  // sprite the following commands attach to
    for (const adm::Node& list : module->children) {
        if (list.tag != adm::Tag::List) continue;
        Command c = toCommand(list);
        Sprite* s = slot >= 0 ? &r.sprites[slot] : nullptr;
        switch (c.symbol) {
        case kIntro:
        case kMain:
        case kOutro:
            slot = c.symbol == kIntro ? 0 : c.symbol == kMain ? 1 : 2;
            r.sprites[slot] = Sprite{};
            if (c.file) {
                r.sprites[slot].flc = image(*c.file);
                r.sprites[slot].name = c.file->name;
            }
            break;
        case kHoldLast:
            if (s) s->holdLastFrame = arg(c, 0) != 0;
            break;
        case kLoop:
            if (s) {
                s->loop = true;
                s->loopCount = arg(c, 0);
            }
            break;
        case kSound:
            if (s && c.file) s->sound = sound(*c.file);
            break;
        case kHitArea:
            r.hitAreas.push_back({arg(c, 0), arg(c, 1), arg(c, 2), arg(c, 3), arg(c, 4), arg(c, 5), arg(c, 6)});
            break;
        case kOnMessage:
            r.events.push_back({0, arg(c, 0), arg(c, 1), arg(c, 2) != 0, true});
            break;
        case kOnClick:
        case kOnEvent2:
        case kOnEvent3:
            r.events.push_back({c.symbol == kOnClick ? 1 : c.symbol == kOnEvent2 ? 2 : 3, arg(c, 0), arg(c, 1),
                                arg(c, 2) != 0, arg(c, 3, 1) != 0});
            break;
        case kNext:
            r.nextState = arg(c, 0);
            break;
        case kRandomNext:
            if (!c.ints.empty()) r.nextState = c.ints[size_t(std::rand()) % c.ints.size()];
            break;
        case kSetAt:
        case kRemoveAt:
        case kSpawnAt:
        case kResumeAt:
        case kPauseAt:
        case kSendAt: {
            // Timed form: first value is the time in high-spec frames.
            if (!s || c.ints.empty()) break;
            const int frame = c.ints.front() / frameScale_;
            c.ints.erase(c.ints.begin());
            c.symbol = c.symbol == kSetAt ? kSet : c.symbol == kRemoveAt ? kRemove : c.symbol == kSpawnAt ? kSpawn
                     : c.symbol == kResumeAt ? kResume : c.symbol == kPauseAt ? kPause : kSend;
            s->cues.emplace_back(frame, std::move(c));
            break;
        }
        case kSet:
        case kRemove:
        case kSpawn:
        case kResume:
        case kPause:
        case kSend:
            if (s) s->cues.emplace_back(-1, std::move(c));  // after a sprite: run when it ends
            else runCommand(obj, r, c, nowMs);
            break;
        default:
            break;  // not yet ported: motion (-326/-455/-493/-548), timers (-157), …
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (r.sprites[i].flc) {
            startSprite(obj, r, i);
            break;
        }
    }
    if (r.current < 0) {
        r.done = true;
        if (r.nextState) enterState(obj, r.nextState, nowMs, depth + 1);
    }
}

void World::startSprite(Object& obj, Runner& r, int index) {
    r.current = index;
    Sprite& s = r.sprites[index];
    s.frame = -1;
    s.finished = false;
    obj.shown = s.flc;
    obj.shownName = s.name;
    obj.shownFrame = 0;
}

void World::runCommand(Object& obj, Runner&, const Command& c, uint32_t nowMs) {
    auto target = [&](int32_t id) -> Object* { return id == kTargetSelf ? &obj : find(id); };
    switch (c.symbol) {
    case kSet:
        if (c.ints.size() < 2) break;
        if (c.ints[0] == sym::Z) obj.z = c.ints[1];
        else if (c.ints[0] == sym::X) obj.x = c.ints[1];
        else if (c.ints[0] == sym::Y) obj.y = c.ints[1];
        else obj.properties[c.ints[0]] = c.ints[1];
        break;
    case kRemove:
        if (Object* t = target(arg(c, 0))) {
            sendEvent(*t, 0, -41, nowMs);
            remove(t->id);
        }
        break;
    case kSpawn:
        // Position relative to the spawning object (1000:6aa0) [inf: offsets and z are relative].
        if (!find(arg(c, 0))) spawn(arg(c, 0), obj.x + arg(c, 1), obj.y + arg(c, 2), obj.z + arg(c, 3), nowMs);
        break;
    case kResume:
        if (Object* t = target(arg(c, 0))) t->paused = false;
        break;
    case kPause:
        if (Object* t = target(arg(c, 0))) t->paused = true;
        break;
    case kSend:
        if (Object* t = target(arg(c, 0))) sendEvent(*t, 0, arg(c, 1), nowMs);
        break;
    default:
        break;
    }
}

void World::sendEvent(Object& obj, int type, int32_t value, uint32_t nowMs) {
    if (!obj.runner) return;
    Runner& r = *obj.runner;
    for (const Event& e : r.events) {
        if (e.type != type || e.value != value) continue;
        if (!e.immediate && !r.done && r.current >= 0) {
            // Runner_FireEvent (1000:b44c): let the current animation finish, then switch.
            r.pending = true;
            r.nextState = e.state;
            r.sprites[r.current].loop = false;
        } else {
            enterState(obj, e.state, nowMs);
        }
        return;
    }
}

void World::runnerFinished(Object& obj, uint32_t nowMs) {
    Runner& r = *obj.runner;
    r.done = true;
    if (r.nextState) enterState(obj, r.nextState, nowMs);
}

void World::step(Object& obj, uint32_t nowMs) {
    if (obj.paused || !obj.runner) return;
    Runner& r = *obj.runner;
    if (r.current < 0 || r.done) return;
    Sprite& s = r.sprites[r.current];
    const uint32_t delay = uint32_t(std::max(s.flc->frameDelayMs(), 20));
    if (nowMs - obj.lastStep < delay) return;
    obj.lastStep = nowMs;

    if (++s.frame == 0 && s.sound && playSound_) playSound_(s.sound);
    obj.shown = s.flc;
    obj.shownFrame = std::min(s.frame, s.flc->frameCount() - 1);
    for (auto& [frame, cmd] : s.cues) {
        if (frame == s.frame) runCommand(obj, r, cmd, nowMs);
    }
    if (obj.runner.get() != &r) return;  // a cue switched state
    if (s.frame < s.flc->frameCount() - 1) return;

    if (s.loop && (s.loopCount == 0 || ++s.loopsDone < s.loopCount)) {
        s.frame = -1;  // Sprite_OnFrame: restart
        return;
    }
    for (auto& [frame, cmd] : s.cues) {
        if (frame == -1 || frame == s.flc->frameCount()) runCommand(obj, r, cmd, nowMs);
    }
    if (obj.runner.get() != &r) return;
    s.finished = true;
    // Runner_OnSpriteFinished (1000:a4f2): intro -> main -> outro, then the runner is done.
    for (int next = r.current + 1; next < 3; ++next) {
        if (r.sprites[next].flc) {
            startSprite(obj, r, next);
            return;
        }
    }
    runnerFinished(obj, nowMs);
}

void World::tick(uint32_t nowMs) {
    for (size_t i = 0; i < objects_.size(); ++i) step(*objects_[i], nowMs);  // index: spawns may append
    for (auto& o : objects_) o->retired.clear();
    for (int32_t id : removed_) {
        objects_.erase(std::remove_if(objects_.begin(), objects_.end(), [&](auto& o) { return o->id == id; }),
                       objects_.end());
    }
    removed_.clear();
}

void World::draw(Surface& target) const {
    std::vector<const Object*> order;
    for (const auto& o : objects_) {
        if (o->shown) order.push_back(o.get());
    }
    std::stable_sort(order.begin(), order.end(), [](const Object* a, const Object* b) { return a->z < b->z; });
    for (const Object* o : order) {
        const Flc& f = *o->shown;
        target.blit(f.frames()[o->shownFrame].data(), f.width(), f.height(), o->x - f.width() / 2, o->y - f.height());
    }
}

bool World::hitTest(const Object& obj, int x, int y, const HitArea** area) const {
    if (!obj.runner || !obj.shown) return false;
    const int left = obj.x - obj.shown->width() / 2, top = obj.y - obj.shown->height();
    for (const HitArea& h : obj.runner->hitAreas) {
        if (x >= left + h.x1 && x < left + h.x2 && y >= top + h.y1 && y < top + h.y2) {
            *area = &h;
            return true;
        }
    }
    return false;
}

int32_t World::hover(int x, int y) const {
    const Object* best = nullptr;
    const HitArea* bestArea = nullptr;
    for (const auto& o : objects_) {
        const HitArea* a;
        if (hitTest(*o, x, y, &a) && (!best || o->z >= best->z)) {
            best = o.get();
            bestArea = a;
        }
    }
    return bestArea ? bestArea->cursor : 0;
}

void World::click(int x, int y, uint32_t nowMs) {
    Object* best = nullptr;
    const HitArea* bestArea = nullptr;
    for (auto& o : objects_) {
        const HitArea* a;
        if (hitTest(*o, x, y, &a) && (!best || o->z >= best->z)) {
            best = o.get();
            bestArea = a;
        }
    }
    if (best) sendEvent(*best, 1, bestArea->id, nowMs);
}

std::string World::describe() const {
    std::string out;
    char line[160];
    for (const auto& o : objects_) {
        std::snprintf(line, sizeof line, "%5d state=%5d pos=(%d,%d) z=%d %s frame=%d%s%s\n", o->id,
                      o->runner ? o->runner->state : 0, o->x, o->y, o->z, o->shownName.c_str(), o->shownFrame,
                      o->paused ? " paused" : "", o->runner && o->runner->done ? " done" : "");
        out += line;
    }
    return out;
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
