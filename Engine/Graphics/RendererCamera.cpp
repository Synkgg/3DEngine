#include "Renderer.h"
#include "RHI/RHI.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Core/Logger.h"
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
	InvalidateTemporalHistory();
}

void Renderer::DrawSky()
{
    const auto data=GetPostDrawData();
    const Velcryn::RHI::TextureHandle others[]={m_WhiteTexture,m_WhiteTexture,m_WhiteTexture};
    DrawFullscreen(m_SkyPipeline,data,m_WhiteTexture,others);
}

void Renderer::DrawGrid()
{
    if(auto* device=Velcryn::RHI::GetDevice()) {
        device->EndRendering();
        BeginSceneRendering(false,false);
        m_Grid.Draw(m_FrameViewProjection, m_Camera.GetPosition());
    }
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
	InvalidateTemporalHistory();
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
	InvalidateTemporalHistory();
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
