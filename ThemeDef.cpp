#include "stdafx.h"
#include "ThemeDef.h"
#include "SublimeTextMenuColor.h"

namespace 
{
  std::wstring LayeredMember(int layer, const wchar_t* name)
  {
    std::wostringstream ss;
    ss << L"layer" << layer << L"." << name;
    return ss.str();
  }

  template <typename T, typename T2>
  std::vector<T> ArrayToVector(const rapidjson::GenericArray<true,T2>& arrayObj)
  {
    std::vector<T> ret;
    ret.reserve(arrayObj.Size());
    for (auto& it : arrayObj)
    {
      ret.push_back(it.Get<T>());
    }
    return ret;
  }

  template <typename T, typename T2>
  std::vector<T> ArrayToVector(const rapidjson::GenericValue<T2>& valueObj)
  {
    if (valueObj.IsArray())
    {
      return ArrayToVector<T>(valueObj.GetArray());
    }
    return std::vector<T>();
  }

  template <typename T>
  bool SkipItem(ThemeDef& themeDef, const T& themeItem)
  {
    auto themeItemObj = themeItem.GetObject();
    auto settings = themeItemObj.FindMember(L"settings");
    if (settings != themeItemObj.end() && settings->value.IsArray())
    {
      for (auto& setting : settings->value.GetArray())
      {
        if (setting.IsString())
        {
          if (!themeDef.GetSetting(setting.GetString()))
          {
            return true;
          }
        }
      }
    }
    return false;
  }

  template <typename T, typename T2>
  void FlattenItem(ThemeDef& themeDef, T& themeItem, const T2& parentList, rapidjson::MemoryPoolAllocator<>& pool)
  {
    auto themeItemObj = themeItem.GetObject();
    auto parentsMember = themeItemObj.FindMember(L"parents");
    if (parentsMember != themeItemObj.end() && parentsMember->value.IsArray())
    {
      auto currentAttrAtttr = themeItemObj.FindMember(L"attributes");
      std::vector<std::wstring> currentAttrTags;
      if (currentAttrAtttr != themeItemObj.end())
        currentAttrTags = ArrayToVector<std::wstring>(currentAttrAtttr->value);

      rapidjson::GenericValue<rapidjson::UTF16<>> tempCollectorItem(rapidjson::kObjectType);
      auto& tempCollectorObject = tempCollectorItem.GetObject();

      for (auto& parent : parentsMember->value.GetArray())
      {
        if (parent.IsObject())
        {
          auto classAttr = parent.GetObject().FindMember(L"class");
          if (classAttr != parent.GetObject().end())
          {
            auto attrAttr = parent.GetObject().FindMember(L"attributes");
            std::vector<std::wstring> attrTags;
            if (attrAttr != parent.GetObject().end())
              attrTags = ArrayToVector<std::wstring>(attrAttr->value);
            std::sort(attrTags.begin(), attrTags.end());

            for (auto a : attrTags)
            {
              if (std::find(currentAttrTags.begin(), currentAttrTags.end(), a) == currentAttrTags.end())
              {
                if (currentAttrAtttr == themeItemObj.end())
                {
                  themeItemObj.AddMember(L"attributes", rapidjson::GenericValue<rapidjson::UTF16<>>(rapidjson::kArrayType), pool);
                  currentAttrAtttr = themeItemObj.FindMember(L"attributes");
                }
                currentAttrAtttr->value.PushBack(rapidjson::GenericValue<rapidjson::UTF16<>>(a, pool), pool);
              }
            }

            bool foundMatch = false;
            for (auto& searchThemeItem : parentList)
            {
              if (!searchThemeItem.IsObject())
                continue;

              auto searchThemeItemObj = searchThemeItem.GetObject();
              auto searchClassMember = searchThemeItemObj.FindMember(L"class");
              if (searchClassMember == searchThemeItemObj.end())
                continue;

              if (searchClassMember->value == classAttr->value)
              {
                if (!SkipItem(themeDef, searchThemeItem))
                {
                  auto searchAttrAttr = searchThemeItemObj.FindMember(L"attributes");
                  std::vector<std::wstring> searchAttrTags;
                  if (searchAttrAttr != searchThemeItemObj.end())
                    searchAttrTags = ArrayToVector<std::wstring>(searchAttrAttr->value);
                  std::sort(searchAttrTags.begin(), searchAttrTags.end());

                  bool anyMismatched = false;
                  for (auto& a : searchAttrTags)
                  {
                    if (std::find(attrTags.begin(), attrTags.end(), a) == attrTags.end())
                    {
                      anyMismatched = true;
                      break;
                    }
                  }
                  if (!anyMismatched)
                  {
                    foundMatch = true;
                    FlattenItem(themeDef, searchThemeItem, parentList, pool);
                    auto parentThingObj = searchThemeItem.GetObject();
                    for (auto& parentMem : parentThingObj)
                    {
                      auto it = tempCollectorObject.FindMember(parentMem.name);
                      if (it == tempCollectorObject.end())
                      {
                        T tempValue(parentMem.value, pool);
                        T tempName(parentMem.name, pool);
                        tempCollectorObject.AddMember(tempName, tempValue, pool);
                      }
                      else
                      {
                        it->value = T(parentMem.value, pool);
                      }
                    }
                  }
                }
              }
            }

            if (!foundMatch)
            {
              std::wostringstream ss;
              ss << L"Can't find parent " << classAttr->value.GetString() << L" tags " << std::endl;
              for (auto i : attrTags)
              {
                ss << L"    " << i << std::endl;
              }
              g_PrintFunc(ss.str().c_str());
              OutputDebugStringW(ss.str().c_str());
            }
          }
        }
      }
   
      for (auto& parentMem : tempCollectorObject)
      {
        auto it = themeItemObj.FindMember(parentMem.name);
        if (it == themeItemObj.end())
        {
          T tempValue(parentMem.value, pool);
          T tempName(parentMem.name, pool);
          themeItemObj.AddMember(tempName, tempValue, pool);
        }
      }
    }
  }

  static double GammaMult(double v, double m)
  {
    double linearCol = pow(v / 255.0, 2.2);
    linearCol *= m;
    return pow(linearCol, 1.0 / 2.2) * 255.0;
  }

  template <typename T>
  bool ReadThemeItem(const T& themeItem, ThemeElement& dest, std::array<double,3>& disabledTint)
  {
    auto themeItemObj = themeItem.GetObject();
#if 0
    rapidjson::GenericStringBuffer<rapidjson::UTF16<>> buffer;

    buffer.Clear();

    rapidjson::PrettyWriter<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>, rapidjson::UTF16<>> writer(buffer);
    rapidjson::GenericDocument<rapidjson::UTF16<>> doc;
    rapidjson::GenericValue<rapidjson::UTF16<>> itemCopy(themeItem, doc.GetAllocator());
    doc.Set(itemCopy.GetObject());
    doc.Accept(writer);

    g_PrintFunc(buffer.GetString());
    OutputDebugStringW(buffer.GetString());

#endif
    auto member = themeItemObj.FindMember(L"content_margin");
    if (member != themeItemObj.end())
    {
        if (!member->value.IsArray())
        {
          g_PrintFunc(L"Can't understand content_margin");
        }
        else
        {
          auto arrayValue = member->value.GetArray();
          dest.contentMargins.cxRightWidth = arrayValue.Size() > 0 ? (int)arrayValue[0].GetFloat() : 0;
          dest.contentMargins.cxLeftWidth = arrayValue.Size() > 1 ? (int)arrayValue[1].GetFloat() : 0;
          dest.contentMargins.cyTopHeight = arrayValue.Size() > 2 ? (int)arrayValue[2].GetFloat() : 0;
          dest.contentMargins.cyBottomHeight = arrayValue.Size() > 3 ? (int)arrayValue[3].GetFloat() : 0;
        }
    }

    member = themeItemObj.FindMember(L"row_padding");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsArray())
      {
        g_PrintFunc(L"Can't understand row_padding");
      }
      else
      {
        auto arrayValue = member->value.GetArray();
        dest.rowPadding.X = arrayValue.Size() > 0 ? arrayValue[0].GetFloat() : 0;
        dest.rowPadding.Y = arrayValue.Size() > 1 ? arrayValue[1].GetFloat() : 0;
      }
    }

    member = themeItemObj.FindMember(L"shadow_offset");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsArray())
      {
        g_PrintFunc(L"Can't understand shadow_offset");
      }
      else
      {
        auto arrayValue = member->value.GetArray();
        dest.shadowOffset.X = arrayValue.Size() > 0 ? arrayValue[0].GetFloat() : 0;
        dest.shadowOffset.Y = arrayValue.Size() > 1 ? arrayValue[1].GetFloat() : 0;
      }
    }

    member = themeItemObj.FindMember(L"shadow_color");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsArray())
      {
        g_PrintFunc(L"Can't understand shadow_color");
      }
      else
      {
        auto arrayValue = member->value.GetArray();
        int rgb[4] = {
          arrayValue.Size() > 0 ? (int)arrayValue[0].GetFloat() : 0,
          arrayValue.Size() > 1 ? (int)arrayValue[1].GetFloat() : 0,
          arrayValue.Size() > 2 ? (int)arrayValue[2].GetFloat() : 0,
          arrayValue.Size() > 3 ? (int)arrayValue[3].GetFloat() : 255
        };
        dest.shadowColor = Gdiplus::Color(rgb[3], rgb[0], rgb[1], rgb[2]);
      }
    }

    member = themeItemObj.FindMember(L"color");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsArray())
      {
        g_PrintFunc(L"Can't understand color");
      }
      else
      {
        auto arrayValue = member->value.GetArray();
        int rgb[4] = {
          arrayValue.Size() > 0 ? (int)arrayValue[0].GetFloat() : 0,
          arrayValue.Size() > 1 ? (int)arrayValue[1].GetFloat() : 0,
          arrayValue.Size() > 2 ? (int)arrayValue[2].GetFloat() : 0,
          arrayValue.Size() > 3 ? (int)arrayValue[3].GetFloat() : 255
        };
        dest.textColor = Gdiplus::Color(rgb[3], rgb[0], rgb[1], rgb[2]);

        int rgbd[4] = {
          arrayValue.Size() > 0 ? (int)GammaMult(arrayValue[0].GetFloat() , disabledTint[0]) : 0,
          arrayValue.Size() > 1 ? (int)GammaMult(arrayValue[1].GetFloat() , disabledTint[1]) : 0,
          arrayValue.Size() > 2 ? (int)GammaMult(arrayValue[2].GetFloat() , disabledTint[2]) : 0,
          arrayValue.Size() > 3 ? (int)arrayValue[3].GetFloat() : 255
        };
        dest.textColorDisabled = Gdiplus::Color(rgbd[3], rgbd[0], rgbd[1], rgbd[2]);
      }
    }

    member = themeItemObj.FindMember(L"fg");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsArray())
      {
        g_PrintFunc(L"Can't understand fg");
      }
      else
      {
        auto arrayValue = member->value.GetArray();
        int rgb[4] = {
          arrayValue.Size() > 0 ? (int)arrayValue[0].GetFloat() : 0,
          arrayValue.Size() > 1 ? (int)arrayValue[1].GetFloat() : 0,
          arrayValue.Size() > 2 ? (int)arrayValue[2].GetFloat() : 0,
          arrayValue.Size() > 3 ? (int)arrayValue[3].GetFloat() : 255
        };
        dest.textColor = Gdiplus::Color(rgb[3], rgb[0], rgb[1], rgb[2]);
      }
    }

    member = themeItemObj.FindMember(L"font.size");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsNumber())
      {
        g_PrintFunc(L"Can't understand font.size");
      }
      else
      {
        dest.fontSize = member->value.GetFloat();
      }
    }

    member = themeItemObj.FindMember(L"font.bold");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsBool())
      {
        g_PrintFunc(L"Can't understand font.bold");
      }
      else
      {
        dest.fontBold = member->value.GetBool();
      }
    }

    member = themeItemObj.FindMember(L"font.italic");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsBool())
      {
        g_PrintFunc(L"Can't understand font.italic");
      }
      else
      {
        dest.fontItalic = member->value.GetBool();
      }
    }

    member = themeItemObj.FindMember(L"font.face");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsString())
      {
        g_PrintFunc(L"Can't understand font.face");
      }
      else
      {
        dest.fontFace = member->value.GetString();
      }
    }

    for (int layer = 0; layer < 4; layer++)
    {
      ThemeElement::Layer& newLayer = dest.layers[layer];

      auto member = themeItemObj.FindMember(LayeredMember(layer, L"texture"));
      if (member != themeItemObj.end())
      {
        if (member->value.IsString())
        {
          newLayer.texturePath = member->value.GetString();
          //g_PrintFunc(newLayer.texturePath.c_str());
        }
        else if (member->value.IsObject())
        {
          //TODO anim
          auto keyframesAttr = member->value.GetObject().FindMember(L"keyframes");
          if (keyframesAttr != member->value.GetObject().end() && keyframesAttr->value.IsArray())
          {
            auto& arrayObj = keyframesAttr->value.GetArray();
            auto& lastFrameVal = arrayObj[arrayObj.Size() - 1];
            if (lastFrameVal.IsString())
            {
              newLayer.texturePath = lastFrameVal.GetString();
              //g_PrintFunc(newLayer.texturePath.c_str());
            }
            else
            {
              g_PrintFunc(L"Can't understand texture object");
            }
          }
          else
          {
            g_PrintFunc(L"Can't understand texture object");
          }
        }
        else
        {
          g_PrintFunc(L"Can't understand texture");
        }
      }

      member = themeItemObj.FindMember(LayeredMember(layer,L"tint"));
      if (member != themeItemObj.end())
      {
        if (!member->value.IsArray())
        {
          g_PrintFunc(L"Can't understand tint");
        }
        else
        {
          auto arrayValue = member->value.GetArray();
          int rgb[3] = {
            arrayValue.Size() > 0 ? (int)arrayValue[0].GetFloat() : 0,
            arrayValue.Size() > 1 ? (int)arrayValue[1].GetFloat() : 0,
            arrayValue.Size() > 2 ? (int)arrayValue[2].GetFloat() : 0
          };
          newLayer.tint = Gdiplus::Color(newLayer.tint.GetA(), rgb[0], rgb[1], rgb[2]);
        }
      }

      member = themeItemObj.FindMember(LayeredMember(layer, L"opacity"));
      if (member != themeItemObj.end())
      {
        float opacity = 0.0f;
        if (member->value.IsNumber())
        {
          opacity = member->value.GetFloat();
        }
        else if (member->value.IsObject())
        {
          //TODO anim
          auto targetAttr = member->value.GetObject().FindMember(L"target");
          if (targetAttr != member->value.GetObject().end() && targetAttr->value.IsNumber())
          {
            opacity = targetAttr->value.GetFloat();
          }
          else
          {
            g_PrintFunc(L"Can't understand opacity object");
          }
        }
        else
        {
          g_PrintFunc(L"Can't understand opacity");
        }
        newLayer.tint = Gdiplus::Color((BYTE)(opacity * 255.0f), newLayer.tint.GetR(), newLayer.tint.GetG(), newLayer.tint.GetB());
      }

      member = themeItemObj.FindMember(LayeredMember(layer, L"inner_margin"));
      if (member != themeItemObj.end())
      {
        if (!member->value.IsArray())
        {
          g_PrintFunc(L"Can't understand inner_margin"); 
        }
        else
        {
          auto arrayValue = member->value.GetArray();
          newLayer.innerMargins.cxRightWidth = arrayValue.Size() > 0 ? (int)arrayValue[0].GetFloat() : 0;
          newLayer.innerMargins.cxLeftWidth = arrayValue.Size() > 1 ? (int)arrayValue[1].GetFloat() : 0;
          newLayer.innerMargins.cyTopHeight = arrayValue.Size() > 2 ? (int)arrayValue[2].GetFloat() : 0;
          newLayer.innerMargins.cyBottomHeight = arrayValue.Size() > 3 ? (int)arrayValue[3].GetFloat() : 0;
        }
      }
    }
    return true;
  }
}

bool ThemeDef::GetSetting(const wchar_t* setting)
{
  auto findIt = settingCache.find(setting);
  if (findIt != settingCache.end())
  {
    return findIt->second;
  }

  bool result;
  if (setting[0] == L'!')
  {
    result = !QueryBoolSetting(setting + 1);
  }
  else
  {
    result = QueryBoolSetting(setting);
  }

  //g_PrintFuncF(L"%s = %s", setting, result ? L"true" : L"false");

  settingCache[setting] = result;
  return result;
}

std::shared_ptr<Gdiplus::Bitmap> ThemeDef::GetBitmap(const wchar_t* imageName)
{
  std::wstring path = L"Packages/";
  path += imageName;

  auto findIt = bitmapCache.find(path);
  if (findIt != bitmapCache.end())
  {
    return findIt->second;
  }

  unsigned int dataSize = 0;
  unsigned char* dataPtr = nullptr;
  if (QueryBinaryResource(path.c_str(), &dataSize, &dataPtr))
  {
    CComPtr<IStream> imageData;
    imageData.Attach(SHCreateMemStream((const BYTE*)dataPtr, dataSize));
    MIDL_user_free(dataPtr);

    auto texture = std::make_shared<Gdiplus::Bitmap>(imageData);
    g_PrintFuncF(L"%s (%u) : %i x %i", imageName, dataSize, texture->GetWidth(), texture->GetHeight());
    bitmapCache[path] = texture;
    return texture;

  }
  return nullptr;
}

HBRUSH ThemeDef::GetBGBrush()
{
  if (bgBrush)
    return bgBrush;

  //for (auto& l : topLabelState[0].layers)
  //{
  //  if (l.opacity > 0)
  //  {
  //    bgBrush = CreateSolidBrush(l.tint.ToCOLORREF());
  //    bgBrushp = std::make_unique<Gdiplus::SolidBrush>(l.tint);
  //    break;
  //  }
  //}

  if (!bgBrush)
  {
    for (auto& s : topLabelState[0])
    {
      for (auto& l : s.layers)
      {
        if (l.tint.GetA() > 0)
        {
          bgBrush = CreateSolidBrush(l.tint.ToCOLORREF());
          bgBrushp = std::make_unique<Gdiplus::SolidBrush>(l.tint);
          return bgBrush;
        }
      }
    }
  }
  
  return bgBrush;
}

Gdiplus::Brush* ThemeDef::GetBGBrushGDIP()
{
  if (!bgBrushp)
  {
    GetBGBrush();
  }
  return bgBrushp.get();
}

static int IndexFromTags(const std::vector<std::wstring>& attrTags)
{
  int elementIndex = 0;
  for (auto& tag : attrTags)
  {
    if (tag == L"selected")
      elementIndex |= ThemeDef::SELECTED;
    else if (tag == L"expandable")
      elementIndex |= ThemeDef::EXPANDABLE;
    else if (tag == L"expanded")
      elementIndex |= ThemeDef::EXPANDED;
    else if (tag == L"hover")
      elementIndex |= ThemeDef::HOVER;
    else if (tag == L"transient")
      elementIndex |= ThemeDef::TRANSIENT;
  }
  return elementIndex;
}

ThemeDef::ThemeDef(const wchar_t* jsonData)
  : isValid(false), bgBrush(0), bgBrushp(nullptr)
  , useSelectedStateForHoverTop(true), useSelectedStateForHoverItem(false)
{
  std::vector<std::wstring> topBarElement = QueryStringArraySetting(L"menu_color_menu_bar_theme_elements", { L"tab_control", L"tab_label" });
  std::vector<std::wstring> itemElement = QueryStringArraySetting(L"menu_color_menu_item_theme_elements", { L"sidebar_tree", L"sidebar_label" });

  if (topBarElement.size() == 0 || itemElement.size() == 0)
  {
    return;
  }

  for (int pass = 0; pass < STATE_COUNT; pass++)
  {
    topLabelState[pass].resize(topBarElement.size());
    labelState[pass].resize(itemElement.size());
  }

  useSelectedStateForHoverTop = QueryBoolSetting(L"menu_color_menu_bar_use_selected_state_for_hover", useSelectedStateForHoverTop);
  useSelectedStateForHoverItem = QueryBoolSetting(L"menu_color_menu_item_use_selected_state_for_hover", useSelectedStateForHoverItem);

  auto disabledColorMultSetting = QueryNumberArraySetting(L"menu_color_disabled_color_mult");
  if (disabledColorMultSetting.size() >= 3)
  {
    std::copy_n(disabledColorMultSetting.begin(), 3, disabledColorMult.begin());
  }
  else
  {
    std::fill_n(disabledColorMult.begin(), 3, 0.45);
  }


	rapidjson::GenericDocument<rapidjson::UTF16<>> doc;
	rapidjson::ParseResult pr = doc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(jsonData);

  if (pr.IsError() || !doc.IsArray())
    return;

  for (auto& themeItem : doc.GetArray())
  {
    if (!themeItem.IsObject())
      continue;

    if (SkipItem(*this, themeItem))
      continue;

    FlattenItem(*this, themeItem, doc.GetArray(), doc.GetAllocator());
  }

  for (int pass = 0; pass < STATE_COUNT; pass++)
  {
    for (auto& themeItem : doc.GetArray())
    {
      if (!themeItem.IsObject())
        continue;

      auto themeItemObj = themeItem.GetObject();
      auto classMember = themeItemObj.FindMember(L"class");
      if (classMember == themeItemObj.end())
        continue;

      std::wstring className(classMember->value.GetString());

      if (SkipItem(*this, themeItem))
        continue;

      auto attrAttr = themeItemObj.FindMember(L"attributes");
      std::vector<std::wstring> attrTags;
      if (attrAttr != themeItemObj.end())
        attrTags = ArrayToVector<std::wstring>(attrAttr->value);
      std::sort(attrTags.begin(), attrTags.end());

      //if (std::find(attrTags.begin(), attrTags.end(), L"transient") != attrTags.end())
      //{
      //  //skip these
      //  continue;
      //}

      auto topBarElementItr = std::find(topBarElement.begin(), topBarElement.end(), className);
      auto itemElementItr = std::find(itemElement.begin(), itemElement.end(), className);

      
      if (topBarElementItr != topBarElement.end())
      {
        int elementIndex = IndexFromTags(attrTags);
        //if ((elementIndex & (~pass)) == 0 || elementIndex == 0)
        if (elementIndex == pass || elementIndex == 0)
        {
          if (!ReadThemeItem(themeItem, topLabelState[pass][topBarElementItr - topBarElement.begin()], disabledColorMult))
          {
            return;
          }
        }
      }
      
      if (itemElementItr != itemElement.end())
      {
        int elementIndex = IndexFromTags(attrTags);
        //if ((elementIndex & (~pass)) == 0 || elementIndex == 0)
        if (elementIndex == pass || elementIndex == 0)
        {
          if (!ReadThemeItem(themeItem, labelState[pass][itemElementItr - itemElement.begin()], disabledColorMult))
          {
            return;
          }
        }
      }
    }
  }

  bool bumpTopBarTextSize = QueryBoolSetting(L"menu_color_menu_bar_increase_text_size", true);

  //Do it now to prevent deadlocks later
  for (auto& e : labelState)
  {
    for (auto& e2 : e)
    {
      e2.ForceLoad(*this);
    }
  }
  for (auto& e : topLabelState)
  {
    for (auto& e2 : e)
    {
      if (bumpTopBarTextSize)
        e2.fontSize++;

      e2.ForceLoad(*this);

    }
  }

  auto bgColorOverride = QueryNumberArraySetting(L"menu_color_menu_bar_bg_color_override");
  if (bgColorOverride.size() >= 3)
  {
    Gdiplus::Color bgOverride((BYTE)bgColorOverride[0], (BYTE)bgColorOverride[1], (BYTE)bgColorOverride[2]);
    bgBrush = CreateSolidBrush(bgOverride.ToCOLORREF());
    bgBrushp = std::make_unique<Gdiplus::SolidBrush>(bgOverride);
  }

 
  GetBGBrush();
  isValid = true;
}


ThemeDef::~ThemeDef()
{
  if (bgBrush)
    DeleteObject(bgBrush);
}

static bool IsRootMenuItem(HMENU rootMenu, UINT itemID)
{
  int count = GetMenuItemCount(rootMenu);
  for (int i = 0; i < count; i++)
  {
    MENUITEMINFOW menuInfo;
    memset(&menuInfo, 0, sizeof(menuInfo));
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIIM_ID;
    GetMenuItemInfoW(rootMenu, i, TRUE, &menuInfo);
    if (menuInfo.wID == itemID)
    {
      return true;
    }
  }
  return false;
}

void ThemeDef::DrawItem(HWND hwnd, const LPDRAWITEMSTRUCT diStruct)
{
  HDC dc = diStruct->hDC;
  Gdiplus::Graphics graphics(dc);
  
  HMENU hmenu = GetMenu(hwnd);
  bool isRootMenu = IsRootMenuItem(hmenu, diStruct->itemID);

  MENUITEMINFOW menuItemInfo;
  memset(&menuItemInfo, 0, sizeof(menuItemInfo));
  menuItemInfo.cbSize = sizeof(menuItemInfo);
  menuItemInfo.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
  GetMenuItemInfoW(hmenu, diStruct->itemID, FALSE, &menuItemInfo);

  std::wstring labelText;
  if (menuItemInfo.cch > 0)
  {
    menuItemInfo.cch++;
    labelText.resize(menuItemInfo.cch);
    menuItemInfo.dwTypeData = &labelText[0];
    GetMenuItemInfoW(hmenu, diStruct->itemID, FALSE, &menuItemInfo);
    labelText.pop_back();
  }


  int elementIndex = 0;

  bool selectedForHover = isRootMenu ? useSelectedStateForHoverTop : useSelectedStateForHoverItem;

  if (menuItemInfo.fState & MFS_CHECKED)
    elementIndex |= EXPANDABLE;

  bool disabled = ((diStruct->itemState & ODS_DISABLED) | (diStruct->itemState & ODS_GRAYED)) != 0;
  bool isSeparator = (menuItemInfo.fType & MFT_SEPARATOR) != 0;

  if (!disabled && !isSeparator)
  {
    if (diStruct->itemState & ODS_SELECTED)
      elementIndex |= SELECTED;
    if (diStruct->itemState & ODS_HOTLIGHT)
      elementIndex |= (selectedForHover ? SELECTED : HOVER);
  }

  Gdiplus::Rect outerRect(diStruct->rcItem.left, diStruct->rcItem.top, diStruct->rcItem.right - diStruct->rcItem.left, diStruct->rcItem.bottom - diStruct->rcItem.top);
  //outerRect.Inflate(1, 1);
  auto& stateStack = isRootMenu ? topLabelState[elementIndex] : labelState[elementIndex];

  Gdiplus::PointF rowPadding(0, 0);
  for (size_t i = 0; i < stateStack.size(); i++)
  {
    auto& state = stateStack[i];
    state.DrawLayers(graphics, outerRect, false);
    rowPadding = rowPadding + state.rowPadding;
    if (i < (stateStack.size() - 1))
    {
      continue;
    }

    Gdiplus::PointF textOrigin(
      diStruct->rcItem.left + state.contentMargins.cxLeftWidth + rowPadding.X,
      diStruct->rcItem.top + state.contentMargins.cyTopHeight + rowPadding.Y
    );

    Gdiplus::PointF textEnd(
      diStruct->rcItem.right - state.contentMargins.cxRightWidth - rowPadding.X,
      diStruct->rcItem.bottom - state.contentMargins.cyBottomHeight - rowPadding.Y
    );

    if (isRootMenu)
    {
      textOrigin = Gdiplus::PointF((float)diStruct->rcItem.left, (float)diStruct->rcItem.top);
      textEnd = Gdiplus::PointF((float)diStruct->rcItem.right, (float)diStruct->rcItem.bottom);
    }

    Gdiplus::RectF textRect(textOrigin, Gdiplus::SizeF(textEnd.X - textOrigin.X, textEnd.Y - textOrigin.Y));

    Gdiplus::StringFormat format;
    format.SetHotkeyPrefix((diStruct->itemState & ODS_NOACCEL) ? Gdiplus::HotkeyPrefixHide : Gdiplus::HotkeyPrefixShow);

    Gdiplus::RectF shadowRect = textRect;
    shadowRect.Offset(state.shadowOffset);

    if (isSeparator)
    {
      if (shadowRect.Height < 1)
        shadowRect.Height = 1;
      if (textRect.Height < 1)
        textRect.Height = 1;
      graphics.FillRectangle(state.GetShadowBrush(), shadowRect);
      graphics.FillRectangle(state.GetTextBrush(disabled), textRect);
    }
    else
    {
      auto tabIndex = labelText.find(L'\t');
      if (tabIndex == std::wstring::npos)
      {
        if (isRootMenu)
          format.SetAlignment(Gdiplus::StringAlignmentCenter);

        graphics.DrawString(labelText.c_str(), (int)labelText.size(), state.GetFont(dc), shadowRect, &format, state.GetShadowBrush());
        graphics.DrawString(labelText.c_str(), (int)labelText.size(), state.GetFont(dc), textRect, &format, state.GetTextBrush(disabled));
      }
      else
      {
        Gdiplus::StringFormat formatRight(&format);
        formatRight.SetAlignment(Gdiplus::StringAlignmentFar);

        graphics.DrawString(labelText.c_str(), (int)tabIndex, state.GetFont(dc), shadowRect, &format, state.GetShadowBrush());
        graphics.DrawString(labelText.c_str() + tabIndex + 1, (int)(labelText.size() - tabIndex), state.GetFont(dc), shadowRect, &formatRight, state.GetShadowBrush());

        graphics.DrawString(labelText.c_str(), (int)tabIndex, state.GetFont(dc), textRect, &format, state.GetTextBrush(disabled));
        graphics.DrawString(labelText.c_str() + tabIndex + 1, (int)(labelText.size() - tabIndex), state.GetFont(dc), textRect, &formatRight, state.GetTextBrush(disabled));
      }
    }
  }
}

void ThemeDef::MeasureItem(HWND hwnd, LPMEASUREITEMSTRUCT miStruct)
{
  HMENU menu = GetMenu(hwnd);
  HDC dc = GetWindowDC(hwnd);
  {
    Gdiplus::Graphics graphics(dc);

    MENUITEMINFOW menuItemInfo;
    memset(&menuItemInfo, 0, sizeof(menuItemInfo));
    menuItemInfo.cbSize = sizeof(menuItemInfo);
    menuItemInfo.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
    GetMenuItemInfoW(menu, miStruct->itemID, FALSE, &menuItemInfo);

    std::wstring labelText;
    if (menuItemInfo.cch > 0)
    {
      menuItemInfo.cch++;
      labelText.resize(menuItemInfo.cch);
      menuItemInfo.dwTypeData = &labelText[0];
      GetMenuItemInfoW(menu, miStruct->itemID, FALSE, &menuItemInfo);
      labelText.pop_back();
    }

    bool isSeparator = (menuItemInfo.fType & MFT_SEPARATOR) != 0;
    bool isRootMenu = IsRootMenuItem(menu, miStruct->itemID);

    //outerRect.Inflate(1, 1);
    auto& stateStack = isRootMenu ? topLabelState[0] : labelState[0];

    Gdiplus::PointF rowPadding(0, 0);
    for (size_t i = 0; i < stateStack.size(); i++)
    {
      auto& state = stateStack[i];
      rowPadding = rowPadding + state.rowPadding;
    }

    auto& state = isRootMenu ? topLabelState[0].back() : labelState[0].back();

    Gdiplus::PointF textOrigin(
      state.contentMargins.cxLeftWidth + rowPadding.X,
      state.contentMargins.cyTopHeight + rowPadding.Y
    );

    if (isRootMenu)
    {
      textOrigin.X = 0;
      textOrigin.Y = 0;
    }

    if (isSeparator)
    {
      miStruct->itemWidth = (UINT)(textOrigin.X + state.contentMargins.cxRightWidth + rowPadding.X);
      float dpiScale = graphics.GetDpiY() / 96.0f;
      miStruct->itemHeight = (UINT)(textOrigin.Y + state.contentMargins.cyBottomHeight + rowPadding.Y + 1.0 * dpiScale);
    }
    else
    {
      Gdiplus::StringFormat format;
      format.SetHotkeyPrefix(Gdiplus::HotkeyPrefixShow);
      Gdiplus::REAL tabs[] = { 50 };
      format.SetTabStops(0, 1, tabs);

      Gdiplus::RectF bounds;
      graphics.MeasureString(labelText.c_str(), (int)labelText.size(), state.GetFont(dc), textOrigin, &format, &bounds);
      if (isRootMenu)
      {
        miStruct->itemWidth = (UINT)(bounds.GetRight());
        miStruct->itemHeight = (UINT)(bounds.GetBottom());
      }
      else
      {
        miStruct->itemWidth = (UINT)(bounds.GetRight() + state.contentMargins.cxRightWidth + rowPadding.X);
        miStruct->itemHeight = (UINT)(bounds.GetBottom() + state.contentMargins.cyBottomHeight + rowPadding.Y);
      }

    }
  }
  ReleaseDC(hwnd, dc);
}

Gdiplus::Font* ThemeElement::GetFont(HDC dc)
{
  if (font)
  {
    return font.get();
  }

  LOGFONTW fontInfo;
  memset(&fontInfo, 0, sizeof(fontInfo));
  fontInfo.lfHeight = -int((fontSize * (float)GetDeviceCaps(dc, LOGPIXELSY) / 96.0f) + 0.5f);
  fontInfo.lfWeight = fontBold ? FW_BOLD : FW_REGULAR;
  fontInfo.lfItalic = fontItalic ? TRUE : FALSE;
  fontInfo.lfCharSet = DEFAULT_CHARSET;
  fontInfo.lfOutPrecision = OUT_DEFAULT_PRECIS;
  fontInfo.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  fontInfo.lfQuality = ANTIALIASED_QUALITY;
  fontInfo.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  if (fontFace.size() > 0)
  {
    wcsncpy_s(fontInfo.lfFaceName, fontFace.c_str(), LF_FACESIZE);
  }
  else
  {
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    wcsncpy_s(fontInfo.lfFaceName, ncm.lfMenuFont.lfFaceName, LF_FACESIZE);
  }
  font = std::make_unique<Gdiplus::Font>(dc, &fontInfo);
  return font.get();
}

void ThemeElement::ForceLoad(ThemeDef& def)
{
  GetTextBrush(false);
  GetShadowBrush();
  for (auto& l : layers)
  {
    l.ForceLoad(def);
  }
}

void ThemeElement::Layer::ForceLoad(ThemeDef& def)
{
  if (texturePath.size() > 0 && !texture)
  {
    texture = def.GetBitmap(texturePath.c_str());
    auto extPos = texturePath.rfind(L'.');
    std::wstring hidpiTex2x = texturePath.substr(0, extPos) + L"@2x" + texturePath.substr(extPos);
    texture2x = def.GetBitmap(hidpiTex2x.c_str());
    std::wstring hidpiTex3x = texturePath.substr(0, extPos) + L"@3x" + texturePath.substr(extPos);
    texture3x = def.GetBitmap(hidpiTex3x.c_str());
  }
  GetTintBrush();
}

void ThemeElement::Layer::DrawLayer(Gdiplus::Graphics& graphics, const Gdiplus::Rect& rect, bool skipImage)
{
  if (tint.GetA() <= 0)
  {
    return;
  }

  Gdiplus::Image* layerImage = texture.get();
  
  float dpiScale = graphics.GetDpiY() / 96.0f;
  int imageSizeFactor = 1;
  if (dpiScale > 2.0f && texture3x)
  {
    layerImage = texture3x.get();
    imageSizeFactor = 3;
  }
  else if (dpiScale > 1.0f && texture2x)
  {
    layerImage = texture2x.get();
    imageSizeFactor = 2;
  }
  
  if (layerImage)
  {
    if (!skipImage)
    {
      Gdiplus::Point imageDims(layerImage->GetWidth(), layerImage->GetHeight());
      Gdiplus::Point srcPts[9];
      Gdiplus::Point srcPtsMax[9];

      int ptsAlongX[4] = { 0, innerMargins.cxLeftWidth * imageSizeFactor, imageDims.X - innerMargins.cxRightWidth * imageSizeFactor, imageDims.X };
      int ptsAlongY[4] = { 0, innerMargins.cyTopHeight * imageSizeFactor, imageDims.Y - innerMargins.cyBottomHeight * imageSizeFactor, imageDims.Y };

      int destPtsAlongX[4] = { rect.GetLeft(), rect.GetLeft() + innerMargins.cxLeftWidth * imageSizeFactor, rect.GetRight() - innerMargins.cxRightWidth * imageSizeFactor, rect.GetRight() };
      int destPtsAlongY[4] = { rect.GetTop(), rect.GetTop() + innerMargins.cyTopHeight * imageSizeFactor, rect.GetBottom() - innerMargins.cyBottomHeight * imageSizeFactor, rect.GetBottom() };
      for (int i = 1; i < 4; i++)
      {
        if (ptsAlongX[i] < ptsAlongX[i - 1])
          ptsAlongX[i] = ptsAlongX[i - 1];

        if (ptsAlongY[i] < ptsAlongY[i - 1])
          ptsAlongY[i] = ptsAlongY[i - 1];

        if (destPtsAlongX[i] < destPtsAlongX[i - 1])
          destPtsAlongX[i] = destPtsAlongX[i - 1];

        if (destPtsAlongY[i] < destPtsAlongY[i - 1])
          destPtsAlongY[i] = destPtsAlongY[i - 1];
      }

      Gdiplus::ImageAttributes ia;
      Gdiplus::ColorMatrix colorMatrix =
      {
        tint.GetR() / 255.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, tint.GetG() / 255.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, tint.GetB() / 255.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, tint.GetA() / 255.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
      ia.SetColorMatrix(&colorMatrix);
      for (int x = 0; x < 3; x++)
      {
        for (int y = 0; y < 3; y++)
        {
          Gdiplus::Rect srcRect(ptsAlongX[x], ptsAlongY[y], ptsAlongX[x + 1] - ptsAlongX[x], ptsAlongY[y + 1] - ptsAlongY[y]);
          Gdiplus::Rect destRect(destPtsAlongX[x], destPtsAlongY[y], destPtsAlongX[x + 1] - destPtsAlongX[x], destPtsAlongY[y + 1] - destPtsAlongY[y]);
          if (!srcRect.IsEmptyArea())
          {
            graphics.DrawImage(layerImage, destRect, srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height, Gdiplus::UnitPixel, &ia, nullptr, nullptr);
          }
        }
      }
    }
  }
  else
  {
    graphics.FillRectangle(GetTintBrush(), rect);
  }
}
