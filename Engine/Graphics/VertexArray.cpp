#include "VertexArray.h"

#include <glad/gl.h>

VertexArray::VertexArray()
	: m_RendererID(0)
{
}

VertexArray::~VertexArray()
{
	Shutdown();
}

bool VertexArray::Initialize()
{
	glGenVertexArrays(1, &m_RendererID);

	if (m_RendererID == 0)
	{
		return false;
	}

	return true;
}

void VertexArray::Bind()
{
	glBindVertexArray(m_RendererID);
}

void VertexArray::Unbind()
{
	glBindVertexArray(0);
}

void VertexArray::Shutdown()
{
	if (m_RendererID != 0)
	{
		glDeleteVertexArrays(1, &m_RendererID);
		m_RendererID = 0;
	}
}