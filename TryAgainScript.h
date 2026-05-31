#pragma once

#include "Shape.h"
#include "Object.h"
#include "ScriptProperty.h"

class IcePee;

class TryAgainScript : public doriax::Object {
public:
    SPROPERTY("Is Active")
    bool isActive = true;
    
    SPROPERTY("Ice Pee Script")
    IcePee* icePeeScript = nullptr;

    TryAgainScript(doriax::Scene* scene, doriax::Entity entity);
    virtual ~TryAgainScript();

    void onButtonPress();
};
