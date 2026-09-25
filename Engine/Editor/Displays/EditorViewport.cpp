#include "../Editor.h"

#include "../../Graphics/Renderer.h"
#include "../../Scene/Scene.h"
#include "../../Scene/Components/TransformComponent.h"
#include "../../Scene/Components/MeshComponent.h"

#include "../../Core/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float DegreesToRadians =
		0.0174532925f;

	constexpr float Epsilon =
		0.000001f;

	Vec3 RotateX(
		const Vec3& value,
		float angle)
	{
		const float c =
			std::cos(angle);

		const float s =
			std::sin(angle);

		return Vec3(
			value.x,

			value.y * c -
			value.z * s,

			value.y * s +
			value.z * c
		);
	}

	Vec3 RotateY(
		const Vec3& value,
		float angle)
	{
		const float c =
			std::cos(angle);

		const float s =
			std::sin(angle);

		return Vec3(
			value.x * c +
			value.z * s,

			value.y,

			-value.x * s +
			value.z * c
		);
	}

	Vec3 RotateZ(
		const Vec3& value,
		float angle)
	{
		const float c =
			std::cos(angle);

		const float s =
			std::sin(angle);

		return Vec3(
			value.x * c -
			value.y * s,

			value.x * s +
			value.y * c,

			value.z
		);
	}

	Vec3 TransformPointToLocal(
		const Vec3& point,
		const Vec3& position,
		const Vec3& rotation,
		const Vec3& scale)
	{
		Vec3 result =
			point - position;

		/*
		 * Inverse of:
		 *
		 * T * Rz * Ry * Rx * S
		 *
		 * Apply the inverse transforms
		 * in reverse order.
		 */
		result =
			RotateZ(
				result,
				-rotation.z
			);

		result =
			RotateY(
				result,
				-rotation.y
			);

		result =
			RotateX(
				result,
				-rotation.x
			);

		if (std::abs(scale.x) > Epsilon)
		{
			result.x /=
				scale.x;
		}

		if (std::abs(scale.y) > Epsilon)
		{
			result.y /=
				scale.y;
		}

		if (std::abs(scale.z) > Epsilon)
		{
			result.z /=
				scale.z;
		}

		return result;
	}

	Vec3 TransformDirectionToLocal(
		const Vec3& direction,
		const Vec3& rotation,
		const Vec3& scale)
	{
		Vec3 result =
			direction;

		result =
			RotateZ(
				result,
				-rotation.z
			);

		result =
			RotateY(
				result,
				-rotation.y
			);

		result =
			RotateX(
				result,
				-rotation.x
			);

		if (std::abs(scale.x) > Epsilon)
		{
			result.x /=
				scale.x;
		}

		if (std::abs(scale.y) > Epsilon)
		{
			result.y /=
				scale.y;
		}

		if (std::abs(scale.z) > Epsilon)
		{
			result.z /=
				scale.z;
		}

		return result;
	}

	bool RayIntersectsLocalBox(
		const Vec3& origin,
		const Vec3& direction,
		float& distance)
	{
		const Vec3 minBounds(
			-0.5f,
			-0.5f,
			-0.5f
		);

		const Vec3 maxBounds(
			0.5f,
			0.5f,
			0.5f
		);

		float tMin =
			0.0f;

		float tMax =
			std::numeric_limits<float>::max();

		const auto testAxis =
			[
				&,
				origin,
				direction
			](
				float originValue,
				float directionValue,
				float minValue,
				float maxValue)
			{
				if (std::abs(directionValue) <
					Epsilon)
				{
					if (originValue < minValue ||
						originValue > maxValue)
					{
						return false;
					}

					return true;
				}

				float t1 =
					(minValue - originValue) /
					directionValue;

				float t2 =
					(maxValue - originValue) /
					directionValue;

				if (t1 > t2)
				{
					std::swap(
						t1,
						t2
					);
				}

				tMin =
					std::max(
						tMin,
						t1
					);

				tMax =
					std::min(
						tMax,
						t2
					);

				return tMin <= tMax;
			};

		if (!testAxis(
			origin.x,
			direction.x,
			minBounds.x,
			maxBounds.x))
		{
			return false;
		}

		if (!testAxis(
			origin.y,
			direction.y,
			minBounds.y,
			maxBounds.y))
		{
			return false;
		}

		if (!testAxis(
			origin.z,
			direction.z,
			minBounds.z,
			maxBounds.z))
		{
			return false;
		}

		distance =
			tMin;

		return true;
	}

	bool RayIntersectsLocalSphere(
		const Vec3& origin,
		const Vec3& direction,
		float& distance)
	{
		const float radius =
			0.5f;

		const float a =
			Vec3::Dot(
				direction,
				direction
			);

		if (a <= Epsilon)
		{
			return false;
		}

		const float b =
			2.0f *
			Vec3::Dot(
				origin,
				direction
			);

		const float c =
			Vec3::Dot(
				origin,
				origin
			) -
			radius * radius;

		const float discriminant =
			b * b -
			4.0f * a * c;

		if (discriminant < 0.0f)
		{
			return false;
		}

		const float sqrtDiscriminant =
			std::sqrt(discriminant);

		float t0 =
			(-b -
				sqrtDiscriminant) /
			(2.0f * a);

		float t1 =
			(-b +
				sqrtDiscriminant) /
			(2.0f * a);

		if (t0 > t1)
		{
			std::swap(
				t0,
				t1
			);
		}

		if (t1 < 0.0f)
		{
			return false;
		}

		if (t0 >= 0.0f)
		{
			distance =
				t0;
		}
		else
		{
			distance =
				t1;
		}

		return true;
	}

	bool RayIntersectsLocalPlane(
		const Vec3& origin,
		const Vec3& direction,
		float& distance)
	{
		if (std::abs(direction.y) <
			Epsilon)
		{
			return false;
		}

		const float t =
			-origin.y /
			direction.y;

		if (t < 0.0f)
		{
			return false;
		}

		const float x =
			origin.x +
			direction.x * t;

		const float z =
			origin.z +
			direction.z * t;

		if (x < -0.5f ||
			x > 0.5f ||
			z < -0.5f ||
			z > 0.5f)
		{
			return false;
		}

		distance =
			t;

		return true;
	}

	bool RayIntersectsLocalCylinder(
		const Vec3& origin,
		const Vec3& direction,
		float& distance)
	{
		const float radius =
			0.5f;

		const float halfHeight =
			0.5f;

		bool hit =
			false;

		float closest =
			std::numeric_limits<float>::max();

		/*
		 * Cylinder side.
		 */
		const float a =
			direction.x *
			direction.x +
			direction.z *
			direction.z;

		if (std::abs(a) > Epsilon)
		{
			const float b =
				2.0f *
				(
					origin.x *
					direction.x +
					origin.z *
					direction.z
					);

			const float c =
				origin.x *
				origin.x +
				origin.z *
				origin.z -
				radius * radius;

			const float discriminant =
				b * b -
				4.0f * a * c;

			if (discriminant >= 0.0f)
			{
				const float sqrtDiscriminant =
					std::sqrt(discriminant);

				float t0 =
					(-b -
						sqrtDiscriminant) /
					(2.0f * a);

				float t1 =
					(-b +
						sqrtDiscriminant) /
					(2.0f * a);

				if (t0 > t1)
				{
					std::swap(
						t0,
						t1
					);
				}

				if (t0 >= 0.0f)
				{
					const float y =
						origin.y +
						direction.y * t0;

					if (y >= -halfHeight &&
						y <= halfHeight)
					{
						closest =
							std::min(
								closest,
								t0
							);

						hit = true;
					}
				}

				if (t1 >= 0.0f)
				{
					const float y =
						origin.y +
						direction.y * t1;

					if (y >= -halfHeight &&
						y <= halfHeight)
					{
						closest =
							std::min(
								closest,
								t1
							);

						hit = true;
					}
				}
			}
		}

		/*
		 * Cylinder caps.
		 */
		if (std::abs(direction.y) >
			Epsilon)
		{
			const float bottomT =
				(-halfHeight -
					origin.y) /
				direction.y;

			if (bottomT >= 0.0f)
			{
				const float x =
					origin.x +
					direction.x *
					bottomT;

				const float z =
					origin.z +
					direction.z *
					bottomT;

				if (x * x +
					z * z <=
					radius * radius)
				{
					closest =
						std::min(
							closest,
							bottomT
						);

					hit = true;
				}
			}

			const float topT =
				(halfHeight -
					origin.y) /
				direction.y;

			if (topT >= 0.0f)
			{
				const float x =
					origin.x +
					direction.x *
					topT;

				const float z =
					origin.z +
					direction.z *
					topT;

				if (x * x +
					z * z <=
					radius * radius)
				{
					closest =
						std::min(
							closest,
							topT
						);

					hit = true;
				}
			}
		}

		if (!hit)
		{
			return false;
		}

		distance =
			closest;

		return true;
	}
}

void Editor::RenderViewport(
	Renderer& renderer,
	Scene& scene)
{
	ImGui::Begin("Scene");

	// m_ViewportPosition/m_ViewportSize represent the actual game image,
	// not the surrounding ImGui window. They are assigned after the
	// viewport toolbar when the image rectangle is known.

	// Scene-local transport: keep play controls attached to the viewport so
	// the rest of the editor stays visually quiet and the scene remains primary.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.125f, 0.130f, 0.138f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.205f, 0.215f, 0.228f, 1.0f));
	ImGui::BeginChild("ViewportToolbar", ImVec2(0.0f, 42.0f), ImGuiChildFlags_Borders);

	if (ImGui::BeginCombo("##ViewMode", "Perspective"))
	{
		ImGui::Selectable("Perspective", true);
		ImGui::TextDisabled("Orthographic views are planned.");
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::BeginCombo("##ShadingMode", "Lit"))
	{
		ImGui::Selectable("Lit", true);
		ImGui::TextDisabled("Debug shading modes are planned.");
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Grid", &m_ShowGrid);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(86.0f);
	ImGui::DragFloat("Speed", &m_EditorCameraSpeed, 0.25f, 0.5f, 40.0f, "%.1f");

	const float transportWidth = 108.0f;
	ImGui::SameLine();
	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - transportWidth) * 0.5f));

	if (!m_Playing)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.38f, 0.56f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.49f, 0.70f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.58f, 0.82f, 1.0f));
		if (ImGui::Button("  PLAY  ", ImVec2(108.0f, 30.0f)))
		{
			m_Playing = true;
			m_SelectedEntity = Entity();
			m_NameEditEntityID = 0;
			m_NameEditBuffer[0] = '\0';
			Logger::Info("Play mode started.");
		}
		ImGui::PopStyleColor(3);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.52f, 0.12f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.17f, 0.24f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.22f, 0.30f, 1.0f));
		if (ImGui::Button("  STOP  ", ImVec2(108.0f, 30.0f)))
			StopPlaying();
		ImGui::PopStyleColor(3);
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !m_Playing)
		{
			m_SelectedEntity = Entity();
			m_NameEditEntityID = 0;
			m_NameEditBuffer[0] = '\0';
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);

	m_ViewportHovered =
		ImGui::IsWindowHovered();

	ImVec2 viewportSize =
		ImGui::GetContentRegionAvail();

	if (viewportSize.x <= 0.0f ||
		viewportSize.y <= 0.0f)
	{
		ImGui::End();
		return;
	}

	/*
 * Keep the 3D render target matched to the
 * actual ImGui scene viewport.
 */
	const ImGuiIO& io =
		ImGui::GetIO();

	const float scaleX =
		io.DisplayFramebufferScale.x > 0.0f
		? io.DisplayFramebufferScale.x
		: 1.0f;

	const float scaleY =
		io.DisplayFramebufferScale.y > 0.0f
		? io.DisplayFramebufferScale.y
		: 1.0f;

	const unsigned int renderWidth =
		static_cast<unsigned int>(
			std::max(
				1.0f,
				std::round(
					viewportSize.x *
					scaleX
				)
			)
			);

	const unsigned int renderHeight =
		static_cast<unsigned int>(
			std::max(
				1.0f,
				std::round(
					viewportSize.y *
					scaleY
				)
			)
			);

	renderer.ResizeViewport(
		renderWidth,
		renderHeight
	);

	ImVec2 viewportPosition =
		ImGui::GetCursorScreenPos();

	// Store the exact rectangle used by ImGui::Image. Runtime UI input,
	// overlays and crosshairs must all use this same coordinate space.
	m_ViewportPosition = viewportPosition;
	m_ViewportSize = viewportSize;

	ImTextureID textureID =
		(ImTextureID)(
			intptr_t(
				renderer.GetViewportTexture()
			)
			);

	ImGui::Image(
		textureID,
		viewportSize,
		ImVec2(0.0f, 1.0f),
		ImVec2(1.0f, 0.0f)
	);

	bool viewportImageHovered =
		ImGui::IsItemHovered();

	/*
 * Transform toolbar overlay
 */
	bool transformToolbarHovered = false;

	const float toolbarWidth =
		225.0f;

	const float toolbarHeight =
		36.0f;

	const float toolbarPadding =
		10.0f;

	ImGui::SetCursorScreenPos(
		ImVec2(
			viewportPosition.x +
			viewportSize.x -
			toolbarWidth -
			toolbarPadding,

			viewportPosition.y +
			toolbarPadding
		)
	);

	ImGui::PushStyleVar(
		ImGuiStyleVar_ItemSpacing,
		ImVec2(4.0f, 0.0f)
	);

	ImGui::PushStyleVar(
		ImGuiStyleVar_WindowPadding,
		ImVec2(4.0f, 2.0f)
	);

	ImGui::PushStyleColor(
		ImGuiCol_ChildBg,
		ImVec4(0.105f, 0.110f, 0.118f, 0.94f)
	);

	ImGui::PushStyleColor(
		ImGuiCol_Button,
		ImVec4(0.155f, 0.162f, 0.172f, 0.94f)
	);

	ImGui::PushStyleColor(
		ImGuiCol_ButtonHovered,
		ImVec4(0.18f, 0.205f, 0.225f, 0.96f)
	);

	ImGui::PushStyleColor(
		ImGuiCol_ButtonActive,
		ImVec4(0.12f, 0.45f, 0.66f, 0.96f)
	);

	ImGui::BeginChild(
		"TransformToolbarOverlay",
		ImVec2(
			toolbarWidth,
			toolbarHeight
		),
		ImGuiChildFlags_Borders,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse
	);

	auto drawGizmoButton =
		[&](
			const char* label,
			ImGuizmo::OPERATION operation)
		{
			const bool active =
				m_GizmoOperation ==
				operation;

			if (active)
			{
				ImGui::PushStyleColor(
					ImGuiCol_Button,
					ImVec4(0.12f, 0.35f, 0.52f, 0.96f)
				);
			}

			if (ImGui::Button(label))
			{
				m_GizmoOperation =
					operation;
			}

			if (active)
			{
				ImGui::PopStyleColor();
			}
		};

	drawGizmoButton(
		"Move",
		ImGuizmo::TRANSLATE
	);

	ImGui::SameLine();

	drawGizmoButton(
		"Rotate",
		ImGuizmo::ROTATE
	);

	ImGui::SameLine();

	drawGizmoButton(
		"Scale",
		ImGuizmo::SCALE
	);

	ImGui::SameLine();

	if (ImGui::Button(
		m_GizmoMode ==
		ImGuizmo::LOCAL
		? "Local"
		: "World"))
	{
		m_GizmoMode =
			m_GizmoMode ==
			ImGuizmo::LOCAL
			? ImGuizmo::WORLD
			: ImGuizmo::LOCAL;
	}

	transformToolbarHovered =
		ImGui::IsWindowHovered();

	ImGui::EndChild();

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);

	/*
	 * Gizmo
	 */
	TransformComponent* transform =
		m_SelectedEntity.IsValid()
		? scene.GetComponent<
		TransformComponent
		>(m_SelectedEntity)
		: nullptr;

	if (!m_Playing && transform != nullptr)
	{
		Mat4 model =
			transform->transform.GetMatrix();

		Mat4 view =
			renderer.GetCameraViewMatrix();

		Mat4 projection =
			renderer.GetCameraProjectionMatrix();

		ImGuizmo::SetDrawlist();

		ImGuizmo::SetRect(
			viewportPosition.x,
			viewportPosition.y,
			viewportSize.x,
			viewportSize.y
		);

		ImGuizmo::SetOrthographic(false);

		ImGuizmo::Manipulate(
			view.elements,
			projection.elements,
			m_GizmoOperation,
			m_GizmoMode,
			model.elements
		);

		if (ImGuizmo::IsUsing())
		{
			float translation[3];
			float rotation[3];
			float scale[3];

			ImGuizmo::DecomposeMatrixToComponents(
				model.elements,
				translation,
				rotation,
				scale
			);

			transform->transform.position =
				Vec3(
					translation[0],
					translation[1],
					translation[2]
				);

			transform->transform.rotation =
				Vec3(
					rotation[0] *
					DegreesToRadians,
					rotation[1] *
					DegreesToRadians,
					rotation[2] *
					DegreesToRadians
				);

			transform->transform.scale =
				Vec3(
					scale[0],
					scale[1],
					scale[2]
				);
		}
	}

	/*
 * Viewport picking
 */
	const ImVec2 mousePosition =
		ImGui::GetMousePos();

	const bool mouseOverViewport =
		mousePosition.x >= viewportPosition.x &&
		mousePosition.x <=
		viewportPosition.x +
		viewportSize.x &&
		mousePosition.y >= viewportPosition.y &&
		mousePosition.y <=
		viewportPosition.y +
		viewportSize.y;

	const float toolbarX =
		viewportPosition.x +
		viewportSize.x -
		toolbarWidth -
		toolbarPadding;

	const float toolbarY =
		viewportPosition.y +
		toolbarPadding;

	const bool mouseOverToolbar =
		mousePosition.x >= toolbarX &&
		mousePosition.x <=
		toolbarX + toolbarWidth &&
		mousePosition.y >= toolbarY &&
		mousePosition.y <=
		toolbarY + toolbarHeight;

	if (!m_Playing &&
		mouseOverViewport &&
		!mouseOverToolbar &&
		ImGui::IsMouseClicked(
			ImGuiMouseButton_Left) &&
		!ImGuizmo::IsUsing())
	{
		const float localMouseX =
			mousePosition.x -
			viewportPosition.x;

		const float localMouseY =
			mousePosition.y -
			viewportPosition.y;

		const float ndcX =
			(localMouseX /
				viewportSize.x) *
			2.0f -
			1.0f;

		const float ndcY =
			1.0f -
			(localMouseY /
				viewportSize.y) *
			2.0f;

		const Entity pickedEntity =
			PickEntity(
				renderer,
				scene,
				ndcX,
				ndcY
			);

		if (pickedEntity.IsValid())
		{
			m_SelectedEntity =
				pickedEntity;
		}
		else
		{
			m_SelectedEntity =
				Entity();

			m_NameEditEntityID =
				0;

			m_NameEditBuffer[0] =
				'\0';
		}
	}

	ImGui::End();
}

Entity Editor::PickEntity(
	Renderer& renderer,
	Scene& scene,
	float ndcX,
	float ndcY)
{
	const Vec3 rayOrigin =
		renderer.GetCameraPosition();

	const Vec3 rayDirection =
		renderer.GetCameraRayDirection(
			ndcX,
			ndcY
		);

	Entity closestEntity;

	float closestDistance =
		std::numeric_limits<float>::max();

	for (const Entity& entity :
		scene.GetEntities())
	{
		TransformComponent* transform =
			scene.GetComponent<
			TransformComponent
			>(entity);

		if (transform == nullptr)
		{
			continue;
		}

		MeshComponent* mesh =
			scene.GetComponent<
			MeshComponent
			>(entity);

		if (mesh == nullptr ||
			mesh->primitive ==
			PrimitiveType::None)
		{
			continue;
		}

		/*
		 * This must match Application.cpp,
		 * where the mesh transform is created.
		 */
		const Vec3 position =
			transform->transform.position +
			mesh->offset;

		const Vec3 rotation(
			transform->transform.rotation.x +
			mesh->rotation.x,

			transform->transform.rotation.y +
			mesh->rotation.y,

			transform->transform.rotation.z +
			mesh->rotation.z
		);

		const Vec3 scale =
			transform->transform.scale;

		if (std::abs(scale.x) <= Epsilon ||
			std::abs(scale.y) <= Epsilon ||
			std::abs(scale.z) <= Epsilon)
		{
			continue;
		}

		/*
		 * Convert the world-space mouse ray
		 * into the mesh's local space.
		 */
		const Vec3 localOrigin =
			TransformPointToLocal(
				rayOrigin,
				position,
				rotation,
				scale
			);

		const Vec3 localDirection =
			TransformDirectionToLocal(
				rayDirection,
				rotation,
				scale
			);

		float hitDistance =
			0.0f;

		bool hit =
			false;

		switch (mesh->primitive)
		{
		case PrimitiveType::Cube:
			hit =
				RayIntersectsLocalBox(
					localOrigin,
					localDirection,
					hitDistance
				);
			break;

		case PrimitiveType::Sphere:
			hit =
				RayIntersectsLocalSphere(
					localOrigin,
					localDirection,
					hitDistance
				);
			break;

		case PrimitiveType::Plane:
			hit =
				RayIntersectsLocalPlane(
					localOrigin,
					localDirection,
					hitDistance
				);
			break;

		case PrimitiveType::Cylinder:
			hit =
				RayIntersectsLocalCylinder(
					localOrigin,
					localDirection,
					hitDistance
				);
			break;

		case PrimitiveType::None:
			continue;
		}

		if (!hit ||
			hitDistance < 0.0f)
		{
			continue;
		}

		/*
		 * Because the local ray was created from
		 * the original world ray without normalizing
		 * it afterward, hitDistance is still the
		 * original world-ray parameter.
		 */
		if (hitDistance <
			closestDistance)
		{
			closestDistance =
				hitDistance;

			closestEntity =
				entity;
		}
	}

	return closestEntity;
}