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

  template <typename T>
  bool ReadThemeItem(const T& themeItem, ThemeElement& dest)
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
          dest.contentMargins.cxRightWidth = arrayValue.Size() > 0 ? arrayValue[0].GetInt() : 0;
          dest.contentMargins.cxLeftWidth = arrayValue.Size() > 1 ? arrayValue[1].GetInt() : 0;
          dest.contentMargins.cyTopHeight = arrayValue.Size() > 2 ? arrayValue[2].GetInt() : 0;
          dest.contentMargins.cyBottomHeight = arrayValue.Size() > 3 ? arrayValue[3].GetInt() : 0;
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
          arrayValue.Size() > 0 ? arrayValue[0].GetInt() : 0,
          arrayValue.Size() > 1 ? arrayValue[1].GetInt() : 0,
          arrayValue.Size() > 2 ? arrayValue[2].GetInt() : 0,
          arrayValue.Size() > 3 ? arrayValue[3].GetInt() : 255
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
          arrayValue.Size() > 0 ? arrayValue[0].GetInt() : 0,
          arrayValue.Size() > 1 ? arrayValue[1].GetInt() : 0,
          arrayValue.Size() > 2 ? arrayValue[2].GetInt() : 0,
          arrayValue.Size() > 3 ? arrayValue[3].GetInt() : 255
        };
        dest.textColor = Gdiplus::Color(rgb[3], rgb[0], rgb[1], rgb[2]);
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
          arrayValue.Size() > 0 ? arrayValue[0].GetInt() : 0,
          arrayValue.Size() > 1 ? arrayValue[1].GetInt() : 0,
          arrayValue.Size() > 2 ? arrayValue[2].GetInt() : 0,
          arrayValue.Size() > 3 ? arrayValue[3].GetInt() : 255
        };
        dest.textColor = Gdiplus::Color(rgb[3], rgb[0], rgb[1], rgb[2]);
      }
    }

    member = themeItemObj.FindMember(L"font.size");
    if (member != themeItemObj.end())
    {
      if (!member->value.IsFloat())
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
          g_PrintFunc(newLayer.texturePath.c_str());
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
              g_PrintFunc(newLayer.texturePath.c_str());
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
            arrayValue.Size() > 0 ? arrayValue[0].GetInt() : 0,
            arrayValue.Size() > 1 ? arrayValue[1].GetInt() : 0,
            arrayValue.Size() > 2 ? arrayValue[2].GetInt() : 0
          };
          newLayer.tint = Gdiplus::Color(255, rgb[0], rgb[1], rgb[2]);
        }
      }

      member = themeItemObj.FindMember(LayeredMember(layer, L"opacity"));
      if (member != themeItemObj.end())
      {
        if (member->value.IsFloat())
        {
          newLayer.opacity = member->value.GetFloat();
        }
        else if (member->value.IsObject())
        {
          //TODO anim
          auto targetAttr = member->value.GetObject().FindMember(L"target");
          if (targetAttr != member->value.GetObject().end() && targetAttr->value.IsFloat())
          {
            newLayer.opacity = targetAttr->value.GetFloat();
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
          newLayer.innerMargins.cxRightWidth = arrayValue.Size() > 0 ? arrayValue[0].GetInt() : 0;
          newLayer.innerMargins.cxLeftWidth = arrayValue.Size() > 1 ? arrayValue[1].GetInt() : 0;
          newLayer.innerMargins.cyTopHeight = arrayValue.Size() > 2 ? arrayValue[2].GetInt() : 0;
          newLayer.innerMargins.cyBottomHeight = arrayValue.Size() > 3 ? arrayValue[3].GetInt() : 0;
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
  bool result = QueryBoolSetting(setting);
  settingCache[setting] = result;
  return result;
}

std::shared_ptr<Gdiplus::Bitmap> ThemeDef::GetBitmap(const wchar_t* imageName)
{
  auto findIt = bitmapCache.find(imageName);
  if (findIt != bitmapCache.end())
  {
    return findIt->second;
  }

  std::wstring path = L"Packages/";
  path += imageName;
  std::vector<uint8_t> pngData = QueryBinaryResource(path.c_str());
  CComPtr<IStream> imageData;
  imageData.Attach(SHCreateMemStream(pngData.data(), (UINT)pngData.size()));

  auto texture = std::make_shared<Gdiplus::Bitmap>(imageData);
  bitmapCache[imageName] = texture;
  return texture;
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
  //    break;
  //  }
  //}

  if (!bgBrush)
  {
    for (auto& l : topContainerElement.layers)
    {
      if (l.opacity > 0)
      {
        bgBrush = CreateSolidBrush(l.tint.ToCOLORREF());
        break;
      }
    }
  }
  
  return bgBrush;
}

ThemeDef::ThemeDef(const wchar_t* jsonData)
  : isValid(false), bgBrush(0)
{
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

      if (std::find(attrTags.begin(), attrTags.end(), L"transient") != attrTags.end())
      {
        //skip these
        continue;
      }

      if (className == L"sidebar_tree" && pass == 0)
      {
        if (!ReadThemeItem(themeItem, containerElement))
        {
          return;
        }
      }
      else if (className == L"tab_control" && pass == 0)
      {
        if (!ReadThemeItem(themeItem, topContainerElement))
        {
          return;
        }
      }
      else if (className == L"sidebar_label")
      {
        int elementIndex = 0;
        for (auto& tag : attrTags)
        {
          if (tag == L"selected")
            elementIndex |= SELECTED;
          else if (tag == L"expandable")
            elementIndex |= EXPANDABLE;
          else if (tag == L"expanded")
            elementIndex |= EXPANDED;
          else if (tag == L"hover")
            elementIndex |= HOVER;
        }
        if ((elementIndex & (~pass)) == 0)
        {
          if (!ReadThemeItem(themeItem, labelState[pass]))
          {
            return;
          }
        }
      }
      else if (className == L"tab_label")
      {
        int elementIndex = 0;
        for (auto& tag : attrTags)
        {
          if (tag == L"selected")
            elementIndex |= SELECTED;
          else if (tag == L"expandable")
            elementIndex |= EXPANDABLE;
          else if (tag == L"expanded")
            elementIndex |= EXPANDED;
          else if (tag == L"hover")
            elementIndex |= HOVER;
        }
        if ((elementIndex & (~pass)) == 0)
        {
          if (!ReadThemeItem(themeItem, topLabelState[pass]))
          {
            return;
          }
        }
      }
    }
  }

  //Do it now to prevent deadlocks later
  containerElement.ForceLoad(*this);
  topContainerElement.ForceLoad(*this);
  for (auto& e : labelState)
  {
    e.ForceLoad(*this);
  }
  for (auto& e : topLabelState)
  {
    e.fontSize++;
    e.ForceLoad(*this);
  }
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

  if (diStruct->itemState & ODS_SELECTED)
    elementIndex |= (isRootMenu ? SELECTED : EXPANDABLE);
  if (diStruct->itemState & ODS_HOTLIGHT)
    elementIndex |= (isRootMenu ? SELECTED : HOVER);

  if (menuItemInfo.fState & MFS_CHECKED)
    elementIndex |= SELECTED;


  Gdiplus::Rect outerRect(diStruct->rcItem.left, diStruct->rcItem.top, diStruct->rcItem.right - diStruct->rcItem.left, diStruct->rcItem.bottom - diStruct->rcItem.top);
  //outerRect.Inflate(1, 1);
  auto& state = isRootMenu ? topLabelState[elementIndex] : labelState[elementIndex];
  auto& container = isRootMenu ? topContainerElement : containerElement;

  container.DrawLayers(graphics, outerRect, false);
 
 
  state.DrawLayers(graphics, outerRect, false);
  
  Gdiplus::PointF textOrigin(
    diStruct->rcItem.left + state.contentMargins.cxLeftWidth + state.rowPadding.X + container.rowPadding.X,
    diStruct->rcItem.top + state.contentMargins.cyTopHeight + state.rowPadding.Y + container.rowPadding.Y
  );

  Gdiplus::PointF textEnd(
    diStruct->rcItem.right - state.contentMargins.cxRightWidth - state.rowPadding.X - container.rowPadding.X,
    diStruct->rcItem.bottom - state.contentMargins.cyBottomHeight - state.rowPadding.Y - container.rowPadding.Y
  );

  if (isRootMenu)
  {
    textOrigin.X = diStruct->rcItem.left;
    textOrigin.Y = diStruct->rcItem.top;
    textEnd.X = diStruct->rcItem.right;
    textEnd.Y = diStruct->rcItem.bottom;
  }

  Gdiplus::RectF textRect(textOrigin, Gdiplus::SizeF(textEnd.X - textOrigin.X, textEnd.Y - textOrigin.Y));

  Gdiplus::StringFormat format;
  format.SetHotkeyPrefix((diStruct->itemState & ODS_NOACCEL) ? Gdiplus::HotkeyPrefixHide : Gdiplus::HotkeyPrefixShow);

  Gdiplus::RectF shadowRect = textRect;
  shadowRect.Offset(state.shadowOffset);

  auto tabIndex = labelText.find(L'\t');
  if (tabIndex == std::wstring::npos)
  {
    if (isRootMenu)
      format.SetAlignment(Gdiplus::StringAlignmentCenter);

    graphics.DrawString(labelText.c_str(), (int)labelText.size(), state.GetFont(dc), shadowRect, &format, state.GetShadowBrush());
    graphics.DrawString(labelText.c_str(), (int)labelText.size(), state.GetFont(dc), textRect, &format, state.GetTextBrush());
  }
  else
  {
    Gdiplus::StringFormat formatRight(&format);
    formatRight.SetAlignment(Gdiplus::StringAlignmentFar);

    graphics.DrawString(labelText.c_str(), (int)tabIndex, state.GetFont(dc), shadowRect, &format, state.GetShadowBrush());
    graphics.DrawString(labelText.c_str() + tabIndex + 1, (int)(labelText.size() - tabIndex), state.GetFont(dc), shadowRect, &formatRight, state.GetShadowBrush());

    graphics.DrawString(labelText.c_str(), (int)tabIndex, state.GetFont(dc), textRect, &format, state.GetTextBrush());
    graphics.DrawString(labelText.c_str() + tabIndex + 1, (int)(labelText.size() - tabIndex), state.GetFont(dc), textRect, &formatRight, state.GetTextBrush());
  }
}

void ThemeDef::MeasureItem(HWND hwnd, LPMEASUREITEMSTRUCT miStruct)
{
  HMENU menu = GetMenu(hwnd);
  HDC dc = GetDC(hwnd);
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

    bool isRootMenu = IsRootMenuItem(menu, miStruct->itemID);
    auto& state = isRootMenu ? topLabelState[0] : labelState[0];
    auto& container = isRootMenu ? topContainerElement : containerElement;

    Gdiplus::PointF textOrigin(
      state.contentMargins.cxLeftWidth + state.rowPadding.X + container.rowPadding.X,
      state.contentMargins.cyTopHeight + state.rowPadding.Y + container.rowPadding.Y
    );

    if (isRootMenu)
    {
      textOrigin.X = 0;
      textOrigin.Y = 0;
    }

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
      miStruct->itemWidth = (UINT)(bounds.GetRight() + state.contentMargins.cxRightWidth + state.rowPadding.X + container.rowPadding.X);
      miStruct->itemHeight = (UINT)(bounds.GetBottom() + state.contentMargins.cyBottomHeight + state.rowPadding.Y + container.rowPadding.Y);
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
  GetTextBrush();
  GetShadowBrush();
  for (auto& l : layers)
  {
    l.ForceLoad(def);
  }
}

Gdiplus::Bitmap* ThemeElement::Layer::GetTexture()
{
  return texture.get();
}

void ThemeElement::Layer::ForceLoad(ThemeDef& def)
{
  if (texturePath.size() > 0 && !texture)
  {
    texture = def.GetBitmap(texturePath.c_str());
  }
  GetTintBrush();
}

void ThemeElement::Layer::DrawLayer(Gdiplus::Graphics& graphics, const Gdiplus::Rect& rect, bool skipImage)
{
  Gdiplus::Image* layerImage = GetTexture();
  if (layerImage && !skipImage)
  {
    Gdiplus::Point imageDims(layerImage->GetWidth(), layerImage->GetHeight());
    Gdiplus::Point srcPts[9];
    Gdiplus::Point srcPtsMax[9];

    int ptsAlongX[4] = { 0, innerMargins.cxLeftWidth, imageDims.X - innerMargins.cxRightWidth, imageDims.X };
    int ptsAlongY[4] = { 0, innerMargins.cyTopHeight, imageDims.Y - innerMargins.cyBottomHeight, imageDims.Y };

    int destPtsAlongX[4] = { rect.GetLeft(), rect.GetLeft() + innerMargins.cxLeftWidth, rect.GetRight() - innerMargins.cxRightWidth, rect.GetRight() };
    int destPtsAlongY[4] = { rect.GetTop(), rect.GetTop() + innerMargins.cyTopHeight, rect.GetBottom() - innerMargins.cyBottomHeight, rect.GetBottom() };
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

    for (int x = 0; x < 3; x++)
    {
      for (int y = 0; y < 3; y++)
      {
        Gdiplus::Rect srcRect(ptsAlongX[x], ptsAlongY[y], ptsAlongX[x + 1] - ptsAlongX[x], ptsAlongY[y + 1] - ptsAlongY[y]);
        Gdiplus::Rect destRect(destPtsAlongX[x], destPtsAlongY[y], destPtsAlongX[x + 1] - destPtsAlongX[x], destPtsAlongY[y + 1] - destPtsAlongY[y]);
        if (!srcRect.IsEmptyArea())
        {
          graphics.DrawImage(layerImage, destRect, srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height, Gdiplus::UnitPixel, nullptr, nullptr, nullptr);
        }
      }
    }
  }
  else if (opacity > 0)
  {
    graphics.FillRectangle(GetTintBrush(), rect);
  }
}
