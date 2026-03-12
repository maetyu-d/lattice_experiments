#include "ExpressionEvaluator.h"

#include <array>
#include <vector>

namespace
{
enum class VariableId : uint8_t
{
    x,
    t,
    row,
    col,
    beat,
    step,
    pi,
    tau
};

enum class FunctionId : uint8_t
{
    sin,
    cos,
    tan,
    abs,
    floor,
    ceil,
    fract,
    min,
    max,
    clamp,
    mix,
    mtof,
    osc,
    saw,
    tri,
    pulse,
    noise,
    clip,
    tanh
};

enum class OpCode : uint8_t
{
    pushConstant,
    pushVariable,
    negate,
    add,
    subtract,
    multiply,
    divide,
    modulo,
    function
};

struct Instruction
{
    OpCode op = OpCode::pushConstant;
    double constant = 0.0;
    uint8_t id = 0;
    uint8_t argCount = 0;
};

double readVariable (VariableId id, const ExpressionScope& scope) noexcept
{
    switch (id)
    {
        case VariableId::x:    return scope.x;
        case VariableId::t:    return scope.t;
        case VariableId::row:  return scope.row;
        case VariableId::col:  return scope.col;
        case VariableId::beat: return scope.beat;
        case VariableId::step: return scope.step;
        case VariableId::pi:   return juce::MathConstants<double>::pi;
        case VariableId::tau:  return juce::MathConstants<double>::twoPi;
    }

    return 0.0;
}

double evaluateFunction (FunctionId id, const double* args, int argCount, const ExpressionScope& scope) noexcept
{
    const auto arg = [args, argCount] (int index) noexcept -> double
    {
        return index < argCount ? args[index] : 0.0;
    };

    switch (id)
    {
        case FunctionId::sin:   return argCount >= 1 ? std::sin (arg (0)) : 0.0;
        case FunctionId::cos:   return argCount >= 1 ? std::cos (arg (0)) : 0.0;
        case FunctionId::tan:   return argCount >= 1 ? std::tan (arg (0)) : 0.0;
        case FunctionId::abs:   return argCount >= 1 ? std::abs (arg (0)) : 0.0;
        case FunctionId::floor: return argCount >= 1 ? std::floor (arg (0)) : 0.0;
        case FunctionId::ceil:  return argCount >= 1 ? std::ceil (arg (0)) : 0.0;
        case FunctionId::fract: return argCount >= 1 ? arg (0) - std::floor (arg (0)) : 0.0;
        case FunctionId::min:   return argCount >= 2 ? std::min (arg (0), arg (1)) : 0.0;
        case FunctionId::max:   return argCount >= 2 ? std::max (arg (0), arg (1)) : 0.0;
        case FunctionId::clamp: return argCount >= 3 ? juce::jlimit (arg (1), arg (2), arg (0)) : 0.0;
        case FunctionId::mix:   return argCount >= 3 ? juce::jmap (arg (2), arg (0), arg (1)) : 0.0;
        case FunctionId::mtof:  return argCount >= 1 ? 440.0 * std::pow (2.0, (arg (0) - 69.0) / 12.0) : 0.0;
        case FunctionId::osc:   return argCount >= 1 ? std::sin (scope.t * juce::MathConstants<double>::twoPi * std::max (1.0, arg (0))) : 0.0;
        case FunctionId::saw:
        {
            if (argCount < 1)
                return 0.0;
            auto phase = scope.t * std::max (1.0, arg (0));
            return ((phase - std::floor (phase)) * 2.0) - 1.0;
        }
        case FunctionId::tri:
        {
            if (argCount < 1)
                return 0.0;
            auto phase = scope.t * std::max (1.0, arg (0));
            auto fract = phase - std::floor (phase);
            return 1.0 - 4.0 * std::abs (fract - 0.5);
        }
        case FunctionId::pulse:
        {
            if (argCount < 1)
                return 0.0;
            auto width = argCount >= 2 ? juce::jlimit (0.01, 0.99, arg (1)) : 0.5;
            auto phase = scope.t * std::max (1.0, arg (0));
            auto fract = phase - std::floor (phase);
            return fract < width ? 1.0 : -1.0;
        }
        case FunctionId::noise:
        {
            if (argCount < 1)
                return 0.0;
            auto stepped = std::floor (scope.t * std::max (1.0, arg (0)));
            auto hash = std::sin (stepped * 12.9898 + 78.233 + scope.row * 17.0 + scope.col * 11.0) * 43758.5453;
            auto fract = hash - std::floor (hash);
            return (fract * 2.0) - 1.0;
        }
        case FunctionId::clip:  return argCount >= 3 ? juce::jlimit (arg (1), arg (2), arg (0)) : 0.0;
        case FunctionId::tanh:  return argCount >= 1 ? std::tanh (arg (0)) : 0.0;
    }

    return 0.0;
}

bool lookupVariable (const juce::String& name, VariableId& outId) noexcept
{
    if (name == "x")    { outId = VariableId::x; return true; }
    if (name == "t")    { outId = VariableId::t; return true; }
    if (name == "row")  { outId = VariableId::row; return true; }
    if (name == "col")  { outId = VariableId::col; return true; }
    if (name == "beat") { outId = VariableId::beat; return true; }
    if (name == "step") { outId = VariableId::step; return true; }
    if (name == "pi")   { outId = VariableId::pi; return true; }
    if (name == "tau")  { outId = VariableId::tau; return true; }
    return false;
}

bool lookupFunction (const juce::String& name, FunctionId& outId) noexcept
{
    if (name == "sin")   { outId = FunctionId::sin; return true; }
    if (name == "cos")   { outId = FunctionId::cos; return true; }
    if (name == "tan")   { outId = FunctionId::tan; return true; }
    if (name == "abs")   { outId = FunctionId::abs; return true; }
    if (name == "floor") { outId = FunctionId::floor; return true; }
    if (name == "ceil")  { outId = FunctionId::ceil; return true; }
    if (name == "fract") { outId = FunctionId::fract; return true; }
    if (name == "min")   { outId = FunctionId::min; return true; }
    if (name == "max")   { outId = FunctionId::max; return true; }
    if (name == "clamp") { outId = FunctionId::clamp; return true; }
    if (name == "mix")   { outId = FunctionId::mix; return true; }
    if (name == "mtof")  { outId = FunctionId::mtof; return true; }
    if (name == "osc")   { outId = FunctionId::osc; return true; }
    if (name == "saw")   { outId = FunctionId::saw; return true; }
    if (name == "tri")   { outId = FunctionId::tri; return true; }
    if (name == "pulse") { outId = FunctionId::pulse; return true; }
    if (name == "noise") { outId = FunctionId::noise; return true; }
    if (name == "clip")  { outId = FunctionId::clip; return true; }
    if (name == "tanh")  { outId = FunctionId::tanh; return true; }
    return false;
}

class Parser
{
public:
    explicit Parser (juce::String sourceIn)
        : source (std::move (sourceIn).removeCharacters (" \t\r\n"))
    {
    }

    std::vector<Instruction> parse() noexcept
    {
        std::vector<Instruction> instructions;

        if (source.isEmpty())
        {
            instructions.push_back ({ OpCode::pushConstant, 0.0, 0, 0 });
            return instructions;
        }

        parseExpression (instructions);

        if (instructions.empty())
            instructions.push_back ({ OpCode::pushConstant, 0.0, 0, 0 });

        return instructions;
    }

private:
    void parseExpression (std::vector<Instruction>& instructions) noexcept
    {
        parseTerm (instructions);

        while (! isAtEnd())
        {
            if (match ('+'))
            {
                parseTerm (instructions);
                instructions.push_back ({ OpCode::add, 0.0, 0, 0 });
            }
            else if (match ('-'))
            {
                parseTerm (instructions);
                instructions.push_back ({ OpCode::subtract, 0.0, 0, 0 });
            }
            else
            {
                break;
            }
        }
    }

    void parseTerm (std::vector<Instruction>& instructions) noexcept
    {
        parseUnary (instructions);

        while (! isAtEnd())
        {
            if (match ('*'))
            {
                parseUnary (instructions);
                instructions.push_back ({ OpCode::multiply, 0.0, 0, 0 });
            }
            else if (match ('/'))
            {
                parseUnary (instructions);
                instructions.push_back ({ OpCode::divide, 0.0, 0, 0 });
            }
            else if (match ('%'))
            {
                parseUnary (instructions);
                instructions.push_back ({ OpCode::modulo, 0.0, 0, 0 });
            }
            else
            {
                break;
            }
        }
    }

    void parseUnary (std::vector<Instruction>& instructions) noexcept
    {
        if (match ('+'))
        {
            parseUnary (instructions);
            return;
        }

        if (match ('-'))
        {
            parseUnary (instructions);
            instructions.push_back ({ OpCode::negate, 0.0, 0, 0 });
            return;
        }

        parsePrimary (instructions);
    }

    void parsePrimary (std::vector<Instruction>& instructions) noexcept
    {
        if (match ('('))
        {
            parseExpression (instructions);
            match (')');
            return;
        }

        if (std::isdigit (peek()) || peek() == '.')
        {
            instructions.push_back ({ OpCode::pushConstant, parseNumber(), 0, 0 });
            return;
        }

        if (std::isalpha (peek()))
        {
            parseIdentifier (instructions);
            return;
        }

        instructions.push_back ({ OpCode::pushConstant, 0.0, 0, 0 });
    }

    double parseNumber() noexcept
    {
        const auto start = position;

        while (! isAtEnd() && (std::isdigit (peek()) || peek() == '.'))
            ++position;

        return source.substring (start, position).getDoubleValue();
    }

    void parseIdentifier (std::vector<Instruction>& instructions) noexcept
    {
        const auto start = position;

        while (! isAtEnd() && (std::isalnum (peek()) || peek() == '_'))
            ++position;

        const auto name = source.substring (start, position);

        if (match ('('))
        {
            uint8_t argCount = 0;

            if (! match (')'))
            {
                do
                {
                    parseExpression (instructions);
                    ++argCount;
                }
                while (match (','));

                match (')');
            }

            FunctionId functionId {};
            if (lookupFunction (name, functionId))
                instructions.push_back ({ OpCode::function, 0.0, (uint8_t) functionId, argCount });
            else
                instructions.push_back ({ OpCode::pushConstant, 0.0, 0, 0 });

            return;
        }

        VariableId variableId {};
        if (lookupVariable (name, variableId))
            instructions.push_back ({ OpCode::pushVariable, 0.0, (uint8_t) variableId, 0 });
        else
            instructions.push_back ({ OpCode::pushConstant, 0.0, 0, 0 });
    }

    bool match (char c) noexcept
    {
        if (peek() != c)
            return false;

        ++position;
        return true;
    }

    bool isAtEnd() const noexcept
    {
        return position >= source.length();
    }

    char peek() const noexcept
    {
        return isAtEnd() ? '\0' : static_cast<char> (source[position]);
    }

    juce::String source;
    int position = 0;
};
}

struct ExpressionEvaluator::Program
{
    std::vector<Instruction> instructions;
    int maxStackDepth = 1;
};

std::shared_ptr<const ExpressionEvaluator::Program> ExpressionEvaluator::compile (const juce::String& expression)
{
    auto program = std::make_shared<Program>();
    program->instructions = Parser (expression).parse();

    auto currentStackDepth = 0;
    auto maxStackDepth = 1;

    for (auto& instruction : program->instructions)
    {
        switch (instruction.op)
        {
            case OpCode::pushConstant:
            case OpCode::pushVariable:
                ++currentStackDepth;
                break;
            case OpCode::negate:
                currentStackDepth = juce::jmax (currentStackDepth, 1);
                break;
            case OpCode::add:
            case OpCode::subtract:
            case OpCode::multiply:
            case OpCode::divide:
            case OpCode::modulo:
                currentStackDepth = juce::jmax (1, currentStackDepth - 1);
                break;
            case OpCode::function:
                currentStackDepth = juce::jmax (1, currentStackDepth - (int) instruction.argCount + 1);
                break;
        }

        maxStackDepth = juce::jmax (maxStackDepth, currentStackDepth);
    }

    program->maxStackDepth = juce::jlimit (1, 64, maxStackDepth);
    return program;
}

double ExpressionEvaluator::evaluate (const Program& program, const ExpressionScope& scope) noexcept
{
    std::array<double, 64> stack {};
    int stackSize = 0;

    const auto push = [&stack, &stackSize] (double value) noexcept
    {
        if (stackSize < (int) stack.size())
            stack[(size_t) stackSize++] = value;
    };

    const auto pop = [&stack, &stackSize] () noexcept -> double
    {
        if (stackSize <= 0)
            return 0.0;

        return stack[(size_t) --stackSize];
    };

    for (auto& instruction : program.instructions)
    {
        switch (instruction.op)
        {
            case OpCode::pushConstant:
                push (instruction.constant);
                break;
            case OpCode::pushVariable:
                push (readVariable ((VariableId) instruction.id, scope));
                break;
            case OpCode::negate:
            {
                auto value = pop();
                push (-value);
                break;
            }
            case OpCode::add:
            {
                auto rhs = pop();
                auto lhs = pop();
                push (lhs + rhs);
                break;
            }
            case OpCode::subtract:
            {
                auto rhs = pop();
                auto lhs = pop();
                push (lhs - rhs);
                break;
            }
            case OpCode::multiply:
            {
                auto rhs = pop();
                auto lhs = pop();
                push (lhs * rhs);
                break;
            }
            case OpCode::divide:
            {
                auto rhs = pop();
                auto lhs = pop();
                push (juce::approximatelyEqual (rhs, 0.0) ? 0.0 : lhs / rhs);
                break;
            }
            case OpCode::modulo:
            {
                auto rhs = pop();
                auto lhs = pop();
                push (juce::approximatelyEqual (rhs, 0.0) ? 0.0 : std::fmod (lhs, rhs));
                break;
            }
            case OpCode::function:
            {
                std::array<double, 8> args {};
                const auto argCount = juce::jlimit (0, (int) args.size(), (int) instruction.argCount);

                for (int i = argCount - 1; i >= 0; --i)
                    args[(size_t) i] = pop();

                push (evaluateFunction ((FunctionId) instruction.id, args.data(), argCount, scope));
                break;
            }
        }
    }

    const auto value = stackSize > 0 ? stack[(size_t) (stackSize - 1)] : 0.0;
    return std::isfinite (value) ? value : 0.0;
}

double ExpressionEvaluator::evaluate (const std::shared_ptr<const Program>& program, const ExpressionScope& scope) noexcept
{
    return program != nullptr ? evaluate (*program, scope) : 0.0;
}
