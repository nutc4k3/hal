#include "netlist/gate_library/gate_library_parser/gate_library_parser_liberty.h"

#include "core/log.h"
#include "netlist/boolean_function.h"
#include "netlist/gate_library/gate_type/gate_type_lut.h"
#include "netlist/gate_library/gate_type/gate_type_sequential.h"

gate_library_parser_liberty::gate_library_parser_liberty(std::stringstream& stream) : gate_library_parser(stream)
{
}

std::shared_ptr<gate_library> gate_library_parser_liberty::parse()
{
    // tokenize file
    if (!tokenize())
    {
        return nullptr;
    }

    // parse tokens into intermediate format
    try
    {
        if (!parse_tokens())
        {
            return nullptr;
        }
    }
    catch (token_stream<std::string>::token_stream_exception& e)
    {
        if (e.line_number != (u32)-1)
        {
            log_error("liberty_parser", "{} near line {}.", e.message, e.line_number);
        }
        else
        {
            log_error("liberty_parser", "{}.", e.message);
        }
        return nullptr;
    }

    return m_gate_lib;
}

bool gate_library_parser_liberty::tokenize()
{
    std::string delimiters = "{}()[];:\",";
    std::string current_token;
    u32 line_number = 0;

    std::string line;
    bool in_string          = false;
    bool was_in_string      = false;
    bool multi_line_comment = false;

    std::vector<token<std::string>> parsed_tokens;

    while (std::getline(m_fs, line))
    {
        line_number++;
        this->remove_comments(line, multi_line_comment);

        for (char c : line)
        {
            if (c == '\"')
            {
                was_in_string = true;
                in_string     = !in_string;
                continue;
            }

            if (std::isspace(c) && !in_string)
            {
                continue;
            }

            if (delimiters.find(c) == std::string::npos || in_string)
            {
                current_token += c;
            }
            else
            {
                if (was_in_string || !current_token.empty())
                {
                    parsed_tokens.emplace_back(line_number, current_token);
                    current_token.clear();
                    was_in_string = false;
                }

                if (!std::isspace(c))
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

    m_token_stream = token_stream(parsed_tokens, {"(", "{"}, {")", "}"});
    return true;
}

bool gate_library_parser_liberty::parse_tokens()
{
    m_token_stream.consume("library", true);
    m_token_stream.consume("(", true);
    auto lib_name = m_token_stream.consume();
    m_token_stream.consume(")", true);
    m_token_stream.consume("{", true);
    m_gate_lib       = std::make_shared<gate_library>(lib_name.string);
    auto library_str = m_token_stream.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, false);
    m_token_stream.consume("}", false);

    do
    {
        auto next_token = library_str.consume();
        if (next_token == "cell" && library_str.peek() == "(")
        {
            if (!parse_cell(library_str))
            {
                return false;
            }
        }
        else if (next_token == "type" && library_str.peek() == "(")
        {
            if (!parse_type(library_str))
            {
                return false;
            }
        }
    } while (library_str.remaining() > 0);

    return m_token_stream.remaining() == 0;
}

bool gate_library_parser_liberty::parse_type(token_stream<std::string>& str)
{
    type_group type;
    i32 width     = 1;
    i32 start     = 0;
    i32 end       = 0;
    i32 direction = 1;

    type.line_number = str.peek().number;
    str.consume("(", true);
    type.name = str.consume().string;
    str.consume(")", true);
    str.consume("{", true);
    auto type_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    while (type_str.remaining() > 0)
    {
        auto next_token = type_str.consume();
        if (next_token == "base_type")
        {
            type_str.consume(":", true);
            type_str.consume("array", true);
        }
        else if (next_token == "data_type")
        {
            type_str.consume(":", true);
            type_str.consume("bit", true);
        }
        else if (next_token == "bit_width")
        {
            type_str.consume(":", true);
            width = std::stol(type_str.consume().string);
        }
        else if (next_token == "bit_from")
        {
            type_str.consume(":", true);
            start = std::stol(type_str.consume().string);
        }
        else if (next_token == "bit_to")
        {
            type_str.consume(":", true);
            end = std::stol(type_str.consume().string);
        }
        else if (next_token == "downto")
        {
            type_str.consume(":", true);
            if (type_str.consume("false"))
            {
                direction = 1;
            }
            else if (type_str.consume("true"))
            {
                direction = -1;
            }
            else
            {
                log_error("liberty_parser", "invalid token '{}' for boolean value in 'downto' statement in type group '{}' near line {}", type_str.peek().string, type.name, type_str.peek().number);
                return false;
            }
        }
        else
        {
            log_error("liberty_parser", "invalid token '{}' in type group '{}' near line {}", type_str.peek().string, type.name, type_str.peek().number);
            return false;
        }

        type_str.consume(";");
    }

    if (width != (direction * (end - start)) + 1)
    {
        log_error("liberty_parser", "invalid 'bit_width' value {} for type group '{}' near line {}", width, type.name, type.line_number);
        return false;
    }

    for (int i = start; i != end + direction; i += direction)
    {
        type.range.push_back((u32)i);
    }

    m_bus_types.emplace(type.name, type);

    return true;
}

bool gate_library_parser_liberty::parse_cell(token_stream<std::string>& str)
{
    cell_group cell;

    cell.line_number = str.peek().number;
    str.consume("(", true);
    cell.name = str.consume().string;
    str.consume(")", true);
    str.consume("{", true);
    auto cell_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    while (cell_str.remaining() > 0)
    {
        auto next_token = cell_str.consume();
        if (next_token == "pin")
        {
            if (!parse_pin(cell_str, cell))
            {
                return false;
            }
        }
        else if (next_token == "bus")
        {
            auto bus = parse_bus(cell_str, cell);
            if (!bus.has_value())
            {
                return false;
            }
            cell.buses.emplace(bus->name, bus.value());
        }
        else if (next_token == "ff")
        {
            auto ff = parse_ff(cell_str);
            if (!ff.has_value())
            {
                return false;
            }
            cell.type = gate_type::base_type::ff;
            cell.ff   = ff.value();
        }
        else if (next_token == "latch")
        {
            auto latch = parse_latch(cell_str);
            if (!latch.has_value())
            {
                return false;
            }
            cell.type  = gate_type::base_type::latch;
            cell.latch = latch.value();
        }
        else if (next_token == "lut")
        {
            auto lut = parse_lut(cell_str);
            if (!lut.has_value())
            {
                return false;
            }
            cell.type = gate_type::base_type::lut;
            cell.lut  = lut.value();
        }
    }

    auto gt = construct_gate_type(cell);
    if (gt == nullptr)
    {
        return false;
    }

    m_gate_lib->add_gate_type(gt);

    return true;
}

bool gate_library_parser_liberty::parse_pin(token_stream<std::string>& str, cell_group& cell, pin_direction direction, const std::string& pin_name)
{
    std::string function, x_function, z_function;
    std::vector<std::string> names;

    u32 line_number = str.peek().number;
    str.consume("(", true);
    auto pin_names_str = str.extract_until(")", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume(")", true);
    str.consume("{", true);
    auto pin_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    do
    {
        std::string name = pin_names_str.consume().string;
        if (!pin_name.empty() && name != pin_name)
        {
            log_error("liberty_parser", "invalid pin name '{}' near line {}", name, pin_names_str.peek().number);
            return false;
        }

        if (pin_names_str.consume("["))
        {
            if (pin_names_str.peek(1) == ":")
            {
                i32 start = std::stol(pin_names_str.consume().string);
                pin_names_str.consume(":", true);
                i32 end = std::stol(pin_names_str.consume().string);
                i32 dir = (start <= end) ? 1 : -1;

                for (int i = start; i != (end + dir); i += dir)
                {
                    auto new_name = name + "(" + std::to_string(i) + ")";
                    names.push_back(new_name);
                }
            }
            else
            {
                u32 index     = std::stoul(pin_names_str.consume().string);
                auto new_name = name + "(" + std::to_string(index) + ")";
                names.push_back(new_name);
            }

            pin_names_str.consume("]", true);
        }
        else
        {
            names.push_back(name);
        }

        pin_names_str.consume(",", pin_names_str.remaining() > 0);
    } while (pin_names_str.remaining() > 0);

    do
    {
        auto next_token = pin_str.consume();
        if (next_token == "direction")
        {
            pin_str.consume(":", true);
            auto direction_str = pin_str.consume().string;
            if (direction_str == "input")
            {
                direction = pin_direction::IN;
            }
            else if (direction_str == "output")
            {
                direction = pin_direction::OUT;
            }
            else if (direction_str == "inout")
            {
                direction = pin_direction::INOUT;
            }
            else
            {
                log_error("liberty_parser", "invalid pin direction '{}' near line {}", direction_str, pin_str.peek().number);
                return false;
            }
            pin_str.consume(";", true);
        }
        else if (next_token == "function")
        {
            pin_str.consume(":", true);
            function = pin_str.consume().string;
            pin_str.consume(";", true);
        }
        else if (next_token == "x_function")
        {
            pin_str.consume(":", true);
            x_function = pin_str.consume().string;
            pin_str.consume(";", true);
        }
        else if (next_token == "three_state")
        {
            pin_str.consume(":", true);
            z_function = pin_str.consume().string;
            pin_str.consume(";", true);
        }
    } while (pin_str.remaining() > 0);

    if (direction == pin_direction::UNKNOWN)
    {
        log_error("liberty_parser", "no pin direction given near line {}", line_number);
        return false;
    }
    else if (pin_name.empty())
    {
        if (direction == pin_direction::IN || direction == pin_direction::INOUT)
        {
            cell.input_pins.insert(cell.input_pins.end(), names.begin(), names.end());
        }

        if (direction == pin_direction::OUT || direction == pin_direction::INOUT)
        {
            cell.output_pins.insert(cell.output_pins.end(), names.begin(), names.end());

            if (!function.empty())
            {
                for (const auto& name : names)
                {
                    cell.functions[name] = function;
                }
            }

            if (!x_function.empty())
            {
                for (const auto& name : names)
                {
                    cell.x_functions[name] = x_function;
                }
            }

            if (!z_function.empty())
            {
                for (const auto& name : names)
                {
                    cell.z_functions[name] = z_function;
                }
            }
        }
    }

    return true;
}

std::optional<gate_library_parser_liberty::bus_group> gate_library_parser_liberty::parse_bus(token_stream<std::string>& str, cell_group& cell)
{
    bus_group bus;

    bus.line_number = str.peek().number;
    str.consume("(", true);
    bus.name = str.consume().string;
    str.consume(")", true);
    str.consume("{", true);
    auto bus_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    do
    {
        auto next_token = bus_str.consume();
        if (next_token == "bus_type")
        {
            bus_str.consume(":", true);
            auto bus_type_str = bus_str.consume().string;
            if (const auto& it = m_bus_types.find(bus_type_str); it != m_bus_types.end())
            {
                bus.range = it->second.range;
            }
            else
            {
                log_error("liberty_parser", "invalid bus type '{}' near line {}", bus_type_str, bus_str.peek().number);
                return std::nullopt;
            }
            bus_str.consume(";", true);
        }
        else if (next_token == "direction")
        {
            bus_str.consume(":", true);
            auto direction_str = bus_str.consume().string;
            if (direction_str == "input")
            {
                bus.direction = pin_direction::IN;
            }
            else if (direction_str == "output")
            {
                bus.direction = pin_direction::OUT;
            }
            else if (direction_str == "inout")
            {
                bus.direction = pin_direction::INOUT;
            }
            else
            {
                log_error("liberty_parser", "invalid pin direction '{}' near line {}", direction_str, bus_str.peek().number);
                return std::nullopt;
            }
            bus_str.consume(";", true);
        }
        else if (next_token == "pin")
        {
            if (!parse_pin(bus_str, cell, bus.direction, bus.name))
            {
                return std::nullopt;
            }
        }
    } while (bus_str.remaining() > 0);

    for (const auto& index : bus.range)
    {
        bus.pin_names.push_back(bus.name + "(" + std::to_string(index) + ")");
    }

    if (bus.direction == pin_direction::UNKNOWN)
    {
        log_error("liberty_parser", "no pin direction given near line {}", bus.line_number);
        return std::nullopt;
    }

    if (bus.direction == pin_direction::IN || bus.direction == pin_direction::INOUT)
    {
        cell.input_pins.insert(cell.input_pins.end(), bus.pin_names.begin(), bus.pin_names.end());
    }

    if (bus.direction == pin_direction::OUT || bus.direction == pin_direction::INOUT)
    {
        cell.output_pins.insert(cell.output_pins.end(), bus.pin_names.begin(), bus.pin_names.end());
    }

    return bus;
}

std::optional<gate_library_parser_liberty::ff_group> gate_library_parser_liberty::parse_ff(token_stream<std::string>& str)
{
    ff_group ff;

    ff.line_number = str.peek().number;
    str.consume("(", true);
    ff.state1 = str.consume().string;
    str.consume(",", true);
    ff.state2 = str.consume().string;
    str.consume(")", true);
    str.consume("{", true);
    auto ff_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    do
    {
        auto next_token = ff_str.consume();
        if (next_token == "clocked_on")
        {
            ff_str.consume(":", true);
            ff.clocked_on = ff_str.consume();
            ff_str.consume(";", true);
        }
        else if (next_token == "next_state")
        {
            ff_str.consume(":", true);
            ff.next_state = ff_str.consume();
            ff_str.consume(";", true);
        }
        else if (next_token == "clear")
        {
            ff_str.consume(":", true);
            ff.clear = ff_str.consume();
            ff_str.consume(";", true);
        }
        else if (next_token == "preset")
        {
            ff_str.consume(":", true);
            ff.preset = ff_str.consume();
            ff_str.consume(";", true);
        }
        else if (next_token == "clear_preset_var1" || next_token == "clear_preset_var2")
        {
            ff_str.consume(":", true);
            auto behav = ff_str.consume();
            ff_str.consume(";", true);

            std::string behav_string = "LHNTX";
            if (auto pos = behav_string.find(behav); pos != std::string::npos)
            {
                if (next_token == "clear_preset_var1")
                {
                    ff.special_behavior_var1 = gate_type_sequential::set_reset_behavior(pos);
                }
                else
                {
                    ff.special_behavior_var2 = gate_type_sequential::set_reset_behavior(pos);
                }
            }
            else
            {
                log_error("liberty_parser", "invalid clear_preset behavior '{}' near line {}.", behav.string, behav.number);
                return std::nullopt;
            }
        }
        else if (next_token == "data_category")
        {
            ff_str.consume(":", true);
            ff.data_category = ff_str.consume();
            ff_str.consume(";", true);
        }
        else if (next_token == "data_key")
        {
            ff_str.consume(":", true);
            ff.data_identifier = ff_str.consume();
            ff_str.consume(";", true);
        }
    } while (ff_str.remaining() > 0);

    return ff;
}

std::optional<gate_library_parser_liberty::latch_group> gate_library_parser_liberty::parse_latch(token_stream<std::string>& str)
{
    latch_group latch;

    latch.line_number = str.peek().number;
    str.consume("(", true);
    latch.state1 = str.consume().string;
    str.consume(",", true);
    latch.state2 = str.consume().string;
    str.consume(")", true);
    str.consume("{", true);
    auto latch_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    do
    {
        auto next_token = latch_str.consume();
        if (next_token == "enable")
        {
            latch_str.consume(":", true);
            latch.enable = latch_str.consume();
            latch_str.consume(";", true);
        }
        else if (next_token == "data_in")
        {
            latch_str.consume(":", true);
            latch.data_in = latch_str.consume();
            latch_str.consume(";", true);
        }
        else if (next_token == "clear")
        {
            latch_str.consume(":", true);
            latch.clear = latch_str.consume();
            latch_str.consume(";", true);
        }
        else if (next_token == "preset")
        {
            latch_str.consume(":", true);
            latch.preset = latch_str.consume();
            latch_str.consume(";", true);
        }
        else if (next_token == "clear_preset_var1" || next_token == "clear_preset_var2")
        {
            latch_str.consume(":", true);
            auto behav = latch_str.consume();
            latch_str.consume(";", true);

            std::string behav_string = "LHNTX";
            if (auto pos = behav_string.find(behav); pos != std::string::npos)
            {
                if (next_token == "clear_preset_var1")
                {
                    latch.special_behavior_var1 = gate_type_sequential::set_reset_behavior(pos);
                }
                else
                {
                    latch.special_behavior_var2 = gate_type_sequential::set_reset_behavior(pos);
                }
            }
            else
            {
                log_error("liberty_parser", "invalid clear_preset behavior '{}' near line {}.", behav.string, behav.number);
                return std::nullopt;
            }
        }
    } while (latch_str.remaining() > 0);

    return latch;
}

std::optional<gate_library_parser_liberty::lut_group> gate_library_parser_liberty::parse_lut(token_stream<std::string>& str)
{
    lut_group lut;

    lut.line_number = str.peek().number;
    str.consume("(", true);
    lut.name = str.consume().string;
    str.consume(")", true);
    str.consume("{", true);
    auto lut_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);

    do
    {
        auto next_token = str.consume();
        if (next_token == "data_category")
        {
            lut_str.consume(":", true);
            lut.data_category = lut_str.consume();
            lut_str.consume(";", true);
        }
        else if (next_token == "data_identifier")
        {
            lut_str.consume(":", true);
            lut.data_identifier = lut_str.consume();
            lut_str.consume(";", true);
        }
        else if (next_token == "direction")
        {
            lut_str.consume(":", true);
            auto direction = lut_str.consume();

            if (direction == "ascending" || direction == "descending")
            {
                lut.data_direction = direction;
            }
            else
            {
                log_error("liberty_parser", "invalid data direction '{}' near line {}.", direction.string, direction.number);
                return std::nullopt;
            }

            lut_str.consume(";", true);
        }
    } while (lut_str.remaining() > 0);

    return lut;
}

std::shared_ptr<gate_type> gate_library_parser_liberty::construct_gate_type(cell_group& cell)
{
    std::shared_ptr<gate_type> gt;
    std::map<std::string, std::string> tmp_functions;

    if (cell.type == gate_type::base_type::combinatorial)
    {
        gt = std::make_shared<gate_type>(cell.name);
    }
    else if (cell.type == gate_type::base_type::ff)
    {
        auto seq_gt = std::make_shared<gate_type_sequential>(cell.name, cell.type);

        if (!cell.ff.clocked_on.empty())
        {
            tmp_functions["clock"] = cell.ff.clocked_on;
        }

        if (!cell.ff.next_state.empty())
        {
            tmp_functions["next_state"] = cell.ff.next_state;
        }

        if (!cell.ff.preset.empty())
        {
            tmp_functions["preset"] = cell.ff.preset;
        }

        if (!cell.ff.clear.empty())
        {
            tmp_functions["clear"] = cell.ff.clear;
        }

        seq_gt->set_set_reset_behavior(cell.ff.special_behavior_var1, cell.ff.special_behavior_var2);
        seq_gt->set_init_data_category(cell.ff.data_category);
        seq_gt->set_init_data_identifier(cell.ff.data_identifier);

        for (auto& [pin_name, bf_string] : cell.functions)
        {
            if (bf_string == cell.ff.state1)
            {
                seq_gt->add_state_output_pin(pin_name);
                bf_string = "";
            }
            else if (bf_string == cell.ff.state2)
            {
                seq_gt->add_inverted_state_output_pin(pin_name);
                bf_string = "";
            }
        }
    }
    else if (cell.type == gate_type::base_type::latch)
    {
        auto seq_gt = std::make_shared<gate_type_sequential>(cell.name, cell.type);

        if (!cell.latch.enable.empty())
        {
            tmp_functions["enable"] = cell.latch.enable;
        }

        if (!cell.latch.data_in.empty())
        {
            tmp_functions["data"] = cell.latch.data_in;
        }

        if (!cell.latch.preset.empty())
        {
            tmp_functions["preset"] = cell.latch.preset;
        }

        if (!cell.latch.clear.empty())
        {
            tmp_functions["clear"] = cell.latch.clear;
        }

        seq_gt->set_set_reset_behavior(cell.latch.special_behavior_var1, cell.latch.special_behavior_var2);

        for (auto& [pin_name, bf_string] : cell.functions)
        {
            if (bf_string == cell.latch.state1)
            {
                seq_gt->add_state_output_pin(pin_name);
                bf_string = "";
            }
            else if (bf_string == cell.latch.state2)
            {
                seq_gt->add_inverted_state_output_pin(pin_name);
                bf_string = "";
            }
        }

        gt = seq_gt;
    }
    else if (cell.type == gate_type::base_type::lut)
    {
        auto lut_gt = std::make_shared<gate_type_lut>(cell.name);

        lut_gt->set_config_data_category(cell.lut.data_category);
        lut_gt->set_config_data_identifier(cell.lut.data_identifier);
        lut_gt->set_config_data_ascending_order(cell.lut.data_direction == "ascending");

        for (auto& [pin_name, bf_string] : cell.functions)
        {
            if (bf_string == cell.lut.name)
            {
                lut_gt->add_output_from_init_string_pin(pin_name);
                bf_string = "";
            }
        }

        gt = lut_gt;
    }

    // TODO construct boolean functions
    if (!cell.buses.empty())
    {
        for (const auto& bus : cell.buses)
        {
            prepare_bus_pin_functions(cell, cell.functions, bus.second);
            prepare_bus_pin_functions(cell, cell.x_functions, bus.second);
            prepare_bus_pin_functions(cell, cell.z_functions, bus.second);
            prepare_bus_pin_functions(cell, tmp_functions, bus.second);
        }
    }
    else
    {
    }

    gt->add_input_pins(cell.input_pins);
    gt->add_output_pins(cell.output_pins);

    return gt;
}

void gate_library_parser_liberty::remove_comments(std::string& line, bool& multi_line_comment)
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
        auto multi_line_comment_begin = line.find("/*");
        auto multi_line_comment_end   = line.find("*/");

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
    }
}

void gate_library_parser_liberty::prepare_bus_pin_functions(cell_group& cell, std::map<std::string, std::string>& functions, const bus_group& output_bus)
{
    std::vector<std::string> sorted_bus_names;
    for (const auto& bus : cell.buses)
    {
        sorted_bus_names.push_back(bus.first);
    }
    std::sort(sorted_bus_names.begin(), sorted_bus_names.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });

    for (unsigned int i = 0; i < output_bus.pin_names.size(); i++)
    {
        const auto& it = functions.find(output_bus.pin_names.at(i));
        if (it == functions.end() || (it->second.empty()))
        {
            continue;
        }

        auto tmp_function = it->second;

        for (const auto& bus_name : sorted_bus_names)
        {
            auto pos = tmp_function.find(bus_name);
            while (pos != std::string::npos)
            {
                if (tmp_function.size() > (pos + bus_name.size()) && tmp_function.at(pos + bus_name.size()) == '[')
                {
                    unsigned int start = pos + bus_name.size();
                    unsigned int colon = tmp_function.find(":", start);
                    unsigned int end   = tmp_function.find("]", start);

                    if (colon < end)
                    {
                        // PIN[A:B]
                        int range_start = std::stoi(tmp_function.substr(start + 1, colon - start - 1));
                        int range_end   = std::stoi(tmp_function.substr(colon + 1, end - colon - 1));
                        int direction   = (range_end > range_start) ? 1 : -1;

                        tmp_function.replace(pos, bus_name.size() + end - start + 1, bus_name + "(" + std::to_string(range_start + i * direction) + ")");
                    }
                    else
                    {
                        // PIN[A]
                        tmp_function.replace(start, 1, "(");
                        tmp_function.replace(end, 1, ")");
                    }
                }
                else
                {
                    // PIN
                    tmp_function.replace(pos, bus_name.size(), bus_name + "(" + std::to_string(cell.buses.at(bus_name).range.at(i)) + ")");
                }
                pos = tmp_function.find(bus_name, pos + 1);
            }
        }

        functions[output_bus.pin_names.at(i)] = tmp_function;
    }
}
