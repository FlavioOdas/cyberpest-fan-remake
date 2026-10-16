// The pest (class -105). Each method is a transliteration of the original function named in
// its comment (segment 2 unless stated); docs/re/pest.md explains the fields and flows.
#include "pest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "symbols.h"
#include "world.h"

namespace {

// States and symbols used by the pest (values from the code; names where the data shows them).
enum : int32_t {
    kStDecide = -324, kStInactive = -422, kStTurnLeft = -459, kStTurnRight = -460,
    kStWalkStraight = -496, kStGetDown = -495, kStWrestle = -504, kStFlee = -505, kStJumpOff = -447,
    kStFetch = -464, kStWalkOff = -455, kStRunOff = -359, kStPeep = -347, kStPeekA = -482, kStPeekB = -480,
    kStSnatch = -497, kStHome = -499, kStGrab = -497, kStCommandFail = -505,
    kStDraggedOff = -446, kStFallBack = -455,
    kProfileTable = -337, kTableIdle = -346, kTableWalk = -353, kTableReaction = -362, kTableReaction2 = -370,
    kTableReaction3 = -375, kPrefActivities = -377, kPrefReactions = -379, kBirthPersonality = -380,
    kTv = -72, kMouse = -91, kFoodFlap = -74, kPhone = -98, kFlea = -518, kMouseVisitor = -314,
};

int rnd(int lo, int hi) {  // Random 1038:5546, inclusive [inf]
    if (hi < lo) std::swap(lo, hi);
    return lo + std::rand() % (hi - lo + 1);
}

long wallSeconds() { return long(std::time(nullptr)); }

}  // namespace

long Pest::nowMs() const { return long(world_.now()); }
bool Pest::clicked() const { return userClicked_ != 0; }

// ---------------------------------------------------------------------------------------------
// Creation

bool Pest::create(const Vec3& pos, bool facing) {  // 0000
    if (!GameObject::create(pos, facing)) return false;
    auto read = [&](int32_t list, int index, int* field) { moduleInt(list, index, field); };
    read(-381, 0, &directWalkPercent_);
    read(-382, 0, &idleMin_), read(-382, 1, &idleMax_);
    read(-383, 0, &idleShortMin_), read(-383, 1, &idleShortMax_);
    read(-384, 0, &t384a_), read(-384, 1, &t384b_);
    read(-385, 0, &walkTimeMin_), read(-385, 1, &walkTimeMax_);
    read(-386, 0, &peepTimeMin_), read(-386, 1, &peepTimeMax_);
    read(-388, 0, &peepLoopsMin_), read(-388, 1, &peepLoopsMax_);
    read(-389, 0, &peekLoopsMin_), read(-389, 1, &peekLoopsMax_);
    read(-387, 0, &offLeftX_), read(-387, 2, &offRightX_), read(-387, 1, &centreX_);
    read(-390, 1, &leftEdgeB_), read(-390, 0, &leftEdgeA_);
    read(-391, 0, &rightEdgeA_), read(-391, 1, &rightEdgeB_);
    read(-392, 0, &floorLeftX_), read(-392, 1, &floorRightX_);
    read(-393, 0, &screenLeftX_), read(-393, 1, &screenRightX_);
    read(-394, 0, &farLeftX_), read(-395, 0, &farRightX_);
    read(-396, 0, &t396_), read(-397, 0, &maxPoos_), read(-398, 0, &t398_), read(-399, 0, &maxMess_);
    read(-400, 0, &t400_), read(-401, 0, &turnDistance2_), read(-402, 0, &turnDistance1_);
    read(-403, 0, &reachX_), read(-404, 0, &jitterX_), read(-405, 0, &reachX2_), read(-406, 0, &reachHeight_);
    read(-407, 0, &t407_), read(-408, 0, &peepY_), read(-409, 0, &farLeftX2_), read(-410, 0, &farRightX2_);
    read(-411, 0, &dropY_);
    int hx = 0, hy = 0;
    read(-412, 0, &hx), read(-412, 1, &hy);
    home_ = Vec3(hx, hy, depthFromY(hy));
    read(-413, 0, &wordStrength_), read(-414, 0, &teachWindow_);
    read(-415, 0, &amountSmall_), read(-415, 1, &amountMedium_), read(-415, 2, &amountLarge_);
    read(-416, 0, &stomachLow_), read(-416, 1, &stomachHigh_);
    read(-417, 0, &hungryWarning_), read(-418, 0, &junkLimit_), read(-419, 0, &t419_), read(-420, 0, &moodStep_);
    return init();
}

bool Pest::init() {  // 8a3e
    good_ = junk_ = bowels_ = mood_ = 0;  // [Cheats] GoodStomach/JunkStomach/Bowels/MentalState… default 0
    fleas_ = rabies_ = heat_ = treated_ = 0;
    std::fill(std::begin(saved29a_), std::end(saved29a_), 0);
    owned_.clear();
    if (const adm::Node* profile = adm::childModule(*module_, kProfileTable)) {
        for (const adm::Node& l : profile->children) {
            if (l.tag == adm::Tag::List) owned_.push_back(0);
        }
    }
    if (!load()) birth();
    lastChoicePoint_ = moodPersonality();
    return loadTable(kTableIdle, &idleTable_) && loadTable(kTableWalk, &walkTable_) &&
           loadTable(kTableReaction, &reactionTable_) && loadTable(kTableReaction2, &reactionTable2_) &&
           loadTable(kTableReaction3, &reactionTable3_);
}

void Pest::birth() {  // 8d28: born now, in this hour
    name_.clear();
    birthTime_ = wallSeconds();
    const std::time_t t = std::time_t(birthTime_);
    const std::tm* lt = std::localtime(&t);
    int hour = lt ? lt->tm_hour % 12 : 12;
    if (hour == 0) hour = 12;
    setBirthHour(hour);
    age_ = 0;
    respawnOwnedObjects();
}

bool Pest::setBirthHour(int hour) {  // 8dca
    birthHour_ = hour;
    // -380: birth personality (data × 1, one row per hour) [inf: values are the N/E pair].
    int n = 0, e = 0;
    if (const adm::Node* table = adm::childModule(*module_, kBirthPersonality)) {
        int row = 0;
        for (const adm::Node& l : table->children) {
            if (l.tag != adm::Tag::List) continue;
            if (++row == hour) {
                const std::vector<int32_t> v = adm::intValues(l);
                if (v.size() >= 2) n = v[0], e = v[1];
            }
        }
    }
    personality_ = birthPersonality_ = {n, e};
    return loadPreferences(hour, kPrefActivities, &activities_) && loadPreferences(hour, kPrefReactions, &reactions_);
}

bool Pest::loadTable(int32_t module, std::vector<TableRow>* rows) {  // 9fd0
    rows->clear();
    const adm::Node* table = adm::childModule(*module_, module);
    if (!table) return false;
    for (const adm::Node& l : table->children) {
        if (l.tag != adm::Tag::List) continue;
        const std::vector<int32_t> v = adm::intValues(l);
        if (v.size() < 3) return false;
        rows->push_back({v[0], {v[1] * 1000, v[2] * 1000}});
    }
    return true;
}

bool Pest::loadPreferences(int hour, int32_t module, PreferenceList* list) {  // a186
    const adm::Node* table = adm::childModule(*module_, module);
    if (!table) return false;
    list->entries.clear();
    list->percent = 0;
    int index = 0;
    for (const adm::Node& c : table->children) {
        if (c.tag == adm::Tag::List && list->percent == 0) {
            const std::vector<int32_t> v = adm::intValues(c);
            if (!v.empty()) list->percent = v[0];
        } else if (c.tag == adm::Tag::Map && ++index == hour) {
            for (const adm::Node& l : c.children) {
                if (l.tag != adm::Tag::List) continue;
                const std::vector<int32_t> v = adm::intValues(l);
                list->entries.push_back({l.id, v.empty() ? 1 : v[0]});
            }
        }
    }
    return list->percent > 0;
}

int32_t Pest::PreferenceList::pick() const {  // 1010:2804
    if (percent <= 0 || percent > 100 || entries.empty()) return 0;
    for (;;) {
        for (const auto& [symbol, tries] : entries) {
            for (int i = 0; i < tries; ++i) {
                if (std::rand() % 100 < percent) return symbol;
            }
        }
    }
}

void Pest::PreferenceList::promote(int32_t symbol) {  // 1010:26a8
    for (size_t i = 1; i < entries.size(); ++i) {
        if (entries[i].first == symbol) {
            std::swap(entries[i], entries[i - 1]);
            return;
        }
    }
}

void Pest::PreferenceList::demote(int32_t symbol) {  // 1010:274c
    for (size_t i = 0; i + 1 < entries.size(); ++i) {
        if (entries[i].first == symbol) {
            std::swap(entries[i], entries[i + 1]);
            return;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Persistence (8f32 / 962c). Same fields as the original PEST.CFG, in a simple text format.

static std::wstring saveDirectory() {
    const wchar_t* appData = _wgetenv(L"APPDATA");
    std::wstring dir = appData ? std::wstring(appData) + L"\\CyberPest" : L".";
    _wmkdir(dir.c_str());
    return dir;
}

bool Pest::save() {  // 8f32
    const std::wstring dir = saveDirectory();
    const std::wstring tmp = dir + L"\\PEST.TMP", cfg = dir + L"\\PEST.CFG";
    FILE* f = _wfopen(tmp.c_str(), L"w");
    if (!f) return false;
    std::fprintf(f, "CyberPest-remake 1\nname %s\nbirth %ld %d %ld\n", name_.c_str(), birthTime_, birthHour_, age_);
    std::fprintf(f, "personality %d %d %d %d\n", personality_.x, personality_.y, birthPersonality_.x, birthPersonality_.y);
    std::fprintf(f, "body %d %d %d %d %d %d %d %d %d %d %d %d %d\n", good_, junk_, bowels_, mood_, fleas_, rabies_, heat_,
                 treated_, saved29a_[0], saved29a_[1], saved29a_[2], saved29a_[3], saved29a_[4]);
    for (const PreferenceList* list : {&activities_, &reactions_}) {
        std::fprintf(f, "prefs %d %zu", list->percent, list->entries.size());
        for (const auto& [sym, tries] : list->entries) std::fprintf(f, " %d %d", sym, tries);
        std::fprintf(f, "\n");
    }
    for (const auto* dict : {&objectWords_, &commandWords_}) {
        std::fprintf(f, "words %zu\n", dict->size());
        for (const auto& [word, w] : *dict) std::fprintf(f, "%s %d %d\n", word.c_str(), w.value, w.strength);
    }
    std::fprintf(f, "owned %zu", owned_.size());
    for (int32_t id : owned_) std::fprintf(f, " %d", id);
    std::fprintf(f, "\n");
    if (std::fclose(f) != 0) return false;
    _wremove(cfg.c_str());
    return _wrename(tmp.c_str(), cfg.c_str()) == 0;
}

bool Pest::load() {  // 962c
    const std::wstring cfg = saveDirectory() + L"\\PEST.CFG";
    FILE* f = _wfopen(cfg.c_str(), L"r");
    if (!f) return false;
    char line[512];
    bool ok = std::fgets(line, sizeof line, f) && std::string(line).rfind("CyberPest-remake 1", 0) == 0;
    auto next = [&]() { return ok && std::fgets(line, sizeof line, f) != nullptr; };
    if (next()) {
        char nm[200] = {};
        std::sscanf(line, "name %199[^\n]", nm);
        name_ = nm;
    }
    ok = ok && next() && std::sscanf(line, "birth %ld %d %ld", &birthTime_, &birthHour_, &age_) == 3;
    ok = ok && next() &&
         std::sscanf(line, "personality %d %d %d %d", &personality_.x, &personality_.y, &birthPersonality_.x,
                     &birthPersonality_.y) == 4;
    ok = ok && next() &&
         std::sscanf(line, "body %d %d %d %d %d %d %d %d %d %d %d %d %d", &good_, &junk_, &bowels_, &mood_, &fleas_,
                     &rabies_, &heat_, &treated_, &saved29a_[0], &saved29a_[1], &saved29a_[2], &saved29a_[3],
                     &saved29a_[4]) == 13;
    for (PreferenceList* list : {&activities_, &reactions_}) {
        if (!(ok = ok && next())) break;
        char* p = line + 6;
        size_t n = 0;
        list->percent = int(std::strtol(p, &p, 10));
        n = size_t(std::strtoul(p, &p, 10));
        list->entries.clear();
        for (size_t i = 0; i < n; ++i) {
            const int32_t sym = int32_t(std::strtol(p, &p, 10));
            const int tries = int(std::strtol(p, &p, 10));
            list->entries.push_back({sym, tries});
        }
    }
    for (auto* dict : {&objectWords_, &commandWords_}) {
        size_t n = 0;
        ok = ok && next() && std::sscanf(line, "words %zu", &n) == 1;
        dict->clear();
        for (size_t i = 0; ok && i < n && next(); ++i) {
            char word[200] = {};
            Word w;
            if (std::sscanf(line, "%199s %d %d", word, &w.value, &w.strength) == 3) (*dict)[word] = w;
        }
    }
    if (ok && next()) {
        char* p = line + 6;
        const size_t n = size_t(std::strtoul(p, &p, 10));
        for (size_t i = 0; i < n && i < owned_.size(); ++i) owned_[i] = int32_t(std::strtol(p, &p, 10));
    }
    std::fclose(f);
    if (!ok) return false;
    for (int32_t id : owned_) {  // things the pest left in the room come back
        if (id && !world_.find(id)) cueSpawn(id, Vec3());
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Hooks

void Pest::onStateStart() {  // 56e8
    if (state() == kStDecide) decide();
}

void Pest::onStateEnd() {  // 571c
    if (!turning_) return;
    turning_ = 0;
    if (!walking()) {
        enterState(kStDecide);
        return;
    }
    if (mode_ == 1 && (interact_ == 6 || interact_ == 3)) followTarget();
    enterState(arrived_ ? kStDecide : walkState_);
}

void Pest::onSpriteStart(Sprite&) {  // 57d6
    if (walking() && state() == walkState_ && playingMain()) walkStep();
}

void Pest::onLastFrame(Sprite&) {  // 583c
    if (walking() && state() == walkState_ && playingMain()) {
        if (mode_ == 1 && interact_ == 6 && target_) followTarget();
        if (mode_ == 1 && interact_ == 3 && target_) {
            const int dx = std::abs(interactionPoint(0, target_).x - position().x);
            if (dx > reachX_) followTarget();
        }
        if (position() == walkTarget() || arrived_) {
            setAttached(false);
            setLoop(mainSprite(), false, 0);
            stopWalking();
        } else {
            walkStep();
            setLoop(mainSprite(), true, 0);
        }
    }
    if (mode_ == 1 && interact_ == 7 &&
        (!target_ || !target_->held() || target_->heightAboveFloor() < 1)) {
        setLoop(runner_ ? runner_->outro() : nullptr, false, 0);
    }
    if (timerOn_ && !timerExpired_) checkTimer();
    if (timer2On_ && !timer2Expired_) checkTimer2();
}

void Pest::onFrame() {  // 69b2
    lifeTick();
    GameObject::onFrame();
}

// ---------------------------------------------------------------------------------------------
// Decision making

void Pest::decide() {  // 0962
    bool startActivityAllowed = true;
    idleChoice_ = 0;
    for (;;) {
        if (state() != kStDecide) return;
        forgetMissingOwnedObjects();
        if (property(-448)) {  // flea spray in progress
            enterState(-447);
            continue;
        }
        if (secondLeg_) {
            secondLeg_ = 0;
            walkTo(0, 0, walkState_, secondLegTarget_);
            continue;
        }
        if (resetMode_) {
            resetMode_ = 0;
            mode_ = 0;
        }
        switch (mode_) {
        default: {
            if (hasCommandedObject_) {
                GameObject* obj = world_.find(commandedObject_);
                const adm::Node* m = world_.db().findModule(commandedObject_);
                const int32_t cls = m ? m->ref : 0;
                if (!obj) {
                    target_ = nullptr;
                    if (cls == -558 || cls == -337 || cls == -129) {
                        enterState(-505);
                        startActivityAllowed = false;
                        hasCommandedObject_ = 0;
                    } else if (cls == -274 || cls == -91) {
                        forcedActivity_ = 12;
                    }
                } else if (!obj->property(-82) && !obj->property(-523) && !obj->property(-190)) {
                    target_ = obj;
                } else {
                    target_ = nullptr;
                    if (cls == -587) {
                        enterState(-505);
                        startActivityAllowed = false;
                        hasCommandedObject_ = 0;
                    } else if (cls == -558 || cls == -274 || cls == -129 || cls == -91) {
                        forcedActivity_ = 12;
                    } else if (cls == -337) {
                        forcedActivity_ = 13;
                    }
                }
            } else if (hasCommandedReaction_ && !target_) {
                forcedActivity_ = 12;
            }
            if (good_ + junk_ > 99 && position().x > screenLeftX_ && position().x < screenRightX_ &&
                position().y < world_.config().floorMaxY) {  // too full: wants attention
                world_.requestAttention();
                enterState(-446);
                break;
            }
            if (target_) startInteraction();
            else if (startActivityAllowed) startActivity();
            break;
        }
        case 1:
            if (!interrupted_) interactStep();
            else wander();
            break;
        case 2:
            if (timerOn_ && !timerExpired_ && !interrupted_) continueTimedActivity();
            else if (scripted_ && !scriptDone_ && !interrupted_) continueScript();
            else wander();
            break;
        case 3:
            if (!target_ && !userClicked_) react();
            else mode_ = 4;
            break;
        case 4:
            if (!target_ || userClicked_) idle();
            else mode_ = 0;
            break;
        }
    }
}

bool Pest::fleeIfParentOffscreen() {  // 0f2a
    GameObject* p = parent();
    if (!p) return false;
    const bool off = p->position().x < screenLeftX_ || p->position().x > screenRightX_;
    if (!(off && p->property(-20))) return false;
    mode_ = 2;
    activity_ = 14;
    scripted_ = 1;
    timerOn_ = 0;
    scriptDone_ = 0;
    interrupted_ = 0;
    scriptStep_ = 0;
    scriptGoToParent(0);
    return true;
}

void Pest::chooseReactionForTarget() {  // 1060
    targetWalkState_ = target_->property(-23) ? target_->property(-23) : walkState();
    if (hasCommandedReaction_) targetReaction_ = commandedReaction_;
    else targetReaction_ = target_->property(-522) ? target_->property(-522) : objectReaction();
    targetDistance_ = 0;
    childFirstInt(targetReaction_, &targetDistance_);
}

void Pest::startInteraction() {  // 1168
    if (!target_) return;
    stopWalking();
    setParent(target_);
    mode_ = 1;
    interact_ = 1;
    timerOn_ = scripted_ = scriptStep_ = scriptDone_ = interrupted_ = 0;
    userClicked_ = resetMode_ = 0;
    chooseReactionForTarget();
}

Vec3 Pest::interactionPoint(int distance, GameObject* target) const {  // 2688
    int side;
    if (target->property(-18)) side = target->facing() ? 1 : -1;
    else if (target->property(-19)) side = target->facing() ? -1 : 1;
    else {
        side = position().x < target->position().x ? -1 : 1;
        if (distance < 0) side = -side;
    }
    Vec3 p = target->position();
    p.x += distance * side;
    p.z += 1;
    p.y = yFromDepth(p.z);
    return p;
}

void Pest::interactStep() {  // 1246
    if (!target_) {
        wander();
        return;
    }
    const Vec3 tp = target_->position(), me = position();
    const bool offLeft = tp.x < screenLeftX_ && me.x <= tp.x;
    const bool offRight = tp.x > screenRightX_ && tp.x <= me.x;
    if (offLeft || offRight) {
        wander();
        return;
    }
    setParent(target_);
    switch (interact_) {
    case 1:
        targetPos_ = target_->position();
        if (!turnTowardsParent()) interact_ = 2;
        break;
    case 2:
        interact_ = 3;
        if (target_->property(-278) && !hasCommandedObject_ && !hasCommandedReaction_ && tp.x > screenLeftX_ &&
            tp.x < screenRightX_ && tp.y < world_.config().floorMaxY) {
            const int32_t s = reactionState2();
            if (s != kStDecide) enterState(s);
        }
        break;
    case 3: {
        if (!target_->property(-21)) {
            interact_ = 6;
            break;
        }
        const Vec3 p = interactionPoint(0, target_);
        const bool near = std::abs(p.x - position().x) <= reachX_ && (position().x < p.x) == facing();
        if (!near) walkTo(0, 1, -464, p);
        else interact_ = 4;
        break;
    }
    case 4: {
        if (!target_->property(-21)) {
            interact_ = 6;
            break;
        }
        Vec3 p = interactionPoint(0, target_);
        if (std::abs(p.x - position().x) > reachX_) {
            interact_ = 3;
            break;
        }
        p.x += rnd(0, jitterX_) * (target_->facing() ? 1 : -1);
        walkTo(0, 0, -465, p);
        interact_ = 5;
        break;
    }
    case 5: {
        if (!target_->property(-21)) {
            interact_ = 6;
            break;
        }
        const Vec3 p = interactionPoint(0, target_);
        if (std::abs(p.x - position().x) > reachX2_) {
            interact_ = 3;
            break;
        }
        target_->sendMessage(-327, this);
        Vec3 at = target_->position() + Vec3(0, 0, 1);
        at.y = yFromDepth(at.z);
        setPosition(at);
        enterState(kStWrestle);
        interact_ = 6;
        break;
    }
    case 6: {
        if (target_->held() && !target_->property(-24)) {
            target_ = nullptr;
            break;
        }
        if (target_->property(-21)) {
            interact_ = 3;
            break;
        }
        int32_t walkSt = targetWalkState_;
        Vec3 p;
        if (target_->held() && target_->heightAboveFloor() > 0) {
            if (target_->heightAboveFloor() < reachHeight_) {
                targetDistance_ = 0;
                p = interactionPoint(targetDistance_, target_);
                if (p == position()) {
                    interact_ = 7;
                    break;
                }
            } else {
                childFirstInt(kStGetDown, &targetDistance_);
                p = interactionPoint(targetDistance_, target_);
                if (p == position()) {
                    interact_ = 8;
                    turnTowardsParent();
                    break;
                }
            }
        } else {
            childFirstInt(targetReaction_, &targetDistance_);
            p = interactionPoint(targetDistance_, target_);
            if (p == position()) {
                interact_ = 10;
                turnTowardsParent();
                break;
            }
        }
        bool turnFirst;
        if (!target_->held() && target_->position() == targetPos_) turnFirst = true;
        else turnFirst = (position().x < parent()->position().x) == facing();
        walkTo(turnFirst, 1, walkSt, p);
        break;
    }
    case 7:
        if (!target_->held() || target_->heightAboveFloor() < 1) {
            targetPos_ = target_->position();
            interact_ = 6;
        } else {
            enterState(-486);
        }
        break;
    case 8: {
        const Vec3 p = interactionPoint(targetDistance_, target_);
        if (target_->held() && position() == p && target_->heightAboveFloor() >= reachHeight_) {
            interact_ = 9;
            enterState(kStGetDown);
            break;
        }
        interact_ = 6;
        break;
    }
    case 9:
        interact_ = (!target_->held() || target_->heightAboveFloor() < 1) ? 6 : 7;
        break;
    case 10:  // arrived next to the target: react to it [rest of 1246, see docs]
        mode_ = 3;
        enterState(targetReaction_ ? targetReaction_ : kStDecide);
        break;
    default:
        mode_ = 0;
        break;
    }
}

void Pest::followTarget() {  // 283e
    if (!target_) {
        clearWalkFlags();
        return;
    }
    if (mode_ == 1 && (interact_ == 3 || interact_ == 4)) {
        targetDistance_ = 0;
    } else if (target_->held() && target_->heightAboveFloor() > 0) {
        if (target_->heightAboveFloor() < reachHeight_) targetDistance_ = 0;
        else childFirstInt(kStGetDown, &targetDistance_);
    } else {
        childFirstInt(targetReaction_, &targetDistance_);
    }
    const Vec3 p = interactionPoint(targetDistance_, target_);
    if (closeEnough(p, position())) {
        if ((position().x < p.x) == facing()) setWalkTargetOnly(p);
        else clearWalkFlags();
    }
}

void Pest::startActivity() {  // 2aa4
    rememberBehaviour(0);
    rememberObject(0);
    if (forcedActivity_) {
        activity_ = forcedActivity_;
        forcedActivity_ = 0;
    } else {
        activity_ = chooseActivity();
    }
    mode_ = 2;
    timerOn_ = scripted_ = scriptStep_ = scriptDone_ = interrupted_ = 0;
    userClicked_ = resetMode_ = 0;
    switch (activity_) {
    case 1:
        startTimer(walkTimeMax_, walkTimeMin_);
        walkActivityState_ = walkState();
        walkAcross();
        break;
    case 2:
        startTimer(walkTimeMax_, walkTimeMin_);
        enterState(-454);
        break;
    case 3:
        startTimer(peepTimeMax_, peepTimeMin_);
        peep();
        break;
    case 4: piddle(); break;
    case 5: poo(); break;
    case 6: vomit(); break;
    case 7:
        if (world_.count(-39) < maxMess_) enterState(-476);
        break;
    case 8: enterState(-477); break;
    case 9: enterState(-479); break;
    case 10: enterState(-478); break;
    case 11:
        scripted_ = 1;
        scriptStep_ = 0;
        if (!startTv()) mode_ = scripted_ = 0;
        break;
    case 12:
        scripted_ = 1;
        scriptStep_ = 0;
        if (!startObjectActivity()) mode_ = scripted_ = 0;
        break;
    case 13:
        scripted_ = 1;
        scriptStep_ = 0;
        if (!startObjectActivity2()) mode_ = scripted_ = 0;
        break;
    default:
        startTimer(walkTimeMax_, walkTimeMin_);
        enterState(-454);
        break;
    }
}

void Pest::wander() {  // 2da6
    mode_ = 3;
    Vec3 p;
    if (position().y >= peepY_) {  // came out of the peep hole: re-enter at a side
        p.x = rnd(0, 1) ? farLeftX2_ : farRightX2_;
        p.y = rnd(world_.config().floorMinY, world_.config().floorMaxY);
        p.z = depthFromY(p.y);
        setPosition(p);
    }
    if (position().x < floorLeftX_ || position().x > floorRightX_) {
        const int dy = rnd(0, world_.config().floorMaxY - world_.config().floorMinY);
        p = Vec3(rnd(floorLeftX_, floorRightX_), world_.config().floorMinY + dy, dy + 3000);
        walkTo(0, 1, activity_ == 1 ? walkActivityState_ : walkState(), p);
    }
}

void Pest::react() {  // 2fcc
    mode_ = 4;
    const int32_t s = reactionState();
    if (s != kStDecide) enterState(s);
}

void Pest::idle() {  // 3012
    mode_ = 5;
    if (!heldByUser_) startTimer(idleShortMax_, idleShortMin_);
    else {
        startTimer(idleMax_, idleMin_);
        idleChoice_ = 1;
    }
    if (idleState() != kStDecide) enterState(idleState());
}

void Pest::startTimer(int maxSeconds, int minSeconds) {  // 30b6
    timerOn_ = 1;
    timerStart_ = nowMs();
    timerLength_ = long(rnd(minSeconds, maxSeconds)) * 1000;
    timerExpired_ = 0;
    interrupted_ = 0;
}

void Pest::checkTimer() {  // 3190
    const long now = nowMs();
    const bool done = interrupted_ || (target_ && mode_ != 1) || now > timerStart_ + timerLength_ || now < timerStart_;
    if (done && !held()) {
        setLoop(mainSprite(), false, 0);
        interrupted_ = 0;
        timerExpired_ = 1;
    }
}

void Pest::checkTimer2() {  // 327c
    const long now = nowMs();
    if (now > timer2Start_ + timer2Length_ || now < timer2Start_) timer2Expired_ = 1;
}

void Pest::continueTimedActivity() {  // 32f8
    if (mode_ == 2 && activity_ == 1) walkAcross();
    else if (mode_ == 2 && activity_ == 3) peep();
    else if (mode_ == 5) enterState(idleState());
}

void Pest::continueScript() {  // 338a
    if (mode_ != 2) return;
    if (activity_ == 14) scriptGoToParent(++scriptStep_);
    else if (activity_ == 15) objectScript(++scriptStep_);
    else if (activity_ == 16) objectScript2(++scriptStep_);
    else if (activity_ == 11) tvScript(++scriptStep_);
}

void Pest::piddle() {  // 3484
    if (world_.find(-330) && world_.find(-331) && world_.find(-332)) return;
    switch (rnd(1, 3)) {
    case 1: if (!world_.find(-330)) enterState(-470); break;
    case 2: if (!world_.find(-331)) enterState(-471); break;
    case 3: if (!world_.find(-332)) enterState(-472); break;
    }
}

void Pest::vomit() {  // 35dc
    if (world_.find(-333) && world_.find(-334) && world_.find(-335)) return;
    switch (rnd(1, 3)) {
    case 1: if (!world_.find(-333)) enterState(-473); break;
    case 2: if (!world_.find(-334)) enterState(-474); break;
    case 3: if (!world_.find(-335)) enterState(-475); break;
    }
}

void Pest::poo() {  // 3734
    if (world_.count(-39) < maxPoos_) enterState(-469);
}

void Pest::walkAcross() {  // 377c
    const int dy = rnd(0, world_.config().floorMaxY - world_.config().floorMinY);
    const int x = position().x < (leftEdgeB_ + rightEdgeA_) / 2 ? rnd(rightEdgeA_, rightEdgeB_)
                                                                : rnd(leftEdgeA_, leftEdgeB_);
    walkTo(0, 1, walkActivityState_, Vec3(x, world_.config().floorMinY + dy, dy + 3000));
}

void Pest::runOff() {  // 38dc
    if (position().x > leftEdgeA_ && position().x < rightEdgeB_ && position().y < world_.config().floorMaxY) {
        Vec3 p = position();
        p.x = facing() ? rightEdgeB_ : leftEdgeA_;
        walkTo(0, 0, -359, p);
    } else {
        enterState(kStDecide);
    }
}

void Pest::hideOffscreen() {  // 3a6a
    Vec3 p = position();
    if (p.x - farLeftX_ < farRightX_ - p.x) {
        p.x = farLeftX_;
        walkTo(0, 0, facing() ? -467 : -468, p);
    } else {
        p.x = farRightX_;
        walkTo(0, 0, facing() ? -468 : -467, p);
    }
}

void Pest::peep() {  // 3b8c: peep in from a side or the hatch
    const bool offScreen = position().x != offLeftX_ && position().x != offRightX_ && position().y != peepY_;
    if (offScreen) {
        Vec3 p = position();
        p.x = position().x < (offLeftX_ + offRightX_) / 2 ? offLeftX_ : offRightX_;
        walkTo(0, 1, 0, p);
        return;
    }
    if (facing() != (position().x == offLeftX_)) {
        turning_ = 1;
        enterState(facing() ? -460 : -459);
        return;
    }
    if (rnd(0, 2) == 0) {
        Vec3 p;
        if (position().y != peepY_) {
            p = Vec3(centreX_, peepY_, 0);
            p.z = depthFromY(p.y);
        } else if (rnd(0, 1) == 0) {
            p = Vec3(offRightX_, rnd(world_.config().floorMinY, world_.config().floorMaxY), 0);
            p.z = depthFromY(p.y);
            setFacing(false);
        } else {
            p = Vec3(offLeftX_, rnd(world_.config().floorMinY, world_.config().floorMaxY), 0);
            p.z = depthFromY(p.y);
            setFacing(true);
        }
        setPosition(p);
    } else if (!peepToggle_) {
        peepToggle_ = 1;
        enterState(-347);
        setLoop(mainSprite(), true, rnd(peepLoopsMin_, peepLoopsMax_));
    } else {
        peepToggle_ = 0;
        enterState(position().y == peepY_ ? -482 : -480);
        setLoop(mainSprite(), true, rnd(peekLoopsMin_, peekLoopsMax_));
    }
}

bool Pest::scriptGoToParent(int step) {  // 4020
    if (step == 0) walkNextTo(1, 0, 0, -496);
    else if (step == 1) turnTowardsParent();
    else if (step == 2) {
        rememberObject(parent() ? parent()->id() : 0);
        rememberBehaviour(0);
        walkRandomly();
    } else if (step == 3) scriptDone_ = 1;
    return true;
}

bool Pest::startTv() {  // 40d0
    GameObject* tv = world_.find(kTv);
    if (!tv || tv->state() != -108) return false;
    setParent(tv);
    activity_ = 11;
    tvScript(0);
    return true;
}

bool Pest::startObjectActivity() {  // 4160
    activityObject_ = randomTreat();
    GameObject* obj = world_.find(activityObject_);
    if (!obj) {
        const int32_t id = randomTreat();
        fetchObject(id);
        target_ = world_.find(id);
        return false;
    }
    if (obj->property(-82) && obj->state() == -108) {
        activityObjectPos_ = obj->position();
        targetWalkState_ = walkState();
        targetReaction_ = -497;
        childFirstInt(targetReaction_, &targetDistance_);
        setParent(obj);
        activity_ = 15;
        objectScript(0);
        return true;
    }
    if (hasCommandedObject_) {
        hasCommandedObject_ = 0;
        enterState(-505);
    }
    return false;
}

bool Pest::startObjectActivity2() {  // 4302
    activityObject_ = randomToy();
    GameObject* obj = world_.find(activityObject_);
    if (!obj) return false;
    if (obj->property(-82) && obj->state() == -108) {
        activityObjectPos_ = obj->position();
        targetWalkState_ = walkState();
        targetReaction_ = -497;
        childFirstInt(targetReaction_, &targetDistance_);
        setParent(obj);
        activity_ = 15;
        objectScript(0);
        return true;
    }
    if (obj->property(-523) && !obj->held()) {
        activityObjectPos_ = obj->position();
        targetWalkState_ = walkState();
        targetReaction_ = -499;
        childFirstInt(targetReaction_, &targetDistance_);
        setParent(obj);
        activity_ = 16;
        objectScript2(0);
        return true;
    }
    if (hasCommandedObject_) {
        hasCommandedObject_ = 0;
        enterState(-505);
    }
    return false;
}

void Pest::tvScript(int step) {  // 4542
    if (step == 0) walkTo(0, 1, 0, home_);
    else if (step == 1) turnTowardsParent();
    else if (step == 2) {
        GameObject* tv = world_.find(kTv);
        if (!tv || tv->state() != -108) scriptDone_ = 1;
        else {
            tv->sendMessage(-125, this);
            enterState(-484);
        }
    } else if (step == 3) {
        if (GameObject* tv = world_.find(kTv)) tv->sendMessage(-127, this);
        scriptDone_ = 1;
    }
}

void Pest::objectScript(int step) {  // 4672
    if (step == 0) {
        if (parent()) walkTo(0, 1, targetWalkState_, interactionPoint(targetDistance_, parent()));
        return;
    }
    if (step == 1) {
        turnTowardsParent();
        return;
    }
    GameObject* obj = world_.find(activityObject_);
    if (step == 2) {
        if (obj && obj->position() == activityObjectPos_ && obj->property(-82)) {
            enterState(-497);
            return;
        }
        scriptDone_ = 1;
        return;
    }
    if (step == 3) {
        if (obj && (obj->property(-82) || obj->held())) {
            enterState(-505);
            hasCommandedObject_ = 0;
        }
        scriptDone_ = 1;
    }
}

void Pest::objectScript2(int step) {  // 4826
    GameObject* obj = world_.find(activityObject_);
    if (!obj) {
        scriptDone_ = 1;
        return;
    }
    if (step == 0) {
        if (obj->property(-523) && !obj->held()) {
            Vec3 p = position();
            p.x = rnd(0, 1) ? farLeftX2_ : farRightX2_;
            walkTo(0, 1, targetWalkState_, p);
            return;
        }
        scriptDone_ = 1;
        return;
    }
    if (step == 1) {
        if (obj->property(-523) && !obj->held()) {
            Vec3 p(obj->position().x, peepY_, 0);
            p.z = depthFromY(p.y);
            setPosition(p);
            obj->setProperty(-180, 0);
            enterState(-499);
            return;
        }
        scriptDone_ = 1;
        return;
    }
    if (step == 2) {
        if (!obj->property(-523) && !obj->held()) {
            target_ = parent();
            mode_ = 0;
        } else {
            enterState(-505);
            hasCommandedObject_ = 0;
        }
        scriptDone_ = 1;
    }
}

bool Pest::fetchObject(int32_t module) {  // 4ad0: bring an object in from off screen
    if (world_.find(module)) return false;
    GameObject* obj = world_.spawn(module, Vec3(), facing());
    if (!obj) return false;
    Vec3 p = randomFloorPoint();
    p.x = rnd(0, 1) ? farRightX_ : farLeftX_;
    obj->setPosition(p);
    setParent(obj);
    return true;
}

void Pest::walkNextTo(int turnIfBehind, int direct, int32_t state, int32_t distanceList) {  // 4c4c
    if (!parent()) return;
    Vec3 p = parent()->position();
    p.y += 1;
    p.z += 1;
    if ((p.x < position().x) == facing()) turnIfBehind = 0;
    int distance = 0;
    childFirstInt(distanceList, &distance);
    if (p.x < screenLeftX_) p.x += distance;
    else if (p.x > screenRightX_) p.x -= distance;
    else p.x += (p.x < position().x ? 1 : -1) * distance;
    walkTo(turnIfBehind, direct, state, p);
}

bool Pest::turnTowardsParent() {  // 4e7e
    GameObject* p = parent();
    if (!p) return false;
    if (position().x < p->position().x && !facing()) {
        turning_ = 1;
        enterState(-460);
        return true;
    }
    if (p->position().x < position().x && facing()) {
        turning_ = 1;
        enterState(-459);
        return true;
    }
    return false;
}

void Pest::walkRandomly() {  // 5004
    Vec3 p;
    do p = randomFloorPoint();
    while (!closeEnough(p, position()));
    walkTo(0, 1, -496, p);
}

Vec3 Pest::randomFloorPoint() const {  // 50ce
    const int x = rnd(floorLeftX_, floorRightX_);
    const int dy = rnd(0, world_.config().floorMaxY - world_.config().floorMinY);
    return Vec3(x, world_.config().floorMinY + dy, dy + 3000);
}

void Pest::walkTo(int turnFirst, int direct, int32_t state, const Vec3& target) {  // 5136
    if (direct && !closeEnough(target, position())) {
        // Too far off the straight line: go via an intermediate floor point first.
        const Vec3 mid = pointNear(target, position(), floorLeftX_, world_.config().floorMinY, floorRightX_,
                                   world_.config().floorMaxY);
        secondLeg_ = 1;
        secondLegTarget_ = target;
        walkTo(0, 0, state, mid);
        return;
    }
    if (state == 0) state = walkState();
    walkState_ = state;
    arrived_ = 0;
    setWalkTarget(target);
    bool facingTarget = false;
    if (walkState_ == -496 || walkState_ == -467) {
        facingTarget = true;
    } else {
        const bool towards = (position().x < target.x) != facing();
        if (turnFirst && towards && std::abs(target.x - position().x) < turnDistance1_) {
            facingTarget = true;
            walkState_ = -463;
        } else if (turnFirst && towards && std::abs(target.x - position().x) < turnDistance2_) {
            facingTarget = true;
            walkState_ = -462;
        } else if (std::abs(target.x - position().x) < turnDistance1_) {
            walkState_ = -355;
        }
    }
    const bool needLeft = position().x < walkTarget().x && facing() == facingTarget;
    const bool needRight = walkTarget().x < position().x && facing() != facingTarget;
    if (needLeft) {
        turning_ = 1;
        enterState(-460);
    } else if (needRight) {
        turning_ = 1;
        enterState(-459);
    } else {
        turning_ = 0;
        enterState(walkState_);
    }
}

int Pest::noticeObject(int fromClick, GameObject* obj) {  // 63b0
    if (!obj) return 0;
    bool interested = false, forget = false;
    if (!obj->property(-82) && !obj->property(-523) && !obj->property(-190) && !obj->property(-1) &&
        (!obj->held() || obj->property(-24))) {
        if (target_ && target_->position().y > world_.config().floorMaxY) {
            interested = false;
        } else if (!hasCommandedObject_ || obj->id() != commandedObject_) {
            if (!obj->property(-282) && !obj->property(-281) && !obj->property(-17)) {
                if (target_ != obj) {
                    if (!target_) interested = true;
                    else if (!fromClick) {
                        if (obj->id() == -166 || target_->property(-558)) interested = true;
                        else if (obj->property(-21) && target_->id() != -166 && !target_->property(-558))
                            interested = true;
                    } else {
                        interested = true;
                    }
                }
            } else {
                forget = true;
            }
        } else if (target_ != obj) {
            interested = true;
        }
    } else {
        forget = true;
    }
    if (!interested || property(-423) || depressed() || overExcited()) {
        if (forget && obj == target_) target_ = nullptr;
        return 0;
    }
    target_ = obj;
    chooseReactionForTarget();
    if (!property(-423) && mode_ != 1 && (mode_ != 2 || (activity_ != 15 && activity_ != 16 && activity_ != 14)))
        return 1;
    return 0;
}

void Pest::wakeUp() {  // 6736: the hatch opened
    sendMessage(-30, this);
    dropTarget();
    clearWalkFlags();
    rememberObject(0);
    rememberBehaviour(0);
    hasCommandedObject_ = hasCommandedReaction_ = 0;
    resetMode_ = 1;
    target_ = nullptr;
    switch (rnd(0, 2)) {
    case 0: {  // drop in from above
        const int dy = rnd(0, world_.config().floorMaxY - world_.config().floorMinY);
        setPosition(Vec3(rnd(floorLeftX_, floorRightX_), dropY_, dy + 3000));
        enterState(-309);
        break;
    }
    case 1: {  // walk in from the left
        Vec3 p(offLeftX_, rnd(world_.config().floorMinY, world_.config().floorMaxY), 0);
        p.z = depthFromY(p.y);
        setPosition(p);
        setFacing(true);
        peepToggle_ = 0;
        forcedActivity_ = 3;
        enterState(kStDecide);
        break;
    }
    case 2: {  // walk in from the right
        Vec3 p(offRightX_, rnd(world_.config().floorMinY, world_.config().floorMaxY), 0);
        p.z = depthFromY(p.y);
        setPosition(p);
        setFacing(false);
        peepToggle_ = 0;
        forcedActivity_ = 3;
        enterState(kStDecide);
        break;
    }
    }
}

void Pest::clearWalkFlags() {  // 69de
    arrived_ = 1;
    secondLeg_ = 0;
}

void Pest::dropTarget() {  // 6a08
    interrupted_ = 1;
    setAttached(false);
    if (target_) {
        target_->setProperty(-180, 1);
        if (target_->state() == -284) target_->sendMessage(-305, this);
    }
    GameObject* tv = world_.find(-72);
    if (tv && (tv->state() == -125 || tv->state() == -135)) tv->sendMessage(-127, this);
}

Vec3 Pest::pointNear(const Vec3& from, const Vec3& to, int x1, int y1, int x2, int y2) {  // 6b44
    Vec3 p;
    for (int i = 0; i < 20; ++i) {
        p = Vec3(rnd(x1, x2), rnd(y1, y2), 0);
        if (closeEnough(p, to) && closeEnough(p, from)) break;
    }
    p.z = depthFromY(p.y);
    return p;
}

bool Pest::closeEnough(const Vec3& a, const Vec3& b) const {  // 6d58: slope test (dy/dx % < c2)
    const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
    if (dx == 0) return false;
    return long(dy) * 100 / dx < directWalkPercent_;
}

// ---------------------------------------------------------------------------------------------
// Behaviour choice

Point2 Pest::moodPersonality() const {  // a2b6
    const int m = std::clamp(mood_, -100, 100);
    return {personality_.x - m * 100, personality_.y + m * 100};
}

int32_t Pest::nearestRow(const std::vector<TableRow>& rows) {  // a3ae
    if (rows.empty()) return 0;
    const Point2 me = moodPersonality();
    int32_t best = 0;
    long long bestDist = 100000000;
    Point2 bestPoint{};
    for (size_t i = 0; i < rows.size(); ++i) {
        const TableRow& r = rows[size_t(rnd(0, int(rows.size()) - 1))];
        const long long dx = r.point.x - me.x, dy = r.point.y - me.y;
        if (dx * dx + dy * dy < bestDist || best == 0) {
            best = r.state;
            bestPoint = r.point;
            bestDist = dx * dx + dy * dy;
        }
    }
    lastChoicePoint_ = bestPoint;
    return best;
}

int32_t Pest::walkState() {  // a530
    if (rabid()) return -466;
    if (starving() || needsToilet()) return -358;
    if (junkSick()) return -360;
    if (onHeat()) return -357;
    if (suicidal()) return -361;
    if (depressed()) return -358;
    if (overExcited()) return -359;
    return nearestRow(walkTable_);
}

int32_t Pest::idleState() {  // a6b8
    if (rabid()) return -442;
    if (starving() || needsToilet()) return -350;
    if (junkSick()) return -352;
    if (onHeat()) return -348;
    if (suicidal()) return -351;
    if (depressed()) return -350;
    if (overExcited()) return kStDecide;
    return nearestRow(idleTable_);
}

int32_t Pest::reactionState() {  // a808
    if (rabid() || starving()) return kStDecide;
    if (needsToilet()) return -366;
    if (junkSick()) return -368;
    if (onHeat()) return -369;
    if (suicidal()) return -367;
    if (depressed() || overExcited()) return kStDecide;
    int32_t s;
    do s = nearestRow(reactionTable_);
    while (happy() && s == kStDecide);
    return s;
}

int32_t Pest::reactionState2() {  // a9aa
    if (rabid() || starving()) return kStDecide;
    if (needsToilet()) return -490;
    if (junkSick()) return -373;
    if (onHeat()) return -374;
    if (suicidal()) return -491;
    if (depressed() || overExcited()) return kStDecide;
    int32_t s;
    do s = nearestRow(reactionTable2_);
    while (happy() && s == kStDecide);
    return s;
}

int32_t Pest::reactionState3() {  // ab4c
    if (rabid() || starving() || needsToilet() || junkSick() || onHeat()) return kStDecide;
    if (suicidal()) return -376;
    if (depressed() || overExcited() || happy()) return kStDecide;
    return nearestRow(reactionTable3_);
}

int32_t Pest::randomTreat() const {  // aca2
    if (hasCommandedObject_) return commandedObject_;
    static const int32_t k[] = {-85, -81, -311, -84, -312, -313, -86, -87};
    return k[rnd(0, 7)];
}

int32_t Pest::randomToy() const {  // ad38
    if (hasCommandedObject_) return commandedObject_;
    static const int32_t k[] = {-338, -339, -340, -341, -342, -343, -344, -345};
    return k[rnd(0, 7)];
}

int32_t Pest::objectReaction() {  // adce
    if (nextReaction_) {
        nextReaction_ = 0;
        return nextReactionValue_;
    }
    if (rabid()) return -266;
    if (starving()) {
        const int r = rnd(0, 2);
        return r == 0 ? -253 : r == 1 ? -256 : -258;
    }
    if (needsToilet()) return -257;
    if (junkSick()) return rnd(0, 1) ? -255 : -254;
    if (onHeat()) return -264;
    if (suicidal()) return -259;
    if (!depressed() && !overExcited() && happy()) return rnd(0, 1) ? -260 : -266;
    if (hasCommandedReaction_) return commandedReaction_;
    int32_t r;
    do r = reactions_.pick();
    while (!reactionAllowed(r));
    return r;
}

bool Pest::reactionAllowed(int32_t r) const {  // b010
    if (good_ + junk_ > stomachHigh_) {
        if (r == -256 || r == -253 || r == -265 || r == -258) return false;
    } else if (good_ + junk_ < stomachLow_) {
        if (r == -259 || r == -260 || r == -263 || r == -264 || r == -255 || r == -266) return false;
    } else if (r == -255) {
        return false;
    }
    return !(bowels_ < stomachLow_ && r == -254);
}

int Pest::chooseActivity() {  // b10e
    if (nextActivity_) {
        nextActivity_ = 0;
        return nextActivityValue_;
    }
    if (rabid()) return 1;
    if (starving()) return rnd(0, 1) ? 13 : 1;
    if (needsToilet()) return rnd(0, 1) ? 8 : 5;
    if (junkSick()) return rnd(0, 1) ? 6 : 5;
    if (onHeat()) return 1;
    if (suicidal()) return 3;
    if (depressed()) return rnd(0, 1) ? 2 : 1;
    if (!overExcited() && happy()) {
        const int r = rnd(0, 2);
        return r == 0 ? 12 : r == 1 ? 11 : 1;
    }
    int32_t a;
    do {
        if (!fleas_ || rnd(0, 1) == 0) a = activities_.pick();
        else a = rnd(0, 1) ? -249 : -250;
    } while (!activityAllowed(a));
    lastChoice_ = a;
    switch (a) {
    case -251: return 11;
    case -250: return 10;
    case -249: return 9;
    case -248: return 4;
    case -247: return 3;
    case -246: return 12;
    case -245: return 7;
    case -244: return 1;
    case -243: return 2;
    case -242: return 8;
    case -241: return 13;
    case -240: return 5;
    case -239: return 6;
    default: return 1;
    }
}

bool Pest::activityAllowed(int32_t a) const {  // b44c
    if (good_ + junk_ > stomachHigh_) {
        if (a == -241) return false;
    } else if (good_ + junk_ < stomachLow_) {
        if (a == -239) return false;
    } else if (a == -241 || a == -239) {
        return false;
    }
    return !(bowels_ < stomachLow_ && a == -240);
}

// ---------------------------------------------------------------------------------------------
// Megaphone words (b5a2..bb5a)

void Pest::hearWords(bool teaching, GameObject* megaphone) {  // b5a2
    const std::string text = megaphone ? megaphone->text() : std::string();
    if (!text.empty()) {
        if (!teaching) {
            auto find = [&](const std::map<std::string, Word>& dict, std::string* word) {  // 1010:1fce
                size_t i = 0;
                while (i < text.size()) {
                    while (i < text.size() && !std::isalpha(static_cast<unsigned char>(text[i]))) ++i;
                    size_t j = i;
                    while (j < text.size() && std::isalpha(static_cast<unsigned char>(text[j]))) ++j;
                    std::string w = text.substr(i, j - i);
                    for (char& c : w) c = char(std::tolower(static_cast<unsigned char>(c)));
                    if (!w.empty() && dict.count(w)) {
                        *word = w;
                        return true;
                    }
                    i = j;
                }
                return false;
            };
            std::string w;
            if (find(objectWords_, &w)) obeyObjectWord(w);
            if (find(commandWords_, &w)) obeyCommandWord(w);
            if (!hasCommandedObject_ && !hasCommandedReaction_) {
                if (Runner* r = runner()) r->nextState = -487;  // didn't understand
            } else {
                rememberObject(0);
                rememberBehaviour(0);
                if (Runner* r = runner()) r->nextState = -488;  // understood
            }
            dropTarget();
        } else {
            int32_t meaning = 0;
            if (recentBehaviour(&meaning)) learnWord(meaning, text, commandWords_);
            else if (recentObject(&meaning)) learnWord(meaning, text, objectWords_);
        }
    }
    save();
}

void Pest::learnWord(int32_t meaning, const std::string& raw, std::map<std::string, Word>& dict) {  // b7aa
    std::string text;
    for (char c : raw) {
        if (std::isalpha(static_cast<unsigned char>(c))) text += char(std::tolower(static_cast<unsigned char>(c)));
    }
    if (text.empty()) return;
    auto it = dict.find(text);
    if (it == dict.end() || it->second.value != meaning) {
        commandWords_.erase(text);
        objectWords_.erase(text);
        dict[text] = {meaning, wordStrength_};
        if (Runner* r = runner()) r->nextState = -489;  // learnt
    } else {
        it->second.strength = std::clamp(it->second.strength + amountLarge_, 0, 100);
    }
}

void Pest::obeyObjectWord(const std::string& word) {  // b890
    auto it = objectWords_.find(word);
    if (it == objectWords_.end()) return;
    objectWord_ = word;
    hasCommandedObject_ = 1;
    commandedObject_ = it->second.value;
}

void Pest::obeyCommandWord(const std::string& word) {  // b90e
    auto it = commandWords_.find(word);
    if (it == commandWords_.end()) return;
    if (rnd(0, 100) < it->second.strength) {
        commandedReaction_ = it->second.value;
    } else {
        hasCommandedReaction_ = 0;
        commandedReaction_ = objectReaction();
    }
    disobeyed_ = commandedReaction_ != it->second.value;
    commandWord_ = word;
    hasCommandedReaction_ = 1;
}

void Pest::rememberObject(int32_t id) {  // b9e2
    recentObject_ = id;
    if (id) recentObjectAt_ = nowMs();
}

bool Pest::recentObject(int32_t* id) const {  // ba1e
    const long now = nowMs();
    if (!recentObject_ || now <= recentObjectAt_ || now >= recentObjectAt_ + long(teachWindow_) * 1000) return false;
    *id = recentObject_;
    return true;
}

void Pest::rememberBehaviour(int32_t state) {  // bac4
    recentBehaviour_ = state;
    lastChoice_ = state;
    if (!state) {
        reinforceWordPending_ = 0;
        return;
    }
    recentBehaviourAt_ = nowMs();
    if (hasCommandedReaction_) {
        reinforceMeaning_ = state;
        reinforceWord_ = commandWord_;
        reinforceWordPending_ = 1;
    }
}

bool Pest::recentBehaviour(int32_t* state) const {  // bb5a
    const long now = nowMs();
    if (!recentBehaviour_ || now <= recentBehaviourAt_ || now >= recentBehaviourAt_ + long(teachWindow_) * 1000)
        return false;
    *state = recentBehaviour_;
    return true;
}

void Pest::reinforce(int delta) {  // bc00: snacks (+) and shocks (−)
    mood_ += delta;
    if (lastChoice_) {
        if (delta < 1) activities_.demote(lastChoice_), reactions_.demote(lastChoice_);
        else activities_.promote(lastChoice_), reactions_.promote(lastChoice_);
    }
    if (reinforceWordPending_) {
        auto it = commandWords_.find(reinforceWord_);
        if (it != commandWords_.end()) {
            const int d = it->second.value == reinforceMeaning_ ? delta : -delta;
            it->second.strength = std::clamp(it->second.strength + d, 0, 100);
        }
    }
    if (lastChoicePoint_.x != personality_.x || lastChoicePoint_.y != personality_.y) {
        // Move the personality toward (reward) or away from (punishment) the rewarded behaviour.
        const long long dx = lastChoicePoint_.x - personality_.x, dy = lastChoicePoint_.y - personality_.y;
        personality_.x = int(std::clamp<long long>(personality_.x + dx * delta / 100, 0, 10000));
        personality_.y = int(std::clamp<long long>(personality_.y + dy * delta / 100, 0, 10000));
    }
    lastChoice_ = 0;
    reinforceWordPending_ = 0;
    lastChoicePoint_ = moodPersonality();
}

// ---------------------------------------------------------------------------------------------
// Body

void Pest::respawnOwnedObjects() {  // c1fc
    const adm::Node* profile = adm::childModule(*module_, kProfileTable);
    if (!profile) return;
    size_t i = 0;
    for (const adm::Node& l : profile->children) {
        if (l.tag != adm::Tag::List || i >= owned_.size()) continue;
        const std::vector<int32_t> v = adm::intValues(l);
        owned_[i] = v.empty() ? 0 : v[0];
        if (owned_[i] && !world_.find(owned_[i])) cueSpawn(owned_[i], Vec3());
        ++i;
    }
}

void Pest::forgetMissingOwnedObjects() {  // c30c
    for (int32_t& id : owned_) {
        if (id && !world_.find(id)) id = 0;
    }
}

void Pest::eat(int amount, bool junk) {  // c3b6
    forgetMissingOwnedObjects();
    if (!junk) good_ += amount;
    else {
        junk_ += amount;
        if (junkSick()) world_.requestAttention();
    }
}

void Pest::emptyBowels(int amount) { bowels_ = std::max(0, bowels_ - amount); }  // c426

void Pest::burnFood(int amount) {  // c472
    if (junk_ > 0) {
        if (junk_ < amount) amount -= junk_, junk_ = 0;
        else junk_ -= amount, amount = 0;
    }
    good_ -= amount;
}

void Pest::digest() {  // c4e4
    if (good_) {
        --good_;
        if (bowels_ < 100 && ++bowels_ == 100) world_.requestAttention();
        if (good_ == hungryWarning_) world_.requestAttention();
    }
    if (junk_) {
        --junk_;
        if (bowels_ < 100 && ++bowels_ == 100) world_.requestAttention();
    }
}

void Pest::drift() {  // c5b4: mood settles, personality returns toward its birth value
    if (mood_ > 0) --mood_;
    if (mood_ < 0) ++mood_;
    personality_.x += personality_.x < birthPersonality_.x ? 10 : -10;
    personality_.y += personality_.y < birthPersonality_.y ? 10 : -10;
}

bool Pest::callerNeeded(int32_t caller) {  // c9ee
    switch (caller) {
    case -209: return ill();         // Pest Vet
    case -208: return roomMessy();   // Pestkeeper
    case -207: return moodExtreme(); // TheraPest
    default: return false;           // -210 Pestrologer, -206 Pesterminator
    }
}

bool Pest::moodExtreme() const { return mood_ < -50 || mood_ > 50; }  // cb34

bool Pest::roomMessy() const {  // cb70
    if (world_.count(-39) >= maxPoos_) return true;
    if (world_.find(-330) && world_.find(-331) && world_.find(-332)) return true;
    if (world_.find(-333) && world_.find(-334) && world_.find(-335)) return true;
    if (world_.count(-39) >= maxMess_) return true;
    return state() == -226 || state() == -227;
}

bool Pest::ill() const {  // ccee
    return good_ < 1 || bowels_ > 99 || junk_ > junkLimit_ || fleas_ || rabies_ || heat_ || state() == -226 ||
           state() == -227;
}

void Pest::spawnFlea() {  // deb0: a flea jumps off the pest
    const Sprite* s = shownSprite();
    const Flc* f = s ? s->image(facing()) : nullptr;
    const int w = f ? f->width() : 40, h = f ? f->height() : 40;
    const Vec3 at = position() + Vec3(rnd(-w / 2, w / 2), rnd(-h, 0), rnd(-1, 1));
    if (GameObject* flea = world_.spawn(kFlea, at, false)) {
        if (Sprite* m = flea->mainSprite()) m->motion.x = rnd(-std::abs(m->motion.x), std::abs(m->motion.x));
    }
}

void Pest::maybeCatchFleas() {  // e0c0
    if (rnd(1, 240) == 1) {
        fleas_ = 1;
        world_.requestAttention();
    }
}

void Pest::checkOnHeat() {  // e104: on heat during its birth hour (after minute 54)
    const std::time_t t = std::time(nullptr);
    const std::tm* lt = std::localtime(&t);
    heat_ = lt && lt->tm_hour % 12 == birthHour_ - 1 && lt->tm_min > 54;
}

void Pest::checkAttention(long now) {  // e17e: bored of being ignored
    const long idleMinutes = (now - lastInteraction_) / 60;
    const int extroversion = std::min(moodPersonality().y / 100, 100);
    if (idleMinutes > 120 - extroversion) {
        world_.requestAttention();
        lastInteraction_ = now;
    }
}

void Pest::maybeMouse() {  // e2a4: a mouse visits from the hole
    GameObject* hole = world_.find(kMouse);
    if (!hole || hole->state() != -321 || world_.find(kMouseVisitor)) return;
    if (!forceMinute_ && rnd(0, 60) != 0) return;
    forceMinute_ = 0;
    world_.spawn(kMouseVisitor, Vec3(), false);
}

void Pest::maybePhone() {  // e3a8: someone phones if they're needed
    static const int32_t callers[] = {-206, -207, -208, -209, -210};
    const int32_t caller = callers[rnd(0, 4)];
    if (callerNeeded(caller)) cueSend(kPhone, caller);
}

void Pest::lifeTick() {  // e452: once per (wall-clock) second
    const long now = wallSeconds();
    if (lastInteraction_ == 0) lastInteraction_ = now;
    if (now == lastSecond_ && !advanceTime_) return;
    age_ += advanceTime_ ? 60 : 1;
    if (fleas_) spawnFlea();
    if ((now % 60 == 0 && lastSecond_ % 60 != 0) || forceMinute_ || advanceTime_) {
        save();
        digest();
        drift();
        maybeCatchFleas();
        checkOnHeat();
        checkAttention(now);
        maybeMouse();
        maybePhone();
    }
    if (age_ % 7200 == 0) cueSend(kFoodFlap, -165);  // every two hours: a delivery at the flap
    lastSecond_ = now;
}

// ---------------------------------------------------------------------------------------------
// Events (5b54)

void Pest::onEvent(GameObject* from, int32_t value, int type) {
    bool interruptedByUser = false;
    if (type == 1 && value != 0 && !property(-423)) {
        userClicked_ = 1;
        interruptedByUser = true;
    } else if (type == 0) {
        switch (value) {
        case -502:  // electric shock
            if (!rabies_) {
                treated_ = 1;
                mood_ -= moodStep_;
            } else {
                treated_ = 0;
                rabies_ = 0;
            }
            break;
        case -494:  // snack
            if (junk_ < junkLimit_) {
                treated_ = 1;
                mood_ += moodStep_;
            } else {
                treated_ = 0;
                junk_ = 0;
            }
            break;
        case -448:  // flea spray
            if (fleas_) fleas_ = 0;
            setProperty(-448, 1);
            clearWalkFlags();
            dropTarget();
            break;
        case -445: hideOffscreen(); break;
        case -441: runOff(); break;
        case -305: setAttached(false); break;
        case -293:
            if (!property(-423)) {
                clearWalkFlags();
                dropTarget();
                userClicked_ = 0;
                enterState(-293);
            }
            break;
        case -283: setAttached(true); break;
        case -226:  // reset stomach to the INI cheat values when overfull
            if (good_ + junk_ > 99) good_ = junk_ = 0;
            if (bowels_ > 99) bowels_ = 0;
            break;
        case -222:
            world_.requestAttention();
            rabies_ = 1;
            break;
        case -202: hearWords(true, from); break;
        case -201:
            if (!rabid()) hearWords(false, from);
            break;
        case -187:
            heldByUser_ = 1;
            if (!rabid()) {
                if (mode_ == 5) {
                    startTimer(idleMax_, idleMin_);
                    idleChoice_ = 1;
                } else if (position().y != 0) {
                    interruptedByUser = true;
                    userClicked_ = 1;
                }
            }
            break;
        case -185:
            heldByUser_ = 0;
            if (idleChoice_) dropTarget();
            break;
        case -175: respawnOwnedObjects(); break;
        case -169:
            if (!property(-423) && state() != -169) {
                stopWalking();
                dropTarget();
                mode_ = 0;
                timerOn_ = 0;
                userClicked_ = 0;
                if (position().y != 0) mode_ = 2;
                enterState(-169);
            }
            break;
        case -153:  // the hatch opened
            mode_ = 2;
            enterState(kStDecide);
            break;
        // Amounts are the tuning values f_128/f_12a/f_12c (5/10/20), as in 5b54.
        case -40: burnFood(amountLarge_); break;
        case -39: emptyBowels(amountLarge_); break;
        case -38: eat(amountLarge_, true); break;
        case -37: eat(amountMedium_, true); break;
        case -36: eat(amountSmall_, true); break;
        case -35: eat(amountLarge_, false); break;
        case -34: eat(amountMedium_, false); break;
        case -33: eat(amountSmall_, true); break;  // junk in the original too
        case -32: reinforce(-amountLarge_); break;
        case -31: reinforce(-amountMedium_); break;
        case -30: reinforce(-amountSmall_); break;
        case -29: reinforce(amountLarge_); break;
        case -28: reinforce(amountMedium_); break;
        case -27: reinforce(amountSmall_); break;
        case -26:
            if (!property(-423)) {
                if (held()) {
                    setProperty(-182, 0);
                    setProperty(-309, 1);
                }
                clearWalkFlags();
                dropTarget();
                rememberObject(0);
                rememberBehaviour(0);
                hasCommandedReaction_ = 0;
                commandedObject_ = 0;
                resetMode_ = 1;
                enterState(-443);
            }
            break;
        case -16:  // an object asks for attention
            noticeObject(1, from);
            rememberObject(from ? from->id() : 0);
            rememberBehaviour(0);
            break;
        case -15: interruptedByUser = noticeObject(0, from) != 0; break;
        case -14:
            forgetMissingOwnedObjects();
            if (parent() == from) setParent(nullptr);
            if (from == target_) target_ = nullptr;
            break;
        case -1: wakeUp(); return;
        default: break;
        }
    }
    if (interruptedByUser) {
        if (walking()) clearWalkFlags();
        if (mode_ == 2 || mode_ == 1) dropTarget();
    }
    GameObject::onEvent(from, value, type);
}

std::string Pest::describeState() const {
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "pest mode=%d activity=%d interact=%d walk=%d good=%d junk=%d bowels=%d mood=%d fleas=%d rabies=%d "
                  "heat=%d personality=(%d,%d) hour=%d age=%ld",
                  mode_, activity_, interact_, walkState_, good_, junk_, bowels_, mood_, fleas_, rabies_, heat_,
                  personality_.x, personality_.y, birthHour_, age_);
    return buf;
}
