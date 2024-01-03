#pragma once

#include "UILocalized.hpp"
#define _S(_LITERAL)    (const char*)u8##_LITERAL
#define _L(_TEXT) UILocalized::get_localized_text(_TEXT)

class UILocalized {
public:
    static void add_localized_font(float m_font_size);
    static void init_loc();
    static const char* get_localized_text(const char* text);
};