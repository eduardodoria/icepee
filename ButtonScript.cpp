#include "ButtonScript.h"

#include "ButtonComponent.h"
#include "Engine.h"
#include "Log.h"

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

    Log::print("Button '%s' pressed", getName().c_str());
    Engine::removeScene(scene);
}

