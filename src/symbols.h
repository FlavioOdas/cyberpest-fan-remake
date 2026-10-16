// ADM symbol ids recovered from PEST.EXE (see docs/re/symbols.tsv and docs/re/state-commands.md).
#pragma once

#include <cstdint>

namespace sym {

// Modules and classes
constexpr int32_t RoomScreen = -59;    // the main room: placement lists
constexpr int32_t ClassPest = -105;
constexpr int32_t ClassObject = -107;  // generic room object
constexpr int32_t ClassState = -109;   // ref of state sub-modules
constexpr int32_t StateInitial = -108;

// Object properties (Object_SetProperty, 1000:63ee)
constexpr int32_t Z = -110;
constexpr int32_t X = -178;
constexpr int32_t Y = -179;

// Room lists
constexpr int32_t Place = -71;           // object, x, y, z
constexpr int32_t RoomSetProperty = -83; // object, property, value

// State commands (State_Run, 1000:8d10)
constexpr int32_t SetProperty = -2;
constexpr int32_t SetPropertyAt = -3;
constexpr int32_t OnEvent = -121;
constexpr int32_t OnClick = -12;
constexpr int32_t Image = -111;
constexpr int32_t ImageSlot2 = -306;
constexpr int32_t ImageSlot3 = -431;
constexpr int32_t PlayMode = -112;
constexpr int32_t HitArea = -114;
constexpr int32_t Sound = -116;
constexpr int32_t SendMessage = -119;
constexpr int32_t NextState = -132;
constexpr int32_t RandomNextState = -151;

}  // namespace sym
