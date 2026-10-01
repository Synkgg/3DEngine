#pragma once

#include "UIWidget.h"
#include <algorithm>

enum class UIProgressBarFillDirection
{
    LeftToRight,
    RightToLeft,
    TopToBottom,
    BottomToTop
};

class UIProgressBar : public UIWidget
{
public:
    UIProgressBar()
        : UIWidget(UIWidgetType::ProgressBar)
    {
        SetName("Progress Bar");
        SetColor(Vec4(0.08f, 0.09f, 0.11f, 1.0f));
        SetSize(Vec2(260.0f, 24.0f));
        SetCornerRadius(4.0f);
    }

    float GetPercent() const { return m_Percent; }
    void SetPercent(float value) { m_Percent = std::clamp(value, 0.0f, 1.0f); }

    const Vec4& GetFillColor() const { return m_FillColor; }
    void SetFillColor(const Vec4& value) { m_FillColor = value; }

    UIProgressBarFillDirection GetFillDirection() const { return m_FillDirection; }
    void SetFillDirection(UIProgressBarFillDirection value) { m_FillDirection = value; }

private:
    float m_Percent = 0.5f;
    Vec4 m_FillColor = Vec4(0.08f, 0.55f, 0.92f, 1.0f);
    UIProgressBarFillDirection m_FillDirection = UIProgressBarFillDirection::LeftToRight;
};
