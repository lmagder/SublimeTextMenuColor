#pragma once

struct ThemeElement
{
	COLORREF textColor;
	COLORREF shadowColor;
	POINT shadowOffset;
	HFONT font;
	MARGINS contentMargins;
	struct Layer
	{
		COLORREF tint;
		float opacity;
		MARGINS innerMargins;
		HBITMAP texture;

		Layer() :tint(RGB(255,255,255)), opacity(1.0f), texture(0)
		{
			memset(&innerMargins, 0, sizeof(innerMargins));
		}

	};
	std::vector<Layer> layers;

	ThemeElement() : textColor(RGB(255, 255, 255)), shadowColor(0), font(0)
	{
		memset(&shadowOffset, 0, sizeof(shadowOffset));
		memset(&contentMargins, 0, sizeof(contentMargins));
	}

	~ThemeElement()
	{
		if (font)
		{
			DeleteObject(font);
		}
		for (auto l : layers)
		{
			if (l.texture)
			{
				DeleteObject(l.texture);
			}
		}
	}
};

class ThemeDef
{
	ThemeElement containerElement;
	enum
	{
		SELECTED = 1,
		HOVER = 2,
		EXPANDED = 4,
		EXPANDABLE = 8,
		STATE_COUNT = 16
	};
	ThemeElement labelState[STATE_COUNT];
public:
	ThemeDef(const wchar_t* jsonData);
	~ThemeDef();

	void DrawItem(HWND hwnd, const LPDRAWITEMSTRUCT diStruct);
	void MeasureItem(HWND hwnd, LPMEASUREITEMSTRUCT miStruct);
};

