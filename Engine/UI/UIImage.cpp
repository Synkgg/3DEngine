#include "UIImage.h"

UIImage::UIImage()
    : UIWidget(UIWidgetType::Image)
{
    SetName("Image");
}

const std::string& UIImage::GetTexturePath() const
{
    return m_TexturePath;
}

void UIImage::SetTexturePath(
    const std::string& path)
{
    m_TexturePath = path;
}