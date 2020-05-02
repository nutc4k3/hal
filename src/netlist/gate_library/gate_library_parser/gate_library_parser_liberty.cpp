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
        if (next_token == "cell")
        {
            if (!parse_cell(library_str))
            {
                return false;
            }
        }
        else if (next_token == "type")
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
            auto pins = parse_pin(cell_str, cell);
            if (!pins.has_value())
            {
                return false;
            }
            cell.pins.push_back(pins.value());
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

std::optional<gate_library_parser_liberty::pin_group> gate_library_parser_liberty::parse_pin(token_stream<std::string>& str, cell_group& cell, pin_direction direction)
{
    pin_group pins;

    pins.line_number = str.peek().number;
    str.consume("(", true);
    auto pin_names_str = str.extract_until(")", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume(")", true);
    str.consume("{", true);
    auto pin_str = str.extract_until("}", token_stream<std::string>::END_OF_STREAM, true, true);
    str.consume("}", true);
    pins.direction = direction;

    do
    {
        auto next_token = pin_str.consume();
        if (next_token == "direction")
        {
            pin_str.consume(":", true);
            auto direction_str = pin_str.consume().string;
            if (direction_str == "input")
            {
                pins.direction = pin_direction::IN;
            }
            else if (direction_str == "output")
            {
                pins.direction = pin_direction::OUT;
            }
            else if (direction_str == "inout")
            {
                pins.direction = pin_direction::INOUT;
            }
            else
            {
                log_error("liberty_parser", "invalid pin direction '{}' near line {}", direction_str, pin_str.peek().number);
                return std::nullopt;
            }
            pin_str.consume(";", true);
        }
        else if (next_token == "function")
        {
            pin_str.consume(":", true);
            pins.function = pin_str.consume().string;
            pin_str.consume(";", true);
        }
        else if (next_token == "x_function")
        {
            pin_str.consume(":", true);
            pins.x_function = pin_str.consume().string;
            pin_str.consume(";", true);
        }
        else if (next_token == "three_state")
        {
            pin_str.consume(":", true);
            pins.z_function = pin_str.consume().string;
            pin_str.consume(";", true);
        }
    } while (pin_str.remaining() > 0);

    std::vector<std::string>* pin_collection;
    if (direction == pin_direction::IN)
    {
        pin_collection = &(cell.input_pins);
    }
    else if (direction == pin_direction::OUT)
    {
        pin_collection = &(cell.output_pins);
    }
    else if (direction == pin_direction::INOUT)
    {
        pin_collection = &(cell.inout_pins);
    }
    else
    {
        log_error("liberty_parser", "no pin direction given near line {}", pins.line_number);
        return std::nullopt;
    }

    do
    {
        std::string name = pin_names_str.consume().string;

        if (pin_names_str.consume("["))
        {
            if (pin_names_str.peek(1) == ":")
            {
                std::vector<std::string> indexed_ports;

                i32 start = std::stol(pin_names_str.consume().string);
                pin_names_str.consume(":", true);
                i32 end = std::stol(pin_names_str.consume().string);
                i32 dir = (start <= end) ? 1 : -1;

                for (int i = start; i != (end + dir); i += dir)
                {
                    auto new_name = name + "(" + std::to_string(i) + ")";
                    indexed_ports.push_back(new_name);
                    pin_collection->push_back(new_name);
                }

                pins.names.push_back(indexed_ports);
            }
            else
            {
                u32 index     = std::stoul(pin_names_str.consume().string);
                auto new_name = name + "(" + std::to_string(index) + ")";
                pins.names.push_back({new_name});
                pin_collection->push_back(new_name);
            }

            pin_names_str.consume("]", true);
        }
        else
        {
            pins.names.push_back({name});
            pin_collection->push_back(name);
        }

        pin_names_str.consume(",", pin_names_str.remaining() > 0);
    } while (pin_names_str.remaining() > 0);

    return pins;
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
            auto pins = parse_pin(bus_str, cell, bus.direction);
            if (!pins.has_value())
            {
                return std::nullopt;
            }
            bus.pin_groups.push_back(pins.value());
        }
    } while (bus_str.remaining() > 0);

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
    std::map<std::string, std::string> functions;

    cell.input_pins.insert(cell.input_pins.end(), cell.inout_pins.begin(), cell.inout_pins.end());
    cell.output_pins.insert(cell.output_pins.end(), cell.inout_pins.begin(), cell.inout_pins.end());

    // TODO: prepare and expand all basic functions (functions, x_functions, z_functions)

    if (cell.type == gate_type::base_type::combinatorial)
    {
        gt = std::make_shared<gate_type>(cell.name);

        for (auto& [pin_name, bf_string] : functions)
        {
            gt->add_boolean_function(pin_name, boolean_function::from_string(bf_string, cell.input_pins));
        }
    }
    else if (cell.type == gate_type::base_type::ff)
    {
        auto seq_gt = std::make_shared<gate_type_sequential>(cell.name, cell.type);

        // TODO: prepare special boolean functions (replace '[X]', no multibit possible)

        if (!cell.ff.clocked_on.empty())
        {
            seq_gt->add_boolean_function("clock", boolean_function::from_string(cell.ff.clocked_on, cell.input_pins));
        }

        if (!cell.ff.next_state.empty())
        {
            seq_gt->add_boolean_function("next_state", boolean_function::from_string(cell.ff.next_state, cell.input_pins));
        }

        if (!cell.ff.preset.empty())
        {
            seq_gt->add_boolean_function("preset", boolean_function::from_string(cell.ff.preset, cell.input_pins));
        }

        if (!cell.ff.clear.empty())
        {
            seq_gt->add_boolean_function("clear", boolean_function::from_string(cell.ff.clear, cell.input_pins));
        }

        seq_gt->set_set_reset_behavior(cell.ff.special_behavior_var1, cell.ff.special_behavior_var2);
        seq_gt->set_init_data_category(cell.ff.data_category);
        seq_gt->set_init_data_identifier(cell.ff.data_identifier);

        for (auto& [pin_name, bf_string] : functions)
        {
            if (bf_string == cell.ff.state1)
            {
                seq_gt->add_state_output_pin(pin_name);
            }
            else if (bf_string == cell.ff.state2)
            {
                seq_gt->add_inverted_state_output_pin(pin_name);
            }
            else
            {
                seq_gt->add_boolean_function(pin_name, boolean_function::from_string(bf_string, cell.input_pins));
            }
        }

        gt = seq_gt;
    }
    else if (cell.type == gate_type::base_type::latch)
    {
        auto seq_gt = std::make_shared<gate_type_sequential>(cell.name, cell.type);

        // TODO: prepare special boolean functions (replace '[X]', no multibit possible)

        if (!cell.latch.enable.empty())
        {
            seq_gt->add_boolean_function("enable", boolean_function::from_string(cell.latch.enable, cell.input_pins));
        }

        if (!cell.latch.data_in.empty())
        {
            seq_gt->add_boolean_function("data", boolean_function::from_string(cell.latch.data_in, cell.input_pins));
        }

        if (!cell.latch.preset.empty())
        {
            seq_gt->add_boolean_function("preset", boolean_function::from_string(cell.latch.preset, cell.input_pins));
        }

        if (!cell.latch.clear.empty())
        {
            seq_gt->add_boolean_function("reset", boolean_function::from_string(cell.latch.clear, cell.input_pins));
        }

        seq_gt->set_set_reset_behavior(cell.latch.special_behavior_var1, cell.latch.special_behavior_var2);

        for (auto& [pin_name, bf_string] : functions)
        {
            if (bf_string == cell.latch.state1)
            {
                seq_gt->add_state_output_pin(pin_name);
            }
            else if (bf_string == cell.latch.state2)
            {
                seq_gt->add_inverted_state_output_pin(pin_name);
            }
            else
            {
                seq_gt->add_boolean_function(pin_name, boolean_function::from_string(bf_string, cell.input_pins));
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

        for (auto& [pin_name, bf_string] : functions)
        {
            if (bf_string == cell.lut.name)
            {
                lut_gt->add_output_from_init_string_pin(pin_name);
            }
            else
            {
                lut_gt->add_boolean_function(pin_name, boolean_function::from_string(bf_string, cell.input_pins));
            }
        }

        gt = lut_gt;
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

// std::map<std::string, std::string> gate_library_parser_liberty::expand_bus_pin_function(const pin_group& pin, const std::map<std::string, bus_group>& buses)
// {
//     std::map<std::string, std::string> res;

//     u32 size = pin.names.size();
//     std::vector<std::string> functions(size, "");

//     std::string delimiters = "!^+|& *()[]:";
//     std::string current_token;
//     std::vector<std::string> tokens;
//     for (char c : pin.function)
//     {
//         if (delimiters.find(c) == std::string::npos)
//         {
//             current_token += c;
//         }
//         else
//         {
//             if (!current_token.empty())
//             {
//                 tokens.push_back(current_token);
//                 current_token.clear();
//             }
//             tokens.emplace_back(std::string(1, c));
//         }
//     }
//     if (!current_token.empty())
//     {
//         tokens.push_back(current_token);
//     }

//     std::string tmp;
//     u32 function_length = tokens.size();

//     for (int i = 0; i < function_length; i++)
//     {
//         if (const auto& it = buses.find(tokens[i]); it != buses.end())
//         {
//             std::vector<u32> range;

//             if ((i + 3) < function_length && tokens[i + 1] == "[")
//             {
//                 if (tokens[i + 3] == ":")
//                 {
//                     i32 start     = std::stoul(tokens[i + 2]);
//                     i32 end       = std::stoul(tokens[i + 4]);
//                     i32 direction = (start <= end) ? 1 : -1;

//                     for (int j = start; j != (end + direction); j += direction)
//                     {
//                         range.push_back(j);
//                     }
//                     i += 5;
//                 }
//                 else
//                 {
//                     range = {std::stoul(tokens[i + 2])};
//                     i += 3;
//                 }
//             }
//             else
//             {
//                 range = it->second.range;
//             }

//             tmp += tokens[i];

//             for (int j = 0; j < size; j++)
//             {
//                 functions[i] += tmp + "(" + std::to_string(range[j]) + ")";
//             }
//             tmp.clear();
//         }
//         else
//         {
//             tmp += tokens[i];
//         }
//     }

//     for (int j = 0; j < size; j++)
//     {
//         res[pin.names[j]] = functions[j] + tmp;
//     }

//     return res;
// }