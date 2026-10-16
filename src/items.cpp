#include "items.h"

#include <algorithm>
#include <cstdlib>

#include "world.h"

bool ShelfItem::create(const Vec3& pos, bool facing) {  // 1008:6ee6
    if (!GameObject::create(pos, facing)) return false;
    shelves_.clear();
    if (const adm::Node* shelfModule = adm::childModule(*module_, -275)) {
        for (const adm::Node& list : shelfModule->children) {
            if (list.tag != adm::Tag::List) continue;
            const std::vector<int32_t> values = adm::intValues(list);
            if (values.size() >= 4) shelves_.push_back({values[0], values[1], values[2], values[3]});
        }
    }
    moduleInt(-276, 0, &minSeconds_);
    moduleInt(-276, 1, &maxSeconds_);
    moduleInt(-277, 0, &maxHeight_);
    timerRunning_ = false;
    return true;
}

void ShelfItem::startTimer() {  // 1008:735e
    setProperty(-17, 1);
    timerRunning_ = true;
    timerStart_ = world_.now();
    const int range = std::max(0, maxSeconds_ - minSeconds_);
    timerDuration_ = uint32_t(std::max(0, minSeconds_ + (range ? std::rand() % (range + 1) : 0))) * 1000;
}

void ShelfItem::expireTimer() {  // 1008:73d4
    if (timerRunning_ && (world_.now() > timerStart_ + timerDuration_ || world_.now() < timerStart_)) {
        setProperty(-17, 0);
        timerRunning_ = false;
    }
}

void ShelfItem::onLastFrame(Sprite&) {  // 1008:746e
    expireTimer();
    cueSend(-105, -15);
}

void ShelfItem::onFrame() {  // 1008:74d0
    if (held()) setProperty(-82, 0);
    GameObject::onFrame();
}

void ShelfItem::onEvent(GameObject* from, int32_t value, int type) {  // 1008:70d4
    if (type == 0) {
        switch (value) {
        case -450:
        case -449:
            setProperty(-82, 0);
            cueSend(-105, -16);
            break;
        case -308:
            startTimer();
            cueSend(-105, -15);
            break;
        case -297:
            if (heightAboveFloor() > maxHeight_) return;
            break;
        case -184:
            if (property(-22)) {
                for (const ShelfRect& shelf : shelves_) {
                    const Vec3& pos = position();
                    if (pos.x >= shelf.left && pos.x < shelf.right && pos.y >= shelf.top && pos.y < shelf.bottom) {
                        setPosition({pos.x, shelf.bottom - 1, 3000});
                        setProperty(-309, 0);
                        setProperty(-82, 1);
                        cueSend(-105, -15);
                        break;
                    }
                }
            }
            break;
        case -182:
            if (property(-82)) {
                setProperty(-110, 3001);
                setProperty(-82, 0);
            }
            break;
        case -41:
            cueSend(-105, -14);
            break;
        default:
            break;
        }
    }
    GameObject::onEvent(from, value, type);
}

bool ShelfCarryItem::create(const Vec3& pos, bool facing) {  // 1008:7f16
    if (!ShelfItem::create(pos, facing)) return false;
    moduleInt(-524, 0, &home_.x);
    moduleInt(-524, 1, &home_.y);
    moduleInt(-524, 2, &home_.z);
    if (home_.x == 0) {
        int minX = 0, maxX = 0;
        moduleInt(-526, 0, &minX);
        moduleInt(-526, 1, &maxX);
        const int range = std::max(0, maxX - minX);
        home_.x = minX + (range ? std::rand() % (range + 1) : 0);
    }
    setPosition(home_);
    return true;
}

void ShelfCarryItem::onMouseMove(Point2 point) {  // 1008:819a
    if (held() && property(-525)) {
        const int dragY = point.y - grabOffset_.y;
        if (dragY > world_.config().floorMaxY && state() != -523) {
            dragOrigin_ = home_;
            setProperty(-110, 5000);
            enterState(-523);
        } else if (dragY <= world_.config().floorMaxY && state() == -523) {
            dragOrigin_ = {position().x, yFromDepth(3000), 3000};
            enterState(-108);
        }
    }
    ShelfItem::onMouseMove(point);
}

void ShelfCarryItem::onEvent(GameObject* from, int32_t value, int type) {  // 1008:805a
    if (type == 0 && value != -288) {
        if (value == -184 && state() == -523) {
            setPosition(home_);
        } else if (value == -182 && state() == -523) {
            setProperty(-110, 5000);
        } else if (value == -1) {
            setPosition(home_);
            enterState(-1);
            return;
        } else if (value == -501) {
            if (GameObject* pest = world_.find(-105)) setPosition(pest->position());
        }
    }
    ShelfItem::onEvent(from, value, type);
}

void ShelfWrapperItem::onStateStart() {  // 1010:2c1c
    if (state() == -559 && world_.anyOtherWithProperty(this, -558)) cueRemove(-204);
}
