#pragma once

#include <JuceHeader.h>
#include <memory>

struct ExpressionScope
{
    double x {};
    double t {};
    double row {};
    double col {};
    double beat {};
    double step {};
};

class ExpressionEvaluator
{
public:
    struct Program;

    static std::shared_ptr<const Program> compile (const juce::String& expression);
    static double evaluate (const Program& program, const ExpressionScope& scope) noexcept;
    static double evaluate (const std::shared_ptr<const Program>& program, const ExpressionScope& scope) noexcept;
};
