#pragma once
#include "UIWidget.h"
#include <algorithm>
class UISlider : public UIWidget { public: UISlider():UIWidget(UIWidgetType::Slider){SetName("Slider");} float GetValue()const{return m_Value;} void SetValue(float v){m_Value=std::clamp(v,0.0f,1.0f);} const Vec4& GetFillColor()const{return m_FillColor;} void SetFillColor(const Vec4& c){m_FillColor=c;} const Vec4& GetHandleColor()const{return m_HandleColor;} void SetHandleColor(const Vec4& c){m_HandleColor=c;} private: float m_Value=1.0f; Vec4 m_FillColor=Vec4(0.15f,0.65f,0.90f,1.0f); Vec4 m_HandleColor=Vec4(0.92f,0.96f,1.0f,1.0f);};
