#include "classes.h"

#include "world.h"

namespace {

// Certificate of infestation (class -593, vtable 1038:3e9a). Only its positioning is ported so
// far: create (1010:47c6) places it from its own list -594 [x, y, z] [inf]. Still to port: name
// entry and the date/time text (1010:513a, 1010:502a, 1010:50e8).
class Certificate : public GameObject {
public:
    using GameObject::GameObject;

    bool create(const Vec3& pos, bool facing) override {
        Vec3 p = pos;
        for (const adm::Node& c : module_->children) {
            if (c.tag == adm::Tag::List && c.id == -594) {
                const std::vector<int32_t> v = adm::intValues(c);
                if (v.size() >= 3) p = Vec3(v[0], v[1], v[2]);
            }
        }
        return GameObject::create(p, facing);
    }
};

}  // namespace

std::unique_ptr<GameObject> createObjectForClass(int32_t classSymbol, World& world, int32_t moduleId,
                                                 const adm::Node* module) {
    switch (classSymbol) {
    case -593:
        return std::make_unique<Certificate>(world, moduleId, module);
    default:
        // Not yet ported classes behave as the generic object (-107) for now.
        return std::make_unique<GameObject>(world, moduleId, module);
    }
}
