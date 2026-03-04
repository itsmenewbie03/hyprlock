#pragma once
#include "../helpers/Log.hpp"
#include "../helpers/Color.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/string/VarList.hpp>
#include <any>
#include <string>
#include <vector>
#include <cmath>

using namespace Hyprutils::String;

enum eConfigValueDataTypes {
    CVD_TYPE_INVALID  = -1,
    CVD_TYPE_LAYOUT   = 0,
    CVD_TYPE_GRADIENT = 1,
};

class ICustomConfigValueData {
  public:
    virtual ~ICustomConfigValueData() = 0;

    virtual eConfigValueDataTypes getDataType() = 0;

    virtual std::string           toString() = 0;
};

class CLayoutValueData : public ICustomConfigValueData {
  public:
    CLayoutValueData() = default;
    virtual ~CLayoutValueData() {};

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_LAYOUT;
    }

    virtual std::string toString() {
        return std::format("{}{},{}{}", m_vValues.x, (m_sIsRelative.x) ? "%" : "px", m_vValues.y, (m_sIsRelative.y) ? "%" : "px");
    }

    static CLayoutValueData* fromAnyPv(const std::any& v) {
        RASSERT(v.type() == typeid(void*), "Invalid config value type");
        const auto P = (CLayoutValueData*)std::any_cast<void*>(v);
        RASSERT(P, "Empty config value");
        return P;
    }

    Hyprutils::Math::Vector2D getAbsolute(const Hyprutils::Math::Vector2D& viewport) {
        return {
            (m_sIsRelative.x ? (m_vValues.x / 100) * viewport.x : m_vValues.x),
            (m_sIsRelative.y ? (m_vValues.y / 100) * viewport.y : m_vValues.y),
        };
    }

    Hyprutils::Math::Vector2D getAbsoluteWithSize(const Hyprutils::Math::Vector2D& viewport, const Hyprutils::Math::Vector2D& size) {
        return {
            evaluateExpression(m_sExpressions.x, m_vValues.x, m_sIsRelative.x, viewport.x, size.x, size.y),
            evaluateExpression(m_sExpressions.y, m_vValues.y, m_sIsRelative.y, viewport.y, size.x, size.y),
        };
    }

    bool hasDynamicExpressions() const {
        return !m_sExpressions.x.empty() || !m_sExpressions.y.empty();
    }

    std::vector<std::string> getWidgetDependencies() const {
        std::vector<std::string> deps;
        extractWidgetRefs(m_sExpressions.x, deps);
        extractWidgetRefs(m_sExpressions.y, deps);
        return deps;
    }

    Hyprutils::Math::Vector2D m_vValues;
    struct {
        bool x = false;
        bool y = false;
    } m_sIsRelative;
    struct {
        std::string x;
        std::string y;
    } m_sExpressions;

  private:
    static void extractWidgetRefs(const std::string& expr, std::vector<std::string>& refs) {
        size_t pos = 0;
        while ((pos = expr.find('$', pos)) != std::string::npos) {
            if (expr.substr(pos, 6) == "$WIDTH" || expr.substr(pos, 7) == "$HEIGHT") {
                pos++;
                continue;
            }

            size_t dotPos = expr.find('.', pos);
            if (dotPos != std::string::npos) {
                std::string widgetName = expr.substr(pos + 1, dotPos - pos - 1);
                if (!widgetName.empty() && std::find(refs.begin(), refs.end(), widgetName) == refs.end())
                    refs.push_back(widgetName);
            }
            pos++;
        }
    }

    static double evaluateExpression(const std::string& expr, double baseValue, bool isRelative, double viewportDim, double width, double height) {
        if (expr.empty())
            return isRelative ? (baseValue / 100) * viewportDim : baseValue;

        std::string evaluated = expr;

        evaluated = resolveWidgetReferences(evaluated);

        size_t pos = 0;
        while ((pos = evaluated.find("$WIDTH", pos)) != std::string::npos) {
            evaluated.replace(pos, 6, std::to_string(width));
            pos += std::to_string(width).length();
        }
        pos = 0;
        while ((pos = evaluated.find("$HEIGHT", pos)) != std::string::npos) {
            evaluated.replace(pos, 7, std::to_string(height));
            pos += std::to_string(height).length();
        }

        try {
            return evaluateSimpleExpression(evaluated, viewportDim);
        } catch (...) { return isRelative ? (baseValue / 100) * viewportDim : baseValue; }
    }

    static std::string resolveWidgetReferences(const std::string& expr);

    static double      evaluateSimpleExpression(const std::string& expr, double viewportDim) {
        std::string trimmed = expr;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

        if (trimmed.ends_with("%")) {
            std::string numPart = trimmed.substr(0, trimmed.length() - 1);
            return (std::stod(numPart) / 100) * viewportDim;
        }

        if (trimmed.find('?') != std::string::npos && trimmed.find(':') != std::string::npos) {
            size_t      qPos = trimmed.find('?');
            size_t      cPos = trimmed.find(':', qPos);

            std::string condition = trimmed.substr(0, qPos);
            std::string trueVal   = trimmed.substr(qPos + 1, cPos - qPos - 1);
            std::string falseVal  = trimmed.substr(cPos + 1);

            bool        condResult = evaluateCondition(condition);
            return evaluateSimpleExpression(condResult ? trueVal : falseVal, viewportDim);
        }

        while (trimmed.find('(') != std::string::npos) {
            size_t openParen  = trimmed.find_last_of('(');
            size_t closeParen = trimmed.find(')', openParen);
            if (closeParen == std::string::npos)
                break;

            std::string subExpr = trimmed.substr(openParen + 1, closeParen - openParen - 1);
            double      result  = evaluateSimpleExpression(subExpr, viewportDim);
            trimmed.replace(openParen, closeParen - openParen + 1, std::to_string(result));
        }

        size_t opPos = std::string::npos;
        char   op    = 0;
        for (size_t i = trimmed.length() - 1; i > 0; i--) {
            if (trimmed[i] == '+' || trimmed[i] == '-') {
                opPos = i;
                op    = trimmed[i];
                break;
            }
        }

        if (opPos == std::string::npos) {
            for (size_t i = trimmed.length() - 1; i > 0; i--) {
                if (trimmed[i] == '*' || trimmed[i] == '/') {
                    opPos = i;
                    op    = trimmed[i];
                    break;
                }
            }
        }

        if (opPos != std::string::npos) {
            double left  = std::stod(trimmed.substr(0, opPos));
            double right = std::stod(trimmed.substr(opPos + 1));
            switch (op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : left;
            }
        }

        return std::stod(trimmed);
    }

    static bool evaluateCondition(const std::string& cond) {
        std::string trimmed = cond;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

        const std::vector<std::pair<std::string, std::function<bool(double, double)>>> ops = {
            {">=", [](double a, double b) { return a >= b; }}, {"<=", [](double a, double b) { return a <= b; }}, {"==", [](double a, double b) { return a == b; }},
            {"!=", [](double a, double b) { return a != b; }}, {">", [](double a, double b) { return a > b; }},   {"<", [](double a, double b) { return a < b; }},
        };

        for (const auto& [opStr, opFunc] : ops) {
            size_t pos = trimmed.find(opStr);
            if (pos != std::string::npos && pos > 0) {
                try {
                    double left  = std::stod(trimmed.substr(0, pos));
                    double right = std::stod(trimmed.substr(pos + opStr.length()));
                    return opFunc(left, right);
                } catch (...) { continue; }
            }
        }

        try {
            return std::stod(trimmed) != 0;
        } catch (...) { return false; }
    }
};

class CGradientValueData : public ICustomConfigValueData {
  public:
    CGradientValueData() {};
    CGradientValueData(CHyprColor col) {
        m_vColors.push_back(col);
        updateColorsOk();
    };
    virtual ~CGradientValueData() {};

    virtual eConfigValueDataTypes getDataType() {
        return CVD_TYPE_GRADIENT;
    }

    void reset(CHyprColor col) {
        m_vColors.clear();
        m_vColors.emplace_back(col);
        m_fAngle = 0;
        updateColorsOk();
    }

    void updateColorsOk() {
        m_vColorsOkLabA.clear();
        for (auto& c : m_vColors) {
            const auto OKLAB = c.asOkLab();
            m_vColorsOkLabA.emplace_back(OKLAB.l);
            m_vColorsOkLabA.emplace_back(OKLAB.a);
            m_vColorsOkLabA.emplace_back(OKLAB.b);
            m_vColorsOkLabA.emplace_back(c.a);
        }
    }

    /* Vector containing the colors */
    std::vector<CHyprColor> m_vColors;

    /* Vector containing pure colors for shoving into opengl */
    std::vector<float> m_vColorsOkLabA;

    /* Float corresponding to the angle (rad) */
    float m_fAngle = 0;

    /* Whether this gradient stores a fallback value (not exlicitly set) */
    bool m_bIsFallback = false;

    //
    bool operator==(const CGradientValueData& other) const {
        if (other.m_vColors.size() != m_vColors.size() || m_fAngle != other.m_fAngle)
            return false;

        for (size_t i = 0; i < m_vColors.size(); ++i)
            if (m_vColors[i] != other.m_vColors[i])
                return false;

        return true;
    }

    virtual std::string toString() {
        std::string result;
        for (auto& c : m_vColors) {
            result += std::format("{:x} ", c.getAsHex());
        }

        result += std::format("{}deg", (int)(m_fAngle * 180.0 / M_PI));
        return result;
    }

    static CGradientValueData* fromAnyPv(const std::any& v) {
        RASSERT(v.type() == typeid(void*), "Invalid config value type");
        const auto P = (CGradientValueData*)std::any_cast<void*>(v);
        RASSERT(P, "Empty config value");
        return P;
    }
};
