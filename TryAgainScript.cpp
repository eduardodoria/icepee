#include "TryAgainScript.h"

#include "IcePee.h"
#include "SceneManager.h"

using namespace doriax;

TryAgainScript::TryAgainScript(Scene* scene, Entity entity): Object(scene, entity) {
    REGISTER_BUTTON_EVENT(onPress, onButtonPress);
}

TryAgainScript::~TryAgainScript() {
    if (scene && scene->isEntityCreated(entity)) {
        UNREGISTER_BUTTON_EVENT(onPress, onButtonPress);
    }
}

void TryAgainScript::onButtonPress() {
    if (!isActive) return;

    if (icePeeScript) {
        icePeeScript->isActive = false;
    }

    SceneManager::addChildScene("Level Scene");
    SceneManager::removeChildScene("Game Over Scene");
}

