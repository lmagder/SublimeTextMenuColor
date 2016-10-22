#pragma once

class ThemeDef;

struct ThemeElement
{
  struct Layer
  {
    Gdiplus::Color tint;
    MARGINS innerMargins;
    std::wstring texturePath;
    std::shared_ptr<Gdiplus::Bitmap> texture;
    std::shared_ptr<Gdiplus::Bitmap> texture2x;
    std::shared_ptr<Gdiplus::Bitmap> texture3x;
    std::unique_ptr<Gdiplus::Brush> tintBrush;

    Layer() :tint(0, 255, 255, 255), texture(nullptr), tintBrush(nullptr)
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
        tintBrush = std::make_unique<Gdiplus::SolidBrush>(tint);
      }
      return tintBrush.get();
    }

    void ForceLoad(ThemeDef& def);

    void DrawLayer(Gdiplus::Graphics& graphics, const Gdiplus::Rect& rect, bool skipImage);
  };

	Gdiplus::Color textColor;
  Gdiplus::Color textColorDisabled;
  Gdiplus::Color shadowColor;
  Gdiplus::PointF shadowOffset;
  Gdiplus::PointF rowPadding;
	MARGINS contentMargins;
  bool fontBold;
  bool fontItalic;
  float fontSize;
  std::wstring fontFace;
  std::unique_ptr<Gdiplus::Font> font;
  std::unique_ptr<Gdiplus::Brush> textBrush, shadowBrush, textBrushDisabled;
	
	std::array<Layer,4> layers;

	ThemeElement() : textColor(255, 255, 255), shadowColor(0), shadowOffset(0,0)
    , rowPadding(0,0), fontBold(false), fontItalic(false), fontSize(10.0f)
    , font(nullptr), textBrush(nullptr), shadowBrush(nullptr)
	{
		memset(&contentMargins, 0, sizeof(contentMargins));
	}

	~ThemeElement()
	{
	}

  Gdiplus::Font* GetFont(HDC dc);

  Gdiplus::Brush* GetTextBrush(bool disabled)
  {
    if (!textBrush)
    {
      textBrush = std::make_unique<Gdiplus::SolidBrush>(textColor);
    }
    if (!textBrushDisabled)
    {
      textBrushDisabled = std::make_unique<Gdiplus::SolidBrush>(textColorDisabled);
    }
    return disabled ? textBrushDisabled.get() : textBrush.get();
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
public:
	enum
	{
		SELECTED = 1,
		HOVER = 2,
		EXPANDED = 4,
		EXPANDABLE = 8,
    TRANSIENT = 16,
		STATE_COUNT = 32
	};
private:
	std::array<std::deque<ThemeElement>, STATE_COUNT> labelState;
  std::array<std::deque<ThemeElement>, STATE_COUNT> topLabelState;


  std::unordered_map<std::wstring, bool> settingCache;
  std::unordered_map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> bitmapCache;
  HBRUSH bgBrush;
  std::unique_ptr<Gdiplus::Brush> bgBrushp;
  bool isValid;
  bool useSelectedStateForHoverTop;
  bool useSelectedStateForHoverItem;
  std::array<double, 3> disabledColorMult;

public:
	ThemeDef(const wchar_t* jsonData);
	~ThemeDef();

	void DrawItem(HWND hwnd, const LPDRAWITEMSTRUCT diStruct);
	void MeasureItem(HWND hwnd, LPMEASUREITEMSTRUCT miStruct);

  bool IsValid() const { return isValid; }
  bool GetSetting(const wchar_t* setting);
  std::shared_ptr<Gdiplus::Bitmap> GetBitmap(const wchar_t* imageName);

  HBRUSH GetBGBrush();
  Gdiplus::Brush* GetBGBrushGDIP();
};

