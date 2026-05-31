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
    if (!isActive || !icePeeScript) return;

    icePeeScript->resetGame();
    icePeeScript->isActive = true;

    SceneManager::addChildScene("Score Scene");
    SceneManager::removeChildScene("Game Over Scene");
}

