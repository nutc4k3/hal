//  MIT License
//
//  Copyright (c) 2019 Ruhr-University Bochum, Germany, Chair for Embedded Security. All Rights reserved.
//  Copyright (c) 2019 Marc Fyrbiak, Sebastian Wallat, Max Hoffmann ("ORIGINAL AUTHORS"). All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#pragma once

#include "def.h"
#include "netlist/gate_library/gate_type/gate_type.h"
#include "netlist/module.h"
#include "netlist/net.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/* forward declaration*/
class netlist;

/**
 * @ingroup hdl_parsers
 */
template<typename T>
class HDL_PARSER_API hdl_parser
{
public:
    /**
     * @param[in] stream - The string stream filled with the hdl code.
     */
    explicit hdl_parser(std::stringstream& stream);

    virtual ~hdl_parser() = default;

    /**
     * TODO 
     * Parses hdl code for a specific netlist library.
     *
     * @param[in] gate_library - The gate library name.
     * @returns The netlist representation of the hdl code or a nullptr on error.
     */
    virtual bool parse() = 0;

    // TODO
    std::shared_ptr<netlist> instantiate(const std::string& gate_library);

protected:
    class signal
    {
    public:
        u32 _line_number;

        // name (may either be the identifier of the signal or a binary string in case of direct assignments)
        T _name;

        // ranges
        std::vector<std::vector<u32>> _ranges;

        // is binary string?
        bool _is_binary = false;

        // are bounds already known? (should only be unknown for left side of port assignments)
        bool _is_bound_known = true;

        signal(u32 line_number, T name, std::vector<std::vector<u32>> ranges, bool is_binary = false, bool is_bound_known = true)
            : _line_number(line_number), _name(name), _ranges(ranges), _is_binary(is_binary), _is_bound_known(is_bound_known)
        {
        }

        i32 size() const
        {
            if (_is_bound_known)
            {
                return _ranges.size();
            }

            return -1;
        }

        void set_ranges(std::vector<std::vector<u32>> ranges)
        {
            _ranges         = ranges;
            _is_bound_known = true;
        }

        bool operator<(const signal& other) const
        {
            // there may be two assignments to the same signal using different bounds
            // without checking bounds, two such signals would be considered equal
            return (_name < other._name) && (_ranges < other._ranges);
        }
    };

    class instance
    {
    public:
        u32 _line_number;

        // name
        T _name;

        // type
        T _type;

        // port assignments: port_name -> (port_signal, assignment_signals)
        std::map<T, std::pair<signal, std::vector<signal>>> _port_assignments;

        // generic assignments: generic_name -> (data_type, data_value)
        std::map<T, std::pair<T, T>> _generic_assignments;

        bool operator<(const instance& other) const
        {
            return _name < other._name;
        }
    };

    class entity
    {
    public:
        u32 _line_number;

        // name
        T _name;

        // ports: port_name -> (direction, signal)
        std::map<T, std::pair<T, signal>> _ports;

        // expanded ports: port_name -> expanded_ports
        std::map<std::string, std::vector<std::string>> _ports_expanded;

        // attributes: attribute_target -> set(attribute_name, attribute_type, attribute_value)
        std::map<T, std::set<std::tuple<T, T, T>>> _entity_attributes;
        std::map<T, std::set<std::tuple<T, T, T>>> _instance_attributes;
        std::map<T, std::set<std::tuple<T, T, T>>> _signal_attributes;

        // signals: signal_name -> signal
        std::map<T, signal> _signals;

        // expanded signals: signal_name -> expanded_signals
        std::map<T, std::vector<T>> _signals_expanded;

        // assignments: set(lhs, rhs)
        std::set<std::pair<std::vector<signal>, std::vector<signal>>> _assignments;

        // instances: instance_name -> instance
        std::map<T, instance> _instances;

        bool operator<(const entity& other) const
        {
            return _name < other._name;
        }
    };

    // stores the input stream to the file
    std::stringstream& m_fs;

    std::map<T, entity> m_entities;
    T m_last_entity;

private:
    std::string m_file_name;

    // stores the netlist
    std::shared_ptr<netlist> m_netlist;

    // unique alias generation
    std::map<T, u32> m_signal_name_occurrences;
    std::map<T, u32> m_instance_name_occurrences;
    std::map<T, u32> m_current_signal_index;
    std::map<T, u32> m_current_instance_index;

    // net generation
    std::map<T, std::shared_ptr<net>> m_net_by_name;
    std::map<T, std::vector<T>> m_nets_to_merge;
    std::map<T, std::function<bool(net&)>> mark_global_net_function_map = {{"in", &net::mark_global_input_net}, {"out", &net::mark_global_output_net}};

    bool build_netlist(const T& top_module);
    std::shared_ptr<module> instantiate_top_module(entity& e);
    std::shared_ptr<module> instantiate(const instance& inst, std::shared_ptr<module> parent);

    T get_unique_signal_suffix(const T& name);
    T get_unique_instance_suffix(const T& name);
    void expand_signal_recursively(std::vector<T>& expanded_signal, T current_signal, const std::vector<std::vector<u32>>& ranges, u32 dimension);
};
