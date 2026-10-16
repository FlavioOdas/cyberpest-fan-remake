// The pest (class -105). Transliterated from the 96 functions behind its vtable overrides
// (segment 2, 1008:*); docs/re/pest.md maps every field and flow. Method names keep the
// original address so each can be checked against `python3 tools/gfunc.py 1008:<addr>`.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "object.h"

class Pest : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override;  // 1008:0000
    void onStateStart() override;                        // 1008:56e8
    void onStateEnd() override;                          // 1008:571c
    void onSpriteStart(Sprite& s) override;              // 1008:57d6
    void onLastFrame(Sprite& s) override;                // 1008:583c
    void onFrame() override;                             // 1008:69b2
    void onEvent(GameObject* from, int32_t value, int type) override;  // 1008:5b54

    // Debug/cheat access.
    std::string describeState() const;
    const std::string& name() const { return name_; }
    long birthTime() const { return birthTime_; }
    void setName(const std::string& name);  // 1038:6f06
    std::string callerText(int32_t caller);  // 1008:ca82
    void setAdvanceTime(bool on) { advanceTime_ = on; }  // [Cheats] AdvanceTime (f_198)

private:
    struct TableRow {     // 1008:9fd0: a behaviour table row
        int32_t state = 0;
        Point2 point;     // personality-space position (data × 1000)
    };
    struct PreferenceList {  // 1010:2574..: ordered list, pick accepts each entry with p%
        int percent = 0;     // +0x12 (list value -378, 20)
        std::vector<std::pair<int32_t, int>> entries;  // (symbol, tries)
        int32_t pick() const;                // 1010:2804
        void promote(int32_t symbol);        // 1010:26a8 (reward)
        void demote(int32_t symbol);         // 1010:274c (punishment)
    };
    struct Word {  // megaphone dictionaries (CMapStringTo… 1010:1c34..1fce)
        int32_t value = 0;
        int strength = 0;
    };

    // ---- decision making (mode f_140) ----
    void decide();                          // 0962
    bool fleeIfParentOffscreen();           // 0f2a
    void chooseReactionForTarget();         // 1060
    void startInteraction();                // 1168
    void interactStep();                    // 1246
    Vec3 interactionPoint(int distance, GameObject* target) const;  // 2688
    void followTarget();                    // 283e
    void startActivity();                   // 2aa4
    void wander();                          // 2da6
    void react();                           // 2fcc
    void idle();                            // 3012
    void startTimer(int maxSeconds, int minSeconds);  // 30b6
    void checkTimer();                      // 3190
    void checkTimer2();                     // 327c
    void continueTimedActivity();           // 32f8
    void continueScript();                  // 338a
    void piddle();                          // 3484: wee (-330..-332)
    void vomit();                           // 35dc: (-333..-335)
    void poo();                             // 3734
    void walkAcross();                      // 377c
    void runOff();                          // 38dc
    void hideOffscreen();                   // 3a6a
    void peep();                            // 3b8c
    bool scriptGoToParent(int step);        // 4020
    bool startTv();                         // 40d0 (object -72)
    bool startObjectActivity();             // 4160
    bool startObjectActivity2();            // 4302
    void tvScript(int step);                // 4542
    void objectScript(int step);            // 4672
    void objectScript2(int step);           // 4826
    bool fetchObject(int32_t module);       // 4ad0
    void walkNextTo(int turnIfBehind, int direct, int32_t state, int32_t distanceList);  // 4c4c
    bool turnTowardsParent();               // 4e7e
    void walkRandomly();                    // 5004
    Vec3 randomFloorPoint() const;          // 50ce
    void walkTo(int turnFirst, int direct, int32_t state, const Vec3& target);  // 5136
    int noticeObject(int fromClick, GameObject* obj);  // 63b0
    void wakeUp();                          // 6736
    void clearWalkFlags();                  // 69de
    void dropTarget();                      // 6a08
    Vec3 pointNear(const Vec3& from, const Vec3& to, int fromX1, int fromY1, int fromX2, int fromY2);  // 6b44
    bool closeEnough(const Vec3& a, const Vec3& b) const;  // 6d58

    // ---- life ----
    bool init();                            // 8a3e
    void birth();                           // 8d28
    bool setBirthHour(int hour);            // 8dca
    bool save();                            // 8f32
    bool load();                            // 962c
    bool loadTable(int32_t module, std::vector<TableRow>* rows);  // 9fd0
    bool loadPreferences(int hour, int32_t module, PreferenceList* list);  // a186
    Point2 moodPersonality() const;         // a2b6
    int32_t nearestRow(const std::vector<TableRow>& rows);  // a3ae
    int32_t walkState();                    // a530
    int32_t idleState();                    // a6b8
    int32_t reactionState();                // a808
    int32_t reactionState2();               // a9aa
    int32_t reactionState3();               // ab4c
    int32_t randomTreat() const;            // aca2
    int32_t randomToy() const;              // ad38
    int32_t objectReaction();               // adce
    bool reactionAllowed(int32_t r) const;  // b010
    int chooseActivity();                   // b10e
    bool activityAllowed(int32_t a) const;  // b44c
    void hearWords(bool teaching, GameObject* megaphone);  // b5a2
    void learnWord(int32_t meaning, const std::string& text, std::map<std::string, Word>& dict);  // b7aa
    void obeyObjectWord(const std::string& word);   // b890
    void obeyCommandWord(const std::string& word);  // b90e
    void rememberObject(int32_t id);        // b9e2
    bool recentObject(int32_t* id) const;   // ba1e
    void rememberBehaviour(int32_t state);  // bac4
    bool recentBehaviour(int32_t* state) const;  // bb5a
    void reinforce(int delta);              // bc00
    void respawnOwnedObjects();             // c1fc
    void forgetMissingOwnedObjects();       // c30c
    void eat(int amount, bool junk);        // c3b6
    void emptyBowels(int amount);           // c426
    void burnFood(int amount);              // c472
    void digest();                          // c4e4
    void drift();                           // c5b4
    bool callerNeeded(int32_t caller);      // c9ee
    bool moodExtreme() const;               // cb34
    bool roomMessy() const;                 // cb70
    bool ill() const;                       // ccee
    void spawnFlea();                       // deb0
    void maybeCatchFleas();                 // e0c0
    void checkOnHeat();                     // e104
    void checkAttention(long now);          // e17e
    void maybeMouse();                      // e2a4
    void maybePhone();                      // e3a8
    void lifeTick();                        // e452

    // Need predicates (1038:5bb0.., docs/re/pest.md)
    bool rabid() const { return rabies_ != 0; }
    bool starving() const { return good_ < 1; }
    bool needsToilet() const { return bowels_ > 99; }
    bool junkSick() const { return junk_ > junkLimit_; }
    bool onHeat() const { return heat_ != 0; }
    bool suicidal() const { return mood_ < -99; }
    bool depressed() const { return mood_ < -49 && mood_ > -100; }
    bool overExcited() const { return mood_ > 99; }
    bool happy() const { return mood_ >= 50 && mood_ <= 99; }
    bool clicked() const;
    long nowMs() const;

    // ---- tuning (create) ----
    int directWalkPercent_ = 50;                 // f_c2
    int idleMin_ = 30, idleMax_ = 60;            // f_c4, f_c6
    int idleShortMin_ = 2, idleShortMax_ = 10;   // f_c8, f_ca
    int t384a_ = 20, t384b_ = 120;               // f_cc, f_ce
    int walkTimeMin_ = 1, walkTimeMax_ = 60;     // f_d0, f_d2
    int peepTimeMin_ = 20, peepTimeMax_ = 120;   // f_d4, f_d6
    int peepLoopsMin_ = 1, peepLoopsMax_ = 5;    // f_d8, f_da
    int peekLoopsMin_ = 1, peekLoopsMax_ = 3;    // f_dc, f_de
    int offLeftX_ = -36, centreX_ = 320, offRightX_ = 676;  // f_e0, f_e4, f_e2
    int leftEdgeA_ = -50, leftEdgeB_ = 200;      // f_e8, f_e6
    int rightEdgeA_ = 440, rightEdgeB_ = 690;    // f_ea, f_ec
    int floorLeftX_ = 100, floorRightX_ = 540;   // f_ee, f_f0
    int screenLeftX_ = 10, screenRightX_ = 630;  // f_f2, f_f4
    int farLeftX_ = -200, farRightX_ = 840;      // f_f6, f_f8
    int t396_ = 3, maxPoos_ = 3, t398_ = 3, maxMess_ = 5, t400_ = 10;  // f_fa..f_102
    int turnDistance2_ = 50, turnDistance1_ = 25;  // f_104, f_106
    int reachX_ = 80, jitterX_ = 50, reachX2_ = 20, reachHeight_ = 90, t407_ = 10;  // f_108..f_110
    int peepY_ = 500, farLeftX2_ = -400, farRightX2_ = 1040, dropY_ = -1000;  // f_112..f_118
    Vec3 home_;                                  // f_11a (-412)
    int wordStrength_ = 50, teachWindow_ = 60;   // f_124, f_126
    int amountSmall_ = 5, amountMedium_ = 10, amountLarge_ = 20;  // f_128, f_12a, f_12c (food/reward amounts; +20 word strength)
    int stomachLow_ = 25, stomachHigh_ = 75;     // f_12e, f_130
    int hungryWarning_ = 10, junkLimit_ = 50, t419_ = 90;  // f_132, f_134, f_136
    int moodStep_ = 40;                          // f_13e

    // ---- AI state ----
    int mode_ = 0;              // f_140
    int activity_ = 0;          // f_142
    int forcedActivity_ = 0;    // f_144
    int interact_ = 0;          // f_146
    int32_t activityObject_ = 0;  // f_148
    Vec3 activityObjectPos_;    // f_14a
    int scripted_ = 0, scriptStep_ = 0, scriptDone_ = 0;  // f_154, f_156, f_158
    int timerOn_ = 0;           // f_15a
    long timerStart_ = 0, timerLength_ = 0;  // f_15c, f_160
    int timerExpired_ = 0;      // f_164
    int timer2On_ = 0;          // f_166
    long timer2Start_ = 0, timer2Length_ = 0;  // f_168, f_16c
    int timer2Expired_ = 0;     // f_170
    int interrupted_ = 0;       // f_172
    int32_t walkActivityState_ = 0;  // f_174
    int peepToggle_ = 0;        // f_176
    int userClicked_ = 0;       // f_178
    int resetMode_ = 0;         // f_17a
    int nextActivity_ = 0, nextActivityValue_ = 0;  // f_17c, f_17e
    int nextReaction_ = 0, nextReactionValue_ = 0;  // f_180, f_182
    int forceMinute_ = 0;       // f_184
    int32_t walkState_ = 0;     // f_186: state entered for the walk (or after turning)
    int turning_ = 0;           // f_188
    int arrived_ = 0;           // f_18a
    int secondLeg_ = 0;         // f_18c
    Vec3 secondLegTarget_;      // f_18e
    bool advanceTime_ = false;  // f_198
    GameObject* target_ = nullptr;  // f_19a
    int32_t targetReaction_ = 0;    // f_19e
    int targetDistance_ = 0;        // f_1a0
    int32_t targetWalkState_ = 0;   // f_1a2
    Vec3 targetPos_;                // f_1a4
    std::map<std::string, Word> objectWords_;   // f_1ae
    std::map<std::string, Word> commandWords_;  // f_1c8
    int heldByUser_ = 0;        // f_1e2
    int idleChoice_ = 0;        // f_1e4
    int32_t commandedObject_ = 0;   // f_1e6
    int32_t commandedReaction_ = 0; // f_1e8
    int hasCommandedObject_ = 0;    // f_1ea
    int hasCommandedReaction_ = 0;  // f_1ec
    std::string objectWord_, commandWord_;  // f_1ee, f_1f6
    int disobeyed_ = 0;         // f_1fe
    int32_t recentObject_ = 0;  // f_200
    long recentObjectAt_ = 0;   // f_202
    int32_t recentBehaviour_ = 0;  // f_206
    long recentBehaviourAt_ = 0;   // f_208
    std::string reinforceWord_;    // f_20c
    int32_t reinforceMeaning_ = 0;  // f_214
    int reinforceWordPending_ = 0;  // f_216
    std::string name_;          // f_218
    long birthTime_ = 0;        // f_220
    int birthHour_ = 0;         // f_224
    long age_ = 0;              // f_226
    Point2 birthPersonality_;   // f_22a
    Point2 personality_;        // f_232 (Neuroticism, Extroversion)
    Point2 lastChoicePoint_;    // f_23a
    std::vector<TableRow> idleTable_, walkTable_, reactionTable_, reactionTable2_, reactionTable3_;  // f_242..f_25e
    PreferenceList activities_, reactions_;  // f_260, f_274
    int32_t lastChoice_ = 0;    // f_288
    int good_ = 0, junk_ = 0, bowels_ = 0, mood_ = 0;  // f_28a..f_290
    int fleas_ = 0, rabies_ = 0, heat_ = 0, treated_ = 0;  // f_292..f_298
    int saved29a_[5] = {};      // f_29a..f_2a2
    std::vector<int32_t> owned_;  // f_138 (count f_13c, from module -337)
    long lastSecond_ = 0;       // DAT_1050_035a
};
