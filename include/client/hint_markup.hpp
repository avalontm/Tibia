#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::client {

struct HintTextStyle {
    int font_number{2};
    int red{224};
    int green{224};
    int blue{224};
    int indent{};
    int horizontal_space{};
};

struct HintMarkupRun {
    std::wstring text;
    HintTextStyle style;
    std::optional<int> skin_id;
};

struct HintParagraph {
    std::vector<HintMarkupRun> runs;
    int spacing_before{};
    int spacing_after{};
};

struct HintLineRun {
    std::wstring text;
    HintTextStyle style;
    int x_offset{};
    std::optional<int> skin_id;
};

struct HintLine {
    std::vector<HintLineRun> runs;
    int spacing_before{};
    int spacing_after{};
};

using HintTextMeasure = std::function<int(std::wstring_view, int)>;
using HintUnitScale = std::function<int(int)>;

[[nodiscard]] std::vector<HintParagraph> parse_hint_markup(
    std::wstring_view markup);
[[nodiscard]] std::vector<HintLine> layout_hint_markup(
    const std::vector<HintParagraph>& paragraphs, int max_width,
    const HintTextMeasure& measure, const HintUnitScale& scale);

}
