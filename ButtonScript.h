#pragma once

#include "Shape.h"
#include "Object.h"
#include "ScriptProperty.h"

namespace doriax {
    class Particles;
}

class IcePee;

class ButtonScript : public doriax::Object {
public:
    // Example properties
    SPROPERTY("Is Active")
    bool isActive = true;

    SPROPERTY("Speed")
    float speed = 5.0f;

    SPROPERTY("Target Position")
    doriax::Vector3 targetPosition = doriax::Vector3(0, 0, 0);

    SPROPERTY("Ice Pee Script")
    IcePee* icePeeScript = nullptr;

    SPROPERTY("Particles")
    doriax::Particles* particles = nullptr;

    ButtonScript(doriax::Scene* scene, doriax::Entity entity);
    virtual ~ButtonScript();

    void onButtonPress();
};
