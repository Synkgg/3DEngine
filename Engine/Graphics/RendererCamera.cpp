#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Core/Logger.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <iostream>

void Renderer::RotateCamera(float yawDelta, float pitchDelta)
{
	m_Camera.Rotate(yawDelta, pitchDelta);
}

void Renderer::SetCameraMoveSpeed(float speed)
{
    m_Camera.SetMoveSpeed(speed);
}

void Renderer::MoveCamera(
	float forward,
	float right,
	float up,
	float deltaTime)
{
	m_Camera.Move(
		forward,
		right,
		up,
		deltaTime
	);
}

void Renderer::ResetCamera()
{
	m_Camera.Reset();
}

void Renderer::DrawSky()
{
    if (m_SkyVAO == 0) return;

    const Vec3 forward = m_Camera.GetForward();
    const Vec3 right = m_Camera.GetRight();
    const Vec3 up = Vec3::Cross(right, forward).Normalized();
    const float tanHalfFov = std::tan(m_Camera.GetFovDegrees() * 0.5f * 0.017453292519943295f);
    const float aspect = m_ViewportHeight > 0
        ? static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight)
        : 1.0f;

    glDisable(GL_DEPTH_TEST);
    m_SkyShader.Bind();
    m_SkyShader.SetVec3("u_CameraForward", forward.x, forward.y, forward.z);
    m_SkyShader.SetVec3("u_CameraRight", right.x, right.y, right.z);
    m_SkyShader.SetVec3("u_CameraUp", up.x, up.y, up.z);
    m_SkyShader.SetVec3("u_SunDirection", m_LightDirection.x, m_LightDirection.y, m_LightDirection.z);
    m_SkyShader.SetVec3("u_SunColor", m_LightColor.x, m_LightColor.y, m_LightColor.z);
    m_SkyShader.SetFloat("u_SunIntensity", m_LightIntensity);
    m_SkyShader.SetFloat("u_TanHalfFov", tanHalfFov);
    m_SkyShader.SetFloat("u_Aspect", aspect);
    m_SkyShader.SetFloat("u_SkyIntensity", m_RenderSettings.skyIntensity);
    glBindVertexArray(m_SkyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    m_SkyShader.Unbind();
    glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawGrid()
{
	Mat4 model =
		Mat4::Identity();

	Mat4 view =
		m_Camera.GetViewMatrix();

	Mat4 projection =
		m_Camera.GetProjectionMatrix();

	Mat4 transform =
		projection *
		view *
		model;

	m_GridShader.Bind();

	m_GridShader.SetMat4(
		"u_Transform",
		transform
	);

	m_GridShader.SetVec4(
		"u_Color",
		0.35f,
		0.35f,
		0.35f,
		1.0f
	);

	m_Grid.Draw();

	m_GridShader.Unbind();
}

Mat4 Renderer::GetCameraViewMatrix() const
{
	return m_Camera.GetViewMatrix();
}

Mat4 Renderer::GetCameraProjectionMatrix() const
{
	return m_Camera.GetProjectionMatrix();
}

Vec3 Renderer::GetCameraPosition() const
{
	return m_Camera.GetPosition();
}

void Renderer::SetCameraPosition(
	const Vec3& position)
{
	m_Camera.SetPosition(position);
}

float Renderer::GetCameraYaw() const
{
	return m_Camera.GetYaw();
}

float Renderer::GetCameraPitch() const
{
	return m_Camera.GetPitch();
}

Vec3 Renderer::GetCameraForward() const
{
	return m_Camera.GetForward();
}

Vec3 Renderer::GetCameraRight() const
{
	return m_Camera.GetRight();
}

void Renderer::SetCameraRotation(float yaw, float pitch)
{
	m_Camera.SetRotation(yaw, pitch);
}

Vec3 Renderer::GetCameraRayDirection(
	float ndcX,
	float ndcY) const
{
	return m_Camera.GetRayDirection(
		ndcX,
		ndcY
	);
}
