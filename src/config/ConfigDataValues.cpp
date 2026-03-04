#include "ConfigDataValues.hpp"
#include "../renderer/WidgetRegistry.hpp"
#include "../helpers/Log.hpp"

std::string CLayoutValueData::resolveWidgetReferences(const std::string& expr) {
    std::string result = expr;
    size_t      pos    = 0;

    while ((pos = result.find('$', pos)) != std::string::npos) {
        if (result.substr(pos, 6) == "$WIDTH" || result.substr(pos, 7) == "$HEIGHT") {
            pos++;
            continue;
        }

        size_t dotPos = result.find('.', pos);
        if (dotPos == std::string::npos) {
            pos++;
            continue;
        }

        size_t endPos = dotPos + 1;
        while (endPos < result.length() && (std::isalnum(result[endPos]) || result[endPos] == '_'))
            endPos++;

        std::string widgetName = result.substr(pos + 1, dotPos - pos - 1);
        std::string property   = result.substr(dotPos + 1, endPos - dotPos - 1);

        widgetName.erase(0, widgetName.find_first_not_of(" \t"));
        widgetName.erase(widgetName.find_last_not_of(" \t") + 1);
        property.erase(0, property.find_first_not_of(" \t"));
        property.erase(property.find_last_not_of(" \t") + 1);

        auto widget = g_pWidgetRegistry.getWidget(widgetName);
        if (!widget) {
            pos = endPos;
            continue;
        }

        double value = 0.0;
        if (property == "WIDTH" || property == "width") {
            value = widget->getSize().x;
        } else if (property == "HEIGHT" || property == "height") {
            value = widget->getSize().y;
        } else {
            pos = endPos;
            continue;
        }

        result.replace(pos, endPos - pos, std::to_string(value));
        pos += std::to_string(value).length();
    }

    return result;
}
