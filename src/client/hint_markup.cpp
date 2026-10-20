#include "client/hint_markup.hpp"
#include "client/ui_skins_792.hpp"

#include <algorithm>
#include <cwctype>
#include <limits>
#include <string>

namespace tibia::client {
namespace {

std::wstring lower(std::wstring_view value) {
    std::wstring result(value);
    std::transform(result.begin(), result.end(), result.begin(),
        [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return result;
}

bool parse_integer(std::wstring_view value, int& result) {
    if (value.empty()) return false;
    int parsed{};
    for (const wchar_t character : value) {
        if (character < L'0' || character > L'9') return false;
        if (parsed > (std::numeric_limits<int>::max() - (character - L'0')) / 10)
            return false;
        parsed = parsed * 10 + (character - L'0');
    }
    result = parsed;
    return true;
}

void append_run(HintParagraph& paragraph, std::wstring_view text,
    const HintTextStyle& style) {
    if (text.empty()) return;
    paragraph.runs.push_back({std::wstring(text), style, std::nullopt});
}

bool is_wrap_whitespace(wchar_t character) {
    return character == L' ' || character == L'\f' || character == L'\n' ||
        character == L'\r' || character == L'\t' || character == L'\v';
}

void append_line_run(HintLine& line, std::wstring_view text,
    const HintTextStyle& style, int x_offset, std::optional<int> skin_id = std::nullopt) {
    if (text.empty() && !skin_id) return;
    if (!line.runs.empty()) {
        auto& previous = line.runs.back();
        if (previous.style.font_number == style.font_number &&
            previous.style.red == style.red && previous.style.green == style.green &&
            previous.style.blue == style.blue && previous.style.indent == style.indent &&
            previous.style.horizontal_space == style.horizontal_space &&
            previous.x_offset == x_offset && !previous.skin_id && !skin_id) {
            previous.text.append(text);
            return;
        }
    }
    line.runs.push_back({std::wstring(text), style, x_offset, skin_id});
}

}

std::vector<HintParagraph> parse_hint_markup(std::wstring_view markup) {
    std::vector<HintParagraph> paragraphs;
    HintTextStyle style;
    HintParagraph current;
    auto flush = [&](int spacing) {
        current.spacing_after = spacing;
        paragraphs.push_back(std::move(current));
        current = HintParagraph{};
    };

    std::size_t position{};
    while (position < markup.size()) {
        if (markup[position] != L'<') {
            const auto next = markup.find(L'<', position);
            const auto end = next == std::wstring_view::npos ? markup.size() : next;
            auto text_start = position;
            for (auto index = position; index < end; ++index) {
                if (markup[index] != L'\n') continue;
                append_run(current, markup.substr(text_start, index - text_start), style);
                flush(0);
                text_start = index + 1;
            }
            append_run(current, markup.substr(text_start, end - text_start), style);
            position = end;
            continue;
        }

        const auto close = markup.find(L'>', position + 1);
        if (close == std::wstring_view::npos) {
            append_run(current, markup.substr(position), style);
            break;
        }

        const auto raw = markup.substr(position + 1, close - position - 1);
        const auto tag = lower(raw);
        bool recognized = true;
        if (tag == L"b") {
            style.red = 255;
            style.green = 128;
            style.blue = 128;
        } else if (tag == L"n") {
            style.red = 224;
            style.green = 224;
            style.blue = 224;
        } else if (tag == L"h") {
            style.red = 224;
            style.green = 224;
            style.blue = 0;
        } else if (tag.starts_with(L"br=")) {
            int spacing{};
            if (parse_integer(std::wstring_view(tag).substr(3), spacing)) flush(spacing);
            else recognized = false;
        } else if (tag.starts_with(L"indent=")) {
            int indent{};
            if (parse_integer(std::wstring_view(tag).substr(7), indent)) style.indent = indent;
            else recognized = false;
        } else if (tag.starts_with(L"hspace=")) {
            int spacing{};
            if (parse_integer(std::wstring_view(tag).substr(7), spacing))
                style.horizontal_space = spacing;
            else recognized = false;
        } else if (tag.starts_with(L"font=")) {
            int font{};
            if (parse_integer(std::wstring_view(tag).substr(5), font)) style.font_number = font;
            else recognized = false;
        } else if (tag.starts_with(L"color=")) {
            int color{};
            if (parse_integer(std::wstring_view(tag).substr(6), color) && color < 216) {
                style.red = (color / 36) * 51;
                style.green = ((color / 6) % 6) * 51;
                style.blue = (color % 6) * 51;
            } else recognized = false;
        } else if (tag.starts_with(L"red=")) {
            int red{};
            if (parse_integer(std::wstring_view(tag).substr(4), red)) style.red = red;
            else recognized = false;
        } else if (tag.starts_with(L"green=")) {
            int green{};
            if (parse_integer(std::wstring_view(tag).substr(6), green)) style.green = green;
            else recognized = false;
        } else if (tag.starts_with(L"blue=")) {
            int blue{};
            if (parse_integer(std::wstring_view(tag).substr(5), blue)) style.blue = blue;
            else recognized = false;
        } else if (tag.starts_with(L"skin=")) {
            int skin_id{};
            if (parse_integer(std::wstring_view(tag).substr(5), skin_id) &&
                skin_id >= 0 && skin_id < static_cast<int>(ui_skins.size())) {
                current.runs.push_back({{}, style, skin_id});
            } else recognized = false;
        } else {
            recognized = false;
        }
        if (!recognized) append_run(current, markup.substr(position, close - position + 1), style);
        position = close + 1;
    }
    if (!current.runs.empty() || paragraphs.empty())
        paragraphs.push_back(std::move(current));
    return paragraphs;
}

std::vector<HintLine> layout_hint_markup(
    const std::vector<HintParagraph>& paragraphs, int max_width,
    const HintTextMeasure& measure, const HintUnitScale& scale) {
    std::vector<HintLine> lines;
    for (const auto& paragraph : paragraphs) {
        HintLine line;
        line.spacing_before = scale(paragraph.spacing_before);
        int x{};
        int line_indent{};
        auto flush_line = [&]() {
            lines.push_back(std::move(line));
            line = HintLine{};
            x = line_indent;
        };
        for (const auto& run : paragraph.runs) {
            const int indent = scale(run.style.indent);
            if (indent != line_indent) {
                x = indent;
                line_indent = indent;
            }
            if (run.skin_id) {
                const auto& skin = ui_skins[static_cast<std::size_t>(*run.skin_id)];
                const int width = scale(skin.width);
                append_line_run(line, {}, run.style, x, run.skin_id);
                x += width;
                continue;
            }
            std::size_t begin{};
            while (begin < run.text.size()) {
                std::size_t last_whitespace = std::wstring::npos;
                std::size_t scan = begin;
                bool wrapped{};
                for (; scan < run.text.size(); ++scan) {
                    if (is_wrap_whitespace(run.text[scan])) last_whitespace = scan;
                    const std::wstring_view prefix(run.text.data() + begin, scan - begin + 1);
                    if (max_width > 0 && x + measure(prefix, run.style.font_number) > max_width) {
                        const auto split = last_whitespace == std::wstring::npos
                            ? scan : last_whitespace;
                        const auto text_length = split - begin;
                        if (text_length > 0) {
                            const std::wstring_view text(run.text.data() + begin, text_length);
                            append_line_run(line, text, run.style, x);
                            x += measure(text, run.style.font_number);
                        }
                        flush_line();
                        begin = split + (last_whitespace == std::wstring::npos ? 0 : 1);
                        wrapped = true;
                        break;
                    }
                }
                if (!wrapped) {
                    const std::wstring_view text(run.text.data() + begin,
                        run.text.size() - begin);
                    append_line_run(line, text, run.style, x);
                    x += measure(text, run.style.font_number);
                    begin = run.text.size();
                }
            }
        }
        line.spacing_after = scale(paragraph.spacing_after);
        if (!line.runs.empty() || paragraph.spacing_after > 0 || lines.empty())
            lines.push_back(std::move(line));
    }
    return lines;
}

}
