#include <sstream>
#include <string>
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "lib/nlohmann/json.hpp"
#include <regex>

using json = nlohmann::basic_json<nlohmann::ordered_map>;
//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

void pluginInit(HANDLE /*hModule*/)
{
}

void pluginCleanUp()
{
}

void commandMenuInit()
{
    setCommand(0, TEXT("Convert"), JSONToXMLConvert, NULL, false);
}

void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}

bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}


std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring escapeXml(const std::wstring& s) {
    std::wstringstream ss;
    for (auto& c : s) {
        switch (c) {
        case L'<':  ss << L"&lt;";   break;
        case L'>':  ss << L"&gt;";   break;
        case L'&':  ss << L"&amp;";  break;
        case L'\'': ss << L"&apos;"; break;
        case L'"':  ss << L"&quot;"; break;
        default: ss << c;
        }
    }
    return ss.str();
}


std::wstring capitalizeFirstLetter(std::wstring s) {
    if (!s.empty()) {
        s[0] = towupper(s[0]);
    }
    return s;
}

void jsonToXmlRecursive(const json& j, std::wstringstream& xml, const std::wstring& tagName, int indentationLevel) {
    std::wstring capitalizedTagName = capitalizeFirstLetter(tagName);
    std::wstring indent(indentationLevel * 4, L' ');

    if (j.is_object()) {
        xml << indent << L"<" << capitalizedTagName << L">";
        xml << L"\n";
        for (auto& el : j.items()) {
            jsonToXmlRecursive(el.value(), xml, utf8_to_wstring(el.key()), indentationLevel + 1);
        }
        xml << indent << L"</" << capitalizedTagName << L">";
        xml << L"\n";
    } else if (j.is_array()) {
        for (const auto& item : j) {
            jsonToXmlRecursive(item, xml, tagName, indentationLevel);
        }
    } else { // Is a primitive type
        xml << indent << L"<" << capitalizedTagName << L">";
        // After the regex hack, large numbers are now strings.
        if (j.is_string()) {
            xml << escapeXml(utf8_to_wstring(j.get<std::string>()));
        } else if (j.is_boolean()) { // Handle bools explicitly
            xml << (j.get<bool>() ? L"true" : L"false");
        } else if (j.is_number()) { // For smaller numbers that were not quoted
            xml << j.get<double>();
        } else if (j.is_null()) {
            // empty tag for null
        }
        xml << L"</" << capitalizedTagName << L">";
        xml << L"\n";
    }
}


void JSONToXMLConvert()
{
    HWND hScintilla = nppData._scintillaMainHandle;
    LRESULT  length = ::SendMessage(hScintilla, SCI_GETTEXTLENGTH, 0, 0);
    if (length == 0) {
        MessageBox(nppData._nppHandle, TEXT("The document is empty."), TEXT("JSON to XML"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::vector<char> buffer(length + 1, 0);
    ::SendMessage(hScintilla, SCI_GETTEXT, length + 1, (LPARAM)buffer.data());
    std::string json_text(buffer.data());
    std::regex long_num_regex(": +([0-9]+)");
    std::string modified_json_text = std::regex_replace(json_text, long_num_regex, ": \"$1\"");
    std::vector<char> utf8_buffer;
    std::wstringstream xml_stream;
    xml_stream << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    try {
        
        json j = json::parse(modified_json_text);

        if (j.is_object()) {
            for (auto& el : j.items()) {
                jsonToXmlRecursive(el.value(), xml_stream, utf8_to_wstring( el.key()), 0);
            }
        } else if (j.is_array()) {
            for (const auto& item : j) {
                jsonToXmlRecursive(item, xml_stream, L"Items", 0);
            }
        }

        std::wstring xml_output = xml_stream.str();

        int required_size = WideCharToMultiByte(CP_UTF8, 0, xml_output.c_str(), -1, NULL, 0, NULL, NULL);
        utf8_buffer.reserve(required_size);
        WideCharToMultiByte(CP_UTF8, 0, xml_output.c_str(), -1, utf8_buffer.data(), required_size, NULL, NULL);

    }
    catch (nlohmann::json::exception& e) { // Catch all json exceptions
        std::string error_msg = "JSON Error: ";
        error_msg += e.what();
        MessageBoxA(nppData._nppHandle, error_msg.c_str(), "JSON Parse Error", MB_OK | MB_ICONERROR);
        return;
    }
    catch (...) {
        MessageBox(nppData._nppHandle, TEXT("An unexpected error occurred during conversion."), TEXT("Error"), MB_OK | MB_ICONERROR);
        return;
    }

    // Open a new document
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);

    // Get the current scintilla
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return;
    HWND curScintilla = (which == 0)? hScintilla :nppData._scintillaSecondHandle;

    ::SendMessage(curScintilla, SCI_SETTEXT,    0, (LPARAM)utf8_buffer.data());


}

