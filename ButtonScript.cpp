#include "ButtonScript.h"

#include "ButtonComponent.h"
#include "IcePee.h"
#include "Log.h"
#include "SceneManager.h"

using namespace doriax;

ButtonScript::ButtonScript(Scene* scene, Entity entity): Object(scene, entity) {
    REGISTER_BUTTON_EVENT(onPress, onButtonPress);

}

ButtonScript::~ButtonScript() {
    if (scene && scene->isEntityCreated(entity)) {
        UNREGISTER_BUTTON_EVENT(onPress, onButtonPress);
    }
}

void ButtonScript::onButtonPress() {
    if (!isActive) return;

    if (icePeeScript) {
        icePeeScript->isActive = true;
    }

    Log::print("Button '%s' pressed", getName().c_str());
    SceneManager::addChildScene("Score Scene");
    SceneManager::removeChildScene("UI Scene");
}

