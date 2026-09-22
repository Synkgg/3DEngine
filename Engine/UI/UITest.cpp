#include "UITest.h"

#include "UIWidget.h"
#include "UILayout.h"
#include "UICanvas.h"
#include "UIWidgetFactory.h"
#include "UIPanel.h"
#include "UIText.h"
#include "UIButton.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    bool NearlyEqual(
        float a,
        float b)
    {
        return std::abs(a - b) < 0.001f;
    }

    bool CheckRect(
        const UIRect& rect,
        float x,
        float y,
        float width,
        float height)
    {
        return
            NearlyEqual(rect.x, x) &&
            NearlyEqual(rect.y, y) &&
            NearlyEqual(rect.width, width) &&
            NearlyEqual(rect.height, height);
    }
}

bool UITest::Run()
{
    std::cout << "\n";
    std::cout << "====================\n";
    std::cout << "     UI TESTS\n";
    std::cout << "====================\n";

    UIRect parent;

    parent.x = 0.0f;
    parent.y = 0.0f;
    parent.width = 1920.0f;
    parent.height = 1080.0f;

    int passed = 0;
    int failed = 0;

    // --------------------------------------------------
    // Test 1: Top-left
    // --------------------------------------------------

    {
        UIWidget widget(
            UIWidgetType::Panel
        );

        widget.SetPosition(
            Vec2(100.0f, 50.0f)
        );

        widget.SetSize(
            Vec2(400.0f, 200.0f)
        );

        widget.SetAnchor(
            Vec2(0.0f, 0.0f)
        );

        widget.SetPivot(
            Vec2(0.0f, 0.0f)
        );

        const UIRect result =
            UILayout::Calculate(
                widget,
                parent
            );

        const bool success =
            CheckRect(
                result,
                100.0f,
                50.0f,
                400.0f,
                200.0f
            );

        if (success)
        {
            std::cout
                << "[PASS] Top-left layout\n";

            ++passed;
        }
        else
        {
            std::cout
                << "[FAIL] Top-left layout\n";

            ++failed;
        }
    }

    // --------------------------------------------------
    // Test 2: Center
    // --------------------------------------------------

    {
        UIWidget widget(
            UIWidgetType::Panel
        );

        widget.SetPosition(
            Vec2(0.0f, 0.0f)
        );

        widget.SetSize(
            Vec2(400.0f, 200.0f)
        );

        widget.SetAnchor(
            Vec2(0.5f, 0.5f)
        );

        widget.SetPivot(
            Vec2(0.5f, 0.5f)
        );

        const UIRect result =
            UILayout::Calculate(
                widget,
                parent
            );

        const bool success =
            CheckRect(
                result,
                760.0f,
                440.0f,
                400.0f,
                200.0f
            );

        if (success)
        {
            std::cout
                << "[PASS] Center layout\n";

            ++passed;
        }
        else
        {
            std::cout
                << "[FAIL] Center layout\n";

            ++failed;
        }
    }

    // --------------------------------------------------
    // Test 3: Bottom-right
    // --------------------------------------------------

    {
        UIWidget widget(
            UIWidgetType::Panel
        );

        widget.SetPosition(
            Vec2(-20.0f, -20.0f)
        );

        widget.SetSize(
            Vec2(300.0f, 100.0f)
        );

        widget.SetAnchor(
            Vec2(1.0f, 1.0f)
        );

        widget.SetPivot(
            Vec2(1.0f, 1.0f)
        );

        const UIRect result =
            UILayout::Calculate(
                widget,
                parent
            );

        const bool success =
            CheckRect(
                result,
                1600.0f,
                960.0f,
                300.0f,
                100.0f
            );

        if (success)
        {
            std::cout
                << "[PASS] Bottom-right layout\n";

            ++passed;
        }
        else
        {
            std::cout
                << "[FAIL] Bottom-right layout\n";

            ++failed;
        }
    }

    // --------------------------------------------------
    // Test 4: Offset from center
    // --------------------------------------------------

    {
        UIWidget widget(
            UIWidgetType::Panel
        );

        widget.SetPosition(
            Vec2(100.0f, -50.0f)
        );

        widget.SetSize(
            Vec2(200.0f, 100.0f)
        );

        widget.SetAnchor(
            Vec2(0.5f, 0.5f)
        );

        widget.SetPivot(
            Vec2(0.5f, 0.5f)
        );

        const UIRect result =
            UILayout::Calculate(
                widget,
                parent
            );

        const bool success =
            CheckRect(
                result,
                960.0f,
                440.0f,
                200.0f,
                100.0f
            );

        if (success)
        {
            std::cout
                << "[PASS] Center offset layout\n";

            ++passed;
        }
        else
        {
            std::cout
                << "[FAIL] Center offset layout\n";

            ++failed;
        }
    }

    // --------------------------------------------------
    // Test 5: Widget hierarchy
    // --------------------------------------------------

    {
        UICanvas canvas;

        UIWidget* root =
            canvas.GetRoot();

        std::unique_ptr<UIWidget> panel =
            UIWidgetFactory::Create(
                UIWidgetType::Panel
            );

        if (!panel)
        {
            std::cout
                << "[FAIL] Panel creation\n";

            ++failed;
        }
        else
        {
            panel->SetName("MainPanel");

            UIWidget* panelPtr =
                root->AddChild(
                    std::move(panel)
                );

            std::unique_ptr<UIWidget> title =
                UIWidgetFactory::Create(
                    UIWidgetType::Text
                );

            std::unique_ptr<UIWidget> button =
                UIWidgetFactory::Create(
                    UIWidgetType::Button
                );

            if (!title || !button)
            {
                std::cout
                    << "[FAIL] Child widget creation\n";

                ++failed;
            }
            else
            {
                title->SetName("Title");
                button->SetName("PlayButton");

                UIWidget* titlePtr =
                    panelPtr->AddChild(
                        std::move(title)
                    );

                UIWidget* buttonPtr =
                    panelPtr->AddChild(
                        std::move(button)
                    );

                const bool success =
                    root->GetChildren().size() == 1 &&
                    panelPtr->GetChildren().size() == 2 &&
                    titlePtr->GetParent() == panelPtr &&
                    buttonPtr->GetParent() == panelPtr &&
                    titlePtr->GetName() == "Title" &&
                    buttonPtr->GetName() == "PlayButton";

                if (success)
                {
                    std::cout
                        << "[PASS] Widget hierarchy\n";

                    ++passed;
                }
                else
                {
                    std::cout
                        << "[FAIL] Widget hierarchy\n";

                    ++failed;
                }
            }
        }
    }

    std::cout << "\n";
    std::cout
        << "UI Tests: "
        << passed
        << " passed, "
        << failed
        << " failed\n";

    std::cout << "====================\n";
    std::cout << "\n";

    return failed == 0;
}

void UITest::SetupCanvas(
    UICanvas& canvas)
{
    canvas.Clear();

    UIWidget* panel =
        canvas.GetRoot()->AddChild(
            UIWidgetFactory::Create(
                UIWidgetType::Panel
            )
        );

    if (!panel)
    {
        return;
    }

    panel->SetName(
        "MainPanel"
    );

    panel->SetPosition(
        Vec2(
            100.0f,
            100.0f
        )
    );

    panel->SetSize(
        Vec2(
            400.0f,
            200.0f
        )
    );

    panel->SetColor(
        Vec4(
            0.1f,
            0.3f,
            0.8f,
            1.0f
        )
    );

    UIWidget* childPanel =
        panel->AddChild(
            UIWidgetFactory::Create(
                UIWidgetType::Panel
            )
        );

    if (childPanel)
    {
        childPanel->SetName(
            "ChildPanel"
        );

        childPanel->SetPosition(
            Vec2(
                25.0f,
                25.0f
            )
        );

        childPanel->SetSize(
            Vec2(
                150.0f,
                100.0f
            )
        );

        childPanel->SetColor(
            Vec4(
                0.8f,
                0.2f,
                0.2f,
                1.0f
            )
        );
    }

    UIWidget* button =
        panel->AddChild(
            UIWidgetFactory::Create(
                UIWidgetType::Button
            )
        );

    if (button)
    {
        button->SetName(
            "PlayButton"
        );

        button->SetPosition(
            Vec2(
                220.0f,
                60.0f
            )
        );

        button->SetSize(
            Vec2(
                140.0f,
                60.0f
            )
        );

        button->SetColor(
            Vec4(
                0.2f,
                0.8f,
                0.3f,
                1.0f
            )
        );
    }

    UIWidget* title =
        panel->AddChild(
            UIWidgetFactory::Create(
                UIWidgetType::Text
            )
        );

    if (title)
    {
        UIText* text =
            static_cast<UIText*>(title);

        text->SetName(
            "Title"
        );

        text->SetText(
            "NEXUS"
        );

        text->SetFontSize(
            6.0f
        );

        text->SetPosition(
            Vec2(
                25.0f,
                140.0f
            )
        );

        text->SetSize(
            Vec2(
                200.0f,
                50.0f
            )
        );

        text->SetColor(
            Vec4(
                1.0f,
                1.0f,
                1.0f,
                1.0f
            )
        );
    }
}