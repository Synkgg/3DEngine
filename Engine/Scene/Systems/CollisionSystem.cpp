#include "CollisionSystem.h"

#include "../Scene.h"

#include "../Components/TransformComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/CharacterControllerComponent.h"

#include <cmath>
#include <vector>
#include <limits>

namespace
{
    constexpr float EPSILON = 0.00001f;

    struct OBB
    {
        Vec3 center;
        Vec3 halfExtents;

        Vec3 axisX;
        Vec3 axisY;
        Vec3 axisZ;
    };

    struct CollisionResult
    {
        bool collided = false;

        Vec3 normal =
            Vec3(
                0.0f,
                0.0f,
                0.0f
            );

        float penetration = 0.0f;
    };

    float Dot(
        const Vec3& a,
        const Vec3& b)
    {
        return
            a.x * b.x +
            a.y * b.y +
            a.z * b.z;
    }

    Vec3 Cross(
        const Vec3& a,
        const Vec3& b)
    {
        return Vec3(
            a.y * b.z -
            a.z * b.y,

            a.z * b.x -
            a.x * b.z,

            a.x * b.y -
            a.y * b.x
        );
    }

    float Length(
        const Vec3& value)
    {
        return std::sqrt(
            value.x * value.x +
            value.y * value.y +
            value.z * value.z
        );
    }

    Vec3 Normalize(
        const Vec3& value)
    {
        const float length =
            Length(value);

        if (length <= EPSILON)
        {
            return Vec3(
                0.0f,
                0.0f,
                0.0f
            );
        }

        return Vec3(
            value.x / length,
            value.y / length,
            value.z / length
        );
    }

    Vec3 RotateVector(
        const Vec3& vector,
        const Vec3& rotation)
    {
        /*
         * Transform::GetMatrix() uses:
         *
         * RotationZ
         * RotationY
         * RotationX
         *
         * and the engine stores rotation in radians.
         */

        const float cx =
            std::cos(rotation.x);

        const float sx =
            std::sin(rotation.x);

        const float cy =
            std::cos(rotation.y);

        const float sy =
            std::sin(rotation.y);

        const float cz =
            std::cos(rotation.z);

        const float sz =
            std::sin(rotation.z);

        /*
         * X rotation.
         */
        Vec3 result(
            vector.x,

            vector.y * cx -
            vector.z * sx,

            vector.y * sx +
            vector.z * cx
        );

        /*
         * Y rotation.
         */
        result =
            Vec3(
                result.x * cy +
                result.z * sy,

                result.y,

                -result.x * sy +
                result.z * cy
            );

        /*
         * Z rotation.
         */
        result =
            Vec3(
                result.x * cz -
                result.y * sz,

                result.x * sz +
                result.y * cz,

                result.z
            );

        return Normalize(result);
    }

    OBB CreateOBB(
        const TransformComponent& transform,
        const ColliderComponent& collider)
    {
        const Vec3 scale =
            transform.transform.scale;

        const Vec3 rotation =
            transform.transform.rotation;

        OBB box;

        box.center =
            transform.transform.position;

        box.halfExtents =
            Vec3(
                collider.width *
                std::abs(scale.x) *
                0.5f,

                collider.height *
                std::abs(scale.y) *
                0.5f,

                collider.depth *
                std::abs(scale.z) *
                0.5f
            );

        box.axisX =
            RotateVector(
                Vec3(
                    1.0f,
                    0.0f,
                    0.0f
                ),
                rotation
            );

        box.axisY =
            RotateVector(
                Vec3(
                    0.0f,
                    1.0f,
                    0.0f
                ),
                rotation
            );

        box.axisZ =
            RotateVector(
                Vec3(
                    0.0f,
                    0.0f,
                    1.0f
                ),
                rotation
            );

        return box;
    }

    float GetProjectedRadius(
        const OBB& box,
        const Vec3& axis)
    {
        return
            box.halfExtents.x *
            std::abs(
                Dot(
                    box.axisX,
                    axis
                )
            ) +

            box.halfExtents.y *
            std::abs(
                Dot(
                    box.axisY,
                    axis
                )
            ) +

            box.halfExtents.z *
            std::abs(
                Dot(
                    box.axisZ,
                    axis
                )
            );
    }

    void TestAxis(
        const OBB& a,
        const OBB& b,
        const Vec3& axis,
        float& smallestPenetration,
        Vec3& smallestAxis,
        bool& separated)
    {
        if (separated)
        {
            return;
        }

        const float axisLength =
            Length(axis);

        if (axisLength <= EPSILON)
        {
            return;
        }

        const Vec3 normalizedAxis =
            Normalize(axis);

        const Vec3 centerDifference(
            b.center.x - a.center.x,
            b.center.y - a.center.y,
            b.center.z - a.center.z
        );

        const float distance =
            std::abs(
                Dot(
                    centerDifference,
                    normalizedAxis
                )
            );

        const float radiusA =
            GetProjectedRadius(
                a,
                normalizedAxis
            );

        const float radiusB =
            GetProjectedRadius(
                b,
                normalizedAxis
            );

        const float penetration =
            radiusA +
            radiusB -
            distance;

        if (penetration < 0.0f)
        {
            separated = true;
            return;
        }

        if (penetration <
            smallestPenetration)
        {
            smallestPenetration =
                penetration;

            smallestAxis =
                normalizedAxis;
        }
    }

    CollisionResult GetOBBCollision(
        const OBB& a,
        const OBB& b)
    {
        CollisionResult result;

        float smallestPenetration =
            std::numeric_limits<float>::max();

        Vec3 smallestAxis(
            0.0f,
            0.0f,
            0.0f
        );

        bool separated = false;

        /*
         * Face normals from A.
         */
        TestAxis(
            a,
            b,
            a.axisX,
            smallestPenetration,
            smallestAxis,
            separated
        );

        TestAxis(
            a,
            b,
            a.axisY,
            smallestPenetration,
            smallestAxis,
            separated
        );

        TestAxis(
            a,
            b,
            a.axisZ,
            smallestPenetration,
            smallestAxis,
            separated
        );

        /*
         * Face normals from B.
         */
        TestAxis(
            a,
            b,
            b.axisX,
            smallestPenetration,
            smallestAxis,
            separated
        );

        TestAxis(
            a,
            b,
            b.axisY,
            smallestPenetration,
            smallestAxis,
            separated
        );

        TestAxis(
            a,
            b,
            b.axisZ,
            smallestPenetration,
            smallestAxis,
            separated
        );

        /*
         * Edge-to-edge axes.
         */
        const Vec3 aAxes[3] =
        {
            a.axisX,
            a.axisY,
            a.axisZ
        };

        const Vec3 bAxes[3] =
        {
            b.axisX,
            b.axisY,
            b.axisZ
        };

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                TestAxis(
                    a,
                    b,
                    Cross(
                        aAxes[i],
                        bAxes[j]
                    ),
                    smallestPenetration,
                    smallestAxis,
                    separated
                );

                if (separated)
                {
                    break;
                }
            }

            if (separated)
            {
                break;
            }
        }

        if (separated)
        {
            return result;
        }

        /*
         * Make the normal point from
         * the character toward the
         * other collider.
         */
        const Vec3 centerDifference(
            b.center.x - a.center.x,
            b.center.y - a.center.y,
            b.center.z - a.center.z
        );

        if (Dot(
            centerDifference,
            smallestAxis
        ) < 0.0f)
        {
            smallestAxis.x *= -1.0f;
            smallestAxis.y *= -1.0f;
            smallestAxis.z *= -1.0f;
        }

        result.collided = true;

        result.normal =
            smallestAxis;

        result.penetration =
            smallestPenetration;

        return result;
    }
}

void CollisionSystem::Update(
    Scene& scene)
{
    const std::vector<Entity>& entities =
        scene.GetEntities();

    for (const Entity& characterEntity :
        entities)
    {
        ColliderComponent* characterCollider =
            scene.GetComponent<
            ColliderComponent
            >(
                characterEntity
            );

        TransformComponent* characterTransform =
            scene.GetComponent<
            TransformComponent
            >(
                characterEntity
            );

        CharacterControllerComponent* controller =
            scene.GetComponent<
            CharacterControllerComponent
            >(
                characterEntity
            );

        if (characterCollider == nullptr ||
            !characterCollider->enabled ||
            characterTransform == nullptr ||
            controller == nullptr)
        {
            continue;
        }

        for (const Entity& otherEntity :
            entities)
        {
            if (otherEntity.GetID() ==
                characterEntity.GetID())
            {
                continue;
            }

            ColliderComponent* otherCollider =
                scene.GetComponent<
                ColliderComponent
                >(
                    otherEntity
                );

            TransformComponent* otherTransform =
                scene.GetComponent<
                TransformComponent
                >(
                    otherEntity
                );

            if (otherCollider == nullptr ||
                !otherCollider->enabled ||
                otherTransform == nullptr)
            {
                continue;
            }

            /*
             * Rebuild both OBBs every time.
             *
             * This means changing an entity's
             * position, rotation, or scale is
             * immediately reflected in collision.
             */
            const OBB characterBox =
                CreateOBB(
                    *characterTransform,
                    *characterCollider
                );

            const OBB otherBox =
                CreateOBB(
                    *otherTransform,
                    *otherCollider
                );

            const CollisionResult collision =
                GetOBBCollision(
                    characterBox,
                    otherBox
                );

            if (!collision.collided)
            {
                continue;
            }

            /*
             * Push the character away from
             * the actual rotated collider.
             */
            characterTransform->
                transform.position.x -=
                collision.normal.x *
                collision.penetration;

            characterTransform->
                transform.position.y -=
                collision.normal.y *
                collision.penetration;

            characterTransform->
                transform.position.z -=
                collision.normal.z *
                collision.penetration;

            /*
             * Determine whether this collision
             * represents standing on a surface
             * or hitting the underside.
             *
             * collision.normal points from the
             * character toward the collider.
             *
             * Therefore a downward normal means
             * the collider is below the player.
             */
            if (collision.normal.y <
                -0.5f)
            {
                controller->verticalVelocity =
                    0.0f;

                controller->grounded =
                    true;
            }
            else if (collision.normal.y >
                0.5f)
            {
                controller->verticalVelocity =
                    0.0f;

                controller->grounded =
                    false;
            }
        }
    }
}