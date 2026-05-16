#include "IcePee.h"
#include "Input.h"

#include "component/ActionComponent.h"
#include "component/ParticlesComponent.h"

#include <cmath>

using namespace doriax;

static Quaternion getPointSpritesBaseRotation() {
    return Quaternion(
        Vector3(0.0f, 0.0f, 1.0f),
        Vector3(0.0f, 1.0f, 0.0f),
        Vector3(-1.0f, 0.0f, 0.0f));
}

static Quaternion getPointSpritesControlRotation(float yaw, float pitch) {
    Quaternion yawRotation(yaw, Vector3(0.0f, 1.0f, 0.0f));
    Quaternion pitchRotation(pitch, Vector3(0.0f, 0.0f, 1.0f));
    return yawRotation * pitchRotation;
}

static Entity findParticlesEntityForTarget(Scene* scene, Entity targetEntity) {
    if (!scene || targetEntity == NULL_ENTITY) {
        return NULL_ENTITY;
    }

    std::vector<Entity> entities = scene->getEntityList();
    for (Entity candidate : entities) {
        ActionComponent* action = scene->findComponent<ActionComponent>(candidate);
        ParticlesComponent* particles = scene->findComponent<ParticlesComponent>(candidate);
        if (action && particles && action->target == targetEntity) {
            return candidate;
        }
    }

    return NULL_ENTITY;
}

static void cacheVelocityConeFromParticles(const ParticlesComponent& particles, float& minSpeed, float& maxSpeed, float& coneAngle) {
    const Vector3& minVelocity = particles.velocityInitializer.minVelocity;
    const Vector3& maxVelocity = particles.velocityInitializer.maxVelocity;

    float minForward = std::min(std::fabs(minVelocity.x), std::fabs(maxVelocity.x));
    float maxForward = std::max(std::fabs(minVelocity.x), std::fabs(maxVelocity.x));
    float lateralX = std::max(std::fabs(minVelocity.y), std::fabs(maxVelocity.y));
    float lateralY = std::max(std::fabs(minVelocity.z), std::fabs(maxVelocity.z));
    float lateralRadius = std::sqrt(lateralX * lateralX + lateralY * lateralY);

    minSpeed = std::max(0.0f, minForward);
    maxSpeed = std::max(minSpeed, maxForward);
    coneAngle = (minSpeed > 0.0f) ? std::atan2(lateralRadius, minSpeed) * 57.2957795130823208768f : 0.0f;
}

static void applyVelocityConeToParticles(ParticlesComponent& particles, float minSpeed, float maxSpeed, float coneAngle) {
    const float clampedMinSpeed = std::max(0.0f, std::min(minSpeed, maxSpeed));
    const float clampedMaxSpeed = std::max(clampedMinSpeed, std::max(minSpeed, maxSpeed));
    const float clampedConeAngle = std::min(std::max(coneAngle, 0.0f), 89.0f);
    const float coneAngleRad = clampedConeAngle * 0.01745329251994329577f;

    // The point sprite base rotation maps local +Z to world -X, so a local cone around +Z
    // produces the requested default direction while drag rotation steers the cone.
    const float lateralExtent = (clampedMinSpeed > 0.0f)
        ? std::tan(coneAngleRad) * clampedMinSpeed * 0.7071067811865475244f
        : 0.0f;

    particles.velocityInitializer.minVelocity = Vector3(-lateralExtent, -lateralExtent, clampedMinSpeed);
    particles.velocityInitializer.maxVelocity = Vector3(lateralExtent, lateralExtent, clampedMaxSpeed);
}

IcePee::IcePee(Scene* scene, Entity entity): ScriptBase(scene, entity) {
    REGISTER_ENGINE_EVENT(onUpdate);

}

IcePee::~IcePee() {

}

void IcePee::onUpdate() {
    if (!isActive) return;

    Vector2 mousePosition = Input::getMousePosition();
    bool isDragging = Input::isMousePressed(S_MOUSE_BUTTON_LEFT);

    if (isDragging && wasDragging) {
        float deltaX = mousePosition.x - lastMousePosition.x;
        float deltaY = mousePosition.y - lastMousePosition.y;
        float rotationSensitivity = speed * 0.1f;

        pointSpritesYaw -= deltaX * rotationSensitivity;
        pointSpritesPitch += deltaY * rotationSensitivity;
    }

    lastMousePosition = mousePosition;
    wasDragging = isDragging;

    if (pointSprites) {
        Quaternion controlRotation = getPointSpritesControlRotation(pointSpritesYaw, pointSpritesPitch);
        pointSprites->setRotation(controlRotation * getPointSpritesBaseRotation());

        Entity targetEntity = pointSprites->getEntity();
        if (particlesEntity == NULL_ENTITY || !scene->findComponent<ParticlesComponent>(particlesEntity)) {
            particlesEntity = findParticlesEntityForTarget(scene, targetEntity);
            particlesConeCached = false;
        }

        ParticlesComponent* particles = scene->findComponent<ParticlesComponent>(particlesEntity);
        if (particles) {
            if (!particlesConeCached) {
                cacheVelocityConeFromParticles(*particles, particlesMinSpeed, particlesMaxSpeed, particlesConeAngle);
                particlesConeCached = true;
            }
            applyVelocityConeToParticles(*particles, particlesMinSpeed, particlesMaxSpeed, particlesConeAngle);
        }
    }

    // Example: Increment counter every frame
    counter++;
}

