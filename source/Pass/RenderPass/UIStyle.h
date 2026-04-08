  #pragma once
#include <nuklear.h>

inline nk_style ApplyUIStyle(nk_style base, float s)
{
  nk_style style = base;

  // --- Color palette ---
  nk_color COL_BG          = nk_rgb( 18,  18,  18);
  nk_color COL_SURFACE     = nk_rgb( 28,  28,  28);
  nk_color COL_SURFACE_MID = nk_rgb( 36,  36,  36);
  nk_color COL_HOVER       = nk_rgb( 48,  48,  48);
  nk_color COL_ACTIVE      = nk_rgb( 58,  58,  58);
  nk_color COL_BORDER      = nk_rgb( 52,  52,  52);
  nk_color COL_ACCENT      = nk_rgb( 70, 130, 180);
  nk_color COL_ACCENT_HOV  = nk_rgb( 90, 150, 200);
  nk_color COL_TEXT        = nk_rgb(210, 210, 210);
  nk_color COL_TEXT_DIM    = nk_rgb(130, 130, 130);

  // --- Spacing ---
  style.window.padding                = nk_vec2(8.0f * s, 6.0f * s);
  style.window.spacing                = nk_vec2(6.0f * s, 4.0f * s);
  style.window.scrollbar_size         = nk_vec2(8.0f * s, 8.0f * s);
  style.window.min_row_height_padding = 6.0f * s;
  style.window.border                 = 1.0f;
  style.window.rounding               = 0.0f;

  style.button.padding                = nk_vec2(8.0f * s, 4.0f * s);
  style.button.rounding               = 2.0f;
  style.button.border                 = 1.0f;

  style.text.padding                  = nk_vec2(2.0f * s, 3.0f * s);

  style.checkbox.padding              = nk_vec2(3.0f * s, 3.0f * s);
  style.checkbox.border               = 1.0f;

  style.tab.padding                   = nk_vec2(6.0f * s, 4.0f * s);
  style.tab.indent                    = 12.0f * s;
  style.tab.border                    = 1.0f;

  style.property.padding              = nk_vec2(4.0f * s, 3.0f * s);
  style.property.border               = 1.0f;
  style.property.rounding             = 2.0f;

  style.combo.content_padding         = nk_vec2(4.0f * s, 3.0f * s);
  style.combo.button_padding          = nk_vec2(4.0f * s, 3.0f * s);
  style.combo.border                  = 1.0f;
  style.combo.rounding                = 2.0f;

  // --- Window ---
  style.window.background             = COL_BG;
  style.window.fixed_background       = nk_style_item_color(COL_BG);
  style.window.border_color           = COL_BORDER;

  // --- Header ---
  style.window.header.normal          = nk_style_item_color(COL_SURFACE_MID);
  style.window.header.hover           = nk_style_item_color(COL_HOVER);
  style.window.header.active          = nk_style_item_color(COL_ACTIVE);
  style.window.header.label_normal    = COL_TEXT;
  style.window.header.label_hover     = COL_TEXT;
  style.window.header.label_active    = COL_TEXT;
  style.window.header.padding         = nk_vec2(6.0f * s, 4.0f * s);
  style.window.header.label_padding   = nk_vec2(4.0f * s, 2.0f * s);

  // --- Text ---
  style.text.color                    = COL_TEXT;

  // --- Button ---
  style.button.normal                 = nk_style_item_color(COL_SURFACE);
  style.button.hover                  = nk_style_item_color(COL_HOVER);
  style.button.active                 = nk_style_item_color(COL_ACTIVE);
  style.button.border_color           = COL_BORDER;
  style.button.text_normal            = COL_TEXT;
  style.button.text_hover             = COL_TEXT;
  style.button.text_active            = COL_TEXT;

  // --- Checkbox ---
  style.checkbox.normal               = nk_style_item_color(COL_SURFACE);
  style.checkbox.hover                = nk_style_item_color(COL_HOVER);
  style.checkbox.active               = nk_style_item_color(COL_ACTIVE);
  style.checkbox.cursor_normal        = nk_style_item_color(COL_ACCENT);
  style.checkbox.cursor_hover         = nk_style_item_color(COL_ACCENT_HOV);
  style.checkbox.border_color         = COL_BORDER;
  style.checkbox.text_normal          = COL_TEXT;
  style.checkbox.text_hover           = COL_TEXT;
  style.checkbox.text_active          = COL_TEXT;

  // --- Property widget ---
  style.property.normal               = nk_style_item_color(COL_SURFACE);
  style.property.hover                = nk_style_item_color(COL_HOVER);
  style.property.active               = nk_style_item_color(COL_HOVER);
  style.property.border_color         = COL_BORDER;
  style.property.label_normal         = COL_TEXT_DIM;
  style.property.label_hover          = COL_TEXT;
  style.property.label_active         = COL_TEXT;
  style.property.inc_button.normal    = nk_style_item_color(COL_SURFACE);
  style.property.inc_button.hover     = nk_style_item_color(COL_ACCENT);
  style.property.inc_button.active    = nk_style_item_color(COL_ACCENT_HOV);
  style.property.inc_button.border_color = COL_BORDER;
  style.property.inc_button.text_normal  = COL_TEXT_DIM;
  style.property.inc_button.text_hover   = COL_TEXT;
  style.property.inc_button.text_active  = COL_TEXT;
  style.property.dec_button           = style.property.inc_button;
  style.property.edit.normal          = nk_style_item_color(COL_SURFACE);
  style.property.edit.hover           = nk_style_item_color(COL_SURFACE);
  style.property.edit.active          = nk_style_item_color(nk_rgb(70, 70, 80));
  style.property.edit.border_color    = COL_SURFACE;
  style.property.edit.text_normal     = COL_TEXT;
  style.property.edit.text_hover      = COL_TEXT;
  style.property.edit.text_active     = COL_TEXT;
  style.property.edit.cursor_normal   = COL_TEXT;
  style.property.edit.cursor_hover    = COL_TEXT;
  style.property.edit.cursor_text_normal = COL_BG;
  style.property.edit.cursor_text_hover  = COL_BG;

  // --- Tab (tree sections) ---
  style.tab.background                = nk_style_item_color(COL_SURFACE_MID);
  style.tab.border_color              = COL_BORDER;
  style.tab.text                      = COL_TEXT;
  style.tab.node_minimize_button.normal  = nk_style_item_color(COL_SURFACE_MID);
  style.tab.node_minimize_button.hover   = nk_style_item_color(COL_HOVER);
  style.tab.node_minimize_button.active  = nk_style_item_color(COL_ACTIVE);
  style.tab.node_minimize_button.text_normal = COL_TEXT_DIM;
  style.tab.node_minimize_button.text_hover  = COL_TEXT;
  style.tab.node_minimize_button.text_active = COL_TEXT;
  style.tab.node_maximize_button      = style.tab.node_minimize_button;

  // --- Combo ---
  style.combo.normal                  = nk_style_item_color(COL_SURFACE);
  style.combo.hover                   = nk_style_item_color(COL_HOVER);
  style.combo.active                  = nk_style_item_color(COL_ACTIVE);
  style.combo.border_color            = COL_BORDER;
  style.combo.label_normal            = COL_TEXT;
  style.combo.label_hover             = COL_TEXT;
  style.combo.label_active            = COL_TEXT;
  style.combo.symbol_normal           = COL_TEXT_DIM;
  style.combo.symbol_hover            = COL_TEXT;
  style.combo.symbol_active           = COL_TEXT;
  style.combo.button.normal           = nk_style_item_color(COL_SURFACE);
  style.combo.button.hover            = nk_style_item_color(COL_HOVER);
  style.combo.button.active           = nk_style_item_color(COL_ACTIVE);
  style.combo.button.text_normal      = COL_TEXT_DIM;
  style.combo.button.text_hover       = COL_TEXT;
  style.combo.button.text_active      = COL_TEXT;

  return style;
}

inline void UpdateDynamicUIStyle(nk_style& style, nk_color checkColor)
{
  style.checkbox.cursor_normal = nk_style_item_color(checkColor);
  style.checkbox.cursor_hover  = nk_style_item_color(checkColor);
}
