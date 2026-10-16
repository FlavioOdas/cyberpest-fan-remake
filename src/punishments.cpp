#include "punishments.h"

#include <cstdlib>

#include "world.h"

namespace {

constexpr int32_t kPest = -105, kPestBusy = -423;

// The pest, when present and not busy with something it can't be interrupted in.
GameObject* availablePest(World& world) {
    GameObject* pest = world.find(kPest);
    return pest && !pest->property(kPestBusy) ? pest : nullptr;
}

// Screen rectangle of an object's current sprite, anchored bottom-centre (1038:19b4 + 1b28).
struct Rect {
    int left, top, right, bottom;
    bool contains(int x, int y) const { return x >= left && x < right && y >= top && y < bottom; }  // 1038:1aac
};

Rect spriteRect(const GameObject& o) {
    const Point2 size = o.spriteSize();
    const int left = o.position().x - size.x / 2, top = o.position().y - size.y;
    return {left, top, left + size.x, top + size.y};
}

bool onFloor(const World& world, int z) {
    return z > 2999 && z <= world.config().floorMaxY + 3000 - world.config().floorMinY;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Punishment display (-92)

bool PunishmentDisplay::create(const Vec3& pos, bool facing) {  // 1008:eaac
    if (!GameObject::create(pos, facing)) return false;
    const adm::Node* list = adm::childModule(*module_, -529);
    if (!list) return false;
    choices_.clear();
    for (const adm::Node& l : list->children) {
        if (l.tag != adm::Tag::List) continue;
        const std::vector<int32_t> v = adm::intValues(l);
        if (v.empty()) return false;
        choices_.push_back(v[0]);
    }
    index_ = 0;
    return !choices_.empty();
}

void PunishmentDisplay::onEvent(GameObject* from, int32_t value, int type) {  // 1008:ebce
    if (type == 0 && value == -537 && !choices_.empty()) {  // next picture
        bool inUse = false;
        for (int32_t id : choices_) inUse = inUse || world_.find(id);
        if (inUse) {
            enterState(-538);  // a punishment is already out
        } else {
            index_ = (index_ + 1) % int(choices_.size());
            enterState(choices_[size_t(index_)]);
        }
    } else if (type == 0 && value == -539 && !choices_.empty()) {  // take the shown punishment
        const int32_t id = choices_[size_t(index_)];
        GameObject* obj = world_.spawn(id, position() + Vec3(0, 0, 1), facing());
        if (!obj) return;
        const Point2 mouse = world_.mouse();
        obj->setPosition(Vec3(mouse.x, mouse.y + obj->spriteSize().y / 2, 3000));
        obj->setProperty(-182, 1);  // held by the pointer
        obj->setProperty(-322, 1);  // shown (1038:2ab2)
    }
    GameObject::onEvent(from, value, type);
}

// ---------------------------------------------------------------------------------------------
// 16-ton weight (-531)

bool Weight::create(const Vec3& pos, bool facing) {  // 1010:0000
    if (!GameObject::create(pos, facing)) return false;
    wasAbove_ = squashing_ = false;
    moduleInt(-546, 0, &restHeight_);
    return true;
}

bool Weight::aboveHead() const {  // 1010:05d4: above the top of the pest's sprite
    const GameObject* pest = world_.find(kPest);
    return pest && position().y < pest->position().y - pest->spriteSize().y;
}

void Weight::onFrame() {  // 1010:0144
    if (held()) {
        // Hovering over the pest: draw in front of it.
        GameObject* pest = position().y < world_.config().floorMinY && aboveHead() ? availablePest(world_) : nullptr;
        if (pest && onFloor(world_, pest->position().z)) setPosition(Vec3(position().x, position().y, pest->position().z + 1));
    } else if (property(-309)) {  // falling
        if (wasAbove_ && !aboveHead()) {
            if (GameObject* pest = availablePest(world_)) {
                const int halfWidth = spriteSize().x / 2;
                const Vec3& p = pest->position();
                if (position().x - halfWidth < p.x && p.x < position().x + halfWidth && onFloor(world_, p.z)) {
                    setPosition(Vec3(position().x, p.y - restHeight_, p.z + 1));
                    setProperty(-309, 0);
                    pest->sendMessage(-293, this);  // squashed
                    squashing_ = true;
                }
            }
        }
        wasAbove_ = aboveHead();
    }
    GameObject::onFrame();
}

void Weight::onEvent(GameObject* from, int32_t value, int type) {  // 1010:0080
    if (type == 0 && value == -184) {  // dropped
        wasAbove_ = aboveHead();
    } else if (type == 0 && value == -182 && squashing_) {  // picked up off the pest
        if (GameObject* pest = world_.find(kPest)) {
            squashing_ = false;
            pest->sendMessage(-453, this);
        }
    }
    GameObject::onEvent(from, value, type);
}

// ---------------------------------------------------------------------------------------------
// Bomb (-534)

void Bomb::onEvent(GameObject* from, int32_t value, int type) {  // 1010:07ea
    if (type == 0 && value == -226) {  // exploding
        if (GameObject* pest = world_.find(kPest)) {
            // The whole pest must be inside the explosion (1038:1aac on both pest corners).
            const Rect pestRect = spriteRect(*pest), blast = spriteRect(*this);
            if (blast.contains(pestRect.left, pestRect.top) && blast.contains(pestRect.right, pestRect.bottom)) {
                pest->sendMessage(-26, this);  // blown up
            }
        }
    }
    GameObject::onEvent(from, value, type);
}

// ---------------------------------------------------------------------------------------------
// Cannonball (-550)

bool Cannonball::create(const Vec3& pos, bool facing) {  // 1010:0b8e
    if (!GameObject::create(pos, facing)) return false;
    moduleInt(-551, 0, &depthTolerance_);
    moduleInt(-552, 0, &hitOffset_.x);
    moduleInt(-552, 1, &hitOffset_.y);
    moduleInt(-552, 2, &hitOffset_.z);
    const GameObject* pest = world_.find(kPest);
    leftOfPest_ = pest && position().x < pest->position().x;
    return true;
}

void Cannonball::onFrame() {  // 1010:0d10
    if (leftOfPest_) {
        if (GameObject* pest = world_.find(kPest)) {
            leftOfPest_ = position().x < pest->position().x;
            if (!leftOfPest_) {  // just passed the pest: did it hit?
                const Vec3& p = pest->position();
                const Vec3& me = position();
                if (p.y - pest->spriteSize().y < me.y && me.y - spriteSize().y < p.y &&
                    std::abs(me.z - p.z) <= depthTolerance_) {
                    setPosition(Vec3(p.x, me.y, me.z) + hitOffset_);
                    enterState(-553);
                }
            }
        }
    }
    GameObject::onFrame();
}

// ---------------------------------------------------------------------------------------------
// Cannon (-172)

void Cannon::onEvent(GameObject* from, int32_t value, int type) {  // 1010:6986
    if (type == 3 && value == -555) setProperty(-178, world_.mouse().x);  // aim: follow the pointer
    if (type == 0 && value == -556) {  // fire
        if (GameObject* pest = world_.find(kPest)) {
            const Point2 mouse = world_.mouse();
            Vec3 at(mouse.x, mouse.y, 3999);
            const bool hit = spriteRect(*pest).contains(at.x, at.y);
            GameObject* shot = world_.spawn(hit ? -534 : -557, at, false);
            if (shot) {
                if (hit) shot->sendMessage(-446, this);
                at.y += shot->spriteSize().y / 2;
                shot->setPosition(at);
            }
        }
    }
    GameObject::onEvent(from, value, type);
}

// ---------------------------------------------------------------------------------------------
// Flea powder (-591)

bool Powder::abovePest() const {  // 1010:3d0a
    const GameObject* pest = world_.find(kPest);
    if (!pest || position().y >= pest->position().y) return false;
    const int half = pest->spriteSize().x / 2;
    return pest->position().x - half < position().x && position().x < pest->position().x + half;
}

void Powder::onFrame() {  // 1010:3c0a
    if (held()) {
        if (!abovePest()) {
            if (state() == -592) setLoop(mainSprite(), false, 0);
        } else if (availablePest(world_)) {
            if (state() == -592) setLoop(mainSprite(), true, 0);
            else enterState(-592);  // sprinkling (sends -448 to the pest)
        }
    }
    GameObject::onFrame();
}
