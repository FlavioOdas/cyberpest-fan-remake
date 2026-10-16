// Game object classes (one per class symbol handled by CreateObjectForModule 1000:5604).
// docs/re/classes.md lists every class, its vtable and the hooks it overrides.
#pragma once

#include <memory>

#include "object.h"

std::unique_ptr<GameObject> createObjectForClass(int32_t classSymbol, World& world, int32_t moduleId,
                                                 const adm::Node* module);
