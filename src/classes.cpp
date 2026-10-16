#include "classes.h"

#include "certificate.h"
#include "bin.h"
#include "clock.h"
#include "items.h"
#include "panels.h"
#include "pest.h"
#include "punishments.h"
#include "room_objects.h"
#include "world.h"

std::unique_ptr<GameObject> createObjectForClass(int32_t classSymbol, World& world, int32_t moduleId,
                                                 const adm::Node* module) {
    switch (classSymbol) {
    case -593:
        return std::make_unique<Certificate>(world, moduleId, module);
    case -105:
        return std::make_unique<Pest>(world, moduleId, module);
    case -90:
        return std::make_unique<Clock>(world, moduleId, module);
    case -78:
        return std::make_unique<Bin>(world, moduleId, module);
    case -274:
    case -558:
        return std::make_unique<ShelfItem>(world, moduleId, module);
    case -337:
    case -587:
        return std::make_unique<ShelfCarryItem>(world, moduleId, module);
    case -129:
        return std::make_unique<ShelfWrapperItem>(world, moduleId, module);
    case -189:
        return std::make_unique<MegaphonePanel>(world, moduleId, module);
    case -188:
        return std::make_unique<PhoneBubble>(world, moduleId, module);
    case -98:
        return std::make_unique<Phone>(world, moduleId, module);
    case -91:
        return std::make_unique<Mouse>(world, moduleId, module);
    case -94:
        return std::make_unique<AudioButton>(world, moduleId, module);
    case -92:
        return std::make_unique<PunishmentDisplay>(world, moduleId, module);
    case -531:
        return std::make_unique<Weight>(world, moduleId, module);
    case -534:
        return std::make_unique<Bomb>(world, moduleId, module);
    case -550:
        return std::make_unique<Cannonball>(world, moduleId, module);
    case -172:
        return std::make_unique<Cannon>(world, moduleId, module);
    case -591:
        return std::make_unique<Powder>(world, moduleId, module);
    default:
        // Classes without overrides of their own behave as the generic object (-107).
        return std::make_unique<GameObject>(world, moduleId, module);
    }
}
