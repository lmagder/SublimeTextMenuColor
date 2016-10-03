#pragma once

class ThemeDef;

struct ThemeElement
{
  struct Layer
  {
    Gdiplus::Color tint;
    float opacity;
    MARGINS innerMargins;
    std::wstring texturePath;
    std::shared_ptr<Gdiplus::Bitmap> texture;
    std::unique_ptr<Gdiplus::Brush> tintBrush;

    Layer() :tint(RGB(255, 255, 255)), opacity(0.0f)
    {
      memset(&innerMargins, 0, sizeof(innerMargins));
    }

    ~Layer()
    {
    }

    Gdiplus::Brush* GetTintBrush()
    {
      if (!tintBrush)
      {
        tintBrush = std::make_unique<Gdiplus::SolidBrush>(Gdiplus::Color((BYTE)(opacity * 255.0f), tint.GetR(), tint.GetG(), tint.GetB()));
      }
      return tintBrush.get();
    }
    Gdiplus::Bitmap* GetTexture();

    void ForceLoad(ThemeDef& def);

    void DrawLayer(Gdiplus::Graphics& graphics, const Gdiplus::Rect& rect, bool skipImage);
  };

	Gdiplus::Color textColor;
  Gdiplus::Color shadowColor;
  Gdiplus::PointF shadowOffset;
  Gdiplus::PointF rowPadding;
	MARGINS contentMargins;
  bool fontBold;
  bool fontItalic;
  float fontSize;
  std::wstring fontFace;
  std::unique_ptr<Gdiplus::Font> font;
  std::unique_ptr<Gdiplus::Brush> textBrush, shadowBrush;
	
	std::array<Layer,4> layers;

	ThemeElement() : textColor(RGB(255, 255, 255)), shadowColor(0), font(nullptr),
    fontBold(false), fontItalic(false), fontSize(10.0f)
	{
		memset(&shadowOffset, 0, sizeof(shadowOffset));
    memset(&rowPadding, 0, sizeof(rowPadding));
		memset(&contentMargins, 0, sizeof(contentMargins));
	}

	~ThemeElement()
	{
	}

  Gdiplus::Font* GetFont(HDC dc);

  Gdiplus::Brush* GetTextBrush()
  {
    if (!textBrush)
    {
      textBrush = std::make_unique<Gdiplus::SolidBrush>(textColor);
    }
    return textBrush.get();
  }

  Gdiplus::Brush* GetShadowBrush()
  {
    if (!shadowBrush)
    {
      shadowBrush = std::make_unique<Gdiplus::SolidBrush>(shadowColor);
    }
    return shadowBrush.get();
  }

  void ForceLoad(ThemeDef& def);
  void DrawLayers(Gdiplus::Graphics& graphics, const Gdiplus::Rect& rect, bool skipImage)
  {
    for (auto& l : layers)
    {
      l.DrawLayer(graphics, rect, skipImage);
    }
  }
};

class ThemeDef
{
	ThemeElement containerElement;
  ThemeElement topContainerElement;
	enum
	{
		SELECTED = 1,
		HOVER = 2,
		EXPANDED = 4,
		EXPANDABLE = 8,
		STATE_COUNT = 16
	};
	ThemeElement labelState[STATE_COUNT];
  ThemeElement topLabelState[STATE_COUNT];
  std::unordered_map<std::wstring, bool> settingCache;
  std::unordered_map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> bitmapCache;
  HBRUSH bgBrush;
  bool isValid;

public:
	ThemeDef(const wchar_t* jsonData);
	~ThemeDef();

	void DrawItem(HWND hwnd, const LPDRAWITEMSTRUCT diStruct);
	void MeasureItem(HWND hwnd, LPMEASUREITEMSTRUCT miStruct);

  bool IsValid() const { return isValid; }
  bool GetSetting(const wchar_t* setting);
  std::shared_ptr<Gdiplus::Bitmap> GetBitmap(const wchar_t* imageName);

  HBRUSH GetBGBrush();
};

