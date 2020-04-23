#include "netlist/hdl_parser/hdl_parser_verilog.h"

#include "core/log.h"
#include "core/utils.h"

#include <iomanip>

// TODO: parse attributes

hdl_parser_verilog::hdl_parser_verilog(std::stringstream& stream) : hdl_parser(stream)
{
}

// ###########################################################################
// ###########          Parse HDL into intermediate format          ##########
// ###########################################################################

bool hdl_parser_verilog::parse()
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
    catch (token_stream<std::string>::token_stream_exception& e)
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

bool hdl_parser_verilog::tokenize()
{
    std::string delimiters = ",()[]{}\\#: ;=.";
    std::string current_token;
    u32 line_number = 0;

    std::string line;
    bool escaped             = false;
    bool multi_line_comment  = false;
    bool multi_line_property = false;

    std::vector<token<std::string>> parsed_tokens;
    while (std::getline(m_fs, line))
    {
        line_number++;
        this->remove_comments(line, multi_line_comment, multi_line_property);

        for (char c : line)
        {
            if (c == '\\')
            {
                escaped = true;
                continue;
            }
            else if (escaped && std::isspace(c))
            {
                escaped = false;
                continue;
            }

            if ((!std::isspace(c) && delimiters.find(c) == std::string::npos) || escaped)
            {
                current_token += c;
            }
            else
            {
                if (!current_token.empty())
                {
                    parsed_tokens.emplace_back(line_number, current_token);
                    current_token.clear();
                }

                if (c == '(' && parsed_tokens.back() == "#")
                {
                    parsed_tokens.back() = "#(";
                }
                else if (!std::isspace(c))
                {
                    parsed_tokens.emplace_back(line_number, std::string(1, c));
                }
            }
        }
        if (!current_token.empty())
        {
            parsed_tokens.emplace_back(line_number, current_token);
            current_token.clear();
        }
    }

    m_token_stream = token_stream(parsed_tokens, {"(", "["}, {")", "]"});
    return true;
}

bool hdl_parser_verilog::parse_tokens()
{
    while (m_token_stream.remaining() > 0)
    {
        if (!parse_entity())
        {
            return false;
        }
    }

    return true;
}

bool hdl_parser_verilog::parse_entity()
{
    entity e;
    std::set<std::string> port_names;

    std::cout << "parsing entity..." << std::endl;

    m_token_stream.consume("module", true);
    e._line_number = m_token_stream.peek().number;
    e._name        = m_token_stream.consume();

    // parse port list
    if (m_token_stream.consume("#("))
    {
        m_token_stream.consume_until(")");
        m_token_stream.consume(")", true);
    }

    parse_port_list(port_names);

    m_token_stream.consume(";", true);

    auto next_token = m_token_stream.peek();
    while (next_token != "endmodule")
    {
        if (next_token == "input" || next_token == "output" || next_token == "inout")
        {
            if (!parse_port_definition(e, port_names))
            {
                return false;
            }
        }
        else if (next_token == "wire")
        {
            if (!parse_signal_definition(e))
            {
                return false;
            }
        }
        else if (next_token == "assign")
        {
            if (!parse_assign(e))
            {
                return false;
            }
        }
        else
        {
            if (!parse_instance(e))
            {
                return false;
            }
        }

        next_token = m_token_stream.peek();
    }

    m_token_stream.consume("endmodule", true);

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

void hdl_parser_verilog::parse_port_list(std::set<std::string>& port_names)
{
    std::cout << "parsing port list..." << std::endl;

    m_token_stream.consume("(", true);
    auto ports = m_token_stream.extract_until(")");
    m_token_stream.consume(")", true);

    while (ports.remaining() > 0)
    {
        port_names.insert(ports.consume().string);
        ports.consume(",", ports.remaining() > 0);
    }
}

bool hdl_parser_verilog::parse_port_definition(entity& e, const std::set<std::string>& port_names)
{
    i32 line_number       = m_token_stream.peek().number;
    std::string direction = m_token_stream.consume();
    auto ports            = parse_signal_list();

    std::cout << "parsing port definition..." << std::endl;

    if (direction == "input")
    {
        direction = "in";
    }
    else if (direction == "output")
    {
        direction = "out";
    }
    else if (direction == "inout")
    {
        direction = "inout";
    }

    if (ports.empty())
    {
        // error already printed in subfunction
        return false;
    }

    for (auto& port : ports)
    {
        if (port_names.find(port.first) == port_names.end())
        {
            log_error("hdl_parser", "port name '{}' in line {} has not been declared in entity port list.", port.first, line_number);
            return false;
        }

        e._ports.emplace(port.first, std::make_pair(direction, port.second));
    }

    return true;
}

bool hdl_parser_verilog::parse_signal_definition(entity& e)
{
    std::cout << "parsing signal definition..." << std::endl;

    m_token_stream.consume("wire", true);
    auto signals = parse_signal_list();

    if (signals.empty())
    {
        // error already printed in subfunction
        return false;
    }

    e._signals.insert(signals.begin(), signals.end());
    return true;
}

bool hdl_parser_verilog::parse_assign(entity& e)
{
    auto line_number = m_token_stream.peek().number;
    m_token_stream.consume("assign", true);
    auto left_str = m_token_stream.extract_until("=");
    m_token_stream.consume("=", true);
    auto right_str = m_token_stream.extract_until(";");
    m_token_stream.consume(";", true);

    std::cout << "parsing assignment..." << std::endl;

    // extract assignments for each bit
    auto left_parts  = get_assignment_signals(e, left_str, false);
    auto right_parts = get_assignment_signals(e, right_str, true);

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

bool hdl_parser_verilog::parse_instance(entity& e)
{
    instance inst;
    inst._line_number = m_token_stream.peek().number;
    inst._type        = m_token_stream.consume();

    std::cout << "parsing instance..." << std::endl;

    // parse generics map
    if (m_token_stream.consume("#("))
    {
        if (!parse_generic_assign(inst))
        {
            return false;
        }
    }

    // parse instance name
    inst._name = m_token_stream.consume();

    // parse port map
    if (!parse_port_assign(e, inst))
    {
        return false;
    }

    // add to vector of instances of current entity
    if (e._instances.find(inst._name) != e._instances.end())
    {
        log_error("hdl_parser", "an instance with the name '{}' does already exist (see line {} and line {}).", inst._name, inst._line_number, e._instances.at(inst._name)._line_number);
        return false;
    }
    e._instances.emplace(inst._name, inst);

    return true;
}

bool hdl_parser_verilog::parse_port_assign(entity& e, instance& inst)
{
    std::cout << "parsing port assignment..." << std::endl;

    m_token_stream.consume("(", true);
    auto port_str = m_token_stream.extract_until(")");
    m_token_stream.consume(")", true);

    while (port_str.remaining() > 0)
    {
        port_str.consume(".", true);
        auto left_str = port_str.consume();
        port_str.consume("(", true);
        auto right_str = port_str.extract_until(")");
        port_str.consume(")", true);
        port_str.consume(",", port_str.remaining() > 0);

        // check if port unconnected
        if (right_str.size() != 0)
        {
            signal s(left_str.number, left_str.string, {}, false, false);
            auto right_parts = get_assignment_signals(e, right_str, true);

            // verify correctness
            if (right_parts.second == 0)
            {
                // error already printed in subfunction
                std::cout << "FUCK 1" << std::endl;
                return false;
            }

            inst._port_assignments.emplace(s._name, std::make_pair(s, right_parts.first));
        }
    }

    m_token_stream.consume(";", true);

    return true;
}

bool hdl_parser_verilog::parse_generic_assign(instance& inst)
{
    std::cout << "parsing generic assignment..." << std::endl;

    auto generic_str = m_token_stream.extract_until(")");
    m_token_stream.consume(")", true);

    while (generic_str.remaining() > 0)
    {
        std::string value, data_type;

        auto line_number = generic_str.peek().number;
        generic_str.consume(".", true);
        auto lhs = generic_str.join_until("(", "");
        generic_str.consume("(", true);
        auto rhs = generic_str.join_until(")", "");
        generic_str.consume(")", true);
        generic_str.consume(",", generic_str.remaining() > 0);

        if (core_utils::is_integer(rhs))
        {
            value     = rhs;
            data_type = "integer";
        }
        else if (core_utils::is_floating_point(rhs))
        {
            data_type = "floating_point";
        }
        else if (rhs.string[0] == '\"' && rhs.string.back() == '\"')
        {
            value     = rhs.string.substr(1, rhs.string.size() - 2);
            data_type = "string";
        }
        else if (isdigit(rhs.string[0]) || rhs.string[0] == '\'')
        {
            value = get_hex_from_literal(rhs);
            if (value.empty())
            {
                return false;
            }

            if (value.size() == 1)
            {
                data_type = "bit_value";
            }
            else
            {
                data_type = "bit_vector";
            }
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

void hdl_parser_verilog::remove_comments(std::string& line, bool& multi_line_comment, bool& multi_line_property)
{
    bool repeat = true;

    while (repeat)
    {
        repeat = false;

        // skip empty lines
        if (line.empty())
        {
            break;
        }

        auto single_line_comment_begin = line.find("//");
        auto multi_line_comment_begin  = line.find("/*");
        auto multi_line_comment_end    = line.find("*/");
        auto multi_line_property_begin = line.find("(*");
        auto multi_line_property_end   = line.find("*)");

        std::string begin = "";
        std::string end   = "";

        if (multi_line_comment == true)
        {
            if (multi_line_comment_end != std::string::npos)
            {
                // multi-line comment ends in current line
                multi_line_comment = false;
                line               = line.substr(multi_line_comment_end + 2);
                repeat             = true;
            }
            else
            {
                // current line entirely within multi-line comment
                line = "";
                break;
            }
        }
        else if (multi_line_property == true)
        {
            if (multi_line_property_end != std::string::npos)
            {
                // multi-line property ends in current line
                multi_line_property = false;
                line                = line.substr(multi_line_property_end + 2);
                repeat              = true;
            }
            else
            {
                // current line entirely in multi-line property
                line = "";
                break;
            }
        }
        else
        {
            if (single_line_comment_begin != std::string::npos)
            {
                if (multi_line_comment_begin == std::string::npos || (multi_line_comment_begin != std::string::npos && multi_line_comment_begin > single_line_comment_begin))
                {
                    // single-line comment
                    line   = line.substr(0, single_line_comment_begin);
                    repeat = true;
                }
            }
            else if (multi_line_comment_begin != std::string::npos)
            {
                if (multi_line_comment_end != std::string::npos)
                {
                    // multi-line comment entirely in current line
                    line   = line.substr(0, multi_line_comment_begin) + line.substr(multi_line_comment_end + 2);
                    repeat = true;
                }
                else
                {
                    // multi-line comment starts in current line
                    multi_line_comment = true;
                    line               = line.substr(0, multi_line_comment_begin);
                }
            }
            else if (multi_line_property_begin != std::string::npos)
            {
                if (multi_line_property_end != std::string::npos)
                {
                    // multi-line property entirely in current line
                    line   = line.substr(0, multi_line_property_begin) + line.substr(multi_line_property_end + 2);
                    repeat = true;
                }
                else
                {
                    // multi-line property starts in current line
                    multi_line_property = true;
                    line                = line.substr(0, multi_line_property_begin);
                }
            }
        }
    }
}

std::vector<u32> hdl_parser_verilog::parse_range(token_stream<std::string>& range_str)
{
    if (range_str.remaining() == 1)
    {
        return {(u32)std::stoi(std::string(range_str.consume().string.data()))};
    }

    int start = std::stoi(std::string(range_str.consume().string.data()));
    range_str.consume(":", true);
    int end = std::stoi(std::string(range_str.consume().string.data()));

    int direction = (start <= end) ? 1 : -1;

    std::vector<u32> res;
    for (int i = start; i != end + direction; i += direction)
    {
        res.push_back((u32)i);
    }
    return res;
}

std::map<std::string, hdl_parser_verilog::signal> hdl_parser_verilog::parse_signal_list()
{
    std::map<std::string, signal> signals;
    std::vector<std::vector<u32>> ranges;

    auto signal_str = m_token_stream.extract_until(";");
    m_token_stream.consume(";", true);

    // extract bounds
    while (signal_str.consume("["))
    {
        auto range = parse_range(signal_str);
        signal_str.consume("]", true);

        ranges.emplace_back(range);
    }

    if (ranges.empty())
    {
        ranges.push_back({});
    }

    // extract names
    do
    {
        auto signal_name = signal_str.consume();

        signal s(signal_str.peek().number, signal_name.string, ranges);
        signals.emplace(signal_name, s);
    } while (signal_str.consume(",", false));

    return signals;
}

std::pair<std::vector<hdl_parser_verilog::signal>, i32> hdl_parser_verilog::get_assignment_signals(entity& e, token_stream<std::string>& signal_str, bool allow_numerics)
{
    // PARSE ASSIGNMENT
    //   assignment can currently be one of the following:
    //   (1) NAME *single-dimensional*
    //   (2) NAME *multi-dimensional*
    //   (3) NUMBER
    //   (4) NAME[INDEX1][INDEX2]...
    //   (5) NAME[BEGIN_INDEX1:END_INDEX1][BEGIN_INDEX2:END_INDEX2]...
    //   (6) {(1 - 5), (1 - 5), ...}

    std::vector<signal> result;
    std::vector<token_stream<std::string>> parts;
    i32 size = 0;

    // (6) {(1) - (5), (1) - (5), ...}
    if (signal_str.peek() == "{")
    {
        signal_str.consume("{", true);

        auto assignment_list_str = signal_str.extract_until("}");
        signal_str.consume("}", true);

        do
        {
            parts.push_back(assignment_list_str.extract_until(","));
        } while (assignment_list_str.consume(",", false));
    }
    else
    {
        parts.push_back(signal_str);
    }

    for (auto& part_stream : parts)
    {
        auto signal_name_token  = part_stream.consume();
        i32 line_number         = signal_name_token.number;
        std::string signal_name = signal_name_token;
        std::vector<std::vector<u_int32_t>> ranges;
        bool is_binary = false;

        // (3) NUMBER
        if (isdigit(signal_name[0]) || signal_name[0] == '\'')
        {
            if (!allow_numerics)
            {
                log_error("hdl_parser", "numeric value {} not allowed at this position in line {}.", signal_name, line_number);
                return {{}, 0};
            }

            signal_name = get_bin_from_literal(signal_name_token);
            if (signal_name.empty())
            {
                return {{}, 0};
            }

            ranges    = {};
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
                log_error("hdl_parser", "signal name '{}' is invalid in assignment in line {}.", signal_name, line_number);
                return {{}, 0};
            }

            // any bounds specified?
            if (part_stream.consume("["))
            {
                // (4) NAME[INDEX1][INDEX2]...
                // (5) NAME[BEGIN_INDEX1:END_INDEX1][BEGIN_INDEX2:END_INDEX2]...
                do
                {
                    auto range_str = part_stream.extract_until("]");
                    ranges.emplace_back(parse_range(range_str));
                    part_stream.consume("]", true);
                } while (part_stream.consume("[", false));

                if (!std::includes(reference_ranges.begin(), reference_ranges.end(), ranges.begin(), ranges.end()))
                {
                    // TODO includes cannot handle nested ranges, need to find other way. Also consider VHDL
                    log_error("hdl_parser", "invalid bounds given for signal or port '{}' in line {}.", signal_name, line_number);
                    return {{}, 0};
                }
            }
            else
            {
                // (1) NAME *single-dimensional*
                // (2) NAME *multi-dimensional*
                ranges = reference_ranges;
            }
        }

        // create new signal for assign
        signal s(line_number, signal_name, ranges, is_binary);
        size += s.size();
        result.push_back(s);
    }

    return std::make_pair(result, size);
}

static std::map<char, std::string> oct_to_bin = {{'0', "000"}, {'1', "001"}, {'2', "010"}, {'3', "011"}, {'4', "100"}, {'5', "101"}, {'6', "110"}, {'7', "111"}};
static std::map<char, std::string> hex_to_bin = {{'0', "0000"},
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

std::string hdl_parser_verilog::get_bin_from_literal(token<std::string>& value_token)
{
    auto line_number  = value_token.number;
    std::string value = core_utils::to_lower(core_utils::replace(value_token.string, "_", ""));

    i32 len;
    std::string prefix;
    std::string number;
    std::string res;

    // base specified?
    if (value.find('\'') == std::string::npos)
    {
        len    = -1;
        prefix = "d";
        number = value;
    }
    else
    {
        len    = std::stoi(value.substr(0, value.find('\'')));
        prefix = value.substr(value.find('\'') + 1, 1);
        number = value.substr(value.find('\'') + 2);
    }

    // select base
    switch (prefix.at(0))
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

        case 'o':
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

        case 'd': {
            u64 tmp_val = 0;

            for (const auto& c : number)
            {
                if (c >= '0' && c <= '9')
                {
                    tmp_val = (tmp_val * 10) + (c - '0');
                }
                else
                {
                    log_error("hdl_parser", "invalid character within octal number literal {} in line {}.", value, line_number);
                    return "";
                }
            }

            do
            {
                res = std::to_string(tmp_val & 0x1) + res;
                tmp_val >>= 1;
            } while (tmp_val != 0);
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

    for (i32 i = 0; i < len - (i32)res.size(); i++)
    {
        res = "0" + res;
    }

    return res;
}

std::string hdl_parser_verilog::get_hex_from_literal(token<std::string>& value_token)
{
    auto line_number  = value_token.number;
    std::string value = core_utils::to_lower(core_utils::replace(value_token, "_", ""));

    i32 len;
    std::string prefix;
    std::string number;
    u32 base;
    std::string res;

    // base specified?
    if (value.find('\'') == std::string::npos)
    {
        len    = -1;
        prefix = "d";
        number = value;
    }
    else
    {
        len    = std::stoi(value.substr(0, value.find('\'')));
        prefix = value.substr(value.find('\'') + 1, 1);
        number = value.substr(value.find('\'') + 2);
    }

    // select base
    switch (prefix.at(0))
    {
        case 'b': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '1'); }))
            {
                log_error("hdl_parser", "invalid character within binary number literal {} in line {}.", value, line_number);
                return "";
            }

            base = 2;
            break;
        }

        case 'o': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '7'); }))
            {
                log_error("hdl_parser", "invalid character within octal number literal {} in line {}.", value, line_number);
                return "";
            }

            base = 8;
            break;
        }

        case 'd': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '9'); }))
            {
                log_error("hdl_parser", "invalid character within decimal number literal {} in line {}.", value, line_number);
                return "";
            }

            base = 10;
            break;
        }

        case 'h': {
            if (!std::all_of(number.begin(), number.end(), [](const char& c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
            {
                log_error("hdl_parser", "invalid character within hexadecimal number literal {} in line {}.", value, line_number);
                return "";
            }

            base = 16;
            break;
        }

        default: {
            log_error("hdl_parser", "invalid base '{}' within number literal {} in line {}.", prefix, value, line_number);
            return "";
        }
    }

    std::stringstream ss;
    ss << std::setfill('0') << std::setw((len + 3) / 4) << std::hex << stoull(number, 0, base);
    return ss.str();
}

bool hdl_parser_verilog::is_in_bounds(const std::vector<std::pair<i32, i32>>& bounds, const std::vector<std::pair<i32, i32>>& reference_bounds) const
{
    if (bounds.size() != reference_bounds.size())
    {
        return false;
    }

    for (u32 i = 0; i < bounds.size(); i++)
    {
        i32 ref_max, ref_min;
        if (reference_bounds[i].first < reference_bounds[i].second)
        {
            ref_min = reference_bounds[i].first;
            ref_max = reference_bounds[i].second;
        }
        else
        {
            ref_min = reference_bounds[i].second;
            ref_max = reference_bounds[i].first;
        }

        if (!(((ref_min <= bounds[i].first) && (bounds[i].first <= ref_max)) && ((ref_min <= bounds[i].second) && (bounds[i].second <= ref_max))))
        {
            return false;
        }
    }

    return true;
}