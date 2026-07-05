#pragma once

#include <string>

const char* WebIndexHtml();
std::string WebIndexHtmlWithToken(const std::string& token,
                                  bool password_required);
const char* WebAccessHelpHtml();
