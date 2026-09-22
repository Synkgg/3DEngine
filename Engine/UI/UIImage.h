#pragma once

#include "UIWidget.h"

#include <string>

class UIImage : public UIWidget
{
public:
    UIImage();

    const std::string& GetTexturePath() const;
    void SetTexturePath(const std::string& path);

private:
    std::string m_TexturePath;
};