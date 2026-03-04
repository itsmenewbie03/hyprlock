#pragma once

#include "../defines.hpp"
#include "widgets/IWidget.hpp"
#include <unordered_map>
#include <string>

class CWidgetRegistry {
  public:
    static CWidgetRegistry& getInstance() {
        static CWidgetRegistry instance;
        return instance;
    }

    void registerWidget(const std::string& name, ASP<IWidget> widget) {
        if (!name.empty())
            widgets[name] = widget;
    }

    void unregisterWidget(const std::string& name) {
        widgets.erase(name);
    }

    ASP<IWidget> getWidget(const std::string& name) {
        auto it = widgets.find(name);
        return it != widgets.end() ? it->second.lock() : nullptr;
    }

    void clearForOutput(OUTPUTID id) {
        outputWidgets.erase(id);
    }

    void setOutputWidgets(OUTPUTID id, const std::vector<std::string>& names) {
        outputWidgets[id] = names;
    }

  private:
    CWidgetRegistry() = default;

    std::unordered_map<std::string, AWP<IWidget>>         widgets;
    std::unordered_map<OUTPUTID, std::vector<std::string>> outputWidgets;
};

inline CWidgetRegistry& g_pWidgetRegistry = CWidgetRegistry::getInstance();
