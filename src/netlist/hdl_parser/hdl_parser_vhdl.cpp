#include "netlist/hdl_parser/hdl_parser_vhdl.h"

#include "core/log.h"
#include "core/utils.h"

#include <algorithm>
#include <iomanip>

hdl_parser_vhdl::hdl_parser_vhdl(std::stringstream& stream) : hdl_parser(stream)
{
}

// ###########################################################################
// ###########          Parse HDL into intermediate format          ##########
// ###########################################################################

bool hdl_parser_vhdl::parse()
{
    // tokenize file
    if (!tokenize())
    {
        return false;
    }

    // parse tokens into intermediate format
    try
    {
        if (!parse_tokens())
        {
            return false;
        }
    }
    catch (token_stream<case_insensitive_string>::token_stream_exception& e)
    {
        if (e.line_number != (u32)-1)
        {
            log_error("hdl_parser", "{} near line {}.", e.message, e.line_number);
        }
        else
        {
            log_error("hdl_parser", "{}.", e.message);
        }
        return false;
    }

    return true;
}

static bool is_digits(const case_insensitive_string& str)
{
    return std::all_of(str.begin(), str.end(), ::isdigit);
}

bool hdl_parser_vhdl::tokenize()
{
    std::vector<token<case_insensitive_string>> tmp_tokens;
    std::string delimiters = ",(): ;=><&";
    case_insensitive_string current_token;
    u32 line_number = 0;

    std::string line;
    bool in_string = false;
    bool escaped   = false;
    while (std::getline(m_fs, line))
    {
        line_number++;
        if (line.find("--") != std::string::npos)
        {
            line = line.substr(0, line.find("--"));
        }
        for (char c : core_utils::trim(line))
        {
            if (c == '\\')
            {
                escaped = !escaped;
            }
            else if (!escaped && c == '"')
            {
                in_string = !in_string;
            }
            if (delimiters.find(c) == std::string::npos || escaped || in_string)
            {
                current_token += c;
            }
            else
            {
                if (!current_token.empty())
                {
                    if (tmp_tokens.size() > 1 && is_digits(tmp_tokens.at(tmp_tokens.size() - 2).string) && tmp_tokens.at(tmp_tokens.size() - 1) == "." && is_digits(current_token))
                    {
                        tmp_tokens.pop_back();
                        tmp_tokens.back() += "." + current_token;
                    }
                    else
                    {
                        tmp_tokens.emplace_back(line_number, current_token);
                    }
                    current_token.clear();
                }
                if (c == '=' && tmp_tokens.at(tmp_tokens.size() - 1) == "<")
                {
                    tmp_tokens.at(tmp_tokens.size() - 1) = "<=";
                }
                else if (c == '=' && tmp_tokens.at(tmp_tokens.size() - 1) == ":")
                {
                    tmp_tokens.at(tmp_tokens.size() - 1) = ":=";
                }
                else if (c == '>' && tmp_tokens.at(tmp_tokens.size() - 1) == "=")
                {
                    tmp_tokens.at(tmp_tokens.size() - 1) = "=>";
                }
                else if (!std::isspace(c))
                {
                    tmp_tokens.emplace_back(line_number, case_insensitive_string(1, c));
                }
            }
        }
        if (!current_token.empty())
        {
            tmp_tokens.emplace_back(line_number, current_token);
            current_token.clear();
        }
    }
    m_token_stream = token_stream(tmp_tokens, {"("}, {")"});
    return true;
}

bool hdl_parser_vhdl::parse_tokens()
{
    while (m_token_stream.remaining() > 0)
    {
        if (m_token_stream.peek() == "library" || m_token_stream.peek() == "use")
        {
            if (!parse_library())
            {
                return false;
            }
        }
        else if (m_token_stream.peek() == "entity")
        {
            if (!parse_entity())
            {
                return false;
            }
        }
        else if (m_token_stream.peek() == "architecture")
        {
            if (!parse_architecture())
            {
                return false;
            }
        }
        else
        {
            log_error("hdl_parser", "unexpected token '{}' in global scope in line {}", m_token_stream.peek().string, m_token_stream.peek().number);
            return false;
        }
    }

    return true;
}

bool hdl_parser_vhdl::parse_library()
{
    if (m_token_stream.peek() == "use")
    {
        m_token_stream.consume("use", true);
        auto lib = m_token_stream.consume().string;
        m_token_stream.consume(";", true);

        // remove specific import like ".all" but keep the "."
        lib = core_utils::trim_t(lib.substr(0, lib.rfind(".") + 1));
        m_libraries.insert(lib);
    }
    else
    {
        m_token_stream.consume_until(";");
        m_token_stream.consume(";", true);
    }
    return true;
}

bool hdl_parser_vhdl::parse_entity()
{
    entity e;

    m_token_stream.consume("entity", true);
    e._line_number = m_token_stream.peek().number;
    e._name        = m_token_stream.consume();
    m_token_stream.consume("is", true);

    auto next_token = m_token_stream.peek();
    while (next_token != "end")
    {
        if (next_token == "generic")
        {
            m_token_stream.consume_until(";");
            m_token_stream.consume(";", true);
        }
        else if (next_token == "port")
        {
            if (!parse_port_definitons(e))
            {
                return false;
            }
        }
        else if (next_token == "attribute")
        {
            if (!parse_attribute(e))
            {
                return false;
            }
        }
        else
        {
            log_error("hdl_parser", "unexpected token '{}' in entity defintion in line {}.", next_token.string, next_token.number);
            return false;
        }

        next_token = m_token_stream.peek();
    }

    m_token_stream.consume("end", true);
    m_token_stream.consume();
    m_token_stream.consume(";", true);

    if (m_entities.find(e._name) != m_entities.end())
    {
        log_error("hdl_parser", "an entity with the name '{}' does already exist (see line {} and line {}).", e._name, e._line_number, m_entities.at(e._name)._line_number);
        return false;
    }

    if (!e._name.empty())
    {
        m_entities[e._name] = e;
        m_last_entity       = e._name;
    }

    return true;
}

static std::set<case_insensitive_string> supported_directions = {"in", "out", "inout"};

bool hdl_parser_vhdl::parse_port_definitons(entity& e)
{
    // default port assignments are not supported
    m_token_stream.consume("port", true);
    m_token_stream.consume("(", true);
    auto port_str = m_token_stream.extract_until(")");

    while (port_str.remaining() > 0)
    {
        std::vector<case_insensitive_string> port_names;
        std::set<signal> signals;

        auto line_number = port_str.peek().number;

        // extract names
        do
        {
            port_names.push_back(port_str.consume().string);
        } while (port_str.consume(",", false));

        port_str.consume(":", true);

        // extract direction
        auto direction = port_str.consume();
        if (supported_directions.find(direction) == supported_directions.end())
        {
            log_error("hdl_parser", "invalid direction '{}' for port declaration in line {}.", direction.string, line_number);
            return false;
        }

        // extract ranges
        auto ranges = parse_signal_ranges(port_str);
        if (ranges.empty())    // TODO verify that this does not trigger for std_logic signals
        {
            // error already printed in subfunction
            return false;
        }

        port_str.consume(";", port_str.remaining() > 0);    // last entry has no semicolon, so no throw in that case

        for (const auto& name : port_names)
        {
            signal s(line_number, name, ranges);
            e._ports.emplace(name, std::make_pair(direction, s));
        }
    }

    m_token_stream.consume(")", true);
    m_token_stream.consume(";", true);

    return true;
}

bool hdl_parser_vhdl::parse_attribute(entity& e)
{
    std::tuple<case_insensitive_string, case_insensitive_string, case_insensitive_string> attribute;
    u32 line_number = m_token_stream.peek().number;

    m_token_stream.consume("attribute", true);
    auto attr_name = m_token_stream.consume().string;

    if (m_token_stream.peek() == ":")
    {
        m_token_stream.consume(":", true);
        m_attribute_types[attr_name] = m_token_stream.join_until(";", " ");
        m_token_stream.consume(";", true);
    }
    else if (m_token_stream.peek() == "of" && m_token_stream.peek(2) == ":")
    {
        m_token_stream.consume("of", true);
        auto attr_target = m_token_stream.consume();
        m_token_stream.consume(":", true);
        auto attr_class = m_token_stream.consume();
        m_token_stream.consume("is", true);
        auto value = m_token_stream.join_until(";", " ").string;
        m_token_stream.consume(";", true);

        if (value[0] == '\"' && value.back() == '\"')
        {
            value = value.substr(1, value.size() - 2);
        }

        auto type_it = m_attribute_types.find(attr_name);
        if (type_it == m_attribute_types.end())
        {
            log_warning("hdl_parser", "attribute {} has unknown base type in line {}.", attr_name, line_number);
            attribute = std::make_tuple(attr_name, "unknown", value);
        }
        else
        {
            attribute = std::make_tuple(attr_name, type_it->second, value);
        }

        if (attr_class == "entity")
        {
            e._entity_attributes[attr_target].insert(attribute);
        }
        else if (attr_class == "label")
        {
            e._instance_attributes[attr_target].insert(attribute);
        }
        else if (attr_class == "signal")
        {
            e._signal_attributes[attr_target].insert(attribute);
        }
        else
        {
            log_error("hdl_parser", "invalid attribute class '{}' in line {}.", attr_class.string, line_number);
            return false;
        }
    }
    else
    {
        log_error("hdl_parser", "malformed attribute defintion in line {}.", line_number);
        return false;
    }

    return true;
}

bool hdl_parser_vhdl::parse_architecture()
{
    m_token_stream.consume("architecture", true);
    m_token_stream.consume();
    m_token_stream.consume("of", true);

    auto entity_name = m_token_stream.consume();

    auto it = m_entities.find(entity_name);
    if (it == m_entities.end())
    {
        log_error("hdl_parser", "architecture refers to entity '{}', but no such entity exists.", entity_name.string);
        return false;
    }
    auto& e = it->second;

    m_token_stream.consume("is", true);

    return parse_architecture_header(e) && parse_architecture_body(e);
}

bool hdl_parser_vhdl::parse_architecture_header(entity& e)
{
    auto next_token = m_token_stream.peek();
    while (next_token != "begin")
    {
        if (next_token == "signal")
        {
            if (!parse_signal_definition(e))
            {
                return false;
            }
        }
        else if (next_token == "component")
        {
            // components are ignored
            m_token_stream.consume_until("end");
            m_token_stream.consume("end", true);
            m_token_stream.consume();
            m_token_stream.consume(";", true);
        }
        else if (next_token == "attribute")
        {
            if (!parse_attribute(e))
            {
                return false;
            }
        }
        else
        {
            log_error("hdl_parser", "unexpected token '{}' in architecture header in line {}", next_token.string, next_token.number);
            return false;
        }

        next_token = m_token_stream.peek();
    }

    return true;
}

bool hdl_parser_vhdl::parse_signal_definition(entity& e)
{
    std::vector<case_insensitive_string> signal_names;

    m_token_stream.consume("signal", true);

    auto line_number = m_token_stream.peek().number;

    // extract names
    do
    {
        signal_names.push_back(m_token_stream.consume().string);
    } while (m_token_stream.consume(",", false));

    m_token_stream.consume(":", true);

    // extract bounds
    auto ranges = parse_signal_ranges(m_token_stream);
    if (ranges.empty())    // TODO verify that this does not trigger for std_logic signals
    {
        // error already printed in subfunction
        return false;
    }

    m_token_stream.consume(";", true);

    for (const auto& name : signal_names)
    {
        signal s(line_number, name, ranges);
        e._signals.emplace(name, s);
    }

    return true;
}

bool hdl_parser_vhdl::parse_architecture_body(entity& e)
{
    m_token_stream.consume("begin", true);

    while (m_token_stream.peek() != "end")
    {
        // new instance found
        if (m_token_stream.peek(1) == ":")
        {
            if (!parse_instance(e))
            {
                return false;
            }
        }
        // not in instance -> has to be a direct assignment
        else if (m_token_stream.find_next("<=") < m_token_stream.find_next(";"))
        {
            if (!parse_assign(e))
            {
                return false;
            }
        }
        else
        {
            log_error("hdl_parser", "unexpected token '{}' in architecture body in line {}", m_token_stream.peek().string, m_token_stream.peek().number);
            return false;
        }
    }

    m_token_stream.consume("end", true);
    m_token_stream.consume();
    m_token_stream.consume(";", true);
    return true;
}

bool hdl_parser_vhdl::parse_assign(entity& e)
{
    auto line_number = m_token_stream.peek().number;
    auto left_str    = m_token_stream.extract_until("<=");
    m_token_stream.consume("<=", true);
    auto right_str = m_token_stream.extract_until(";");
    m_token_stream.consume(";", true);

    // extract assignments for each bit
    auto left_parts  = get_assignment_signals(e, left_str, true, false);
    auto right_parts = get_assignment_signals(e, right_str, false, false);

    // verify correctness
    if (left_parts.second == 0 || right_parts.second == 0)
    {
        // error already printed in subfunction
        return false;
    }

    if (left_parts.second != right_parts.second)
    {
        log_error("hdl_parser", "assignment width mismatch: left side has size {} and right side has size {} in line {}.", left_parts.second, right_parts.second, line_number);
        return false;
    }

    e._assignments.emplace(left_parts.first, right_parts.first);

    return true;
}

bool hdl_parser_vhdl::parse_instance(entity& e)
{
    instance inst;

    inst._line_number = m_token_stream.peek().number;
    inst._name        = m_token_stream.consume();
    m_token_stream.consume(":", true);

    // remove prefix from type
    if (m_token_stream.peek() == "entity")
    {
        m_token_stream.consume("entity", true);
        inst._type = m_token_stream.consume();
        auto pos   = inst._type.find('.');
        if (pos != std::string::npos)
        {
            inst._type = inst._type.substr(pos + 1);
        }
    }
    else if (m_token_stream.peek() == "component")
    {
        m_token_stream.consume("component", true);
        inst._type = m_token_stream.consume();
    }
    else
    {
        inst._type = m_token_stream.consume();
        case_insensitive_string prefix;

        // find longest matching library prefix
        for (const auto& lib : m_libraries)
        {
            if (lib.size() > prefix.size() && core_utils::starts_with_t(inst._type, lib))
            {
                prefix = lib;
            }
        }

        // remove prefix
        if (!prefix.empty())
        {
            inst._type = inst._type.substr(prefix.size());
        }
    }

    if (m_token_stream.consume("generic"))
    {
        if (!parse_generic_assign(inst))
        {
            return false;
        }
    }

    if (m_token_stream.peek() == "port")
    {
        if (!parse_port_assign(e, inst))
        {
            return false;
        }
    }

    m_token_stream.consume(";", true);

    e._instances.emplace(inst._name, inst);

    return true;
}

bool hdl_parser_vhdl::parse_port_assign(entity& e, instance& inst)
{
    m_token_stream.consume("port", true);
    m_token_stream.consume("map", true);
    m_token_stream.consume("(", true);
    auto port_str = m_token_stream.extract_until(")");
    m_token_stream.consume(")", true);

    while (port_str.remaining() > 0)
    {
        auto left_str = port_str.extract_until("=>");
        port_str.consume("=>", true);
        auto right_str = port_str.extract_until(",");
        port_str.consume(",", port_str.remaining() > 0);    // last entry has no comma

        if (!right_str.consume("open"))
        {
            auto left_parts  = get_assignment_signals(e, left_str, true, true);
            auto right_parts = get_assignment_signals(e, right_str, false, true);

            if (left_parts.second == 0 || right_parts.second == 0)
            {
                // error already printed in subfunction
                return false;
            }

            inst._port_assignments.emplace(left_parts.first.at(0)._name, std::make_pair(left_parts.first.at(0), right_parts.first));
        }
    }

    return true;
}

bool hdl_parser_vhdl::parse_generic_assign(instance& inst)
{
    m_token_stream.consume("map", true);
    m_token_stream.consume("(", true);
    auto generic_str = m_token_stream.extract_until(")");
    m_token_stream.consume(")", true);

    while (generic_str.remaining() > 0)
    {
        case_insensitive_string value, data_type;

        auto line_number = generic_str.peek().number;
        auto lhs         = generic_str.join_until("=>", "");
        generic_str.consume("=>", true);
        auto rhs = generic_str.join_until(",", "");
        generic_str.consume(",", generic_str.remaining() > 0);    // last entry has no comma

        // determine data type
        if ((rhs == "true") || (rhs == "false"))
        {
            value     = rhs;
            data_type = "boolean";
        }
        else if (core_utils::is_integer_t(rhs.string))
        {
            value     = rhs;
            data_type = "integer";
        }
        else if (core_utils::is_floating_point_t(rhs.string))
        {
            data_type = "floating_point";
        }
        else if (core_utils::ends_with_t(rhs.string, case_insensitive_string("s")) || core_utils::ends_with_t(rhs.string, case_insensitive_string("sec"))
                 || core_utils::ends_with_t(rhs.string, case_insensitive_string("min")) || core_utils::ends_with_t(rhs.string, case_insensitive_string("hr")))
        {
            value     = rhs;
            data_type = "time";
        }
        else if (rhs.string[0] == '\"' && rhs.string.back() == '\"')
        {
            value     = rhs.string.substr(1, rhs.string.size() - 2);
            data_type = "string";
        }
        else if (rhs.string[0] == '\'' && rhs.string[2] == '\'')
        {
            value     = rhs.string.substr(1, 1);
            data_type = "bit_value";
        }
        else if (core_utils::starts_with_t(rhs.string, case_insensitive_string("b\"")) || core_utils::starts_with_t(rhs.string, case_insensitive_string("o\""))
                 || core_utils::starts_with_t(rhs.string, case_insensitive_string("x\"")))
        {
            value = get_hex_from_literal(rhs);
            if (value.empty())
            {
                return false;
            }

            data_type = "bit_vector";
        }
        else
        {
            log_error("hdl_parser", "cannot identify data type of generic map value '{}' in instance '{}' in line {}", rhs.string, inst._name, line_number);
            return false;
        }

        inst._generic_assignments.emplace(lhs, std::make_pair(data_type, value));
    }

    return true;
}

// ###########################################################################
// ###################          Helper functions          ####################
// ###########################################################################

std::vector<u32> hdl_parser_vhdl::parse_range(token_stream<case_insensitive_string>& range_str)
{
    if (range_str.remaining() == 1)
    {
        return {(u32)std::stoi(std::string(range_str.consume().string.data()))};
    }

    int direction = 1;
    int start     = std::stoi(std::string(range_str.consume().string.data()));

    if (range_str.peek() == "downto")
    {
        range_str.consume("downto");
        direction = -1;
    }
    else
    {
        range_str.consume("to", true);
    }

    int end = std::stoi(std::string(range_str.consume().string.data()));

    std::vector<u32> res;
    for (int i = start; i != end + direction; i += direction)
    {
        res.push_back((u32)i);
    }
    return res;
}

static std::map<case_insensitive_string, size_t> id_to_dim = {{"std_logic_vector", 1}, {"std_logic_vector2", 2}, {"std_logic_vector3", 3}};

std::vector<std::vector<u32>> hdl_parser_vhdl::parse_signal_ranges(token_stream<case_insensitive_string>& signal_str)
{
    auto line_number = signal_str.peek().number;

    // extract bounds
    auto type_str = signal_str.extract_until(":=");    // default assignment will be ignored for now

    if (type_str.size() == 1)
    {
        type_str.consume("std_logic", true);
        return {{}};
    }

    std::vector<std::vector<u32>> ranges;
    auto type_name = type_str.consume();
    type_str.consume("(", true);
    auto bound_str = type_str.extract_until(")");
    type_str.consume(")", true);

    // process ranges
    while (bound_str.remaining() > 0)
    {
        auto bound = bound_str.extract_until(",");
        bound_str.consume(",", bound_str.remaining() > 0);
        ranges.emplace_back(parse_range(bound));
    }

    if (id_to_dim.find(type_name) != id_to_dim.end())
    {
        auto dimension = id_to_dim.at(type_name);

        if (ranges.size() != dimension)
        {
            log_error("hdl_parser", "dimension-bound mismatch in line {} : expected {}, got {}.", line_number, dimension, ranges.size());
            return {};
        }
    }
    else
    {
        log_error("hdl_parser", "type name {} is invalid in line {}.", type_name.string, line_number);
        return {};
    }

    return ranges;
}

std::pair<std::vector<hdl_parser_vhdl::signal>, i32> hdl_parser_vhdl::get_assignment_signals(entity& e, token_stream<case_insensitive_string>& signal_str, bool is_left_half, bool is_port_assignment)
{
    // PARSE ASSIGNMENT
    //   assignment can currently be one of the following:
    //   (1) NAME
    //   (2) NUMBER
    //   (3) NAME(INDEX1, INDEX2, ...)
    //   (4) NAME(BEGIN_INDEX1 to/downto END_INDEX1, BEGIN_INDEX2 to/downto END_INDEX2, ...)
    //   (5) ((1 - 4), (1 - 4), ...)

    std::vector<signal> result;
    std::vector<token_stream<case_insensitive_string>> parts;
    i32 size = 0;

    // (5) ((1 - 4), (1 - 4), ...)
    if (!is_left_half)
    {
        if (signal_str.consume("("))
        {
            do
            {
                parts.push_back(signal_str.extract_until(","));
            } while (signal_str.consume(",", false));

            signal_str.consume(")", true);
        }
    }
    else
    {
        if (signal_str.find_next(",") != token_stream<case_insensitive_string>::END_OF_STREAM)
        {
            log_error("hdl_parser", "aggregation is not allowed at this position in line {}.", signal_str.peek().number);
            return {{}, 0};
        }
        parts.push_back(signal_str);
    }

    for (auto& part_str : parts)
    {
        auto signal_name_token = part_str.consume();
        i32 line_number        = signal_name_token.number;
        auto signal_name       = signal_name_token;
        std::vector<std::vector<u32>> ranges;
        bool is_binary      = false;
        bool is_bound_known = true;

        // (2) NUMBER
        if (core_utils::starts_with_t(signal_name.string, case_insensitive_string("b\"")) || core_utils::starts_with_t(signal_name.string, case_insensitive_string("o\""))
            || core_utils::starts_with_t(signal_name.string, case_insensitive_string("x\"")))
        {
            if (is_left_half)
            {
                log_error("hdl_parser", "numeric value {} not allowed at this position in line {}.", signal_name.string, line_number);
                return {{}, 0};
            }

            signal_name = get_bin_from_literal(signal_name_token);
            if (signal_name.string.empty())
            {
                // error printed in subfunction
                return {{}, 0};
            }

            std::vector<u32> range(signal_name.string.size());
            std::generate(range.begin(), range.end(), [n = signal_name.string.size()]() mutable { return --n; });
            ranges    = {range};    // TODO verify range
            is_binary = true;
        }
        else
        {
            std::vector<std::vector<u32>> reference_ranges;

            if (auto signal_it = e._signals.find(signal_name); signal_it != e._signals.end())
            {
                reference_ranges = signal_it->second._ranges;
            }
            else if (auto port_it = e._ports.find(signal_name); port_it != e._ports.end())
            {
                reference_ranges = port_it->second.second._ranges;
            }
            else
            {
                log_error("hdl_parser", "signal name '{}' is invalid in assignment in line {}.", signal_name.string, line_number);
                return {{}, 0};
            }

            // (3) NAME(INDEX1, INDEX2, ...)
            // (4) NAME(BEGIN_INDEX1 to/downto END_INDEX1, BEGIN_INDEX2 to/downto END_INDEX2, ...)
            if (part_str.consume("("))
            {
                do
                {
                    ranges.emplace_back(parse_range(part_str));

                } while (part_str.consume(",", false));
                part_str.consume(")", true);

                if (!std::includes(reference_ranges.begin(), reference_ranges.end(), ranges.begin(), ranges.end()))
                {
                    log_error("hdl_parser", "invalid bounds given for signal or port '{}' in line {}.", signal_name.string, line_number);
                    return {{}, 0};
                }
            }
            else
            {
                // (1) NAME
                if (is_port_assignment && is_left_half)
                {
                    is_bound_known = false;
                    ranges         = {};
                }
                else
                {
                    ranges = reference_ranges;
                }
            }
        }

        // create new signal for assign
        signal s(line_number, signal_name, ranges, is_binary, is_bound_known);
        size += s.size();
        result.push_back(s);
    }

    return std::make_pair(result, size);
}

static std::map<char, case_insensitive_string> oct_to_bin = {{'0', "000"}, {'1', "001"}, {'2', "010"}, {'3', "011"}, {'4', "100"}, {'5', "101"}, {'6', "110"}, {'7', "111"}};
static std::map<char, case_insensitive_string> hex_to_bin = {{'0', "0000"},
                                                             {'1', "0001"},
                                                             {'2', "0010"},
                                                             {'3', "0011"},
                                                             {'4', "0100"},
                                                             {'5', "0101"},
                                                             {'6', "0110"},
                                                             {'7', "0111"},
                                                             {'8', "1000"},
                                                             {'9', "1001"},
                                                             {'a', "1010"},
                                                             {'b', "1011"},
                                                             {'c', "1100"},
                                                             {'d', "1101"},
                                                             {'e', "1110"},
                                                             {'f', "1111"}};

case_insensitive_string hdl_parser_vhdl::get_bin_from_literal(token<case_insensitive_string>& value_token)
{
    auto line_number = value_token.number;
    auto value       = core_utils::to_lower_t(core_utils::replace_t(value_token.string, case_insensitive_string("_"), case_insensitive_string("")));

    case_insensitive_string res;

    auto prefix = value[0];
    auto number = value.substr(2, value.find('\"') - 2);

    // parse number literal
    switch (prefix)
    {
        case 'b': {
            for (const auto& c : number)
            {
                if (c >= '0' && c <= '1')
                {
                    res += c;
                }
                else
                {
                    log_error("hdl_parser", "invalid character within binary number literal {} in line {}.", value, line_number);
                    return "";
                }
            }
            break;
        }

        case 'o': {
            for (const auto& c : number)
            {
                if (c >= '0' && c <= '7')
                {
                    res += oct_to_bin[c];
                }
                else
                {
                    log_error("hdl_parser", "invalid character within octal number literal {} in line {}.", value, line_number);
                    return "";
                }
            }
            break;
        }

        case 'h': {
            for (const auto& c : number)
            {
                if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
                {
                    res += hex_to_bin[c];
                }
                else
                {
                    log_error("hdl_parser", "invalid character within hexadecimal number literal {} in line {}.", value, line_number);
                    return "";
                }
            }
            break;
        }

        default: {
            log_error("hdl_parser", "invalid base '{}' within number literal {} in line {}.", prefix, value, line_number);
            return "";
        }
    }

    return res;
}

case_insensitive_string hdl_parser_vhdl::get_hex_from_literal(token<case_insensitive_string>& value_token)
{
    auto line_number = value_token.number;
    auto value       = core_utils::to_lower_t(core_utils::replace_t(value_token.string, case_insensitive_string("_"), case_insensitive_string("")));

    u32 base;

    auto prefix = value[0];
    auto number = value.substr(2, value.find('\"') - 2);
    i32 len;

    // select base
    switch (prefix)
    {
        case 'b': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '1'); }))
            {
                log_error("hdl_parser", "invalid character within binary number literal {} in line {}.", value, line_number);
                return "";
            }

            len  = number.size() + 3 / 4;
            base = 2;
            break;
        }

        case 'o': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '7'); }))
            {
                log_error("hdl_parser", "invalid character within octal number literal {} in line {}.", value, line_number);
                return "";
            }

            len  = number.size() + 1 / 2;
            base = 8;
            break;
        }

        case 'h': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
            {
                log_error("hdl_parser", "invalid character within hexadecimal number literal {} in line {}.", value, line_number);
                return "";
            }

            len  = number.size();
            base = 16;
            break;
        }

        default: {
            log_error("hdl_parser", "invalid base '{}' within number literal {} in line {}.", prefix, value, line_number);
            return "";
        }
    }

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(len) << std::hex << stoull(std::string(number.data()), 0, base);
    return case_insensitive_string(ss.str().data());
}