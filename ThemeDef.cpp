#include "stdafx.h"
#include "ThemeDef.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

ThemeDef::ThemeDef(const wchar_t* jsonData)
{
	std::wstringstream wrapped;
	wrapped << L"{ \"elements\":\n\n" << jsonData << L"}";
	rapidjson::GenericDocument<rapidjson::UTF16<>> doc;
	rapidjson::ParseResult pr = doc.Parse<rapidjson::kParseCommentsFlag>(wrapped.str().c_str());
	if (pr.IsError())
	{
		const wchar_t* jsonDataErr = wrapped.str().c_str() + pr.Offset();
	}

}


ThemeDef::~ThemeDef()
{
}

void ThemeDef::DrawItem(HWND hwnd, const LPDRAWITEMSTRUCT diStruct)
{
}

void ThemeDef::MeasureItem(HWND hwnd, LPMEASUREITEMSTRUCT miStruct)
{
}
